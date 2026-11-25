/* ajuste.cpp — los tres ficheros del canal de ajuste vivo (ver ajuste.h). */
#include "ajuste.h"

#include "util.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <system_error>
#include <unistd.h>

namespace ajuste {

/* Los topes sanos: son los mismos que imponen las teclas y el sombreador. Pasar de ahi no arregla
   nada (la trama sale gris o el relieve se sale del presupuesto de la lente por ojo). */
const std::vector<Clave> &claves() {
  static const std::vector<Clave> k = {
      {"fase", "fase de la trama", -1.0, 1.0, 0.0005, false},
      {"paso", "paso de la lente", 0.5, 2.0, 0.0002, false},
      {"filtro", "filtro de la trama", 0.0, 30.0, 0.5, false},
      {"crosstalk", "crosstalk", -0.2, 0.2, 0.002, false},
      /* la compensacion PAREJA de la diafonia (v0.83): cuanta imagen del otro ojo se resta en toda
         la pantalla. `crosstalk` de arriba solo pesa hacia los bordes (en el centro vale 0). */
      {"crosstalk_base", "crosstalk parejo", 0.0, 0.25, 0.002, false},
      {"conv", "convergencia", -0.05, 0.05, 0.0005, false},
      {"rampa_x", "rampa de fase en x", -0.05, 0.05, 0.00001, false},
      {"rampa_y", "rampa de fase en y", -0.05, 0.05, 0.00001, false},
      {"fondo", "ganancia del fondo", 0.2, 2.0, 0.05, false},
      {"frente", "ganancia del frente", 0.2, 2.0, 0.05, false},
      {"empuje_fondo", "empuje del fondo", -20.0, 20.0, 0.5, false},
      {"empuje_frente", "empuje del frente", -20.0, 20.0, 0.5, false},
      {"planos", "dureza de los dos planos", 0.0, 1.0, 0.05, false},
      {"muestras", "muestras intermedias", 0.0, 3.0, 1.0, true},
      {"temblor", "temblor del desplazamiento", 0.0, 2.0, 0.01, false},
      {"canto", "ancho del puente del canto", 0.0005, 0.05, 0.0005, false},
      {"fase_temblor", "temblor de la fase", 0.0, 1.0, 0.01, false},
      {"relieve", "3D Salida", 0, 100, 1, true},
      {"apoyo", "plano de apoyo", 0, 100, 1, true},
  };
  return k;
}

const Clave *buscarClave(const std::string &nombre) {
  for (const Clave &c : claves())
    if (nombre == c.nombre) return &c;
  return nullptr;
}

std::string rutaAjuste() { return rutaEstadoOdyssey("ajuste"); }
std::string rutaEstado() { return rutaEstadoOdyssey("estado"); }
std::string rutaHistorial() { return rutaEstadoOdyssey("preferencias.log"); }
std::string rutaHub() { return rutaEstadoOdyssey("hub"); }

bool esOrden(const std::string &nombre) {
  return nombre == "overlay" || nombre == "camara" || nombre == "mono";
}

/* Las ordenes del fichero que llevan palabra o ruta en vez de numero: las recoge `leer`
   en su mapa de texto. `camara` y `mono` las atiende el vigilante; `capa` la atiende el VISOR
   (`capa = ocultar|mostrar|alternar` es la tecla Ctrl+Shift+F1 mandada por fichero, para cuando el
   teclado de la pantalla lo tiene otra ventana a pantalla completa), y `legado` tambien la atiende el
   VISOR (modo de legado: trama de fabrica, sin conversor y pastilla sencilla). `vista` tambien es del
   VISOR: `vista = 1` le pide que vuelque el cuadro de la camara a `camara.jpg` unas ocho veces por
   segundo en vez de cada dos segundos, que es lo que necesita la casilla de Previsualizacion de la
   ventana de ajustes para ensenar la camara en vivo mientras hay una sesion abierta; `vista = 0` la
   devuelve a su cadencia tranquila. */
static bool esOrdenTexto(const std::string &nombre) {
  return nombre == "camara" || nombre == "mono" || nombre == "capa" || nombre == "3d" ||
         nombre == "legado" || nombre == "vista" || nombre == "barra";
}

static std::string recortar(std::string s) {
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) ++i;
  return s.substr(i);
}

