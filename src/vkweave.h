/* vkweave.h — el visor por Vulkan: intercambio de imágenes (swapchain), la pasada de trama
   con triángulo a pantalla completa y la pasada de la interfaz encima, todo con dibujado
   dinámico (Vulkan 1.3) y sin render pass. El fotograma de KWin se muestrea directo desde su
   DMA-BUF: no pasa por la CPU. */
#ifndef ODYSSEY_VKWEAVE_H
#define ODYSSEY_VKWEAVE_H

#include "frame.h"
#include "vkdev.h"
#include "efectos_vk.h"
#include "vkui.h"
#include "weave.h"
#include "util.h"

#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

/* Tamano del bloque std140 de weave.frag. Vive aqui para que el almacen en CPU
   (`uboData_`) y las copias al buffer de Vulkan midan lo MISMO: en v0.45 se anadio el
   octavo vec4 (las capas por profundidad) y el bloque paso de 288 a 320 bytes, pero el
   almacen siguio a 288; cada cuadro se escribian 32 bytes fuera y el visor se pintaba
   gris (los numeros de la trama y del contraste se pisaban). Los desfases de cada campo
   estan en kOff... dentro del .cpp, con un static_assert que avisa si crece mas. */
/* v0.62: noveno vec4 (tun8, el agrandado guiado del mapa): 320 -> 336.
   v0.67: decimo vec4 (tun9, el paralaje de movimiento): 336 -> 352.
   v0.83: tun10 (video dividido suavizado): 352 -> 368. v0.90: tun11 (la franja de la barra): 368 -> 384. */
