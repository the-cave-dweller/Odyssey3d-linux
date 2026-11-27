/* vkui.h — la interfaz del hub: la pastilla gris de arriba a la derecha con el interruptor de
   3D (ON/OFF), el boton de la red y los dos controles (profundidad y salida), mas el aviso abajo a
   la derecha que pregunta como ver la ventana a pantalla completa. Todo se dibuja con
   cuadros sobre Vulkan (atlas R8 con fuentes y mascaras) y se maneja con el raton y los
   atajos de la aplicacion oficial. No toca Vulkan: solo geometria, texto y entradas.

   Desde v0.56 la pastilla es RECTA: sin esquinas redondeadas en el panel, los botones, las cajas
   del Facetracking ni las tarjetas del aviso. Unicas curvas: el propio interruptor ON/OFF (asi lo
   pidio Rodolfo, con su bola) y las cabecitas de los deslizadores. La fila de ocultar la pastilla
   ya no existe como boton: se oculta y se muestra con Ctrl+Shift+F1 (o con `odysseyvk ctl capa`,
   que es la misma orden por fichero y sirve cuando el teclado lo tiene otra ventana).

   Desde v0.78 la pastilla es UNA fila (interruptor, dos deslizadores y la casilla de «no volver a
   preguntar») con las teclas en un renglon, y la tarjeta de la orilla se parece a la de la
   aplicacion oficial: redonda, con dos teselas ilustradas. La pastilla sigue recta. */
#ifndef ODYSSEY_VKUI_H
#define ODYSSEY_VKUI_H

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct HudVertex {
  float x, y;   /* píxeles, (0,0) arriba a la izquierda */
  float u, v;   /* coordenada dentro del atlas */
  float r, g, b, a;
};

class Hud {
public:
  /* Una fila del panel de ajustes (v0.84). El panel no sabe de ajustes: pinta las filas que le den y
     dice cual se pico; que hace cada una lo decide quien lo ensena (src/preferencias.cpp). */
  struct ItemPanel {
    /* Boton (v0.92): rotulo y nota a la izquierda, un boton a la derecha con `valorTexto`.
       Info (v0.92): una «i» en un anillo, el rotulo y, debajo, la nota. */
    /* Atajo (v1.09): el rotulo y, a la derecha, la combinacion en una tecla; `valor` = esperando la
       combinacion nueva; `quitar` = no es la de fabrica (sale la flecha de volver a ella). */
    /* Texto (v1.15): un campo para escribir (la pestana Integraciones): el rotulo a la izquierda y la caja a la
       derecha con `valorTexto`; vacio, la caja ensena `nota` en gris. `cursor` >= 0 = escribiendo, con el cursor en
       ese byte; `seleccion` = todo marcado (lo que se escriba lo reemplaza).
       Linea (v1.15): un texto largo partido en renglones en una caja oscura (la linea que va a Steam), con un boton
       a la derecha que dice `nota` (Copiar; `valor` = recien copiado, en verde); vacio, sale `rotulo` en gris. */
    /* Desplegable (v1.16): una caja con lo elegido (`valorTexto`) y su flechita (el rotulo, el de la seccion de encima); picada, debajo cuelga la
       lista de `opciones`, cada una con su etiqueta a la derecha (`ayudas[k]`) en azul (`estadosOpc[k]` 0: esta) o en
       gris (1: se baja al picarla; 2: no se puede ahora). `elegido`, la puesta (-1 = ninguna) */
    enum Tipo { Seccion, Interruptor, Opciones, Deslizador, Fila, Juego, Nota, Boton, Info, Atajo, Texto, Linea, Desplegable };
    Tipo tipo = Nota;
    std::string rotulo, nota;
    std::string valorTexto;          /* a la derecha: el numero del deslizador, el modo de una fila... */
    bool valor = false;              /* interruptor */
    int elegido = 0;                 /* opciones */
    std::vector<std::string> opciones;
    float frac = 0.0f;               /* deslizador, 0..1 */
    /* (v1.16, de la PR #1 de Noa3) el valor de un deslizador para una fraccion: es el que se ve mientras se arrastra (el
       numero de la derecha sigue a la bola); `fracDefecto` >= 0 pone a la izquierda de la pista una flechita para volver
       a esa fraccion */
    std::string (*textoFraccion)(float) = nullptr;
    float fracDefecto = -1.0f;
    bool quitar = false;             /* fila con aspa para quitarla */
    bool peligro = false;            /* boton en rojo suave: desinstalar (v1.06) */
    /* una seccion que se pliega (rama experimental): la cabecera lleva una flechita y se pica entera;
       `valor` = desplegada (las filas de debajo solo se ponen si lo esta) */
    bool plegable = false;
    /* la ayuda al pasar el raton (rama experimental): `ayuda` para la fila entera y, en las opciones,
       `ayudas[k]` para cada una (si la hay, manda sobre `ayuda`) */
    std::string ayuda;
    std::vector<std::string> ayudas;
    /* la caratula de un juego (RGBA, imgW x imgH). El atlas de la interfaz es de un solo canal, asi
       que las imagenes van aparte y solo las pinta el pintado de CPU (el del panel). */
    std::shared_ptr<const std::vector<uint8_t>> img;
    int imgW = 0, imgH = 0;
    int cursor = -1;          /* Texto: escribiendo, donde esta el cursor (en bytes) */
    bool seleccion = false;   /* Texto: todo marcado */
    /* (v1.15) en las integraciones de un juego: este valor es suyo, no el de todos (un punto del acento delante) */
    bool propio = false;
    /* (v1.15) un interruptor que no se puede encender ahora (lo que no esta instalado, o lo que es uno u otro): sale
       en gris y no se pica, salvo para apagarlo si ya estaba puesto; `pastilla`, a la izquierda del interruptor
       («No instalada») */
    bool apagado = false;
    std::string pastilla;
    /* (v1.16) una caratula de Juegos en gris (dentro de un juego, las que no son ese): sin color y sin picarse */
    bool gris = false;
    /* (v1.16) las redes de profundidad: `descargable` = en gris como `apagado`, pero se pica (para bajarla);
       `pastillaAcento` = la pastilla en el azul del acento («Descargada») en vez de gris */
    bool descargable = false;
    bool pastillaAcento = false;
    /* (v1.16) la mitad del ancho: 0 = entera, 1 = la izquierda, 2 = la derecha (las seguidas van lado a lado; con la vista
       de depuracion, arriba y abajo de la columna). `centrado`: la casilla y su rotulo en medio de la fila */
    int lado = 0;
    bool centrado = false;
    std::vector<int> estadosOpc;   /* Desplegable */
  };

