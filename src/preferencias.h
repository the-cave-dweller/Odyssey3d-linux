/* preferencias.h — los ajustes de todos los dias y la biblioteca de juegos (v0.84).

   La ventana de ajustes de siempre (`odysseyvk-ajustes`, Qt) es un banco de taller: veinte numeros de
   la trama, perfiles por red, la vista de la camara. Aqui esta lo que de verdad se toca usando el
   monitor, que es lo que ensena el panel flotante (`odysseyvk ajustes`, el engranaje de la pastilla
   o el icono del sistema):

     - que hacer al ver un video a pantalla completa (preguntar, video normal, video dividido)
     - suavizar los videos divididos (el unico sitio donde se quita)
     - seguir la cabeza con la camara
     - HDR durante el 3D, atajos de teclado globales
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
  bool hdr = false;
  bool atajos = true;
  double fantasma = 0.0128535;    /* crosstalk_base: 0 .. 0,10 en el panel */
  double relieve = 1.0;           /* fuerza del relieve del conversor: 0,5 .. 2 */
  std::vector<std::pair<std::string, std::string>> recordadas;   /* aplicacion -> modo */
};
Valores leer();

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
bool aplicar(const std::string &id, int sub, float frac, bool soltado, const std::vector<Juego> &juegos,
             std::string *aviso, bool *cerrar);

}  /* namespace prefs */

#endif
