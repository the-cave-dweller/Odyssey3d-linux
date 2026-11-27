/* odysseyvk_capa.cpp — la capa de Vulkan de OdysseyVK que encuentra la profundidad que el juego le da
   a DLSS (v1.13: fase 1, encontrarla; fase 2, compartirla en vivo con el visor).

   Rodolfo: «I actually want my users to be able to use this without neural forge, just with the game
   dlss itself». La red de profundidad adivina; el juego la tiene exacta, y DLSS la recibe en cada
   cuadro. Con Proton, DLSS corre como nucleos de CUDA lanzados por Vulkan (`vkCmdCuLaunchKernelNVX`,
   VK_NVX_binary_import), y a cada imagen que esos nucleos leen le pide antes al controlador un numero
   (`vkGetImageViewHandleNVX`, VK_NVX_image_view_handle). Esta capa mira las dos cosas: entre las
   imagenes cuyo numero aparece en los parametros de un lanzamiento, la que tiene formato de
   profundidad (D16/D24/D32) es la del juego. Para DLSS es su unica entrada de ese formato.

   MEDIDO en Cyberpunk (fase 1): DLSS pide numero para una imagen 1920x1080 D32_SFLOAT, el tamano de
   render; los parametros de sus lanzamientos no la nombran como se esperaba (vienen en la lista de
   «extras» de CUDA, con `extraCount` = 1), asi que se toma la ULTIMA imagen de profundidad a la que
   DLSS pidio numero. Copiada asi, sale identica bit a bit a la del volcado de Neural Forge del mismo
   cuadro.

   Fase 1: un registro y, a peticion, UN cuadro en un archivo (para comparar).
   Fase 2: cada cuadro, la profundidad a la memoria compartida que lee el visor (capa/protocolo.h). La
   tarjeta del juego la escribe directamente en esa memoria (VK_EXT_external_memory_host, que la capa
   enciende al crear el dispositivo), en el mismo buffer de comandos de DLSS; la CPU del juego no la
   toca. Solo mientras el visor diga que la lee (su latido, de menos de 2 s): sin el 3D puesto, el
   juego no paga nada. ODYSSEYVK_COMPARTIR=0 lo apaga.

   Fase 3 (sonda, ODYSSEYVK_CAPA_SONDA=1): al volcar, tambien las imagenes de color de DLSS y el cuadro
   presentado (con el HUD). Fase 3b, con la misma sonda: los destinos de dibujo (vkCmdBeginRendering) de
   cada vkCmdDraw*, para encontrar la imagen donde el juego dibuja el HUD. Al volcar se abre una «ventana»
   hasta la presentacion siguiente: cada dibujo con destino se apunta en el registro ([destinos]) y los
   de la resolucion de salida se copian (destino-<pid>-<n>.raw + .json con su orden, dibujos y limpiado).
   ODYSSEYVK_CAPA_DESTINOS_TRAS=<n> (taller, sin DLSS) la abre en la presentacion n.

   Fase 3c, siempre que se comparte (ODYSSEYVK_HUD=0 lo quita): LA MASCARA DEL HUD. Al acabar el dibujo cuyo
   destino es la imagen del HUD (el unico de color a la resolucion de salida, RGBA8, limpiado a (0,0,0,0),
   entero y sin profundidad: MEDIDO en la fase 3b), esa imagen se reduce en la tarjeta del juego a 960x540
   (dos vkCmdBlitImage a la mitad) y se copia a los huecos del HUD de la memoria compartida (protocolo.h,
   version 2). El visor pone en el plano de la pantalla lo que su alfa marca. ODYSSEYVK_CAPA_HUD_PRUEBA=1
   (taller) toma cualquier destino grande como HUD y crea la memoria compartida sin DLSS.

   Se enciende por juego, como MangoHud o Neural Forge: con `ODYSSEYVK=1` en las opciones de
   lanzamiento (el manifiesto la declara capa implicita con esa variable; ODYSSEYVK_DISABLE=1 la quita).
   Sin ella no se carga.
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

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cstddef>

#include <algorithm>
#include <atomic>
#include <cerrno>
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

#include "protocolo.h"

#define EXPORTA extern "C" __attribute__((visibility("default")))

namespace {
/* (v1.18) la cuenta de la generacion de cuadros de MAKO: ver el contador, al final */
void contarReal();

std::mutex g_mx;

/* ---------- registro ---------- */
std::string carpeta() {
  static const std::string c = "/tmp/odysseyvk-capa-" + std::to_string((long)getuid());
  return c;
}
/* los volcados de la sonda (fase 3) pesan cientos de MB: van al disco, no a /tmp (que es RAM) */
std::string carpetaVolcados();
FILE *g_log = nullptr;
bool sonda();
/* (v1.18) Ningun juego puede escribir una linea por cuadro (Rodolfo: «asegurate de que ningun juego pueda escribir una
   linea por cuadro»). MEDIDO en Raji, 27-nov: una linea que salia en cada cuadro dio 16 000 lineas (3 MB) en 8 minutos
   y 10 MB a los 20, en /tmp (que es memoria) y con una escritura por cuadro en el hilo del juego. El tope va aqui, en la
   unica salida del registro, y no en cada llamada: vale tambien para las lineas que se anadan despues. Cada linea del
   codigo (su formato) tiene su cupo: kRafaga seguidas y despues una cada kCada segundos, con cuantas se callaron
   entretanto; la primera vez que se calla una, se dice. Y el archivo entero, kTopeArchivo como mucho. Con la sonda
   (ODYSSEYVK_CAPA_SONDA=1, el taller, que vuelca a proposito cientos de lineas) solo cuenta el tope del archivo */
constexpr double kRafaga = 60.0, kCada = 10.0;
constexpr long kTopeArchivo = 4L << 20;
struct Cupo {
  double fichas = kRafaga, t = -1.0;
  long calladas = 0;
  bool avisada = false;
};
void anota(const char *fmt, ...) {
  static std::mutex mx;   /* solo para el registro: no se llama a nada que tome otro candado */
  static std::unordered_map<const char *, Cupo> cupos;
  static long escrito = 0;
  static bool lleno = false;
  std::lock_guard<std::mutex> g(mx);
  if (lleno) return;
  long calladas = 0;
  bool avisar = false;
  if (!sonda()) {
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    Cupo &c = cupos[fmt];
    if (c.t >= 0.0) c.fichas = std::min(kRafaga, c.fichas + (t - c.t) / kCada);
    c.t = t;
    if (c.fichas < 1.0) {
      if (c.calladas++ == 0 && !c.avisada) c.avisada = avisar = true;
      if (!avisar) return;
    } else {
      c.fichas -= 1.0;
      calladas = c.calladas;
      c.calladas = 0;
    }
  }
  if (!g_log) {
    mkdir(carpeta().c_str(), 0700);
    const std::string r = carpeta() + "/capa-" + std::to_string((long)getpid()) + ".log";
    g_log = std::fopen(r.c_str(), "a");
    if (!g_log) return;
    setvbuf(g_log, nullptr, _IOLBF, 0);
    const long ya = std::ftell(g_log);   /* en «a» el archivo de un pid repetido ya trae lo suyo */
    escrito = ya > 0 ? ya : 0;
  }
  int n = 0;
  if (avisar) {
    /* la primera que se calla: cual es, para saber que linea se repite tanto */
    std::string cual(fmt);
    if (cual.size() > 70) cual = cual.substr(0, 70) + "...";
    n = std::fprintf(g_log, "[registro] esta linea se repite demasiado: desde ahora una cada %.0f s, con cuantas se "
                            "callaron: %s\n", kCada, cual.c_str());
  } else {
    va_list ap;
    va_start(ap, fmt);
    n = std::vfprintf(g_log, fmt, ap);
    va_end(ap);
    if (calladas > 0) n += std::fprintf(g_log, " · (y %ld como esta calladas)", calladas);
    n += std::fputc('\n', g_log) == EOF ? 0 : 1;
  }
  escrito += n > 0 ? n : 0;
  if (escrito >= kTopeArchivo) {
    std::fprintf(g_log, "[registro] lleno (%ld MB): no se apunta nada mas en este juego\n", kTopeArchivo >> 20);
    lleno = true;
  }
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
  PFN_vkGetPhysicalDeviceProperties2 props2 = nullptr;
  PFN_vkEnumerateDeviceExtensionProperties enumExt = nullptr;
};
std::unordered_map<void *, Instancia> g_inst;

struct Imagen {
  VkFormat formato;
  uint32_t w, h;
  VkSampleCountFlagBits muestras;
  VkImageUsageFlags uso = 0;
  VkImageCreateFlags flags = 0;   /* (v1.13) para el registro: SPARSE, ALIAS... */
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
  bool color = false;          /* fase 3 (sonda): una imagen de color de DLSS, no la profundidad */
  uint32_t bpp = 4, uso = 0, descriptores = 0;
  const char *tipo = nullptr;  /* "destino": un destino de dibujo (fase 3b); nullptr = color de DLSS */
  std::string extra;           /* mas campos para su .json (ya con su coma delante) */
};

/* fase 3b (sonda): cada dibujo con destino (de vkCmdBeginRendering a vkCmdEndRendering) */
struct Ambito {
  std::vector<VkImage> colores;
  std::vector<int> cargas;                /* VkAttachmentLoadOp de cada uno */
  std::vector<VkClearColorValue> limpia;
  bool conProf = false, reanuda = false, suspende = false;
  uint32_t w = 0, h = 0;
  long dibujos = 0;
};
struct CbSonda {
  bool trasDlss = false;   /* en este buffer de comandos ya se grabo un lanzamiento de DLSS */
  Ambito actual;
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
  PFN_vkCmdUpdateBuffer updateBuf = nullptr;
  PFN_vkCmdEndRenderPass endRenderPass = nullptr;
  VkImage ultimaCreada = VK_NULL_HANDLE;   /* solo para ODYSSEYVK_CAPA_PRUEBA */
  PFN_vkDeviceWaitIdle devWaitIdle = nullptr;
  PFN_vkGetMemoryHostPointerPropertiesEXT hostPtrProps = nullptr;
  VkDeviceSize alineacionHost = 4096;

  /* fase 2: la memoria compartida con el visor */
  int shmFd = -1;
  uint8_t *shm = nullptr;
  size_t shmTam = 0;
  VkBuffer shmBuf = VK_NULL_HANDLE;
  VkDeviceMemory shmMem = VK_NULL_HANDLE;
  uint32_t shmW = 0, shmH = 0;
  VkFormat shmFmt = VK_FORMAT_UNDEFINED;
  /* (v1.18) una medida nueva de la profundidad, mientras aguanta (ver medidaNueva) */
  uint32_t nuevaW = 0, nuevaH = 0;
  VkFormat nuevaFmt = VK_FORMAT_UNDEFINED;
  double nuevaDesde = -1.0;
  uint64_t seq = 0;
  double ultimaCopia = 0.0;
  long copias = 0;
  bool shmFallo = false, latidoVisto = false;

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
  VkImage ultimaPedida = VK_NULL_HANDLE;   /* solo para saber el tamano de render: NO se copia (ver elegirProfundidad) */
  /* (v1.13) la profundidad que se copia: de las imagenes de profundidad a las que DLSS pidio numero, la que el juego
     uso como destino de profundidad mas reciente (en `usoComoProfundidad`, el numero de presentacion) */
  std::unordered_set<VkImage> profundidadesDlss;
  std::unordered_map<VkImage, long> usoComoProfundidad;
  VkImage profElegida = VK_NULL_HANDLE;
  bool profAvisada = false;
  /* (v1.18) las ya anunciadas en el registro. MEDIDO en Raji, 27-nov: el juego turna dos profundidades, una por cuadro,
     y cada cambio se apuntaba: 16 000 lineas (3 MB) en 8 minutos, una escritura por cuadro en el hilo del juego */
  std::unordered_set<VkImage> profAnunciadas;
  /* fase 3 (sonda): las imagenes de color a las que DLSS pidio numero, con los tipos de descriptor (bit 1 =
     para leer con muestreador, bit 3 = de almacenamiento, la que escribe) */
  std::unordered_map<VkImage, uint32_t> colorDlss;
  int volcadosColor = 0, volcadosPresente = 0, autos = 0;
  double ultimoAuto = 0.0;
  /* fase 3 (sonda): el cuadro que se presenta (con el HUD), copiado en vkQueuePresentKHR */
  PFN_vkCreateSwapchainKHR createSwapchain = nullptr;
  PFN_vkGetSwapchainImagesKHR getSwapImages = nullptr;
  PFN_vkQueuePresentKHR present = nullptr;
  PFN_vkGetDeviceQueue getQueue = nullptr;
  PFN_vkGetDeviceQueue2 getQueue2 = nullptr;
  PFN_vkCreateCommandPool createPool = nullptr;
  PFN_vkDestroyCommandPool destroyPool = nullptr;
  PFN_vkAllocateCommandBuffers allocCb = nullptr;
  PFN_vkBeginCommandBuffer beginCb = nullptr;
  PFN_vkEndCommandBuffer endCb = nullptr;
  PFN_vkCreateSemaphore createSem = nullptr;
  PFN_vkDestroySemaphore destroySem = nullptr;
  struct Swap {
    VkFormat fmt = VK_FORMAT_UNDEFINED;
    VkExtent2D ext{};
    bool copiable = false;
    std::vector<VkImage> imgs;
  };
  std::unordered_map<VkSwapchainKHR, Swap> swaps;
  std::unordered_map<VkQueue, uint32_t> familias;
  bool capturarPresente = false;
  long presentes = 0;
  PFN_vkSetDeviceLoaderData setLoaderData = nullptr;
  long extrasVistos = 0;
  /* fase 3b (sonda): los destinos de dibujo, para encontrar la imagen donde el juego dibuja el HUD.
     MEDIDO (fase 3): la salida 4K de DLSS, copiada al volcar, ya tenia trozos del HUD: el juego lo dibuja en
     una imagen propia que comparte memoria con ella. Su alfa seria la mascara exacta. */
  PFN_vkCmdBeginRendering beginRendering = nullptr, beginRenderingKHR = nullptr;
  PFN_vkCmdEndRendering endRendering = nullptr, endRenderingKHR = nullptr;
  PFN_vkCmdBeginRenderPass beginRenderPass = nullptr;
  PFN_vkCmdBeginRenderPass2 beginRenderPass2 = nullptr, beginRenderPass2KHR = nullptr;
  PFN_vkCmdDraw draw = nullptr;
  PFN_vkCmdDrawIndexed drawIndexed = nullptr;
  PFN_vkCmdDrawIndirect drawIndirect = nullptr;
  PFN_vkCmdDrawIndexedIndirect drawIndexedIndirect = nullptr;
  PFN_vkCmdDrawIndirectCount drawIndirectCount = nullptr, drawIndirectCountKHR = nullptr;
  PFN_vkCmdDrawIndexedIndirectCount drawIndexedIndirectCount = nullptr, drawIndexedIndirectCountKHR = nullptr;
  std::unordered_map<VkCommandBuffer, CbSonda> cbs;   /* los nodos no se borran: los punteros a ellos valen */
  /* la ventana: desde el lanzamiento de DLSS de un volcado hasta la presentacion siguiente se apunta cada
     destino de dibujo y se copian los de la resolucion de salida */
  bool ventana = false, ventanaTaller = false;
  long ventanaPresente = 0;
  double ventanaDesde = 0.0;
  int ventanaAmbitos = 0, ventanaCopias = 0;
  std::unordered_map<VkImage, int> ventanaCopiadas;
  int volcadosDestino = 0;
  long pasadasClasicas = 0;
  /* fase 3c: la mascara del HUD al visor (ver protocolo.h, version 2). La imagen del HUD se reduce en la
     tarjeta del juego en dos pasos a la mitad (3840 -> 1920 -> 960: cada punto, la media de 4 x 4, asi un
     trazo fino de una letra no se pierde entre dos muestras) y se copia a la memoria compartida */
  /* las regiones compartidas viejas (la profundidad cambio de tamano): viven hasta cerrar el dispositivo */
  struct Retirada {
    int fd;
    uint8_t *shm;
    size_t tam;
    VkBuffer buf;
    VkDeviceMemory mem;
  };
  std::vector<Retirada> retiradas;
  double shmCreada = 0.0;
  /* (v1.13) lo que la tarjeta del juego lee por nosotros (la profundidad, la capa del HUD, la sonda) no se suelta
     hasta kAplazar presentaciones despues de grabar la lectura: ver aplazarSiLeida */
  PFN_vkBindImageMemory2 bindImg2 = nullptr, bindImg2KHR = nullptr;
  std::unordered_map<VkImage, VkDeviceMemory> memoriaDe;   /* la memoria de cada imagen (vkBindImageMemory*) */
  std::unordered_map<VkImage, long> leidaEn;               /* presentacion en que grabamos la ultima lectura */
  std::unordered_map<VkDeviceMemory, long> memLeidaEn;
  struct Aplazado {
    VkImage img;
    VkDeviceMemory mem;
    const VkAllocationCallbacks *a;
    long desde;
  };
  std::vector<Aplazado> aplazados;
  long aplazadosTotal = 0;
  PFN_vkGetImageMemoryRequirements imgReqs = nullptr;
  PFN_vkBindImageMemory bindImg = nullptr;
  PFN_vkCmdBlitImage blit = nullptr;
  VkImage hudA = VK_NULL_HANDLE, hudB[ODVK_HUD_CAPAS]{};   /* 1920x1080 y una de 960x540 por capa, nuestras */
  VkDeviceMemory hudAMem = VK_NULL_HANDLE, hudBMem[ODVK_HUD_CAPAS]{};
  int hudCapa = 0;                      /* la capa de este cuadro que se escribio la ultima */
  long hudMarca = -1;                   /* el cuadro (presentaciones) de esa capa */
  int hudCapasCuadro = 0, hudCapasMaxVistas = 0;
  std::unordered_set<VkImage> hudVistas;   /* las imagenes ya apuntadas como HUD (una linea cada una) */
  bool hudFallo = false;
  uint64_t hudSeq = 0;
  double hudUltima = 0.0;
  double siguienteCopia = 0.0, hudSiguiente = 0.0;   /* (protocolo v6) el turno de la proxima copia con tope (tocaPorTope) */
  uint32_t topeVisto = 0;        /* (protocolo v6) el ultimo tope de FPS que pidio el visor, para el registro */
  VkImage hudImagen = VK_NULL_HANDLE;   /* la ultima reconocida como HUD (para el registro) */
  long hudCopias = 0;
  int hudQuerido = -1;   /* (protocolo v5) lo ultimo que dijo el visor (-1 = aun nada) */
  uint32_t salidaW = 0, salidaH = 0;    /* la cadena de presentacion: el HUD va a ese tamano */
};
/* sube al destruir un dispositivo: la cache por hilo de los ganchos de dibujo deja de valer */
std::atomic<uint64_t> g_generacion{1};
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
    case VK_FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
    case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
    case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "A2B10G10R10_UNORM";
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "A2R10G10B10_UNORM";
    case VK_FORMAT_R16G16B16A16_UNORM: return "R16G16B16A16_UNORM";
    case VK_FORMAT_R8_UNORM: return "R8_UNORM";
    default: return nullptr;
  }
}
std::string formato(VkFormat f) {
  const char *n = nombreFormato(f);
  return n ? n : ("formato " + std::to_string((int)f));
}

