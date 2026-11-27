/* efectos_vk.cpp — (v1.18) la cadena de efectos de ReShade en la tarjeta. Ver efectos_vk.h.
   Lo de cada pase (formatos, mezclas, muestreadores, el back buffer a turnos) sigue lo que hace vkDaedalus en
   src/effects/effect_reshade.cpp (zlib), escrito otra vez para el visor: una sola fuente por cuadro en lugar de una por
   imagen del intercambio, todo en GENERAL y con el dibujado dinamico de Vulkan 1.3 que ya usa la trama. */
#include "efectos_vk.h"

#include "util.h"
#include "vkdev.h"

#include "effect_module.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <random>

bool pngLoadRGBA(const std::string &path, int &w, int &h, std::vector<uint8_t> &rgba);   /* glutil.cpp */

namespace {
using reshadefx::effect_module;

constexpr VkFormat kFondo = VK_FORMAT_R8G8B8A8_UNORM;   /* el back buffer */
constexpr uint32_t kProfW = 960, kProfH = 540;          /* la profundidad de los efectos (el mapa del 3D es mas pequeno) */

VkFormat formatoDe(reshadefx::texture_format f) {
  using F = reshadefx::texture_format;
  switch (f) {
    case F::r8: return VK_FORMAT_R8_UNORM;
    case F::r16f: return VK_FORMAT_R16_SFLOAT;
    case F::r16: return VK_FORMAT_R16_UNORM;
    case F::r32f: return VK_FORMAT_R32_SFLOAT;
    case F::r32u: return VK_FORMAT_R32_UINT;
    case F::r32i: return VK_FORMAT_R32_SINT;
    case F::rg8: return VK_FORMAT_R8G8_UNORM;
    case F::rg16f: return VK_FORMAT_R16G16_SFLOAT;
    case F::rg16: return VK_FORMAT_R16G16_UNORM;
    case F::rg32f: return VK_FORMAT_R32G32_SFLOAT;
    case F::rgba8: return VK_FORMAT_R8G8B8A8_UNORM;
    case F::rgba16f: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case F::rgba16: return VK_FORMAT_R16G16B16A16_UNORM;
    case F::rgba32f: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case F::rgba32u: return VK_FORMAT_R32G32B32A32_UINT;
    case F::rgba32i: return VK_FORMAT_R32G32B32A32_SINT;
    case F::rgb10a2: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case F::rg11b10f: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    default: return VK_FORMAT_R8G8B8A8_UNORM;
  }
}
/* su pareja sRGB (solo RGBA8 la tiene) */
VkFormat srgbDe(VkFormat f) { return f == VK_FORMAT_R8G8B8A8_UNORM ? VK_FORMAT_R8G8B8A8_SRGB : f; }

VkBlendFactor factorDe(reshadefx::blend_factor b) {
  using B = reshadefx::blend_factor;
  switch (b) {
    case B::zero: return VK_BLEND_FACTOR_ZERO;
    case B::one: return VK_BLEND_FACTOR_ONE;
    case B::source_color: return VK_BLEND_FACTOR_SRC_COLOR;
    case B::one_minus_source_color: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case B::dest_color: return VK_BLEND_FACTOR_DST_COLOR;
    case B::one_minus_dest_color: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case B::source_alpha: return VK_BLEND_FACTOR_SRC_ALPHA;
    case B::one_minus_source_alpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case B::dest_alpha: return VK_BLEND_FACTOR_DST_ALPHA;
    case B::one_minus_dest_alpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  }
  return VK_BLEND_FACTOR_ONE;
}
VkBlendOp opDe(reshadefx::blend_op o) {
  using O = reshadefx::blend_op;
  switch (o) {
    case O::add: return VK_BLEND_OP_ADD;
    case O::subtract: return VK_BLEND_OP_SUBTRACT;
    case O::reverse_subtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
    case O::min: return VK_BLEND_OP_MIN;
    case O::max: return VK_BLEND_OP_MAX;
  }
  return VK_BLEND_OP_ADD;
}
VkPrimitiveTopology topologiaDe(reshadefx::primitive_topology t) {
  using T = reshadefx::primitive_topology;
  switch (t) {
    case T::point_list: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case T::line_list: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case T::line_strip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case T::triangle_strip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
}
VkSamplerAddressMode direccionDe(reshadefx::texture_address_mode m) {
  using M = reshadefx::texture_address_mode;
  switch (m) {
    case M::wrap: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case M::mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case M::border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    default: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
}

/* una barrera de memoria para todo: entre pases no hace falta afinar mas (son pocos y a pantalla completa) */
void barreraTodo(VkCommandBuffer cb) {
  VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
  mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_UNIFORM_READ_BIT;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0, 1, &mb, 0, nullptr, 0, nullptr);
}

const reshadefx::annotation *anot(const std::vector<reshadefx::annotation> &v, const char *n) {
  for (const auto &a : v)
    if (a.name == n) return &a;
  return nullptr;
}

/* un valor en el bufer de uniformes, en su tipo (los bool van como uint de 32 bits, como los deja el SPIR-V de ReShade) */
void escribir(std::vector<uint8_t> &d, uint32_t off, const reshadefx::type &t, int comp, double v) {
  const size_t o = (size_t)off + (size_t)comp * 4u;
  if (o + 4 > d.size()) return;
  if (t.is_floating_point()) {
    const float f = (float)v;
    std::memcpy(d.data() + o, &f, 4);
  } else if (t.is_boolean()) {
    const uint32_t u = v != 0.0 ? 1u : 0u;
    std::memcpy(d.data() + o, &u, 4);
  } else if (t.is_signed()) {
    const int32_t i = (int32_t)std::lround(v);
    std::memcpy(d.data() + o, &i, 4);
  } else {
    const uint32_t u = (uint32_t)std::max(0L, std::lround(v));
    std::memcpy(d.data() + o, &u, 4);
  }
}
double ahoraS() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace

CadenaEfectos::~CadenaEfectos() { destruir(); }

uint32_t CadenaEfectos::tipoMemoria(uint32_t bits, VkMemoryPropertyFlags props) const {
  for (uint32_t i = 0; i < memProps_.memoryTypeCount; i++)
    if ((bits & (1u << i)) && (memProps_.memoryTypes[i].propertyFlags & props) == props) return i;
  return UINT32_MAX;
}

bool CadenaEfectos::crearImg(Img &i, uint32_t w, uint32_t h, VkFormat f, uint32_t niveles, VkImageUsageFlags uso, std::string *err) {
  VkDevice d = dev_->device();
  i = Img{};
  i.fmt = f, i.w = w, i.h = h, i.niveles = std::max(1u, niveles);
  const bool mutable_ = srgbDe(f) != f;
  VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ci.flags = mutable_ ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0;
  ci.imageType = VK_IMAGE_TYPE_2D;
  ci.format = f;
  ci.extent = {w, h, 1};
  ci.mipLevels = i.niveles;
  ci.arrayLayers = 1;
  ci.samples = VK_SAMPLE_COUNT_1_BIT;
  ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  ci.usage = uso | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (VkResult r = vkCreateImage(d, &ci, nullptr, &i.img); r != VK_SUCCESS) {
    if (err) *err = std::string("vkCreateImage: ") + vkResultName(r);
    return false;
  }
  VkMemoryRequirements mr;
  vkGetImageMemoryRequirements(d, i.img, &mr);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = mr.size;
  ai.memoryTypeIndex = tipoMemoria(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(d, &ai, nullptr, &i.mem) != VK_SUCCESS ||
      vkBindImageMemory(d, i.img, i.mem, 0) != VK_SUCCESS) {
    if (err) *err = "sin memoria para una textura de " + std::to_string(w) + "x" + std::to_string(h);
    borrarImg(i);
    return false;
  }
  for (int s = 0; s < 2; s++) {
    if (s == 1 && !mutable_) {
      i.todo[1] = i.todo[0], i.base[1] = i.base[0];
      break;
    }
    for (int b = 0; b < 2; b++) {
      VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      vi.image = i.img;
      vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
      vi.format = s == 1 ? srgbDe(f) : f;
      vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, b == 0 ? i.niveles : 1u, 0, 1};
      VkImageView &v = b == 0 ? i.todo[s] : i.base[s];
      if (vkCreateImageView(d, &vi, nullptr, &v) != VK_SUCCESS) {
        if (err) *err = "vkCreateImageView";
        borrarImg(i);
        return false;
      }
    }
  }
  return true;
}

void CadenaEfectos::borrarImg(Img &i) {
  if (!dev_ || !dev_->ok()) return;
  VkDevice d = dev_->device();
  for (int s = 1; s >= 0; s--) {
    if (s == 1 && i.todo[1] == i.todo[0]) continue;
    if (i.todo[s]) vkDestroyImageView(d, i.todo[s], nullptr);
    if (i.base[s]) vkDestroyImageView(d, i.base[s], nullptr);
  }
  if (i.img) vkDestroyImage(d, i.img, nullptr);
  if (i.mem) vkFreeMemory(d, i.mem, nullptr);
  i = Img{};
}

void CadenaEfectos::borrarEfecto(Efecto &e) {
  VkDevice d = dev_->device();
  for (Pase &p : e.pases)
    if (p.pipe) vkDestroyPipeline(d, p.pipe, nullptr);
  for (VkShaderModule m : e.modulos) vkDestroyShaderModule(d, m, nullptr);
  for (VkSampler s : e.muestreadores) vkDestroySampler(d, s, nullptr);
  for (Img &i : e.texs) borrarImg(i);
  if (e.pool) vkDestroyDescriptorPool(d, e.pool, nullptr);
  if (e.lay) vkDestroyPipelineLayout(d, e.lay, nullptr);
  if (e.dslUbo) vkDestroyDescriptorSetLayout(d, e.dslUbo, nullptr);
  if (e.dslMuestras) vkDestroyDescriptorSetLayout(d, e.dslMuestras, nullptr);
  if (e.ubo) vkDestroyBuffer(d, e.ubo, nullptr);
  if (e.uboMem) vkFreeMemory(d, e.uboMem, nullptr);
  e = Efecto{};
}

void CadenaEfectos::borrarCadena() {
  if (!dev_ || !dev_->ok()) return;
  for (Efecto &e : cadena_) borrarEfecto(e);
  cadena_.clear();
  ajustesCadena_.clear();
}

void CadenaEfectos::destruir() {
  if (!dev_ || !dev_->ok()) return;
  vkDeviceWaitIdle(dev_->device());
  borrarCadena();
  borrarImg(a_);
  borrarImg(b_);
  borrarImg(prof_);
  claveHecha_.clear();
  w_ = h_ = 0;
}

void CadenaEfectos::ajustar(const efectos::Ajustes &a) {
  ajustes_ = a;
  /* lo que solo cambia valores se aplica ya, sin compilar: el siguiente cuadro los lleva */
  if (!cadena_.empty())
    for (size_t k = 0; k < cadena_.size(); k++)
      for (const efectos::Efecto &e : a.lista)
        if (e.nombre == cadena_[k].nombre) {
          ajustesCadena_[k] = e;
          llenarValores(cadena_[k], e);
        }
  sucia_ = true;
}

/* lo que decide la cadena: los efectos encendidos, sus #define y el tamano (BUFFER_WIDTH/HEIGHT van compilados) */
std::string CadenaEfectos::claveDe(int w, int h) const {
  if (!ajustes_.algoQueAplicar()) return {};
  std::string k = std::to_string(w) + "x" + std::to_string(h);
  for (const efectos::Efecto &e : ajustes_.lista) {
    if (!e.activo) continue;
    k += "|" + e.nombre;
    for (const auto &d : e.defs) k += "@" + d.first + "=" + d.second;
  }
  return k;
}

bool CadenaEfectos::preparar(int w, int h, VkImageView *salida) {
  *salida = VK_NULL_HANDLE;
  if (!dev_ || !dev_->ok() || w <= 0 || h <= 0) return false;
  if (!memLeida_) vkGetPhysicalDeviceMemoryProperties(dev_->phys(), &memProps_), memLeida_ = true;
  const std::string clave = claveDe(w, h);
  if (clave.empty()) {
    if (!cadena_.empty()) {   /* apagados: se suelta todo */
      vkDeviceWaitIdle(dev_->device());
      borrarCadena();
      claveHecha_.clear();
      std::printf("[efectos] apagados\n");
      std::fflush(stdout);
    }
    return false;
  }
  /* lo compilado aparte, si ya esta */
  if (pedido_ && pedido_->listo.load()) {
    std::shared_ptr<Pedido> p = pedido_;
    pedido_.reset();
    if (p->clave == clave) {
      std::string err;
      vkDeviceWaitIdle(dev_->device());
      if (!construir(*p, &err)) {
        std::printf("[efectos] no se pudo armar la cadena: %s\n", err.c_str());
        borrarCadena();
        claveHecha_ = clave;   /* no se reintenta hasta que cambie algo */
      }
      std::printf("[efectos] %s\n", resumen().c_str());
      std::fflush(stdout);
    }
  }
  if (clave != claveHecha_ && !pedido_) {
    /* a compilar en otro hilo (unos ms por efecto, pero que no pare el dibujo) */
    auto p = std::make_shared<Pedido>();
    p->clave = clave, p->w = w, p->h = h;
    for (const efectos::Efecto &e : ajustes_.lista)
      if (e.activo) p->lista.push_back(e);
    pedido_ = p;
    std::thread([p] {
      const auto disp = efectos::disponibles();
      for (const efectos::Efecto &e : p->lista) {
        const auto it = disp.find(e.nombre);
        if (it == disp.end()) {
          p->hechos.push_back(nullptr);
          p->fallos.push_back(e.nombre + ": no esta en la carpeta");
          continue;
        }
        auto c = efectos::compilar(it->second, p->w, p->h, e.defs);
        if (!c->ok) p->fallos.push_back(e.nombre + ": " + c->error.substr(0, 300));
        p->hechos.push_back(c->ok ? c : nullptr);
      }
      p->listo = true;
    }).detach();
  }
  /* mientras compila, la de antes si es del mismo tamano */
  if (cadena_.empty() || w != w_ || h != h_) return false;
  *salida = salidaEnB_ ? b_.todo[0] : a_.todo[0];
  return true;
}

bool CadenaEfectos::construir(Pedido &p, std::string *err) {
  borrarCadena();
  fallos_ = p.fallos;
  if ((int)a_.w != p.w || (int)a_.h != p.h || !a_.img) {
    borrarImg(a_);
    borrarImg(b_);
    if (!crearImg(a_, p.w, p.h, kFondo, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, err) ||
        !crearImg(b_, p.w, p.h, kFondo, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, err))
      return false;
  }
  /* en 16 bits, para que el estirado lineal no pierda nada. Pero el mapa del 3D es de 8 bits: MEDIDO con DisplayDepth, las
     normales salen en terrazas igual (los escalones estan en el mapa). Niebla, desenfoque por profundidad y Depth_Cues van
     bien; la oclusion (MXAO), que mira normales, queda tosca */
  if (!prof_.img && !crearImg(prof_, kProfW, kProfH, VK_FORMAT_R16G16_SFLOAT, 1, 0, err)) return false;
  w_ = p.w, h_ = p.h;
  for (size_t k = 0; k < p.lista.size(); k++) {
    if (!p.hechos[k]) continue;
    Efecto e;
    e.nombre = p.lista[k].nombre;
    e.comp = p.hechos[k];
    std::string e2;
    if (!construirEfecto(e, p.lista[k], &e2)) {
      fallos_.push_back(e.nombre + ": " + e2);
      borrarEfecto(e);
      continue;
    }
    cadena_.push_back(std::move(e));
    ajustesCadena_.push_back(p.lista[k]);
  }
  /* todas las imagenes nuevas a GENERAL y en negro (las de archivo ya se subieron: se dejan) */
  VkCommandBuffer cb = dev_->beginOneTime(err);
  if (!cb) return false;
  auto poner = [&](Img &i, bool limpiar) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = limpiar ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = i.img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, i.niveles, 0, 1};
    b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    if (limpiar) {
      VkClearColorValue c{};
      vkCmdClearColorImage(cb, i.img, VK_IMAGE_LAYOUT_GENERAL, &c, 1, &b.subresourceRange);
    }
  };
  poner(a_, true), poner(b_, true), poner(prof_, true);
  for (Efecto &e : cadena_)
    for (size_t t = 0; t < e.texs.size(); t++)
      if (e.texs[t].img) {
        /* las de archivo (con la anotacion source) ya estan subidas: esas no se borran */
        bool deArchivo = false;
        for (size_t m = 0; m < e.texDe.size(); m++)
          if (e.texDe[m] == (int)t && anot(e.comp->modulo->textures[m].annotations, "source")) deArchivo = true;
        poner(e.texs[t], !deArchivo);
      }
  barreraTodo(cb);
  if (!dev_->endOneTime(cb, err)) return false;
  /* la salida: A si los pases que escriben el back buffer son pares, B si nones */
  int escrituras = 0;
  for (const Efecto &e : cadena_)
    for (const Pase &ps : e.pases)
      if (ps.destinos.empty()) escrituras++;
  salidaEnB_ = escrituras % 2 == 1;
  profPuesta_ = false;
  claveHecha_ = p.clave;
  sucia_ = true;
  return true;
}