static std::string numeroADouble(std::string v) {
  for (char &c : v)
    if (c == ',') c = '.';
  return v;
}

static std::string expandir(const std::string &ruta) {
  if (ruta.size() > 1 && ruta[0] == '~') {
    const char *home = std::getenv("HOME");
    return std::string(home ? home : "") + ruta.substr(1);
  }
  return ruta;
}

bool leer(std::map<std::string, double> &numeros, std::string &modelo,
          std::map<std::string, std::string> *texto) {
  numeros.clear();
  modelo.clear();
  if (texto) texto->clear();
  std::string contenido;
  if (!readSmallFile(rutaAjuste(), contenido)) return false;   /* sin canal no hay nada que aplicar */
  size_t inicio = 0;
  while (inicio <= contenido.size()) {
    size_t fin = contenido.find('\n', inicio);
    if (fin == std::string::npos) fin = contenido.size();
    const std::string l = recortar(contenido.substr(inicio, fin - inicio));
    inicio = fin + 1;
    if (l.empty() || l[0] == '#') continue;
    const size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    std::string k = strLower(recortar(l.substr(0, eq)));
    const std::string v = recortar(l.substr(eq + 1));
    if (k == "modelo") {
      if (!v.empty()) modelo = expandir(v);
      continue;
    }
    if (k == "borrar" || k == "limpiar") continue;   /* no es un numero: es una orden */
    if (esOrdenTexto(k)) {
      if (texto) (*texto)[k] = v;
      continue;   /* `camara`, `mono`: palabra, no numero */
    }
    double d = std::strtod(numeroADouble(v).c_str(), nullptr);
    if (const Clave *c = buscarClave(k)) {
      if (d < c->minimo) d = c->minimo;
      if (d > c->maximo) d = c->maximo;
      if (c->entero) d = std::floor(d + 0.5);
    }
    numeros[k] = d;
  }
  return true;
}

bool Lector::cambio(std::map<std::string, double> &numeros, std::string &modelo,
                    std::map<std::string, std::string> *texto) {
  std::error_code ec;
  const std::string ruta = rutaAjuste();
  const auto tam = (long long)std::filesystem::file_size(ruta, ec);
  if (ec) {
    marca_ = -1; numeros.clear(); modelo.clear();
    if (texto) texto->clear();
    return false;   /* no hay fichero */
  }
  const auto mt = std::filesystem::last_write_time(ruta, ec);
  if (ec) { marca_ = -1; return false; }
  const long long mtime = (long long)std::chrono::duration_cast<std::chrono::nanoseconds>(mt.time_since_epoch()).count();
  const long long marca = mtime * 1024 + tam;
  if (marca == marca_) return false;
  marca_ = marca;
  return leer(numeros, modelo, texto);
}

bool escribir(const std::map<std::string, double> &numeros, const std::string &modelo, std::string *err) {
  return escribir(numeros, modelo, std::map<std::string, std::string>(), err);
}

bool escribir(const std::map<std::string, double> &numeros, const std::string &modelo,
              const std::map<std::string, std::string> &ordenes, std::string *err) {
  /* El fichero es una ORDEN, no la memoria del visor: se reescribe ENTERO con lo que se mando ahora,
     sin arrastrar lo de mandatos anteriores. Si se arrastrara, un numero que alguien toco despues con
     las teclas dentro del visor volveria atras al reenviar otro numero distinto. */
  std::string texto = "# Ajuste vivo del visor: lo escribe la ventana de ajustes o"
                      " `odysseyvk ctl ajuste`, y lo lee el visor cada decima de segundo.\n"
                      "# Son numeros sueltos, en las mismas palabras que serve.conf; se quita todo con"
                      " `odysseyvk ctl ajuste limpiar`.\n";
  for (const auto &kv : numeros) {
    if (kv.first == "modelo") continue;
    texto += kv.first + " = " + std::to_string(kv.second) + "\n";
  }
  /* `modelo` tambien puede venir dentro de `numeros` (la ventana lo manda por ahi a veces). */
  std::string pedido = modelo;
  for (const auto &kv : numeros)
    if (kv.first == "modelo") pedido = kv.second;
  if (!pedido.empty()) texto += "modelo = " + pedido + "\n";
  for (const auto &kv : ordenes)
    if (esOrdenTexto(kv.first) && !kv.second.empty()) texto += kv.first + " = " + kv.second + "\n";

  const std::string destino = rutaAjuste();
  const std::string temporal = destino + ".tmp";
  FILE *f = std::fopen(temporal.c_str(), "we");
  if (!f) {
    if (err) *err = "no se pudo escribir " + temporal + ": " + std::strerror(errno);
    return false;
  }
  std::fputs(texto.c_str(), f);
  std::fclose(f);
  if (std::rename(temporal.c_str(), destino.c_str()) != 0) {
    if (err) *err = "no se pudo dejar en su sitio " + destino + ": " + std::strerror(errno);
    return false;
  }
  return true;
}