  struct State {
    bool overlayOn = true;
    bool weaveOn = true;
    int depth = 50;    /* "3D Depth" oficial: 0..100 · 50 = sin cambio */
    int popout = 0;    /* "3D Popout" oficial: -50..+50 */
    bool ask = false;  /* aviso de conversión (abajo a la derecha) */
    /* con esto puesto, la tarjeta de la orilla no pregunta por el video: avisa de que hay esta
       version nueva en GitHub, con Actualizar, Luego y «no avisar» (v0.95) */
    std::string askActualizacion;
    /* con esto puesto, la tarjeta es un aviso de texto con un boton «Entendido» (v1.00): el titulo
       y hasta tres renglones */
    std::string askAviso;
    std::vector<std::string> askAvisoLineas;
    /* (v1.12) los segundos que le quedan al aviso antes de cerrarse solo; > 0 los ensena en el boton,
       «Entendido (3)». Los cuenta el servicio */
    int askAvisoCuenta = 0;
    bool remember = false;
    /* la misma casilla, pero dentro del video (en la pastilla): `recordarVisible` la ensena y
       `recordar` dice si esta aplicacion pasa directa al 3D sin preguntar. La pone el visor cuando
       lo lanzo el servicio, que es quien guarda la eleccion en serve.conf. */
    bool recordarVisible = false;
    bool recordar = false;
    /* los botones de icono de la pastilla (v0.79): que tipo de 3D se esta haciendo y poder
       cambiarlo en marcha. 1 = video normal (el conversor), 2 = video dividido (lado a lado, con el
       relieve que trae), 3 = video dividido suavizado (v0.83: una de sus dos imagenes y el relieve
       del conversor). */
    bool modoVisible = false;
    int modo3d = 1;
    /* v0.84: la pastilla lleva solo DOS botones, video normal y video dividido. El dividido es el
       suavizado salvo que en los ajustes del programa se haya quitado: `suavizado` dice cual de los
       dos dibujos lleva ese boton (la imagen con la raya a trazos, o las dos imagenes separadas). */
    bool suavizado = true;
    /* el engranaje de la pastilla, que abre el panel de ajustes (solo lo pone el visor) */
    bool engranaje = false;
    /* ---- el panel de ajustes flotante (v0.84) ---- */
    bool panel = false;
    int pestana = 0;
    std::vector<std::string> pestanas;
    /* (v1.16) el panel al estilo de vkDaedalus (Rodolfo: «la interfaz del resto siga un estilo como el de vkdaedalus
       ... misma escala tambien, para que se sientan consistentes»): a pantalla completa con un velo, la barra de
       arriba, una fila de teselas y la tarjeta de la pestana debajo. `marco` = false, el panel flotante de antes.
       Las pestanas son las teselas grandes del centro, con `pestanasNota` debajo (lo que tiene cada una: «13
       juegos»); `teselas`, las chicas de los lados (tres a la izquierda y tres a la derecha): un rotulo, lo que
       vale y si esta puesto; al picarla se aplica su `id` como una fila. `aparicion` va de 0 a 1 al abrirse: la
       interfaz sube desde la pastilla y se hace opaca */
    bool marco = true;
    /* (v1.16) en el escritorio el marco va en una ventana que se mueve por su barra (Rodolfo: «en la vista de escritorio
       hazlo movible, y con boton de cierre»); sobre un juego o el 3D, a pantalla completa */
    bool marcoVentana = false;
    /* (v1.16) abierto desde la pastilla (su engranaje): junto al aspa, la flecha de volver a ella (Rodolfo: «beside the
       closing button after accessing the overlay from the pill add a back button to just return to the pill») */
    bool desdePastilla = false;
    /* (v1.16) las pestanas que se ven pero no se pueden abrir (Integraciones dentro de un juego): en gris */
    std::vector<bool> pestanaGris;
    std::vector<std::string> pestanasNota;
    struct Tesela {
      std::string id, titulo, valor;
      bool puesta = false;   /* el valor, en el acento */
      int icono = 0;         /* ver Hud::iconoTesela */
      bool menu = false;     /* (v1.17) con una flechita que abre su menu (el cuadrito de los FPS) */
    };
    std::vector<Tesela> teselas;
    float aparicion = 1.0f;
    std::vector<ItemPanel> items;
    /* (v1.16, PR #1) la fila del deslizador mientras se tiene apretado: aqui (antes en la capa) para que el panel, al
       rehacer la lista cada segundo, la conserve con su bola donde esta */
    int arrastrando = -1;
    /* (v1.16) el desplegable abierto (su fila, -1 = ninguno) y la vista de depuracion del panel: solo las dos listas de
       redes en una columna a la izquierda, para probarlas en vivo con la profundidad a la vista (Rodolfo) */
    int desplegado = -1;
    bool depuracion = false;
    /* (v1.17) el menu abierto de una tesela (su numero, -1 = ninguno), sus opciones y cuales van marcadas */
    int teselaMenu = -1;
    std::vector<std::string> menuOpciones;
    std::vector<bool> menuMarcadas;
    float corrida = 0.0f;            /* cuanto se ha corrido la lista hacia arriba, en puntos del lienzo */
    std::string pie;                 /* la frase del pie: lo ultimo que se cambio */
    std::string version;             /* "OdysseyVK 0.84", abajo a la derecha */
    /* la actualizacion (v0.95): `actualizacion` = version nueva que hay en GitHub (vacia = ninguna);
       `alDia` = ya se comprobo y es la ultima. Pintan el boton junto al aspa: amarillo con la
       flecha si hay una pendiente, flecha verde si esta al dia, gris si no se ha mirado. */
    std::string actualizacion;
    bool alDia = false;
    /* el boton del idioma (v1.07), a la izquierda del de la actualizacion: abierto, cuelga debajo la
       lista de los siete idiomas */
    bool menuIdioma = false;
    /* el pin a la izquierda del nombre (rama experimental): fijado, el panel se queda encima aunque se
       pique el juego (sin velo y con el teclado a pedido) */
    bool fijado = false;
    /* el panel de ajustes (rama experimental): su acento, el color de resaltado de KDE (de fabrica el
       morado de siempre), y su tamano en puntos del lienzo (0 = el de fabrica, 820 x 600), que se cambia
       arrastrando la esquina de abajo a la derecha */
    float acentoPanel[3] = {0.46f, 0.37f, 0.92f};
    float panelAncho = 0.0f, panelAlto = 0.0f;
    /* el interruptor AI | Rendimiento junto al logo (rama experimental): el video normal con la red (AI) o
       sin ella (modo rendimiento) */
    bool sinRedPanel = false;
    /* el renglon de las teclas de la pastilla cuando hay atajos cambiados (v1.09); vacio = el de fabrica */
    std::string teclas;
    /* la pastilla del modo rendimiento (rama experimental): el conversor va sin red. Lleva el acento
       ambar y, en vez de Profundidad, Salida y los botones de tipo de video, un boton con el rayo que
       despliega el menu del modo: los tres preajustes y los cuatro deslizadores finos.
       `preajuste` = el puesto (0 suave, 1 medio, 2 intenso; -1 = valores a mano); `rendValor` =
       mezcla, fuerza, alcance y paralaje (los rangos, en rendimiento.h); `menuRendimiento` = abierto.
       `teclasRendimiento` es el renglon de las teclas con atajos cambiados (vacio = el de fabrica). */
    bool rendimiento = false;
    int preajuste = -1;
    bool menuRendimiento = false;
    /* el Foco automatico del modo normal (rama experimental): el boton del chip «AI» de la pastilla.
       `focoVisible` lo ensena (conversor con la red); `focoAuto` = puesto: el plano de la pantalla lo
       elige la saliencia en cada escena y la Profundidad se esconde. `teclasFoco` es el renglon de las
       teclas con atajos cambiados mientras esta puesto (vacio = el de fabrica). */
    bool focoVisible = false;
    bool focoAuto = false;
    /* el hub del modo normal (rama experimental): el boton del chip abre su menu (`menuNormal`), con
       Auto | Manual; en Auto el menu ensena el plano y la Salida que se calcularon (`focoPlano` 0..100,
       `focoSalida` en unidades del deslizador) */
    bool menuNormal = false;
    int focoPlano = 0, focoSalida = 0, focoAlcance = 100;   /* el alcance en centesimas (x2,88 = 288) */
    int focoRelieve = 0;   /* el relieve entre los ojos que da Auto, en px (cuantiles 2..98 %, con los topes) */
    float rendValor[4] = {0.35f, 1.0f, 2.75f, 2.0f};
    std::string teclasRendimiento;
    int hud = 0;       /* 3 = lente y scaler listos · 0 = ninguno */
    int red = 2;       /* que red de profundidad esta puesta: 1 = la de Samsung, 2 = la nativa */
    bool ratonLibre = false;  /* el raton atraviesa la ventana fuera de la pastilla */
    bool redViva = false;  /* hay conversor abierto (si no, el boton de la red se apaga) */
    /* Modo de legado: la pastilla se queda solo con lo que ese camino puede tocar (el 3D, la salida
       y el plano de apoyo). Se cae el boton de la red, el deslizador de la profundidad del conversor y
       los avisos de las teclas que alli no existen. La pastilla mide 648 px en vez de 824. */
    bool legado = false;
    /* La fila de abajo de la pastilla: Facetracking. Solo la enseña el visor (el hub no
       rastrea), asi que la pastilla del hub no crece. No es un boton: el rastreo no se ofrece
       a encender ni a apagar, la pastilla dice con que camara se esta siguiendo la trama. */
    bool controlOjo = false;
    bool rastreoVivo = false;    /* hay hilo de camara abierto (si no, la fila sale apagada) */
    bool rastreoEstudio = false; /* la camara es la integrada del monitor: "3D Studio" */
    bool rastreoCara = false;    /* en el ultimo cuadro habia una cara delante */
    std::string rastreoNota;     /* lo que la camara reporta: "sin cara", "sin pose valida"... */
    std::string rastreoCamara;   /* lo que anuncia el conductor: "3D Stereo WebCam" */
    /* (v1.13) el cuadrito a la izquierda de la pastilla con los FPS del juego. `fpsModo` 1 = con DLSS: «DLSS», los
       del juego y los de la capa (las profundidades que manda); 2 = sin DLSS, con la red sobre un juego: «Modelo
       de serie», los cuadros nuevos del juego que llegan y los mapas de la red (en `fpsCapa`); 0 = no sale.
       `fpsCuadro` es el ajuste */
    float fpsJuego = 0.0f, fpsCapa = 0.0f;
    int fpsModo = 0;
    bool fpsCuadro = true;
    /* (v1.17) lo que se eligio vigilar en el menu de la tesela del cuadrito: una fila por cosa, con sus valores (el
       numero y su unidad). Con alguna, el cuadrito sale tambien sin juego (sobre un video), como «Monitor» */
    struct FilaMonitor {
      std::string rotulo;
      std::vector<std::pair<std::string, std::string>> valores;
    };
    std::vector<FilaMonitor> monitor;
    /* (v1.18) la generacion de cuadros de MAKO desde el cuadrito: el clic derecho abre su menu (`menuFg`), como el de
       vkDaedalus (Rodolfo: «que click derecho en el contador de fps muestre opciones de generacion de frames, haz lo
       mismo que vkdaedalus»). Lo que dice el perfil del juego en el conf.toml de MAKO (ver mako.h) */
    bool menuFg = false;
    struct Fg {
      bool mako = false;          /* MAKO esta en el equipo */
      bool generador = false;     /* su modelo propio, MAKO FG (mako-fg-host): el de MAKO o el que instalo OdysseyVK */
      bool generadorJuego = true; /* y el juego que corre lo recibio (su MAKO_FG_HOST, o MAKO tiene el suyo) */
      /* (v1.18) su instalacion desde el menu (Rodolfo: «offer an install button»): el boton sale sin MAKO FG */
      bool instalando = false;
      float avance = 0.0f;        /* 0 a 1 */
      long megas = 19;            /* lo que baja, para el boton */
      bool lsfg = false;          /* y el de Lossless Scaling (su Lossless.dll en Steam) */
      int modelo = 0;             /* el del perfil: 0 = LSFG (lossless-scaling), 1 = MAKO FG (mako-fg) */
      bool enJuego = false;       /* y su capa, cargada en el juego */
      std::string exe;            /* el ejecutable del juego; vacio = el 3D no esta sobre un juego */
      bool puesta = false;
      int multiplicador = 2;
      int flujo = 85;             /* % */
      bool rendimiento = false;
      std::string aviso;          /* el pie: lo que paso al tocar, o por que no se puede */
      float enPantalla = 0.0f;    /* los cuadros que llegan al visor por segundo, generados incluidos */
    } fg;
    /* (v1.14) en la pestana Juegos, en medio del pie del panel: el parametro de Steam para la profundidad de DLSS y
       su boton Copiar (Rodolfo: «el parametro ahi, por el centro»). `pieParametro` vacio = no sale. Recien copiado
       (`pieCopiado`), el boton dice `pieCopiadoTexto` un momento; `pieAyuda`, al pasar el raton */
    std::string pieParametro, pieCopiar, pieCopiadoTexto, pieAyuda;
    bool pieCopiado = false;
    /* (v1.16) en vez del boton Copiar, una casilla (`pieValor`) con `pieParametro` al lado: ODYSSEYVK=1 para todos los
       juegos, en el pie de Juegos; su recordatorio (`pieAyuda`) solo al pasar el raton (Rodolfo: «put a reminder to
       enable the option either per game or globally for DLSS depth, show it only after hovering, place the option
       there for the global setting») */
    bool pieInterruptor = false, pieValor = false;
  };

