#pragma once
/* efectos_vk.h — (v1.18) la cadena de efectos de ReShade en la tarjeta: lo que corre en el visor sobre cada cuadro de un
   juego, antes del 3D (ver efectos.h).

   Como ReShade, sin su runtime: cada efecto compilado (efectos::compilar) da sus texturas, sus muestreadores, su bufer de
   uniformes y sus pases. Aqui:
     - la fuente (la captura del juego, la imagen de cada cuadro) se copia a una imagen propia RGBA8, A; el «back buffer»
       de ReShade son A y B a turnos: cada pase que dibuja en el back buffer lee la una (la textura COLOR) y escribe la
       otra, y la salida es la ultima escrita. Todas en VK_IMAGE_LAYOUT_GENERAL, como las del visor;
     - la profundidad (la textura DEPTH de ReShade.fxh) es el mapa del 3D (R8G8, 255 = cerca) copiado a una imagen propia
       de tamano fijo y 16 bits; efectos.cpp define RESHADE_DEPTH_INPUT_IS_REVERSED y el plano lejano en 1 para que
       ReShade::GetLinearizedDepth de 0 cerca y 1 lejos;
     - los uniformes van en el bufer del comando (vkCmdUpdateBuffer): cambian en cada cuadro sin pisar uno en vuelo;
     - se compila en otro hilo: mientras tanto el visor sigue con la cadena de antes (o sin efectos).
   Lo que no hay: pases de computo, texturas 3D y la plantilla (stencil, que en SMAA solo ahorra trabajo). Un efecto con
   algo de eso se salta y lo dice el registro. */
#include "efectos.h"

#include <vulkan/vulkan.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class VkDev;

class CadenaEfectos {
public:
  CadenaEfectos() = default;
  ~CadenaEfectos();
  CadenaEfectos(const CadenaEfectos &) = delete;
  CadenaEfectos &operator=(const CadenaEfectos &) = delete;

  void iniciar(VkDev *dev) { dev_ = dev; }
  void destruir();
  /* los ajustes que valen ahora (los de un juego, o nada: Ajustes{} sin puestos) */
  void ajustar(const efectos::Ajustes &a);
  /* antes de grabar el cuadro: con la fuente de w x h, ¿hay cadena lista? Recoge lo compilado y crea lo de la tarjeta
     (espera a que la tarjeta acabe si cambia la cadena). Deja en *salida la imagen que hay que mostrar */
  bool preparar(int w, int h, VkImageView *salida);
  /* hay que volver a pasarla aunque la fuente no haya cambiado (un ajuste nuevo, o un efecto con tiempo) */
  bool sucia() const { return sucia_; }
  /* graba la cadena en cb: de `fuente` (en GENERAL) a la salida. `prof` (R8G8, GENERAL) puede ser nula */
  void grabar(VkCommandBuffer cb, VkImage fuente, int w, int h, VkImage prof, int pw, int ph);
  /* la vista de la salida de la ultima pasada (vacia sin cadena): las capturas del visor la usan, como la pantalla */
  VkImageView vistaSalida() const { return cadena_.empty() ? VK_NULL_HANDLE : (salidaEnB_ ? b_.todo[0] : a_.todo[0]); }
  /* la imagen de la salida (para la prueba, que la lee) */
  VkImage imagenSalida() const { return salidaEnB_ ? b_.img : a_.img; }
  /* para el registro: lo que corre y lo que no */
  std::string resumen() const;

private:
  struct Img {
    VkImage img = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkFormat fmt = VK_FORMAT_UNDEFINED;
    uint32_t w = 0, h = 0, niveles = 1;
    VkImageView todo[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};   /* para muestrear: [0] UNORM, [1] SRGB */
    VkImageView base[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};   /* el nivel 0, para dibujar */
  };
  struct Pase {
    VkPipeline pipe = VK_NULL_HANDLE;
    std::vector<int> destinos;   /* indices en texs del efecto; vacio = el back buffer */
    bool srgb = false, cargar = false, limpiar = false, copiarAntes = false, mipmaps = false;
    uint32_t vertices = 3, vw = 0, vh = 0;
  };
  struct Efecto {
    std::string nombre;
    std::shared_ptr<efectos::Compilado> comp;
    std::vector<Img> texs;            /* las del .fx (las COLOR y DEPTH no tienen imagen propia) */
    std::vector<int> texDe;           /* por textura del modulo: indice en texs, -1 COLOR, -2 DEPTH */
    std::vector<VkSampler> muestreadores;
    std::vector<VkShaderModule> modulos;
    VkDescriptorSetLayout dslUbo = VK_NULL_HANDLE, dslMuestras = VK_NULL_HANDLE;
    VkPipelineLayout lay = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet setUbo = VK_NULL_HANDLE, setMuestras[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};   /* COLOR = A o B */
    VkBuffer ubo = VK_NULL_HANDLE;
    VkDeviceMemory uboMem = VK_NULL_HANDLE;
    std::vector<uint8_t> datos;       /* el bufer de uniformes, en la CPU */
    std::vector<Pase> pases;
    bool conTiempo = false;           /* algun uniforme con source (timer, frametime...) */
  };
  struct Pedido {   /* lo que compila el otro hilo */
    std::string clave;
    int w = 0, h = 0;
    std::vector<efectos::Efecto> lista;
    std::vector<std::shared_ptr<efectos::Compilado>> hechos;
    std::vector<std::string> fallos;
    std::atomic<bool> listo{false};
  };

  bool crearImg(Img &i, uint32_t w, uint32_t h, VkFormat f, uint32_t niveles, VkImageUsageFlags uso, std::string *err);
  void borrarImg(Img &i);
  uint32_t tipoMemoria(uint32_t bits, VkMemoryPropertyFlags props) const;
  bool construir(Pedido &p, std::string *err);
  bool construirEfecto(Efecto &e, const efectos::Efecto &aj, std::string *err);
  void borrarEfecto(Efecto &e);
  void borrarCadena();
  void llenarValores(Efecto &e, const efectos::Efecto &aj);
  void llenarFuentes(Efecto &e);
  bool subirTextura(Img &i, const std::string &ruta, std::string *err);
  void mipmaps(VkCommandBuffer cb, Img &i);
  std::string claveDe(int w, int h) const;

  VkDev *dev_ = nullptr;
  efectos::Ajustes ajustes_;
  std::vector<Efecto> cadena_;
  std::vector<efectos::Efecto> ajustesCadena_;   /* los de cada efecto de la cadena, en su orden */
  Img a_, b_, prof_;
  int w_ = 0, h_ = 0;
  std::string claveHecha_;
  std::shared_ptr<Pedido> pedido_;
  std::vector<std::string> fallos_;
  bool sucia_ = false;
  bool salidaEnB_ = false;
  bool profPuesta_ = false;
  uint64_t cuadros_ = 0;
  double t0_ = -1.0, tAnterior_ = -1.0;
  VkPhysicalDeviceMemoryProperties memProps_{};
  bool memLeida_ = false;
};
