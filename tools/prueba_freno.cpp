/* prueba_freno.cpp — el freno del juego que pierde la pantalla completa al entrar el 3D (src/frenojuego.h), con los
   tiempos del registro de Rodolfo del 27-nov (Cyberpunk): sesiones de 3-4 s que el servicio relanzaba a los 4 s de
   volver el juego a pantalla completa, siete seguidas. Sin ventanas ni tarjeta: solo el reloj.
   g++ -std=c++17 -O1 -Isrc tools/prueba_freno.cpp -o /tmp/freno && /tmp/freno   */
#include "frenojuego.h"

#include <cstdio>

static int fallos = 0;
#define MIRA(c, que)                                                        \
  do {                                                                      \
    const bool bien_ = (c); /* una sola vez: las condiciones llaman a cierre() */ \
    if (!bien_) ++fallos;                                                   \
    std::printf("%s  %s\n", bien_ ? "bien " : "MAL  ", que);                \
  } while (0)

/* el servicio de antes y el de ahora sobre la misma historia: el juego vuelve a pantalla completa 0,4 s despues de
   cada cierre, el 3D se lanza a los 4 s de verlo asi (si no hay freno) y cada sesion dura `dura` segundos. Cuenta
   cuantas veces entra el 3D en `total` segundos */
static int vueltas(bool conFreno, double dura, double total, bool juego) {
  FrenoJuego f;
  const std::string u = "{972491ad-24e4-44de-a640-f47129d9ce96}";
  int n = 0;
  double t = 0.0;
  while (t < total) {
    if (conFreno && f.frenada(u, t)) {
      t += 0.2;   /* una vuelta del bucle del servicio */
      continue;
    }
    ++n;         /* lanza */
    t += dura;   /* y la sesion se cierra por la salida de pantalla completa */
    if (conFreno) f.cierre(u, dura, true, juego, t);
    t += 0.4 + 4.0;
  }
  return n;
}

int main() {
  const std::string a = "{a}", b = "{b}";
  {
    FrenoJuego f;
    MIRA(f.cierre(a, 3.1, true, true, 10.0) == 0.0 && !f.frenada(a, 10.0), "el primer cierre corto no frena");
    MIRA(f.cierre(a, 3.3, true, true, 18.0) == 60.0 && f.frenada(a, 18.1), "el segundo seguido frena un minuto");
    MIRA(f.frenada(a, 77.9) && !f.frenada(a, 78.0), "y al minuto se le vuelve a lanzar");
    MIRA(f.cierre(a, 3.2, true, true, 85.0) == 300.0 && f.frenada(a, 384.9) && !f.frenada(a, 385.0),
         "el tercero, cinco minutos");
    MIRA(!f.frenada(b, 90.0), "otra ventana no lleva freno");
    MIRA(f.cierre(a, 40.0, false, true, 500.0) == 0.0 && !f.frenada(a, 500.0) && f.cortasSeguidas == 0,
         "una sesion que aguanta 15 s lo borra");
    MIRA(f.cierre(a, 3.0, true, true, 510.0) == 0.0, "y la cuenta empieza de cero");
  }
  {
    FrenoJuego f;
    f.cierre(a, 3.0, true, false, 1.0);
    MIRA(f.cierre(a, 3.0, true, false, 9.0) == 0.0 && !f.frenada(a, 9.0),
         "un video que sale de pantalla completa dos veces no se frena (lo hace el usuario)");
    f.cierre(b, 3.0, false, true, 20.0);
    MIRA(f.cierre(b, 3.0, false, true, 28.0) == 0.0 && !f.frenada(b, 28.0),
         "un juego cuya sesion se cierra por otra cosa tampoco (eso lo cuenta el enfriado de las muertes)");
  }
  {
    FrenoJuego f;
    f.cierre(a, 3.0, true, true, 1.0);
    f.cierre(b, 3.0, true, true, 5.0);
    MIRA(f.cierre(a, 3.0, true, true, 9.0) == 0.0, "los cierres han de ser seguidos en la misma ventana");
  }
  {
    FrenoJuego f;
    f.cierre(a, 3.0, true, true, 1.0);
    f.cierre(a, 3.0, true, true, 9.0);
    f.podar({a, b});
    MIRA(f.frenada(a, 10.0), "la ventana que solo salio de pantalla completa conserva el freno");
    f.podar({b});
    MIRA(!f.frenada(a, 10.0) && f.cortasSeguidas == 0, "la que ya no existe se olvida");
  }
  {
    /* lo medido: 7 entradas en 54 s (18:26:53-18:27:47) */
    const int antes = vueltas(false, 3.4, 54.0, true), ahora = vueltas(true, 3.4, 54.0, true);
    std::printf("       en 54 s: antes %d entradas del 3D, ahora %d\n", antes, ahora);
    MIRA(antes == 7 && ahora == 2, "en los 54 s del registro: de 7 entradas a 2");
    const int hora = vueltas(true, 3.4, 3600.0, true);
    std::printf("       en una hora de bucle: %d entradas (sin freno, %d)\n", hora, vueltas(false, 3.4, 3600.0, true));
    MIRA(hora <= 14, "en una hora, una cada cinco minutos como mucho");
    MIRA(vueltas(true, 3.4, 54.0, false) == 7, "y a un video no se le toca");
  }
  std::printf("%s\n", fallos ? "HAY FALLOS" : "todo bien");
  return fallos ? 1 : 0;
}
