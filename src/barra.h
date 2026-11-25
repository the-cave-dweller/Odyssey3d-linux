/* barra.h — la barra del reproductor en un video dividido (lado a lado).

   Con el video dividido cada ojo recibe una mitad del cuadro estirada al ancho entero. Eso vale para
   el video, que trae dos imagenes, pero no para lo que el reproductor pinta ENCIMA a lo ancho de la
   ventana: la barra de progreso, los botones, el titulo. Eso tambien se parte en dos, y cada ojo ve
   media barra distinta (la queja de Rodolfo con la 0.80: «la funcion se parte como el video en dos»).

   Arreglo: mirar el cuadro y encontrar las filas donde las dos mitades NO casan. En una fila de
   video las dos mitades son casi la misma imagen (corrida unos puntos, que es el relieve); en una
   fila con la barra del reproductor no se parecen en nada (botones a la izquierda en una, a la
   derecha en la otra; la parte ya vista de la barra en una y la que falta en la otra). Esas filas,
   arriba y abajo de la pantalla, se tejen en 2D con el cuadro ENTERO: la barra sale de una pieza y
   donde se ve es donde se pica. El resto de la pantalla sigue en 3D.

   El detector trabaja sobre la copia chica del cuadro que ya saca la trama para el conversor
   (518 x 518): por fila, la diferencia media entre las dos mitades despues de buscar el corrimiento
   que mejor las casa. El umbral se pone contra lo que miden las filas del centro, que son video
   seguro: si ahi tampoco casan, el video no viene dividido y no se toca nada. */
#ifndef ODYSSEY_BARRA_H
#define ODYSSEY_BARRA_H

#include <cstdint>
#include <vector>

struct Filas2D {
  float arriba = 0.0f;   /* fraccion del alto, desde arriba, que se teje en 2D (0 = nada) */
  float abajo = 0.0f;    /* lo mismo desde abajo */
};

class DetectorBarra {
 public:
  /* lo que midio el ultimo analisis, para el registro y la prueba */
  struct Medida {
    float centro = 0.0f;      /* residuo mediano de las filas del centro (video seguro) */
    float umbral = 0.0f;      /* a partir de cuanto una fila se da por pintada encima */
    float maxAbajo = 0.0f, maxArriba = 0.0f;   /* el residuo mas alto de cada zona */
    int filasAbajo = 0, filasArriba = 0;       /* cuantas filas pasan el umbral en cada zona */
    bool dividido = true;     /* falso: las mitades no casan ni en el centro (no es video dividido) */
  };

  /* `rgba` es el cuadro entero reducido a w x h (primera fila arriba); `ahora` en segundos.
     Devuelve las filas que hay que tejer en 2D en este momento (con su memoria: hacen falta dos
     analisis seguidos para poner una banda y medio segundo sin verla para quitarla). */
  Filas2D analizar(const uint8_t *rgba, int w, int h, double ahora, Medida *m = nullptr);
  void olvidar();
  Filas2D actual() const { return puesta_; }

 private:
  struct Zona {
    int seguidas = 0;         /* analisis seguidos viendola */
    double ultima = -1.0;     /* cuando se vio por ultima vez */
    float extension = 0.0f;   /* hasta donde llega (se queda con la mayor mientras dure) */
    bool puesta = false;
  };
  Zona abajo_, arriba_;
  Filas2D puesta_;
};

/* El residuo de cada fila (0..255): diferencia media entre la mitad izquierda y la derecha con el
   mejor corrimiento. Las filas que no se piden salen a -1. `cada` > 1 mide una fila de cada tantas
   en el centro (alli solo hace falta la mediana). Expuesto para la prueba. */
std::vector<float> residuoPorFila(const uint8_t *rgba, int w, int h, int cadaCentro = 1);

#endif
