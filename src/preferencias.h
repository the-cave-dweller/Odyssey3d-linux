/* preferencias.h — los ajustes de todos los dias y la biblioteca de juegos (v0.84).

   La ventana de ajustes de siempre (`odysseyvk-ajustes`, Qt) es un banco de taller: veinte numeros de
   la trama, perfiles por red, la vista de la camara. Aqui esta lo que de verdad se toca usando el
   monitor, que es lo que ensena el panel flotante (`odysseyvk ajustes`, el engranaje de la pastilla
   o el icono del sistema):

     - que hacer al ver un video a pantalla completa (preguntar, video normal, video dividido)
     - suavizar los videos divididos (el unico sitio donde se quita)
     - seguir la cabeza con la camara
     - atajos de teclado globales (el HDR ya no se elige: se respeta el del escritorio)
     - cuanta imagen del otro ojo se resta (el fantasma) y la fuerza del relieve del conversor
     - las aplicaciones que pasan directas sin preguntar, para poder olvidarlas

   Todo vive en ~/.config/odysseyvk/serve.conf, que es lo que lee el servicio; lo que tiene efecto
   al momento se le manda ademas al visor abierto por el canal de ajuste.

   Y la primera piedra del lanzador de juegos: la biblioteca de Steam leida de sus propios ficheros
   (libraryfolders.vdf y los appmanifest_*.acf), para listarlos y arrancarlos desde el panel. */
#ifndef ODYSSEY_PREFERENCIAS_H
#define ODYSSEY_PREFERENCIAS_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "vkui.h"