bool CadenaEfectos::subirTextura(Img &i, const std::string &ruta, std::string *err) {
  int w = 0, h = 0;
  std::vector<uint8_t> rgba;
#ifdef HAVE_PNG
  if (!pngLoadRGBA(ruta, w, h, rgba) || w <= 0 || h <= 0) {
    if (err) *err = "no se pudo leer " + ruta;
    return false;
  }
#else
  if (err) *err = "sin libpng: no se lee " + ruta;
  return false;
#endif
  /* al tamano que declara el .fx (ReShade la estira igual), por el mas cercano */
  std::vector<uint8_t> px((size_t)i.w * i.h * 4);
  for (uint32_t y = 0; y < i.h; y++)
    for (uint32_t x = 0; x < i.w; x++) {
      const size_t sx = std::min<size_t>((size_t)w - 1, (size_t)x * (size_t)w / i.w), sy = std::min<size_t>((size_t)h - 1, (size_t)y * (size_t)h / i.h);
      std::memcpy(&px[((size_t)y * i.w + x) * 4], &rgba[(sy * (size_t)w + sx) * 4], 4);
    }
  size_t bpp = 4;
  if (i.fmt == VK_FORMAT_R8_UNORM) {
    for (size_t k = 0; k < (size_t)i.w * i.h; k++) px[k] = px[k * 4];
    px.resize((size_t)i.w * i.h), bpp = 1;
  } else if (i.fmt == VK_FORMAT_R8G8_UNORM) {
    for (size_t k = 0; k < (size_t)i.w * i.h; k++) px[k * 2] = px[k * 4], px[k * 2 + 1] = px[k * 4 + 1];
    px.resize((size_t)i.w * i.h * 2), bpp = 2;
  } else if (i.fmt != VK_FORMAT_R8G8B8A8_UNORM) {
    if (err) *err = "textura de archivo en un formato que no es de 8 bits";
    return false;
  }
  VkBuffer buf = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  if (!dev_->createBuffer(px.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, mem, err))
    return false;
  void *p = nullptr;
  vkMapMemory(dev_->device(), mem, 0, px.size(), 0, &p);
  std::memcpy(p, px.data(), px.size());
  vkUnmapMemory(dev_->device(), mem);
  (void)bpp;
  bool ok = false;
  if (VkCommandBuffer cb = dev_->beginOneTime(err)) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = i.img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, i.niveles, 0, 1};
    b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    VkBufferImageCopy c{};
    c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    c.imageExtent = {i.w, i.h, 1};
    vkCmdCopyBufferToImage(cb, buf, i.img, VK_IMAGE_LAYOUT_GENERAL, 1, &c);
    barreraTodo(cb);
    if (i.niveles > 1) mipmaps(cb, i);
    ok = dev_->endOneTime(cb, err);
  }
  vkDestroyBuffer(dev_->device(), buf, nullptr);
  vkFreeMemory(dev_->device(), mem, nullptr);
  return ok;
}

