/* integraciones.h — la pestana Integraciones (v1.15): lo que va en las opciones de lanzamiento de Steam de cada
   juego, puesto por piezas (las capas de Vulkan, un comando delante, gamescope) en vez de escrito a mano.

   Rodolfo: «si suelo ejecutar otras capas de parametros de steam puedes tener una pestaña de Integrations?, quiero
   que tenga parametros de lanzamiento de steam que se puedan activar y desactivar para todo» y «quiero que cada
   juego tenga su herramientita para personalizar esto»; y al preguntarle como llevarlo: «Write the whole line to
   Steam».

   Lo de todos los juegos y lo propio de cada uno viven en ~/.config/odysseyvk/integraciones.conf: cada juego toma
   lo de todos salvo lo que tenga suyo. La linea entera se escribe en el localconfig.vdf de Steam, que Steam solo
   lee al arrancar y reescribe al salir: con Steam abierto queda pendiente y el servicio la escribe cuando se
   cierra (antes guarda una copia del fichero). Lo que se cambio por fuera (a mano en Steam, o Neural Forge) se
   toma como propio del juego: nada se pierde. Una linea que no se sabe leer (con `;`, `&&`, tuberias...) se
   deja como esta. */
#ifndef ODYSSEY_INTEGRACIONES_H
#define ODYSSEY_INTEGRACIONES_H

#include <map>
#include <string>
#include <vector>

