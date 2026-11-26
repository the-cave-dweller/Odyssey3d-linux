/* vkdev.cpp — implementación de VkDev (ver vkdev.h). */
#include "vkdev.h"
#include "frame.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

const char *vkResultName(VkResult r) {
  switch (r) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "sin memoria de host";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "sin memoria de dispositivo";
    case VK_ERROR_INITIALIZATION_FAILED: return "fallo de inicialización";
    case VK_ERROR_DEVICE_LOST: return "GPU perdida (VK_ERROR_DEVICE_LOST)";
    case VK_ERROR_MEMORY_MAP_FAILED: return "fallo al mapear memoria";
    case VK_ERROR_LAYER_NOT_PRESENT: return "capa no presente";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "extensión no presente";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "función no soportada";
    case VK_ERROR_TOO_MANY_OBJECTS: return "demasiados objetos";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "formato no soportado";
    case VK_ERROR_UNKNOWN: return "error desconocido";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "manejo externo inválido (DMA-BUF)";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "distribución de planos DRM inválida";
    default: break;
  }
  return "error de Vulkan";
}

VkDev::~VkDev() { destroy(); }

#define VKTRY(call)                                                    \
  do {                                                                 \
    VkResult r_ = (call);                                              \
    if (r_ != VK_SUCCESS) {                                            \
      setErr(std::string(#call) + ": " + vkResultName(r_), err);            \
      return false;                                                    \
    }                                                                  \
  } while (0)

bool VkDev::init(const std::vector<const char *> &extraInstExts, std::string *err) {
  (void)err;
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.apiVersion = VK_API_VERSION_1_3;
  app.applicationVersion = VK_MAKE_VERSION(0, 4, 0);
  app.engineVersion = app.applicationVersion;

  VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ci.pApplicationInfo = &app;
  ci.enabledExtensionCount = static_cast<uint32_t>(extraInstExts.size());
  ci.ppEnabledExtensionNames = extraInstExts.data();
  /* Sin capas: no las necesitamos (y si el entorno fuerza una inexistente, avisamos abajo). */
  VkResult r = vkCreateInstance(&ci, nullptr, &inst_);
  if (r == VK_ERROR_LAYER_NOT_PRESENT) {
    setErr("hay una capa Vulkan forzada en el entorno (por ejemplo VK_LAYER_*) que no está instalada", err);
    return false;
  }
  if (r != VK_SUCCESS) {
    setErr(std::string("vkCreateInstance: ") + vkResultName(r), err);
    return false;
  }
  return true;
}

VkFormat VkDev::vkFormatFor(uint32_t fourcc) {
  switch (fourcc) {
    case fourccOf('X', 'R', '2', '4'): return VK_FORMAT_B8G8R8A8_UNORM; /* memoria B,G,R,X */
    case fourccOf('X', 'B', '2', '4'): return VK_FORMAT_R8G8B8A8_UNORM; /* memoria R,G,B,X */
    case fourccOf('A', 'B', '2', '4'): return VK_FORMAT_B8G8R8A8_UNORM;
    case fourccOf('A', 'R', '2', '4'): return VK_FORMAT_R8G8B8A8_UNORM;
    default: return VK_FORMAT_UNDEFINED; /* NV12 y demás formatos de 2 planos no se usan aquí */
  }
}

const char *VkDev::vkFormatName(VkFormat f) {
  switch (f) {
    case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
    case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
    case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
    case VK_FORMAT_UNDEFINED: return "UNDEFINED";
    default: return "otro";
  }
}

std::string VkDev::modifierName(uint64_t m) {
  if (m == 0) return "lineal";
  unsigned vendor = (unsigned)((m >> 56) & 0x3f);
  const char *v = "desconocido";
  switch (vendor) {
    case 0: v = "GENERIC"; break;
    case 1: v = "INTEL"; break;
    case 2: v = "AMD"; break;
    case 3: v = "NVIDIA"; break;
    case 4: v = "VC4"; break;
    case 5: v = "VIV"; break;
    case 6: v = "ETVNAV"; break;
    case 7: v = "BRCM"; break;
    case 8: v = "EXYNOS"; break;
    case 9: v = "AMLOGIC"; break;
    case 10: v = "MESAX"; break;
    case 11: v = "ALLWINNER"; break;
    case 12: v = "ARM"; break;
    case 13: v = "QCOM"; break;
    default: break;
  }
  char buf[96];
  std::snprintf(buf, sizeof buf, "0x%016llx (%s)", (unsigned long long)m, v);
  return buf;
}

bool VkDev::createDevice(const std::vector<const char *> &extraDevExts, VkSurfaceKHR surface, std::string *err,
                           int gpuIndex) {
  uint32_t n = 0;
  vkEnumeratePhysicalDevices(inst_, &n, nullptr);
  if (!n) {
    setErr("no hay ninguna GPU con controlador Vulkan", err);
    return false;
  }
  std::vector<VkPhysicalDevice> devs(n);
  vkEnumeratePhysicalDevices(inst_, &n, devs.data());

  int best = -1;
  int bestScore = -1;
  std::string why;
  for (uint32_t i = 0; i < n; i++) {
    VkPhysicalDeviceProperties p{};
    vkGetPhysicalDeviceProperties(devs[i], &p);

    uint32_t en = 0;
    vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &en, nullptr);
    std::vector<VkExtensionProperties> ex(en);
    vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &en, ex.data());
    auto has = [&](const char *name) {
      for (auto &e : ex)
        if (std::strcmp(e.extensionName, name) == 0) return true;
      return false;
    };
    bool db = has("VK_EXT_external_memory_dma_buf") && has("VK_EXT_image_drm_format_modifier") &&
              has("VK_KHR_external_memory_fd");
    bool canPresent = true;
    if (surface != VK_NULL_HANDLE) {
      uint32_t qn = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qn, nullptr);
      std::vector<VkQueueFamilyProperties> qp(qn);
      vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &qn, qp.data());
      canPresent = false;
      for (uint32_t q = 0; q < qn; q++) {
        VkBool32 ok = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(devs[i], q, surface, &ok);
        if (ok) canPresent = true;
      }
    }
    int score = 0;
    if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 4;
    if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 2;
    if (db) score += 3;
    if (canPresent) score += 2;
    std::printf("[vulkan] gpu %u: %s · %s · dma-buf %s · presentación %s\n", i, p.deviceName,
                p.apiVersion >= VK_API_VERSION_1_3 ? "API >= 1.3" : "API < 1.3", db ? "sí" : "no",
                canPresent ? "sí" : "no");
    if (canPresent && score > bestScore) {
      bestScore = score;
      best = (int)i;
    }
    if (!canPresent) why = std::string(p.deviceName) + " no puede presentar en esa ventana";
  }
  if (gpuIndex >= 0 && gpuIndex < (int)n) best = gpuIndex;
  if (best < 0) {
    setErr(why.empty() ? "ninguna GPU sirve para esta ventana" : why, err);
    return false;
  }
  phys_ = devs[best];
  VkPhysicalDeviceProperties pp{};
  vkGetPhysicalDeviceProperties(phys_, &pp);
  devName_ = pp.deviceName;

  uint32_t qn = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qn, nullptr);
  std::vector<VkQueueFamilyProperties> qp(qn);
  vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qn, qp.data());
  bool found = false;
  for (uint32_t q = 0; q < qn; q++) {
    if (!(qp[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
    if (surface != VK_NULL_HANDLE) {
      VkBool32 ok = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(phys_, q, surface, &ok);
      if (!ok) continue;
    }
    qFamily_ = q;
    found = true;
    break;
  }
  if (!found) {
    setErr("no hay cola gráfica que presente en esta ventana", err);
    return false;
  }

  uint32_t en = 0;
  vkEnumerateDeviceExtensionProperties(phys_, nullptr, &en, nullptr);
  std::vector<VkExtensionProperties> devExts(en);
  vkEnumerateDeviceExtensionProperties(phys_, nullptr, &en, devExts.data());
  auto has = [&](const char *name) {
    for (auto &e : devExts)
      if (std::strcmp(e.extensionName, name) == 0) return true;
    return false;
  };
  dmaBufExt_ = has("VK_EXT_external_memory_dma_buf") && has("VK_EXT_image_drm_format_modifier") &&
               has("VK_KHR_external_memory_fd");

  std::vector<const char *> names = extraDevExts;
  const char *opt[] = {"VK_EXT_external_memory_dma_buf", "VK_EXT_image_drm_format_modifier",
                       "VK_KHR_external_memory_fd", "VK_EXT_queue_family_foreign"};
  for (const char *o : opt)
    if (has(o)) names.push_back(o);

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = qFamily_;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;

  /* Necesitamos la representación dinámica (dibujar sin render pass), que en Vulkan 1.3
     es una función opcional y hay que pedirla explícitamente. */
  VkPhysicalDeviceVulkan13Features f13{};
  f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;

  /* Prioridad de la cola (rama experimental, tirones en Cyberpunk 26-nov): con el juego llenando la
     tarjeta, nuestros cuadros esperaban en la fila detras de los suyos 15-23 ms al presentar. Con
     prioridad alta la tarjeta pasa el cuadro del visor por delante, como hacen los compositores de
     realidad virtual; el juego pierde un poco. Si el controlador no la da (VK_ERROR_NOT_PERMITTED),
     se crea el dispositivo con la prioridad de siempre. */
  VkDeviceQueueGlobalPriorityCreateInfoKHR gp{VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR};
  std::string pedida;
  if (const char *e = std::getenv("ODYSSEYVK_PRIORIDAD")) pedida = e;
  const bool quiereAlta = pedida == "alta" || pedida == "high" || pedida == "1";
  const bool quiereTR = pedida == "tiempo_real" || pedida == "realtime" || pedida == "2";
  const char *extPrio = has("VK_KHR_global_priority") ? "VK_KHR_global_priority"
                        : (has("VK_EXT_global_priority") ? "VK_EXT_global_priority" : nullptr);
  VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  di.pNext = &f13;
  di.queueCreateInfoCount = 1;
  di.pQueueCreateInfos = &qci;
  VkResult rDev = VK_ERROR_INITIALIZATION_FAILED;
  if ((quiereAlta || quiereTR) && extPrio) {
    std::vector<const char *> conPrio = names;
    conPrio.push_back(extPrio);
    for (int intento = 0; intento < 2; intento++) {
      const bool tr = quiereTR && intento == 0;
      if (intento == 1 && !quiereTR) break;
      gp.globalPriority = tr ? VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR : VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR;
      qci.pNext = &gp;
      di.enabledExtensionCount = static_cast<uint32_t>(conPrio.size());
      di.ppEnabledExtensionNames = conPrio.data();
      rDev = vkCreateDevice(phys_, &di, nullptr, &device_);
      if (rDev == VK_SUCCESS) {
        prioridad_ = tr ? "tiempo real" : "alta";
        break;
      }
      prioridadNota_ += std::string(prioridadNota_.empty() ? "" : "; ") + (tr ? "tiempo real" : "alta") + " no: " +
                        (rDev == VK_ERROR_NOT_PERMITTED_KHR ? std::string("sin permiso (hace falta CAP_SYS_NICE)")
                                                            : std::string(vkResultName(rDev)) + " " + std::to_string((int)rDev));
    }
  } else if (quiereAlta || quiereTR) {
    prioridadNota_ = "la tarjeta no ofrece VK_KHR_global_priority";
  }
  if (rDev != VK_SUCCESS) {
    qci.pNext = nullptr;
    di.enabledExtensionCount = static_cast<uint32_t>(names.size());
    di.ppEnabledExtensionNames = names.data();
    VKTRY(vkCreateDevice(phys_, &di, nullptr, &device_));
  }
  if (!f13.dynamicRendering) {
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
    setErr("esta GPU no ofrece VK_KHR_dynamic_rendering (hace falta Vulkan 1.3)", err);
    return false;
  }
  vkGetDeviceQueue(device_, qFamily_, 0, &queue_);

  VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  cpi.queueFamilyIndex = qFamily_;
  cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VKTRY(vkCreateCommandPool(device_, &cpi, nullptr, &cmdPool_));

  VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  si.magFilter = VK_FILTER_LINEAR;
  si.minFilter = VK_FILTER_LINEAR;
  si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  si.mipLodBias = 0.0f;
  si.minLod = 0.0f;
  si.maxLod = 0.0f;
  si.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  VKTRY(vkCreateSampler(device_, &si, nullptr, &sampler_));
  return true;
}

void VkDev::destroyImg(VkImg &i) {
  if (device_ == VK_NULL_HANDLE) return;
  if (i.view != VK_NULL_HANDLE) vkDestroyImageView(device_, i.view, nullptr);
  if (i.img != VK_NULL_HANDLE) vkDestroyImage(device_, i.img, nullptr);
  if (i.mem != VK_NULL_HANDLE) vkFreeMemory(device_, i.mem, nullptr);
  i = VkImg{};
}

void VkDev::destroy() {
  if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
  if (descPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, descPool_, nullptr);
  if (sampler_ != VK_NULL_HANDLE) vkDestroySampler(device_, sampler_, nullptr);
  if (cmdPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, cmdPool_, nullptr);
  if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
  if (inst_ != VK_NULL_HANDLE) vkDestroyInstance(inst_, nullptr);
  device_ = VK_NULL_HANDLE;
  inst_ = VK_NULL_HANDLE;
  phys_ = VK_NULL_HANDLE;
  queue_ = VK_NULL_HANDLE;
  sampler_ = VK_NULL_HANDLE;
  descPool_ = VK_NULL_HANDLE;
  cmdPool_ = VK_NULL_HANDLE;
}

std::string VkDev::describeModifiers(VkFormat f) const {
  if (device_ == VK_NULL_HANDLE) return "sin dispositivo";
  VkDrmFormatModifierPropertiesListEXT list{};
  list.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
  list.drmFormatModifierCount = 0;
  list.pDrmFormatModifierProperties = nullptr;
  VkPhysicalDeviceImageFormatInfo2 info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
  info.format = f;
  info.type = VK_IMAGE_TYPE_2D;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  VkImageFormatProperties2 props{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};
  props.pNext = &list;
  VkResult r = vkGetPhysicalDeviceImageFormatProperties2(phys_, &info, &props);
  if (r != VK_SUCCESS) return std::string("la consulta falló: ") + vkResultName(r);
  uint32_t cnt = list.drmFormatModifierCount;
  if (!cnt) return std::string(vkFormatName(f)) + ": sin modificadores DRM";
  std::vector<VkDrmFormatModifierPropertiesEXT> v(cnt);
  list.pDrmFormatModifierProperties = v.data();
  vkGetPhysicalDeviceImageFormatProperties2(phys_, &info, &props);
  std::string out = std::string(vkFormatName(f)) + ": " + std::to_string(cnt) + " modificadores";
  for (uint32_t i = 0; i < cnt && i < 40; i++) {
    out += "\n  ";
    out += modifierName(v[i].drmFormatModifier);
  }
  return out;
}

/* Qué handles externos admite esta GPU para un formato normal (sin modificador DRM). */
std::string VkDev::describeExternal(VkFormat f) const {
  if (device_ == VK_NULL_HANDLE) return "sin dispositivo";
  VkPhysicalDeviceExternalImageFormatInfo extInfo{};
  extInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
  extInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  VkPhysicalDeviceImageFormatInfo2 info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
  info.format = f;
  info.type = VK_IMAGE_TYPE_2D;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  info.pNext = &extInfo;
  VkExternalImageFormatProperties ext{};
  ext.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
  VkImageFormatProperties2 props{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};
  props.pNext = &ext;
  VkResult r = vkGetPhysicalDeviceImageFormatProperties2(phys_, &info, &props);
  if (r != VK_SUCCESS) return std::string(vkFormatName(f)) + ": " + vkResultName(r);
  char buf[128];
  std::snprintf(buf, sizeof buf, "handles 0x%x · rasgos 0x%x (importable %d, dedicada %d, exportable %d)",
                static_cast<unsigned>(ext.externalMemoryProperties.compatibleHandleTypes),
                static_cast<unsigned>(ext.externalMemoryProperties.externalMemoryFeatures),
                (ext.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) ? 1 : 0,
                (ext.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) ? 1 : 0,
                (ext.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) ? 1 : 0);
  return std::string(vkFormatName(f)) + ": " + buf;
}

/* Un fd de DRM PRIME se importa con DMA_BUF_BIT_EXT; OPAQUE_FD es el tipo que Vulkan usa
   para sus propios fds. Probamos los dos: el driver se queda con el fd en todo caso. */
static VkResult allocImportFd(VkDevice dev, VkImg &out, VkDeviceSize size, uint32_t memType, int srcFd, bool dedicada,
                              std::string *probados) {
  const VkExternalMemoryHandleTypeFlagBits tipos[] = {VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                                      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT};
  VkResult r = VK_ERROR_INITIALIZATION_FAILED;
  for (VkExternalMemoryHandleTypeFlagBits ht : tipos) {
    VkImportMemoryFdInfoKHR imp{};
    imp.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    imp.handleType = ht;
    imp.fd = ::dup(srcFd);
    if (imp.fd < 0) {
      if (probados) *probados += "dup() del fd falló ";
      return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkMemoryDedicatedAllocateInfo dai{};
    dai.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dai.image = out.img;
    imp.pNext = dedicada ? &dai : nullptr;
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.pNext = &imp;
    mai.allocationSize = size;
    mai.memoryTypeIndex = memType;
    r = vkAllocateMemory(dev, &mai, nullptr, &out.mem);
    if (probados)
      *probados += std::string(ht == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT ? "DMA-BUF" : "OPAQUE_FD") + " -> " +
                   vkResultName(r) + " ";
    if (r == VK_SUCCESS) break;
  }
  return r;
}

bool VkDev::canImport(const GpuFrame &f, VkFormat *fmt, std::string *err) const {
  if (!dmaBufExt_) {
    setErr("la GPU no tiene VK_EXT_external_memory_dma_buf / VK_EXT_image_drm_format_modifier", err);
    return false;
  }
  static const VkFormat tryFmt[] = {vkFormatFor(f.fourcc), VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
  std::string motivo;
  for (VkFormat cand : tryFmt) {
    if (cand == VK_FORMAT_UNDEFINED) continue;
    /* Para PREGUNTAR por capacidades van otras estructuras (lo dice la capa de
       validación): VkPhysicalDeviceExternalImageFormatInfo y
       VkPhysicalDeviceImageDrmFormatModifierInfoEXT. Las de VkExternalMemoryImageCreateInfo
       y la lista de modificadores son sólo para crear la imagen. */
    VkPhysicalDeviceExternalImageFormatInfo extInfo{};
    extInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    extInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    VkPhysicalDeviceImageDrmFormatModifierInfoEXT drm{};
    drm.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
    drm.drmFormatModifier = f.modifier;
    drm.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    extInfo.pNext = &drm;
    VkPhysicalDeviceImageFormatInfo2 info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
    info.format = cand;
    info.type = VK_IMAGE_TYPE_2D;
    info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.pNext = &extInfo;
    VkExternalImageFormatProperties ext{};
    ext.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    VkImageFormatProperties2 props{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};
    props.pNext = &ext;
    VkResult r = vkGetPhysicalDeviceImageFormatProperties2(phys_, &info, &props);
    if (r != VK_SUCCESS) {
      if (motivo.empty())
        motivo = std::string(vkFormatName(cand)) + ": " + vkResultName(r);
      continue;
    }
    const VkExternalMemoryHandleTypeFlags okTipos = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT |
                                                    VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    if (!(ext.externalMemoryProperties.compatibleHandleTypes & okTipos)) {
      if (motivo.empty()) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "0x%x", static_cast<unsigned>(ext.externalMemoryProperties.compatibleHandleTypes));
        motivo = std::string(vkFormatName(cand)) + ": handles compatibles " + buf;
      }
      continue;
    }
    if (fmt) *fmt = cand;
    return true;
  }
  setErr("esta GPU no puede importar el fotograma (fourcc " + std::string(fourccText(f.fourcc)) + " modificador " +
         modifierName(f.modifier) + (motivo.empty() ? "" : ", " + motivo) + ")", err);
  return false;
}

bool VkDev::importImage(const GpuFrame &f, VkImg &out, std::string *err, bool exigirConsulta) {
  if (!ok()) return false;
  if (f.fd < 0) {
    setErr("el fotograma no trae DMA-BUF", err);
    return false;
  }
  VkFormat fmt = VK_FORMAT_UNDEFINED;
  if (exigirConsulta) {
    if (!canImport(f, &fmt, err)) return false;
  } else {
    fmt = vkFormatFor(f.fourcc);
    if (fmt == VK_FORMAT_UNDEFINED) fmt = VK_FORMAT_B8G8R8A8_UNORM;
  }

  VkExternalMemoryImageCreateInfo emi{};
  emi.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  emi.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT | VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

  VkSubresourceLayout lay{};
  lay.offset = (VkDeviceSize)f.offset;
  lay.rowPitch = (VkDeviceSize)f.stride;
  lay.size = (VkDeviceSize)f.stride * (VkDeviceSize)f.h;
  VkImageDrmFormatModifierExplicitCreateInfoEXT ex{};
  ex.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
  ex.drmFormatModifier = f.modifier;
  ex.drmFormatModifierPlaneCount = 1;
  ex.pPlaneLayouts = &lay;

  VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ii.pNext = &ex;
  ex.pNext = &emi;
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = fmt;
  ii.extent = {(uint32_t)f.w, (uint32_t)f.h, 1};
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkResult r = vkCreateImage(device_, &ii, nullptr, &out.img);
  if (r != VK_SUCCESS) {
    setErr(std::string("vkCreateImage (DMA-BUF): ") + vkResultName(r) + " · fourcc " + fourccText(f.fourcc) +
           " · " + modifierName(f.modifier), err);
    out.img = VK_NULL_HANDLE;
    return false;
  }
  out.w = f.w;
  out.h = f.h;
  out.imported = true;

  /* El tamaño real del búfer DMA-BUF sale del propio fd (los PRIME traen st_size). */
  VkDeviceSize size = (VkDeviceSize)f.stride * (VkDeviceSize)f.h + (VkDeviceSize)f.offset;
  struct stat st;
  if (::fstat(f.fd, &st) == 0 && st.st_size > 0) size = (VkDeviceSize)st.st_size;

  /* Requisitos de la imagen: con tiling por modificador hay que usar la variante "2",
     y la memoria de un DMA-BUF casi siempre tiene que ser una asignación dedicada. */
  VkImageMemoryRequirementsInfo2 mri{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
  mri.image = out.img;
  VkMemoryDedicatedRequirements ded{};
  ded.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
  VkMemoryRequirements2 memReq2{};
  memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
  memReq2.pNext = &ded;
  vkGetImageMemoryRequirements2(device_, &mri, &memReq2);
  const VkMemoryRequirements &req = memReq2.memoryRequirements;
  uint32_t memType = 0;
  if (req.memoryTypeBits) {
    uint32_t bits = req.memoryTypeBits;
    while (!(bits & 1u)) {
      bits >>= 1;
      memType++;
    }
  }

  std::string probados;
  r = allocImportFd(device_, out, size, memType, f.fd,
                    ded.prefersDedicatedAllocation || ded.requiresDedicatedAllocation, &probados);
  if (r != VK_SUCCESS) {
    setErr(std::string("vkAllocateMemory del DMA-BUF: ") + vkResultName(r) + " (tamaño " + std::to_string(size) +
               " · tipo de memoria " + std::to_string(memType) + " · dedicada " +
               std::to_string(ded.requiresDedicatedAllocation ? 1 : 0) + " · " + probados + ")", err);
    vkDestroyImage(device_, out.img, nullptr);
    out.img = VK_NULL_HANDLE;
    out.mem = VK_NULL_HANDLE;
    return false;
  }
  VKTRY(vkBindImageMemory(device_, out.img, out.mem, 0));

  VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vi.image = out.img;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = fmt;
  vi.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                   VK_COMPONENT_SWIZZLE_IDENTITY};
  vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  VKTRY(vkCreateImageView(device_, &vi, nullptr, &out.view));
  return true;
}

/* Segundo intento: importar el DMA-BUF sin decirle al driver el modificador (tiling
   ÓPTIMO). Los drivers de NVIDIA no reconocen el modificador que publica KWin, pero a
   veces aceptan el fd por esta vía. */
bool VkDev::importImageImplicit(const GpuFrame &f, VkImg &out, std::string *info, std::string *err) {
  if (!ok() || f.fd < 0) {
    setErr("no hay DMA-BUF que importar", err);
    return false;
  }
  VkFormat fmt = vkFormatFor(f.fourcc);
  if (fmt == VK_FORMAT_UNDEFINED) fmt = VK_FORMAT_B8G8R8A8_UNORM;

  VkExternalMemoryImageCreateInfo emi{};
  emi.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  emi.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT | VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
  VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ii.pNext = &emi;
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = fmt;
  ii.extent = {(uint32_t)f.w, (uint32_t)f.h, 1};
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkResult r = vkCreateImage(device_, &ii, nullptr, &out.img);
  if (r != VK_SUCCESS) {
    out.img = VK_NULL_HANDLE;
    setErr(std::string("vkCreateImage (DMA-BUF sin modificador): ") + vkResultName(r), err);
    return false;
  }
  out.w = f.w;
  out.h = f.h;
  out.imported = true;

  VkImageMemoryRequirementsInfo2 mri{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
  mri.image = out.img;
  VkMemoryDedicatedRequirements ded{};
  ded.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
  VkMemoryRequirements2 memReq2{};
  memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
  memReq2.pNext = &ded;
  vkGetImageMemoryRequirements2(device_, &mri, &memReq2);
  const VkMemoryRequirements &req = memReq2.memoryRequirements;
  uint32_t memType = 0;
  if (req.memoryTypeBits) {
    uint32_t bits = req.memoryTypeBits;
    while (!(bits & 1u)) {
      bits >>= 1;
      memType++;
    }
  }
  VkDeviceSize size = (VkDeviceSize)f.stride * (VkDeviceSize)f.h + (VkDeviceSize)f.offset;
  struct stat st;
  if (::fstat(f.fd, &st) == 0 && st.st_size > 0) size = (VkDeviceSize)st.st_size;
  if (req.size > size) size = req.size;

  std::string probados2;
  r = allocImportFd(device_, out, size, memType, f.fd, true, &probados2);
  if (info)
    *info += "intento sin modificador: vkAllocateMemory -> " + std::string(vkResultName(r)) + " (tamaño " +
             std::to_string(size) + " · tipo " + std::to_string(memType) + " · bits de tipo 0x" +
             [&] { char b[16]; std::snprintf(b, sizeof b, "%x", req.memoryTypeBits); return std::string(b); }() +
             " · dedicada " + std::to_string(ded.requiresDedicatedAllocation ? 1 : 0) + " · " + probados2 + ")\n";
  if (r != VK_SUCCESS) {
    setErr(std::string("vkAllocateMemory (DMA-BUF sin modificador): ") + vkResultName(r), err);
    vkDestroyImage(device_, out.img, nullptr);
    out.img = VK_NULL_HANDLE;
    out.mem = VK_NULL_HANDLE;
    return false;
  }
  if (vkBindImageMemory(device_, out.img, out.mem, 0) != VK_SUCCESS) {
    setErr("vkBindImageMemory del DMA-BUF sin modificador falló", err);
    vkDestroyImage(device_, out.img, nullptr);
    vkFreeMemory(device_, out.mem, nullptr);
    out = VkImg{};
    return false;
  }
  VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vi.image = out.img;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = fmt;
  vi.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                   VK_COMPONENT_SWIZZLE_IDENTITY};
  vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  if (vkCreateImageView(device_, &vi, nullptr, &out.view) != VK_SUCCESS) {
    setErr("no se pudo crear la vista de la imagen importada", err);
    vkDestroyImage(device_, out.img, nullptr);
    vkFreeMemory(device_, out.mem, nullptr);
    out = VkImg{};
    return false;
  }
  return true;
}

bool VkDev::createImage(int w, int h, VkFormat f, VkImageUsageFlags usage, VkImg &out, std::string *err) {
  if (!ok()) return false;
  VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.format = f;
  ii.extent = {(uint32_t)w, (uint32_t)h, 1};
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.usage = usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VKTRY(vkCreateImage(device_, &ii, nullptr, &out.img));
  out.w = w;
  out.h = h;
  out.imported = false;

  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(device_, out.img, &req);
  VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  mai.allocationSize = req.size;
  bool done = false;
  vkGetPhysicalDeviceMemoryProperties(phys_, &memProps_);
  for (uint32_t i = 0; i < memProps_.memoryTypeCount; i++) {
    if (!(req.memoryTypeBits & (1u << i))) continue;
    if ((memProps_.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0) continue;
    mai.memoryTypeIndex = i;
    done = true;
    break;
  }
  if (!done) {
    for (uint32_t i = 0; i < memProps_.memoryTypeCount; i++) {
      if (!(req.memoryTypeBits & (1u << i))) continue;
      mai.memoryTypeIndex = i;
      done = true;
      break;
    }
  }
  if (!done) {
    setErr("no hay tipo de memoria para la imagen", err);
    return false;
  }
  VKTRY(vkAllocateMemory(device_, &mai, nullptr, &out.mem));
  VKTRY(vkBindImageMemory(device_, out.img, out.mem, 0));
  VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vi.image = out.img;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = f;
  vi.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                   VK_COMPONENT_SWIZZLE_IDENTITY};
  vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  VKTRY(vkCreateImageView(device_, &vi, nullptr, &out.view));
  return true;
}

bool VkDev::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer &buf,
                         VkDeviceMemory &mem, std::string *err) {
  if (!ok()) return false;
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = size;
  bi.usage = usage;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VKTRY(vkCreateBuffer(device_, &bi, nullptr, &buf));
  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(device_, buf, &req);
  VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  mai.allocationSize = req.size;
  vkGetPhysicalDeviceMemoryProperties(phys_, &memProps_);
  bool done = false;
  for (uint32_t i = 0; i < memProps_.memoryTypeCount; i++) {
    if (!(req.memoryTypeBits & (1u << i))) continue;
    if ((memProps_.memoryTypes[i].propertyFlags & props) != props) continue;
    mai.memoryTypeIndex = i;
    done = true;
    break;
  }
  if (!done) {
    setErr("no hay tipo de memoria para el búfer", err);
    vkDestroyBuffer(device_, buf, nullptr);
    buf = VK_NULL_HANDLE;
    return false;
  }
  VKTRY(vkAllocateMemory(device_, &mai, nullptr, &mem));
  VKTRY(vkBindBufferMemory(device_, buf, mem, 0));
  return true;
}

bool VkDev::uploadBytes(const void *data, VkDeviceSize n, VkBuffer &buf, VkDeviceMemory &mem, std::string *err) {
  if (!ok()) return false;
  if (!createBuffer(n, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, mem, err))
    return false;
  void *p = nullptr;
  VKTRY(vkMapMemory(device_, mem, 0, n, 0, &p));
  std::memcpy(p, data, (size_t)n);
  vkUnmapMemory(device_, mem);
  return true;
}

bool VkDev::ensurePool(uint32_t sets, std::string *err) {
  if (descPool_ != VK_NULL_HANDLE) return true;
  VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16},
                                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16}};
  VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pi.maxSets = sets;
  pi.poolSizeCount = 2;
  pi.pPoolSizes = sizes;
  VKTRY(vkCreateDescriptorPool(device_, &pi, nullptr, &descPool_));
  return true;
}

VkCommandBuffer VkDev::beginOneTime(std::string *err) {
  VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool = cmdPool_;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VkCommandBuffer cb = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device_, &ai, &cb) != VK_SUCCESS) {
    setErr("no se pudo reservar el búfer de comandos", err);
    return VK_NULL_HANDLE;
  }
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(cb, &bi) != VK_SUCCESS) {
    setErr("vkBeginCommandBuffer falló", err);
    return VK_NULL_HANDLE;
  }
  (void)err;
  return cb;
}

bool VkDev::endOneTime(VkCommandBuffer cb, std::string *err) {
  if (cb == VK_NULL_HANDLE) return false;
  if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
    setErr("vkEndCommandBuffer falló", err);
    return false;
  }
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;
  if (vkQueueSubmit(queue_, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
    setErr("vkQueueSubmit falló", err);
    return false;
  }
  VkResult r = vkQueueWaitIdle(queue_);
  vkFreeCommandBuffers(device_, cmdPool_, 1, &cb);
  if (r != VK_SUCCESS) {
    setErr(std::string("vkQueueWaitIdle: ") + vkResultName(r), err);
    return false;
  }
  return true;
}
