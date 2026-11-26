/* odysseyvk_capa.cpp — la capa de Vulkan de OdysseyVK que encuentra la profundidad que el juego le da
   a DLSS (v1.13, PRUEBA: fase 1).

   Rodolfo: «I actually want my users to be able to use this without neural forge, just with the game
   dlss itself». La red de profundidad adivina; el juego la tiene exacta, y DLSS la recibe en cada
   cuadro. Con Proton, DLSS corre como nucleos de CUDA lanzados por Vulkan (`vkCmdCuLaunchKernelNVX`,
   VK_NVX_binary_import), y a cada imagen que esos nucleos leen le pide antes al controlador un numero
   (`vkGetImageViewHandleNVX`, VK_NVX_image_view_handle). Esta capa mira las dos cosas: entre las
   imagenes cuyo numero aparece en los parametros de un lanzamiento, la que tiene formato de
   profundidad (D16/D24/D32) es la del juego. Para DLSS es su unica entrada de ese formato.

   Fase 1 (esta): apuntarlo todo en un registro y, a peticion, guardar UN cuadro de esa profundidad
   en un archivo, para compararlo con el volcado de Neural Forge del mismo juego. Nada mas: no
   comparte nada en vivo con el visor todavia (fase 2).

   Se enciende por juego, como MangoHud o Neural Forge: con `ODYSSEYVK_CAPA=1` en las opciones de
   lanzamiento (el manifiesto la declara capa implicita con esa variable). Sin ella no se carga.
     - registro:  /tmp/odysseyvk-capa-<uid>/capa-<pid>.log
     - volcar:    touch /tmp/odysseyvk-capa-<uid>/volcar   (se mira una vez por segundo)
                  o ODYSSEYVK_CAPA_VOLCAR_TRAS=<segundos> desde el primer lanzamiento de DLSS
     - salida:    /tmp/odysseyvk-capa-<uid>/profundidad-<n>.r32f + .json (profundidad del
                  dispositivo en float32, la fila de arriba primero; D16/D24 divididos por su maximo)

   Atajos de la prueba (anotados para la fase 2): la copia se graba justo antes del lanzamiento que lee
   la profundidad, con barreras de memoria anchas y el diseno GENERAL (el controlador de NVIDIA no mira
   los disenos; en la fase 2 se lleva la cuenta de verdad); a las imagenes de profundidad se les anade
   TRANSFER_SRC al crearlas (como hace Neural Forge); y el volcado espera a la cola una vez
   (vkQueueWaitIdle), un tiron de un cuadro cuando se pide. */
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define EXPORTA extern "C" __attribute__((visibility("default")))

namespace {

std::mutex g_mx;

/* ---------- registro ---------- */
std::string carpeta() {
  static const std::string c = "/tmp/odysseyvk-capa-" + std::to_string((long)getuid());
  return c;
}
FILE *g_log = nullptr;
void anota(const char *fmt, ...) {
  if (!g_log) {
    mkdir(carpeta().c_str(), 0700);
    const std::string r = carpeta() + "/capa-" + std::to_string((long)getpid()) + ".log";
    g_log = std::fopen(r.c_str(), "a");
    if (!g_log) return;
    setvbuf(g_log, nullptr, _IOLBF, 0);
  }
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(g_log, fmt, ap);
  va_end(ap);
  std::fputc('\n', g_log);
}

double ahora() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* ---------- tablas de despacho ---------- */
inline void *llave(const void *h) { return *(void *const *)h; }

struct Instancia {
  VkInstance inst = VK_NULL_HANDLE;
  PFN_vkGetInstanceProcAddr gipa = nullptr;
  PFN_vkDestroyInstance destroy = nullptr;
  PFN_vkGetPhysicalDeviceMemoryProperties memProps = nullptr;
  PFN_vkGetPhysicalDeviceProperties props = nullptr;
};
std::unordered_map<void *, Instancia> g_inst;

struct Imagen {
  VkFormat formato;
  uint32_t w, h;
  VkSampleCountFlagBits muestras;
};
struct Vista {
  VkImage img;
  VkFormat formato;
};
struct Pendiente {
  VkCommandBuffer cb;
  VkBuffer buf;
  VkDeviceMemory mem;
  uint32_t w, h;
  VkFormat formato;
};

struct Dispositivo {
  VkDevice dev = VK_NULL_HANDLE;
  VkPhysicalDevice fisico = VK_NULL_HANDLE;
  Instancia *inst = nullptr;
  PFN_vkGetDeviceProcAddr gdpa = nullptr;
  PFN_vkDestroyDevice destroy = nullptr;
  PFN_vkCreateImage createImage = nullptr;
  PFN_vkDestroyImage destroyImage = nullptr;
  PFN_vkCreateImageView createView = nullptr;
  PFN_vkDestroyImageView destroyView = nullptr;
  PFN_vkGetImageViewHandleNVX handle32 = nullptr;
  PFN_vkGetImageViewHandle64NVX handle64 = nullptr;
  PFN_vkCmdCuLaunchKernelNVX cuLaunch = nullptr;
  PFN_vkQueueSubmit submit = nullptr;
  PFN_vkQueueSubmit2 submit2 = nullptr;
  PFN_vkQueueSubmit2KHR submit2khr = nullptr;
  PFN_vkQueueWaitIdle waitIdle = nullptr;
  PFN_vkCreateBuffer createBuffer = nullptr;
  PFN_vkDestroyBuffer destroyBuffer = nullptr;
  PFN_vkGetBufferMemoryRequirements bufReqs = nullptr;
  PFN_vkAllocateMemory alloc = nullptr;
  PFN_vkFreeMemory free = nullptr;
  PFN_vkBindBufferMemory bind = nullptr;
  PFN_vkMapMemory map = nullptr;
  PFN_vkUnmapMemory unmap = nullptr;
  PFN_vkCmdPipelineBarrier barrier = nullptr;
  PFN_vkCmdCopyImageToBuffer copyImgBuf = nullptr;

