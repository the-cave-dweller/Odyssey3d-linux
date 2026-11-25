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

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStandardItemModel>
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
#include <cstdio>
#include <ctime>
#include <map>
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
    std::printf("redes que tendria la lista desplegable: %d\n", (int)redes.size());
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

  /* ---- la red de profundidad: lo que la pastilla del visor no deja elegir ---- */
  auto *grupoRed = new QGroupBox(QString::fromUtf8("Red de profundidad (2D a 3D)"), &ventana);
  auto *cajaRed = new QHBoxLayout(grupoRed);
  auto *modelo = new QComboBox(grupoRed);
  modelo->setInsertPolicy(QComboBox::NoInsert);
  auto *aplicarModelo = new QPushButton(QString::fromUtf8("Aplicar al visor"), grupoRed);
  auto *buscarModelo = new QPushButton(QString::fromUtf8("Otro archivo…"), grupoRed);
  cajaRed->addWidget(modelo, 1);
  cajaRed->addWidget(aplicarModelo);
  cajaRed->addWidget(buscarModelo);
  principal->addWidget(grupoRed);

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
  auto *grupoNum = new QGroupBox(QString::fromUtf8("Numeros de la trama y del conversor"), &ventana);
  auto *forma = new QFormLayout(grupoNum);
  forma->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
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
  }
  principal->addWidget(grupoNum);

  /* ---- recetas: dejar guardada una combinacion entera y volver a ella ---- */
  auto *grupoReceta = new QGroupBox(QString::fromUtf8("Receta (todos los numeros de una vez)"), &ventana);
  auto *cajaReceta = new QHBoxLayout(grupoReceta);
  auto *nombre = new QLineEdit("prueba", grupoReceta);
  auto *guardar = new QPushButton(QString::fromUtf8("Guardar"), grupoReceta);
  auto *cargar = new QPushButton(QString::fromUtf8("Cargar en el visor"), grupoReceta);
  cajaReceta->addWidget(nombre, 1);
  cajaReceta->addWidget(guardar);
  cajaReceta->addWidget(cargar);
  principal->addWidget(grupoReceta);

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
  std::map<std::string, double> pendiente;
  QTimer junta;
  junta.setSingleShot(true);
  junta.setInterval(350);
  const auto mandarPendientes = [&pendiente, &modelo, &aplicarModelo]() {
    if (pendiente.empty()) return;
    std::string err;
    if (!ajuste::escribir(pendiente, std::string(), &err))
      aplicarModelo->setText(QString::fromUtf8(("No se pudo escribir: " + err).c_str()));
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

  QObject::connect(aplicarModelo, &QPushButton::clicked, &ventana, [&]() {
    const QString ruta = modelo->currentData().toString();
    if (ruta.isEmpty()) return;
    std::string err;
    if (!ajuste::escribir({}, std::string(ruta.toUtf8().constData()), &err))
      aplicarModelo->setText(QString::fromUtf8(("No se pudo escribir: " + err).c_str()));
    else
      aplicarModelo->setText(QString::fromUtf8("pedido enviado"));
  });
  QObject::connect(buscarModelo, &QPushButton::clicked, &ventana, [&]() {
    const QString ruta = QFileDialog::getOpenFileName(&ventana, QString::fromUtf8("Modelo .onnx"),
                                                      QDir::homePath() + "/.local/share/odyssey3d/modelos",
                                                      "*.onnx");
    if (ruta.isEmpty()) return;
    const QString absoluta = QFileInfo(ruta).absoluteFilePath();
    /* si ya estaba en la lista (porque vive en una de las carpetas que escaneamos), seleccionarla y listo */
    for (int i = 0; i < modelo->count(); ++i) {
      if (modelo->itemData(i).toString() == absoluta) {
        modelo->setCurrentIndex(i);
        return;
      }
    }
    /* y si no, anadirla marcada como OTRA: la elegimos a mano y no sabemos como calza aqui */
    QString texto = QFileInfo(absoluta).fileName() + "  (" +
                    QString::number(QFileInfo(absoluta).size() / 1024) + " KB)   —  otros (a mano)";
    const QString aviso = avisoConocido(QFileInfo(absoluta).fileName());
    if (!aviso.isEmpty()) texto += " · " + aviso;
    modelo->addItem(texto, absoluta);
    modelo->setCurrentIndex(modelo->count() - 1);
  });

  QObject::connect(guardar, &QPushButton::clicked, &ventana, [&]() {
    std::map<std::string, double> valores;
    for (const Fila &f : filas) valores[f.clave->nombre] = f.caja->value();
    std::string err;
    if (!ajuste::escribir(valores, std::string(), &err)) {
      guardar->setText(QString::fromUtf8(("No se pudo: " + err).c_str()));
      return;
    }
    const QString ruta = QString::fromStdString(ajuste::rutaEstado()) + "-receta-" + nombre->text() + ".ajuste";
    QFile archivo(ruta);
    if (!archivo.open(QIODevice::WriteOnly | QIODevice::Text)) {
      guardar->setText(QString::fromUtf8("No se pudo guardar la receta"));
      return;
    }
    QTextStream ts(&archivo);
    for (const Fila &f : filas) ts << f.clave->nombre << " = " << f.caja->value() << "\n";
    guardar->setText(QString::fromUtf8("guardada"));
  });
  QObject::connect(cargar, &QPushButton::clicked, &ventana, [&]() {
    const QString ruta = QString::fromStdString(ajuste::rutaEstado()) + "-receta-" + nombre->text() + ".ajuste";
    QFile archivo(ruta);
    if (!archivo.open(QIODevice::ReadOnly | QIODevice::Text)) {
      cargar->setText(QString::fromUtf8("no existe esa receta"));
      return;
    }
    std::map<std::string, double> valores;
    QTextStream ts(&archivo);
    while (!ts.atEnd()) {
      const QString l = ts.readLine().trimmed();
      const int eq = l.indexOf('=');
      if (l.isEmpty() || l.startsWith('#') || eq < 0) continue;
      valores[l.left(eq).trimmed().toStdString()] = l.mid(eq + 1).trimmed().replace(',', '.').toDouble();
    }
    ajuste::escribir(valores, std::string(), nullptr);
    cargar->setText(QString::fromUtf8("enviada"));
  });

  /* ---------------- la lectura del estado, dos veces por segundo ---------------- */
  QTimer refresco;
  refresco.setInterval(500);
  std::string logVisto;
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

  /* las redes que hay en el disco, al arrancar: separadas en NATIVAS (las instaladas en la maquina,
     que son las que hemos probado aqui) y OTRAS (la memoria USB, carpetas sueltas, lo que diga
     ODYSSEY3D_MODELOS). Las dos lineas de titulo son separadores: no se pueden elegir. */
  const std::map<std::string, std::string> arranque = ajuste::leerEstado();
  const std::string puesto = arranque.count("modelo") ? arranque.at("modelo") : std::string();
  auto *listaModelo = qobject_cast<QStandardItemModel *>(modelo->model());
  const auto poneTitulo = [&](const QString &texto) {
    modelo->addItem(texto);
    if (listaModelo)
      if (QStandardItem *it = listaModelo->item(modelo->count() - 1)) it->setEnabled(false);
  };
  const auto poneRed = [&](const Red &r) {
    const QFileInfo fi(r.ruta);
    QString texto = fi.fileName() + "  (" + QString::number(fi.size() / 1024) + " KB)";
    if (!r.aviso.isEmpty()) texto += "   —  " + r.aviso;
    modelo->addItem(texto, r.ruta);
    if (!puesto.empty() && fi.absoluteFilePath().toStdString() == puesto)
      modelo->setCurrentIndex(modelo->count() - 1);
  };
  const QList<Red> redes = redesDelDisco(puesto);
  int nativas = 0, otras = 0;
  for (const Red &r : redes) {
    if (r.nativa) ++nativas;
    else ++otras;
  }
  if (nativas > 0) {
    poneTitulo(QString::fromUtf8("· NATIVAS: instaladas en la maquina, las de siempre ·"));
    for (const Red &r : redes)
      if (r.nativa) poneRed(r);
  }
  if (otras > 0) {
    poneTitulo(QString::fromUtf8("· OTROS: en la USB o en carpetas sueltas (sin probar aqui) ·"));
    for (const Red &r : redes)
      if (!r.nativa) poneRed(r);
  }
  if (redes.isEmpty()) modelo->addItem(QString::fromUtf8("(no hay .onnx en las carpetas de siempre)"));

  ventana.resize(760, 760);
  ventana.show();
  return app.exec();
}