constexpr size_t kUboBytes = 384;

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
  /* (v1.18) la generacion de cuadros de los videos (fgvideo.h): mientras haya una vista aqui, la trama dibuja esa (un
     cuadro generado, en GENERAL) en lugar de la fuente real; VK_NULL_HANDLE = la real. La vista es de quien la pone */
  void setFuenteGenerada(VkImageView v) { fuenteGenerada_ = v; }
  /* (v1.18) los efectos de ReShade sobre la fuente, antes de la trama (efectos_vk.h): el visor le da los ajustes del juego */
  CadenaEfectos &efectos() { return efectos_; }
  /* (v1.18) los efectos en la trama, o solo en la vista de los efectos (la pestana Efectos abierta con ellos apagados) */
  void efectosEnPantalla(bool si) { efectosEnPantalla_ = si; }
  /* (v1.18) la vista previa de los efectos (Rodolfo: «show a preview square for the game with the effect applied on the
     half of the screen shown», «can you find a way to make the preview fast?, maybe not cpu based», y luego «distribuyelo
     como en la vista de depuracion»): con su casilla palomeada el panel queda en una columna a la izquierda y el visor
     pinta en cada cuadro el juego en (x, y, ancho, alto), a la izquierda de la raya `corte` sin efectos y a la derecha con
     ellos: copias en la tarjeta, sin pasar por la CPU. Todo en fracciones de la pantalla; ancho 0 = ninguna. Si tapa la
     pantalla entera, la trama de ese cuadro no se pinta */
  void ponerVista(float x, float y, float w, float h, float corte) {
    vista_[0] = x, vista_[1] = y, vista_[2] = w, vista_[3] = h, vista_[4] = corte;
  }
  /* la fuente real mas nueva (la que llego y aun no paso por frame(), o la ultima que paso) */
  const VkImg *fuenteReal() const;

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
  /* cuanto se ha movido la cabeza, en medias distancias entre ojos y ya con ganancia y tope (v0.67) */
  void setParalaje(float h) { paralaje_ = h < -1.0f ? -1.0f : (h > 1.0f ? 1.0f : h); }
  /* profundidad del sujeto en el mapa (0..1), para el realce de detalle; < 0 = usar el plano de apoyo (v0.68) */
  void setSujeto(float d) { sujeto_ = d; }
  /* Video dividido: las filas de arriba y de abajo (fraccion del alto) que se tejen en 2D con el
     cuadro entero, porque ahi el reproductor pinto su barra a lo ancho (ver barra.h). 0 = ninguna. */
  void setFilas2D(float arriba, float abajo) { filasArriba_ = arriba, filasAbajo_ = abajo; }
  /* `barra_2d = entera`: esas filas, enteras y tal cual (la franja de la v0.81), sin la limpieza de la v0.90 */
  void setFranjaEntera(bool e) { franjaEntera_ = e; }
  /* Video dividido suavizado (v0.83): la fuente trae dos imagenes lado a lado y el conversor trabaja
     solo con la izquierda, estirada al ancho entero. Al cambiarlo hay que tirar el mapa de
     profundidad que hubiera (`dropDepthMap`): era de otra imagen. */
  void setMediaFuente(bool m) { mediaFuente_ = m; }
  bool mediaFuente() const { return mediaFuente_; }
  void dropDepthMap() { depthUsable_ = false; }
  /* Cantos finos (v0.89): el mapa se sube tal cual sale de la red (sin repartirlo por ojo en la CPU)
     y el sombreador busca la vista de cada ojo punto a punto. Ver buscaVista en weave.frag. */
  void setCantosFinos(bool c) { cantosFinos_ = c; }
  bool cantosFinos() const { return cantosFinos_; }
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
  /* fuente -> RGBA sz x sz. De normal sale lo que ve el conversor (con `setMediaFuente`, solo la
     imagen izquierda); `entera` = el cuadro entero pase lo que pase, que es lo que necesita el
     detector de la barra del reproductor para comparar las dos mitades. */
  bool readSmall(std::vector<uint8_t> &rgba, std::string *err, bool entera = false);
  bool setDepthMap(const uint8_t *g, int w, int h, std::string *err);
  /* lo mismo, con el mapa ya repartido por ojo (dos bytes por punto: izquierdo, derecho) */
  bool setDepthMapRG(const uint8_t *rg, int w, int h, std::string *err);
  /* cantos finos (v0.89): el mapa tal cual sale de la red y, con el, el color de la imagen que
     lo dio (cuatro bytes por punto: profundidad, rojo, verde, azul). Ver afinar() en weave.frag. */
  bool setDepthMapGuia(const uint8_t *rgba, int w, int h, std::string *err);
  /* La vista de depuracion de la profundidad (v1.12): con ella puesta, en vez de las dos vistas se ve
     plana la profundidad que usa la trama (gris, blanco = cerca), encima la saliencia en naranja y en
     cian lo que cae en el plano de la pantalla. `setSaliencia` sube la rejilla de DepthNet
     (rejillaSaliencia, lado x lado, R8). */
  void setVerProfundidad(bool v) { verProfundidad_ = v; }
  bool verProfundidad() const { return verProfundidad_; }
  bool setSaliencia(const uint8_t *s, int lado, std::string *err);
  /* cuantos pases de afinado se han dibujado y cuantos se ahorraron por no haber cambiado nada */
  void cuentaAfinado(long *hechos, long *ahorrados) const { *hechos = finoHechos_, *ahorrados = finoAhorrados_; }
  bool hasDepthMap() const { return depthUsable_; }
  void setConverter(float gain, float ref, bool on) { convGain_ = gain, convRef_ = ref, convOn_ = on; }
  /* para el volcado: poder apagar el releve un momento y volverlo a dejar como estaba */
  bool converterOn() const { return convOn_; }
  float converterGain() const { return convGain_; }
  float converterRef() const { return convRef_; }
  /* El temblor del desplazamiento, el ancho del puente del canto y el temblor de la fase venian de
     una variable de entorno leida UNA sola vez al arrancar (ODYSSEYVK_DITHER / _CANTO /
     _FASE_TEMBLOR). Ahora son miembros, para que la ventana de desarrollo pueda moverlos con el
     visor abierto; el valor de arranque es exactamente el de siempre. */
  void setTemblor(float px) { temblorPx_ = px; }
  void setCanto(float v) { cantoS_ = v > 0.0f ? v : 0.004f; }
  void setFaseTemblor(float v) { faseTemblor_ = v > 0.0f && v < 1.0f ? v : 0.0f; }
  float temblor() const { return temblorPx_; }
  float canto() const { return cantoS_; }
  float faseTemblor() const { return faseTemblor_; }
  /* Lo mismo con el paso de la rejilla escrita: `rampa_x`/`rampa_y` de la configuracion eran una
     variable de entorno leida una vez, y ahora se pueden mover con la ventana abierta. Se suman a
     lo que llevan las teclas `[` `]` y `;` ' (tune_.rampaDX/rampaDY), y el total sigue topado. */
  void setRampaBase(bool ejeX, float v) {
    const float t = (v > -0.05f && v < 0.05f) ? v : (v >= 0.05f ? 0.05f : -0.05f);
    if (ejeX) rampaBaseX_ = t; else rampaBaseY_ = t;
  }
  float rampaBase(bool ejeX) const { return ejeX ? rampaBaseX_ : rampaBaseY_; }

  /* un cuadro completo: adquirir, dibujar trama + interfaz, presentar */
  bool frame(std::string *err);
  /* Donde se va el tiempo de presentar (rama experimental, tirones): en la CPU, esperar a que la
     tarjeta acabe el cuadro anterior de este hueco (valla), esperar una imagen libre del
     intercambio (adquirir) y entregar (presentar); y en la TARJETA, lo que tardo de verdad nuestro
     cuadro (marcas de tiempo al principio y al final de su lista de mandos). Maximos y medias desde
     la ultima llamada, en ms; la llamada los pone a cero. */
  struct TiemposCuadro {
    double vallaMax = 0, adquirirMax = 0, presentarMax = 0, gpuMax = 0, gpuMedia = 0;
    long n = 0, gpuN = 0;
  };
  TiemposCuadro tomaTiempos();
  /* agranda los topes por ojo (+12,67 / -9,98 px) por ese factor: el modo rendimiento sin red exagera
     el 3D, y sin agrandar los topes el relieve de mas se aplastaba contra ellos (rama experimental) */
  void setTopeEscala(float f) { topeEscala_ = f < 1.0f ? 1.0f : (f > 4.0f ? 4.0f : f); }
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
    /* el pase de afinado del mapa (v0.89) lleva sus propios enlaces y parametros */
    VkDescriptorSet weavePre = VK_NULL_HANDLE;
    VkBuffer uboPre = VK_NULL_HANDLE;
    VkDeviceMemory uboPreMem = VK_NULL_HANDLE;
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
  void writeWeaveSet(VkDescriptorSet set, VkImageView srcView, VkBuffer ubo, VkImageView finoView = VK_NULL_HANDLE);
  /* El pase de afinado (v0.89): antes de la trama se dibuja, una vez por pixel, la profundidad
     de la fuente con el canto en su sitio (afinar() en weave.frag) en una imagen del tamano de
     la pantalla; la trama la lee de ahi al buscar la vista de cada ojo. */
  bool asegurarFino(int w, int h);
  void pasoFino(VkCommandBuffer cb, VkDescriptorSet set);
  void uboDelAfinado(uint8_t *dst) const;
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
  VkQueryPool marcas_ = VK_NULL_HANDLE;      /* dos marcas de tiempo por hueco */
  std::vector<bool> marcasPuestas_;
  double periodoMarcaNs_ = 0.0;              /* ns por tic del reloj de la tarjeta (0 = sin marcas) */
  TiemposCuadro tiempos_{};
  double gpuSuma_ = 0.0;
  size_t frameNo_ = 0;
  bool lost_ = false;

  std::vector<Keep> keep_; /* fuentes vivas: cada hueco de cuadro apunta a una generación */
  uint64_t gen_ = 0;
  VkImg next_{};           /* fuente nueva pendiente de entrar */
  VkImageView fuenteGenerada_ = VK_NULL_HANDLE;   /* (v1.18) */
  CadenaEfectos efectos_;                          /* (v1.18) */
  int efectosGen_ = -1;                            /* la fuente que ya paso por los efectos */
  bool efectosEnPantalla_ = true;                  /* (v1.18) */
  /* (v1.18) la vista de los efectos: el hueco del panel, si el intercambio deja copiar en sus imagenes, y el blanco de la raya */
  float vista_[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  bool swapCopiable_ = false;
  VkImg blanco_{};
  void pintarVista(VkCommandBuffer cb, VkImage destino, VkExtent2D lienzo, const VkImg &fuente, VkImage conEfectos);
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
  int depthCanales_ = 2;   /* 2 = un canal por ojo · 4 = profundidad + el color de la imagen que la dio */
  VkImg fino_{};           /* la profundidad afinada, a tamano de pantalla (v0.89) */
  int finoW_ = 0, finoH_ = 0;
  bool finoEnUso_ = false; /* este cuadro se dibuja con el pase de afinado */
  bool finoAvisado_ = false;
  uint64_t mapaGen_ = 0;   /* sube con cada mapa nuevo */
  std::vector<uint8_t> finoFirma_;   /* de que se dibujo el afinado que hay en la imagen */
  long finoHechos_ = 0, finoAhorrados_ = 0;
  std::vector<uint8_t> mapRG_;   /* el mapa ya partido por ojo, antes de subirlo */
  bool verProfundidad_ = false;  /* ver setVerProfundidad */
  VkImg sal_{};                  /* la rejilla de la saliencia (R8), solo con la vista de depuracion */
  int salLado_ = 0;
  VkBuffer salStage_ = VK_NULL_HANDLE;
  VkDeviceMemory salStageMem_ = VK_NULL_HANDLE;
  bool subirMapaRG(const uint8_t *rg, int w, int h, std::string *err, int canales = 2);   /* la subida a la tarjeta, sin mas */
  double msHoyos_ = 0.0;         /* lo que tarda rellenar los huecos (medible con ODYSSEYVK_MEDIR=1) */
  long mapasContados_ = 0;
  bool depthUsable_ = false;
  int smallW_ = 0, smallH_ = 0;
  VkBuffer mapStage_ = VK_NULL_HANDLE;    /* por aqui sube el mapa de profundidad, sin crear y
                                             borrar una imagen cada cuadro */
  VkDeviceMemory mapStageMem_ = VK_NULL_HANDLE;
  int depthW_ = 0, depthH_ = 0;
  float convGain_ = 0.0f, convRef_ = 0.5f;
  bool convOn_ = false;
  /* Leidos del entorno una sola vez, al armar el objeto: son los valores de siempre y siguen
     pudiendo forzarse con ODYSSEYVK_DITHER / _CANTO / _FASE_TEMBLOR. Ahora son miembros porque la
     ventana de desarrollo los mueve con el visor abierto (ver setTemblor mas arriba). */
  float temblorPx_ = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_DITHER");
    return e ? (float)std::atof(e) : 0.25f;   /* 0,01 de dither_strength sobre los 25 px de recorrido */
  }();
  float cantoS_ = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_CANTO");
    const float v = e ? (float)std::atof(e) : 0.004f;
    return v > 0.0f ? v : 0.004f;
  }();
  float faseTemblor_ = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_FASE_TEMBLOR");
    const float v = e ? (float)std::atof(e) : 0.0f;
    return v > 0.0f && v < 1.0f ? v : 0.0f;
  }();
  float rampaBaseX_ = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_RAMPA_X");
    const float v = e ? (float)std::atof(e) : 0.0f;
    return (v > -0.05f && v < 0.05f) ? v : 0.0f;
  }();
  float rampaBaseY_ = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_RAMPA_Y");
    const float v = e ? (float)std::atof(e) : 0.0f;
    return (v > -0.05f && v < 0.05f) ? v : 0.0f;
  }();
  float topeEscala_ = 1.0f;   /* los topes por ojo, agrandados (modo rendimiento sin red: exagerar el 3D) */
  bool conTope_ = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_TOPE");
    return !e || e[0] != '0';
  }();
  std::vector<HudVertex> hud_;
  bool atlasDirty_ = false;

  LensField field_{};
  EyePose eye_{};
  WeaveTuning tune_{};
  std::vector<uint8_t> atlasPx_;
  int atlasW_ = 0, atlasH_ = 0;
  int hudState_ = 0;
  float depthShift_ = 0.0f;
  float paralaje_ = 0.0f;
  float sujeto_ = -1.0f;
  float filasArriba_ = 0.0f, filasAbajo_ = 0.0f;
  bool franjaEntera_ = false;
  bool mediaFuente_ = false;
  bool cantosFinos_ = false;
  bool srcFlipY_ = false;
  VkCommandPool cmdPool_ = VK_NULL_HANDLE;
  int pw_ = 3840, ph_ = 2160;
  bool cornersDirty_ = true;
  uint8_t uboData_[kUboBytes]{};  /* tamano del bloque std140 de weave.frag (kUboBytes arriba) */
};

#endif