/* fase 3 (sonda): los formatos de color que se guardan, con sus bytes por punto (0 = no se guarda) */
uint32_t bytesColor(VkFormat f) {
  switch (f) {
    case VK_FORMAT_R16G16B16A16_SFLOAT: case VK_FORMAT_R16G16B16A16_UNORM: return 8;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_B8G8R8A8_SRGB: case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      return 4;
    case VK_FORMAT_R8_UNORM: return 1;   /* fase 3b: una mascara de un canal */
    default:
      return 0;
  }
}
bool sonda() {
  static const bool s = [] {
    const char *e = std::getenv("ODYSSEYVK_CAPA_SONDA");
    return e && e[0] == '1';
  }();
  return s;
}
/* fase 2: la profundidad al visor, salvo ODYSSEYVK_COMPARTIR=0 */
bool compartirActivo() {
  static const bool c = [] {
    const char *e = std::getenv("ODYSSEYVK_COMPARTIR");
    return !(e && e[0] == '0');
  }();
  return c;
}
/* fase 3c: la mascara del HUD va con la profundidad salvo ODYSSEYVK_HUD=0 (o sin compartir nada) */
bool hudActivo() {
  static const bool h = [] {
    const char *e = std::getenv("ODYSSEYVK_HUD");
    const char *c = std::getenv("ODYSSEYVK_COMPARTIR");
    return !(e && e[0] == '0') && !(c && c[0] == '0');
  }();
  return h;
}
/* ODYSSEYVK_CAPA_HUD_PRUEBA=1 (taller, sin juego): cualquier destino de color grande de un solo color vale como
   HUD, y la memoria compartida se crea sin DLSS. Sirve para probar el camino de la tarjeta con el visor */
bool hudPrueba() {
  static const bool p = [] {
    const char *e = std::getenv("ODYSSEYVK_CAPA_HUD_PRUEBA");
    return e && e[0] == '1';
  }();
  return p;
}
bool esRgba8(VkFormat f) {
  return f == VK_FORMAT_R8G8B8A8_UNORM || f == VK_FORMAT_R8G8B8A8_SRGB || f == VK_FORMAT_B8G8R8A8_UNORM ||
         f == VK_FORMAT_B8G8R8A8_SRGB;
}