namespace prefs {

struct Juego {
  std::string id;          /* el appid de Steam */
  std::string nombre;
  long long ultimaVez = 0; /* LastPlayed, en segundos; 0 = nunca */
  /* la caratula de la cache de Steam, ya reducida a RGBA de caraW x caraH (vacia si no la hay) */
  std::shared_ptr<const std::vector<uint8_t>> cara;
  int caraW = 0, caraH = 0;
};
/* Los juegos instalados en todas las bibliotecas de Steam, los jugados hace menos primero. No
   entran las herramientas (Proton, los Steam Linux Runtime, los redistribuibles). */
std::vector<Juego> juegosSteam();
/* Arranca un juego por su appid (`steam steam://rungameid/ID`), suelto de este proceso. */
bool lanzarJuego(const std::string &id, std::string *err = nullptr);

struct Valores {
  std::string modo = "ask";       /* ask · conv · sbs */
  int suavizar = 1;               /* los videos divididos: 0 nunca · 1 siempre · 2 solo si hace falta */
  bool rastreo = false;
  bool atajos = true;
  double fantasma = 0.0128535;    /* crosstalk_base: 0 .. 0,10 en el panel */
  double mezclaVistas = 0.10;     /* `filtro`: la parte del ciclo que mezcla las dos vistas (1 / pendiente), 0,10 .. 1 */
  double relieve = 1.0;           /* fuerza del relieve del conversor: 0,5 .. 2 */
  std::string modelo;             /* `modelo =` de serve.conf, ya expandido; vacio = la red abierta de siempre */
  bool avisarActualizaciones = true;   /* `actualizaciones = no` apaga la tarjeta de version nueva (v0.95) */
  bool actualizarAuto = false;         /* `actualizar_auto = si`: con una version nueva se instala sola (v0.95) */
  bool enBarraTareas = true;           /* `barra_tareas = no`: el visor 3D no sale en la barra de tareas (v1.02) */
  bool enBandeja = true;               /* `bandeja = no`: sin icono en la bandeja del sistema (v1.03) */
  std::vector<std::pair<std::string, std::string>> recordadas;   /* aplicacion -> modo */
  /* la rama experimental (pestana Experimental) */
  std::string expRed = "cuadrado";   /* `exp_red`: cuadrado | 518 | 770 | 1022 */
  int expMapa = 518;                 /* `exp_mapa`: 518 | 770 | 1022 */
  bool expTope = false;              /* `exp_tope_arrastre = si` */
  int expAnticipacion = 0;           /* `exp_anticipacion`: ms, 0 = apagada */
  double expTopesRed = 1.0;          /* `exp_topes_red`: los topes por ojo con la red, x1..x4 */
  std::string expPrioridad = "normal";   /* `exp_prioridad`: normal | alta | tiempo_real */
  std::string expSinRed = "no";          /* `exp_sin_red`: no | si (suelo e imagen, de antes, valen como si) */
  bool vrrEn3d = false;                  /* `vrr_en_3d = si`: el VRR puesto durante el 3D */
  /* (v1.13) los juegos con DLSS (con ODYSSEYVK=1 en sus opciones de lanzamiento) */
  bool profDlss = true;                  /* `profundidad_dlss = no`: la red aunque llegue la de DLSS */
  bool hudPlano = true;                  /* `hud_plano = no`: el HUD del juego con el relieve de lo de detras */
  bool fpsJuego = true;                  /* `fps_juego = no`: sin el cuadrito de FPS junto a la pastilla */
};
Valores leer();

/* La red de profundidad abierta (Depth Anything V2 pequena, v0.92): de donde se baja, cuanto pesa y
   si ya esta en su sitio (~/.local/share/odysseyvk/modelos/depth_anything_v2_small.onnx). */
extern const char *const kUrlRedAbierta;
constexpr long kBytesRedAbierta = 49642442;   /* la fp16, la de fabrica desde la rama experimental */
constexpr long kBytesRedVieja = 99060839;     /* la de siempre (fp32), que sigue valiendo */
bool redAbiertaPresente();   /* la una o la otra */
bool redLigeraPresente();
/* La descarga, con un `curl` aparte: `descargar` la arranca (una sola a la vez); `vigilarDescarga`
   se llama de vez en cuando desde el bucle del panel y devuelve true si cambio algo (avance, fin o
   fallo); `descargaEnMarcha` y `textoDescarga` son lo que ensena el panel. */
bool descargar(std::string *err);
bool vigilarDescarga();
bool descargaEnMarcha();
std::string textoDescarga();

/* ---- la actualizacion (v0.95) ------------------------------------------------------------------
   La version nueva se mira en el VERSION de la rama main del repositorio de GitHub. La comprobacion
   es un `curl` aparte (como la descarga): `comprobarActualizacion` la arranca, `vigilarComprobacion`
   se llama de vez en cuando y devuelve true cuando acabo (deja el resultado en el archivo de estado
   `actualizacion`: `remota = 0.95` y `hora = ...`), y `estadoActualizacion` lo lee. */
extern const char *const kRepoGitHub;
extern const char *const kUrlVersionRemota;
struct Actualizacion {
  std::string remota;     /* la version que hay en GitHub (vacia = nunca se ha mirado o no se pudo) */
  bool pendiente = false; /* remota es mas nueva que la instalada */
  bool alDia = false;     /* se miro y la instalada es la ultima */
  long hora = 0;          /* cuando se miro (segundos desde 1970) */
};
bool versionMasNueva(const std::string &a, const std::string &b);   /* a > b, comparando numero a numero */
bool comprobarActualizacion(std::string *err);
bool vigilarComprobacion();
bool comprobacionEnMarcha();
Actualizacion estadoActualizacion();
/* `odysseyvk actualizar` / `odysseyvk desinstalar` en una unidad transitoria de systemd (fuera del
   servicio, que se va a reiniciar o a quitar) */
bool lanzarActualizacion(std::string *err);
bool lanzarDesinstalacion(std::string *err);

/* Lo que ensena el panel: rellena pestanas y filas a partir de los valores y de la biblioteca.
   `ids` sale alineado con las filas y dice que es cada una ("modo", "rastreo", "auto:zen",
   "juego:1332010"...), para saber que tocar cuando se pica. */
void llenar(Hud::State &st, const Valores &v, const std::vector<Juego> &juegos, std::vector<std::string> &ids);

/* Un golpe sobre la fila `id` (con `sub` = que opcion o que boton, y `frac` = por donde va el
   deslizador). Escribe serve.conf y avisa al visor abierto si el cambio se nota al momento.
   `soltado` = falso mientras se arrastra un deslizador: ahi solo se avisa al visor (se ve al
   momento) y serve.conf se escribe una vez, al soltar.
   Devuelve cierto si cambio algo; `aviso` trae la frase para el pie del panel y `cerrar` pide
   cerrar el panel (al lanzar un juego). */
/* Los atajos (v1.09): la accion que espera su combinacion nueva (vacio = ninguna). El panel le pasa
   cada combinacion pulsada a `teclaCapturada`, que la comprueba y la guarda en serve.conf; cierto si
   cambio algo que haya que repintar. `cancelarCaptura` deja de esperar. */
const std::string &capturando();
bool teclaCapturada(const std::string &combo, std::string *aviso);
void cancelarCaptura();
bool aplicar(const std::string &id, int sub, float frac, bool soltado, const std::vector<Juego> &juegos,
             std::string *aviso, bool *cerrar);

}  /* namespace prefs */

#endif
