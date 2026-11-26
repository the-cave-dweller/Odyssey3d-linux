/* vkdev.h — instancia, dispositivo y colas Vulkan + importación de DMA-BUF.
   Todo el dibujado del proyecto va sobre Vulkan: el fotograma que entrega KWin llega
   como DMA-BUF (con modificador de NVIDIA) y se importa directo a una VkImage, sin
   pasar por la CPU. */
#ifndef ODYSSEY_VKDEV_H
#define ODYSSEY_VKDEV_H

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

struct GpuFrame; /* ver glimport.h: fd, ancho, alto, paso, offset, modificador, fourcc */

struct VkImg {
  VkImage img = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  int w = 0, h = 0;
  bool imported = false;
  bool valid() const { return img != VK_NULL_HANDLE; }
};

class VkDev {
public:
  VkDev() = default;
  ~VkDev();
  VkDev(const VkDev &) = delete;
  VkDev &operator=(const VkDev &) = delete;

  /* Crea la instancia. extraInstExts: extensiones de superficie (las pide GLFW). */
  bool init(const std::vector<const char *> &extraInstExts, std::string *err = nullptr);
  /* Elige la GPU: discreta, con importación de DMA-BUF y (si se pasa) que presente en
     esa superficie. Devuelve el nombre en deviceName(). */
  /* gpuIndex >= 0 fuerza esa GPU (para pruebas); -1 = la mejor. */
  bool createDevice(const std::vector<const char *> &extraDevExts, VkSurfaceKHR surface, std::string *err = nullptr,
                    int gpuIndex = -1);
  void destroy();
  bool ok() const { return device_ != VK_NULL_HANDLE; }

  VkInstance instance() const { return inst_; }
  VkPhysicalDevice phys() const { return phys_; }
  VkDevice device() const { return device_; }
  VkQueue queue() const { return queue_; }
  uint32_t queueFamily() const { return qFamily_; }
  const std::string &deviceName() const { return devName_; }
  bool hasDmaBufImport() const { return dmaBufExt_; }
  /* la prioridad que dio el controlador a nuestra cola (rama experimental): "normal", "alta" o
     "tiempo real". Se pide con ODYSSEYVK_PRIORIDAD=alta|tiempo_real; si no la dan, normal. */
  const std::string &prioridad() const { return prioridad_; }
  const std::string &prioridadNota() const { return prioridadNota_; }

  /* CuatroCC DRM -> VkFormat (XR24 = B8G8R8A8, XB24 = R8G8B8A8, ...). */
  static VkFormat vkFormatFor(uint32_t fourcc);
  static const char *vkFormatName(VkFormat f);
  static std::string modifierName(uint64_t m);

  /* Modificadores DRM que admite esta GPU para ese formato (para el diagnóstico). */
  std::string describeModifiers(VkFormat f) const;
  /* Qué handles externos admite para ese formato (OPAQUE_FD / DMA-BUF). */
  std::string describeExternal(VkFormat f) const;
  /* ¿Se puede importar este fotograma tal cual? Deja en *fmt el formato a usar. */
  bool canImport(const GpuFrame &f, VkFormat *fmt, std::string *err = nullptr) const;

  /* DMA-BUF -> VkImage con tiling VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT.
     El fd se duplica: el imagen es dueña de su referencia.
     exigirConsulta = false intenta la importación aunque la consulta formal diga que no
     (NVIDIA no reporta los handles externos en la consulta pero sí acepta el fd). */
  bool importImage(const GpuFrame &f, VkImg &out, std::string *err = nullptr, bool exigirConsulta = true);
  /* Segundo intento: DMA-BUF "implícito" (tiling OPTIMAL, sin cadena de modificadores).
     Es la vía que usan los drivers de NVIDIA cuando no reconocen el modificador. */
  bool importImageImplicit(const GpuFrame &f, VkImg &out, std::string *info, std::string *err = nullptr);
  void destroyImg(VkImg &i);

  /* Imagen lineal propia (para el resultado del trazado / lectura por CPU). */
  bool createImage(int w, int h, VkFormat f, VkImageUsageFlags usage, VkImg &out, std::string *err = nullptr);

  bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer &buf,
                    VkDeviceMemory &mem, std::string *err = nullptr);
  bool uploadBytes(const void *data, VkDeviceSize n, VkBuffer &buf, VkDeviceMemory &mem, std::string *err = nullptr);

  VkSampler sampler() const { return sampler_; }
  VkDescriptorPool descPool() const { return descPool_; }
  bool ensurePool(uint32_t sets, std::string *err = nullptr);

  VkCommandBuffer beginOneTime(std::string *err = nullptr);
  bool endOneTime(VkCommandBuffer cb, std::string *err = nullptr);

private:
  /* Deja el motivo del fallo en el puntero que pasó quien llamó. */
  static void setErr(const std::string &s, std::string *e) {
    if (e) *e = s;
  }

  VkInstance inst_ = VK_NULL_HANDLE;
  VkPhysicalDevice phys_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  uint32_t qFamily_ = 0;
  std::string prioridad_ = "normal", prioridadNota_;
  VkCommandPool cmdPool_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkDescriptorPool descPool_ = VK_NULL_HANDLE;
  VkPhysicalDeviceMemoryProperties memProps_{};
  std::string devName_;
  mutable std::string err_;
  bool dmaBufExt_ = false;
};

/* Nombre legible de un VkResult. */
const char *vkResultName(VkResult r);

#endif
