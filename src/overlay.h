/* overlay.h — la interfaz del hub puesta POR ENCIMA de un vídeo a pantalla completa.
   Wayland no tiene una orden de "mantener esta ventana arriba": una ventana normal
   siempre la tapa otra que esté a pantalla completa, y las notificaciones de KDE se
   quedan detrás del vídeo (o acaban archivadas en «no molestar»), así que no sirven para elegir el
   3D. Este fichero usa el protocolo wlr-layer-shell, que sí permite una capa «overlay»
   por encima de todo: la tarjeta de aviso se ve y se puede pinchar con el ratón, y no
   roba el teclado (el reproductor sigue mandando con las teclas).
   Se dibuja en la CPU con la misma geometría, las mismas fuentes y los mismos colores
   que la interfaz Vulkan (clases de vkui.h), así que es idéntico a la pastilla del hub. */
#ifndef ODYSSEY_OVERLAY_H
#define ODYSSEY_OVERLAY_H

#include <cstdint>
#include <memory>

#include "vkui.h"
#include <string>
#include <utility>
#include <vector>

class Overlay {
public:
  Overlay();
  ~Overlay();
  Overlay(const Overlay &) = delete;
  Overlay &operator=(const Overlay &) = delete;

  /* Conecta con el compositor y enlaza la capa superpuesta. Devuelve false si el
     compositor no ofrece wlr-layer-shell (en ese caso el servicio seguirá avisando
     por el centro de notificaciones). */
  bool open(std::string *err = nullptr);
  bool available() const;

  /* Descriptor para poll() y forma de mover la máquina de eventos. */
  int fd() const;
  int dispatchPending();
  int waitAndDispatch(int timeoutMs);

  /* Muestra la tarjeta de aviso (abajo a la derecha, como la del hub). `botones` son
     pares «clave, texto»: las claves reconocidas son sbs, tb, flat y no. */
  void showAsk(const std::string &titulo, const std::string &cuerpo);
  void hideAsk();
  bool askVisible() const;

  /* estado de la casilla "guardar el tipo y no volver a preguntar" */
  bool remember() const;
  void setRemember(bool v);

  /* Acción pulsada con el ratón desde la última llamada (vacía si nadie pulsó):
     "sbs", "tb", "flat" o "close". */
  std::string takeAction();

  /* ---- el panel de ajustes (v0.84) --------------------------------------------------------
     La misma capa, pero en el centro de la pantalla y con el panel flotante en vez de la tarjeta.
     Quien lo ensena rellena las filas en `hud().state()` (src/preferencias.cpp), lo pone con
     `showPanel` y va recogiendo los golpes del raton con `takeGolpe`. `grande` lo dibuja al
     doble: es para cuando hay un video en 3D debajo y el panel se ve a traves de la lente. */
  /* el clic en el velo, fuera del panel (rama experimental): quien ensena el panel lo cierra y el
     juego de debajo recupera el control */
  static constexpr int kClicFuera = -100;
  struct Golpe {
    int tipo = 0;          /* un Hud::Hit: HitPanelCerrar, HitPanelPestana o HitPanelItem (o kClicFuera) */
    int item = -1, sub = 0;
    float frac = 0.0f;     /* por donde va el deslizador */
    bool soltado = true;   /* falso mientras se arrastra un deslizador */
  };
  Hud &hud();
  /* `velo` (rama experimental): con el panel sobre un juego, debajo del panel va una capa transparente
     que cubre la pantalla entera. Se queda con el raton mientras el panel esta abierto: el juego no ve
     movimiento ni clics y suelta el cursor; un clic en ella llega como `kClicFuera`. */
  void showPanel(bool grande, bool velo = false);
  void repaint();
  bool takeGolpe(Golpe &g);
  /* El teclado (v1.09): con `tomarTeclado(true)` la capa se queda con todo el teclado mientras este
     puesta (el juego o el video de debajo no reciben teclas). `takeTecla` da cada combinacion
     pulsada, escrita como los atajos ("Ctrl+Shift+K", "Alt+F5") o "Escape". */
  void tomarTeclado(bool si);
  /* 0 = sin teclado · 1 = exclusivo · 2 = a pedido (rama experimental: el panel fijado encima toma el
     teclado solo cuando se le pica; un clic en el juego se lo devuelve). `tecladoAPedido` dice si el
     compositor ofrece el 2 (layer-shell v4); si no, el 2 se queda sin teclado. */
  void modoTeclado(int modo);
  bool tecladoAPedido() const;
  /* quita el velo de debajo del panel (al fijarlo encima: los clics fuera llegan al juego) */
  void quitarVelo();
  /* (v1.18) El velo solo, sin tarjeta ni panel: lo usa el visor para quitarle el raton al juego sin hacerse la ventana
     activa (Rodolfo: «no puedo usar las settings mientras el juego anda, el juego se pausa y quita foco»). Una capa no
     toma el foco: el juego sigue siendo la activa, con su teclado, y no se pausa ni se minimiza. Lo que el raton hace
     sobre el velo sale crudo por `takeRaton`, en puntos logicos de la pantalla */
  struct Raton {
    int que = 0;      /* 0 = se mueve · 1 = aprieta · -1 = suelta · 2 = entra · -2 = sale */
    int boton = 0;    /* 0 = izquierdo · 1 = derecho · 2 = otro */
    double x = 0.0, y = 0.0;
  };
  bool ponSoloVelo();
  bool veloPuesto() const;
  bool ratonEnVelo() const;
  bool takeRaton(Raton &r);
  /* para probarlo sin mover el raton: lo mete en la misma cola, como si hubiera llegado del compositor */
  void ratonPruebaVelo(const Raton &r);
  bool takeTecla(std::string &combo);
  /* (v1.15) y lo que escribe esa pulsacion (UTF-8, ya con las teclas muertas; vacio si no escribe): los campos de
     texto. La combinacion sale vacia para una tecla que solo escribe (la ñ, una letra con acento); Retroceso, Intro
     y Tab van como "Backspace", "Return" y "Tab". La tecla que se deja pulsada se repite */
  bool takeTecla(std::string &combo, std::string &texto);
  /* (v1.15) `odysseyvk ctl teclado [es]`: sin compositor, que da cada pulsacion en esa distribucion */
  static int pruebaTeclado(const char *distribucion);