void escribirEstado(const std::string &texto) {
  const std::string destino = rutaEstado();
  const std::string temporal = destino + ".tmp";
  FILE *f = std::fopen(temporal.c_str(), "we");
  if (!f) return;
  std::fputs(texto.c_str(), f);
  std::fputc('\n', f);
  std::fclose(f);
  std::rename(temporal.c_str(), destino.c_str());   /* que nadie lo lea a medias */
}

void registrar(const std::string &clave, const std::string &antes, const std::string &ahora,
               const std::string &modelo, const char *fuente) {
  const std::string ruta = rutaHistorial();
  std::error_code ec;
  const auto grande = std::filesystem::file_size(ruta, ec);
  if (!ec && grande > 256u * 1024u) std::filesystem::rename(ruta, ruta + ".anterior", ec);
  FILE *f = std::fopen(ruta.c_str(), "ae");
  if (!f) return;
  char hora[32] = {0};
  const std::time_t t = std::time(nullptr);
  std::tm tm{};
  localtime_r(&t, &tm);
  std::strftime(hora, sizeof hora, "%Y-%m-%d %H:%M:%S", &tm);
  std::fprintf(f, "%s  %-14s %-12s -> %-12s  red %s  por %s\n", hora, clave.c_str(), antes.c_str(),
               ahora.c_str(), modelo.empty() ? "sin conversor" : modelo.c_str(), fuente ? fuente : "?");
  std::fclose(f);
}

void escribirEstadoHub(const std::string &texto) {
  const std::string destino = rutaHub();
  const std::string temporal = destino + ".tmp";
  FILE *f = std::fopen(temporal.c_str(), "we");
  if (!f) return;
  std::fputs(texto.c_str(), f);
  std::fputc('\n', f);
  std::fclose(f);
  std::rename(temporal.c_str(), destino.c_str());   /* que nadie lo lea a medias */
}

/* Pares `clave = valor` de un fichero de estado. */
static std::map<std::string, std::string> paresDe(const std::string &ruta) {
  std::map<std::string, std::string> pares;
  std::string contenido;
  if (!readSmallFile(ruta, contenido)) return pares;
  size_t inicio = 0;
  while (inicio <= contenido.size()) {
    size_t fin = contenido.find('\n', inicio);
    if (fin == std::string::npos) fin = contenido.size();
    const std::string l = recortar(contenido.substr(inicio, fin - inicio));
    inicio = fin + 1;
    if (l.empty() || l[0] == '#') continue;
    const size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    pares[strLower(recortar(l.substr(0, eq)))] = recortar(l.substr(eq + 1));
  }
  return pares;
}

std::map<std::string, std::string> leerEstado() { return paresDe(rutaEstado()); }

std::string rutaCamaraVista() { return rutaEstadoOdyssey("camara.jpg"); }