std::string carpetaVolcados() {
  if (!sonda()) return carpeta();
  static const std::string c = [] {
    const char *h = std::getenv("HOME");
    std::string base = std::string(h ? h : "/tmp") + "/.cache";
    mkdir(base.c_str(), 0755);
    base += "/odysseyvk";
    mkdir(base.c_str(), 0755);
    base += "/sonda";
    mkdir(base.c_str(), 0700);
    return base;
  }();
  return c;
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
  /* ODYSSEYVK_CAPA_VOLCAR_CADA=<s>: uno cada tantos segundos, como mucho ODYSSEYVK_CAPA_VOLCAR_MAX (4) */
  if (const char *e = std::getenv("ODYSSEYVK_CAPA_VOLCAR_CADA")) {
    const char *m = std::getenv("ODYSSEYVK_CAPA_VOLCAR_MAX");
    const int maximo = m ? std::atoi(m) : 4;
    const double cada = std::atof(e);
    if (cada > 0.0 && d->primerLanzamiento > 0.0 && d->autos < maximo && t - std::max(d->ultimoAuto, d->primerLanzamiento) >= cada) {
      d->ultimoAuto = t;
      d->autos++;
      d->armado = true;
      anota("[volcar] automatico %d de %d (cada %.0f s)", d->autos, maximo, cada);
    }
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

/* ---------- (v1.13) que el juego no suelte lo que leemos ----------
   MEDIDO en Cyberpunk (26-nov, cuatro veces de 16:50 a 17:25, siempre con DLSS en Ultra Performance y al entrar o
   salir del menu): `NVRM: Xid 31 ... MMU Fault: ENGINE CE0 ... ACCESS_TYPE_VIRT_READ` (el motor de copia leyendo
   memoria que ya no existia) con el juego en `TXAA_Blurs`, la lista donde corre DLSS y donde grabamos la copia de
   la profundidad. Sin la capa, con el mismo modo y el mismo menu, no se cae. La copia se graba al grabar la lista
   y la tarjeta la hace despues: si entre medias el juego suelta esa imagen (porque para el ya no la usa: al rehacer
   sus imagenes, DLSS pide numero a varias seguidas y no sabemos cual lee en ese cuadro), la tarjeta lee memoria
   que ya no existe. Por eso, una imagen que leimos hace menos de kAplazar presentaciones no se destruye cuando el
   juego lo pide, ni la memoria donde esta: se apartan y se sueltan de verdad kAplazar presentaciones despues
   (en vkQueuePresentKHR), cuando la lectura ya acabo hace rato. Cada vez sale en el registro ([aplazar]). */
constexpr long kAplazar = 10;
/* con g_mx tomado: la tarjeta va a leer `img` (grabado ahora mismo en un buffer de comandos del juego) */
void marcarLectura(Dispositivo *d, VkImage img) {
  d->leidaEn[img] = d->presentes;
  auto m = d->memoriaDe.find(img);
  if (m != d->memoriaDe.end()) d->memLeidaEn[m->second] = d->presentes;
}
/* ODYSSEYVK_CAPA_APLAZAR_TODO=1 (taller, sin juego): todo lo que se destruye o se suelta pasa por el aplazado */
bool aplazarTodo() {
  static const bool t = [] {
    const char *e = std::getenv("ODYSSEYVK_CAPA_APLAZAR_TODO");
    return e && e[0] == '1';
  }();
  return t;
}

bool grabarCopiaImagen(Dispositivo *d, VkCommandBuffer cb, VkImage img, const Imagen &im, bool color, uint32_t descriptores,
                       const char *tipo = nullptr, const std::string &extra = std::string()) {
  if (im.muestras != VK_SAMPLE_COUNT_1_BIT) {
    anota("[volcar] la imagen tiene %d muestras (MSAA): no se puede copiar asi", (int)im.muestras);
    return false;
  }
  const bool dos = im.formato == VK_FORMAT_D16_UNORM || im.formato == VK_FORMAT_D16_UNORM_S8_UINT;
  const uint32_t bpp = color ? bytesColor(im.formato) : (dos ? 2 : 4);
  if (!bpp) return false;
  const VkDeviceSize tam = (VkDeviceSize)im.w * im.h * bpp;
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = tam;
  bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkBuffer buf = VK_NULL_HANDLE;
  if (d->createBuffer(d->dev, &bi, nullptr, &buf) != VK_SUCCESS) { anota("[volcar] vkCreateBuffer fallo"); return false; }
  VkMemoryRequirements mr{};
  d->bufReqs(d->dev, buf, &mr);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = mr.size;
  ai.memoryTypeIndex = tipoMemoria(d, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VkDeviceMemory mem = VK_NULL_HANDLE;
  if (ai.memoryTypeIndex == UINT32_MAX || d->alloc(d->dev, &ai, nullptr, &mem) != VK_SUCCESS) {
    anota("[volcar] sin memoria visible para la copia");
    d->destroyBuffer(d->dev, buf, nullptr);
    return false;
  }
  d->bind(d->dev, buf, mem, 0);
  VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
  mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  d->barrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
  VkBufferImageCopy reg{};
  reg.imageSubresource = {(VkImageAspectFlags)(color ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT), 0, 0, 1};
  reg.imageExtent = {im.w, im.h, 1};
  d->copyImgBuf(cb, img, VK_IMAGE_LAYOUT_GENERAL, buf, 1, &reg);
  mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;
  d->barrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0,
             nullptr, 0, nullptr);
  marcarLectura(d, img);
  Pendiente p{cb, buf, mem, im.w, im.h, im.formato};
  p.color = color, p.bpp = bpp, p.uso = (uint32_t)im.uso, p.descriptores = descriptores;
  p.tipo = tipo, p.extra = extra;
  d->pendientes.push_back(p);
  anota("[volcar] copia grabada: %ux%u %s%s", im.w, im.h, formato(im.formato).c_str(),
        tipo ? " (destino de dibujo)" : color ? " (color de DLSS)" : "");
  return true;
}

/* ---------- fase 3b (sonda): la ventana de los destinos de dibujo ---------- */
void abrirVentana(Dispositivo *d, bool taller) {
  d->ventana = true;
  d->ventanaTaller = taller;
  d->ventanaPresente = d->presentes;
  d->ventanaDesde = ahora();
  d->ventanaAmbitos = d->ventanaCopias = 0;
  d->ventanaCopiadas.clear();
  anota("[destinos] ventana abierta%s: cada dibujo con destino hasta la presentacion siguiente (pasadas clasicas "
        "hasta ahora: %ld)", taller ? " (ODYSSEYVK_CAPA_DESTINOS_TRAS)" : "", d->pasadasClasicas);
}
void cerrarVentana(Dispositivo *d, const char *por) {
  if (!d->ventana) return;
  d->ventana = false;
  anota("[destinos] ventana cerrada (%s): %d dibujos con destino, %d copiados", por, d->ventanaAmbitos, d->ventanaCopias);
}

/* el volcado: la profundidad y, con la sonda (fase 3), cada imagen de color de DLSS que se pueda copiar */
void grabarCopia(Dispositivo *d, VkCommandBuffer cb, VkImage img, const Imagen &im) {
  grabarCopiaImagen(d, cb, img, im, false, 0);
  if (sonda()) {
    int n = 0;
    for (const auto &par : d->colorDlss) {
      auto it = d->imagenes.find(par.first);
      if (it == d->imagenes.end() || !(it->second.uso & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) || it->second.w < 1280) continue;
      if (n >= 8) break;
      if (grabarCopiaImagen(d, cb, par.first, it->second, true, par.second)) n++;
    }
    anota("[volcar] sonda: %d imagenes de color de DLSS de %zu conocidas", n, d->colorDlss.size());
    d->capturarPresente = true;   /* y el cuadro que se presente ahora, con el HUD */
    abrirVentana(d, false);       /* y los destinos de dibujo de aqui a esa presentacion (fase 3b) */
  }
  d->armado = false;
}

/* con g_mx tomado: saca las copias grabadas en estos buffers de comandos (ya enviados) */
std::vector<Pendiente> sacarPendientes(Dispositivo *d, const std::vector<VkCommandBuffer> &enviados) {
  std::vector<Pendiente> listos, quedan;
  for (const Pendiente &p : d->pendientes) {
    bool este = false;
    for (VkCommandBuffer cb : enviados)
      if (cb == p.cb) este = true;
    (este ? listos : quedan).push_back(p);
  }
  d->pendientes.swap(quedan);
  return listos;
}
/* SIN g_mx: espera a la cola y escribe los archivos. Esperar con el candado tomado puede dejar al juego parado
   para siempre (ver liberarCompartida) */
void leerPendientes(Dispositivo *d, VkQueue cola, const std::vector<Pendiente> &listos) {
  if (listos.empty()) return;
  d->waitIdle(cola);
  for (const Pendiente &p : listos) {
    void *ptr = nullptr;
    if (p.color && d->map(d->dev, p.mem, 0, VK_WHOLE_SIZE, 0, &ptr) == VK_SUCCESS) {
      int k;
      {
        std::lock_guard<std::mutex> g(g_mx);
        k = p.tipo ? ++d->volcadosDestino : ++d->volcadosColor;
      }
      const std::string base = carpetaVolcados() + "/" + (p.tipo ? p.tipo : "color") + "-" + std::to_string((long)getpid()) +
                               "-" + std::to_string(k);
      if (FILE *o = std::fopen((base + ".raw").c_str(), "wb")) {
        std::fwrite(ptr, 1, (size_t)p.w * p.h * p.bpp, o);
        std::fclose(o);
      }
      if (FILE *o = std::fopen((base + ".json").c_str(), "w")) {
        std::fprintf(o, "{\"width\": %u, \"height\": %u, \"format\": \"%s\", \"vkformat\": %d, \"bpp\": %u, "
                        "\"usage\": %u, \"descriptors\": %u, \"pid\": %ld%s}\n",
                     p.w, p.h, formato(p.formato).c_str(), (int)p.formato, p.bpp, p.uso, p.descriptores, (long)getpid(),
                     p.extra.c_str());
        std::fclose(o);
      }
      d->unmap(d->dev, p.mem);
      anota("[volcar] escrito %s.raw (%ux%u %s, uso 0x%x, descriptores 0x%x)", base.c_str(), p.w, p.h,
            formato(p.formato).c_str(), p.uso, p.descriptores);
    } else if (!p.color && d->map(d->dev, p.mem, 0, VK_WHOLE_SIZE, 0, &ptr) == VK_SUCCESS) {
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
      int k;
      {
        std::lock_guard<std::mutex> g(g_mx);
        k = ++d->volcados;
      }
      const std::string base = carpetaVolcados() + "/profundidad-" + std::to_string((long)getpid()) + "-" + std::to_string(k);
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
}

/* ---------- fase 2: la memoria compartida con el visor (capa/protocolo.h) ---------- */
/* al salir el proceso del juego se borra la que hizo este proceso: muchos juegos (Cyberpunk) salen sin
   destruir su dispositivo de Vulkan, y quedaba en /dev/shm (el visor tambien la borra si ve que el juego
   ya no existe) */
struct BorrarAlSalir {
  std::string nombre;
  ~BorrarAlSalir() {
    if (nombre.empty()) return;
    const int fd = shm_open(nombre.c_str(), O_RDONLY, 0);
    if (fd < 0) return;
    OdvkProfCabecera c{};
    const bool mia = pread(fd, &c, sizeof c, 0) == (ssize_t)sizeof c && c.pid == (int32_t)getpid();
    close(fd);
    if (mia) shm_unlink(nombre.c_str());
  }
} g_borrarAlSalir;
std::string nombreShm() {
  if (const char *e = std::getenv("ODYSSEYVK_PROFUNDIDAD_SHM")) return e;
  return "/odysseyvk-profundidad-" + std::to_string((long)getuid());
}
uint64_t monotonoNs() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

/* Suelta la region de ahora SIN esperar a la tarjeta: solo vale para una que la tarjeta no ha usado (un fallo al
   crearla) o al cerrar el dispositivo, despues de esperarla (cerrarCompartidas).
   MEDIDO (26-nov 16:50, Cyberpunk congelado al cambiar DLSS de 1920x1080 a 1280x720): antes aqui se esperaba a
   la tarjeta (vkDeviceWaitIdle) dentro del lanzamiento de DLSS y con g_mx tomado. La tarjeta esperaba trabajo que
   el juego aun no habia enviado, y el hilo que lo envia estaba parado en g_mx (vkQueueSubmit y, desde la fase
   3c, cada vkCmdBeginRendering pasan por el): bloqueo para siempre. Ahora la region vieja se retira entera
   (retirarCompartida) y vive hasta que se cierra el dispositivo: la tarjeta aun puede estar copiando en ella, y
   vkd3d-proton puede volver a enviar un buffer de comandos ya grabado. */
void liberarCompartida(Dispositivo *d, bool borrar) {
  if (d->shmBuf) d->destroyBuffer(d->dev, d->shmBuf, nullptr);
  if (d->shmMem) d->free(d->dev, d->shmMem, nullptr);
  d->shmBuf = VK_NULL_HANDLE;
  d->shmMem = VK_NULL_HANDLE;
  if (d->shm) munmap(d->shm, d->shmTam);
  if (d->shmFd >= 0) close(d->shmFd);
  d->shm = nullptr;
  d->shmFd = -1;
  d->shmTam = 0;
  if (borrar) shm_unlink(nombreShm().c_str());
}

/* la region de ahora se aparta sin tocarla (ver liberarCompartida) y su nombre se borra: la nueva sera otro
   objeto (otro inodo, el visor la vuelve a abrir), y la vieja sigue viva mientras este mapeada aqui */
void retirarCompartida(Dispositivo *d) {
  if (!d->shm) return;
  d->retiradas.push_back({d->shmFd, d->shm, d->shmTam, d->shmBuf, d->shmMem});
  d->shmFd = -1, d->shm = nullptr, d->shmTam = 0, d->shmBuf = VK_NULL_HANDLE, d->shmMem = VK_NULL_HANDLE;
  shm_unlink(nombreShm().c_str());
  anota("[compartir] la region vieja se aparta (%zu apartadas): se suelta al cerrar el dispositivo", d->retiradas.size());
}
/* al cerrar el dispositivo, fuera de g_mx: ahi si se puede esperar a la tarjeta */
void cerrarCompartidas(Dispositivo *d) {
  const bool habia = d->shm != nullptr;
  if (habia || !d->retiradas.empty() || d->hudA) d->devWaitIdle(d->dev);
  liberarCompartida(d, habia);   /* y se borra: el visor vuelve a la red */
  for (const Dispositivo::Retirada &r : d->retiradas) {
    if (r.buf) d->destroyBuffer(d->dev, r.buf, nullptr);
    if (r.mem) d->free(d->dev, r.mem, nullptr);
    if (r.shm) munmap(r.shm, r.tam);
    if (r.fd >= 0) close(r.fd);
  }
  d->retiradas.clear();
}

/* la region entera (cabecera + 3 huecos), mapeada aqui e importada en Vulkan */
bool crearCompartida(Dispositivo *d, const Imagen &im) {
  retirarCompartida(d);
  const bool dos = im.formato == VK_FORMAT_D16_UNORM || im.formato == VK_FORMAT_D16_UNORM_S8_UINT;
  const uint64_t bpp = dos ? 2 : 4;
  const uint64_t a = d->alineacionHost < 4096 ? 4096 : d->alineacionHost;
  auto redondea = [a](uint64_t v) { return (v + a - 1) / a * a; };
  const uint64_t hueco = redondea(ODVK_PROF_DATOS + (uint64_t)im.w * im.h * bpp + 8);
  /* (version 2) y detras, los tres de la mascara del HUD: 2 MB por capa (version 3: dos capas) */
  const uint64_t hudInicio = redondea(ODVK_PROF_CABECERA + ODVK_PROF_HUECOS * hueco);
  const uint64_t hudHueco = redondea(ODVK_PROF_DATOS + ODVK_HUD_CAPAS * (uint64_t)ODVK_HUD_ANCHO * ODVK_HUD_ALTO * 4 + 8);
  const uint64_t total = hudInicio + ODVK_PROF_HUECOS * hudHueco;
  const std::string nombre = nombreShm();
  shm_unlink(nombre.c_str());   /* siempre un objeto nuevo: la de un juego anterior que se cayo, o la retirada */
  d->shmFd = shm_open(nombre.c_str(), O_CREAT | O_RDWR, 0600);
  if (d->shmFd < 0 || ftruncate(d->shmFd, (off_t)total) != 0) {
    anota("[compartir] no se pudo crear %s: %s", nombre.c_str(), std::strerror(errno));
    liberarCompartida(d, false);
    return false;
  }
  /* la direccion tiene que ir alineada a lo que pida el controlador (4096 en NVIDIA): si pide mas, se
     reserva de sobra y se pone la memoria compartida encima, en el punto alineado */
  void *base = MAP_FAILED;
  if (a <= (uint64_t)sysconf(_SC_PAGESIZE)) {
    base = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, d->shmFd, 0);
  } else {
    void *reserva = mmap(nullptr, total + a, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserva != MAP_FAILED) {
      uint8_t *al = (uint8_t *)(((uintptr_t)reserva + a - 1) / a * a);
      base = mmap(al, total, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, d->shmFd, 0);
    }
  }
  if (base == MAP_FAILED) {
    anota("[compartir] mmap fallo: %s", std::strerror(errno));
    liberarCompartida(d, false);
    return false;
  }
  d->shm = (uint8_t *)base;
  d->shmTam = total;
  g_borrarAlSalir.nombre = nombre;
  std::memset(d->shm, 0, ODVK_PROF_CABECERA);
  OdvkProfCabecera *c = (OdvkProfCabecera *)d->shm;
  c->version = ODVK_PROF_VERSION;
  c->pid = (int32_t)getpid();
  c->ancho = im.w, c->alto = im.h;
  c->formato = (uint32_t)im.formato;
  c->bytesPunto = (uint32_t)bpp;
  c->huecos = ODVK_PROF_HUECOS;
  c->tamHueco = hueco;
  c->hudAncho = ODVK_HUD_ANCHO, c->hudAlto = ODVK_HUD_ALTO;
  c->hudBytesPunto = 4;
  c->hudHuecos = ODVK_PROF_HUECOS;
  c->hudInicio = hudInicio;
  c->hudTamHueco = hudHueco;
  c->hudCapasMax = ODVK_HUD_CAPAS;
  d->hudSeq = 0;

  VkMemoryHostPointerPropertiesEXT hp{VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
  if (d->hostPtrProps(d->dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, d->shm, &hp) != VK_SUCCESS) {
    anota("[compartir] vkGetMemoryHostPointerPropertiesEXT fallo");
    liberarCompartida(d, true);
    return false;
  }
  VkExternalMemoryBufferCreateInfo eb{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
  eb.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.pNext = &eb;
  bi.size = total;
  bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if (d->createBuffer(d->dev, &bi, nullptr, &d->shmBuf) != VK_SUCCESS) {
    anota("[compartir] vkCreateBuffer de la memoria compartida fallo");
    liberarCompartida(d, true);
    return false;
  }
  VkMemoryRequirements mr{};
  d->bufReqs(d->dev, d->shmBuf, &mr);
  uint32_t tipo = UINT32_MAX;
  const uint32_t bits = mr.memoryTypeBits & hp.memoryTypeBits;
  for (uint32_t i = 0; i < 32; i++)
    if (bits & (1u << i)) { tipo = i; break; }
  VkImportMemoryHostPointerInfoEXT imp{VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT};
  imp.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
  imp.pHostPointer = d->shm;
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.pNext = &imp;
  ai.allocationSize = total;
  ai.memoryTypeIndex = tipo;
  if (tipo == UINT32_MAX || d->alloc(d->dev, &ai, nullptr, &d->shmMem) != VK_SUCCESS ||
      d->bind(d->dev, d->shmBuf, d->shmMem, 0) != VK_SUCCESS) {
    anota("[compartir] no se pudo importar la memoria compartida en Vulkan (tipo %u)", tipo);
    liberarCompartida(d, true);
    return false;
  }
  d->shmW = im.w, d->shmH = im.h, d->shmFmt = im.formato;
  d->shmCreada = ahora();
  std::atomic_thread_fence(std::memory_order_release);
  c->magia = ODVK_PROF_MAGIA;   /* la ultima: con ella el visor ya puede leer */
  anota("[compartir] %s lista: %ux%u %s, %u huecos de %llu bytes (%.1f MB)", nombre.c_str(), im.w, im.h,
        formato(im.formato).c_str(), ODVK_PROF_HUECOS, (unsigned long long)hueco, total / 1048576.0);
  return true;
}

/* el visor ha dicho que lee en los ultimos 2 s */
bool hayLector(Dispositivo *d) {
  if (!d->shm) return false;
  const uint64_t latido = ((volatile OdvkProfCabecera *)d->shm)->lectorLatido;
  const uint64_t t = monotonoNs();
  return latido > 0 && t >= latido && t - latido < 2000000000ull;
}

/* las cuatro escrituras de la tarjeta (ver protocolo.h), en el buffer de comandos de DLSS */
/* `desde`: el diseno en que esta la imagen si se sabe (modo de prueba, al acabar una pasada de render: comprimida
   en DEPTH_STENCIL_ATTACHMENT_OPTIMAL); UNDEFINED = no se sabe y se copia como GENERAL, que es lo que vale en el
   lanzamiento de DLSS (ahi el juego ya la dejo en un diseno de lectura) */
void grabarCompartida(Dispositivo *d, VkCommandBuffer cb, VkImage img, const Imagen &im, VkImageLayout desde) {
  marcarLectura(d, img);
  const OdvkProfCabecera *c = (const OdvkProfCabecera *)d->shm;
  const uint64_t seq = ++d->seq;
  const uint64_t hueco = odvkHueco(c, seq % ODVK_PROF_HUECOS);
  VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  auto paso = [&](VkPipelineStageFlags de, VkAccessFlags acDe) {
    mb.srcAccessMask = acDe;
    mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    d->barrier(cb, de, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
  };
  paso(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT);
  d->updateBuf(cb, d->shmBuf, hueco, 8, &seq);
  paso(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
  VkBufferImageCopy reg{};
  reg.bufferOffset = hueco + ODVK_PROF_DATOS;
  reg.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
  reg.imageExtent = {im.w, im.h, 1};
  const bool conStencil = im.formato == VK_FORMAT_D16_UNORM_S8_UINT || im.formato == VK_FORMAT_D24_UNORM_S8_UINT ||
                          im.formato == VK_FORMAT_D32_SFLOAT_S8_UINT;
  VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  ib.image = img;
  ib.subresourceRange = {(VkImageAspectFlags)(VK_IMAGE_ASPECT_DEPTH_BIT | (conStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)), 0, 1, 0, 1};
  if (desde != VK_IMAGE_LAYOUT_UNDEFINED) {
    ib.oldLayout = desde, ib.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    ib.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, ib.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    d->barrier(cb, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib);
  }
  d->copyImgBuf(cb, img, desde != VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
                d->shmBuf, 1, &reg);
  if (desde != VK_IMAGE_LAYOUT_UNDEFINED) {
    ib.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ib.newLayout = desde;
    ib.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT, ib.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    d->barrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib);
  }
  paso(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
  d->updateBuf(cb, d->shmBuf, hueco + odvkSeqDespues(c), 8, &seq);
  paso(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
  d->updateBuf(cb, d->shmBuf, offsetof(OdvkProfCabecera, ultimoSeq), 8, &seq);
  mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;
  d->barrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb,
             0, nullptr, 0, nullptr);
  d->copias++;
}

/* (v1.14) El tope de FPS de la capa como MEDIA EXACTA. Cada copia adelanta su turno 1/tope; no vale «al menos 1/tope
   desde la ultima»: los cuadros del juego llegan a saltos fijos (16,7 ms a 60 fps) y con 50 de tope «20 ms desde la
   ultima» deja pasar uno de cada dos, 30 por segundo y no 50 (Rodolfo: «compensate the displayed frames to the
   loss»). Asi salen 5 de cada 6, 50 por segundo. Si el juego va mas lento que el tope, el turno no se queda atras
   mas de un intervalo: se copian todos sus cuadros, sin rafagas. 2 ms de holgura por el temblor del reloj. */
bool tocaPorTope(double &siguiente, double t, uint32_t tope) {
  const double intervalo = 1.0 / (double)tope;
  if (siguiente <= 0.0 || t - siguiente > intervalo) siguiente = t;
  if (t + 0.002 < siguiente) return false;
  siguiente += intervalo;
  return true;
}

/* (v1.18) La memoria compartida se rehace con otra medida de profundidad solo si la nueva aguanta medio segundo. Si un
   juego turnara dos profundidades de medidas distintas (Raji turna dos, pero de la misma), rehacerla en cada cuadro
   apartaria una region por cuadro (decenas de MB de /dev/shm, que es memoria, hasta cerrar el juego: ver
   retirarCompartida) y apuntaria dos lineas por cuadro. Mientras, se copian los cuadros de la medida de ahora. Un
   cambio de verdad (otro modo de DLSS en el menu) se queda: a los 0,5 s se rehace */
bool medidaAguanta(Dispositivo *d, const Imagen &im, double t) {
  if (!d->shm) return true;   /* la primera, sin esperar */
  if (d->nuevaDesde < 0.0 || d->nuevaW != im.w || d->nuevaH != im.h || d->nuevaFmt != im.formato)
    d->nuevaW = im.w, d->nuevaH = im.h, d->nuevaFmt = im.formato, d->nuevaDesde = t;
  return t - d->nuevaDesde >= 0.5;
}

/* en cada lanzamiento de DLSS: como mucho una copia cada 4 ms (una por cuadro hasta 250 por segundo) */
void compartir(Dispositivo *d, VkCommandBuffer cb, VkImage img, const Imagen &im,
               VkImageLayout desde = VK_IMAGE_LAYOUT_UNDEFINED) {
  static const bool kApagado = [] {
    const char *e = std::getenv("ODYSSEYVK_COMPARTIR");
    return e && e[0] == '0';
  }();
  if (kApagado || d->shmFallo || !d->hostPtrProps || !d->updateBuf) return;
  if (im.muestras != VK_SAMPLE_COUNT_1_BIT) return;
  if (d->shm && d->shmW == im.w && d->shmH == im.h && d->shmFmt == im.formato) {
    d->nuevaDesde = -1.0;   /* vuelve la de ahora: la nueva tiene que empezar a contar otra vez */
  } else {
    if (!medidaAguanta(d, im, ahora())) return;
    d->nuevaDesde = -1.0;
    if (!crearCompartida(d, im)) {
      d->shmFallo = true;
      return;
    }
  }
  if (!hayLector(d)) {
    if (d->latidoVisto) anota("[compartir] el visor dejo de leer: sin copias hasta que vuelva");
    d->latidoVisto = false;
    return;
  }
  if (!d->latidoVisto) anota("[compartir] el visor lee: copiando la profundidad cada cuadro");
  d->latidoVisto = true;
  const double t = ahora();
  /* (protocolo v6) el tope de FPS de la capa, el que pida el visor (Ajustes → Fluidez), como media exacta (ver
     tocaPorTope); con o sin tope, una copia cada 4 ms como mucho */
  const uint32_t tope = ((volatile OdvkProfCabecera *)d->shm)->lectorTopeFps;
  if (tope != d->topeVisto) {
    if (tope) anota("[compartir] tope de FPS de la capa: %u por segundo (lo pide el visor)", tope);
    else anota("[compartir] sin tope de FPS en la capa");
    d->topeVisto = tope;
  }
  if (t - d->ultimaCopia < 0.004) return;
  if (tope > 0 && !tocaPorTope(d->siguienteCopia, t, tope)) return;
  d->ultimaCopia = t;
  grabarCompartida(d, cb, img, im, desde);
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
  /* fase 3 (sonda): tambien las de color grandes de los formatos que se guardan, que DLSS puede usar */
  /* (fase 3b) y los destinos de dibujo de color: entre ellos esta donde se dibuja el HUD */
  if (sonda() && bytesColor(c.format) && c.extent.width >= 1280 && c.samples == VK_SAMPLE_COUNT_1_BIT &&
      (c.usage & (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)))
    c.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  /* fase 3c: los destinos RGBA8 grandes, entre los que esta la imagen del HUD, para poder reducirla */
  if (hudActivo() && esRgba8(c.format) && c.extent.width >= 1280 && c.samples == VK_SAMPLE_COUNT_1_BIT &&
      (c.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
    c.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  const VkResult r = d->createImage(device, &c, a, img);
  if (r == VK_SUCCESS) {
    std::lock_guard<std::mutex> g(g_mx);
    d->imagenes[*img] = {c.format, c.extent.width, c.extent.height, c.samples, c.usage, c.flags};
    if (esProfundidad(c.format)) d->ultimaCreada = *img;
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
    d->colorDlss.erase(img);
    if (d->ultimaPedida == img) d->ultimaPedida = VK_NULL_HANDLE;
    if (d->ultimaCreada == img) d->ultimaCreada = VK_NULL_HANDLE;
    d->profundidadesDlss.erase(img);
    d->usoComoProfundidad.erase(img);
    if (d->profElegida == img) d->profElegida = VK_NULL_HANDLE;
    d->profAnunciadas.erase(img);
    /* (v1.13) la leimos hace nada: se aparta (ver kAplazar) */
    auto l = d->leidaEn.find(img);
    const bool aplazar = aplazarTodo() || (l != d->leidaEn.end() && d->presentes - l->second < kAplazar);
    if (l != d->leidaEn.end()) {
      if (aplazar && ++d->aplazadosTotal <= 40)
        anota("[aplazar] el juego destruye %p, que la tarjeta leyo por nosotros hace %ld presentaciones: se destruye "
              "dentro de %ld", (void *)img, d->presentes - l->second, kAplazar);
      d->leidaEn.erase(l);
    } else if (aplazar) {
      d->aplazadosTotal++;
    }
    d->memoriaDe.erase(img);
    if (aplazar) {
      d->aplazados.push_back({img, VK_NULL_HANDLE, a, d->presentes});
      return;
    }
  }
  d->destroyImage(device, img, a);
}

/* (v1.13) la memoria de cada imagen, para no soltarla con una lectura nuestra pendiente (ver kAplazar) */
VKAPI_ATTR VkResult VKAPI_CALL BindImageMemory(VkDevice device, VkImage img, VkDeviceMemory mem, VkDeviceSize off) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  const VkResult r = d->bindImg(device, img, mem, off);
  if (r == VK_SUCCESS && mem != VK_NULL_HANDLE) {
    std::lock_guard<std::mutex> g(g_mx);
    d->memoriaDe[img] = mem;
  }
  return r;
}
VkResult bindImagenes2(VkDevice device, uint32_t n, const VkBindImageMemoryInfo *bi, bool khr) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  const VkResult r = khr ? d->bindImg2KHR(device, n, bi) : d->bindImg2(device, n, bi);
  if (r == VK_SUCCESS) {
    std::lock_guard<std::mutex> g(g_mx);
    for (uint32_t i = 0; i < n; i++)
      if (bi[i].memory != VK_NULL_HANDLE) d->memoriaDe[bi[i].image] = bi[i].memory;
  }
  return r;
}
VKAPI_ATTR VkResult VKAPI_CALL BindImageMemory2(VkDevice device, uint32_t n, const VkBindImageMemoryInfo *bi) {
  return bindImagenes2(device, n, bi, false);
}
VKAPI_ATTR VkResult VKAPI_CALL BindImageMemory2KHR(VkDevice device, uint32_t n, const VkBindImageMemoryInfo *bi) {
  return bindImagenes2(device, n, bi, true);
}
VKAPI_ATTR void VKAPI_CALL FreeMemory(VkDevice device, VkDeviceMemory mem, const VkAllocationCallbacks *a) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
    auto l = d->memLeidaEn.find(mem);
    const bool aplazar = mem != VK_NULL_HANDLE &&
                         (aplazarTodo() || (l != d->memLeidaEn.end() && d->presentes - l->second < kAplazar));
    if (l != d->memLeidaEn.end()) {
      if (aplazar && ++d->aplazadosTotal <= 40)
        anota("[aplazar] el juego suelta la memoria %p de una imagen que la tarjeta leyo por nosotros hace %ld "
              "presentaciones: se suelta dentro de %ld", (void *)mem, d->presentes - l->second, kAplazar);
      d->memLeidaEn.erase(l);
    } else if (aplazar) {
      d->aplazadosTotal++;
    }
    if (aplazar) {
      d->aplazados.push_back({VK_NULL_HANDLE, mem, a, d->presentes});
      return;
    }
  }
  d->free(device, mem, a);
}
/* suelta lo apartado: lo que ya cumplio kAplazar (o todo, al cerrar el dispositivo). Fuera de g_mx; primero las
   imagenes y despues las memorias */
void soltarAplazados(Dispositivo *d, std::vector<Dispositivo::Aplazado> &lista) {
  for (const Dispositivo::Aplazado &x : lista)
    if (x.img != VK_NULL_HANDLE) d->destroyImage(d->dev, x.img, x.a);
  for (const Dispositivo::Aplazado &x : lista)
    if (x.mem != VK_NULL_HANDLE) d->free(d->dev, x.mem, x.a);
  lista.clear();
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
      if (im != d->imagenes.end() && esProfundidad(im->second.formato)) {
        d->ultimaPedida = v->second.img;
        d->profundidadesDlss.insert(v->second.img);
      }
      else if (im != d->imagenes.end()) d->colorDlss[v->second.img] |= 1u << (uint32_t)info->descriptorType;
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

/* (v1.13) La profundidad que se copia, con g_mx tomado. MEDIDO en Cyberpunk, 26-nov 17:07: al entrar y salir del
   menu el juego rehace sus imagenes y DLSS pide numero a varias de profundidad seguidas (41 a 65); antes se copiaba
   la ULTIMA pedida, y esa no tenia por que ser la que el juego usa en ese cuadro. El juego la libero mientras
   nuestra copia esperaba en la tarjeta: «NVRM: Xid 31 ... MMU Fault: ENGINE CE0 ... FAULT_PTE ACCESS_TYPE_VIRT_READ»
   (el motor de copia leyendo memoria que ya no existia) y el juego cayo con «Device Removed». Es lo mismo que a
   las 15:54 dio una profundidad llena de NaN (entonces la memoria aun existia). Ahora solo vale una imagen a la
   que DLSS pidio numero Y que el juego uso como destino de profundidad en los ultimos 3 cuadros: esa la usa el
   propio juego en este cuadro y no la suelta hasta acabarlo, despues de nuestra copia. Si no hay ninguna, no se
   copia (el visor espera medio segundo y vuelve a la red). */
VkImage elegirProfundidad(Dispositivo *d, Imagen *im) {
  VkImage mejor = VK_NULL_HANDLE;
  long uso = 0;
  for (VkImage i : d->profundidadesDlss) {
    auto u = d->usoComoProfundidad.find(i);
    if (u == d->usoComoProfundidad.end()) continue;
    if (mejor == VK_NULL_HANDLE || u->second > uso) mejor = i, uso = u->second;
  }
  auto it = mejor != VK_NULL_HANDLE ? d->imagenes.find(mejor) : d->imagenes.end();
  const bool vale = it != d->imagenes.end() && uso >= d->presentes - 3;
  if (vale && mejor != d->profElegida && d->profAnunciadas.insert(mejor).second)
    anota("[compartir] la profundidad de DLSS: %p %ux%u %s, banderas 0x%x (el juego la usa como profundidad en este "
          "cuadro%s)", (void *)mejor, it->second.w, it->second.h, formato(it->second.formato).c_str(),
          (unsigned)it->second.flags, d->profAnunciadas.size() > 1 ? "; las ya vistas no se vuelven a apuntar" : "");
  else if (!vale && (d->profElegida != VK_NULL_HANDLE || !d->profAvisada))
    anota("[compartir] ninguna profundidad de DLSS en uso en este cuadro (%zu pedidas): no se copia hasta que vuelva una",
          d->profundidadesDlss.size());
  d->profAvisada = true;
  d->profElegida = vale ? mejor : VK_NULL_HANDLE;
  if (!vale) return VK_NULL_HANDLE;
  *im = it->second;
  return mejor;
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
    if (sonda()) d->cbs[cb].trasDlss = true;   /* fase 3b: lo que se dibuje despues en este buffer va tras DLSS */
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
    /* la de este lanzamiento, si los parametros la nombraran (no lo hacen en Cyberpunk); si no, elegirProfundidad */
    VkImage elegida = prof;
    Imagen imElegida = improf;
    if (elegida == VK_NULL_HANDLE) elegida = elegirProfundidad(d, &imElegida);
    if (prof == VK_NULL_HANDLE && d->armado && elegida != VK_NULL_HANDLE) {
      anota("[volcar] por la profundidad de DLSS que el juego usa en este cuadro (los parametros no la nombran)");
      grabarCopia(d, cb, elegida, imElegida);
    }
    if (elegida != VK_NULL_HANDLE) compartir(d, cb, elegida, imElegida);   /* fase 2 */
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
      anota("[resumen] numeros pedidos %ld · lanzamientos %ld · con profundidad %ld · imagenes de profundidad leidas %zu · "
            "copias al visor %ld · mascaras del HUD %ld (capas en el ultimo cuadro %d) · apartadas %ld (%zu ahora)",
            d->pedidos, d->lanzamientos, d->conProfundidad, d->vistasDeProfundidad.size(), d->copias, d->hudCopias,
            d->hudCapasCuadro, d->aplazadosTotal, d->aplazados.size());
    }
  }
  d->cuLaunch(cb, li);
}

/* ---------- fase 3 (sonda): el cuadro presentado, con el HUD ---------- */
VKAPI_ATTR void VKAPI_CALL GetDeviceQueue(VkDevice device, uint32_t familia, uint32_t indice, VkQueue *q) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  d->getQueue(device, familia, indice, q);
  std::lock_guard<std::mutex> g(g_mx);
  if (q && *q) d->familias[*q] = familia;
}
VKAPI_ATTR void VKAPI_CALL GetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *info, VkQueue *q) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  d->getQueue2(device, info, q);
  std::lock_guard<std::mutex> g(g_mx);
  if (q && *q) d->familias[*q] = info->queueFamilyIndex;
}
VKAPI_ATTR VkResult VKAPI_CALL CreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *ci, const VkAllocationCallbacks *a,
                                                  VkSwapchainKHR *sw) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  VkSwapchainCreateInfoKHR c = *ci;
  if (sonda()) c.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;   /* para copiar el cuadro con el HUD */
  bool copiable = sonda();
  VkResult r = d->createSwapchain(device, &c, a, sw);
  if (r != VK_SUCCESS && sonda()) {
    copiable = false;
    r = d->createSwapchain(device, ci, a, sw);
  }
  if (r == VK_SUCCESS) {
    std::lock_guard<std::mutex> g(g_mx);
    Dispositivo::Swap &w = d->swaps[*sw];
    w.fmt = ci->imageFormat, w.ext = ci->imageExtent, w.copiable = copiable;
    /* fase 3c: el tamano de salida, el del HUD */
    d->salidaW = ci->imageExtent.width, d->salidaH = ci->imageExtent.height;
    anota("[cadena] de presentacion %ux%u %s%s", ci->imageExtent.width, ci->imageExtent.height,
          formato(ci->imageFormat).c_str(), sonda() && !copiable ? " (no se puede copiar)" : "");
  }
  return r;
}
VKAPI_ATTR VkResult VKAPI_CALL GetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR sw, uint32_t *n, VkImage *imgs) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(device);
  }
  const VkResult r = d->getSwapImages(device, sw, n, imgs);
  if (imgs && (r == VK_SUCCESS || r == VK_INCOMPLETE)) {
    std::lock_guard<std::mutex> g(g_mx);
    d->swaps[sw].imgs.assign(imgs, imgs + *n);
  }
  return r;
}