  std::unordered_map<VkImage, Imagen> imagenes;
  std::unordered_map<VkImageView, Vista> vistas;
  std::unordered_map<uint64_t, VkImageView> numeros;   /* numero NVX -> vista */
  std::unordered_set<uint32_t> numeros32;              /* para mirar rapido los parametros */
  long pedidos = 0, lanzamientos = 0, conProfundidad = 0, detallados = 0;
  double primerLanzamiento = 0.0, ultimaMirada = 0.0, ultimoResumen = 0.0;
  bool armado = false, autoHecho = false;
  int volcados = 0;
  std::vector<Pendiente> pendientes;
  std::unordered_set<VkImage> vistasDeProfundidad;     /* imagenes de profundidad que DLSS ha leido */
  /* la ultima imagen de profundidad a la que DLSS le pidio numero (MEDIDO en Cyberpunk: 1920x1080
     D32_SFLOAT, el tamano de render, varias veces al cambiar de escena) */
  VkImage ultimaPedida = VK_NULL_HANDLE;
  long extrasVistos = 0;
};
std::unordered_map<void *, Dispositivo *> g_dev;

Dispositivo *dispositivo(const void *h) {
  auto it = g_dev.find(llave(h));
  return it == g_dev.end() ? nullptr : it->second;
}

bool esProfundidad(VkFormat f) {
  switch (f) {
    case VK_FORMAT_D16_UNORM: case VK_FORMAT_D16_UNORM_S8_UINT: case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D24_UNORM_S8_UINT: case VK_FORMAT_D32_SFLOAT: case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return true;
    default:
      return false;
  }
}
const char *nombreFormato(VkFormat f) {
  switch (f) {
    case VK_FORMAT_D16_UNORM: return "D16_UNORM";
    case VK_FORMAT_D16_UNORM_S8_UINT: return "D16_UNORM_S8_UINT";
    case VK_FORMAT_X8_D24_UNORM_PACK32: return "X8_D24_UNORM";
    case VK_FORMAT_D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";
    case VK_FORMAT_D32_SFLOAT: return "D32_SFLOAT";
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return "D32_SFLOAT_S8_UINT";
    case VK_FORMAT_R32_SFLOAT: return "R32_SFLOAT";
    case VK_FORMAT_R16G16_SFLOAT: return "R16G16_SFLOAT";
    case VK_FORMAT_R16G16B16A16_SFLOAT: return "R16G16B16A16_SFLOAT";
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return "B10G11R11_UFLOAT";
    case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
    case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "A2B10G10R10_UNORM";
    default: return nullptr;
  }
}
std::string formato(VkFormat f) {
  const char *n = nombreFormato(f);
  return n ? n : ("formato " + std::to_string((int)f));
}

uint32_t tipoMemoria(Dispositivo *d, uint32_t bits, VkMemoryPropertyFlags quiero) {
  VkPhysicalDeviceMemoryProperties mp{};
  d->inst->memProps(d->fisico, &mp);
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
    if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & quiero) == quiero) return i;
  return UINT32_MAX;
}