bool escribirCamaraVista(const std::vector<uint8_t> &jpeg, std::string *err) {
  /* el cuadro tal cual lo solto la camara (MJPG): no se reencodifica, para que la ventana
     ensene exactamente lo que esta viendo el rastreo. Se deja atomicamente, como todo aqui. */
  if (jpeg.empty() || jpeg.size() > 8u * 1024u * 1024u) return false;
  const std::string destino = rutaCamaraVista();
  const std::string temporal = destino + ".tmp";
  FILE *f = std::fopen(temporal.c_str(), "wb");
  if (!f) {
    if (err) *err = "no se pudo escribir " + temporal + ": " + std::strerror(errno);
    return false;
  }
  const size_t escritos = std::fwrite(jpeg.data(), 1, jpeg.size(), f);
  std::fclose(f);
  if (escritos != jpeg.size()) {
    if (err) *err = "cuadro truncado en " + temporal;
    return false;
  }
  if (std::rename(temporal.c_str(), destino.c_str()) != 0) {
    if (err) *err = "no se pudo dejar en su sitio " + destino + ": " + std::strerror(errno);
    return false;
  }
  return true;
}

std::map<std::string, std::string> leerServeConf() { return paresDe(rutaServeConf()); }
std::map<std::string, std::string> leerEstadoHub() { return paresDe(rutaHub()); }

std::string textoAjuste() {
  std::string s;
  readSmallFile(rutaAjuste(), s);
  return s;
}

std::vector<std::string> historial(int n) {
  std::string s;
  std::vector<std::string> lineas;
  if (!readSmallFile(rutaHistorial(), s)) return lineas;
  size_t inicio = 0;
  while (inicio <= s.size()) {
    size_t fin = s.find('\n', inicio);
    if (fin == std::string::npos) fin = s.size();
    const std::string l = recortar(s.substr(inicio, fin - inicio));
    inicio = fin + 1;
    if (!l.empty()) lineas.push_back(l);
    if (fin == s.size()) break;
  }
  if (n > 0 && (int)lineas.size() > n) lineas.erase(lineas.begin(), lineas.end() - n);
  return lineas;
}

/* ---------------- lo de fabrica y los guardados por red ---------------- */

std::string rutaServeConf() {
  /* la misma ruta que lee el vigilante (src/main.cpp), para que "de fabrica" signifique lo mismo aqui
     y alla: lo que trae el programa, corregido con lo que el usuario puso en serve.conf. */
  const char *home = std::getenv("HOME");
  return std::string(home ? home : "") + "/.config/odysseyvk/serve.conf";
}

std::map<std::string, double> valoresFabrica() {
  /* Los numeros con los que arranca el visor, leidos del codigo: weave.h (10 de filtro, 1 de paso y de
     ganancias, 0 del resto), vkui.h y vkview.cpp (3D Salida 50 y plano de apoyo 55 en cuanto la trama se
     pone), y el temblor y el canto que trae VkWeave. No son topes: son los valores reales de arranque. */
  static const struct { const char *clave; double valor; } kFabrica[] = {
      {"fase", 0.0},          {"paso", 1.0},          {"filtro", 10.0},
      {"crosstalk", 0.0},     {"crosstalk_base", 0.0128535}, {"conv", 0.0}, {"rampa_x", 0.0},
      {"rampa_y", 0.0},       {"rampa_x_teclas", 0.0}, {"rampa_y_teclas", 0.0},
      {"fondo", 1.0},         {"frente", 1.0},        {"empuje_fondo", 0.0},
      {"empuje_frente", 0.0}, {"planos", 0.0},        {"muestras", 0.0},
      {"temblor", 0.25},      {"canto", 0.004},       {"fase_temblor", 0.0},
      {"relieve", 50.0},      {"apoyo", 55.0},
  };
  std::map<std::string, double> v;
  for (const auto &f : kFabrica) v[f.clave] = f.valor;
  /* serve.conf manda encima: usa las mismas palabras que este canal. */
  for (const auto &par : paresDe(rutaServeConf())) {
    const Clave *c = buscarClave(par.first);
    if (!c) continue;                     /* `modo`, `pantalla`, `espera`... no son numeros de la trama */
    double d = std::strtod(numeroADouble(par.second).c_str(), nullptr);
    if (d < c->minimo) d = c->minimo;
    if (d > c->maximo) d = c->maximo;
    if (c->entero) d = std::floor(d + 0.5);
    v[par.first] = d;
  }
  return v;
}

