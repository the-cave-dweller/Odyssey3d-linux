/* vkweave.h — el visor por Vulkan: intercambio de imágenes (swapchain), la pasada de trama
   con triángulo a pantalla completa y la pasada de la interfaz encima, todo con dibujado
   dinámico (Vulkan 1.3) y sin render pass. El fotograma de KWin se muestrea directo desde su
   DMA-BUF: no pasa por la CPU. */
#ifndef ODYSSEY_VKWEAVE_H
#define ODYSSEY_VKWEAVE_H

#include "frame.h"
#include "vkdev.h"
#include "vkui.h"
#include "weave.h"

#include <cstdint>
#include <string>
#include <vector>

class VkWeave {
public:
  bool init(VkDev &dev, VkSurfaceKHR surface, int w, int h, std::string *err);
  void destroy();
  bool ok() const { return sc_ != VK_NULL_HANDLE; }
  int width() const { return ext_.width; }
  int height() const { return ext_.height; }
  VkFormat colorFormat() const { return fmt_; }

  /* fuente nueva: DMA-BUF importado o imagen subida por la CPU (patrón de prueba, PNG) */
  bool sourceFromDma(const GpuFrame &f, std::string *err);
  bool sourceFromRgba(const uint8_t *rgba, int w, int h, bool topFirst, std::string *err);
  bool hasSource() const { return gen_ > 0; }

  /* mapas de corrección del panel (gris R8) */
  void setCorrections(const uint8_t *a, int aw, int ah, const uint8_t *b, int bw, int bh);

  void setField(const LensField &f, const EyePose &e) { field_ = f, eye_ = e, cornersDirty_ = true; }
  void setPanelPixels(int w, int h) { pw_ = w, ph_ = h, cornersDirty_ = true; }
  WeaveTuning &tuning() { return tune_; }
  void setAtlas(const std::vector<uint8_t> &px, int w, int h);
  void setHud(const std::vector<HudVertex> &v) { hud_ = v; }
  void setHudState(int st) { hudState_ = st; }
  void setDepthShift(float d) { depthShift_ = d; }
  void setSrcFlipY(bool f) { srcFlipY_ = f; }

  /* cuadro fuera de pantalla: dibuja trama + interfaz en una imagen propia y la baja a la CPU
     (para --dump, sin tocar el panel). Devuelve RGBA con la primera fila arriba. */
  bool grab(std::vector<uint8_t> &rgbaTopFirst, std::string *err);

  /* un cuadro completo: adquirir, dibujar trama + interfaz, presentar */
  bool frame(std::string *err);
  bool resize(int w, int h);   /* intercambio perdido (ventana cambiada o fuera de fecha) */
  bool lost() const { return lost_; }
  void waitIdle();

private:
  struct FrameSlot {
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore sem = VK_NULL_HANDLE;
    VkDescriptorSet weave = VK_NULL_HANDLE, hud = VK_NULL_HANDLE;
    VkBuffer vbo = VK_NULL_HANDLE;
    VkDeviceMemory vboMem = VK_NULL_HANDLE;
    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceMemory uboMem = VK_NULL_HANDLE;
    size_t vboSize = 0;
    int gen = -1;         /* generación de la fuente que usó este hueco */
  };

  struct Keep {
    VkImg img;
    int gen = -1;
  };

  bool createSwapchain(int w, int h, std::string *err);
  void destroySwapchain();
  bool createPipelines(std::string *err);
  bool createFrames(std::string *err);
  void destroyFrames();
  bool uploadR8(const uint8_t *px, int w, int h, VkImg &out, std::string *err);
  bool uploadRgba8(const uint8_t *px, int w, int h, VkImg &out, std::string *err);
  void barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to, VkAccessFlags srcA,
               VkAccessFlags dstA);
  void fillUbo(uint8_t *dst);
  VkDescriptorSet newSet(VkDescriptorSetLayout lay);
  void writeWeaveSet(VkDescriptorSet set, VkImageView srcView, VkBuffer ubo);
  void writeHudSet(VkDescriptorSet set, VkImageView atlasView);

  VkDev *dev_ = nullptr;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkSwapchainKHR sc_ = VK_NULL_HANDLE;
  VkFormat fmt_ = VK_FORMAT_UNDEFINED;
  VkColorSpaceKHR cs_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  VkPresentModeKHR pm_ = VK_PRESENT_MODE_FIFO_KHR;
  VkExtent2D ext_{};
  std::vector<VkImage> imgs_;
  std::vector<VkImageView> views_;

  VkDescriptorSetLayout srcSetLay_ = VK_NULL_HANDLE, hudSetLay_ = VK_NULL_HANDLE;
  VkPipelineLayout weaveLay_ = VK_NULL_HANDLE, hudLay_ = VK_NULL_HANDLE;
  VkPipeline weavePipe_ = VK_NULL_HANDLE, hudPipe_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;

  std::vector<FrameSlot> slots_;
  size_t frameNo_ = 0;
  bool lost_ = false;

  std::vector<Keep> keep_; /* fuentes vivas: cada hueco de cuadro apunta a una generación */
  uint64_t gen_ = 0;
  VkImg next_{};           /* fuente nueva pendiente de entrar */
  VkImg black_{};          /* reserva por si aún no hay fuente */
  VkImg corrA_{};          /* neutros si no hay ficheros */
  VkImg corrB_{};
  VkImg atlas_{};
  std::vector<HudVertex> hud_;
  bool atlasDirty_ = false;

  LensField field_{};
  EyePose eye_{};
  WeaveTuning tune_{};
  std::vector<uint8_t> atlasPx_;
  int atlasW_ = 0, atlasH_ = 0;
  int hudState_ = 0;
  float depthShift_ = 0.0f;
  bool srcFlipY_ = false;
  VkCommandPool cmdPool_ = VK_NULL_HANDLE;
  int pw_ = 3840, ph_ = 2160;
  bool cornersDirty_ = true;
  uint8_t uboData_[256]{};  /* tamano real del bloque std140 de weave.frag */
};

#endif