/* ---------- el volcado ---------- */
void mirarPeticion(Dispositivo *d) {
  const double t = ahora();
  if (t - d->ultimaMirada < 1.0) return;
  d->ultimaMirada = t;
  const std::string f = carpeta() + "/volcar";
  struct stat st{};
  if (stat(f.c_str(), &st) == 0) {
    unlink(f.c_str());
    d->armado = true;
    anota("[volcar] pedido por archivo");
  }
  if (!d->autoHecho) {
    if (const char *e = std::getenv("ODYSSEYVK_CAPA_VOLCAR_TRAS")) {
      if (d->primerLanzamiento > 0.0 && t - d->primerLanzamiento >= std::atof(e)) {
        d->autoHecho = true;
        d->armado = true;
        anota("[volcar] pedido por ODYSSEYVK_CAPA_VOLCAR_TRAS=%s", e);
      }
    }
  }
}

void grabarCopia(Dispositivo *d, VkCommandBuffer cb, VkImage img, const Imagen &im) {
  if (im.muestras != VK_SAMPLE_COUNT_1_BIT) {
    anota("[volcar] la profundidad tiene %d muestras (MSAA): no se puede copiar asi en la fase 1", (int)im.muestras);
    d->armado = false;
    return;
  }
  const bool dos = im.formato == VK_FORMAT_D16_UNORM || im.formato == VK_FORMAT_D16_UNORM_S8_UINT;
  const VkDeviceSize tam = (VkDeviceSize)im.w * im.h * (dos ? 2 : 4);
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = tam;
  bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkBuffer buf = VK_NULL_HANDLE;
  if (d->createBuffer(d->dev, &bi, nullptr, &buf) != VK_SUCCESS) { anota("[volcar] vkCreateBuffer fallo"); return; }
  VkMemoryRequirements mr{};
  d->bufReqs(d->dev, buf, &mr);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = mr.size;
  ai.memoryTypeIndex = tipoMemoria(d, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VkDeviceMemory mem = VK_NULL_HANDLE;
  if (ai.memoryTypeIndex == UINT32_MAX || d->alloc(d->dev, &ai, nullptr, &mem) != VK_SUCCESS) {
    anota("[volcar] sin memoria visible para la copia");
    d->destroyBuffer(d->dev, buf, nullptr);
    return;
  }
  d->bind(d->dev, buf, mem, 0);
  VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
  mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  d->barrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
  VkBufferImageCopy reg{};
  reg.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
  reg.imageExtent = {im.w, im.h, 1};
  d->copyImgBuf(cb, img, VK_IMAGE_LAYOUT_GENERAL, buf, 1, &reg);
  mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;
  d->barrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0,
             nullptr, 0, nullptr);
  d->pendientes.push_back({cb, buf, mem, im.w, im.h, im.formato});
  d->armado = false;
  anota("[volcar] copia grabada: %ux%u %s", im.w, im.h, formato(im.formato).c_str());
}

void leerPendientes(Dispositivo *d, VkQueue cola, const std::vector<VkCommandBuffer> &enviados) {
  bool alguno = false;
  for (const Pendiente &p : d->pendientes)
    for (VkCommandBuffer cb : enviados)
      if (cb == p.cb) alguno = true;
  if (!alguno) return;
  d->waitIdle(cola);
  std::vector<Pendiente> quedan;
  for (const Pendiente &p : d->pendientes) {
    bool este = false;
    for (VkCommandBuffer cb : enviados)
      if (cb == p.cb) este = true;
    if (!este) { quedan.push_back(p); continue; }
    void *ptr = nullptr;
    if (d->map(d->dev, p.mem, 0, VK_WHOLE_SIZE, 0, &ptr) == VK_SUCCESS) {
      const size_t n = (size_t)p.w * p.h;
      std::vector<float> f(n);
      if (p.formato == VK_FORMAT_D16_UNORM || p.formato == VK_FORMAT_D16_UNORM_S8_UINT) {
        const uint16_t *s = (const uint16_t *)ptr;
        for (size_t i = 0; i < n; i++) f[i] = s[i] / 65535.0f;
      } else if (p.formato == VK_FORMAT_X8_D24_UNORM_PACK32 || p.formato == VK_FORMAT_D24_UNORM_S8_UINT) {
        const uint32_t *s = (const uint32_t *)ptr;
        for (size_t i = 0; i < n; i++) f[i] = (s[i] & 0xFFFFFFu) / 16777215.0f;
      } else {
        std::memcpy(f.data(), ptr, n * 4);
      }
      d->unmap(d->dev, p.mem);
      const int k = ++d->volcados;
      const std::string base = carpeta() + "/profundidad-" + std::to_string((long)getpid()) + "-" + std::to_string(k);
      if (FILE *o = std::fopen((base + ".r32f").c_str(), "wb")) {
        std::fwrite(f.data(), 4, n, o);
        std::fclose(o);
      }
      if (FILE *o = std::fopen((base + ".json").c_str(), "w")) {
        std::fprintf(o, "{\"width\": %u, \"height\": %u, \"format\": \"%s\", \"pid\": %ld}\n", p.w, p.h,
                     formato(p.formato).c_str(), (long)getpid());
        std::fclose(o);
      }
      size_t ceros = 0, unos = 0;
      for (float v : f) { ceros += v <= 0.0f; unos += v >= 1.0f; }
      anota("[volcar] escrito %s.r32f (%ux%u %s) · %zu puntos a 0 y %zu a 1", base.c_str(), p.w, p.h,
            formato(p.formato).c_str(), ceros, unos);
    }
    d->destroyBuffer(d->dev, p.buf, nullptr);
    d->free(d->dev, p.mem, nullptr);
  }
  d->pendientes.swap(quedan);
}