bool fijarEnServeConf(const std::string &clave, const std::string &valor, std::string *err) {
  /* serve.conf es donde vive de verdad la eleccion de camara y demas ajustes de arranque: se cambia
     UNA linea y se deja el resto tal cual, comentarios incluidos. Si la clave no estaba, se anade al
     final. Antes de tocarlo se deja una copia en `serve.conf.auterior`, por si la eleccion nueva
     deja el rastreo sin camara y hay que volver. */
  const std::string ruta = rutaServeConf();
  std::string contenido;
  const bool existia = readSmallFile(ruta, contenido);
  std::vector<std::string> lineas;
  size_t inicio = 0;
  while (inicio <= contenido.size()) {
    size_t fin = contenido.find('\n', inicio);
    const bool ultimo = (fin == std::string::npos);
    if (ultimo) fin = contenido.size();
    lineas.push_back(contenido.substr(inicio, fin - inicio));
    inicio = fin + 1;
    if (ultimo) break;
  }
  bool puesto = false;
  for (std::string &l : lineas) {
    const std::string limpio = recortar(l);
    if (limpio.empty() || limpio[0] == '#') continue;
    const size_t eq = limpio.find('=');
    if (eq == std::string::npos) continue;
    if (strLower(recortar(limpio.substr(0, eq))) != clave) continue;
    l = clave + " = " + valor;
    puesto = true;
  }
  if (!puesto) lineas.push_back(clave + " = " + valor);
  std::string salida;
  for (const std::string &l : lineas) salida += l + "\n";

  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(ruta).parent_path(), ec);
  if (existia) {
    /* El nombre del respaldo lleva "u" (auterior) desde que existe y asi lo anuncian el README y la
       ventana de ajustes; aqui estaba escrito ".anterior", asi que desde v0.57 en adelante habia dos
       ficheros en ~/.config/odysseyvk: el respaldito nuevo y el viejo que prometia la documentacion.
       Se escribe el nombre de siempre. (El `preferencias.log` si rota a `.anterior`: son cosas
       distintas y ahi no se promise otro nombre.) */
    std::filesystem::copy_file(ruta, ruta + ".auterior",
                               std::filesystem::copy_options::overwrite_existing, ec);
  }
  const std::string temporal = ruta + ".tmp";
  FILE *f = std::fopen(temporal.c_str(), "we");
  if (!f) {
    if (err) *err = "no se pudo escribir " + temporal + ": " + std::strerror(errno);
    return false;
  }
  std::fputs(salida.c_str(), f);
  std::fclose(f);
  if (std::rename(temporal.c_str(), ruta.c_str()) != 0) {
    if (err) *err = "no se pudo dejar en su sitio " + ruta + ": " + std::strerror(errno);
    return false;
  }
  return true;
}

/* ---------------- «no volver a preguntar» ---------------- */

std::string rutaRecordar() { return rutaEstadoOdyssey("recordar"); }
std::string rutaBarra() { return rutaEstadoOdyssey("barra"); }

bool pedirRecordar(const std::string &app, const std::string &modo, std::string *err) {
  if (app.empty()) {
    if (err) *err = "no se sabe de que aplicacion es la ventana";
    return false;
  }
  const std::string ruta = rutaRecordar(), temporal = ruta + ".tmp";
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(ruta).parent_path(), ec);
  FILE *f = std::fopen(temporal.c_str(), "we");
  if (!f) {
    if (err) *err = "no se pudo escribir " + temporal + ": " + std::strerror(errno);
    return false;
  }
  std::fprintf(f, "app = %s\nmodo = %s\n", app.c_str(), modo.empty() ? "no" : modo.c_str());
  std::fclose(f);
  if (std::rename(temporal.c_str(), ruta.c_str()) != 0) {
    if (err) *err = "no se pudo dejar en su sitio " + ruta + ": " + std::strerror(errno);
    return false;
  }
  return true;
}

bool tomarRecordar(std::string &app, std::string &modo) {
  const std::string ruta = rutaRecordar();
  if (::access(ruta.c_str(), R_OK) != 0) return false;   /* lo normal: no hay recado */
  const std::map<std::string, std::string> pares = paresDe(ruta);
  ::unlink(ruta.c_str());
  const auto a = pares.find("app"), m = pares.find("modo");
  if (a == pares.end() || m == pares.end() || a->second.empty()) return false;
  app = strLower(a->second);
  modo = strLower(m->second);
  if (modo != "sbs" && modo != "tb" && modo != "conv" && modo != "flat" && modo != "suave")
    modo.clear();   /* `no` y lo raro */
  return true;
}

