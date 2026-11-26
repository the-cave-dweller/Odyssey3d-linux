/* profjuego.h — la profundidad del juego, la que recibe DLSS, leida de la memoria compartida que escribe
   la capa de Vulkan de OdysseyVK dentro del juego (v1.13, fase 2; ver capa/protocolo.h).

   Rodolfo: «I actually want my users to be able to use this without neural forge, just with the game
   dlss itself». DepthNet la usa en lugar de la red cuando el visor esta sobre un juego y llega fresca:
   es exacta (la geometria del juego), no parpadea y no cuesta la red. Si deja de llegar (el juego
   cerro, la capa no esta, otro juego sin DLSS), se vuelve a la red sola. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class ProfundidadJuego {
public:
  ProfundidadJuego() = default;
  ~ProfundidadJuego();
  ProfundidadJuego(const ProfundidadJuego &) = delete;
  ProfundidadJuego &operator=(const ProfundidadJuego &) = delete;

  /* El ultimo cuadro entero, como cercania 0..1 (1 = cerca, el cielo a 0) en una rejilla de lado x lado
     (la del mapa de la red: el cuadro aplastado a un cuadrado, como la copia chica). Falso si no hay
     juego escribiendo, si lo ultimo tiene mas de medio segundo o si el cuadro salio a medio escribir.
     Cada llamada le dice a la capa que alguien lee (su latido): sin eso, la capa no copia. */
  bool leer(std::vector<float> &cercania, int lado);
  /* lo que se leyo la ultima vez, para el registro: «1920x1080 D32_SFLOAT, Z invertida» */
  const std::string &descripcion() const { return desc_; }
  /* (fase 3c) la mascara del HUD del juego: el alfa de su capa del HUD reducida a w x h (960 x 540, el cuadro
     entero; 0 = nada de HUD); si el cuadro tuvo dos capas (un menu encima), la union. Falso si la capa no la escribe (otro juego, ODYSSEYVK_HUD=0), si lo ultimo tiene
     mas de medio segundo o si salio a medio escribir. Despues de leer(), en el mismo hilo. */
  bool leerHud(std::vector<uint8_t> &alfa, int &w, int &h);
  /* el tamano de la imagen del HUD del juego, para el registro */
  void fuenteHud(int &w, int &h) const;

private:
  bool abrir();
  void cerrar();
  int fd_ = -1;
  uint8_t *base_ = nullptr;
  size_t tam_ = 0;
  uint64_t inodo_ = 0;
  uint64_t ultimoSeq_ = 0;
  double cuandoCambio_ = 0.0, ultimoIntento_ = -10.0, avisoDescarte_ = -10.0;
  uint64_t descartados_ = 0;   /* cuadros tirados por no ser una profundidad (ver leer) */
  uint64_t hudSeq_ = 0;
  double hudCambio_ = 0.0;
  std::vector<uint8_t> crudo_;
  std::vector<float> fila_;
  std::string desc_;
};