/* ---------- ganchos de dispositivo ---------- */
VKAPI_ATTR VkResult VKAPI_CALL CreateImage(VkDevice device, const VkImageCreateInfo *ci, const VkAllocationCallbacks *a, VkImage *img) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  VkImageCreateInfo c = *ci;
  if (esProfundidad(c.format)) c.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;   /* para poder copiarla */
  const VkResult r = d->createImage(device, &c, a, img);
  if (r == VK_SUCCESS) {
    std::lock_guard<std::mutex> g(g_mx);
    d->imagenes[*img] = {c.format, c.extent.width, c.extent.height, c.samples};
  }
  return r;
}
VKAPI_ATTR void VKAPI_CALL DestroyImage(VkDevice device, VkImage img, const VkAllocationCallbacks *a) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
    d->imagenes.erase(img);
    d->vistasDeProfundidad.erase(img);
    if (d->ultimaPedida == img) d->ultimaPedida = VK_NULL_HANDLE;
  }
  d->destroyImage(device, img, a);
}
VKAPI_ATTR VkResult VKAPI_CALL CreateImageView(VkDevice device, const VkImageViewCreateInfo *ci, const VkAllocationCallbacks *a, VkImageView *v) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  const VkResult r = d->createView(device, ci, a, v);
  if (r == VK_SUCCESS) {
    std::lock_guard<std::mutex> g(g_mx);
    d->vistas[*v] = {ci->image, ci->format};
  }
  return r;
}
VKAPI_ATTR void VKAPI_CALL DestroyImageView(VkDevice device, VkImageView v, const VkAllocationCallbacks *a) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
    d->vistas.erase(v);
  }
  d->destroyView(device, v, a);
}

void apuntarNumero(Dispositivo *d, const VkImageViewHandleInfoNVX *info, uint64_t n) {
  d->pedidos++;
  d->numeros[n] = info->imageView;
  d->numeros32.insert((uint32_t)n);
  {
    auto v = d->vistas.find(info->imageView);
    if (v != d->vistas.end()) {
      auto im = d->imagenes.find(v->second.img);
      if (im != d->imagenes.end() && esProfundidad(im->second.formato)) d->ultimaPedida = v->second.img;
    }
  }
  if (d->pedidos <= 300) {
    auto v = d->vistas.find(info->imageView);
    if (v != d->vistas.end()) {
      auto im = d->imagenes.find(v->second.img);
      if (im != d->imagenes.end())
        anota("[numero] %ld · %ux%u %s (vista %s) · descriptor %d", d->pedidos, im->second.w, im->second.h,
              formato(im->second.formato).c_str(), formato(v->second.formato).c_str(), (int)info->descriptorType);
    }
  }
}
VKAPI_ATTR uint32_t VKAPI_CALL GetImageViewHandleNVX(VkDevice device, const VkImageViewHandleInfoNVX *info) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  const uint32_t n = d->handle32(device, info);
  std::lock_guard<std::mutex> g(g_mx);
  apuntarNumero(d, info, n);
  return n;
}
VKAPI_ATTR uint64_t VKAPI_CALL GetImageViewHandle64NVX(VkDevice device, const VkImageViewHandleInfoNVX *info) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  const uint64_t n = d->handle64(device, info);
  std::lock_guard<std::mutex> g(g_mx);
  apuntarNumero(d, info, n);
  return n;
}