  enum Hit {
    HitNone = 0,
    Hit3D,             /* el interruptor ON/OFF del 3D */
    HitDepth,
    HitPopout,
    HitRed,
    HitAskSbs,
    HitAskTop,
    HitAskFlat,
    HitAskConv,
    HitAskDont,
    HitAskClose,
    HitAskAyuda,       /* el "?" de la tarjeta: no manda nada, solo cambia el texto al pasar por encima */
    HitAskActualizar,  /* la tarjeta de la actualizacion (v0.95): instalar, luego, no avisar mas */
    HitAskLuego,
    HitAskNoAvisar,
    HitRecordar,       /* la casilla de «no volver a preguntar» de la pastilla */
    HitModoNormal,     /* los dos botones de icono de la pastilla: video normal ... */
    HitModoDividido,   /* ... video dividido ... */
    HitModoSuave,      /* ... y video dividido suavizado (v0.83) */
    HitEngranaje,      /* el engranaje de la pastilla: abre el panel de ajustes */
    HitPreajuste1,     /* los tres preajustes del menu del rayo (rama experimental) */
    HitPreajuste2,
    HitPreajuste3,
    HitRendimientoMenu,   /* el boton del rayo: abre y cierra su menu */
    HitFocoAuto,          /* el boton del chip «AI» del modo normal: abre y cierra su menu */
    HitNormalAuto,        /* su menu: Auto (plano y Salida automaticos) ... */
    HitNormalManual,      /* ... o Manual (Profundidad y Salida a mano) */
    HitRendMezcla,        /* los cuatro deslizadores del menu del rayo */
    HitRendFuerza,
    HitRendAlcance,
    HitRendParalaje,
    HitRendFondo,         /* dentro del menu del rayo, sobre nada que se pique */
    HitPanel,          /* dentro del panel de ajustes, sobre nada que se pique */
    HitPanelCerrar,
    HitPanelActualizar,   /* el boton de la actualizacion, junto al aspa (v0.95) */
    HitPanelIdioma,       /* el boton del idioma (v1.07): abre y cierra la lista */
    HitPanelIdiomaOpcion, /* un idioma de la lista: `sub` = cual (idioma::Lengua) */
    HitPanelFijar,        /* el pin junto al nombre: fijar el panel encima (rama experimental) */
    HitPanelRedimensionar, /* la esquina de abajo a la derecha: cambiar el tamano del panel (rama experimental) */
    HitPanelModoRed,      /* el interruptor AI | Rendimiento: `sub` 0 = AI (con la red), 1 = Rendimiento (sin red) */
    HitModoRed,           /* (v1.12) el mismo interruptor, en la pastilla del video: pasa al otro extremo en marcha */
    HitPanelPestana,   /* `sub` = cual */
    HitPanelTitulo,    /* la cabecera: arrastrandola se mueve el panel (v0.86) */
    HitPanelItem,      /* `item` = que fila; `sub` = que opcion (o 1 = el aspa de quitar); `frac` = deslizador */
    HitPanelCopiar,    /* (v1.14) el boton Copiar del pie de la pestana Juegos: el parametro de Steam */
    HitPanelTesela,    /* (v1.16) una tesela chica de los lados: `sub` = cual (State::teselas) */
    HitPanelVolver,    /* (v1.16) la flecha de volver a la pastilla, junto al aspa */
    /* (v1.18) la generacion de cuadros: el cuadrito (su clic derecho abre el menu), el menu, y sus opciones: fila * 8 +
       la opcion, desde HitFgOpcion0. La fila 5 es el boton de instalar MAKO FG */
    HitFgCuadro,
    HitFgFondo,
    HitFgOpcion0,
    HitFgOpcionFin = HitFgOpcion0 + 47
  };

