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

namespace ajuste {

/* Los topes sanos: son los mismos que imponen las teclas y el sombreador. Pasar de ahi no arregla
   nada (la trama sale gris o el relieve se sale del presupuesto de la lente por ojo). */
const std::vector<Clave> &claves() {
  static const std::vector<Clave> k = {
      {"fase", "fase de la trama", -1.0, 1.0, 0.0005, false},
      {"paso", "paso de la lente", 0.5, 2.0, 0.0002, false},
      {"filtro", "filtro de la trama", 0.0, 30.0, 0.5, false},
      {"crosstalk", "crosstalk", -0.2, 0.2, 0.002, false},
      {"conv", "convergencia", -0.05, 0.05, 0.0005, false},
      {"rampa_x", "rampa de fase en x", -0.05, 0.05, 0.0001, false},
      {"rampa_y", "rampa de fase en y", -0.05, 0.05, 0.0001, false},
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
  return nombre == "overlay";
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

bool leer(std::map<std::string, double> &numeros, std::string &modelo) {
  numeros.clear();
  modelo.clear();
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

bool Lector::cambio(std::map<std::string, double> &numeros, std::string &modelo) {
  std::error_code ec;
  const std::string ruta = rutaAjuste();
  const auto tam = (long long)std::filesystem::file_size(ruta, ec);
  if (ec) { marca_ = -1; numeros.clear(); modelo.clear(); return false; }   /* no hay fichero */
  const auto mt = std::filesystem::last_write_time(ruta, ec);
  if (ec) { marca_ = -1; return false; }
  const long long mtime = (long long)std::chrono::duration_cast<std::chrono::nanoseconds>(mt.time_since_epoch()).count();
  const long long marca = mtime * 1024 + tam;
  if (marca == marca_) return false;
  marca_ = marca;
  return leer(numeros, modelo);
}

bool escribir(const std::map<std::string, double> &numeros, const std::string &modelo, std::string *err) {
  /* El fichero es una ORDEN, no la memoria del visor: se reescribe ENTERO con lo que se mando ahora,
     sin arrastrar lo de mandatos anteriores. Si se arrastrara, un numero que alguien toco despues con
     las teclas dentro del visor volveria atras al reenviar otro numero distinto. */
  std::string texto = "# Ajuste vivo del visor: lo escribe la ventana de ajustes o"
                      " `odyssey3d ctl ajuste`, y lo lee el visor cada decima de segundo.\n"
                      "# Son numeros sueltos, en las mismas palabras que serve.conf; se quita todo con"
                      " `odyssey3d ctl ajuste limpiar`.\n";
  for (const auto &kv : numeros) {
    if (kv.first == "modelo") continue;
    texto += kv.first + " = " + std::to_string(kv.second) + "\n";
  }
  /* `modelo` tambien puede venir dentro de `numeros` (la ventana lo manda por ahi a veces). */
  std::string pedido = modelo;
  for (const auto &kv : numeros)
    if (kv.first == "modelo") pedido = kv.second;
  if (!pedido.empty()) texto += "modelo = " + pedido + "\n";

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

}  /* namespace ajuste */