/* los numeros de vista que aparecen en los parametros del lanzamiento */
void buscarEn(Dispositivo *d, const uint8_t *p, size_t bytes, std::vector<VkImageView> &out) {
  for (size_t o = 0; o + 4 <= bytes; o += 4) {
    uint32_t v32;
    std::memcpy(&v32, p + o, 4);
    if (v32 && d->numeros32.count(v32)) {
      auto it = d->numeros.find(v32);
      if (it != d->numeros.end()) out.push_back(it->second);
    }
    if ((o % 8) == 0 && o + 8 <= bytes) {
      uint64_t v64;
      std::memcpy(&v64, p + o, 8);
      if (v64 > 0xFFFFFFFFull) {
        auto it = d->numeros.find(v64);
        if (it != d->numeros.end()) out.push_back(it->second);
      }
    }
  }
}

VKAPI_ATTR void VKAPI_CALL CmdCuLaunchKernelNVX(VkCommandBuffer cb, const VkCuLaunchInfoNVX *li) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
    d->lanzamientos++;
    if (d->primerLanzamiento <= 0.0) {
      d->primerLanzamiento = ahora();
      anota("[lanzar] primer lanzamiento de nucleo (DLSS u otro NGX)");
    }
    mirarPeticion(d);
    std::vector<VkImageView> vistas;
    /* los parametros: cada uno es un puntero a su valor (tamanos que no sabemos: se leen 4 bytes) */
    for (size_t i = 0; li && i < li->paramCount; i++)
      if (li->pParams && li->pParams[i]) buscarEn(d, (const uint8_t *)li->pParams[i], 4, vistas);
    /* los «extra» de CUDA: {BUFFER_POINTER=1, ptr, BUFFER_SIZE=2, &tam, END=0} */
    if (li && li->pExtras && li->extraCount > 0 && d->extrasVistos < 6) {
      d->extrasVistos++;
      std::string crudo;
      char tmp[64];
      for (size_t i = 0; i < li->extraCount && i < 8; i++) {
        std::snprintf(tmp, sizeof tmp, " [%zu]=%p", i, li->pExtras[i]);
        crudo += tmp;
      }
      anota("[extra] lanzamiento %ld · %zu extra:%s", d->lanzamientos, li->extraCount, crudo.c_str());
      for (size_t i = 0; i < li->extraCount && i < 2; i++) {
        const uintptr_t v = (uintptr_t)li->pExtras[i];
        if (v > 0x10000 && v < 0x7fffffffffffull && (v % 8) == 0) {
          const uint32_t *w = (const uint32_t *)li->pExtras[i];
          std::string hex;
          for (int k = 0; k < 16; k++) {
            std::snprintf(tmp, sizeof tmp, " %08x", w[k]);
            hex += tmp;
          }
          anota("[extra]   [%zu] apunta a:%s", i, hex.c_str());
        }
      }
    }
    const void *bufp = nullptr;
    size_t buft = 0;
    for (size_t i = 0; li && li->pExtras && i + 1 < li->extraCount; i += 2) {
      const uintptr_t clave = (uintptr_t)li->pExtras[i];
      if (clave == 0) break;
      if (clave == 1) bufp = li->pExtras[i + 1];
      else if (clave == 2 && li->pExtras[i + 1]) buft = *(const size_t *)li->pExtras[i + 1];
    }
    if (bufp && buft && buft < (1u << 20)) buscarEn(d, (const uint8_t *)bufp, buft, vistas);
    VkImage prof = VK_NULL_HANDLE;
    Imagen improf{};
    std::string lista;
    for (VkImageView v : vistas) {
      auto vv = d->vistas.find(v);
      if (vv == d->vistas.end()) continue;
      auto im = d->imagenes.find(vv->second.img);
      if (im == d->imagenes.end()) continue;
      if (d->detallados < 60) lista += " " + std::to_string(im->second.w) + "x" + std::to_string(im->second.h) + ":" + formato(im->second.formato);
      if (esProfundidad(im->second.formato) && prof == VK_NULL_HANDLE) prof = vv->second.img, improf = im->second;
    }
    if (d->detallados < 60 && (!vistas.empty() || d->detallados < 10)) {
      d->detallados++;
      anota("[lanzar] %ld · parametros %zu, extra %zu (bufer de %zu bytes) · imagenes:%s", d->lanzamientos,
            li ? li->paramCount : 0, li ? li->extraCount : 0, buft, lista.empty() ? " (ninguna conocida)" : lista.c_str());
    }
    if (prof == VK_NULL_HANDLE && d->armado && d->ultimaPedida != VK_NULL_HANDLE) {
      auto im = d->imagenes.find(d->ultimaPedida);
      if (im != d->imagenes.end()) {
        anota("[volcar] por la ultima imagen de profundidad a la que DLSS pidio numero (los parametros no la nombran)");
        grabarCopia(d, cb, d->ultimaPedida, im->second);
      }
    }
    if (prof != VK_NULL_HANDLE) {
      d->conProfundidad++;
      if (!d->vistasDeProfundidad.count(prof)) {
        d->vistasDeProfundidad.insert(prof);
        anota("[profundidad] DLSS lee una imagen de profundidad: %ux%u %s (%zu distintas hasta ahora)", improf.w, improf.h,
              formato(improf.formato).c_str(), d->vistasDeProfundidad.size());
      }
      if (d->armado) grabarCopia(d, cb, prof, improf);
    }
    const double t = ahora();
    if (t - d->ultimoResumen >= 10.0) {
      d->ultimoResumen = t;
      anota("[resumen] numeros pedidos %ld · lanzamientos %ld · con profundidad %ld · imagenes de profundidad leidas %zu",
            d->pedidos, d->lanzamientos, d->conProfundidad, d->vistasDeProfundidad.size());
    }
  }
  d->cuLaunch(cb, li);
}