/* La copia va en un buffer de comandos propio, en la misma cola: espera a lo que esperaba la presentacion,
   copia la imagen y avisa con un semaforo nuestro, que es lo que espera la presentacion de verdad. Despues
   se espera a la cola una vez (un tiron de un cuadro por volcado) y se escribe el archivo. */
/* SIN g_mx: espera a la cola (ver liberarCompartida); lo que necesita de d se copia con el candado */
VkResult presentarCapturando(Dispositivo *d, VkQueue q, const VkPresentInfoKHR *pi) {
  Dispositivo::Swap w;
  bool hay;
  uint32_t fam;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d->capturarPresente = false;
    auto it = d->swaps.find(pi->pSwapchains[0]);
    hay = it != d->swaps.end();
    if (hay) w = it->second;
    fam = d->familias.count(q) ? d->familias[q] : 0;
  }
  const uint32_t idx = pi->pImageIndices[0];
  if (!hay || !w.copiable || idx >= w.imgs.size() || !bytesColor(w.fmt)) {
    anota("[sonda] el cuadro presentado no se puede copiar (cadena desconocida, sin TRANSFER_SRC o formato %s)",
          hay ? formato(w.fmt).c_str() : "?");
    return d->present(q, pi);
  }
  const VkImage img = w.imgs[idx];
  const uint32_t bpp = bytesColor(w.fmt);
  const VkDeviceSize tam = (VkDeviceSize)w.ext.width * w.ext.height * bpp;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cb = VK_NULL_HANDLE;
  VkSemaphore sem = VK_NULL_HANDLE;
  VkBuffer buf = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  auto limpiar = [&]() {
    if (sem) d->destroySem(d->dev, sem, nullptr);
    if (pool) d->destroyPool(d->dev, pool, nullptr);
    if (buf) d->destroyBuffer(d->dev, buf, nullptr);
    if (mem) d->free(d->dev, mem, nullptr);
  };
  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pci.queueFamilyIndex = fam;
  VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cai.commandBufferCount = 1;
  cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = tam;
  bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bool ok = d->createPool(d->dev, &pci, nullptr, &pool) == VK_SUCCESS;
  if (ok) {
    cai.commandPool = pool;
    ok = d->allocCb(d->dev, &cai, &cb) == VK_SUCCESS;
  }
  if (ok) {
    /* el buffer de comandos es nuestro: hay que darle el despacho del cargador (lo pide la especificacion de capas) */
    if (d->setLoaderData) d->setLoaderData(d->dev, cb);
    else *(void **)cb = *(void **)d->dev;
    ok = d->createSem(d->dev, &sci, nullptr, &sem) == VK_SUCCESS && d->createBuffer(d->dev, &bi, nullptr, &buf) == VK_SUCCESS;
  }
  if (ok) {
    VkMemoryRequirements mr{};
    d->bufReqs(d->dev, buf, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = tipoMemoria(d, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    ok = ai.memoryTypeIndex != UINT32_MAX && d->alloc(d->dev, &ai, nullptr, &mem) == VK_SUCCESS && d->bind(d->dev, buf, mem, 0) == VK_SUCCESS;
  }
  if (!ok) {
    anota("[sonda] no se pudo preparar la copia del cuadro presentado");
    limpiar();
    return d->present(q, pi);
  }
  VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  d->beginCb(cb, &cbi);
  VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  ib.image = img;
  ib.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  ib.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, ib.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  ib.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT, ib.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  d->barrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib);
  VkBufferImageCopy reg{};
  reg.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  reg.imageExtent = {w.ext.width, w.ext.height, 1};
  d->copyImgBuf(cb, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &reg);
  ib.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ib.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  ib.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT, ib.dstAccessMask = 0;
  d->barrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib);
  d->endCb(cb);
  std::vector<VkPipelineStageFlags> etapas(pi->waitSemaphoreCount, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.waitSemaphoreCount = pi->waitSemaphoreCount;
  si.pWaitSemaphores = pi->pWaitSemaphores;
  si.pWaitDstStageMask = etapas.data();
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;
  si.signalSemaphoreCount = 1;
  si.pSignalSemaphores = &sem;
  if (d->submit(q, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
    anota("[sonda] no se pudo enviar la copia del cuadro presentado");
    limpiar();
    return d->present(q, pi);
  }
  VkPresentInfoKHR p2 = *pi;
  p2.waitSemaphoreCount = 1;
  p2.pWaitSemaphores = &sem;
  const VkResult r = d->present(q, &p2);
  d->waitIdle(q);
  void *ptr = nullptr;
  if (d->map(d->dev, mem, 0, VK_WHOLE_SIZE, 0, &ptr) == VK_SUCCESS) {
    int k;
    {
      std::lock_guard<std::mutex> g(g_mx);
      k = ++d->volcadosPresente;
    }
    const std::string base = carpetaVolcados() + "/presente-" + std::to_string((long)getpid()) + "-" + std::to_string(k);
    if (FILE *o = std::fopen((base + ".raw").c_str(), "wb")) {
      std::fwrite(ptr, 1, (size_t)tam, o);
      std::fclose(o);
    }
    if (FILE *o = std::fopen((base + ".json").c_str(), "w")) {
      std::fprintf(o, "{\"width\": %u, \"height\": %u, \"format\": \"%s\", \"vkformat\": %d, \"bpp\": %u, \"pid\": %ld}\n",
                   w.ext.width, w.ext.height, formato(w.fmt).c_str(), (int)w.fmt, bpp, (long)getpid());
      std::fclose(o);
    }
    d->unmap(d->dev, mem);
    anota("[sonda] cuadro presentado escrito: %s.raw (%ux%u %s)", base.c_str(), w.ext.width, w.ext.height, formato(w.fmt).c_str());
  }
  limpiar();
  return r;
}
VKAPI_ATTR VkResult VKAPI_CALL QueuePresentKHR(VkQueue q, const VkPresentInfoKHR *pi) {
  Dispositivo *d;
  bool capturar;
  std::vector<Dispositivo::Aplazado> soltar;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(q);
    d->presentes++;
    contarReal();   /* (v1.18) para la generacion de cuadros de MAKO: ver el contador, abajo */
    /* (v1.13) lo apartado que ya cumplio sus kAplazar presentaciones */
    if (!d->aplazados.empty()) {
      std::vector<Dispositivo::Aplazado> quedan;
      for (const Dispositivo::Aplazado &x : d->aplazados)
        (d->presentes - x.desde >= kAplazar ? soltar : quedan).push_back(x);
      d->aplazados.swap(quedan);
    }
    /* (protocolo v4) para los FPS del juego en el visor */
    if (d->shm) ((volatile OdvkProfCabecera *)d->shm)->presentes = (uint64_t)d->presentes;
    /* ODYSSEYVK_CAPA_PRESENTE_TRAS=<n> (taller): captura el cuadro presentado numero n, sin DLSS (vkcube) */
    static const long kTras = [] {
      const char *e = std::getenv("ODYSSEYVK_CAPA_PRESENTE_TRAS");
      return e ? std::atol(e) : -1L;
    }();
    if (kTras > 0 && d->presentes == kTras) d->capturarPresente = true;
    /* fase 3b: la ventana de los destinos acaba en la presentacion siguiente a su apertura; con la de taller
       (ODYSSEYVK_CAPA_DESTINOS_TRAS=<n>, sin DLSS) se abre en la presentacion n y se copia la n+1 */
    if (d->ventana && d->presentes > d->ventanaPresente) {
      if (d->ventanaTaller) d->capturarPresente = true;
      cerrarVentana(d, "presentacion");
    }
    static const long kDestinos = [] {
      const char *e = std::getenv("ODYSSEYVK_CAPA_DESTINOS_TRAS");
      return e ? std::atol(e) : -1L;
    }();
    if (kDestinos > 0 && d->presentes == kDestinos) abrirVentana(d, true);
    capturar = d->capturarPresente && pi && pi->swapchainCount >= 1;
  }
  if (!soltar.empty()) soltarAplazados(d, soltar);
  if (capturar) return presentarCapturando(d, q, pi);   /* fuera de g_mx: espera a la cola */
  return d->present(q, pi);
}