bool fijarAutoEnServeConf(const std::string &app, const std::string &modo, std::string *err) {
  const std::string ruta = rutaServeConf();
  std::string contenido;
  const bool existia = readSmallFile(ruta, contenido);
  std::vector<std::string> lineas;
  size_t inicio = 0;
  while (inicio < contenido.size()) {
    size_t fin = contenido.find('\n', inicio);
    if (fin == std::string::npos) fin = contenido.size();
    lineas.push_back(contenido.substr(inicio, fin - inicio));
    inicio = fin + 1;
  }
  bool cambio = false;
  std::vector<std::string> salen;
  for (const std::string &l : lineas) {
    const std::string limpio = recortar(l);
    const size_t eq = limpio.find('=');
    if (limpio.empty() || limpio[0] == '#' || eq == std::string::npos ||
        strLower(recortar(limpio.substr(0, eq))) != "auto") {
      salen.push_back(l);
      continue;
    }
    /* auto = mpv:sbs, haruna:tb  -> se quita solo el par de esta aplicacion */
    std::string resto, par;
    const std::string valor = limpio.substr(eq + 1) + ",";
    for (char c : valor) {
      if (c != ',' && c != ' ' && c != '\t') {
        par += c;
        continue;
      }
      if (par.empty()) continue;
      const size_t dd = par.find(':');
      if (dd != std::string::npos && strLower(par.substr(0, dd)) == app) {
        cambio = true;
      } else {
        resto += (resto.empty() ? "" : ", ") + par;
      }
      par.clear();
    }
    if (!resto.empty()) salen.push_back("auto = " + resto);
  }
  if (!modo.empty()) {
    salen.push_back("auto = " + app + ":" + modo);
    cambio = true;
  }
  if (!cambio) return true;   /* no estaba apuntada y no hay nada que apuntar */
  std::string salida;
  for (const std::string &l : salen) salida += l + "\n";
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(ruta).parent_path(), ec);
  if (existia)
    std::filesystem::copy_file(ruta, ruta + ".auterior", std::filesystem::copy_options::overwrite_existing, ec);
  const std::string temporal = ruta + ".tmp";
  FILE *f = std::fopen(temporal.c_str(), "we");
  if (!f) {
    if (err) *err = "no se pudo escribir " + temporal + ": " + std::strerror(errno);
    return false;
  }
  std::fputs(salida.c_str(), f);
  std::fclose(f);
  if (std::rename(temporal.c_str(), ruta.c_str()) != 0) {
    if (err) *err = "no se pudo dejar en su sitio " + ruta + ": " + std::strerror(errno);
    return false;
  }
  return true;
}

int diferencias(const std::map<std::string, double> &numeros, const std::map<std::string, double> &base,
                std::vector<std::string> *nombres) {
  int n = 0;
  for (const auto &kv : numeros) {
    const auto it = base.find(kv.first);
    const double ref = it == base.end() ? 0.0 : it->second;
    if (std::fabs(kv.second - ref) > 1e-6) {
      ++n;
      if (nombres) nombres->push_back(kv.first);
    }
  }
  return n;
}

std::string rutaPerfiles() { return rutaEstadoOdyssey("perfiles"); }

std::map<std::string, std::map<std::string, double>> leerPerfiles() {
  std::map<std::string, std::map<std::string, double>> todo;
  std::string contenido;
  if (!readSmallFile(rutaPerfiles(), contenido)) return todo;
  std::string seccion;
  size_t inicio = 0;
  while (inicio <= contenido.size()) {
    size_t fin = contenido.find('\n', inicio);
    if (fin == std::string::npos) fin = contenido.size();
    const std::string l = recortar(contenido.substr(inicio, fin - inicio));
    inicio = fin + 1;
    if (l.empty() || l[0] == '#') continue;
    if (l.front() == '[') {
      const size_t eq = l.find('=');
      const size_t cierre = l.find(']');
      if (eq == std::string::npos || cierre == std::string::npos || cierre < eq) {
        seccion.clear();
        continue;
      }
      seccion = expandir(recortar(l.substr(eq + 1, cierre - eq - 1)));
      continue;
    }
    if (seccion.empty()) continue;
    const size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    const std::string k = strLower(recortar(l.substr(0, eq)));
    if (!buscarClave(k)) continue;   /* si manoseamos el fichero a mano, lo desconocido se ignora */
    const std::string valor = recortar(l.substr(eq + 1));
    todo[seccion][k] = std::strtod(numeroADouble(valor).c_str(), nullptr);
  }
  return todo;
}