  /* ---- atlas (se construye una vez en CPU y se sube como R8) ---- */
  bool buildAtlas(std::string *err);
  const std::vector<uint8_t> &atlasPixels() const { return atlas_; }
  int atlasW() const { return aw_; }
  int atlasH() const { return ah_; }
  bool hasText() const { return hasText_; }

  /* ---- estado visible ---- */
  State &state() { return st_; }
  const State &state() const { return st_; }
  void setInfo(const std::string &s) { info_ = s; }
  /* texto del aviso: por defecto el de la aplicacion oficial; el servicio pone aqui
     el titulo de la pelicula que esta dando pantalla completa */
  void setAskText(const std::string &titulo, const std::string &cuerpo) {
    askTitulo_ = titulo;
    askCuerpo_ = cuerpo;
  }

  /* ---- cuadro de la superposición y pruebas de golpe de ratón ---- */
  /* altoLogico: alto de la pantalla en puntos logicos (los que ve el escritorio). Lo usa la
     tarjeta del hub, que vive en el escritorio: con el panel al 150% son 1440 y no los 2160
     fisicos. El visor NO lo pasa: cuando el panel entra en el modo 3D se queda con escala 1 y
     alli la pastilla se dibuja en pixeles fisicos, asi que mide lo mismo con el panel en 2 o en 3.
     setScale deja el tamano fijo a mano (Ctrl+Shift+7 / 8 en el visor, --ui-escala al arrancar). */
  void begin(int w, int h, float altoLogico = 0.0f);  /* limpia y calcula la disposición */
  void setScale(float s) { scUser_ = s; }             /* 0 = tamano automatico */
  float scale() const { return sc_; }
  const std::vector<HudVertex> &vertices() const { return v_; }
  int vertexCount() const { return (int)v_.size(); }
  /* de cuanto es la pastilla que se dibujo en el ultimo `begin`: 824 de normal, 648 en modo de legado
     (o menos si la ventana es angosta). Lo mide la prueba de `ctl hud`. */
  float panelW() const { return panelW_; }
  float panelH() const { return panelH_; }
  Hit hit(float mx, float my) const;
  void dragTo(Hit which, float mx);    /* arrastre sobre un control deslizante */
  void toggle3D();
  void toggleOverlay();
  /* la pastilla a la vista sin más (PB, o el raton en el rincon de arriba a la derecha) */
  void showOverlay() { st_.overlayOn = true; }
  void nudgeDepth(int delta);
  void nudgePopout(int delta);
  /* el raton llega en pixeles reales de la ventana y se pasa al espacio virtual */
  void setMouse(float x, float y) { mx_ = x / sc_, my_ = y / sc_; }
  bool interactive() const { return st_.overlayOn || st_.ask; }