/* ---------- fase 3b (sonda): los destinos de dibujo ---------- */
/* Los ganchos de dibujo van en cada vkCmdDraw* del juego (miles por cuadro): el dispositivo y el contador del
   dibujo con destino abierto se guardan por hilo, para no tomar el candado en cada uno (un buffer de comandos
   se graba en un hilo a la vez). Solo con la sonda: sin ella ni se enganchan. */
struct CacheCb {
  VkCommandBuffer cb = VK_NULL_HANDLE;
  Dispositivo *d = nullptr;
  long *dibujos = nullptr;
  uint64_t gen = 0;
};
thread_local CacheCb tl_cb;
/* con g_mx tomado */
void cachear(Dispositivo *d, VkCommandBuffer cb) {
  tl_cb.cb = cb;
  tl_cb.d = d;
  tl_cb.dibujos = &d->cbs[cb].actual.dibujos;
  tl_cb.gen = g_generacion.load(std::memory_order_relaxed);
}
inline Dispositivo *dispositivoDibujo(VkCommandBuffer cb) {
  if (tl_cb.cb == cb && tl_cb.gen == g_generacion.load(std::memory_order_relaxed)) {
    ++*tl_cb.dibujos;
    return tl_cb.d;
  }
  std::lock_guard<std::mutex> g(g_mx);
  Dispositivo *d = dispositivo(cb);
  cachear(d, cb);
  ++*tl_cb.dibujos;
  return d;
}
VKAPI_ATTR void VKAPI_CALL CmdDraw(VkCommandBuffer cb, uint32_t a, uint32_t b, uint32_t c, uint32_t e) {
  dispositivoDibujo(cb)->draw(cb, a, b, c, e);
}
VKAPI_ATTR void VKAPI_CALL CmdDrawIndexed(VkCommandBuffer cb, uint32_t a, uint32_t b, uint32_t c, int32_t o, uint32_t e) {
  dispositivoDibujo(cb)->drawIndexed(cb, a, b, c, o, e);
}
VKAPI_ATTR void VKAPI_CALL CmdDrawIndirect(VkCommandBuffer cb, VkBuffer b, VkDeviceSize o, uint32_t n, uint32_t s) {
  dispositivoDibujo(cb)->drawIndirect(cb, b, o, n, s);
}
VKAPI_ATTR void VKAPI_CALL CmdDrawIndexedIndirect(VkCommandBuffer cb, VkBuffer b, VkDeviceSize o, uint32_t n, uint32_t s) {
  dispositivoDibujo(cb)->drawIndexedIndirect(cb, b, o, n, s);
}
VKAPI_ATTR void VKAPI_CALL CmdDrawIndirectCount(VkCommandBuffer cb, VkBuffer b, VkDeviceSize o, VkBuffer cbuf, VkDeviceSize co,
                                                uint32_t m, uint32_t s) {
  dispositivoDibujo(cb)->drawIndirectCount(cb, b, o, cbuf, co, m, s);
}
VKAPI_ATTR void VKAPI_CALL CmdDrawIndirectCountKHR(VkCommandBuffer cb, VkBuffer b, VkDeviceSize o, VkBuffer cbuf, VkDeviceSize co,
                                                   uint32_t m, uint32_t s) {
  dispositivoDibujo(cb)->drawIndirectCountKHR(cb, b, o, cbuf, co, m, s);
}
VKAPI_ATTR void VKAPI_CALL CmdDrawIndexedIndirectCount(VkCommandBuffer cb, VkBuffer b, VkDeviceSize o, VkBuffer cbuf,
                                                       VkDeviceSize co, uint32_t m, uint32_t s) {
  dispositivoDibujo(cb)->drawIndexedIndirectCount(cb, b, o, cbuf, co, m, s);
}
VKAPI_ATTR void VKAPI_CALL CmdDrawIndexedIndirectCountKHR(VkCommandBuffer cb, VkBuffer b, VkDeviceSize o, VkBuffer cbuf,
                                                          VkDeviceSize co, uint32_t m, uint32_t s) {
  dispositivoDibujo(cb)->drawIndexedIndirectCountKHR(cb, b, o, cbuf, co, m, s);
}

VKAPI_ATTR VkResult VKAPI_CALL BeginCommandBuffer(VkCommandBuffer cb, const VkCommandBufferBeginInfo *bi) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
    d->cbs[cb] = CbSonda{};   /* se graba de nuevo: lo de antes ya no cuenta */
  }
  return d->beginCb(cb, bi);
}

