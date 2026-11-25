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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/* Tamano del bloque std140 de weave.frag. Vive aqui para que el almacen en CPU
   (`uboData_`) y las copias al buffer de Vulkan midan lo MISMO: en v0.45 se anadio el
   octavo vec4 (las capas por profundidad) y el bloque paso de 288 a 320 bytes, pero el
   almacen siguio a 288; cada cuadro se escribian 32 bytes fuera y el visor se pintaba
   gris (los numeros de la trama y del contraste se pisaban). Los desfases de cada campo
   estan en kOff... dentro del .cpp, con un static_assert que avisa si crece mas. */
constexpr size_t kUboBytes = 320;

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
  void setEye(const EyePose &e) { eye_ = e; cornersDirty_ = true; }
  const EyePose &eye() const { return eye_; }
  const LensField &field() const { return field_; }
  void setPanelPixels(int w, int h) { pw_ = w, ph_ = h, cornersDirty_ = true; }
  WeaveTuning &tuning() { return tune_; }
  void setAtlas(const std::vector<uint8_t> &px, int w, int h);
  void setHud(const std::vector<HudVertex> &v) { hud_ = v; }
  void setHudState(int st) { hudState_ = st; }
  void setDepthShift(float d) { depthShift_ = d; }
  void setSrcFlipY(bool f) { srcFlipY_ = f; }

  /* cuadro fuera de pantalla: dibuja trama + interfaz en una imagen propia y la baja a la CPU
     (para --dump, sin tocar el panel). Devuelve RGBA con la primera fila arriba.
     `copiaPura` dibuja la misma imagen SIN tramar (el pase de copia del sombreador): sirve para
     comparar y saber si un cuadro plano viene de la fuente o de los calculos de la trama. */
  bool grab(std::vector<uint8_t> &rgbaTopFirst, std::string *err, bool copiaPura = false);

  /* --- conversor de video normal a 3D -------------------------------------------------
     El modelo de profundidad necesita una copia pequena de la imagen (se reduce en el propio
     GPU: 1 MB en lugar de los 33 MB del cuadro completo) y devuelve un mapa de 8 bits por punto
     que el shader de la trama usa para desplazar la vista de cada ojo. */
  bool prepareConverter(int sz, std::string *err);      /* crear la copia pequena y el mapa */
  bool converterReady() const { return small_.valid(); }
  int converterSize() const { return smallW_; }
  bool readSmall(std::vector<uint8_t> &rgba, std::string *err);   /* fuente -> RGBA sz x sz */
  bool setDepthMap(const uint8_t *g, int w, int h, std::string *err);
  bool hasDepthMap() const { return depthUsable_; }
  void setConverter(float gain, float ref, bool on) { convGain_ = gain, convRef_ = ref, convOn_ = on; }
  /* para el volcado: poder apagar el releve un momento y volverlo a dejar como estaba */
  bool converterOn() const { return convOn_; }
  float converterGain() const { return convGain_; }
  float converterRef() const { return convRef_; }

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
  /* canales=2 es el mapa de la profundidad, que lleva un canal por ojo (R8G8: .r izquierdo,
     .g derecho). Cada ojo ve el mapa ya puesto en su sitio de destino y con el hueco de detras
     del canto relleno; ver src/hoyos.cpp. */
  bool uploadR8(const uint8_t *px, int w, int h, VkImg &out, std::string *err,
                VkFormat fmt = VK_FORMAT_R8_UNORM, int canales = 1);
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
  VkImg small_{};          /* copia pequena de la fuente, para el modelo de profundidad */
  VkBuffer smallStage_ = VK_NULL_HANDLE;   /* por donde baja la copia a la CPU */
  VkDeviceMemory smallStageMem_ = VK_NULL_HANDLE;
  VkBuffer smallUbo_ = VK_NULL_HANDLE;     /* parametros fijos del pase pequeno */
  VkDeviceMemory smallUboMem_ = VK_NULL_HANDLE;
  VkDescriptorSet smallSet_ = VK_NULL_HANDLE;
  VkImg depth_{};          /* mapa de profundidad (R8G8: un canal por ojo) que lee el shader */
  std::vector<uint8_t> mapRG_;   /* el mapa ya partido por ojo, antes de subirlo */
  double msHoyos_ = 0.0;         /* lo que tarda rellenar los huecos (medible con ODYSSEY3D_MEDIR=1) */
  long mapasContados_ = 0;
  bool depthUsable_ = false;
  int smallW_ = 0, smallH_ = 0;
  VkBuffer mapStage_ = VK_NULL_HANDLE;    /* por aqui sube el mapa de profundidad, sin crear y
                                             borrar una imagen cada cuadro */
  VkDeviceMemory mapStageMem_ = VK_NULL_HANDLE;
  int depthW_ = 0, depthH_ = 0;
  float convGain_ = 0.0f, convRef_ = 0.5f;
  bool convOn_ = false;
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
  uint8_t uboData_[kUboBytes]{};  /* tamano del bloque std140 de weave.frag (kUboBytes arriba) */
};

#endif