VKAPI_ATTR VkResult VKAPI_CALL QueueSubmit(VkQueue q, uint32_t n, const VkSubmitInfo *s, VkFence f) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(q);
  }
  const VkResult r = d->submit(q, n, s, f);
  std::lock_guard<std::mutex> g(g_mx);
  if (!d->pendientes.empty()) {
    std::vector<VkCommandBuffer> cbs;
    for (uint32_t i = 0; i < n; i++)
      for (uint32_t k = 0; k < s[i].commandBufferCount; k++) cbs.push_back(s[i].pCommandBuffers[k]);
    leerPendientes(d, q, cbs);
  }
  return r;
}
VkResult submit2Comun(Dispositivo *d, VkQueue q, uint32_t n, const VkSubmitInfo2 *s, VkFence f, bool khr) {
  const VkResult r = khr ? d->submit2khr(q, n, s, f) : d->submit2(q, n, s, f);
  std::lock_guard<std::mutex> g(g_mx);
  if (!d->pendientes.empty()) {
    std::vector<VkCommandBuffer> cbs;
    for (uint32_t i = 0; i < n; i++)
      for (uint32_t k = 0; k < s[i].commandBufferInfoCount; k++) cbs.push_back(s[i].pCommandBufferInfos[k].commandBuffer);
    leerPendientes(d, q, cbs);
  }
  return r;
}
VKAPI_ATTR VkResult VKAPI_CALL QueueSubmit2(VkQueue q, uint32_t n, const VkSubmitInfo2 *s, VkFence f) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(q);
  }
  return submit2Comun(d, q, n, s, f, false);
}
VKAPI_ATTR VkResult VKAPI_CALL QueueSubmit2KHR(VkQueue q, uint32_t n, const VkSubmitInfo2 *s, VkFence f) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(q);
  }
  return submit2Comun(d, q, n, s, f, true);
}

VKAPI_ATTR void VKAPI_CALL DestroyDevice(VkDevice device, const VkAllocationCallbacks *a) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
    g_dev.erase(llave(device));
    anota("[dispositivo] cerrado · numeros pedidos %ld · lanzamientos %ld · con profundidad %ld", d->pedidos,
          d->lanzamientos, d->conProfundidad);
  }
  PFN_vkDestroyDevice destruir = d->destroy;
  delete d;
  destruir(device, a);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL GetDeviceProcAddr(VkDevice device, const char *nombre);