void empezarAmbito(Dispositivo *d, VkCommandBuffer cb, const VkRenderingInfo *ri) {
  std::lock_guard<std::mutex> g(g_mx);
  CbSonda &c = d->cbs[cb];
  c.actual = Ambito{};
  Ambito &a = c.actual;
  a.w = ri->renderArea.extent.width, a.h = ri->renderArea.extent.height;
  a.reanuda = (ri->flags & VK_RENDERING_RESUMING_BIT) != 0;
  /* suspendido: sigue en otro vkCmdBeginRendering con RESUMING y entre los dos no se puede grabar nada */
  a.suspende = (ri->flags & VK_RENDERING_SUSPENDING_BIT) != 0;
  for (uint32_t i = 0; i < ri->colorAttachmentCount; i++) {
    const VkRenderingAttachmentInfo &at = ri->pColorAttachments[i];
    auto v = d->vistas.find(at.imageView);
    if (at.imageView == VK_NULL_HANDLE || v == d->vistas.end()) continue;
    a.colores.push_back(v->second.img);
    a.cargas.push_back((int)at.loadOp);
    a.limpia.push_back(at.clearValue.color);
  }
  a.conProf = ri->pDepthAttachment && ri->pDepthAttachment->imageView != VK_NULL_HANDLE;
  if (a.conProf) {   /* (v1.13) el juego la usa en este cuadro: se puede copiar (ver elegirProfundidad) */
    auto v = d->vistas.find(ri->pDepthAttachment->imageView);
    if (v != d->vistas.end()) d->usoComoProfundidad[v->second.img] = d->presentes;
  }
  cachear(d, cb);
}

bool esDeLaCadena(Dispositivo *d, VkImage img) {
  for (const auto &s : d->swaps)
    for (VkImage i : s.second.imgs)
      if (i == img) return true;
  return false;
}

/* ---------- fase 3c: la mascara del HUD ---------- */
/* La imagen del HUD, como se MIDIO en Cyberpunk (fase 3b, las tres ventanas de juego): el unico destino de
   color del cuadro que esta a la resolucion de salida, es RGBA8, se limpia a (0, 0, 0, 0), cubre la imagen
   entera y va sin profundidad. Ninguna otra pasada del cuadro cumple las cinco cosas. */
bool esHud(Dispositivo *d, const Ambito &a, VkImage *img, const Imagen **im) {
  if (a.colores.size() != 1 || a.conProf || a.reanuda || a.suspende) return false;
  auto it = d->imagenes.find(a.colores[0]);
  if (it == d->imagenes.end()) return false;
  const Imagen &m = it->second;
  if (!esRgba8(m.formato) || m.muestras != VK_SAMPLE_COUNT_1_BIT || !(m.uso & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) return false;
  if (a.w != m.w || a.h != m.h || m.w < 1280) return false;
  if (!hudPrueba()) {
    const VkClearColorValue &l = a.limpia[0];
    if (a.cargas[0] != VK_ATTACHMENT_LOAD_OP_CLEAR || l.uint32[0] || l.uint32[1] || l.uint32[2] || l.uint32[3]) return false;
    if (d->salidaW && (m.w != d->salidaW || m.h != d->salidaH)) return false;
  }
  if (esDeLaCadena(d, a.colores[0])) return false;
  *img = a.colores[0];
  *im = &m;
  return true;
}

bool imagenPropia(Dispositivo *d, uint32_t w, uint32_t h, VkImage *img, VkDeviceMemory *mem) {
  VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ci.imageType = VK_IMAGE_TYPE_2D;
  ci.format = VK_FORMAT_R8G8B8A8_UNORM;
  ci.extent = {w, h, 1};
  ci.mipLevels = ci.arrayLayers = 1;
  ci.samples = VK_SAMPLE_COUNT_1_BIT;
  ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  ci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (d->createImage(d->dev, &ci, nullptr, img) != VK_SUCCESS) return false;   /* la de abajo: no la apuntamos */
  VkMemoryRequirements mr{};
  d->imgReqs(d->dev, *img, &mr);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = mr.size;
  ai.memoryTypeIndex = tipoMemoria(d, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (ai.memoryTypeIndex == UINT32_MAX || d->alloc(d->dev, &ai, nullptr, mem) != VK_SUCCESS) return false;
  return d->bindImg(d->dev, *img, *mem, 0) == VK_SUCCESS;
}
bool prepararHud(Dispositivo *d) {
  if (d->hudA && d->hudB[ODVK_HUD_CAPAS - 1]) return true;
  bool ok = imagenPropia(d, ODVK_HUD_ANCHO * 2, ODVK_HUD_ALTO * 2, &d->hudA, &d->hudAMem);
  for (uint32_t k = 0; ok && k < ODVK_HUD_CAPAS; k++) ok = imagenPropia(d, ODVK_HUD_ANCHO, ODVK_HUD_ALTO, &d->hudB[k], &d->hudBMem[k]);
  if (!ok) {
    anota("[hud] no se pudieron crear las imagenes para reducir el HUD: sin mascara");
    d->hudFallo = true;
    return false;
  }
  return true;
}
void liberarHud(Dispositivo *d) {
  if (d->hudA) d->destroyImage(d->dev, d->hudA, nullptr);
  if (d->hudAMem) d->free(d->dev, d->hudAMem, nullptr);
  d->hudA = VK_NULL_HANDLE, d->hudAMem = VK_NULL_HANDLE;
  for (uint32_t k = 0; k < ODVK_HUD_CAPAS; k++) {
    if (d->hudB[k]) d->destroyImage(d->dev, d->hudB[k], nullptr);
    if (d->hudBMem[k]) d->free(d->dev, d->hudBMem[k], nullptr);
    d->hudB[k] = VK_NULL_HANDLE, d->hudBMem[k] = VK_NULL_HANDLE;
  }
}

/* en el buffer de comandos del juego, justo despues de su vkCmdEndRendering del HUD. La imagen del HUD se lee
   como GENERAL sin cambiarle el diseno, como la profundidad en la fase 2 (el controlador de NVIDIA no mira los
   disenos; DLSS solo existe ahi); las nuestras van con sus disenos de verdad */
/* `k`: la capa de este cuadro (0 la primera). Se reduce a su imagen y se publican las k + 1 capas del cuadro
   juntas en un hueco: cada publicacion lleva todo lo de ese cuadro hasta ahi */
void grabarHud(Dispositivo *d, VkCommandBuffer cb, VkImage img, const Imagen &im, uint32_t k) {
  marcarLectura(d, img);
  const OdvkProfCabecera *c = (const OdvkProfCabecera *)d->shm;
  const uint64_t seq = ++d->hudSeq;
  const uint64_t hueco = odvkHudHueco(c, seq % ODVK_PROF_HUECOS);
  const uint32_t aw = ODVK_HUD_ANCHO * 2, ah = ODVK_HUD_ALTO * 2, bw = ODVK_HUD_ANCHO, bh = ODVK_HUD_ALTO;
  VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  VkImageMemoryBarrier ib[2]{};
  for (VkImageMemoryBarrier &b : ib) {
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  }
  /* 1. lo que dibujo el juego, visible para copiar; las dos nuestras a destino (lo de antes no importa) */
  mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
  ib[0].image = d->hudA, ib[1].image = d->hudB[k];   /* las capas de antes de este cuadro se quedan como estan */
  for (VkImageMemoryBarrier &b : ib) {
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.srcAccessMask = 0, b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  }
  d->barrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 2, ib);
  /* 2. a la mitad, y otra vez a la mitad (con filtro lineal y medida exacta, cada paso es la media de 2 x 2) */
  VkImageBlit r{};
  r.srcSubresource = r.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  r.srcOffsets[1] = {(int32_t)im.w, (int32_t)im.h, 1};
  r.dstOffsets[1] = {(int32_t)aw, (int32_t)ah, 1};
  d->blit(cb, img, VK_IMAGE_LAYOUT_GENERAL, d->hudA, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r, VK_FILTER_LINEAR);
  ib[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ib[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  ib[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, ib[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  d->barrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib[0]);
  r.srcOffsets[1] = {(int32_t)aw, (int32_t)ah, 1};
  r.dstOffsets[1] = {(int32_t)bw, (int32_t)bh, 1};
  d->blit(cb, d->hudA, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, d->hudB[k], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r, VK_FILTER_LINEAR);
  ib[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ib[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  ib[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, ib[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  d->barrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib[1]);
  /* 3. al hueco, con las cuatro escrituras de siempre (ver protocolo.h) */
  auto paso = [&]() {
    mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    d->barrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
  };
  d->updateBuf(cb, d->shmBuf, hueco, 8, &seq);
  const uint32_t cuantas = k + 1;
  d->updateBuf(cb, d->shmBuf, hueco + ODVK_HUD_CUANTAS, 4, &cuantas);
  paso();
  for (uint32_t j = 0; j <= k; j++) {
    VkBufferImageCopy reg{};
    reg.bufferOffset = hueco + ODVK_PROF_DATOS + (uint64_t)j * bw * bh * 4;
    reg.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    reg.imageExtent = {bw, bh, 1};
    d->copyImgBuf(cb, d->hudB[j], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, d->shmBuf, 1, &reg);
  }
  paso();
  d->updateBuf(cb, d->shmBuf, hueco + c->hudTamHueco - 8, 8, &seq);
  paso();
  d->updateBuf(cb, d->shmBuf, offsetof(OdvkProfCabecera, hudUltimoSeq), 8, &seq);
  mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;
  d->barrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb,
             0, nullptr, 0, nullptr);
}

/* con g_mx tomado, al acabar cada dibujo con destino */
void mirarHud(Dispositivo *d, VkCommandBuffer cb, const Ambito &a) {
  if (!hudActivo() || d->hudFallo || !d->blit || !d->imgReqs || !d->bindImg || !d->updateBuf) return;
  VkImage img = VK_NULL_HANDLE;
  const Imagen *im = nullptr;
  if (!esHud(d, a, &img, &im)) return;
  if (hudPrueba() && !d->shmFallo) {   /* taller: sin DLSS no hay profundidad que la cree */
    /* ODYSSEYVK_CAPA_RECREAR_CADA=<s>: se rehace cada tantos segundos con otra medida, como cuando DLSS cambia de
       modo (lo que congelo Cyberpunk el 26-nov: ver liberarCompartida) */
    static const double kRecrear = [] {
      const char *e = std::getenv("ODYSSEYVK_CAPA_RECREAR_CADA");
      return e ? std::atof(e) : 0.0;
    }();
    if (!d->shm || (kRecrear > 0.0 && ahora() - d->shmCreada > kRecrear)) {
      const uint32_t lado = d->shmW == 64 ? 32 : 64;
      const Imagen falsa{VK_FORMAT_D32_SFLOAT, lado, lado, VK_SAMPLE_COUNT_1_BIT, 0};
      if (!d->hostPtrProps || !crearCompartida(d, falsa)) d->shmFallo = true;
    }
  }
  if (!d->shm) return;
  {
    OdvkProfCabecera *c = (OdvkProfCabecera *)d->shm;
    c->hudFuenteAncho = im->w, c->hudFuenteAlto = im->h;
  }
  /* una linea por imagen: MEDIDO en Cyberpunk, en los menus se turnan dos en cada cuadro (11 000 lineas) */
  if (d->hudVistas.insert(img).second)
    anota("[hud] una capa del HUD: %p %ux%u %s%s (%zu distintas hasta ahora)", (void *)img, im->w, im->h,
          formato(im->formato).c_str(),
          hudPrueba() ? " (ODYSSEYVK_CAPA_HUD_PRUEBA: cualquier destino grande)" : ", limpiada a transparente",
          d->hudVistas.size());
  if (!hayLector(d)) return;
  /* (protocolo v5) el visor dice si la quiere: con el ajuste del HUD apagado, ni se toca la imagen del HUD */
  {
    const bool quiere = ((volatile OdvkProfCabecera *)d->shm)->lectorQuiereHud != 0;
    if ((int)quiere != d->hudQuerido) {
      anota(quiere ? "[hud] el visor quiere la mascara del HUD" : "[hud] el visor no quiere la mascara del HUD (ajuste apagado): no se copia");
      d->hudQuerido = quiere ? 1 : 0;
    }
    if (!quiere) return;
  }
  /* la capa de este cuadro: un cuadro nuevo (otra presentacion, o mas de 10 ms sin ninguna, como en la prueba
     sin juego) empieza en la 0; una tercera del mismo cuadro no cabe y se deja */
  const double t = ahora();
  const bool nuevo = d->presentes != d->hudMarca || t - d->hudUltima > 0.010;
  /* (protocolo v6) el tope de FPS de la capa tambien para la mascara: un cuadro nuevo solo si paso 1/tope desde la
     ultima; las capas de un mismo cuadro (el HUD y el menu) van juntas */
  if (nuevo) {
    const uint32_t tope = ((volatile OdvkProfCabecera *)d->shm)->lectorTopeFps;
    if (tope > 0 && !tocaPorTope(d->hudSiguiente, t, tope)) return;
  }
  const int k = nuevo ? 0 : d->hudCapa + 1;
  if (k >= (int)ODVK_HUD_CAPAS) return;
  if (!prepararHud(d)) return;
  d->hudMarca = d->presentes, d->hudCapa = k, d->hudUltima = t;
  d->hudCapasCuadro = k + 1;
  if (d->hudCapasCuadro > d->hudCapasMaxVistas) {
    d->hudCapasMaxVistas = d->hudCapasCuadro;
    anota("[hud] %d capas en un mismo cuadro: la mascara es su union", d->hudCapasMaxVistas);
  }
  grabarHud(d, cb, img, *im, (uint32_t)k);
  if (++d->hudCopias == 1) anota("[hud] primera mascara del HUD al visor (%ux%u)", ODVK_HUD_ANCHO, ODVK_HUD_ALTO);
}

/* con g_mx tomado, despues del vkCmdEndRendering de abajo (ya fuera del dibujo: se puede copiar) */
void acabarAmbito(Dispositivo *d, VkCommandBuffer cb) {
  CbSonda &c = d->cbs[cb];
  const Ambito &a = c.actual;
  mirarHud(d, cb, a);
  if (!d->ventana || a.suspende) return;   /* (suspendido: hasta que se reanude no se puede grabar nada) */
  if (ahora() - d->ventanaDesde > 1.0) {
    cerrarVentana(d, "un segundo sin presentar");
    return;
  }
  const int k = ++d->ventanaAmbitos;
  /* el tamano de render de DLSS (el de su profundidad): lo que se dibuja a ese tamano es de ANTES de DLSS (la
     escena del cuadro siguiente, que ya se graba); el HUD va despues, a la resolucion de salida */
  uint32_t renderW = 0, renderH = 0;
  if (d->ultimaPedida != VK_NULL_HANDLE) {
    auto it = d->imagenes.find(d->ultimaPedida);
    if (it != d->imagenes.end()) renderW = it->second.w, renderH = it->second.h;
  }
  std::string lista;
  char tmp[192];
  for (size_t i = 0; i < a.colores.size(); i++) {
    const VkImage img = a.colores[i];
    auto it = d->imagenes.find(img);
    const VkClearColorValue &l = a.limpia[i];
    const char *carga = a.cargas[i] == VK_ATTACHMENT_LOAD_OP_CLEAR ? "limpia" : a.cargas[i] == VK_ATTACHMENT_LOAD_OP_LOAD ? "carga" : "da igual";
    if (it == d->imagenes.end()) {
      std::snprintf(tmp, sizeof tmp, " [%p %s %s]", (void *)img, esDeLaCadena(d, img) ? "de la cadena de presentacion" : "desconocida", carga);
      lista += tmp;
      continue;
    }
    const Imagen &im = it->second;
    std::snprintf(tmp, sizeof tmp, " [%p %ux%u %s %s%s]", (void *)img, im.w, im.h, formato(im.formato).c_str(), carga,
                  a.cargas[i] == VK_ATTACHMENT_LOAD_OP_CLEAR
                      ? (" a (" + std::to_string(l.float32[0]) + "," + std::to_string(l.float32[1]) + "," +
                         std::to_string(l.float32[2]) + "," + std::to_string(l.float32[3]) + ")").c_str()
                      : "");
    lista += tmp;
    /* se copian los de la resolucion de salida, cada imagen hasta 3 veces (el HUD puede ir en varias
       pasadas), 16 copias por ventana como mucho (66 MB cada una en RGBA16F 4K) */
    const bool copiable = (im.uso & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) && bytesColor(im.formato) && im.w >= 1280 &&
                          im.muestras == VK_SAMPLE_COUNT_1_BIT && !(im.w == renderW && im.h == renderH);
    int &veces = d->ventanaCopiadas[img];
    if (copiable && veces < 3 && d->ventanaCopias < 16) {
      std::snprintf(tmp, sizeof tmp,
                    ", \"orden\": %d, \"dibujos\": %ld, \"carga\": %d, \"limpia\": [%g, %g, %g, %g], \"tras_dlss\": %s, "
                    "\"imagen\": \"%p\", \"vez\": %d",
                    k, a.dibujos, a.cargas[i], l.float32[0], l.float32[1], l.float32[2], l.float32[3],
                    c.trasDlss ? "true" : "false", (void *)img, veces + 1);
      if (grabarCopiaImagen(d, cb, img, im, true, 0, "destino", tmp)) {
        veces++;
        d->ventanaCopias++;
      }
    }
  }
  if (k <= 300)
    anota("[destinos] %d · %ux%u · %ld dibujos · %s%s%s · color:%s", k, a.w, a.h, a.dibujos,
          c.trasDlss ? "tras DLSS en su buffer" : "otro buffer", a.conProf ? " · con profundidad" : "",
          a.reanuda ? " · reanuda" : "", lista.empty() ? " ninguno" : lista.c_str());
}

VKAPI_ATTR void VKAPI_CALL CmdBeginRendering(VkCommandBuffer cb, const VkRenderingInfo *ri) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
  }
  if (ri) empezarAmbito(d, cb, ri);
  d->beginRendering(cb, ri);
}
VKAPI_ATTR void VKAPI_CALL CmdBeginRenderingKHR(VkCommandBuffer cb, const VkRenderingInfo *ri) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
  }
  if (ri) empezarAmbito(d, cb, ri);
  d->beginRenderingKHR(cb, ri);
}
VKAPI_ATTR void VKAPI_CALL CmdEndRendering(VkCommandBuffer cb) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
  }
  d->endRendering(cb);
  std::lock_guard<std::mutex> g(g_mx);
  acabarAmbito(d, cb);
}
VKAPI_ATTR void VKAPI_CALL CmdEndRenderingKHR(VkCommandBuffer cb) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
  }
  d->endRenderingKHR(cb);
  std::lock_guard<std::mutex> g(g_mx);
  acabarAmbito(d, cb);
}
/* las pasadas clasicas (vkCmdBeginRenderPass) solo se cuentan: vkd3d-proton dibuja con vkCmdBeginRendering. Si
   el registro dice que hay muchas, el HUD podria ir por ahi y habria que seguirlas tambien */