void CadenaEfectos::mipmaps(VkCommandBuffer cb, Img &i) {
  for (uint32_t l = 1; l < i.niveles; l++) {
    VkImageBlit bl{};
    bl.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, l - 1, 0, 1};
    bl.srcOffsets[1] = {(int32_t)std::max(1u, i.w >> (l - 1)), (int32_t)std::max(1u, i.h >> (l - 1)), 1};
    bl.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, l, 0, 1};
    bl.dstOffsets[1] = {(int32_t)std::max(1u, i.w >> l), (int32_t)std::max(1u, i.h >> l), 1};
    vkCmdBlitImage(cb, i.img, VK_IMAGE_LAYOUT_GENERAL, i.img, VK_IMAGE_LAYOUT_GENERAL, 1, &bl, VK_FILTER_LINEAR);
    barreraTodo(cb);
  }
}

bool CadenaEfectos::construirEfecto(Efecto &e, const efectos::Efecto &aj, std::string *err) {
  VkDevice d = dev_->device();
  const effect_module &m = *e.comp->modulo;
  if (m.techniques.empty()) {
    if (err) *err = "no tiene tecnica";
    return false;
  }
  /* la primera tecnica, como vkBasalt cuando el nombre no dice otra */
  const reshadefx::technique &tec = m.techniques[0];
  for (const auto &ps : tec.passes)
    if (!ps.cs_entry_point.empty()) {
      if (err) *err = "tiene pases de computo (aqui no corren)";
      return false;
    }
  /* las texturas */
  e.texDe.assign(m.textures.size(), -1);
  for (size_t t = 0; t < m.textures.size(); t++) {
    const reshadefx::texture &tx = m.textures[t];
    if (tx.semantic == "COLOR") continue;
    if (tx.semantic == "DEPTH") {
      e.texDe[t] = -2;
      continue;
    }
    if (tx.type != reshadefx::texture_type::texture_2d) {
      if (err) *err = "la textura " + tx.name + " no es 2D";
      return false;
    }
    uint32_t todos = 1;
    for (uint32_t s = std::max(tx.width, tx.height); s > 1; s /= 2) todos++;
    Img i;
    if (!crearImg(i, tx.width, tx.height, formatoDe(tx.format), std::min<uint32_t>(tx.levels, todos),
                  tx.render_target ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0, err))
      return false;
    if (const auto *src = anot(tx.annotations, "source")) {
      const std::string ruta = efectos::rutaTextura(src->value.string_data);
      if (ruta.empty()) {
        if (err) *err = "falta la textura " + src->value.string_data;
        borrarImg(i);
        return false;
      }
      if (!subirTextura(i, ruta, err)) {
        borrarImg(i);
        return false;
      }
    }
    e.texDe[t] = (int)e.texs.size();
    e.texs.push_back(i);
  }
  auto texPorNombre = [&](const std::string &unico) -> int {
    for (size_t t = 0; t < m.textures.size(); t++)
      if (m.textures[t].unique_name == unico) return (int)t;
    return -1;
  };
  /* los muestreadores, cada uno en su enlace (su indice), en el conjunto 1 */
  std::vector<VkDescriptorSetLayoutBinding> enl;
  for (size_t s = 0; s < m.samplers.size(); s++) {
    const reshadefx::sampler &sm = m.samplers[s];
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    const uint8_t f = (uint8_t)sm.filter;
    si.magFilter = (f & 0x04) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.minFilter = (f & 0x10) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    si.mipmapMode = (f & 0x01) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = direccionDe(sm.address_u);
    si.addressModeV = direccionDe(sm.address_v);
    si.addressModeW = direccionDe(sm.address_w);
    si.mipLodBias = sm.lod_bias;
    si.minLod = std::max(0.0f, sm.min_lod);
    si.maxLod = sm.max_lod > 1000.0f ? VK_LOD_CLAMP_NONE : std::max(si.minLod, sm.max_lod);
    si.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    VkSampler v = VK_NULL_HANDLE;
    if (vkCreateSampler(d, &si, nullptr, &v) != VK_SUCCESS) {
      if (err) *err = "vkCreateSampler";
      return false;
    }
    e.muestreadores.push_back(v);
    enl.push_back({(uint32_t)s, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
  }
  VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  VkDescriptorSetLayoutBinding bu{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  lci.bindingCount = 1, lci.pBindings = &bu;
  if (vkCreateDescriptorSetLayout(d, &lci, nullptr, &e.dslUbo) != VK_SUCCESS) return false;
  lci.bindingCount = (uint32_t)enl.size(), lci.pBindings = enl.data();
  if (vkCreateDescriptorSetLayout(d, &lci, nullptr, &e.dslMuestras) != VK_SUCCESS) return false;
  VkDescriptorSetLayout sets[2] = {e.dslUbo, e.dslMuestras};
  VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pli.setLayoutCount = 2, pli.pSetLayouts = sets;
  if (vkCreatePipelineLayout(d, &pli, nullptr, &e.lay) != VK_SUCCESS) return false;
  VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)std::max<size_t>(1, m.samplers.size()) * 2}};
  VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pci.maxSets = 3, pci.poolSizeCount = 2, pci.pPoolSizes = ps;
  if (vkCreateDescriptorPool(d, &pci, nullptr, &e.pool) != VK_SUCCESS) return false;
  VkDescriptorSetLayout tres[3] = {e.dslUbo, e.dslMuestras, e.dslMuestras};
  VkDescriptorSet salen[3];
  VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  dai.descriptorPool = e.pool, dai.descriptorSetCount = 3, dai.pSetLayouts = tres;
  if (vkAllocateDescriptorSets(d, &dai, salen) != VK_SUCCESS) return false;
  e.setUbo = salen[0], e.setMuestras[0] = salen[1], e.setMuestras[1] = salen[2];
  /* el bufer de uniformes: se llena en cada cuadro con vkCmdUpdateBuffer */
  const uint32_t tam = std::max<uint32_t>(16, (m.total_uniform_size + 15u) & ~15u);
  if (tam > 65536) {
    if (err) *err = "uniformes de mas de 64 KiB";
    return false;
  }
  if (!dev_->createBuffer(tam, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, e.ubo, e.uboMem, err))
    return false;
  e.datos.assign(tam, 0);
  llenarValores(e, aj);
  for (const auto &u : m.uniforms)
    if (anot(u.annotations, "source")) e.conTiempo = true;
  /* los descriptores: el COLOR va a A en uno y a B en el otro */
  std::vector<VkWriteDescriptorSet> wr;
  std::vector<VkDescriptorImageInfo> ii(m.samplers.size() * 2);
  VkDescriptorBufferInfo bi{e.ubo, 0, tam};
  VkWriteDescriptorSet wu{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  wu.dstSet = e.setUbo, wu.dstBinding = 0, wu.descriptorCount = 1, wu.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, wu.pBufferInfo = &bi;
  wr.push_back(wu);
  for (int k = 0; k < 2; k++)
    for (size_t s = 0; s < m.samplers.size(); s++) {
      const reshadefx::sampler &sm = m.samplers[s];
      const int t = texPorNombre(sm.texture_name);
      const int idx = t < 0 ? -1 : e.texDe[t];
      const int srgb = sm.srgb ? 1 : 0;
      VkImageView v = idx == -1 ? (k == 0 ? a_.todo[srgb] : b_.todo[srgb]) : idx == -2 ? prof_.todo[0] : e.texs[idx].todo[srgb];
      ii[k * m.samplers.size() + s] = {e.muestreadores[s], v, VK_IMAGE_LAYOUT_GENERAL};
      VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      w.dstSet = e.setMuestras[k], w.dstBinding = (uint32_t)s, w.descriptorCount = 1;
      w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, w.pImageInfo = &ii[k * m.samplers.size() + s];
      wr.push_back(w);
    }
  vkUpdateDescriptorSets(d, (uint32_t)wr.size(), wr.data(), 0, nullptr);
  /* los modulos de sombreado: uno por punto de entrada */
  std::map<std::string, VkShaderModule> mods;
  for (const auto &[nombre, palabras] : e.comp->spirv) {
    VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    si.codeSize = palabras.size() * 4, si.pCode = palabras.data();
    VkShaderModule sm = VK_NULL_HANDLE;
    if (vkCreateShaderModule(d, &si, nullptr, &sm) != VK_SUCCESS) {
      if (err) *err = "vkCreateShaderModule " + nombre;
      return false;
    }
    mods[nombre] = sm;
    e.modulos.push_back(sm);
  }
  /* los pases */
  for (const reshadefx::pass &pa : tec.passes) {
    Pase p;
    p.srgb = pa.srgb_write_enable;
    p.limpiar = pa.clear_render_targets;
    p.vertices = pa.num_vertices;
    std::vector<VkFormat> formatos;
    for (int r = 0; r < 8 && !pa.render_target_names[r].empty(); r++) {
      const int t = texPorNombre(pa.render_target_names[r]);
      if (t < 0 || e.texDe[t] < 0) {
        if (err) *err = "el pase " + pa.name + " dibuja en " + pa.render_target_names[r] + ", que no es una textura propia";
        return false;
      }
      p.destinos.push_back(e.texDe[t]);
      const Img &im = e.texs[e.texDe[t]];
      formatos.push_back(p.srgb ? srgbDe(im.fmt) : im.fmt);
      if (im.niveles > 1 && pa.generate_mipmaps) p.mipmaps = true;
    }
    if (p.destinos.empty()) formatos.push_back(p.srgb ? srgbDe(kFondo) : kFondo);
    const uint32_t tw = p.destinos.empty() ? (uint32_t)w_ : e.texs[p.destinos[0]].w;
    const uint32_t th = p.destinos.empty() ? (uint32_t)h_ : e.texs[p.destinos[0]].h;
    p.vw = pa.viewport_width ? pa.viewport_width : tw;
    p.vh = pa.viewport_height ? pa.viewport_height : th;
    p.cargar = !p.limpiar;
    /* al back buffer: se dibuja en la otra imagen; si el pase mezcla o no la cubre entera, antes se copia la actual */
    if (p.destinos.empty())
      p.copiarAntes = pa.blend_enable[0] || pa.num_vertices != 3 || pa.topology != reshadefx::primitive_topology::triangle_list ||
                      p.vw != tw || p.vh != th;
    const auto vs = mods.find(pa.vs_entry_point), fs = mods.find(pa.ps_entry_point);
    if (vs == mods.end() || fs == mods.end()) {
      if (err) *err = "al pase " + pa.name + " le falta su sombreador";
      return false;
    }
    VkPipelineShaderStageCreateInfo st[2]{};
    st[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs->second, pa.vs_entry_point.c_str(), nullptr};
    st[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs->second, pa.ps_entry_point.c_str(), nullptr};
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = topologiaDe(pa.topology);
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1, vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL, rs.cullMode = VK_CULL_MODE_NONE, rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    std::vector<VkPipelineColorBlendAttachmentState> mezclas(formatos.size());
    for (size_t r = 0; r < formatos.size(); r++) {
      VkPipelineColorBlendAttachmentState &b = mezclas[r];
      b.blendEnable = pa.blend_enable[r] ? VK_TRUE : VK_FALSE;
      b.srcColorBlendFactor = factorDe(pa.source_color_blend_factor[r]);
      b.dstColorBlendFactor = factorDe(pa.dest_color_blend_factor[r]);
      b.colorBlendOp = opDe(pa.color_blend_op[r]);
      b.srcAlphaBlendFactor = factorDe(pa.source_alpha_blend_factor[r]);
      b.dstAlphaBlendFactor = factorDe(pa.dest_alpha_blend_factor[r]);
      b.alphaBlendOp = opDe(pa.alpha_blend_op[r]);
      b.colorWriteMask = pa.render_target_write_mask[r] & 0xF;
    }
    VkPipelineColorBlendStateCreateInfo cbs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cbs.attachmentCount = (uint32_t)mezclas.size(), cbs.pAttachments = mezclas.data();
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    ds.dynamicStateCount = 2, ds.pDynamicStates = dyn;
    VkPipelineRenderingCreateInfo rci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rci.colorAttachmentCount = (uint32_t)formatos.size(), rci.pColorAttachmentFormats = formatos.data();
    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.pNext = &rci;
    gp.stageCount = 2, gp.pStages = st;
    gp.pVertexInputState = &vi, gp.pInputAssemblyState = &ia, gp.pViewportState = &vp, gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms, gp.pColorBlendState = &cbs, gp.pDynamicState = &ds, gp.layout = e.lay;
    if (VkResult r = vkCreateGraphicsPipelines(d, VK_NULL_HANDLE, 1, &gp, nullptr, &p.pipe); r != VK_SUCCESS) {
      if (err) *err = "el pase " + pa.name + ": vkCreateGraphicsPipelines " + vkResultName(r);
      return false;
    }
    e.pases.push_back(p);
  }
  return true;
}

void CadenaEfectos::llenarValores(Efecto &e, const efectos::Efecto &aj) {
  if (!e.comp || !e.comp->modulo) return;
  for (const reshadefx::uniform &u : e.comp->modulo->uniforms) {
    if (anot(u.annotations, "source") || !u.type.is_numeric()) continue;
    const int n = (int)std::min<uint32_t>(16, u.type.components() * std::max<uint32_t>(1, u.type.is_array() ? u.type.array_length : 1));
    if (u.has_initializer_value)
      for (int k = 0; k < n; k++)
        escribir(e.datos, u.offset, u.type, k,
                 u.type.is_floating_point() ? (double)u.initializer_value.as_float[k]
                 : u.type.is_signed()        ? (double)u.initializer_value.as_int[k]
                                             : (double)u.initializer_value.as_uint[k]);
    if (u.type.is_matrix() || u.type.is_array()) continue;
    for (int k = 0; k < (int)u.type.rows; k++) {
      const std::string clave = u.type.rows > 1 ? u.name + "[" + std::to_string(k) + "]" : u.name;
      const std::string *v = aj.valor(clave);
      if (!v) continue;
      const double x = *v == "true" ? 1.0 : *v == "false" ? 0.0 : std::strtod(v->c_str(), nullptr);
      escribir(e.datos, u.offset, u.type, k, x);
    }
  }
}

/* los uniformes que pone el programa (las anotaciones `source` de ReShade) */
void CadenaEfectos::llenarFuentes(Efecto &e) {
  static std::mt19937 azar{12345};
  const double t = ahoraS();
  if (t0_ < 0.0) t0_ = t;
  const double dt = tAnterior_ < 0.0 ? 1.0 / 60.0 : std::max(0.0, t - tAnterior_);
  for (const reshadefx::uniform &u : e.comp->modulo->uniforms) {
    const auto *s = anot(u.annotations, "source");
    if (!s) continue;
    const std::string &src = s->value.string_data;
    if (src == "timer") escribir(e.datos, u.offset, u.type, 0, (t - t0_) * 1000.0);
    else if (src == "frametime") escribir(e.datos, u.offset, u.type, 0, dt * 1000.0);
    else if (src == "framecount") escribir(e.datos, u.offset, u.type, 0, (double)(cuadros_ % 0x7fffffff));
    else if (src == "random") {
      int lo = 0, hi = 32767;
      if (const auto *a = anot(u.annotations, "min")) lo = a->value.as_int[0];
      if (const auto *a = anot(u.annotations, "max")) hi = a->value.as_int[0];
      if (hi < lo) std::swap(lo, hi);
      for (uint32_t k = 0; k < u.type.rows; k++) escribir(e.datos, u.offset, u.type, (int)k, lo + (int)(azar() % (uint32_t)(hi - lo + 1)));
    } else if (src == "date") {
      const time_t ahora = std::time(nullptr);
      struct tm lt{};
      localtime_r(&ahora, &lt);
      const double v[4] = {(double)lt.tm_year + 1900, (double)lt.tm_mon + 1, (double)lt.tm_mday, (double)(lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec)};
      for (uint32_t k = 0; k < u.type.rows && k < 4; k++) escribir(e.datos, u.offset, u.type, (int)k, v[k]);
    } else if (src == "pingpong") {
      /* va y viene entre min y max a `step` por segundo: (valor, direccion) */
      float lo = 0.0f, hi = 1.0f, paso = 1.0f;
      if (const auto *a = anot(u.annotations, "min")) lo = a->type.is_floating_point() ? a->value.as_float[0] : (float)a->value.as_int[0];
      if (const auto *a = anot(u.annotations, "max")) hi = a->type.is_floating_point() ? a->value.as_float[0] : (float)a->value.as_int[0];
      if (const auto *a = anot(u.annotations, "step")) paso = a->type.is_floating_point() ? a->value.as_float[0] : (float)a->value.as_int[0];
      float v = 0.0f, dir = 1.0f;
      std::memcpy(&v, e.datos.data() + u.offset, 4);
      if (u.type.rows > 1) std::memcpy(&dir, e.datos.data() + u.offset + 4, 4);
      if (dir == 0.0f) dir = 1.0f;
      v += dir * paso * (float)dt;
      if (v >= hi) v = hi, dir = -1.0f;
      if (v <= lo) v = lo, dir = 1.0f;
      escribir(e.datos, u.offset, u.type, 0, v);
      if (u.type.rows > 1) escribir(e.datos, u.offset, u.type, 1, dir);
    } else if (src == "bufready_depth") {
      escribir(e.datos, u.offset, u.type, 0, profPuesta_ ? 1.0 : 0.0);
    }
    /* key, mousepoint, overlay_open...: en cero (el visor no se los da) */
  }
}

void CadenaEfectos::grabar(VkCommandBuffer cb, VkImage fuente, int w, int h, VkImage prof, int pw, int ph) {
  if (cadena_.empty() || w != w_ || h != h_) return;
  cuadros_++;
  const bool conTiempo = std::any_of(cadena_.begin(), cadena_.end(), [](const Efecto &e) { return e.conTiempo; });
  for (Efecto &e : cadena_)
    if (e.conTiempo) llenarFuentes(e);
  tAnterior_ = ahoraS();
  /* que acabe lo que leia o escribia estas imagenes en el cuadro anterior (la misma cola: la barrera lo cubre) */
  {
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
  }
  for (Efecto &e : cadena_) vkCmdUpdateBuffer(cb, e.ubo, 0, e.datos.size(), e.datos.data());
  /* la fuente a A (convierte BGRA a RGBA si hace falta) */
  VkImageBlit bl{};
  bl.srcSubresource = bl.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  bl.srcOffsets[1] = bl.dstOffsets[1] = {w, h, 1};
  vkCmdBlitImage(cb, fuente, VK_IMAGE_LAYOUT_GENERAL, a_.img, VK_IMAGE_LAYOUT_GENERAL, 1, &bl, VK_FILTER_NEAREST);
  /* la profundidad del 3D, estirada a la propia; sin ella, en 0 (lejos) */
  const VkImageSubresourceRange todoRango{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  if (prof && pw > 0 && ph > 0) {
    VkImageBlit bp{};
    bp.srcSubresource = bp.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    bp.srcOffsets[1] = {pw, ph, 1};
    bp.dstOffsets[1] = {(int32_t)prof_.w, (int32_t)prof_.h, 1};
    vkCmdBlitImage(cb, prof, VK_IMAGE_LAYOUT_GENERAL, prof_.img, VK_IMAGE_LAYOUT_GENERAL, 1, &bp, VK_FILTER_LINEAR);
    profPuesta_ = true;
  } else if (profPuesta_ || cuadros_ == 1) {
    VkClearColorValue c{};
    vkCmdClearColorImage(cb, prof_.img, VK_IMAGE_LAYOUT_GENERAL, &c, 1, &todoRango);
    profPuesta_ = false;
  }
  barreraTodo(cb);
  int cur = 0;   /* 0 = A, 1 = B: el back buffer de ahora */
  Img *fondo[2] = {&a_, &b_};
  for (Efecto &e : cadena_) {
    for (const Pase &p : e.pases) {
      const bool alFondo = p.destinos.empty();
      const int otro = 1 - cur;
      if (alFondo && p.copiarAntes) {
        VkImageCopy c{};
        c.srcSubresource = c.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        c.extent = {(uint32_t)w, (uint32_t)h, 1};
        vkCmdCopyImage(cb, fondo[cur]->img, VK_IMAGE_LAYOUT_GENERAL, fondo[otro]->img, VK_IMAGE_LAYOUT_GENERAL, 1, &c);
        barreraTodo(cb);
      }
      VkRenderingAttachmentInfo at[8]{};
      uint32_t n = 0, rw = (uint32_t)w, rh = (uint32_t)h;
      auto poner = [&](VkImageView v, bool cargar) {
        at[n] = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        at[n].imageView = v;
        at[n].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        at[n].loadOp = p.limpiar ? VK_ATTACHMENT_LOAD_OP_CLEAR : cargar ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        at[n].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        n++;
      };
      if (alFondo) {
        poner(fondo[otro]->base[p.srgb ? 1 : 0], p.copiarAntes);
      } else {
        for (int t : p.destinos) poner(e.texs[t].base[p.srgb ? 1 : 0], p.cargar);
        rw = e.texs[p.destinos[0]].w, rh = e.texs[p.destinos[0]].h;
      }
      VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
      ri.renderArea = {{0, 0}, {rw, rh}};
      ri.layerCount = 1;
      ri.colorAttachmentCount = n, ri.pColorAttachments = at;
      vkCmdBeginRendering(cb, &ri);
      VkViewport vp{0.0f, 0.0f, (float)p.vw, (float)p.vh, 0.0f, 1.0f};
      VkRect2D sc{{0, 0}, {std::min(p.vw, rw), std::min(p.vh, rh)}};
      vkCmdSetViewport(cb, 0, 1, &vp);
      vkCmdSetScissor(cb, 0, 1, &sc);
      vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, p.pipe);
      VkDescriptorSet sets[2] = {e.setUbo, e.setMuestras[cur]};
      vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, e.lay, 0, 2, sets, 0, nullptr);
      vkCmdDraw(cb, p.vertices, 1, 0, 0);
      vkCmdEndRendering(cb);
      barreraTodo(cb);
      if (!alFondo && p.mipmaps)
        for (int t : p.destinos)
          if (e.texs[t].niveles > 1) mipmaps(cb, e.texs[t]);
      if (alFondo) cur = otro;
    }
  }
  /* para la trama: la salida, lista para leer */
  VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
  mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
  sucia_ = conTiempo;
}

std::string CadenaEfectos::resumen() const {
  std::string s;
  for (const Efecto &e : cadena_) s += (s.empty() ? "" : " → ") + e.nombre + " (" + std::to_string(e.pases.size()) + " pases)";
  if (s.empty()) s = "ninguno";
  s = "corren: " + s + " sobre " + std::to_string(w_) + "x" + std::to_string(h_);
  for (const std::string &f : fallos_) s += " · NO: " + f;
  return s;
}
