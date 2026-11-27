#pragma once
/* efectos.h — (v1.18) los efectos de ReShade (.fx) sobre los juegos, dentro del visor 3D.
   Rodolfo: «Add a shaders icon to each game on the games tab menu, use the code from daedalus to get them working, and
   use my folder for the shader and lut files ... the shaders must have a toggle pill, each of them a scrollbar and their
   respective setting sliders», «as you can take advantage of depth just as original reshade did».

   vkDaedalus (un vkBasalt con menu, zlib) los corre DENTRO del juego, como capa de Vulkan. Aqui no: el visor los pasa
   sobre cada cuadro que captura del juego, antes del 3D. Asi valen para cualquier juego (sin capa en el juego) y tienen
   profundidad siempre, la de la red o la de DLSS (la que ya usa el 3D), donde ReShade solo la tiene si el juego le deja
   su bufer de profundidad. Tambien pasan por los cuadros que genera MAKO, que llegan al visor por la captura.

   El compilador es el de ReShade (ReShade FX → SPIR-V, de crosire, BSD-3), en la version de vkDaedalus con sus arreglos
   (third_party/reshadefx). Los .fx y sus texturas (las LUT) se leen de la carpeta de ReShade de Rodolfo,
   ~/.config/vkdaedalus/reshade (Shaders/ y Textures/, con sus subcarpetas), o la de `efectos_carpeta` en serve.conf.

   Los ajustes: ~/.config/odysseyvk/effects/all.conf (General → Efectos: los de todos los juegos) y
   ~/.config/odysseyvk/effects/APPID.conf (el icono de efectos de la caratula: los de ese juego). Un juego sin el suyo
   usa los de todos. Por dentro, como los de vkBasalt: `effects = A:B`, `disabledEffects = B`, `A.Valor = 1.5`,
   `A.Vector[1] = 0.2`, `A@DEFINE = 1`; y `puestos = si|no` (todos apagados de una vez). */
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace reshadefx {
struct effect_module;
}