  /* Área útil de la ventana: lo que hay debajo de la pastilla sigue dibujando vídeo. */
  bool overlayVisible() const { return st_.overlayOn; }
  /* (v1.17) el cuadrito de los FPS del juego a la vista (sale aunque la pastilla este oculta) */
  bool cuadroFps() const { return st_.fpsCuadro && (st_.fpsModo != 0 || !st_.monitor.empty()); }
  /* (v1.18) donde quedaron el cuadrito y el menu de la generacion de cuadros (pixeles del lienzo), para la region de
     entrada: el clic derecho en el cuadrito tiene que llegar al visor */
  bool cuadroFpsBounds(float *x, float *y, float *w, float *h) const;
  bool menuFgBounds(float *x, float *y, float *w, float *h) const;

  /* Cuadro ocupado por el aviso / la pastilla en la disposicion actual, en pixeles
     reales de la ventana (los mismos que llevan los vertices). Lo usa la superposicion
     de Wayland para recortar su ventana justo a esa esquina. */
  bool askBounds(float *x, float *y, float *w, float *h) const;
  /* lo mismo para el panel de ajustes, y `flotanteBounds` da el que este puesto de los dos (el panel
     manda): es la caja que la capa de Wayland recorta */
  bool panelBounds(float *x, float *y, float *w, float *h) const;
  bool flotanteBounds(float *x, float *y, float *w, float *h) const {
    return st_.panel ? panelBounds(x, y, w, h) : askBounds(x, y, w, h);
  }
  /* El golpe con sus senas: que fila del panel, que parte de ella y por donde va el deslizador. */
  Hit hitDetalle(float mx, float my, int *item, int *sub, float *frac) const;
  /* arrastrando el deslizador de la fila `item`: la fraccion que corresponde a esa x del raton */
  float fracDeslizador(int item, float mx) const;
  /* la rueda del raton sobre la lista del panel (positivo = bajar) */
  void correr(float cuanto);
  /* Cuanto mide una etiqueta con la fuente de la interfaz: para comprobar que el texto de un boton
     cabe en su caja sin mirar la pantalla (con la letra mal medida los rotulos se salian del recuadro). */
  float mideTexto(const std::string &s, float px) const { return textWidth(s, px); }
  /* caja del boton de la red: la usa la depuracion (ODYSSEY_GOLPE=red) para pulsarlo sin raton */
  bool redBounds(float *x, float *y, float *w, float *h) const {
    if (btnRed_.w <= 0.0f) return false;
    if (x) *x = btnRed_.x;
    if (y) *y = btnRed_.y;
    if (w) *w = btnRed_.w;
    if (h) *h = btnRed_.h;
    return true;
  }
  bool pillBounds(float *x, float *y, float *w, float *h) const;
  /* caja de la casilla de la pastilla, en pixeles reales (para la prueba y para ODYSSEY_GOLPE) */
  bool recordarBounds(float *x, float *y, float *w, float *h) const {
    if (chkRecordar_.w <= 0.0f) return false;
    if (x) *x = chkRecordar_.x * sc_;
    if (y) *y = chkRecordar_.y * sc_;
    if (w) *w = chkRecordar_.w * sc_;
    if (h) *h = chkRecordar_.h * sc_;
    return true;
  }
  /* Los vertices del ultimo `begin` pintados en la CPU sobre un lienzo de ancho x alto cuyo origen
     esta en (ox, oy) de la ventana. Sale RGBA en coma flotante, color ya multiplicado por opacidad. */
  void pintarCpu(float ox, float oy, int ancho, int alto, std::vector<float> &acum) const;
  /* La punta derecha de la caja que ocuparia la pastilla, aun estando ocultada. Es el rincon que
     el visor deja pinchable para que el raton pueda devolver la pastilla sin usar el teclado. */
  bool huecoDeLaPastilla(float *x, float *y, float *w, float *h) const;

private:
  struct Glyph {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    int w = 0, h = 0, bx = 0, by = 0, adv = 0;
    bool ok = false;
  };
  struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool inside(float mx, float my) const { return mx >= x && mx <= x + w && my >= y && my <= y + h; }
  };

  void quad(Rect r, Rect uv, float cr, float cg, float cb, float ca);
  void solid(float x, float y, float w, float h, float cr, float cg, float cb, float ca);
  void mask9(Rect r, int which, float radius, float cr, float cg, float cb, float ca);
  void icono(Rect r, const Rect &m, float cr, float cg, float cb, float ca, float fx0 = 0.0f, float fy0 = 0.0f,
             float fx1 = 1.0f, float fy1 = 1.0f);
  float muestra(float u, float v) const;
  /* el paisaje de las teselas y de los botones de la pastilla: `apretado` lo estrecha (cada mitad
     del video dividido), `luz` lo apaga (el boton que no esta puesto) y `radio` > 0 le redondea las
     esquinas tapandolas con el color `fondo` */
  void paisaje(Rect r, float apretado, float luz, float radio, const float *fondo, const float *tinte = nullptr);
  void dibujaPanel(int vw, int vh);
  /* recorte de todo lo que se pinte mientras este puesto (la lista del panel, que corre) */
  Rect recorte_;
  bool hayRecorte_ = false;
  struct ImagenCpu {
    Rect caja, recorte;      /* en puntos del lienzo virtual */
    const uint8_t *px = nullptr;
    int w = 0, h = 0;
    float radio = 0.0f;
    size_t orden = 0;        /* (v1.14) cuantos vertices habia en v_ al ponerla: se pinta en su turno, no encima de todo */
    float alfa = 1.0f;       /* (v1.16) con el panel apareciendo, se hace opaca con el */
    bool gris = false;       /* (v1.16) sin color y mas oscura */
  };
  std::vector<ImagenCpu> imagenes_;
  float text(const std::string &s, float x, float y, float size, float cr, float cg, float cb, float ca,
             bool center = false);
  float textWidth(const std::string &s, float size) const;
  /* como text(), pero sin pasar de `maxAncho`: primero achica la letra hasta un 80 % y, si aun no
     cabe, corta con «…» (v1.07: el frances y el aleman son mas largos que el español) */
  float textoCabe(const std::string &s, float x, float y, float size, float maxAncho, float cr, float cg, float cb,
                  float ca, bool center = false);
  /* un texto partido en renglones de hasta `maxAncho` (las palabras sin espacios, como el chino o el
     japones, se parten por letras): lo usan las cajas de ayuda */
  std::vector<std::string> renglones(const std::string &s, float size, float maxAncho) const;
  /* la caja de ayuda con su texto ya partido, con la esquina de arriba a la izquierda en (x, y) */
  void cajaAyuda(const std::vector<std::string> &lineas, float x, float y, float size);

  /* máscaras del atlas: 0 píxel blanco, 1 píldora, 2 tarjeta */
  Rect white_, pill_, card_;
  /* los dibujos de la tarjeta: aspa, palomita, anillo del "?", el `hueco` que redondea esquinas y
     las dos siluetas del paisaje (todas en pixeles del atlas) */
  Rect icoCerrar_, icoPalomita_, icoAnillo_, hueco_, monteLejos_, monteCerca_, icoEngranaje_, icoFlecha_, icoGlobo_,
      icoRayo_, icoBajar_, icoDerecha_, icoPin_, icoChip_, icoLlave_, icoLogo_, icoEncender_;
  /* (v1.16) los de las teselas al estilo de vkDaedalus: el cubo del 3D, el mando de Juegos, el enchufe de
     Integraciones, los deslizadores de Ajustes, la cabeza del seguimiento, las barras de los FPS y las capas del HUD */
  Rect icoCubo_, icoMando_, icoEnchufe_, icoDeslizadores_, icoCabeza_, icoBarras_, icoCapas_;
  Rect icoAtras_;   /* (v1.16) la flecha de volver a la pastilla: ← con su palo, como la de actualizar */
  const Rect &iconoTesela(int k) const;
  /* (v1.16) la paleta del modo (Rodolfo: «make the rest of the ui follow the yellow and blue tint on the currently
     purple parts, also adapt the background to an almost black but tinted version of each»): con la red, el azul de
     la capsula Neural; en el modo rendimiento (o con «sin red» en los ajustes), el ambar. Los fondos, casi negros
     con un toque de cada uno. Sustituye a la paleta soft (morada) de vkDaedalus */
  struct Paleta {
    uint32_t acento, fuerte, fuerteHov, suave;               /* resaltes; boton principal; fondo de lo elegido */
    uint32_t barra, tarjeta, ventana, elevado, gris;          /* barra, tarjeta, fondo de la ventana, teselas, en gris */
    uint32_t control, controlHov, filo, filoCasilla, filoCasillaHov, pozo;
  };
  Paleta paleta() const {
    if (st_.sinRedPanel || st_.rendimiento)
      return {0xDEAA7Cu, 0xB87A3Au, 0xC88A4Au, 0x3A2E22u, 0x0E0B08u, 0x15110Du, 0x110E0Bu, 0x231C15u, 0x1B1611u,
              0x282018u, 0x32281Eu, 0x463A2Eu, 0x4C4034u, 0x6A5A48u, 0x0F0C09u};
    return {0x66B8FFu, 0x2F6FDEu, 0x3F7FEEu, 0x1C2A42u, 0x090B0Fu, 0x0F1218u, 0x0C0F14u, 0x18202Bu, 0x141920u,
            0x1B2430u, 0x22303Fu, 0x2E3A4Au, 0x38465Au, 0x506276u, 0x0A0D12u};
  }
  static float canal(uint32_t c, int k) { return (float)((c >> (16 - 8 * k)) & 0xFFu) / 255.0f; }
  Paleta pal_{};
  /* el radio que trae dibujada cada mascara, en píxeles de la mascara: mask9 saca de ahi la
     porcion de esquina y la estira hasta el radio que se pida en pantalla. Con la píldora es la
     mitad del cuadrado (48 de 96), o sea un cuarto de circulo entero; de ahi sale la pista y la
     bola del interruptor. Antes se usaba el radio de pantalla para recortar la mascara, y salian
     puntas de lente en vez de esquinas redondas. */
  float maskRad_[2] = {48.0f, 24.0f};   /* {píldora, tarjeta} */
  Glyph glyphs_[256]{};
  /* unos pocos signos fuera del rango latino-1 (euro, raya, comillas tipograficas, puntos
     suspensivos): sin ellos el titulo de la tarjeta salta a "?" en mitad de la frase */
  std::vector<std::pair<unsigned, Glyph>> extra_;
  const Glyph *glifo(unsigned cp) const;
  float asc_ = 17.0f;   /* ascenso de la fuente en píxeles del atlas */
  /* (v1.16) Barlow, la letra de vkDaedalus (dentro del programa): [0] normal, [1] seminegrita, solo el latino-1; lo
     que no tenga (ruso, chino, japones) sale de la de siempre. `fuente_` elige con cual escriben text() y
     textWidth(): 0 = la de siempre (la pastilla), 1 = Barlow, 2 = Barlow seminegrita */
  Glyph glyphsB_[2][256]{};
  float ascB_[2] = {17.0f, 17.0f};
  bool hayBarlow_ = false;
  int fuente_ = 0;
  float alfaGlobal_ = 1.0f, desplazY_ = 0.0f;   /* (v1.16) la aparicion del panel: todo lo que se pinta, mas abajo y mas claro */
  bool hasText_ = false;
  std::vector<uint8_t> atlas_;
  int aw_ = 0, ah_ = 0;
  int shelfX_ = 0, shelfY_ = 0, shelfH_ = 0;

  State st_{};
  float mx_ = -1.0f, my_ = -1.0f;
  std::string info_;
  std::string askTitulo_, askCuerpo_;   /* vacio = texto oficial */
  std::vector<HudVertex> v_;
  int W_ = 0, H_ = 0;      /* tamano real de la ventana */
  float sc_ = 1.0f;      /* escala de la interfaz: 1 en 1080, hasta 2 en 2160 */
  float scUser_ = 0.0f;  /* tamano fijado a mano (--ui-escala, Ctrl+Shift+7/8); 0 = automatico */

  /* sw3D_ es el boton redondo de encendido del 3D (v1.12; antes la pista del interruptor ON/OFF y
     swKnob_ su bola) y pillModo_ el interruptor Neural | Rendimiento de la pastilla del video normal.
     btnOv_ ya no esta: ocultar la pastilla es tecla. */
  Rect pillPanel_, sw3D_, swKnob_, pillModo_, btnRed_, depTrack_, popTrack_, depKnob_, popKnob_;
  float panelW_ = 0.0f, panelH_ = 0.0f;   /* de cuanto salio la pastilla en el ultimo begin */
  Rect askPanel_, askSbs_, askTop_, askFlat_, askConv_, askDont_, askClose_, askAyuda_;
  Rect askActualizar_, askLuego_, askNoAvisar_;
  Rect chkRecordar_, btnNormal_, btnDividido_, btnSuave_, btnEngranaje_;
  Rect btnPre_[3];   /* los preajustes del menu del rayo */
  /* el boton del rayo de la pastilla, su menu desplegado y las pistas de sus cuatro deslizadores */
  Rect btnRayo_, menuRend_, rendTrack_[4], rendFila_[4];
  Rect btnFoco_;     /* el boton del chip «AI» del hub normal */
  Rect btnAM_[2];    /* su menu: Auto y Manual */
  /* (v1.18) el cuadrito de los FPS y el menu de la generacion de cuadros: 5 filas de hasta 8 opciones */
  Rect cuadroFps_{-10.0f, -10.0f, 0.0f, 0.0f}, menuFg_{-10.0f, -10.0f, 0.0f, 0.0f};
  Rect fgOpc_[6][8];
  void dibujaMenuFg(const float accent[3], const float ink[4], const float dim[4]);
  /* el panel de ajustes: su caja, el aspa, las pestanas, el cuerpo que corre y la caja de cada fila */
  struct ZonaItem {
    Rect caja;
    std::vector<Rect> subs;   /* las opciones de un segmentado, o el aspa de una fila */
    Rect pista;               /* la pista de un deslizador */
  };
  Rect panelCaja_, panelActualizar_, panelCerrar_, panelCuerpo_, panelIdioma_, panelFijar_;
  Rect panelCopiar_{-10.0f, -10.0f, 0.0f, 0.0f};   /* (v1.14) el boton Copiar del pie (pestana Juegos) */
  Rect panelLogo_;   /* el logo y el nombre: la primera pestana (rama experimental) */
  Rect panelEsquina_; /* la esquina para cambiar el tamano */
  Rect panelModo_;    /* el interruptor AI | Rendimiento */
