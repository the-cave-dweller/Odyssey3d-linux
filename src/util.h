/* util.h — pequeñas ayudas compartidas (texto, hex, sysfs). */
#ifndef ODYSSEY_UTIL_H
#define ODYSSEY_UTIL_H

#include <cstdint>
#include <string>
#include <vector>

std::string strTrim(const std::string &s);
std::string strLower(std::string s);
std::vector<uint8_t> unhex(const std::string &s);
std::string toHexUpper(const uint8_t *p, size_t n);
std::string toHexUpper(const std::vector<uint8_t> &v);

/* Lee un archivo pequeño (sysfs, config) en una cadena. False si no existe. */
bool readSmallFile(const std::string &path, std::string &out);

/* Como nos llamamos nosotros: el titulo de la ventana del visor y el app_id con el que el hub
   reconoce su propia ventana (GLFW saca el app_id del nombre del ejecutable, y el ejecutable se
   llama igual). Las dos cosas viven juntas aqui para que no se desafinen: el titulo se compara
   literal contra lo que reporta el compositor, asi que va sin acentos ni espacios. */
inline constexpr const char *NOMBRE_APP = "OdysseyVK";

/* Copia la carpeta de datos del nombre viejo (odyssey3d) a la nueva (odysseyvk) si es que la nueva
   todavia no tiene nada: la calibracion, las redes, los perfiles y el canal de ajuste siguen ahi
   despues del cambio de nombre. No borra ni toca lo que ya existe, y solo trabaja una vez por
   proceso. Devuelve cuantos archivos copio (0 si no habia nada que copiar). */
int migrarDatosDeViejoNombre();

/* Lee una variable de entorno nuestra: primero el nombre nuevo (ODYSSEYVK_...) y, si no esta
   puesta, la del nombre viejo (ODYSSEY3D_...), por los atajos y los scripts de siempre. */
const char *entornoOdyssey(const char *nombre);

/* Preferencia de la red del conversor 2D a 3D, guardada en ~/.config/odysseyvk/red-elegida:
   1 = la red de Samsung (mde_*.onnx), 2 = la nativa (depth_anything_v2_small.onnx).
   leerRedPreferida() devuelve 0 si todavia no se eligio ninguna. */
std::string rutaConfigOdyssey(const char *nombre);
/* Ruta dentro de ~/.local/state/odysseyvk: lo que esta pasando ahora mismo (la instantanea del
   panel, la senal de que entregamos la pantalla a proposito), no una preferencia. */
std::string rutaEstadoOdyssey(const char *nombre);
/* (v1.18) un ajuste nuestro que antes vivia en ~/.local/state/odysseyvk (Rodolfo: «todo lo tuyo que tenga un setting debe
   estar en tu carpeta, lo que no te sirva en otra»): su ruta en ~/.config/odysseyvk; la primera vez que no esta ahi y si en
   la vieja, se pasa (se mueve) */
std::string rutaAjusteOdyssey(const char *nombre);
int leerRedPreferida();
void grabarRedPreferida(int red);
/* Ruta por convencion de la red nativa (la que baja tools/descargar-modelo.sh). */
/* A que boton (1 o 2) pertenece un .onnx por como se llama: las redes que trajo el programa de
   Samsung son mde_... o Disp_...; la nuestra se llama depth_anything_v2_small.onnx. */
int slotRed(const std::string &ruta);
std::string rutaRedNuestra();
/* las dos versiones de la abierta: la ligera (fp16, la de fabrica) y la de siempre (fp32) */
/* (v1.16) ~/.local/share/odysseyvk/models/ (con la barra al final): las redes de profundidad; antes `modelos` */
std::string carpetaRedes();
std::string rutaRedLigera();
std::string rutaRedVieja();
/* (v1.16) las tarjetas de AMD (el conductor amdgpu), leidas de /sys/class/drm/cardN/device: para elegir donde va la red con
   ROCm (MIGraphX) y para la pestana Ajustes. `pci` es la direccion PCI en minusculas («0000:03:00.0»); `nombre`, el
   product_name que da el conductor o, si no lo da, lo de entre corchetes de pci.ids («Radeon 610M»); lo que no se pueda
   leer queda en -1 */