  /* Vuelca la tarjeta a un archivo PNG (para comprobar el dibujo sin el compositor). */
  bool dumpPng(const std::string &ruta, std::string *err = nullptr);
  /* (v1.16) para el guion del panel: un clic del boton izquierdo en (x, y) del lienzo virtual, por el mismo camino que
     uno del raton de verdad (el que decide que golpes llegan al bucle del panel) */
  void clicPrueba(float x, float y);
  /* (v1.16) para probar el servicio: contestar la tarjeta como si se hubiera picado (`3d`, `2d` o `close`) */
  void contestaPrueba(const std::string &accion);
  /* (v1.16) y para arrastrar (un deslizador): apretar en (x, y), mover a (x, y) con el boton apretado, soltar */
  void ratonPrueba(int que, float x, float y);   /* que: 1 = apretar, 0 = mover, -1 = soltar */

  /* Preferencia de pantalla para la tarjeta (nombre o modelo, sin distinguir
     mayúsculas; por ejemplo "DP-4"). Sin esto se elige solo, y por defecto se buscan
     las palabras odyssey / syncmaster / g90 en el monitor que anuncia el compositor.
     Vale también la variable de entorno ODYSSEYVK_PANTALLA. */
  void setPantalla(const std::string &pista);
  /* Cierto si la pantalla elegida se reconoció como el panel del Odyssey (o la pediste
     tú a propósito). Si es false, avisa: puede que la tarjeta caiga en otro monitor. */
  bool pantallaReconocida() const;

  /* Cierto si el compositor nos cerró la conexión de la capa (algo salió mal con el
     tamaño o con el lienzo). Desde ahi la tarjeta no vuelve a aparecer: reiniciar el
     servicio la recupera. */
  bool conexionCaida() const;
  /* Una entrada en la barra de tareas mientras el panel esta abierto (v1.04). Las capas (layer-shell)
     nunca salen en el gestor de tareas de KDE, asi que se pone al lado una ventana normal de 1x1
     transparente, con el nombre y el icono de OdysseyVK y sin entrada de raton: lo que se ve es su
     boton en la barra. Cerrarla desde la barra cierra el panel (`entradaCerrada`). */
  void ponEntradaBarra(const std::string &titulo);
  void quitaEntradaBarra();
  bool entradaCerrada() const;

  /* Comprueba si el compositor puso la capa en la pantalla pedida; si no, la rehace
     (como mucho dos veces) y deja el motivo en `lastError()`. Devuelve true si reaccionó. */
  bool corrigeColocacion();
  /* true una sola vez cada vez que `corrigeColocacion()` detecta algo, para avisar. */
  bool hayAvisoColocacion();

  /* Todas las pantallas que anunció el compositor, para elegir `pantalla=`. */
  std::vector<std::string> pantallas() const;

  /* Monitor donde se colocó la tarjeta y su escala, para los mensajes. */
  std::string outputName() const;
  double scale() const;

  const std::string &lastError() const { return err_; }

private:
  struct Impl;
  std::unique_ptr<Impl> d;
  std::string err_;
};

#endif
