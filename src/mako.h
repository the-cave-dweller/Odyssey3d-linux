#pragma once
/* mako.h — (v1.18) la generacion de cuadros de MAKO desde el cuadrito de los FPS (Rodolfo: «ahora que click derecho en el
   contador de fps muestre opciones de generacion de frames, haz lo mismo que vkdaedalus»).

   MAKO (mako-render, GPL-3.0, eugeniosegala.github.io/MAKO) es una capa de Vulkan que genera cuadros con el modelo de
   Lossless Scaling, como lsfg-vk. Se configura en ~/.config/mako-render/conf.toml (o $MAKO_CONFIG), un [[profile]] por
   juego que lo nombra en `active_in`, y lo vuelve a leer en marcha. vkDaedalus (un vkBasalt con menu) hace lo mismo desde
   su menu: escribe el perfil del juego en ese fichero («Set writes it into MAKO's file»). Aqui igual, sin su codigo: se
   toca el perfil que ya nombra al juego o se crea uno al final; lo demas del fichero (otros perfiles, comentarios) se
   deja como esta. Antes de la primera escritura se guarda una copia en ~/.local/state/odysseyvk/mako/, y lo nuevo se
   comprueba con `mako-cli validate` antes de cambiar el fichero (si no valida, no se toca).

   MEDIDO con el cargador de Vulkan (VK_LOADER_DEBUG=layer, 27-nov): la cadena del dispositivo es juego → OdysseyVK →
   MAKO → vkDaedalus → controlador. MAKO genera por debajo de la capa de OdysseyVK: la capa solo ve (y copia la
   profundidad de) los cuadros de verdad del juego, y los generados llegan a la pantalla y al visor por la captura. */
#include <map>
#include <string>
#include <vector>

namespace mako {

/* la capa de MAKO esta en el equipo (su manifiesto de capa implicita) */
bool instalado();
/* el fichero de configuracion: $MAKO_CONFIG o $XDG_CONFIG_HOME/mako-render/conf.toml */
std::string rutaConf();

struct Perfil {
  bool existe = false;        /* hay un [[profile]] que nombra al juego */
  std::string nombre;         /* su `name` */
  bool puesta = false;        /* frame_generation_enabled */
  int multiplicador = 2;      /* multiplier: 2 a 5 (lo que acepta MAKO) */
  double flujo = 0.85;        /* flow_scale: 0,25 a 1 */
  bool rendimiento = false;   /* performance_mode */
  std::string tarjeta;        /* gpu: el nombre de Vulkan de la tarjeta, o vacio (la del juego) */
};
/* el perfil de un juego por su ejecutable («Raji-Win64-Shipping.exe»): el primero cuyo active_in lo nombre */
Perfil leer(const std::string &exe);
/* pone esas claves (clave de MAKO → su valor ya en TOML: «true», «3», «0.85», «'NVIDIA GeForce RTX 5080'») en el perfil
   del juego, o en uno nuevo «OdysseyVK · exe»; un valor vacio quita la clave (`gpu` vacia: la tarjeta del juego). Falso
   (y `err`) si no se pudo o no valida */
bool fijar(const std::string &exe, const std::map<std::string, std::string> &claves, std::string *err = nullptr);
/* las tarjetas para la clave `gpu`, con el nombre que les da Vulkan («NVIDIA GeForce RTX 5080»; las NVIDIA del equipo) */
std::vector<std::string> tarjetas();

}  // namespace mako