struct TarjetaAmd {
  std::string pci, nombre;
  long totalMiB = -1, usadaMiB = -1;
  int ocupada = -1;       /* % (gpu_busy_percent) */
  double tempC = -1.0;
};
std::vector<TarjetaAmd> tarjetasAmd();
/* (v1.18) el nombre de una tarjeta en /usr/share/hwdata/pci.ids por su fabricante y modelo («10de», «2c02»: sin 0x): lo de
   entre corchetes («GeForce RTX 5080») o, sin corchetes, la linea entera; vacio si no esta */
std::string nombrePciIds(const std::string &fabricante, const std::string &modelo);
/* (v1.18) la direccion PCI de la tarjeta que da una salida de DRM («DP-4» → «0000:01:00.0»). En sysfs el `device` del
   conector (card2-DP-4/device) es la tarjeta de DRM, no la PCI: la PCI es la de card2/device. Vacio si no esta */
std::string pciDeConector(const std::string &conector);
/* (v1.18) la salida conectada cuyo EDID lleve `texto` («G90XF»: el monitor 3D), o vacio */
std::string conectorConEdid(const std::string &texto);
/* (v1.16) una red de video (Video Depth Anything): lleva «video» en el nombre del archivo. Mira 32 cuadros a la vez, solo
   para videos (los juegos siguen con la de siempre), y solo en la tarjeta */
bool esRedDeVideo(const std::string &ruta);
/* la red de video que se baja desde el panel (Video Depth Anything pequena, exportada a ONNX por la comunidad) */
std::string rutaRedVideo();
/* Marcas del arranque del visor (v1.12): cada paso, en ms desde que arranco el proceso (de
   /proc/self/stat, no del reloj del registro, que agrupa las lineas por tandas). `resumenArranque`
   da la linea con lo que tardo cada paso. */
void marcaArranque(const char *paso);
std::string resumenArranque();
/* el pin del panel de ajustes (rama experimental): fijado encima o no, en ~/.config/odysseyvk/panel-fijado (v1.18; antes en ~/.local/state) */
bool panelFijado();
void fijarPanel(bool si);
/* el color de resaltado de KDE (de ~/.config/kdeglobals), 0..1; false si no lo hay */
bool colorResaltadoKde(float rgb[3]);

/* True si el archivo existe y se puede leer. */
bool fileExists(const std::string &path);

/* Lee un archivo binario completo (calibraciones, correctores). False si falta. */
bool readBinaryFile(const std::string &path, std::vector<uint8_t> &out);

/* Ejecuta un comando y captura stdout+stderr. Devuelve el código de salida. */
int runCapture(const std::string &cmd, std::string &out);

/* Forma "aaaa-bbbb" a partir de un entero 0xNNNN. */
std::string usbIdToHex(uint16_t v);

/* Asegura XDG_RUNTIME_DIR + WAYLAND_DISPLAY en el entorno del proceso.
   Devuelve el nombre del socket ("wayland-1") o vacío si no hay sesión Wayland. */
std::string ensureWaylandEnv();
/* (v1.16) abre el panel de ajustes con estos argumentos («--juegos», «--desde-pastilla»...) en este proceso, que se
   sustituye: con `odysseyvk-panel` si esta junto a este ejecutable (sin las bibliotecas del visor: arranca en ~1 ms
   en vez de ~48), si no con `odysseyvk ajustes`. ODYSSEYVK_PANEL_PESADO=1 obliga a lo segundo. Solo vuelve si no
   pudo arrancar ninguno */
void execPanelDeAjustes(const std::vector<std::string> &args);

#endif
