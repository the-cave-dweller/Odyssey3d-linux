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
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace mako {

/* la capa de MAKO esta en el equipo (su manifiesto de capa implicita) */
bool instalado();

/* ---- MAKO FG, el generador propio de MAKO ----
   Un programa aparte, mako-fg-host (19 MB, con el modelo V1.24 dentro), que la capa de MAKO arranca para cada juego.
   El paquete mako-render 5.0.0 de Rodolfo (compilado de las fuentes) no lo trae, y sin el MAKO dice «MAKO FG is not
   installed: mako-fg-host is missing» y presenta el juego tal cual.
   MEDIDO el 27-nov (su codigo, engine/mako-backend/src/mako_fg.cpp, y una sonda en las llamadas de vkcube con MAKO):
   lo busca en $MAKO_FG_HOST y, si no, solo junto a su biblioteca (/usr/lib/mako-fg/mako-fg-host y
   /lib64/mako-fg/mako-fg-host). El modelo va dentro del programa («source=release»); MAKO_FG_PACKAGE solo lo cambia
   por otro. Lo arranca sin argumentos y le habla por un socket (fd 3), con el entorno del juego.
   (v1.18) Rodolfo: «offer an install button». OdysseyVK lo instala sin root en ~/.local/share/odysseyvk/mako-fg/: la
   version V1.24 de Hugging Face (eugeniosegala/MAKO-FG, la que salio con MAKO 5.0.0 el 12-nov), fijada a su revision y
   con el SHA-256 de cada archivo comprobado. Se lo da a los juegos con MAKO_FG_HOST en su linea de Steam
   (Integraciones, junto a ENABLE_MAKO=1). Su licencia es CC BY-NC 4.0: uso no comercial. */
/* el de MAKO, junto a su biblioteca (donde lo pone su paquete oficial, mako-renderer-bin); vacio si no esta */
std::string hostSistema();
/* el que instalo OdysseyVK, si esta entero; vacio si no */
std::string hostPropio();
/* su carpeta: $XDG_DATA_HOME/odysseyvk/mako-fg */
std::string carpetaGenerador();
/* el MAKO_FG_HOST que hay que darles a los juegos: vacio si MAKO ya tiene el suyo (o no hay ninguno) */
std::string hostParaJuegos();
/* alguno: $MAKO_FG_HOST, el de MAKO o el de OdysseyVK */
bool generadorInstalado();
/* si el juego que corre puede generar con MAKO FG: su MAKO_FG_HOST (`hostDelJuego`, de /proc/PID/environ) lleva a un
   programa, o MAKO tiene el suyo */
bool generadorEnJuego(const std::string &hostDelJuego);

/* la instalacion: `odysseyvk mako instalar` la hace (bloqueando). El boton del menu de los FPS y el de Integraciones la
   arrancan aparte (lanzarInstalacion) y leen como va en ~/.local/state/odysseyvk/mako-fg (leerInstalacion) */
extern const char *const kVersionGenerador;   /* «1.24» */
/* lo que se baja: el programa, el modelo (que tambien va dentro del programa; aqui por si se quiere MAKO_FG_PACKAGE), su
   licencia y sus avisos */
struct ArchivoGenerador {
  const char *origen;    /* su ruta en la revision fijada */
  const char *destino;   /* y en carpetaGenerador() */
  const char *sha256;
  long bytes;
  bool ejecutable;
};
const std::vector<ArchivoGenerador> &archivosGenerador();   /* el primero es mako-fg-host */
long bytesGenerador();                                      /* todo junto: 19,3 MB */
std::string origenGenerador();   /* la URL de la revision (ODYSSEYVK_MAKO_FG_ORIGEN, para las pruebas: un file://) */
struct Instalacion {
  bool enMarcha = false;   /* hay una corriendo (su proceso vive) */
  bool lista = false;      /* la ultima acabo bien */
  long hecho = 0, total = 0;
  std::string error;       /* la ultima fallo: por que */
  long hora = 0;           /* cuando escribio por ultima vez (segundos desde 1970) */
  long pid = 0;            /* la que la escribio */
};
Instalacion leerInstalacion();
bool lanzarInstalacion(std::string *err);
/* (v1.18) `odysseyvk mako SUB` aparte (instalar, quitar, desinstalar), como la instalacion: para los botones de la pestana FG */
bool lanzarOrden(const char *sub, std::string *err);
/* la biblioteca de la capa de MAKO (la de su manifiesto), o vacio */
std::string biblioteca();
/* las dos de verdad (mako_instalar.cpp, solo en `odysseyvk`: el SHA-256 es de libcrypto): bajar cada archivo con curl a una
   carpeta aparte, comprobar su tamano y su SHA-256, y cambiarla por la de antes de una vez (a medias no queda nada puesto).
   `avance` (hecho, total), si se da, va diciendo como va */
bool instalarGenerador(std::string *err, const std::function<void(long, long)> &avance = {});
bool quitarGenerador(std::string *err);
/* (v1.18) el otro modelo de MAKO, `lossless-scaling` (LSFG), no necesita mako-fg-host: usa el Lossless.dll de Lossless Scaling
   de Steam. Lo de los documentos de MAKO (mako-fg.app, CONFIGURATION-REFERENCE): `frame_generation_model` es «mako-fg» (lo
   de fabrica) o «lossless-scaling»; `dll` en «Automatic» lo busca. Aqui: la `dll` de [global] si la hay, si no la carpeta
   de Lossless Scaling en cada biblioteca de Steam (libraryfolders.vdf). Vacio si no esta */
std::string dllLossless();
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
  std::string modelo;         /* frame_generation_model: «mako-fg» (de fabrica, si no lo dice) o «lossless-scaling» */
};
/* el perfil de un juego por su ejecutable («Raji-Win64-Shipping.exe»): el primero cuyo active_in lo nombre */
Perfil leer(const std::string &exe);
/* pone esas claves (clave de MAKO → su valor ya en TOML: «true», «3», «0.85», «'NVIDIA GeForce RTX 5080'») en el perfil
   del juego, o en uno nuevo «OdysseyVK · exe»; un valor vacio quita la clave (`gpu` vacia: la tarjeta del juego). Falso
   (y `err`) si no se pudo o no valida */
bool fijar(const std::string &exe, const std::map<std::string, std::string> &claves, std::string *err = nullptr);

}  // namespace mako