namespace efectos {

/* la carpeta de ReShade: `efectos_carpeta` de serve.conf, o la primera que exista de ~/.config/vkdaedalus/reshade,
   ~/.config/vkBasalt-overlay/reshade, ~/.config/vkBasalt/reshade; vacio si ninguna */
std::string carpeta();
/* sus carpetas de sombreadores y de texturas (Shaders/ y Textures/ con todas sus subcarpetas) */
std::vector<std::string> carpetasSombreadores();
std::vector<std::string> carpetasTexturas();
/* los .fx que hay: nombre (sin .fx) → ruta. Si dos se llaman igual, el mas cerca de Shaders/ */
std::map<std::string, std::string> disponibles();

struct Efecto {
  std::string nombre;   /* «CRT», como el archivo sin .fx */
  bool activo = true;   /* su pastilla */
  std::vector<std::pair<std::string, std::string>> valores;   /* «Gamma» → «2.493», «Angle[0]» → «0.005» */
  std::vector<std::pair<std::string, std::string>> defs;      /* «PI» → «3.1415927» (#define del efecto) */
  const std::string *valor(const std::string &clave) const;
  void fijar(const std::string &clave, const std::string &v);
};
struct Ajustes {
  bool puestos = false;          /* el interruptor de todos */
  std::vector<Efecto> lista;     /* en el orden en que se aplican */
  bool propio = false;           /* (al leer) vienen del archivo del juego, no de los de todos */
  Efecto *buscar(const std::string &nombre);
  bool algoQueAplicar() const;
};
/* los de fabrica: los favoritos de Rodolfo (su vkdaedalus.conf del 27-nov), con CRT y DPX apagados */
Ajustes deFabrica();
/* "" = todos los juegos */
std::string rutaAjustes(const std::string &appid);
/* los del juego si tiene los suyos; si no, los de todos; si tampoco, los de fabrica */
Ajustes cargar(const std::string &appid);
bool guardar(const std::string &appid, const Ajustes &a, std::string *err = nullptr);
/* el juego vuelve a los de todos */
bool quitarPropios(const std::string &appid);
/* para vigilar cambios: la hora de cambio de los dos archivos que pueden valer para ese juego, juntas */
long long firma(const std::string &appid);

/* ---- lo que enseña el panel de cada efecto: sus controles (las `uniform` con anotaciones ui_*) ---- */
struct Control {
  enum Tipo { Real, Entero, Bool, Lista } tipo = Real;
  std::string nombre;        /* el de la uniform: la clave en los ajustes */
  std::string rotulo;        /* ui_label, o el nombre */
  std::string ayuda;         /* ui_tooltip */
  std::string categoria;     /* ui_category */
  int componentes = 1;       /* float2 → 2: «Angle[0]», «Angle[1]» */
  float minimo = 0.0f, maximo = 1.0f, paso = 0.0f;
  std::vector<std::string> opciones;   /* ui_items de combo/radio */
  float defecto[4] = {0, 0, 0, 0};    /* el valor que trae el .fx */
  bool color = false;        /* ui_type = color: componentes de 0 a 1 */
};
struct Descripcion {
  bool ok = false;
  std::string error;
  std::vector<Control> controles;
  bool usaProfundidad = false;   /* tiene una textura DEPTH */
  bool computo = false;          /* tiene pases de computo: aqui no corren */
};
/* (v1.18) si el .fx usa la profundidad: lo dice su texto (GetLinearizedDepth, DepthBuffer, `: DEPTH`, la de qUINT...).
   ReShade.fxh declara la textura DEPTH para todos, asi que el modulo no sirve. Sin compilar: rapido para las 139 de la
   lista de añadir. MEDIDO en la carpeta de Rodolfo: 39 de 139 */
bool usaProfundidad(const std::string &ruta);
/* compila el .fx (sin SPIR-V) y saca sus controles; se guarda por ruta y hora de cambio */
Descripcion describir(const std::string &ruta);
/* el valor de un control: el de los ajustes o, si no lo dicen, el del .fx */
float valorDe(const Efecto &e, const Control &c, int componente);
std::string claveDe(const Control &c, int componente);

/* ---- compilar de verdad (el visor y la prueba) ---- */
struct Compilado {
  bool ok = false;
  std::string error, avisos;
  std::shared_ptr<reshadefx::effect_module> modulo;
  std::map<std::string, std::vector<uint32_t>> spirv;   /* punto de entrada → SPIR-V */
};
/* `ancho` x `alto`: BUFFER_WIDTH y BUFFER_HEIGHT; la profundidad va como la da el visor (255 = cerca, sin linealizar) */
std::shared_ptr<Compilado> compilar(const std::string &ruta, int ancho, int alto,
                                    const std::vector<std::pair<std::string, std::string>> &defs);
/* una textura de un .fx (su anotacion `source`): busca en las carpetas de texturas */
std::string rutaTextura(const std::string &nombre);

/* ---- (v1.18) la vista de los efectos en el panel: el panel la pide (pedirVista, cada segundo con la pestana abierta) y el
   visor, con un juego, deja en $XDG_RUNTIME_DIR/odysseyvk-efectos-vista una copia chica del cuadro con la mitad
   izquierda sin efectos y la derecha con ellos (RGBA, tras «OKVV» y su ancho y alto) ---- */
void pedirVista();
bool vistaPedida();   /* pedida hace menos de 2,5 s */
bool escribirVista(const std::vector<uint8_t> &rgba, int w, int h);
/* la ultima, si cambio desde `*firma` (la hora del archivo); falso si no hay o no cambio */
bool leerVista(std::vector<uint8_t> &rgba, int &w, int &h, long long *firma);

/* ---- (v1.18) fuentes publicas de efectos: los repositorios de ReShade mas conocidos, con su boton de bajar ----
   Rodolfo: «add some public shader sources to the list with their download button» y «gray out already downloaded one
   unless the files differ, never duplicate files, always download the difference». No se baja el repositorio entero: se
   pide su arbol a GitHub (la API de arboles: cada archivo con su SHA-1 de git) y se compara con lo que ya hay en la
   carpeta de ReShade, por contenido. Lo que ya esta (en cualquier sitio) no se baja; lo que se llama como uno que ya hay
   (en el mismo sitio y distinto, o en otro) tampoco: se queda el que hay (Rodolfo: «dont replace old ones, delete
   duplicated downloaded files»); lo demas se baja, sin pisar nada. Solo lo de Shaders/ y Textures/ del repositorio, cada
   uno a la de la carpeta; ReShade.fxh y ReShadeUI.fxh solo de los de crosire. Cada archivo bajado se comprueba con su
   SHA-1 de git antes de ponerlo y se apunta en ~/.local/state/odysseyvk/efectos-bajados: si despues queda duplicado (otro
   igual o con su nombre), limpiarDuplicados lo borra. Lo hace `odysseyvk efectos fuentes` (comprobar), `odysseyvk
   efectos bajar ID` y `odysseyvk efectos limpiar` (efectos_fuentes.cpp, con libcrypto); el panel los arranca aparte */
struct Fuente {
  const char *id, *nombre, *repo, *nota;
};
const std::vector<Fuente> &fuentesPublicas();
/* lo que dijo la ultima comprobacion de una (~/.local/state/odysseyvk/efectos-fuentes) */
struct EstadoFuente {
  bool comprobada = false;
  int nuevos = 0, cambian = 0, conflictos = 0, total = 0;
  long long bytes = 0;
  long hora = 0;
  std::string error;   /* «no-encontrada», «limite» (GitHub dijo que no por ahora) u otro */
};
std::map<std::string, EstadoFuente> leerFuentes();
long horaComprobacion();            /* la de la ultima comprobacion completa, 0 si nunca */
bool comprobando();                 /* hay una `odysseyvk efectos fuentes` en marcha */
/* la bajada en marcha o la ultima (~/.local/state/odysseyvk/efectos-bajada) */
struct Bajada {
  bool enMarcha = false, lista = false;
  std::string id, error;
  long hecho = 0, total = 0, hora = 0;
};
Bajada leerBajada();
/* `odysseyvk efectos ARGS...`, aparte y suelto (como mako::lanzarOrden) */
bool lanzarOrden(const std::vector<std::string> &args, std::string *err = nullptr);
/* (efectos_fuentes.cpp, solo en `odysseyvk`) comprobar unas (todas si `ids` va vacio) y bajar una */
int comprobarFuentes(const std::vector<std::string> &ids, bool decir);
/* (v1.18) borra lo bajado por OdysseyVK que haya quedado duplicado (mismo contenido o mismo nombre que otro); cuantos */
int limpiarDuplicados(bool decir);
bool bajarFuente(const std::string &id, std::string *err);

}  // namespace efectos
