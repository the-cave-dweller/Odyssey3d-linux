/* ui_ajustes.cpp — la ventana de ajustes del visor 3D (proceso APARTE del visor).

   Para que sirve: el visor se queda el teclado y el raton de la pantalla del panel, asi que dentro
   de la superposicion solo se puede afinar con las teclas y no hay forma de elegir QUE red de
   profundidad esta puesta (la de Samsung, la nuestra, la de desplazamiento...). Esta ventana es un
   programa independiente que habla con el visor por los tres ficheros de src/ajuste.h:

     escribe  ~/.local/state/odyssey3d/ajuste          (los numeros y el .onnx que el visor aplica)
     lee      ~/.local/state/odyssey3d/estado          (lo que el visor esta usando de verdad)
     lee      ~/.local/state/odyssey3d/preferencias.log(que se toco, cuando y con que red puesta)

   No enlaza con el visor ni con Vulkan: si esta ventana se cierra o se cuelga, la trama no se entera
   y el 3D sigue igual. Por eso tambien sirve para probar redes raras (los .onnx que la pastilla del
   visor no ofrece) sin tocar el codigo del visor.

   Se compila solo si hay Qt6 (ver CMakeLists.txt, opcion ODYSSEY3D_UI). Sin pantalla se puede probar
   con QT_QPA_PLATFORM=offscreen. */
#include "ajuste.h"
#include <cstdio>
#include <cstring>

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QList>
#include <QScreen>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

/* ---------------- como se traduce un deslizador a un numero ---------------- */

static double delDeslizador(const ajuste::Clave &c, int posicion) {
  const double bruto = c.minimo + (c.maximo - c.minimo) * (double)posicion / 1000.0;
  if (c.entero) return std::floor(bruto + 0.5);
  const double paso = c.paso > 0.0 ? c.paso : 0.001;
  return std::floor(bruto / paso + 0.5) * paso;
}

static int alDeslizador(const ajuste::Clave &c, double v) {
  if (c.maximo <= c.minimo) return 0;
  double p = (v - c.minimo) / (c.maximo - c.minimo) * 1000.0;
  if (p < 0.0) p = 0.0;
  if (p > 1000.0) p = 1000.0;
  return (int)std::lround(p);
}

/* ---------------- las redes que hay en el disco ---------------- */

/* Una red y como se llama en la lista. `nativa` es la que esta instalada en la maquina
   (~/.local/share/odyssey3d/modelos, o la carpeta `modelos` del propio programa): son las que hemos
   probado aqui y las que usa el conversor por defecto. Lo demas —la memoria USB, carpetas sueltas, lo
   que diga ODYSSEY3D_MODELOS— sale bajo `Otros`, por si acaso: no sabemos si abren ni como calzan. */
struct Red {
  QString ruta;
  bool nativa = false;
  QString aviso;    /* algo que conviene saber antes de pulsar `Aplicar al visor` */
};

/* Las que sabemos que aqui NO abren, con el motivo medido. La tabla es pequena a proposito: en
   cuanto una abre, se quita de aca. `mde_tiny` la probamos el 24/11 y ONNX Runtime la rechazo con
   "Got invalid dimensions for input: input" (pide otro tamano de entrada que el nuestro). */
static QString avisoConocido(const QString &nombre) {
  const QString n = nombre.toLower();
  if (n.startsWith("mde_tiny"))
    return QString::fromUtf8("aqui no abrio: pide otras dimensiones de entrada");
  if (n.contains("face_detection") || n.contains("yunet"))
    return QString::fromUtf8("no es de profundidad: es la cara para el rastreo");
  return QString();
}

static bool esNativa(const QString &ruta, const QStringList &carpetasNativas) {
  const QString dir = QFileInfo(ruta).absolutePath();
  for (const QString &c : carpetasNativas)
    if (QFileInfo(c).absoluteFilePath() == dir) return true;
  return false;
}

/* Las carpetas que contamos como "de la casa". */
static QStringList carpetasNativas() {
  const char *home = std::getenv("HOME");
  const std::string h(home ? home : "/home");
  return QStringList()
         << QString::fromStdString(h + "/.local/share/odyssey3d/modelos")
         << QString::fromStdString(h + "/Documentos/LU agent/odyssey3d-linux/modelos");
}

/* Las rutas de las redes, ordenadas y con el grupo marcado, para la lista desplegable. */
static QList<Red> redesDelDisco(const std::string &modeloEnUso) {
  const QStringList nativas = carpetasNativas();
  QStringList dirs = nativas;
  dirs << "/run/media/rodolfo/067ABD417ABD2DF5/AnalisisOdyssey/modelos";
  if (const char *e = std::getenv("ODYSSEY3D_MODELOS"))
    for (const QString &d : QString(e).split(':', Qt::SkipEmptyParts)) dirs << d;
  if (!modeloEnUso.empty()) {
    const std::string r = modeloEnUso;
    const size_t b = r.find_last_of('/');
    if (b != std::string::npos) dirs << QString::fromStdString(r.substr(0, b));
  }
  QList<Red> salida;
  QStringList vistos;
  for (const QString &d : dirs) {
    QDir dir(d);
    if (!dir.exists()) continue;
    for (const QFileInfo &fi : dir.entryInfoList({"*.onnx"}, QDir::Files, QDir::Name)) {
      const QString ruta = fi.absoluteFilePath();
      if (vistos.contains(ruta)) continue;
      vistos << ruta;
      Red r{};
      r.ruta = ruta;
      r.nativa = esNativa(ruta, nativas);
      r.aviso = avisoConocido(fi.fileName());
      salida.push_back(r);
    }
  }
  std::sort(salida.begin(), salida.end(), [](const Red &a, const Red &b) {
    if (a.nativa != b.nativa) return a.nativa;      /* las nativas primero */
    return a.ruta < b.ruta;
  });
  return salida;
}


/* Que hace cada numero, en una linea, para el globo del raton y para los avisos. Son las mismas
   palabras que estan comentadas en weave.h y en el README, resumidas. */