static bool escribirPerfiles(const std::map<std::string, std::map<std::string, double>> &todo,
                             std::string *err) {
  const std::string destino = rutaPerfiles();
  const std::string temporal = destino + ".tmp";
  FILE *f = std::fopen(temporal.c_str(), "we");
  if (!f) {
    if (err) *err = "no se pudo escribir " + temporal + ": " + std::strerror(errno);
    return false;
  }
  std::fputs("# Los numeros que se dejaron puestos para cada red (.onnx). Sirven para dos cosas: saber\n"
             "# que red esta modificada respecto de lo de fabrica, y poder volver atras con el boton de\n"
             "# revertir. Se quitan los renglones a mano si hace falta; lo desconocido se ignora.\n",
             f);
  for (const auto &p : todo) {
    std::fprintf(f, "\n[ruta=%s]\n", p.first.c_str());
    for (const auto &kv : p.second) std::fprintf(f, "%s = %s\n", kv.first.c_str(),
                                                 std::to_string(kv.second).c_str());
  }
  std::fclose(f);
  if (std::rename(temporal.c_str(), destino.c_str()) != 0) {
    if (err) *err = "no se pudo dejar en su sitio " + destino + ": " + std::strerror(errno);
    return false;
  }
  return true;
}

bool guardarPerfil(const std::string &modelo, const std::map<std::string, double> &numeros,
                   std::string *err) {
  if (modelo.empty()) {
    if (err) *err = "no hay red que anotar";
    return false;
  }
  auto todo = leerPerfiles();
  if (todo[modelo] == numeros) return true;   /* no reescribir por reescribir */
  todo[modelo] = numeros;
  return escribirPerfiles(todo, err);
}

bool borrarPerfil(const std::string &modelo, std::string *err) {
  auto todo = leerPerfiles();
  if (!todo.count(modelo)) return true;
  todo.erase(modelo);
  return escribirPerfiles(todo, err);
}

std::string rutaSeleccion() { return rutaEstadoOdyssey("seleccion"); }

std::vector<std::string> leerSeleccion() {
  std::vector<std::string> rutas;
  std::string contenido;
  if (!readSmallFile(rutaSeleccion(), contenido)) return rutas;
  size_t inicio = 0;
  while (inicio <= contenido.size()) {
    size_t fin = contenido.find('\n', inicio);
    if (fin == std::string::npos) fin = contenido.size();
    const std::string l = recortar(contenido.substr(inicio, fin - inicio));
    inicio = fin + 1;
    if (l.empty() || l[0] == '#') continue;
    rutas.push_back(expandir(l));
  }
  return rutas;
}

bool guardarSeleccion(const std::vector<std::string> &rutas, std::string *err) {
  const std::string destino = rutaSeleccion();
  const std::string temporal = destino + ".tmp";
  FILE *f = std::fopen(temporal.c_str(), "we");
  if (!f) {
    if (err) *err = "no se pudo escribir " + temporal + ": " + std::strerror(errno);
    return false;
  }
  std::fputs("# Las redes que la ventana de ajustes tiene a mano (la lista de la derecha). Son solo\n"
             "# rutas: el visor no las lee. Una por linea.\n", f);
  for (const std::string &r : rutas) std::fprintf(f, "%s\n", r.c_str());
  std::fclose(f);
  if (std::rename(temporal.c_str(), destino.c_str()) != 0) {
    if (err) *err = "no se pudo dejar en su sitio " + destino + ": " + std::strerror(errno);
    return false;
  }
  return true;
}

}  /* namespace ajuste */