VKAPI_ATTR VkResult VKAPI_CALL CreateDevice(VkPhysicalDevice fisico, const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *a,
                                            VkDevice *device) {
  VkLayerDeviceCreateInfo *li = (VkLayerDeviceCreateInfo *)ci->pNext;
  while (li && !(li->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && li->function == VK_LAYER_LINK_INFO))
    li = (VkLayerDeviceCreateInfo *)li->pNext;
  if (!li) return VK_ERROR_INITIALIZATION_FAILED;
  PFN_vkGetInstanceProcAddr gipa = li->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  PFN_vkGetDeviceProcAddr gdpa = li->u.pLayerInfo->pfnNextGetDeviceProcAddr;
  li->u.pLayerInfo = li->u.pLayerInfo->pNext;
  Instancia *inst;
  {
    std::lock_guard<std::mutex> g(g_mx);
    auto it = g_inst.find(llave(fisico));
    if (it == g_inst.end()) return VK_ERROR_INITIALIZATION_FAILED;
    inst = &it->second;
  }
  auto crear = (PFN_vkCreateDevice)gipa(inst->inst, "vkCreateDevice");
  const VkResult r = crear(fisico, ci, a, device);
  if (r != VK_SUCCESS) return r;
  Dispositivo *d = new Dispositivo;
  d->dev = *device;
  d->fisico = fisico;
  d->inst = inst;
  d->gdpa = gdpa;
#define CARGA(campo, nombre) d->campo = (decltype(d->campo))gdpa(*device, nombre)
  CARGA(destroy, "vkDestroyDevice");
  CARGA(createImage, "vkCreateImage");
  CARGA(destroyImage, "vkDestroyImage");
  CARGA(createView, "vkCreateImageView");
  CARGA(destroyView, "vkDestroyImageView");
  CARGA(handle32, "vkGetImageViewHandleNVX");
  CARGA(handle64, "vkGetImageViewHandle64NVX");
  CARGA(cuLaunch, "vkCmdCuLaunchKernelNVX");
  CARGA(submit, "vkQueueSubmit");
  CARGA(submit2, "vkQueueSubmit2");
  CARGA(submit2khr, "vkQueueSubmit2KHR");
  CARGA(waitIdle, "vkQueueWaitIdle");
  CARGA(createBuffer, "vkCreateBuffer");
  CARGA(destroyBuffer, "vkDestroyBuffer");
  CARGA(bufReqs, "vkGetBufferMemoryRequirements");
  CARGA(alloc, "vkAllocateMemory");
  CARGA(free, "vkFreeMemory");
  CARGA(bind, "vkBindBufferMemory");
  CARGA(map, "vkMapMemory");
  CARGA(unmap, "vkUnmapMemory");
  CARGA(barrier, "vkCmdPipelineBarrier");
  CARGA(copyImgBuf, "vkCmdCopyImageToBuffer");
#undef CARGA
  VkPhysicalDeviceProperties pp{};
  inst->props(fisico, &pp);
  std::string ext;
  for (uint32_t i = 0; i < ci->enabledExtensionCount; i++)
    if (std::strstr(ci->ppEnabledExtensionNames[i], "NVX")) ext += std::string(" ") + ci->ppEnabledExtensionNames[i];
  {
    std::lock_guard<std::mutex> g(g_mx);
    g_dev[llave(*device)] = d;
    anota("[dispositivo] %s · extensiones NVX:%s · numeros %s · lanzar %s", pp.deviceName, ext.empty() ? " ninguna" : ext.c_str(),
          (d->handle32 || d->handle64) ? "si" : "no", d->cuLaunch ? "si" : "no");
  }
  return VK_SUCCESS;
}