static QString paraQueEs(const char *nombre) {
  const std::string k(nombre);
  if (k == "fase") return QString::fromUtf8("corre la trama de lado: con 0 cae donde la puso Samsung; se afina hasta que las franjas desaparecen");
  if (k == "paso") return QString::fromUtf8("ensancha o estrecha la rampa de fase escrita en el panel (1 es la nominal de la lente)");
  if (k == "filtro") return QString::fromUtf8("pendiente del filtro triangular entre vistas: 10 es el de siempre, 0 deja los bordes duros");
  if (k == "crosstalk") return QString::fromUtf8("crosstalk segun la posicion de la pupila (el estatico ya viene en la trama)");
  if (k == "conv") return QString::fromUtf8("convergencia: desplaza una vista sobre la otra, en puntos de panel");
  if (k == "rampa_x") return QString::fromUtf8("rampa extra en x: es la que arregla el paso de la rejilla contra la lente del panel");
  if (k == "rampa_y") return QString::fromUtf8("rampa extra en y: lo mismo, en vertical");
  if (k == "fondo") return QString::fromUtf8("cuanto se estira el relieve DETRAS del plano de apoyo (1 es como siempre)");
  if (k == "frente") return QString::fromUtf8("cuanto se estira el relieve DELANTE del plano de apoyo (1 es como siempre)");
  if (k == "empuje_fondo") return QString::fromUtf8("empuje fijo de la banda del fondo, en puntos de panel (0 es como siempre)");
  if (k == "empuje_frente") return QString::fromUtf8("empuje fijo de la banda del frente, en puntos de panel (0 es como siempre)");
  if (k == "planos") return QString::fromUtf8("0 deja el relieve continuo; 1 lo aplasta a dos laminas planas, como el Nintendo 3DS");
  if (k == "muestras") return QString::fromUtf8("niveles intermedios entre las dos laminas (0 deja rampa continua en el medio)");
  if (k == "temblor") return QString::fromUtf8("temblor del desplazamiento: 0,25 px es lo de siempre; con 0 los cantos pueden parpadear");
  if (k == "canto") return QString::fromUtf8("ancho del puente del canto, en puntos de panel");
  if (k == "fase_temblor") return QString::fromUtf8("temblor de la fase: hace que las franjas bailen un poco y se rompa el batido con la lente");
  if (k == "relieve") return QString::fromUtf8("3D Salida: cuanto relieve en total (50 es el de Samsung, el que no se sale de los topes por ojo)");
  if (k == "apoyo") return QString::fromUtf8("plano de apoyo: que profundidad se queda clavada en la pantalla (55 es como siempre)");
  return QString();
}

static QString numTexto(double v) {
  QString s = QString::number(v, 'g', 6);
  s.replace('.', ',');
  return s;
}

/* ---------------- el globo transparente que sigue al raton sobre cada barra ----------------
   El deslizador no dice en pantalla que numero estas tocando: la caja de la derecha se actualiza
   cuando el visor responde, medio segundo despues. Este globo se planta junto al cursor mientras se
   arrastra o se pasa por encima y cuenta el valor de verdad en ese momento: que numero es, cuanto
   vale, de cuanto a cuanto se mueve, y cuanto valia de fabrica si ya lo cambiamos. */
class Burbuja : public QObject {
 public:
  explicit Burbuja(QObject *parent = nullptr) : QObject(parent) {}
  void ponTexto(QObject *barra, std::function<QString()> texto) { textos_[barra] = std::move(texto); }
  void refrescar(QWidget *barra);   /* cuando el numero cambia por teclado o por senal, no por raton */
  bool eventFilter(QObject *obj, QEvent *ev) override;

 private:
  void mostrar(const QString &texto);
  void ocultar();
  QWidget *globo_ = nullptr;
  QLabel *nota_ = nullptr;
  std::map<QObject *, std::function<QString()>> textos_;
};

void Burbuja::mostrar(const QString &texto) {
  if (!globo_) {
    globo_ = new QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                                   Qt::WindowTransparentForInput);
    globo_->setAttribute(Qt::WA_TranslucentBackground);      /* las esquinas, transparentes de verdad */
    globo_->setAttribute(Qt::WA_ShowWithoutActivating);      /* no le roba el foco a la ventana */
    auto *caja = new QVBoxLayout(globo_);
    caja->setContentsMargins(0, 0, 0, 0);
    nota_ = new QLabel(globo_);
    nota_->setTextFormat(Qt::RichText);
    nota_->setStyleSheet(QStringLiteral(
        "QLabel { background: rgba(18,20,24,212); color: #f2f2f2; padding: 7px 9px;"
        " border: 1px solid rgba(255,255,255,70); border-radius: 7px; }"));
    caja->addWidget(nota_);
  }
  nota_->setText(texto);
  globo_->adjustSize();
  QPoint p = QCursor::pos();
  p.rx() += 16;
  p.ry() += 22;
  if (const QScreen *pantalla = QGuiApplication::primaryScreen()) {
    const QRect g = pantalla->availableGeometry();
    if (p.x() + globo_->width() > g.right()) p.rx() = p.x() - globo_->width() - 26;
    if (p.y() + globo_->height() > g.bottom()) p.ry() = p.y() - globo_->height() - 32;
  }
  globo_->move(p);
  if (!globo_->isVisible()) globo_->show();
}

/* El cambio de numero de un deslizador llega como senal, no como evento, y el filtro de arriba no la
   ve. Esta funcion la usan las conexiones de valueChanged y solo planta el globo si estamos de verdad
   encima (arrastrando) o si la barra tiene el teclado: asi el globo no sale cuando el numero lo mueve
   el visor desde su fichero de estado. */
void Burbuja::refrescar(QWidget *barra) {
  const auto it = textos_.find(barra);
  if (it == textos_.end()) return;
  const QSlider *desl = qobject_cast<const QSlider *>(barra);
  if (!barra->hasFocus() && !(desl && desl->isSliderDown())) return;
  mostrar(it->second());
}

void Burbuja::ocultar() {
  if (globo_) globo_->hide();
}

bool Burbuja::eventFilter(QObject *obj, QEvent *ev) {
  const auto it = textos_.find(obj);
  if (it == textos_.end()) return QObject::eventFilter(obj, ev);
  switch (ev->type()) {
    case QEvent::Enter:
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::Wheel:
    case QEvent::KeyPress:
      mostrar(it->second());
      break;
    case QEvent::Leave: {
      QSlider *desl = qobject_cast<QSlider *>(obj);
      if (!desl || !desl->isSliderDown()) ocultar();   /* arrastrando fuera de la barra, el globo sigue */
      break;
    }
    case QEvent::WindowDeactivate:
      ocultar();
      break;
    default:
      break;
  }
  return QObject::eventFilter(obj, ev);
}

/* ---------------- la ventana ---------------- */