void contarClasica(Dispositivo *d) {
  std::lock_guard<std::mutex> g(g_mx);
  if (++d->pasadasClasicas == 1) anota("[destinos] el juego usa tambien pasadas clasicas (vkCmdBeginRenderPass)");
}
VKAPI_ATTR void VKAPI_CALL CmdBeginRenderPass(VkCommandBuffer cb, const VkRenderPassBeginInfo *bi, VkSubpassContents c) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
  }
  contarClasica(d);
  d->beginRenderPass(cb, bi, c);
}
VKAPI_ATTR void VKAPI_CALL CmdBeginRenderPass2(VkCommandBuffer cb, const VkRenderPassBeginInfo *bi, const VkSubpassBeginInfo *si) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
  }
  contarClasica(d);
  d->beginRenderPass2(cb, bi, si);
}
VKAPI_ATTR void VKAPI_CALL CmdBeginRenderPass2KHR(VkCommandBuffer cb, const VkRenderPassBeginInfo *bi, const VkSubpassBeginInfo *si) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
  }
  contarClasica(d);
  d->beginRenderPass2KHR(cb, bi, si);
}

/* ODYSSEYVK_CAPA_PRUEBA=1 (taller): sin DLSS, al acabar cada pasada de render se comparte la ultima imagen de
   profundidad creada. Sirve para probar el camino de la tarjeta a la memoria compartida con vkcube. */
bool modoPrueba() {
  static const bool p = [] {
    const char *e = std::getenv("ODYSSEYVK_CAPA_PRUEBA");
    return e && e[0] == '1';
  }();
  return p;
}
VKAPI_ATTR void VKAPI_CALL CmdEndRenderPass(VkCommandBuffer cb) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(cb);
  }
  d->endRenderPass(cb);
  std::lock_guard<std::mutex> g(g_mx);
  if (d->ultimaCreada != VK_NULL_HANDLE) {
    auto it = d->imagenes.find(d->ultimaCreada);
    if (it != d->imagenes.end()) compartir(d, cb, d->ultimaCreada, it->second, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  }
}