public:
  /* el tamano del panel que se dibujo en el ultimo `begin`, en puntos del lienzo (para la capa, que lo
     cambia arrastrando la esquina) */
  float panelAnchoVisto() const { return panelCaja_.w; }
  float panelAltoVisto() const { return panelCaja_.h; }
private:
  void dibujaCuadroFps(float xDer, float ph, bool compacta, bool orilla, const float accent[3], const float ink[4],
                       const float dim[4]);
  std::vector<Rect> panelIdiomaOpc_;   /* las filas de la lista de idiomas, si esta abierta */
  std::vector<Rect> panelDespOpc_;   /* (v1.16) las opciones del desplegable abierto */
  std::vector<Rect> panelTeselaFlecha_, panelMenuOpc_;   /* (v1.17) las flechitas de las teselas y su menu abierto */
  Rect panelMenuAncla_{-10.0f, -10.0f, 0.0f, 0.0f};
  Rect panelDespCaja_{-10.0f, -10.0f, 0.0f, 0.0f};   /* (v1.16) y su caja entera */
  std::vector<Rect> panelPestanas_;
  std::vector<Rect> panelTeselas_;   /* (v1.16) las teselas chicas, en el orden de State::teselas */
  Rect panelVolver_{-10.0f, -10.0f, 0.0f, 0.0f};   /* (v1.16) la flecha de volver a la pastilla */
  std::vector<ZonaItem> zonas_;
  float contenidoAlto_ = 0.0f;
};

#endif
