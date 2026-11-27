#pragma once
/* fgvideo.h — (v1.18) la generacion de cuadros para los VIDEOS (no los juegos), dentro del visor.
   Rodolfo: «y no puedes poner una capa de vulkan entre los videos (no juegos) y el 3d donde se puedan generar?» y
   «todo esto puede no afectar a como lo hace con los juegos?, digo, ser independiente».

   Una capa de Vulkan solo vive dentro del programa que dibuja (el juego, mpv): entre la ventana del video y el visor no
   hay ninguna. Asi que aqui el visor habla con mako-fg-host (MAKO FG, el que instala `odysseyvk mako instalar`) por su
   protocolo 3, el mismo que usa la capa de MAKO (engine/mako-backend/src/mako_fg.cpp):
     - Hello (protocolo 3), Devices (la tarjeta del visor, por su deviceUUID) y Open con 2 imagenes de entrada y N de
       salida RGBA8 optimas, de memoria dedicada exportada (fd opaco), y un semaforo de linea de tiempo exportado
     - cada cuadro real del video se copia a la entrada b%2 y señala v; Schedule con los tiempos i/(N+1); el host señala
       v+1..v+N al acabar cada generado; el siguiente real es v+N+1
   MEDIDO el 27-nov con un clip de 24 fps (tools/prueba_fg_video.cpp): 1,7 ms por cuadro a 1080p y 2,3 ms a 4K en la 4090.

   Los generados se copian a un anillo propio (el host vuelve a escribir sus salidas en el lote siguiente, y el visor
   puede tener cuadros en vuelo que aun los leen) y se ensenan repartidos en el intervalo del video: con x2, el generado
   entre el real anterior y el nuevo en cuanto llega el nuevo, y el nuevo medio intervalo despues. El video va por eso
   N/(N+1) de intervalo por detras (21 ms con 24 fps y x2).

   Independiente de los juegos: solo se abre con el 3D sobre algo que no es un juego; su mako-fg-host es otro que el de
   cada juego; nada va a la linea de Steam ni a la capa. Si algo falla, se apaga para la sesion y el 3D sigue con los
   cuadros reales. */
#include "vkdev.h"

#include <string>
#include <vector>

class FgVideo {
public:
  FgVideo() = default;
  ~FgVideo() { cerrar(); }
  FgVideo(const FgVideo &) = delete;
  FgVideo &operator=(const FgVideo &) = delete;

  /* abre (o deja como esta si ya vale) el host y las imagenes para cuadros de w x h con este multiplicador (2 a 4) */
  /* `formato`: el de la fuente (B8G8R8A8 la captura, R8G8B8A8 la subida por la CPU): el anillo lo lleva igual, porque los
     bytes pasan tal cual por las imagenes RGBA8 del host */
  bool preparar(VkDev &dev, int w, int h, int mult, VkFormat formato, std::string *err);
  void cerrar();
  bool abierto() const { return ctx_ >= 0; }
  int multiplicador() const { return (int)n_ + 1; }

  /* un cuadro real nuevo (la imagen importada de la captura, en GENERAL): se copia a la entrada, se piden los generados y
     se planean sus momentos. Falso si el host fallo (entonces se cierra) */
  bool nuevoReal(const VkImg &src, double ahora, std::string *err);
  /* lo que hay que dibujar ahora: la vista de un generado, o VK_NULL_HANDLE (el real). `cambio` = otra que la anterior */
  VkImageView vista(double ahora, bool *cambio);
  /* cuantos se han ensenado (para el registro y el estado) */
  long reales() const { return reales_; }
  long generados() const { return generadosVistos_; }

private:
  struct Img {
    VkImage img = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
  };
  bool crearExportada(Img &im, VkImageUsageFlags uso, int *fd, std::string *err);
  bool crearAnillo(Img &im, std::string *err);
  void destruir(Img &im);
  bool enviar(size_t k, uint64_t espera, uint64_t senal, std::string *err);   /* el buffer k de cbs_, con su valla */

  VkDev *dev_ = nullptr;
  int w_ = 0, h_ = 0;
  VkFormat formato_ = VK_FORMAT_B8G8R8A8_UNORM;
  uint32_t n_ = 0;   /* generados por real */
  int control_ = -1, ctx_ = -1;
  std::vector<Img> fuente_, destino_, anillo_;
  VkSemaphore tl_ = VK_NULL_HANDLE;
  VkCommandPool pool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> cbs_;
  std::vector<VkFence> vallas_;
  size_t cbSig_ = 0;
  uint64_t valor_ = 1, ultimoFin_ = 0;
  long lote_ = 0;
  size_t anilloSig_ = 0;
  /* el plan: cuando ensenar cada generado del ultimo lote, y luego el real */
  std::vector<std::pair<double, VkImageView>> plan_;
  VkImageView ultimaVista_ = VK_NULL_HANDLE;
  double ultimoReal_ = -1.0, intervalo_ = 1.0 / 24.0;
  long reales_ = 0, generadosVistos_ = 0;
};