/* ---------- ganchos de instancia ---------- */
VKAPI_ATTR VkResult VKAPI_CALL CreateInstance(const VkInstanceCreateInfo *ci, const VkAllocationCallbacks *a, VkInstance *inst) {
  VkLayerInstanceCreateInfo *li = (VkLayerInstanceCreateInfo *)ci->pNext;
  while (li && !(li->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && li->function == VK_LAYER_LINK_INFO))
    li = (VkLayerInstanceCreateInfo *)li->pNext;
  if (!li) return VK_ERROR_INITIALIZATION_FAILED;
  PFN_vkGetInstanceProcAddr gipa = li->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  li->u.pLayerInfo = li->u.pLayerInfo->pNext;
  auto crear = (PFN_vkCreateInstance)gipa(VK_NULL_HANDLE, "vkCreateInstance");
  const VkResult r = crear(ci, a, inst);
  if (r != VK_SUCCESS) return r;
  Instancia in;
  in.inst = *inst;
  in.gipa = gipa;
  in.destroy = (PFN_vkDestroyInstance)gipa(*inst, "vkDestroyInstance");
  in.memProps = (PFN_vkGetPhysicalDeviceMemoryProperties)gipa(*inst, "vkGetPhysicalDeviceMemoryProperties");
  in.props = (PFN_vkGetPhysicalDeviceProperties)gipa(*inst, "vkGetPhysicalDeviceProperties");
  std::lock_guard<std::mutex> g(g_mx);
  g_inst[llave(*inst)] = in;
  anota("[instancia] creada (%s)", ci->pApplicationInfo && ci->pApplicationInfo->pApplicationName ? ci->pApplicationInfo->pApplicationName : "sin nombre");
  return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL DestroyInstance(VkInstance inst, const VkAllocationCallbacks *a) {
  PFN_vkDestroyInstance destruir;
  {
    std::lock_guard<std::mutex> g(g_mx);
    auto it = g_inst.find(llave(inst));
    if (it == g_inst.end()) return;
    destruir = it->second.destroy;
    g_inst.erase(it);
  }
  destruir(inst, a);
}

PFN_vkVoidFunction gancho(const char *n) {
#define G(nombre, fn) if (!std::strcmp(n, nombre)) return (PFN_vkVoidFunction)&fn
  G("vkCreateImage", CreateImage);
  G("vkDestroyImage", DestroyImage);
  G("vkCreateImageView", CreateImageView);
  G("vkDestroyImageView", DestroyImageView);
  G("vkQueueSubmit", QueueSubmit);
  G("vkQueueSubmit2", QueueSubmit2);
  G("vkQueueSubmit2KHR", QueueSubmit2KHR);
  G("vkDestroyDevice", DestroyDevice);
#undef G
  return nullptr;
}
/* los de NVX solo si el dispositivo de abajo los tiene: si no, devolverlos haria creer que existen */
PFN_vkVoidFunction ganchoNvx(const char *n) {
  if (!std::strcmp(n, "vkGetImageViewHandleNVX")) return (PFN_vkVoidFunction)&GetImageViewHandleNVX;
  if (!std::strcmp(n, "vkGetImageViewHandle64NVX")) return (PFN_vkVoidFunction)&GetImageViewHandle64NVX;
  if (!std::strcmp(n, "vkCmdCuLaunchKernelNVX")) return (PFN_vkVoidFunction)&CmdCuLaunchKernelNVX;
  return nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL GetDeviceProcAddr(VkDevice device, const char *n) {
  if (!std::strcmp(n, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)&GetDeviceProcAddr;
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  if (!d) return nullptr;
  if (PFN_vkVoidFunction f = gancho(n)) {
    if (!d->gdpa(device, n)) return nullptr;   /* vkQueueSubmit2 sin 1.3, por ejemplo */
    return f;
  }
  if (PFN_vkVoidFunction f = ganchoNvx(n)) return d->gdpa(device, n) ? f : nullptr;
  return d->gdpa(device, n);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL GetInstanceProcAddr(VkInstance inst, const char *n) {
  if (!std::strcmp(n, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)&GetInstanceProcAddr;
  if (!std::strcmp(n, "vkCreateInstance")) return (PFN_vkVoidFunction)&CreateInstance;
  if (!std::strcmp(n, "vkDestroyInstance")) return (PFN_vkVoidFunction)&DestroyInstance;
  if (!std::strcmp(n, "vkCreateDevice")) return (PFN_vkVoidFunction)&CreateDevice;
  if (!std::strcmp(n, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)&GetDeviceProcAddr;
  if (!inst) return nullptr;
  PFN_vkGetInstanceProcAddr siguiente;
  {
    std::lock_guard<std::mutex> g(g_mx);
    auto it = g_inst.find(llave(inst));
    if (it == g_inst.end()) return nullptr;
    siguiente = it->second.gipa;
  }
  PFN_vkVoidFunction abajo = siguiente(inst, n);
  /* los de dispositivo, solo si los de abajo existen (vkQueueSubmit2 sin 1.3, los de NVX sin NVIDIA) */
  if (abajo) {
    if (PFN_vkVoidFunction f = gancho(n)) return f;
    if (PFN_vkVoidFunction f = ganchoNvx(n)) return f;
  }
  return abajo;
}

}  // namespace

EXPORTA VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *v) {
  if (!v || v->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) return VK_ERROR_INITIALIZATION_FAILED;
  if (v->loaderLayerInterfaceVersion >= 2) {
    v->pfnGetInstanceProcAddr = &GetInstanceProcAddr;
    v->pfnGetDeviceProcAddr = &GetDeviceProcAddr;
    v->pfnGetPhysicalDeviceProcAddr = nullptr;
    v->loaderLayerInterfaceVersion = 2;
  }
  return VK_SUCCESS;
}