namespace integ {

using Ajustes = std::map<std::string, std::string>;

/* las claves, en el orden de la pestana:
     odysseyvk mangohud vkdaedalus neural_forge vkbasalt   las variables (1 = puesta, 0 = no)
     nf_offload                                           (v1.18) el modelo de Neural Forge en la segunda tarjeta:
                                                          NEURAL_FORGE_OFFLOAD_GPU (ver secundariaNf)
     otros                                                otras variables, tal cual: «DXVK_HUD=fps PROTON_LOG=1»
     vkdaedalus_run comando                               delante del juego: vkdaedalus-run y uno a mano
     gamescope reescalado filtro render salida escala     gamescope: filtro fsr|nis (-F), render y salida «AxB»
     pantalla_completa cursor gs_otros                    (-w -h, -W -H; "" = sin poner), escala -S, -f, el cursor
     opciones                                             detras de %command%: «--launcher-skip» */
const std::vector<std::string> &claves();
/* lo de fabrica de cada clave (todo apagado; gamescope, si se pone, a 2048x1536 sobre 3840x2160 estirado) */
const Ajustes &deFabrica();
bool esInterruptor(const std::string &clave);

/* las resoluciones que se eligen por su abreviatura (Rodolfo: «solo las abreviaturas de cada una, no ambas
   medidas»): HD, HD+, FHD, QXGA, QHD, 4K */
struct Resolucion { const char *abrev, *medida; };
const std::vector<Resolucion> &resoluciones();

/* lo que esta instalado en el equipo, por clave (las demas claves, siempre): una capa, si algun manifiesto de capa
   implicita de Vulkan se enciende con su variable (enable_environment, donde la busca el cargador de Vulkan);
   vkdaedalus_run y gamescope (y el reescalado), si su comando esta en el PATH. Se vuelve a mirar solo si cambian
   esas carpetas (Rodolfo: «que esten en gris todos los que no esten instalados en el equipo») */
bool instalado(const std::string &clave);
/* ENABLE_VKDAEDALUS y vkdaedalus-run son uno u otro (Rodolfo: «que enable_vkdaedalus no permita habilitar
   vkdaedalus-run y viceversa»): la clave que no se puede poner con esta, o "" */
const char *excluyente(const std::string &clave);

/* ---- el estado: integraciones.conf ---- */
struct Juego {
  Ajustes propios;          /* lo que este juego tiene distinto de lo de todos */
  bool visto = false;       /* ya se leyo su linea de Steam alguna vez */
  std::string lineaSteam;   /* la ultima que se vio (o escribio) en Steam: si cambia, se cambio por fuera */
  bool propia = false;      /* su linea no se sabe leer: se deja como esta */
  std::string nfExe;        /* el NEURAL_FORGE_TARGET_EXE que traia su linea (si Neural Forge no lo da) */
  std::string nfGpu;        /* (v1.18) el NEURAL_FORGE_OFFLOAD_GPU que traia su linea, si no es el de secundariaNf */
};
struct Estado {
  Ajustes todos;            /* completo: lo de fabrica con lo cambiado encima */
  std::map<std::string, Juego> juegos;   /* por appid */
};
std::string rutaConf();
Estado cargar();
bool guardar(const Estado &e, std::string *err = nullptr);
/* lo que vale para un juego: lo de todos con lo suyo encima */
Ajustes efectivo(const Estado &e, const std::string &appid);
/* cambia una clave en todos (appid vacio) o en un juego: si queda igual que lo de todos, deja de ser propia.
   El reescalado enciende gamescope; apagar gamescope apaga el reescalado (Rodolfo: «la casilla de reescalado
   Gamescope, que al activarla se active lo de gamescope») */
void fijar(Estado &e, const std::string &appid, const std::string &clave, const std::string &valor);
/* el juego vuelve a tomar todo de lo de todos (y si su linea no se sabia leer, pasa a llevarla OdysseyVK) */
void comoTodos(Estado &e, const std::string &appid);

/* ---- la linea ---- */
/* la linea de lanzamiento de unos ajustes, con el ejecutable de Neural Forge si va puesto:
   gamescope -w 2048 -h 1536 -W 3840 -H 2160 -S stretch -f --force-grab-cursor -- env MANGOHUD=1 ... %command% --x */
std::string linea(const Ajustes &a, const std::string &nfExe, const std::string &nfGpu = std::string());
/* lo que dice una linea de Steam: `ajustes` solo trae las claves que la linea decide (sin gamescope no dice nada
   de su resolucion). Falso si no se sabe leer. */
struct Leida {
  Ajustes ajustes;
  std::string nfExe;
  std::string nfGpu;   /* (v1.18) el NEURAL_FORGE_OFFLOAD_GPU de la linea si no es el de secundariaNf (vacio: el nuestro) */
};
bool leerLinea(const std::string &linea, Leida &l);

/* ---- Steam ---- */
std::string raizSteam();
std::string rutaLocalconfig();   /* el de la cuenta con la que se entro la ultima vez (releida si cambia loginusers.vdf) */
/* Steam esta abierto: como lo mira el propio steam.sh, el proceso de ~/.steam/steam.pid vivo (y llamado steam);
   sin ese fichero (Steam de Flatpak), un `steam` o un `steamwebhelper` de este usuario en /proc */
bool steamAbierto();
/* lo que, si cambia, cambia lo que ensena la pestana: las fechas de localconfig.vdf, integraciones.conf y la marca,
   si Steam esta abierto y las carpetas de lo instalado. El panel la mira cada dos segundos y solo rehace la pestana
   si es otra (antes la rehacia entera: leer y analizar los ~100 KB de localconfig.vdf y recorrer /proc) */
std::string firma();
/* appid → sus opciones de lanzamiento (los que no tienen, no salen) */
bool leerSteam(std::map<std::string, std::string> &opciones, std::string *err = nullptr);
/* escribe esas lineas (appid → linea) en localconfig.vdf con Steam cerrado: antes guarda una copia en
   ~/.local/state/odysseyvk/steam/ (la primera de todas se queda para siempre), escribe aparte y cambia el
   fichero de golpe, y comprueba lo escrito leyendolo de vuelta */
bool escribirSteam(const std::map<std::string, std::string> &lineas, std::string *respaldo, std::string *err);

/* Neural Forge: appid → sus ejecutables («PlayRDR2.exe,RDR2.exe»), de `neural-forge-cli games` (Rodolfo: «este
   ultimo no lo preguntes, escanealo de neuralforge y añadelo al respectivo juego si activan NEURAL_FORGE_ENABLE=1»).
   Se mira una vez por proceso; vacio si no esta instalado */
const std::map<std::string, std::string> &exesNeuralForge();
/* el ejecutable que lleva un juego con Neural Forge puesto: el de Neural Forge o, si no lo da, el de su linea */
std::string exeDe(const Estado &e, const std::string &appid);

/* (v1.18) La segunda tarjeta, para el offloading de Neural Forge (Rodolfo: «añade el interruptor del offloading de Neural
   Forge en Integraciones», «remember that neural needs a parameter to offload and needs the game added to their list and
   the parameters on the game», «remember to consider some users use x8»). Neural Forge la quiere en
   `NEURAL_FORGE_OFFLOAD_GPU=fabricante:modelo@direccion` (o su numero, que cambia con el orden de la lista) y no la escribe
   nunca solo: se pone a mano. La principal es la del monitor 3D (la salida `pantalla` de serve.conf, la del hub o, sin
   ninguna, la que diga «G90XF» en su EDID; si tampoco, la que arranco el equipo, boot_vga): ahi se queda el juego y el 3D.
   La segunda, una NVIDIA que no sea esa (Neural Forge pide una RTX 40 o mas nueva: eso lo mira el); con varias, la de mas
   enlace. El enlace es lo que da de verdad: la menor de la tarjeta y su ranura (MEDIDO en el equipo de Rodolfo: la 5080
   puede PCIe 5.0 x16 y su ranura, 0000:04:00.0, solo 4.0 x4), no lo de ahora, que en reposo baja de velocidad */
struct Secundaria {
  std::string pci;       /* «0000:05:00.0» */
  std::string ids;       /* «10de:2c02» */
  std::string nombre;    /* «RTX 5080» */
  int gen = 0;           /* la generacion de PCIe que da el enlace (4 = PCIe 4.0) */
  int ancho = 0;         /* las lineas que da el enlace (x4) */
  int anchoTarjeta = 0;  /* las que podria la tarjeta (x16) */
  std::string valor() const { return ids + "@" + pci; }
};
/* falso si no hay segunda NVIDIA; se mira una vez por proceso */
bool secundariaNf(Secundaria &s);
/* el NEURAL_FORGE_OFFLOAD_GPU propio de un juego (el que traia su linea), o vacio: el de secundariaNf */
std::string gpuDe(const Estado &e, const std::string &appid);

/* ---- poner al dia y escribir ---- */
/* toma lo que se cambio en Steam por fuera desde la ultima vez (lo nuevo pasa a ser propio del juego); cierto
   si el estado cambio */
bool sincronizar(Estado &e, const std::vector<std::string> &appids, const std::map<std::string, std::string> &steam);
/* appid → la linea que le falta escribir en Steam (los que ya dicen lo mismo, aunque en otro orden, no salen) */
std::map<std::string, std::string> porEscribir(const Estado &e, const std::vector<std::string> &appids,
                                               const std::map<std::string, std::string> &steam);

struct Resultado {
  bool ok = true;
  bool steamAbierto = false;
  int pendientes = 0;   /* juegos con la linea por escribir */
  int escritos = 0;
  std::vector<std::string> cuales;   /* los appid escritos (o por escribir) */
  std::string respaldo, error;
};
/* lee Steam, toma lo cambiado por fuera y, si `escribir` y Steam esta cerrado, escribe lo pendiente; con Steam
   abierto y algo pendiente deja la marca para que el servicio lo escriba al cerrarse */
Resultado aplicar(const std::vector<std::string> &appids, bool escribir);
/* para el servicio, cada dos segundos: con la marca puesta y Steam cerrado en dos miradas seguidas (por si se esta
   reiniciando), escribe lo pendiente; si no puede, lo reintenta unas veces y lo deja (el panel sigue diciendo que
   falta). Cierto si intento escribir (lo hecho, en `r`). `appids` se pide solo cuando toca */
struct Vigia {
  int sinSteam = 0, fallos = 0;
};
bool vigilar(Vigia &v, const std::vector<std::string> &(*appids)(), Resultado &r);
/* la marca de «hay que escribir al cerrar Steam» (~/.local/state/odysseyvk/integraciones-pendiente) */
bool hayMarca();
void ponerMarca();
void quitarMarca();

}  // namespace integ

#endif
