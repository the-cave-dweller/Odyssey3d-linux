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
   (518 x 518): por fila, la diferencia media entre las dos mitades despues de buscar, trozo a trozo,
   el corrimiento que mejor las casa (hasta un 12 % del ancho de cada mitad desde la v0.90: los
   videos con mucho relieve separan sus dos imagenes hasta 6 cm, y con el 3 % de antes un objeto muy
   cercano se tomaba por barra y la franja se quedaba puesta). El umbral se pone contra lo que miden
   las filas del centro, que son video seguro: si ahi tampoco casan, el video no viene dividido y no
   se toca nada.

   Lo que se ve en esas filas lo pone el sombreador (franjaBarra en weave.frag, v0.90): el video
   sigue, en 2D, suave y atenuado, y encima van los mandos del reproductor recortados del cuadro. */
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
    int fuertesAbajo = 0, fuertesArriba = 0;   /* y cuantas pasan el doble del umbral */
    bool dividido = true;     /* falso: las mitades no casan ni en el centro (no es video dividido) */
  };

  /* `rgba` es el cuadro entero reducido a w x h (primera fila arriba); `ahora` en segundos.
     Devuelve las filas que hay que tejer en 2D en este momento. Tiene memoria, y poner la banda
     pide mas que mantenerla (v0.82):
       - para PONERLA, dos analisis que la vean en medio segundo, cada uno con 4 filas pasando el
         umbral o 2 pasando el doble;
       - para MANTENERLA basta con 2 filas pasando el umbral dentro de la banda (o una pasando el
         doble), y se quita a los 0,6 s de no verlas.
     MEDIDO con el reproductor de YouTube (sesion del 25/11, 15:30): al ponerse la banda solo 4 a 6
     filas pasaban el umbral (la barra de progreso; los botones, de trazo fino, no llegan), o sea
     justo el minimo, y en 30 s la banda se quito y se volvio a poner dos veces en menos de un
     segundo (0,3 s y 0,8 s). */
  Filas2D analizar(const uint8_t *rgba, int w, int h, double ahora, Medida *m = nullptr);
  void olvidar();
  Filas2D actual() const { return puesta_; }

 private:
  struct Zona {
    double previa = -1.0;     /* el analisis anterior que la vio entera (para ponerla hacen falta dos) */
    double ultima = -1.0;     /* cuando se vio por ultima vez, aunque fuera lo justo para mantenerla */
    float extension = 0.0f;   /* hasta donde llega (se queda con la mayor mientras dure) */
    bool puesta = false;
  };
  Zona abajo_, arriba_;
  Filas2D puesta_;
};

/* Cuanto separa el propio video sus dos imagenes (v0.83). Es lo que decide si un video dividido se
   puede ver tal cual: en este panel el conversor de video normal no separa nunca mas de unos 4 mm, y
   un objeto que el video trae a 4 cm se ve doble (MEDIDO por Rodolfo el 25/11: 2..3 mm en lo normal,
   casi 4 cm en lo muy cercano). Se mide casando bloques de la imagen izquierda contra la derecha
   sobre la copia chica del cuadro; las cifras van en FRACCION DEL ANCHO de la imagen (que en
   pantalla es el ancho del panel), positivas delante de la pantalla y negativas detras. */
struct Separacion {
  bool valida = false;     /* falso: no hubo bloques con dibujo suficiente que casaran */
  float normal = 0.0f;     /* la mediana: por donde anda la escena */
  float cerca = 0.0f;      /* lo mas cercano: el tercer bloque por arriba (perdona dos casados en falso) */
  float lejos = 0.0f;      /* lo mas lejano: el tercero por abajo */
  int bloques = 0;         /* cuantos bloques entraron en la cuenta */
};
Separacion medirSeparacion(const uint8_t *rgba, int w, int h);

/* El residuo de cada fila (0..255): diferencia media entre la mitad izquierda y la derecha con el
   mejor corrimiento. Las filas que no se piden salen a -1. `cada` > 1 mide una fila de cada tantas
   en el centro (alli solo hace falta la mediana). Expuesto para la prueba. */
std::vector<float> residuoPorFila(const uint8_t *rgba, int w, int h, int cadaCentro = 1);

#endif
