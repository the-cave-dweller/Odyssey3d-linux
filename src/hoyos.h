/* hoyos.h — los huecos que deja mover cada ojo su mitad de la imagen.
   Al desplazar el cuadro para el ojo izquierdo y para el derecho, detras del canto de un objeto
   cercano no hay color que poner: ahi el programa de Samsung rellena (warp.least_hole,
   warp.hole_edge, inpaint.push/pull). Aqui no se rellena el color, se rellena el MAPA en el
   sitio de destino: cada ojo recibe su propio mapa, ya puesto donde va a caer, de modo que el
   shader de la trama lee ahi la profundidad correcta y va a buscar el color del objeto en vez de
   estirar el fondo por encima del canto.
   La salida va intercalada (R = ojo izquierdo, G = ojo derecho) porque el shader lee las dos del
   mismo texelFetch: asi el coste en la grafica no sube respecto de leer un solo mapa. */
#pragma once
#include <cstdint>

/* mapa: n x n, un byte por punto (255 = lo mas cerca). ganMapPx: cuanto desplaza una unidad
   completa del mapa, en puntos del propio mapa (convGain_ x ancho del mapa). plano: el valor del
   mapa que se queda quieto, en 0..1. rg: salida, 2 bytes por punto.
   Devuelve cuantos puntos hizo falta rellenar entre los dos ojos (para poder medirlo sin pantalla;
   si hoyos es no nula, recibe alli el mismo numero). */
int rellenarVistas(const uint8_t *mapa, int n, float ganMapPx, float plano, uint8_t *rg, int *hoyos);

/* si hay que rellenar: ODYSSEYVK_HOYOS=0 lo apaga (y entonces los dos canales llevan el mapa en
   crudo, que es el comportamiento de la v0.35 y anteriores) */
bool hoyosActivo();