struct Fila {
  const ajuste::Clave *clave = nullptr;
  QSlider *deslizador = nullptr;
  QDoubleSpinBox *caja = nullptr;
};

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("odyssey3d-ajustes");

  /* `--prueba` sirve para comprobar la ventana sin pantalla (por ssh, o con QT_QPA_PLATFORM=offscreen):
     cuenta lo que ve en el disco y en los ficheros del visor y sale sin abrir nada. */
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--prueba") != 0) continue;
    const std::map<std::string, std::string> est = ajuste::leerEstado();
    std::printf("fichero de orden : %s\n", ajuste::rutaAjuste().c_str());
    std::printf("fichero de estado: %s\n", ajuste::rutaEstado().c_str());
    if (est.empty()) {
      std::printf("estado: el visor no ha escrito nada, osea que no hay sesion 3D abierta\n");
    } else {
      std::printf("estado del visor (%d numeros y nombres)\n", (int)est.size());
      for (const auto &kv : est)
        std::printf("  %-16s %s\n", kv.first.c_str(), kv.second.c_str());
    }
    const QList<Red> redes = redesDelDisco(est.count("modelo") ? est.at("modelo") : std::string());
    std::printf("redes que tendria la lista de disponibles: %d\n", (int)redes.size());
    std::printf("· NATIVAS: instaladas en la maquina, las de siempre ·\n");
    for (const Red &r : redes)
      if (r.nativa)
        std::printf("  %s%s%s\n", r.ruta.toUtf8().constData(),
                    r.aviso.isEmpty() ? "" : "   —  ", r.aviso.toUtf8().constData());
    std::printf("· OTROS: en la USB o en carpetas sueltas (sin probar aqui) ·\n");
    for (const Red &r : redes)
      if (!r.nativa)
        std::printf("  %s%s%s\n", r.ruta.toUtf8().constData(),
                    r.aviso.isEmpty() ? "" : "   —  ", r.aviso.toUtf8().constData());
    const std::map<std::string, std::string> hub = ajuste::leerEstadoHub();
    if (hub.empty())
      std::printf("vigilante: %s esta vacio, osea que no hay `odyssey3d serve` corriendo\n",
                  ajuste::rutaHub().c_str());
    else {
      std::printf("vigilante (%s)\n", ajuste::rutaHub().c_str());
      for (const auto &kv : hub) std::printf("  %-10s %s\n", kv.first.c_str(), kv.second.c_str());
    }
    const auto fabrica = ajuste::valoresFabrica();
    std::printf("de fabrica (%d numeros, la tabla del programa corregida con %s):\n", (int)fabrica.size(),
                ajuste::rutaServeConf().c_str());
    for (const auto &kv : fabrica) std::printf("  %-16s %g\n", kv.first.c_str(), kv.second);
    const auto perfiles = ajuste::leerPerfiles();
    std::printf("guardados (%s): %d redes\n", ajuste::rutaPerfiles().c_str(), (int)perfiles.size());
    for (const auto &p : perfiles) {
      std::vector<std::string> nombres;
      const int fuera = ajuste::diferencias(p.second, fabrica, &nombres);
      std::printf("  %s — %d fuera de fabrica:", p.first.c_str(), fuera);
      for (const std::string &n : nombres) std::printf(" %s", n.c_str());
      std::printf("\n");
    }
    const std::vector<std::string> sel = ajuste::leerSeleccion();
    std::printf("seleccionadas (%s): %d\n", ajuste::rutaSeleccion().c_str(), (int)sel.size());
    for (const std::string &r : sel) std::printf("  %s\n", r.c_str());
    const std::vector<std::string> hist = ajuste::historial(5);
    std::printf("ultimos %d cambios apuntados en %s\n", (int)hist.size(), ajuste::rutaHistorial().c_str());
    for (const std::string &l : hist) std::printf("  %s\n", l.c_str());
    return 0;
  }

  QWidget ventana;
  ventana.setWindowTitle(QString::fromUtf8("Ajustes del visor 3D (proceso aparte)"));
  auto *principal = new QVBoxLayout(&ventana);

  /* ---- arriba: con quien estamos hablando ---- */
  auto *estado = new QLabel(&ventana);
  estado->setTextInteractionFlags(Qt::TextSelectableByMouse);
  principal->addWidget(estado);

  /* ---- de fabrica y puestas: de donde se parte y con que red estamos hablando ---- */
  const std::map<std::string, double> fabrica = ajuste::valoresFabrica();
  std::string redPuesta;   /* la que tiene abierta el visor ahora mismo (la cuenta el fichero estado) */
  QStringList extras;      /* los .onnx que elegimos a mano y no viven en las carpetas que escaneamos */

  /* ---- las redes: a la izquierda las que hay en el disco (stock), a la derecha las que tenemos
         a mano para poner, y al lado, independiente, los guardados ------------------------------
     Antes era una sola lista desplegable: no dejaba ver que redes habia sin desplegar, no guardaba la
     eleccion entre arranques, y no decia nada de lo que se habia cambiado para cada una. Ahora son
     dos listas lado a lado con flechas en el medio, y una tercera de guardados que se llena sola con
     las redes cuyos numeros se salieron de fabrica. */
  auto *grupoRed = new QGroupBox(QString::fromUtf8("Redes de profundidad (2D a 3D): disponibles, seleccionadas y guardadas"), &ventana);
  auto *cajaRed = new QHBoxLayout(grupoRed);

  const auto cabecita = [](QWidget *p, const QString &rico) {
    auto *l = new QLabel(rico, p);
    l->setTextFormat(Qt::RichText);
    l->setWordWrap(true);
    return l;
  };

  auto *listaDispo = new QListWidget(grupoRed);
  listaDispo->setSelectionMode(QAbstractItemView::ExtendedSelection);
  listaDispo->setMinimumHeight(210);
  auto *listaSel = new QListWidget(grupoRed);
  listaSel->setMinimumHeight(210);
  auto *listaGuard = new QListWidget(grupoRed);
  listaGuard->setMinimumHeight(210);

  auto *colDispo = new QVBoxLayout;
  colDispo->addWidget(cabecita(grupoRed, QString::fromUtf8("<b>DISPONIBLES (stock)</b><br>los .onnx del disco: primero los nativos de la maquina, despues los demas")));
  colDispo->addWidget(listaDispo, 1);

  auto *anadir = new QPushButton(QString::fromUtf8("→   pasar"), grupoRed);
  auto *anadirTodas = new QPushButton(QString::fromUtf8("→→   todas"), grupoRed);
  auto *quitar = new QPushButton(QString::fromUtf8("←   quitar"), grupoRed);
  auto *vaciarSel = new QPushButton(QString::fromUtf8("←←   vaciar"), grupoRed);
  anadir->setToolTip(QString::fromUtf8("Pasa a la derecha las redes marcadas en la izquierda (puedes marcar varias con Ctrl o Mayus). Doble clic en la izquierda hace lo mismo."));
  anadirTodas->setToolTip(QString::fromUtf8("Pasa todas las disponibles."));
  quitar->setToolTip(QString::fromUtf8("Quita de la derecha la seleccionada. No borra el fichero: solo sale de la lista de trabajo."));
  vaciarSel->setToolTip(QString::fromUtf8("Deja la lista de la derecha vacia."));
  auto *colFlechas = new QVBoxLayout;
  colFlechas->addStretch(1);
  colFlechas->addWidget(anadir);
  colFlechas->addWidget(anadirTodas);
  colFlechas->addSpacing(10);
  colFlechas->addWidget(quitar);
  colFlechas->addWidget(vaciarSel);
  colFlechas->addStretch(1);

  auto *aplicarModelo = new QPushButton(QString::fromUtf8("Poner esta red en el visor"), grupoRed);
  auto *buscarModelo = new QPushButton(QString::fromUtf8("Otro archivo…"), grupoRed);
  aplicarModelo->setToolTip(QString::fromUtf8("Le pide al visor que cambie a la red seleccionada en la lista de la derecha, en marcha. Si la red no abre, el visor sigue con la que tenia y lo dice."));
  buscarModelo->setToolTip(QString::fromUtf8("Elegir un .onnx a mano: entra en la lista de la izquierda (como OTRO) y en la de la derecha."));
  auto *colSel = new QVBoxLayout;
  colSel->addWidget(cabecita(grupoRed, QString::fromUtf8("<b>SELECCIONADAS</b><br>las que tienes a mano para poner; la negrita es la puesta. Doble clic = ponerla")));
  colSel->addWidget(listaSel, 1);
  auto *filaPoner = new QHBoxLayout;
  filaPoner->addWidget(aplicarModelo);
  filaPoner->addWidget(buscarModelo);
  colSel->addLayout(filaPoner);

  auto *grupoGuard = new QGroupBox(QString::fromUtf8("Guardados (las redes que se modificaron)"), grupoRed);
  auto *colGuard = new QVBoxLayout(grupoGuard);
  colGuard->addWidget(cabecita(grupoGuard, QString::fromUtf8("Se llenan solos: en cuanto una red tiene numeros fuera de fabrica queda apuntada en <b>%1</b>").arg(QString::fromStdString(ajuste::rutaPerfiles()))));
  colGuard->addWidget(listaGuard, 1);
  auto *revertir = new QPushButton(QString::fromUtf8("Revertir los cambios de esta red"), grupoGuard);
  auto *guardarAhora = new QPushButton(QString::fromUtf8("Guardar lo de ahora para esta red"), grupoGuard);
  auto *exportar = new QPushButton(QString::fromUtf8("Exportar…"), grupoGuard);
  auto *eliminar = new QPushButton(QString::fromUtf8("Eliminar del listado"), grupoGuard);
  revertir->setToolTip(QString::fromUtf8(
      "Devuelve TODOS los numeros del visor a los de fabrica (los que trae el programa, corregidos con"
      " lo que dice serve.conf) y quita el guardado de esa red. Es lo contrario de afinar: si estabas"
      " probando una red y la dejaste rara, este boton la deja como venia."));
  guardarAhora->setToolTip(QString::fromUtf8(
      "Apunta los numeros que tienen los deslizadores ahora mismo para la red seleccionada a la derecha."
      " Con una sesion abierta no hace falta: los guardados se actualizan solos con lo que cuenta el visor."));
  exportar->setToolTip(QString::fromUtf8(
      "Escribe los numeros de esa red en un fichero que tu elijas, en el mismo formato que serve.conf:"
      " lo que se salio de fabrica suelto y lo demas comentado. Sirve para pasar el ajuste a otra maquina"
      " o a otra version, o para tenerlo a mano."));
  eliminar->setToolTip(QString::fromUtf8(
      "Quita el guardado de esa red del fichero de perfiles. No toca los numeros que tiene el visor"
      " puestos ahora: para eso esta Revertir."));
  auto *notaGuard = new QLabel(QString::fromUtf8("De fabrica: %1").arg(QString::fromStdString(ajuste::rutaServeConf())), grupoGuard);
  notaGuard->setWordWrap(true);
  auto *filaPar = new QHBoxLayout;
  filaPar->addWidget(exportar);
  filaPar->addWidget(eliminar);
  colGuard->addWidget(revertir);
  colGuard->addLayout(filaPar);
  colGuard->addWidget(guardarAhora);
  colGuard->addWidget(notaGuard);

  cajaRed->addLayout(colDispo, 6);
  cajaRed->addLayout(colFlechas);
  cajaRed->addLayout(colSel, 5);
  cajaRed->addWidget(grupoGuard, 6);
  principal->addWidget(grupoRed);

  /* ---------------- las tres listas ---------------- */
  const auto rutasDeLista = [](QListWidget *l) {
    QStringList r;
    for (int i = 0; i < l->count(); ++i) {
      const QString x = l->item(i)->data(Qt::UserRole).toString();
      if (!x.isEmpty()) r << x;
    }
    return r;
  };
  const auto itemFijo = [](QListWidget *l, const QString &texto) {
    auto *it = new QListWidgetItem(texto, l);
    it->setFlags(Qt::NoItemFlags);   /* titulo o aviso: no se puede elegir */
  };
  /* las listas se rellenan solas dos veces por segundo, asi que al rehacerlas devolvemos la seleccion
     que tenia el usuario (por ruta, no por numero de renglon: los renglones se mueven) */
  const auto seleccionDe = [](QListWidget *l) {
    return l->currentItem() ? l->item(l->currentRow())->data(Qt::UserRole).toString() : QString();
  };
  const auto devolverSeleccion = [](QListWidget *l, const QString &ruta) {
    if (ruta.isEmpty()) return;
    for (int i = 0; i < l->count(); ++i)
      if (l->item(i)->data(Qt::UserRole).toString() == ruta) { l->setCurrentRow(i); return; }
  };
  const auto guardarListaSel = [&]() {
    std::vector<std::string> v;
    for (const QString &r : rutasDeLista(listaSel)) v.push_back(r.toStdString());
    std::string err;
    if (!ajuste::guardarSeleccion(v, &err))
      notaGuard->setText(QString::fromUtf8(("no se pudo guardar la lista de seleccionadas: " + err).c_str()));
  };
  const auto etiquetaRed = [&](const QString &ruta, bool puesta) {
    const QFileInfo fi(ruta);
    QString texto = fi.fileName() + "  (" + QString::number(fi.size() / 1024) + " KB)";
    const QString aviso = avisoConocido(fi.fileName());
    if (!aviso.isEmpty()) texto += "   —  " + aviso;
    if (!fi.exists()) texto += "   (ya no esta en el disco)";
    if (puesta) texto = "· PUESTA · " + texto;
    return texto;
  };
  const auto rellenarDispo = [&]() {
    const QStringList yaPuestas = rutasDeLista(listaSel);
    const QString previa = seleccionDe(listaDispo);
    listaDispo->clear();
    const QList<Red> redes = redesDelDisco(redPuesta);
    int hayNativas = 0, hayOtras = 0;
    for (const Red &r : redes) {
      if (r.nativa) ++hayNativas;
      else ++hayOtras;
    }
    const auto poneRed = [&](const Red &r) {
      QString texto = etiquetaRed(r.ruta, !redPuesta.empty() && r.ruta.toStdString() == redPuesta);
      if (yaPuestas.contains(r.ruta)) texto = "(esta a la derecha) " + texto;
      auto *it = new QListWidgetItem(texto, listaDispo);
      it->setData(Qt::UserRole, r.ruta);
      it->setToolTip(r.ruta + (r.aviso.isEmpty() ? QString() : QString::fromUtf8("\n") + r.aviso));
    };
    if (hayNativas > 0) {
      itemFijo(listaDispo, QString::fromUtf8("· NATIVAS: instaladas en la maquina, las de siempre ·"));
      for (const Red &r : redes)
        if (r.nativa) poneRed(r);
    }
    if (hayOtras > 0 || !extras.isEmpty()) {
      itemFijo(listaDispo, QString::fromUtf8("· OTROS: en la USB o en carpetas sueltas (sin probar aqui) ·"));
      for (const Red &r : redes)
        if (!r.nativa) poneRed(r);
      for (const QString &r : extras) {
        bool esta = false;
        for (const Red &d : redes)
          if (d.ruta == r) { esta = true; break; }
        if (!esta) poneRed(Red{r, false, QString::fromUtf8("elegida a mano")});
      }
    }
    if (redes.isEmpty() && extras.isEmpty())
      itemFijo(listaDispo, QString::fromUtf8("(no hay .onnx en las carpetas de siempre)"));
    devolverSeleccion(listaDispo, previa);
  };
  const auto rellenarSel = [&]() {
    const QString previa = seleccionDe(listaSel);
    QStringList rutas;
    for (const std::string &r : ajuste::leerSeleccion()) rutas << QString::fromStdString(r);
    const QString puesta = QString::fromStdString(redPuesta);
    if (!puesta.isEmpty() && !rutas.contains(puesta)) rutas << puesta;
    listaSel->clear();
    for (const QString &r : rutas) {
      const bool esPuesta = !redPuesta.empty() && r.toStdString() == redPuesta;
      auto *it = new QListWidgetItem(etiquetaRed(r, esPuesta), listaSel);
      it->setData(Qt::UserRole, r);
      it->setToolTip(r);
      if (esPuesta) {
        QFont g = it->font();
        g.setBold(true);
        it->setFont(g);
      }
    }
    if (rutas.isEmpty()) itemFijo(listaSel, QString::fromUtf8("(nada seleccionado: pasa algo de la izquierda con la flecha)"));
    devolverSeleccion(listaSel, previa);
  };
  const auto rellenarGuard = [&]() {
    const QString previa = seleccionDe(listaGuard);
    const auto perfiles = ajuste::leerPerfiles();
    listaGuard->clear();
    for (const auto &p : perfiles) {
      std::vector<std::string> nombres;
      const int fuera = ajuste::diferencias(p.second, fabrica, &nombres);
      if (fuera <= 0) continue;
      QString que;
      int contados = 0;
      for (const std::string &n : nombres) {
        if (contados >= 6) {
          que += QString::fromUtf8(" …");
          break;
        }
        if (contados) que += ", ";
        que += QString::fromStdString(n);
        ++contados;
      }
      QString texto = QFileInfo(QString::fromStdString(p.first)).fileName() + "  —  " +
                      QString::number(fuera) + " fuera de fabrica: " + que;
      if (p.first == redPuesta) texto = "· PUESTA · " + texto;
      auto *it = new QListWidgetItem(texto, listaGuard);
      it->setData(Qt::UserRole, QString::fromStdString(p.first));
      it->setToolTip(QString::fromStdString(p.first));
    }
    if (listaGuard->count() == 0)
      itemFijo(listaGuard, QString::fromUtf8("(ninguna red tiene numeros fuera de fabrica: estas de fabrica)"));
  };
  const auto redElegida = [&]() -> QString {
    if (QListWidgetItem *it = listaSel->currentItem()) {
      const QString r = it->data(Qt::UserRole).toString();
      if (!r.isEmpty()) return r;
    }
    return QString::fromStdString(redPuesta);
  };
  const auto redGuardadaElegida = [&]() -> QString {
    if (QListWidgetItem *it = listaGuard->currentItem()) {
      const QString r = it->data(Qt::UserRole).toString();
      if (!r.isEmpty()) return r;
    }
    return QString::fromStdString(redPuesta);
  };

  /* ---- el overlay FORZADO en el escritorio del monitor ---- */
  auto *grupoEscritorio = new QGroupBox(QString::fromUtf8("Overlay en el escritorio del monitor (sin video)"), &ventana);
  auto *cajaEscritorio = new QHBoxLayout(grupoEscritorio);
  auto *botonEscritorio = new QPushButton(QString::fromUtf8("Poner el overlay en el escritorio"), grupoEscritorio);
  auto *notaEscritorio = new QLabel(QString::fromUtf8(""), grupoEscritorio);
  notaEscritorio->setWordWrap(true);
  botonEscritorio->setToolTip(QString::fromUtf8(
      "Un clic monta el visor sobre el escritorio: el panel pasa a 3D y se teje el patron de trama de"
      " siempre, sin ningun video detras. Sirve para ver y afinar la trama, las franjas y las rampas con"
      " el escritorio por delante, y para mirar el 3D sin abrir nada. Otro clic lo quita. No capturamos"
      " lo que hay en la pantalla: ahi estaria nuestra propia ventana y la trama se tejeria a si misma."
      " La orden va al vigilante (`odyssey3d serve`); deja el fichero de ajuste solo con ella, los numeros"
      " que estuvieran escritos se llevan (el visor ya los aplico)."));
  cajaEscritorio->addWidget(botonEscritorio);
  cajaEscritorio->addWidget(notaEscritorio, 1);
  principal->addWidget(grupoEscritorio);

  /* que sesion tiene abierta el vigilante: `escritorio` (la forzada con este boton), `video` o
     `ninguna`. Lo cuenta el hub en ~/.local/state/odyssey3d/hub, una vez por segundo. */
  QString sesionHub;
  QObject::connect(botonEscritorio, &QPushButton::clicked, &ventana, [&]() {
    const bool quiere = (sesionHub != "escritorio");
    std::string err;
    if (!ajuste::escribir({{"overlay", quiere ? 1.0 : 0.0}}, std::string(), &err)) {
      notaEscritorio->setText(QString::fromUtf8(("no se pudo enviar la orden: " + err).c_str()));
      return;
    }
    notaEscritorio->setText(quiere ? QString::fromUtf8("orden enviada: ponerlo (el panel pasa a 3D,"
                                                       " tarda un segundo)")
                                   : QString::fromUtf8("orden enviada: quitarlo"));
  });

  /* ---- los numeros: un deslizador y una caja por numero ---- */
  /* lo que llevamos escrito y aun no confirmado por el visor (lo usa el globo para decir "enviando") */
  std::map<std::string, double> pendiente;

  auto *grupoNum = new QGroupBox(QString::fromUtf8("Numeros de la trama y del conversor (el globo sigue al raton)"), &ventana);
  auto *forma = new QFormLayout(grupoNum);
  forma->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

  /* El globo que sale junto al cursor. Cada deslizador y cada caja le prestan una funcion que arma el
     texto con el valor que se esta tocando EN ESTE MOMENTO (no el que confirmo el visor hace medio
     segundo), el rango, el paso, cuanto valia de fabrica y para que sirve ese numero. */
  auto *burb = new Burbuja(&ventana);
  const auto globo = [&](const ajuste::Clave &c, double valor) {
    QString t = "<b>" + QString::fromUtf8(c.etiqueta) + "</b>";
    t += "  <span style=\"color:#93a1af\">" + QString::fromLatin1(c.nombre) + "</span><br>";
    t += "<span style=\"font-size:14pt\"><b>" + numTexto(valor) + "</b></span>   de " + numTexto(c.minimo) +
         " a " + numTexto(c.maximo) + " · paso " + numTexto(c.paso);
    const auto f = fabrica.find(c.nombre);
    if (f != fabrica.end() && std::fabs(f->second - valor) > 1e-6)
      t += "<br><span style=\"color:#e8b44a\">de fabrica " + numTexto(f->second) + "</span>";
    const QString que = paraQueEs(c.nombre);
    if (!que.isEmpty()) t += "<br><span style=\"color:#c6ced6\">" + que + "</span>";
    if (pendiente.count(c.nombre)) t += "<br><span style=\"color:#7fc8a0\">enviando…</span>";
    return t;
  };

  std::vector<Fila> filas;
  for (const ajuste::Clave &c : ajuste::claves()) {
    auto *desl = new QSlider(Qt::Horizontal, grupoNum);
    desl->setRange(0, 1000);
    desl->setPageStep(50);
    auto *caja = new QDoubleSpinBox(grupoNum);
    caja->setRange(c.minimo, c.maximo);
    caja->setDecimals(c.entero ? 0 : (c.paso >= 0.01 ? 3 : 5));
    caja->setSingleStep(c.entero ? 1.0 : c.paso);
    auto *fila = new QHBoxLayout;
    fila->addWidget(desl, 1);
    fila->addWidget(caja, 0);
    forma->addRow(QString::fromUtf8(c.etiqueta), fila);
    filas.push_back(Fila{&c, desl, caja});
    /* el globo, también sobre la caja: ahi se teclea y tambien hay que decir que numero es.
       Ojo: se captura el PUNTERO a la clave (viva, esta en la tabla estatica de ajuste.cpp), no la
       variable del bucle, que se muere al acabar la vuelta. */
    const ajuste::Clave *p = &c;
    burb->ponTexto(desl, [&globo, p, desl]() { return globo(*p, delDeslizador(*p, desl->value())); });
    burb->ponTexto(caja, [&globo, p, caja]() { return globo(*p, caja->value()); });
    desl->installEventFilter(burb);
    caja->installEventFilter(burb);
    QObject::connect(desl, &QSlider::valueChanged, &ventana, [burb, desl]() { burb->refrescar(desl); });
    QObject::connect(caja, qOverload<double>(&QDoubleSpinBox::valueChanged), &ventana,
                     [burb, caja]() { burb->refrescar(caja); });
  }
  principal->addWidget(grupoNum);

  /* ---- lo ultimo que se toco, tal como lo apunto el visor ---- */
  auto *grupoLog = new QGroupBox(QString::fromUtf8("Lo ultimo que se cambio (preferencias.log)"), &ventana);
  auto *cajaLog = new QVBoxLayout(grupoLog);
  auto *log = new QPlainTextEdit(grupoLog);
  log->setReadOnly(true);
  log->setLineWrapMode(QPlainTextEdit::NoWrap);
  QFont mono("monospace");
  mono.setStyleHint(QFont::Monospace);
  log->setFont(mono);
  cajaLog->addWidget(log);
  principal->addWidget(grupoLog, 1);

  /* ---------------- la escritura al canal, con algo de calma ----------------
     Un deslizador suelto manda veinte valores por segundo; el visor lo lee una vez cada decima y
     preferencias.log se llenaria de renglones. Se juntan los cambios medio segundo. */
  QTimer junta;
  junta.setSingleShot(true);
  junta.setInterval(350);
  const auto mandarPendientes = [&pendiente, &aplicarModelo, &notaGuard]() {
    if (pendiente.empty()) return;
    std::string err;
    if (!ajuste::escribir(pendiente, std::string(), &err))
      notaGuard->setText(QString::fromUtf8(("no se pudo escribir el pedido: " + err).c_str()));
    pendiente.clear();
  };
  QObject::connect(&junta, &QTimer::timeout, &ventana, mandarPendientes);

  for (Fila &f : filas) {
    /* Ojo con las capturas: el bucle se acaba y las señales se disparan despues, asi que cada conexion
       se queda con el puntero al deslizador y a la caja, no con la variable del bucle. */
    const ajuste::Clave *c = f.clave;
    QSlider *desl = f.deslizador;
    QDoubleSpinBox *caja = f.caja;
    QObject::connect(desl, &QSlider::sliderReleased, &ventana, [&, c, desl, caja]() {
      pendiente[c->nombre] = delDeslizador(*c, desl->value());
      junta.start();
    });
    QObject::connect(caja, qOverload<double>(&QDoubleSpinBox::valueChanged), &ventana, [&, c, desl, caja]() {
      pendiente[c->nombre] = caja->value();
      desl->setValue(alDeslizador(*c, caja->value()));
      junta.start();
    });
  }

  /* ---------------- poner una red, y mover redes entre las dos listas ---------------- */
  const auto pedirRed = [&](const QString &ruta) {
    if (ruta.isEmpty()) {
      notaGuard->setText(QString::fromUtf8("No hay ninguna red marcada en la lista de la derecha."));
      return;
    }
    std::string err;
    if (!ajuste::escribir({}, std::string(ruta.toUtf8().constData()), &err)) {
      notaGuard->setText(QString::fromUtf8(("no se pudo enviar la red: " + err).c_str()));
      return;
    }
    notaGuard->setText(QString::fromUtf8("pedida %1 al visor: si esa red no abre aqui, el visor se queda con la que tenia y lo dice en preferencias.log.").arg(QFileInfo(ruta).fileName()));
  };
  const auto meterEnSel = [&](const QString &r) {
    if (r.isEmpty()) return;
    for (int i = 0; i < listaSel->count(); ++i)
      if (listaSel->item(i)->data(Qt::UserRole).toString() == r) return;
    auto *n = new QListWidgetItem(etiquetaRed(r, !redPuesta.empty() && r.toStdString() == redPuesta), listaSel);
    n->setData(Qt::UserRole, r);
    n->setToolTip(r);
  };
  QObject::connect(aplicarModelo, &QPushButton::clicked, &ventana, [&]() { pedirRed(redElegida()); });
  QObject::connect(listaSel, &QListWidget::itemDoubleClicked, &ventana, [&](QListWidgetItem *it) {
    if (!it) return;
    const QString r = it->data(Qt::UserRole).toString();
    if (!r.isEmpty()) pedirRed(r);
  });
  QObject::connect(listaDispo, &QListWidget::itemDoubleClicked, &ventana, [&](QListWidgetItem *) {
    anadir->click();   /* doble clic en la izquierda = pasala a la derecha */
  });
  QObject::connect(anadir, &QPushButton::clicked, &ventana, [&]() {
    QList<QListWidgetItem *> marcadas = listaDispo->selectedItems();
    if (marcadas.isEmpty() && listaDispo->currentItem()) marcadas << listaDispo->currentItem();
    for (QListWidgetItem *it : marcadas) meterEnSel(it->data(Qt::UserRole).toString());
    guardarListaSel();
    rellenarSel();
    rellenarDispo();
  });
  QObject::connect(anadirTodas, &QPushButton::clicked, &ventana, [&]() {
    for (int i = 0; i < listaDispo->count(); ++i) meterEnSel(listaDispo->item(i)->data(Qt::UserRole).toString());
    guardarListaSel();
    rellenarSel();
    rellenarDispo();
  });
  QObject::connect(quitar, &QPushButton::clicked, &ventana, [&]() {
    QList<int> filasBorrar;
    for (QListWidgetItem *it : listaSel->selectedItems()) filasBorrar << listaSel->row(it);
    if (filasBorrar.isEmpty() && listaSel->currentItem()) filasBorrar << listaSel->row(listaSel->currentItem());
    std::sort(filasBorrar.begin(), filasBorrar.end(), std::greater<int>());
    for (int f : filasBorrar) delete listaSel->takeItem(f);
    guardarListaSel();
    rellenarSel();
    rellenarDispo();
  });
  QObject::connect(vaciarSel, &QPushButton::clicked, &ventana, [&]() {
    listaSel->clear();
    guardarListaSel();
    rellenarSel();
    rellenarDispo();
  });
  QObject::connect(buscarModelo, &QPushButton::clicked, &ventana, [&]() {
    const QString ruta = QFileDialog::getOpenFileName(&ventana, QString::fromUtf8("Red .onnx"),
                                                      QDir::homePath() + "/.local/share/odyssey3d/modelos",
                                                      "*.onnx");
    if (ruta.isEmpty()) return;
    const QString absoluta = QFileInfo(ruta).absoluteFilePath();
    /* la que elegimos a mano entra en la izquierda como OTRA (no sabemos como calza aqui) y en la derecha */
    if (!extras.contains(absoluta)) extras << absoluta;
    meterEnSel(absoluta);
    guardarListaSel();
    rellenarSel();
    rellenarDispo();
    pedirRed(absoluta);
  });

  /* ---------------- los guardados: revertir, guardar a mano, exportar, eliminar ---------------- */
  QObject::connect(revertir, &QPushButton::clicked, &ventana, [&]() {
    const QString ruta = redGuardadaElegida();
    if (ruta.isEmpty()) {
      notaGuard->setText(QString::fromUtf8("Marca primero una red en la lista de guardados (o abre una sesion 3D)."));
      return;
    }
    std::string err;
    if (!ajuste::escribir(fabrica, std::string(), &err)) {
      notaGuard->setText(QString::fromUtf8(("no se pudo mandar la reversal: " + err).c_str()));
      return;
    }
    ajuste::borrarPerfil(ruta.toStdString(), &err);
    pendiente.clear();
    notaGuard->setText(QString::fromUtf8(
        "Revertida %1: los %2 numeros vuelven a fabrica (%3). El visor los toma en cuanto lea el pedido;"
        " si esta cerrado, esperan a que se abra. Para recuperar lo que tu habias afinado a mano mira"
        " preferencias.log: ahi esta el antes y el despues de cada tecla.").arg(QFileInfo(ruta).fileName()).arg(fabrica.size()).arg(QString::fromStdString(ajuste::rutaServeConf())));
    rellenarGuard();
  });
  QObject::connect(guardarAhora, &QPushButton::clicked, &ventana, [&]() {
    const QString ruta = redElegida();
    if (ruta.isEmpty()) {
      notaGuard->setText(QString::fromUtf8("Marca primero una red en la lista de la derecha (o abre una sesion 3D)."));
      return;
    }
    std::map<std::string, double> valores;
    for (const Fila &f : filas) valores[f.clave->nombre] = f.caja->value();
    std::string err;
    if (!ajuste::guardarPerfil(ruta.toStdString(), valores, &err))
      notaGuard->setText(QString::fromUtf8(("no se pudo guardar: " + err).c_str()));
    else
      notaGuard->setText(QString::fromUtf8("guardados los %1 numeros de %2 en %3").arg(valores.size()).arg(QFileInfo(ruta).fileName()).arg(QString::fromStdString(ajuste::rutaPerfiles())));
    rellenarGuard();
  });
  QObject::connect(exportar, &QPushButton::clicked, &ventana, [&]() {
    const QString ruta = redGuardadaElegida();
    if (ruta.isEmpty()) {
      notaGuard->setText(QString::fromUtf8("Marca primero una red en la lista de guardados."));
      return;
    }
    /* lo que hay guardado para esa red; si no hay nada guardado, lo que tienen los deslizadores ahora */
    std::map<std::string, double> valores;
    const auto perfiles = ajuste::leerPerfiles();
    const auto p = perfiles.find(ruta.toStdString());
    if (p != perfiles.end()) valores = p->second;
    for (const Fila &f : filas) valores[f.clave->nombre] = f.caja->value();
    if (valores.empty()) {
      notaGuard->setText(QString::fromUtf8("Esa red no tiene numeros guardados y el visor no esta contando ninguno."));
      return;
    }
    const QString carpeta = QFileInfo(QString::fromStdString(ajuste::rutaEstado())).absolutePath() + "/exportados";
    QDir().mkpath(carpeta);
    QString destino = QFileDialog::getSaveFileName(&ventana, QString::fromUtf8("Exportar los numeros de esta red"),
                                                   carpeta + "/" + QFileInfo(ruta).fileName() + ".ajuste", "*.ajuste");
    if (destino.isEmpty()) return;
    QFile archivo(destino);
    if (!archivo.open(QIODevice::WriteOnly | QIODevice::Text)) {
      notaGuard->setText(QString::fromUtf8("no se pudo escribir " + destino.toStdString()));
      return;
    }
    std::vector<std::string> fuera;
    ajuste::diferencias(valores, fabrica, &fuera);
    const std::set<std::string> fueraSet(fuera.begin(), fuera.end());
    QTextStream ts(&archivo);
    ts << "# numeros de la red " << ruta << "\n";
    ts << "# exportados " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm") << " por la ventana de ajustes\n";
    ts << "# lo que se salio de fabrica va suelto; lo demas va comentado, para que se vea el conjunto\n";
    ts << "# para que sea la base de esa red: pega estas lineas en "
         << QString::fromStdString(ajuste::rutaServeConf()) << "\n";
    ts << "# o mandalas vivas: odyssey3d ctl ajuste clave valor\n";
    ts << "modelo = " << ruta << "\n\n";
    for (const ajuste::Clave &c : ajuste::claves()) {
      const auto v = valores.find(c.nombre);
      if (v == valores.end()) continue;
      if (!fueraSet.count(c.nombre)) ts << "# ";
      ts << c.nombre << " = " << QString::number(v->second, 'g', 8) << "\n";
    }
    archivo.close();
    notaGuard->setText(QString::fromUtf8("exportado a %1 (%2 numeros fuera de fabrica)").arg(destino).arg(fuera.size()));
  });
  QObject::connect(eliminar, &QPushButton::clicked, &ventana, [&]() {
    const QString ruta = redGuardadaElegida();
    if (ruta.isEmpty()) {
      notaGuard->setText(QString::fromUtf8("Marca primero una red en la lista de guardados."));
      return;
    }
    std::string err;
    if (!ajuste::borrarPerfil(ruta.toStdString(), &err))
      notaGuard->setText(QString::fromUtf8(("no se pudo eliminar: " + err).c_str()));
    else
      notaGuard->setText(QString::fromUtf8("borrado el guardado de %1; los numeros que tiene el visor puestos no se tocaron").arg(QFileInfo(ruta).fileName()));
    rellenarGuard();
  });


  /* ---------------- la lectura del estado, dos veces por segundo ---------------- */
  QTimer refresco;
  refresco.setInterval(500);
  std::string logVisto;
  std::string guardVisto;
  QObject::connect(&refresco, &QTimer::timeout, &ventana, [&]() {
    const std::map<std::string, std::string> est = ajuste::leerEstado();
    const auto dato = [&est](const char *k) -> QString {
      const auto it = est.find(k);
      return it == est.end() ? QString() : QString::fromStdString(it->second);
    };

    /* lo que esta haciendo el vigilante: es lo unico que sabe si el overlay del escritorio esta puesto,
       porque la sesion forzada la lanza el y no nosotros */
    const std::map<std::string, std::string> hub = ajuste::leerEstadoHub();
    const auto datoHub = [&hub](const char *k) -> QString {
      const auto it = hub.find(k);
      return it == hub.end() ? QString() : QString::fromStdString(it->second);
    };
    sesionHub = datoHub("sesion").toLower();
    /* el hub escribe ahi una vez por segundo: si el fichero viene viejo, el servicio se fue y el
       contenido es un recuerdo (lo borra al pararse, pero un corte no) */
    long long hubViejo = 999;
    const QFileInfo fiHub(QString::fromStdString(ajuste::rutaHub()));
    if (fiHub.exists()) hubViejo = fiHub.lastModified().secsTo(QDateTime::currentDateTimeUtc());
    if (hub.empty() || hubViejo > 5) {
      botonEscritorio->setEnabled(false);
      notaEscritorio->setText(hub.empty()
          ? QString::fromUtf8("El vigilante (odyssey3d serve) no escribe en %1: no hay servicio corriendo "
                              "que pueda montar el overlay.").arg(QString::fromStdString(ajuste::rutaHub()))
          : QString::fromUtf8("El vigilante dejo de escribir hace %1 segundos: el servicio se fue, hay que "
                              "arrancarlo otra vez (`systemctl --user restart odyssey3d-serve`).").arg(hubViejo));
    } else {
      botonEscritorio->setEnabled(true);
      const bool puesto = (sesionHub == "escritorio");
      botonEscritorio->setText(puesto ? QString::fromUtf8("Quitar el overlay del escritorio")
                                      : QString::fromUtf8("Poner el overlay en el escritorio"));
      if (puesto)
        notaEscritorio->setText(QString::fromUtf8("PUESTO: el escritorio lleva la trama encima (%1, %2, red %3)."
                                                  " Tambien lo quita Tab o ESC sobre el visor.")
                                    .arg(datoHub("pantalla"), datoHub("modo"), datoHub("red")));
      else if (sesionHub == "video")
        notaEscritorio->setText(QString::fromUtf8(
            "Ahora hay una sesion de VIDEO abierta (%1): el boton espera a que se cierre para no pelearse con ella."
            ).arg(datoHub("ventana")));
      else
        notaEscritorio->setText(QString::fromUtf8("Quitado: la pantalla esta en 2D normal (%1, %2)."
                                                  ).arg(datoHub("pantalla"), datoHub("servicio")));
    }

    QString cab;
    if (est.empty()) {
      cab = QString::fromUtf8("El visor no esta escribiendo su estado: no hay sesion 3D abierta "
                              "(los cambios quedan esperando a que se abra).");
    } else {
      long long viejos = 999;
      const QFileInfo fi(QString::fromStdString(ajuste::rutaEstado()));
      if (fi.exists()) viejos = fi.lastModified().secsTo(QDateTime::currentDateTimeUtc());
      const char *vivo = viejos <= 3 ? "abierto y escribiendo" : "sin escribir hace poco (¿cerrado?)";
      cab = QString::fromUtf8("Visor ") + dato("visor") + " · " + vivo + " · " +
            QString::fromUtf8("trama ") + dato("trama") + " · " + QString::fromUtf8("conversor ") +
            dato("conversor") + "\n" + QString::fromUtf8("Red puesta: ") +
            (dato("modelo").isEmpty() ? QString::fromUtf8("(ninguna)") : dato("modelo")) + " en " +
            dato("tarjeta") + " · ultimo estado " + dato("hora");
      /* los numeros del visor, pero solo si el usuario no los esta tocando en esta ventana */
      for (const Fila &f : filas) {
        const auto it = est.find(f.clave->nombre);
        if (it == est.end()) continue;
        if (f.caja->hasFocus() || f.deslizador->isSliderDown()) continue;
        const double v = QString::fromStdString(it->second).toDouble();
        if (!qFuzzyCompare(v, f.caja->value())) {
          const bool bloqueada = f.caja->blockSignals(true);
          const bool bloqueada2 = f.deslizador->blockSignals(true);
          f.caja->setValue(v);
          f.deslizador->setValue(alDeslizador(*f.clave, v));
          f.caja->blockSignals(bloqueada);
          f.deslizador->blockSignals(bloqueada2);
        }
      }
      /* la red puesta y su guardado: el visor es el que cuenta los numeros de verdad, asi que el
         apunte se hace solo (y se borra solo si la red vuelve a estar de fabrica) */
      const std::string modelo = dato("modelo").toStdString();
      if (viejos <= 3 && !modelo.empty()) {
        std::map<std::string, double> vivos;
        for (const Fila &f : filas) {
          const auto it = est.find(f.clave->nombre);
          if (it == est.end()) continue;
          vivos[f.clave->nombre] = QString::fromStdString(it->second).replace(',', '.').toDouble();
        }
        if (vivos.size() == filas.size()) {
          const bool cambioRed = (modelo != redPuesta);
          if (ajuste::diferencias(vivos, fabrica) > 0)
            ajuste::guardarPerfil(modelo, vivos, nullptr);
          else
            ajuste::borrarPerfil(modelo, nullptr);
          if (cambioRed) {
            redPuesta = modelo;
            rellenarSel();
            rellenarDispo();
          }
          /* la lista de guardados solo se rehace si de verdad cambio algo: si no, el usuario perderia
             la seleccion mientras afinaba */
          std::string firma;
          for (const auto &p : ajuste::leerPerfiles()) {
            firma += p.first + "=";
            for (const auto &kv : p.second) firma += kv.first + ":" + std::to_string(kv.second) + ",";
            firma += ";";
          }
          if (firma != guardVisto) {
            guardVisto = firma;
            rellenarGuard();
          }
        }
      }
    }
    estado->setText(cab);
    /* el historial, por si acaso: solo lo volvemos a pintar si cambio */
    std::string actual;
    for (const std::string &l : ajuste::historial(40)) actual += l + "\n";
    if (actual != logVisto) {
      logVisto = actual;
      log->setPlainText(QString::fromStdString(actual));
      log->verticalScrollBar()->setValue(log->verticalScrollBar()->maximum());
    }
  });
  refresco.start();

  /* al arrancar: las redes del disco a la izquierda (NATIVAS primero, OTROS despues), a la derecha las
     que dejamos seleccionadas la ultima vez (el fichero `seleccion`), y los guardados. Si el visor tiene
     puesta una red que no esta en la lista, entra. */
  const std::map<std::string, std::string> arranque = ajuste::leerEstado();
  if (arranque.count("modelo")) redPuesta = arranque.at("modelo");
  rellenarDispo();
  rellenarSel();
  rellenarGuard();
  notaGuard->setText(QString::fromUtf8(
      "De fabrica: los valores del programa corregidos con %1.\nGuardados en %2 · la seleccion en %3.")
        .arg(QString::fromStdString(ajuste::rutaServeConf()), QString::fromStdString(ajuste::rutaPerfiles()),
             QString::fromStdString(ajuste::rutaSeleccion())));

  ventana.resize(1240, 900);
  ventana.show();
  return app.exec();
}