VKAPI_ATTR VkResult VKAPI_CALL QueueSubmit(VkQueue q, uint32_t n, const VkSubmitInfo *s, VkFence f) {
  Dispositivo *d;
  {
    std::lock_guard<std::mutex> g(g_mx);
    d = dispositivo(q);
  }
  const VkResult r = d->submit(q, n, s, f);
  std::vector<Pendiente> listos;
  {
    std::lock_guard<std::mutex> g(g_mx);
    if (!d->pendientes.empty()) {
      std::vector<VkCommandBuffer> cbs;
      for (uint32_t i = 0; i < n; i++)
        for (uint32_t k = 0; k < s[i].commandBufferCount; k++) cbs.push_back(s[i].pCommandBuffers[k]);
      listos = sacarPendientes(d, cbs);
    }
  }
  leerPendientes(d, q, listos);
  return r;
}
VkResult submit2Comun(Dispositivo *d, VkQueue q, uint32_t n, const VkSubmitInfo2 *s, VkFence f, bool khr) {
  const VkResult r = khr ? d->submit2khr(q, n, s, f) : d->submit2(q, n, s, f);
  std::vector<Pendiente> listos;
  {
    std::lock_guard<std::mutex> g(g_mx);
    if (!d->pendientes.empty()) {
      std::vector<VkCommandBuffer> cbs;
      for (uint32_t i = 0; i < n; i++)
        for (uint32_t k = 0; k < s[i].commandBufferInfoCount; k++) cbs.push_back(s[i].pCommandBufferInfos[k].commandBuffer);
      listos = sacarPendientes(d, cbs);
    }
  }
  leerPendientes(d, q, listos);
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
    g_generacion.fetch_add(1);   /* la cache por hilo de los dibujos apuntaba a este */
    anota("[dispositivo] cerrado · numeros pedidos %ld · lanzamientos %ld · con profundidad %ld", d->pedidos,
          d->lanzamientos, d->conProfundidad);
  }
  if (!d->aplazados.empty()) d->devWaitIdle(d->dev);
  cerrarCompartidas(d);   /* espera a la tarjeta (aqui, fuera de g_mx, si se puede) y lo suelta todo */
  liberarHud(d);
  if (!d->aplazados.empty()) {
    anota("[aplazar] al cerrar el dispositivo: %zu apartadas se sueltan ahora (%ld en total)", d->aplazados.size(),
          d->aplazadosTotal);
    std::vector<Dispositivo::Aplazado> todo;
    todo.swap(d->aplazados);
    soltarAplazados(d, todo);
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
  PFN_vkSetDeviceLoaderData setLoader = nullptr;
  for (VkLayerDeviceCreateInfo *x = (VkLayerDeviceCreateInfo *)ci->pNext; x; x = (VkLayerDeviceCreateInfo *)x->pNext)
    if (x->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && x->function == VK_LOADER_DATA_CALLBACK)
      setLoader = x->u.pfnSetDeviceLoaderData;
  li->u.pLayerInfo = li->u.pLayerInfo->pNext;
  Instancia *inst;
  {
    std::lock_guard<std::mutex> g(g_mx);
    auto it = g_inst.find(llave(fisico));
    if (it == g_inst.end()) return VK_ERROR_INITIALIZATION_FAILED;
    inst = &it->second;
  }
  auto crear = (PFN_vkCreateDevice)gipa(inst->inst, "vkCreateDevice");
  /* fase 2: la tarjeta escribe la profundidad en la memoria compartida (VK_EXT_external_memory_host);
     se enciende aqui si el dispositivo la tiene y el juego no la pidio ya */
  VkDeviceCreateInfo c2 = *ci;
  std::vector<const char *> exts(ci->ppEnabledExtensionNames, ci->ppEnabledExtensionNames + ci->enabledExtensionCount);
  bool conHost = false;
  {
    uint32_t n = 0;
    std::vector<VkExtensionProperties> todas;
    if (inst->enumExt && inst->enumExt(fisico, nullptr, &n, nullptr) == VK_SUCCESS) {
      todas.resize(n);
      inst->enumExt(fisico, nullptr, &n, todas.data());
    }
    bool hay = false, pedida = false;
    for (const VkExtensionProperties &e : todas)
      if (!std::strcmp(e.extensionName, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME)) hay = true;
    for (const char *e : exts)
      if (!std::strcmp(e, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME)) pedida = true;
    if (hay && !pedida) exts.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
    conHost = hay;
  }
  c2.enabledExtensionCount = (uint32_t)exts.size();
  c2.ppEnabledExtensionNames = exts.data();
  VkResult r = crear(fisico, &c2, a, device);
  if (r != VK_SUCCESS && conHost) {
    conHost = false;   /* por si acaso: sin la extension, como lo pidio el juego */
    r = crear(fisico, ci, a, device);
  }
  if (r != VK_SUCCESS) return r;
  Dispositivo *d = new Dispositivo;
  d->dev = *device;
  d->fisico = fisico;
  d->inst = inst;
  d->gdpa = gdpa;
  d->setLoaderData = setLoader;
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
  CARGA(updateBuf, "vkCmdUpdateBuffer");
  CARGA(endRenderPass, "vkCmdEndRenderPass");
  CARGA(createSwapchain, "vkCreateSwapchainKHR");
  CARGA(getSwapImages, "vkGetSwapchainImagesKHR");
  CARGA(present, "vkQueuePresentKHR");
  CARGA(getQueue, "vkGetDeviceQueue");
  CARGA(getQueue2, "vkGetDeviceQueue2");
  CARGA(createPool, "vkCreateCommandPool");
  CARGA(destroyPool, "vkDestroyCommandPool");
  CARGA(allocCb, "vkAllocateCommandBuffers");
  CARGA(beginCb, "vkBeginCommandBuffer");
  CARGA(endCb, "vkEndCommandBuffer");
  CARGA(createSem, "vkCreateSemaphore");
  CARGA(destroySem, "vkDestroySemaphore");
  CARGA(devWaitIdle, "vkDeviceWaitIdle");
  CARGA(beginRendering, "vkCmdBeginRendering");
  CARGA(beginRenderingKHR, "vkCmdBeginRenderingKHR");
  CARGA(endRendering, "vkCmdEndRendering");
  CARGA(endRenderingKHR, "vkCmdEndRenderingKHR");
  CARGA(beginRenderPass, "vkCmdBeginRenderPass");
  CARGA(beginRenderPass2, "vkCmdBeginRenderPass2");
  CARGA(beginRenderPass2KHR, "vkCmdBeginRenderPass2KHR");
  CARGA(draw, "vkCmdDraw");
  CARGA(drawIndexed, "vkCmdDrawIndexed");
  CARGA(drawIndirect, "vkCmdDrawIndirect");
  CARGA(drawIndexedIndirect, "vkCmdDrawIndexedIndirect");
  CARGA(drawIndirectCount, "vkCmdDrawIndirectCount");
  CARGA(drawIndirectCountKHR, "vkCmdDrawIndirectCountKHR");
  CARGA(drawIndexedIndirectCount, "vkCmdDrawIndexedIndirectCount");
  CARGA(drawIndexedIndirectCountKHR, "vkCmdDrawIndexedIndirectCountKHR");
  CARGA(imgReqs, "vkGetImageMemoryRequirements");
  CARGA(bindImg, "vkBindImageMemory");
  CARGA(bindImg2, "vkBindImageMemory2");
  CARGA(bindImg2KHR, "vkBindImageMemory2KHR");
  CARGA(blit, "vkCmdBlitImage");
  if (conHost) CARGA(hostPtrProps, "vkGetMemoryHostPointerPropertiesEXT");
#undef CARGA
  if (conHost && inst->props2) {
    VkPhysicalDeviceExternalMemoryHostPropertiesEXT hp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT};
    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    p2.pNext = &hp;
    inst->props2(fisico, &p2);
    if (hp.minImportedHostPointerAlignment) d->alineacionHost = hp.minImportedHostPointerAlignment;
  }
  VkPhysicalDeviceProperties pp{};
  inst->props(fisico, &pp);
  std::string ext;
  for (uint32_t i = 0; i < ci->enabledExtensionCount; i++)
    if (std::strstr(ci->ppEnabledExtensionNames[i], "NVX")) ext += std::string(" ") + ci->ppEnabledExtensionNames[i];
  {
    std::lock_guard<std::mutex> g(g_mx);
    g_dev[llave(*device)] = d;
    anota("[dispositivo] %s · extensiones NVX:%s · numeros %s · lanzar %s · memoria del anfitrion %s (alineacion %llu)",
          pp.deviceName, ext.empty() ? " ninguna" : ext.c_str(), (d->handle32 || d->handle64) ? "si" : "no",
          d->cuLaunch ? "si" : "no", d->hostPtrProps ? "si" : "no", (unsigned long long)d->alineacionHost);
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
  in.props2 = (PFN_vkGetPhysicalDeviceProperties2)gipa(*inst, "vkGetPhysicalDeviceProperties2");
  in.enumExt = (PFN_vkEnumerateDeviceExtensionProperties)gipa(*inst, "vkEnumerateDeviceExtensionProperties");
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
  if (modoPrueba()) G("vkCmdEndRenderPass", CmdEndRenderPass);
  /* fase 3c: la cadena (su tamano) y los destinos de dibujo, para el HUD; y desde el 26-nov, para elegir la
     profundidad que se copia (elegirProfundidad): siempre que se comparte */
  if (sonda() || hudActivo() || compartirActivo()) {
    /* (v1.13) para no soltar lo que leemos (ver kAplazar) */
    G("vkBindImageMemory", BindImageMemory);
    G("vkBindImageMemory2", BindImageMemory2);
    G("vkBindImageMemory2KHR", BindImageMemory2KHR);
    G("vkFreeMemory", FreeMemory);
    G("vkCreateSwapchainKHR", CreateSwapchainKHR);
    G("vkGetSwapchainImagesKHR", GetSwapchainImagesKHR);
    G("vkCmdBeginRendering", CmdBeginRendering);
    G("vkCmdBeginRenderingKHR", CmdBeginRenderingKHR);
    G("vkCmdEndRendering", CmdEndRendering);
    G("vkCmdEndRenderingKHR", CmdEndRenderingKHR);
  }
  if (sonda() || hudActivo() || compartirActivo()) G("vkQueuePresentKHR", QueuePresentKHR);   /* los cuadros */
  if (sonda()) {   /* fase 3: el cuadro presentado, solo con la sonda */
    G("vkGetDeviceQueue", GetDeviceQueue);
    G("vkGetDeviceQueue2", GetDeviceQueue2);
    /* fase 3b: los dibujos de cada destino */
    G("vkBeginCommandBuffer", BeginCommandBuffer);
    G("vkCmdBeginRenderPass", CmdBeginRenderPass);
    G("vkCmdBeginRenderPass2", CmdBeginRenderPass2);
    G("vkCmdBeginRenderPass2KHR", CmdBeginRenderPass2KHR);
    G("vkCmdDraw", CmdDraw);
    G("vkCmdDrawIndexed", CmdDrawIndexed);
    G("vkCmdDrawIndirect", CmdDrawIndirect);
    G("vkCmdDrawIndexedIndirect", CmdDrawIndexedIndirect);
    G("vkCmdDrawIndirectCount", CmdDrawIndirectCount);
    G("vkCmdDrawIndirectCountKHR", CmdDrawIndirectCountKHR);
    G("vkCmdDrawIndexedIndirectCount", CmdDrawIndexedIndirectCount);
    G("vkCmdDrawIndexedIndirectCountKHR", CmdDrawIndexedIndirectCountKHR);
  }
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

/* ---- (v1.18) el contador: una segunda capa en esta biblioteca, debajo de MAKO ----
   Rodolfo: «you can see what are the generating frames, look how vkbasalt_overlay does it». vkDaedalus lo hace asi: una
   capa minima que solo cuenta vkQueuePresentKHR (VK_LAYER_VKDAEDALUS_present_counter), puesta debajo de MAKO por una
   meta-capa (MAKO genera debajo de la capa principal: arriba se ven los reales, debajo reales y generados). Aqui igual:
   VK_LAYER_odysseyvk_contador (manifiesto explicito) y la meta-capa VK_LAYER_odysseyvk_mako_contado, con
   ODYSSEYVK_CONTAR_MAKO=1: [esta capa, MAKO, el contador]. MEDIDO el 27-nov con el cargador (VK_LOADER_DEBUG=layer): asi
   queda Neural Forge → OdysseyVK → MAKO → contador; con el contador solo en la meta-capa, el cargador ponia la meta-capa
   delante de las demas implicitas y esta capa acababa DEBAJO de MAKO.
   Las dos cuentas van a /odysseyvk-fg-<pid> (protocolo.h: OdvkFgCuenta). Las dos capas son la misma biblioteca, asi que
   el puntero es el mismo; lo crea el contador al crear su dispositivo. Nada de registro por cuadro: dos sumas. */
namespace {

std::atomic<OdvkFgCuenta *> g_fgCuenta{nullptr};
bool g_fgMia = false;

void contarReal() {
  if (OdvkFgCuenta *c = g_fgCuenta.load(std::memory_order_relaxed)) __atomic_fetch_add(&c->reales, 1, __ATOMIC_RELAXED);
}

std::string nombreFg() { return "/odysseyvk-fg-" + std::to_string((long)getpid()); }

void abrirFg() {
  static std::mutex mx;
  std::lock_guard<std::mutex> g(mx);
  if (g_fgCuenta.load()) return;
  /* el generador de MAKO (mako-fg-host) es otro programa de Vulkan con el entorno del juego: tambien carga estas capas, pero
     no presenta nada. MEDIDO: su region salia vacia junto a la del juego */
  {
    char exe[512];
    const ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0 && std::strstr(std::string(exe, (size_t)n).c_str(), "mako-fg-host")) return;
  }
  /* las de juegos que se mataron (sin su destructor): las de un pid que ya no existe */
  if (DIR *dir = opendir("/dev/shm")) {
    while (dirent *e = readdir(dir)) {
      long otro = 0;
      if (std::sscanf(e->d_name, "odysseyvk-fg-%ld", &otro) == 1 && otro > 0 && otro != (long)getpid() && kill((pid_t)otro, 0) != 0 &&
          errno == ESRCH)
        shm_unlink((std::string("/") + e->d_name).c_str());
    }
    closedir(dir);
  }
  const std::string nombre = nombreFg();
  shm_unlink(nombre.c_str());   /* la de un juego anterior con el mismo pid */
  const int fd = shm_open(nombre.c_str(), O_CREAT | O_RDWR, 0600);
  if (fd < 0) return;
  void *m = MAP_FAILED;
  if (ftruncate(fd, ODVK_FG_TAM) == 0) m = mmap(nullptr, ODVK_FG_TAM, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (m == MAP_FAILED) {
    shm_unlink(nombre.c_str());
    return;
  }
  OdvkFgCuenta *c = (OdvkFgCuenta *)m;
  c->version = ODVK_FG_VERSION, c->pid = (int32_t)getpid();
  __atomic_store_n(&c->magia, ODVK_FG_MAGIA, __ATOMIC_RELEASE);
  g_fgMia = true;
  g_fgCuenta.store(c);
  anota("[contador] la generacion de cuadros de MAKO se cuenta en %s (reales encima, salida debajo)", nombre.c_str());
}

__attribute__((destructor)) void cerrarFg() {
  if (g_fgMia) shm_unlink(nombreFg().c_str());
}

struct ContInstancia {
  VkInstance instancia = VK_NULL_HANDLE;
  PFN_vkGetInstanceProcAddr gipa = nullptr;
  PFN_vkDestroyInstance destruir = nullptr;
};
struct ContDispositivo {
  PFN_vkGetDeviceProcAddr gdpa = nullptr;
  PFN_vkDestroyDevice destruir = nullptr;
  PFN_vkQueuePresentKHR presentar = nullptr;
};
std::mutex g_contMx;
std::unordered_map<void *, ContInstancia> g_contInst;
std::unordered_map<void *, ContDispositivo> g_contDisp;

template <typename H>
void *claveCont(H h) {
  return *reinterpret_cast<void **>(h);
}

VKAPI_ATTR VkResult VKAPI_CALL ContCreateInstance(const VkInstanceCreateInfo *ci, const VkAllocationCallbacks *a, VkInstance *inst) {
  auto *l = (VkLayerInstanceCreateInfo *)ci->pNext;
  while (l && !(l->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && l->function == VK_LAYER_LINK_INFO))
    l = (VkLayerInstanceCreateInfo *)l->pNext;
  if (!l || !l->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
  const PFN_vkGetInstanceProcAddr sig = l->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  l->u.pLayerInfo = l->u.pLayerInfo->pNext;
  auto crear = (PFN_vkCreateInstance)sig(VK_NULL_HANDLE, "vkCreateInstance");
  if (!crear) return VK_ERROR_INITIALIZATION_FAILED;
  const VkResult r = crear(ci, a, inst);
  if (r != VK_SUCCESS) return r;
  std::lock_guard<std::mutex> g(g_contMx);
  g_contInst[claveCont(*inst)] = {*inst, sig, (PFN_vkDestroyInstance)sig(*inst, "vkDestroyInstance")};
  return r;
}

VKAPI_ATTR void VKAPI_CALL ContDestroyInstance(VkInstance inst, const VkAllocationCallbacks *a) {
  if (!inst) return;
  PFN_vkDestroyInstance destruir = nullptr;
  {
    std::lock_guard<std::mutex> g(g_contMx);
    auto it = g_contInst.find(claveCont(inst));
    if (it == g_contInst.end()) return;
    destruir = it->second.destruir;
    g_contInst.erase(it);
  }
  if (destruir) destruir(inst, a);
}

VKAPI_ATTR VkResult VKAPI_CALL ContCreateDevice(VkPhysicalDevice f, const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *a, VkDevice *dev) {
  auto *l = (VkLayerDeviceCreateInfo *)ci->pNext;
  while (l && !(l->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && l->function == VK_LAYER_LINK_INFO))
    l = (VkLayerDeviceCreateInfo *)l->pNext;
  if (!l || !l->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
  const PFN_vkGetInstanceProcAddr sigI = l->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  const PFN_vkGetDeviceProcAddr sigD = l->u.pLayerInfo->pfnNextGetDeviceProcAddr;
  l->u.pLayerInfo = l->u.pLayerInfo->pNext;
  VkInstance inst = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> g(g_contMx);
    auto it = g_contInst.find(claveCont(f));
    if (it != g_contInst.end()) inst = it->second.instancia;
  }
  if (inst == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
  auto crear = (PFN_vkCreateDevice)sigI(inst, "vkCreateDevice");
  if (!crear) return VK_ERROR_INITIALIZATION_FAILED;
  const VkResult r = crear(f, ci, a, dev);
  if (r != VK_SUCCESS) return r;
  ContDispositivo d;
  d.gdpa = sigD;
  d.destruir = (PFN_vkDestroyDevice)sigD(*dev, "vkDestroyDevice");
  d.presentar = (PFN_vkQueuePresentKHR)sigD(*dev, "vkQueuePresentKHR");
  {
    std::lock_guard<std::mutex> g(g_contMx);
    g_contDisp[claveCont(*dev)] = d;
  }
  if (d.presentar) abrirFg();
  return r;
}

VKAPI_ATTR void VKAPI_CALL ContDestroyDevice(VkDevice dev, const VkAllocationCallbacks *a) {
  if (!dev) return;
  PFN_vkDestroyDevice destruir = nullptr;
  {
    std::lock_guard<std::mutex> g(g_contMx);
    auto it = g_contDisp.find(claveCont(dev));
    if (it == g_contDisp.end()) return;
    destruir = it->second.destruir;
    g_contDisp.erase(it);
  }
  if (destruir) destruir(dev, a);
}

VKAPI_ATTR VkResult VKAPI_CALL ContQueuePresentKHR(VkQueue q, const VkPresentInfoKHR *pi) {
  PFN_vkQueuePresentKHR presentar = nullptr;
  {
    std::lock_guard<std::mutex> g(g_contMx);
    auto it = g_contDisp.find(claveCont(q));
    if (it != g_contDisp.end()) presentar = it->second.presentar;
  }
  if (OdvkFgCuenta *c = g_fgCuenta.load(std::memory_order_relaxed)) __atomic_fetch_add(&c->salida, 1, __ATOMIC_RELAXED);
  return presentar ? presentar(q, pi) : VK_ERROR_DEVICE_LOST;
}

}  // namespace

EXPORTA VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL OdysseyContador_GetDeviceProcAddr(VkDevice dev, const char *n);
EXPORTA VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL OdysseyContador_GetInstanceProcAddr(VkInstance inst, const char *n);

EXPORTA VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL OdysseyContador_GetDeviceProcAddr(VkDevice dev, const char *n) {
  if (!std::strcmp(n, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)&OdysseyContador_GetDeviceProcAddr;
  if (!std::strcmp(n, "vkDestroyDevice")) return (PFN_vkVoidFunction)&ContDestroyDevice;
  PFN_vkGetDeviceProcAddr sig = nullptr;
  bool presenta = false;
  {
    std::lock_guard<std::mutex> g(g_contMx);
    auto it = g_contDisp.find(claveCont(dev));
    if (it == g_contDisp.end()) return nullptr;
    sig = it->second.gdpa, presenta = it->second.presentar != nullptr;
  }
  if (!std::strcmp(n, "vkQueuePresentKHR") && presenta) return (PFN_vkVoidFunction)&ContQueuePresentKHR;
  return sig(dev, n);
}

EXPORTA VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL OdysseyContador_GetInstanceProcAddr(VkInstance inst, const char *n) {
  if (!std::strcmp(n, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)&OdysseyContador_GetInstanceProcAddr;
  if (!std::strcmp(n, "vkCreateInstance")) return (PFN_vkVoidFunction)&ContCreateInstance;
  if (!std::strcmp(n, "vkDestroyInstance")) return (PFN_vkVoidFunction)&ContDestroyInstance;
  if (!std::strcmp(n, "vkCreateDevice")) return (PFN_vkVoidFunction)&ContCreateDevice;
  if (!std::strcmp(n, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)&OdysseyContador_GetDeviceProcAddr;
  if (!std::strcmp(n, "vkDestroyDevice")) return (PFN_vkVoidFunction)&ContDestroyDevice;
  if (inst == VK_NULL_HANDLE) return nullptr;
  PFN_vkGetInstanceProcAddr sig = nullptr;
  {
    std::lock_guard<std::mutex> g(g_contMx);
    auto it = g_contInst.find(claveCont(inst));
    if (it == g_contInst.end()) return nullptr;
    sig = it->second.gipa;
  }
  return sig(inst, n);
}

/* el contador con su propia negociacion, nombrada en su manifiesto: si no, el cargador busca en la biblioteca la de nombre
   de fabrica (vkNegotiateLoaderLayerInterfaceVersion), que es la de la capa principal, y pone la capa principal tambien
   debajo de MAKO. MEDIDO el 27-nov: vkcube se caia (SIGSEGV dentro de vkCreateInstance de MAKO) */
EXPORTA VKAPI_ATTR VkResult VKAPI_CALL OdysseyContador_Negotiate(VkNegotiateLayerInterface *v) {
  if (!v || v->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT) return VK_ERROR_INITIALIZATION_FAILED;
  if (v->loaderLayerInterfaceVersion >= 2) {
    v->pfnGetInstanceProcAddr = &OdysseyContador_GetInstanceProcAddr;
    v->pfnGetDeviceProcAddr = &OdysseyContador_GetDeviceProcAddr;
    v->pfnGetPhysicalDeviceProcAddr = nullptr;
    v->loaderLayerInterfaceVersion = 2;
  }
  return VK_SUCCESS;
}

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
