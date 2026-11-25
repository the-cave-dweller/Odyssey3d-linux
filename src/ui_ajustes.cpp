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

static QStringList rutasDeModelos(const std::string &modeloEnUso) {
  QStringList dirs;
  const char *home = std::getenv("HOME");
  const std::string h(home ? home : "/home");
  dirs << QString::fromStdString(h + "/.local/share/odyssey3d/modelos");
  dirs << "/run/media/rodolfo/067ABD417ABD2DF5/AnalisisOdyssey/modelos";
  dirs << QString::fromStdString(h + "/Documentos/LU agent/odyssey3d-linux/modelos");
  if (const char *e = std::getenv("ODYSSEY3D_MODELOS"))
    for (const QString &d : QString(e).split(':', Qt::SkipEmptyParts)) dirs << d;
  if (!modeloEnUso.empty()) {
    const std::string r = modeloEnUso;
    const size_t b = r.find_last_of('/');
    if (b != std::string::npos) dirs << QString::fromStdString(r.substr(0, b));
  }
  QStringList salida;
  const QStringList vistos = dirs;
  for (const QString &d : vistos) {
    QDir dir(d);
    if (!dir.exists()) continue;
    for (const QFileInfo &fi : dir.entryInfoList({"*.onnx"}, QDir::Files, QDir::Name)) {
      const QString ruta = fi.absoluteFilePath();
      if (!salida.contains(ruta)) salida << ruta;
    }
  }
  salida.sort();
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
    modelo->addItem(QFileInfo(ruta).fileName() + "  (" + QString::number(QFileInfo(ruta).size() / 1024) +
                               " KB)",
                    ruta);
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

  /* las redes que hay en el disco, al arrancar */
  const std::map<std::string, std::string> arranque = ajuste::leerEstado();
  const std::string puesto = arranque.count("modelo") ? arranque.at("modelo") : std::string();
  for (const QString &ruta : rutasDeModelos(puesto)) {
    const QFileInfo fi(ruta);
    modelo->addItem(fi.fileName() + "  (" + QString::number(fi.size() / 1024) + " KB)", ruta);
    if (!puesto.empty() && fi.absoluteFilePath().toStdString() == puesto) modelo->setCurrentIndex(modelo->count() - 1);
  }
  if (modelo->count() == 0) modelo->addItem(QString::fromUtf8("(no hay .onnx en las carpetas de siempre)"));

  ventana.resize(760, 760);
  ventana.show();
  return app.exec();
}
