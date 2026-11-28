/* efectos_fuentes.cpp — (v1.18) las fuentes publicas de efectos: comprobar y bajar solo la diferencia. Ver efectos.h.
   Solo en `odysseyvk` (el SHA-1 es de libcrypto): el panel arranca `odysseyvk efectos fuentes|bajar` aparte.

   Lo de GitHub: GET https://api.github.com/repos/DUENO/REPO/git/trees/HEAD?recursive=1 da cada archivo del repositorio con
   su `sha`, el SHA-1 de git de su contenido (sha1("blob <tamano>\0" + contenido)), y su `size`. El mismo SHA-1 de cada
   archivo de la carpeta de ReShade dice, sin bajar nada, que hay ya y que no. Lo que falta se baja de
   https://raw.githubusercontent.com/DUENO/REPO/HEAD/<ruta> y se comprueba con ese SHA-1 antes de ponerlo.
   Para las pruebas: ODYSSEYVK_EFECTOS_API=base (el arbol es base + DUENO_REPO + ".json") y ODYSSEYVK_EFECTOS_RAW=base
   (cada archivo, base + DUENO_REPO + "/" + ruta), por ejemplo file:// a una carpeta de pruebas. */
#include "efectos.h"

#include "util.h"

#include <openssl/evp.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace efectos {
namespace {

/* ---- lo de cada archivo ---- */
std::string hex(const unsigned char *d, unsigned n) {
  static const char *k = "0123456789abcdef";
  std::string s;
  for (unsigned i = 0; i < n; i++) s += k[d[i] >> 4], s += k[d[i] & 15];
  return s;
}
/* el SHA-1 de git de un contenido: sha1("blob <tamano>\0" + contenido) */
std::string blobDe(const std::string &contenido) {
  const std::string cab = "blob " + std::to_string(contenido.size());
  unsigned char d[EVP_MAX_MD_SIZE];
  unsigned n = 0;
  EVP_MD_CTX *c = EVP_MD_CTX_new();
  bool ok = c && EVP_DigestInit_ex(c, EVP_sha1(), nullptr) == 1 && EVP_DigestUpdate(c, cab.data(), cab.size() + 1) == 1 &&
            EVP_DigestUpdate(c, contenido.data(), contenido.size()) == 1 && EVP_DigestFinal_ex(c, d, &n) == 1;
  if (c) EVP_MD_CTX_free(c);
  return ok ? hex(d, n) : std::string();
}
bool leer(const std::string &ruta, std::string &out) {
  std::ifstream f(ruta, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}
std::string minusculas(std::string s) {
  for (char &c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}
std::string nombreBase(const std::string &r) {
  const size_t b = r.rfind('/');
  return b == std::string::npos ? r : r.substr(b + 1);
}

/* ---- programas aparte (curl) ---- */
/* corre `args` y devuelve su codigo de salida; su salida estandar, en *salida */
int correr(const std::vector<std::string> &args, std::string *salida) {
  int tubo[2];
  if (::pipe(tubo) != 0) return -1;
  const pid_t p = ::fork();
  if (p < 0) return -1;
  if (p == 0) {
    ::dup2(tubo[1], 1);
    const int nulo = ::open("/dev/null", O_RDWR);
    if (nulo >= 0) ::dup2(nulo, 0), ::dup2(nulo, 2);
    ::close(tubo[0]);
    std::vector<char *> av;
    for (const std::string &a : args) av.push_back((char *)a.c_str());
    av.push_back(nullptr);
    ::execvp(av[0], av.data());
    ::_exit(127);
  }
  ::close(tubo[1]);
  std::string s;
  char b[4096];
  ssize_t n;
  while ((n = ::read(tubo[0], b, sizeof b)) > 0) s.append(b, (size_t)n);
  ::close(tubo[0]);
  int st = 0;
  while (::waitpid(p, &st, 0) < 0 && errno == EINTR) {
  }
  if (salida) *salida = s;
  return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
/* baja una URL a un archivo: "" si bien, si no el motivo («no-encontrada», «limite» o el codigo) */
std::string bajarUrl(const std::string &url, const std::string &destino) {
  std::string cod;
  /* sin datos de nadie: el agente de curl de siempre, y lo de GitHub que pide su API */
  const int r = correr({"curl", "-sS", "-L", "--max-time", "60", "-H", "Accept: application/vnd.github+json", "-o", destino, "-w",
                        "%{http_code}", url},
                       &cod);
  cod = strTrim(cod);
  if (r == 0 && (cod == "200" || cod == "000")) return {};   /* 000: file:// (las pruebas) */
  if (cod == "404" || r == 37) return "no-encontrada";        /* 37: el archivo de file:// no esta */
  if (cod == "403" || cod == "429") return "limite";
  return "curl " + std::to_string(r) + (cod.empty() ? std::string() : " · HTTP " + cod);
}
std::string slug(const std::string &repo) {
  std::string s = repo;
  std::replace(s.begin(), s.end(), '/', '_');
  return s;
}
/* cada trozo de la ruta, con lo que no va tal cual en una URL (espacios...) en %XX */
std::string rutaUrl(const std::string &ruta) {
  std::string o;
  for (unsigned char c : ruta) {
    if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
    else {
      char b[4];
      std::snprintf(b, sizeof b, "%%%02X", c);
      o += b;
    }
  }
  return o;
}
std::string urlArbol(const Fuente &f) {
  if (const char *b = std::getenv("ODYSSEYVK_EFECTOS_API")) return std::string(b) + slug(f.repo) + ".json";
  return std::string("https://api.github.com/repos/") + f.repo + "/git/trees/HEAD?recursive=1";
}
std::string urlArchivo(const Fuente &f, const std::string &ruta) {
  if (const char *b = std::getenv("ODYSSEYVK_EFECTOS_RAW")) return std::string(b) + slug(f.repo) + "/" + rutaUrl(ruta);
  return std::string("https://raw.githubusercontent.com/") + f.repo + "/HEAD/" + rutaUrl(ruta);
}

/* ---- el arbol de GitHub: lo justo de JSON (cadenas con sus escapes y numeros) ---- */
struct Remoto {
  std::string ruta, sha;
  long long tam = 0;
};
bool cadenaJson(const std::string &s, size_t &i, std::string &out) {
  if (i >= s.size() || s[i] != '"') return false;
  out.clear();
  for (i++; i < s.size(); i++) {
    const char c = s[i];
    if (c == '"') {
      i++;
      return true;
    }
    if (c != '\\') {
      out += c;
      continue;
    }
    if (++i >= s.size()) return false;
    switch (s[i]) {
      case 'n': out += '\n'; break;
      case 't': out += '\t'; break;
      case 'r': out += '\r'; break;
      case 'b': out += '\b'; break;
      case 'f': out += '\f'; break;
      case 'u': {
        if (i + 4 >= s.size()) return false;
        unsigned cp = (unsigned)std::strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16);
        i += 4;
        /* sin pares sustitutos (rutas de shaders: no hacen falta) */
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) out += (char)(0xC0 | (cp >> 6)), out += (char)(0x80 | (cp & 0x3F));
        else out += (char)(0xE0 | (cp >> 12)), out += (char)(0x80 | ((cp >> 6) & 0x3F)), out += (char)(0x80 | (cp & 0x3F));
        break;
      }
      default: out += s[i];
    }
  }
  return false;
}
/* los objetos de "tree": sus "path", "type", "sha" y "size" (no hay objetos dentro de ellos) */
bool leerArbol(const std::string &json, std::vector<Remoto> &out, std::string *err) {
  size_t i = json.find("\"tree\"");
  if (i == std::string::npos) {
    if (err) *err = "sin arbol";
    return false;
  }
  i = json.find('[', i);
  if (i == std::string::npos) return false;
  i++;
  while (i < json.size()) {
    while (i < json.size() && json[i] != '{' && json[i] != ']') i++;
    if (i >= json.size() || json[i] == ']') break;
    i++;
    std::map<std::string, std::string> campos;
    while (i < json.size() && json[i] != '}') {
      while (i < json.size() && json[i] != '"' && json[i] != '}') i++;
      if (i >= json.size() || json[i] == '}') break;
      std::string clave, valor;
      if (!cadenaJson(json, i, clave)) return false;
      while (i < json.size() && (json[i] == ':' || std::isspace((unsigned char)json[i]))) i++;
      if (i < json.size() && json[i] == '"') {
        if (!cadenaJson(json, i, valor)) return false;
      } else {
        const size_t a = i;
        while (i < json.size() && json[i] != ',' && json[i] != '}') i++;
        valor = strTrim(json.substr(a, i - a));
      }
      campos[clave] = valor;
      if (i < json.size() && json[i] == ',') i++;
    }
    if (i < json.size()) i++;   /* la } */
    if (campos["type"] == "blob" && !campos["path"].empty() && campos["sha"].size() == 40)
      out.push_back({campos["path"], campos["sha"], std::atoll(campos["size"].c_str())});
  }
  return true;
}

/* ---- lo que ya hay en la carpeta de ReShade ---- */
struct Local {
  std::map<std::string, std::string> shaDe;                  /* «Shaders/SweetFX/CRT.fx» → su SHA-1 de git */
  std::set<std::string> shas;                                /* todos */
  std::map<std::string, std::vector<std::string>> porNombre;  /* «shaders/crt.fx» (lo de arriba y el nombre) → sus rutas */
  std::map<std::string, std::vector<std::string>> porSha;     /* un contenido → donde esta */
};
Local indexar(const std::string &carpeta) {
  Local l;
  /* (v1.18) lo que el usuario borro (efectos::aLaPapelera) cuenta como contenido que ya tiene: no se vuelve a bajar. Solo
     su contenido: no entra en los duplicados ni en los nombres, que son de lo que esta en uso */
  {
    std::error_code ec;
    const fs::path pap = fs::path(carpeta) / "Papelera";
    if (fs::is_directory(pap, ec))
      for (fs::recursive_directory_iterator it(pap, fs::directory_options::skip_permission_denied, ec), fin; it != fin; it.increment(ec)) {
        if (ec) break;
        std::string contenido;
        if (it->is_regular_file(ec) && leer(it->path().string(), contenido)) l.shas.insert(blobDe(contenido));
      }
  }
  for (const char *arriba : {"Shaders", "Textures"}) {
    std::error_code ec;
    const fs::path base = fs::path(carpeta) / arriba;
    if (!fs::is_directory(base, ec)) continue;
    for (fs::recursive_directory_iterator it(base, fs::directory_options::skip_permission_denied, ec), fin; it != fin; it.increment(ec)) {
      if (ec) break;
      if (!it->is_regular_file(ec)) continue;
      std::string contenido;
      if (!leer(it->path().string(), contenido)) continue;
      const std::string rel = std::string(arriba) + "/" + fs::relative(it->path(), base, ec).generic_string();
      const std::string sha = blobDe(contenido);
      l.shaDe[rel] = sha;
      l.shas.insert(sha);
      l.porNombre[minusculas(std::string(arriba) + "/" + it->path().filename().string())].push_back(rel);
      l.porSha[sha].push_back(rel);
    }
  }
  return l;
}

/* la ruta de un archivo del repositorio en la carpeta: lo de despues de su primer «Shaders» o «Textures» (sin mirar
   mayusculas), dentro de la de la carpeta; vacio si no va (README, licencias, lo de otros programas) */
std::string rutaLocal(const Fuente &f, const std::string &ruta) {
  std::vector<std::string> trozos;
  std::stringstream ss(ruta);
  std::string t;
  while (std::getline(ss, t, '/'))
    if (!t.empty()) trozos.push_back(t);
  for (size_t k = 0; k + 1 < trozos.size(); k++) {
    const std::string m = minusculas(trozos[k]);
    if (m != "shaders" && m != "textures") continue;
    std::string rel = m == "shaders" ? "Shaders" : "Textures";
    for (size_t j = k + 1; j < trozos.size(); j++) rel += "/" + trozos[j];
    /* las cabeceras de ReShade, solo de las de crosire (las demas llevan copias suyas, a veces viejas) */
    const std::string nb = nombreBase(rel);
    if ((nb == "ReShade.fxh" || nb == "ReShadeUI.fxh") && std::string(f.id) != "reshade-shaders") return {};
    return rel;
  }
  return {};
}

/* lo de arriba (Shaders o Textures) y el nombre, sin mayusculas: dos archivos con la misma clave serian un duplicado */
std::string claveNombre(const std::string &rel) {
  const size_t barra = rel.find('/');
  return minusculas(rel.substr(0, barra + 1) + nombreBase(rel));
}

struct Plan {
  struct Pieza {
    Remoto r;
    std::string rel;
  };
  std::vector<Pieza> bajar;
  int nuevos = 0, cambian = 0, conflictos = 0, total = 0;
  long long bytes = 0;
  std::string error;
};
Plan planear(const Fuente &f, const Local &l, const std::string &tmp) {
  Plan p;
  const std::string arbol = tmp + "/arbol-" + f.id + ".json";
  p.error = bajarUrl(urlArbol(f), arbol);
  if (!p.error.empty()) return p;
  std::string json;
  std::vector<Remoto> rs;
  std::string e;
  if (!leer(arbol, json) || !leerArbol(json, rs, &e)) {
    p.error = e.empty() ? "arbol ilegible" : e;
    return p;
  }
  std::set<std::string> vistas, shasPlan, nombresPlan;
  for (const Remoto &r : rs) {
    const std::string rel = rutaLocal(f, r.ruta);
    if (rel.empty() || !vistas.insert(rel).second) continue;
    p.total++;
    /* ese contenido ya esta (aqui o en otro sitio, o ya va en esta misma bajada): nada */
    if (l.shas.count(r.sha) || shasPlan.count(r.sha)) continue;
    /* (v1.18) Rodolfo: «dont replace old ones, delete duplicated downloaded files». Si ya hay uno con ese nombre (en el
       mismo sitio, distinto, o en otro), se queda el que hay y este no se baja; tampoco dos con el mismo nombre en una
       bajada */
    const std::string clave = claveNombre(rel);
    if (l.shaDe.count(rel) || l.porNombre.count(clave) || nombresPlan.count(clave)) {
      p.conflictos++;
      continue;
    }
    shasPlan.insert(r.sha), nombresPlan.insert(clave);
    p.bajar.push_back({r, rel}), p.nuevos++, p.bytes += r.tam;
  }
  return p;
}

std::string carpetaTmp() {
  const std::string d = rutaEstadoOdyssey("efectos-tmp");
  std::error_code ec;
  fs::create_directories(d, ec);
  return d;
}
void escribirEstado(const std::string &nombre, const std::string &txt) {
  const std::string r = rutaEstadoOdyssey(nombre.c_str());
  {
    std::ofstream f(r + ".nuevo", std::ios::trunc);
    f << txt;
  }
  std::rename((r + ".nuevo").c_str(), r.c_str());
}
/* la linea de una fuente en efectos-fuentes, cambiando solo la suya */
void anotarFuente(const std::string &id, const Plan &p) {
  std::map<std::string, std::string> lineas;
  {
    std::ifstream f(rutaEstadoOdyssey("efectos-fuentes"));
    std::string l;
    while (std::getline(f, l))
      if (!l.empty() && l[0] != '#') lineas[l.substr(0, l.find('\t'))] = l;
  }
  std::ostringstream o;
  o << id << '\t' << p.nuevos << '\t' << p.cambian << '\t' << p.conflictos << '\t' << p.total << '\t' << p.bytes << '\t'
    << (long)std::time(nullptr) << '\t' << p.error;
  lineas[id] = o.str();
  std::string txt = "# OdysseyVK: lo que falta de cada fuente publica de efectos (id, nuevos, cambian, conflictos, total, bytes, hora, error)\n";
  for (const auto &kv : lineas) txt += kv.second + "\n";
  escribirEstado("efectos-fuentes", txt);
}
const Fuente *buscarFuente(const std::string &id) {
  for (const Fuente &f : fuentesPublicas())
    if (id == f.id) return &f;
  return nullptr;
}
}  // namespace

int comprobarFuentes(const std::vector<std::string> &ids, bool decir) {
  const std::string carpeta = efectos::carpeta();
  if (carpeta.empty()) {
    std::fprintf(stderr, "no hay carpeta de ReShade\n");
    return 1;
  }
  escribirEstado("efectos-comprobando", "estado = comprobando\npid = " + std::to_string((long)::getpid()) + "\nhora = " +
                                            std::to_string(horaComprobacion()) + "\n");
  limpiarDuplicados(decir);   /* lo bajado que haya quedado duplicado, fuera antes de mirar */
  const Local l = indexar(carpeta);
  const std::string tmp = carpetaTmp();
  int fallos = 0;
  for (const Fuente &f : fuentesPublicas()) {
    if (!ids.empty() && std::find(ids.begin(), ids.end(), f.id) == ids.end()) continue;
    const Plan p = planear(f, l, tmp);
    anotarFuente(f.id, p);
    if (!p.error.empty()) fallos++;
    if (decir)
      std::printf("  %-16s %-34s %s\n", f.id, f.repo,
                  !p.error.empty() ? ("error: " + p.error).c_str()
                                   : (std::to_string(p.total) + " archivos · " + std::to_string(p.nuevos) + " nuevos · " +
                                      std::to_string(p.conflictos) + " con un nombre que ya tienes · " +
                                      std::to_string(p.bytes / 1024) + " KiB que bajar")
                                         .c_str());
    std::error_code ec;
    fs::remove(tmp + "/arbol-" + f.id + ".json", ec);
  }
  escribirEstado("efectos-comprobando", "estado = hecho\nhora = " + std::to_string((long)std::time(nullptr)) + "\n");
  if (decir) std::printf("carpeta: %s · %zu archivos tuyos mirados\n", carpeta.c_str(), l.shaDe.size());
  return fallos ? 1 : 0;
}

bool bajarFuente(const std::string &id, std::string *err) {
  const Fuente *f = buscarFuente(id);
  const std::string carpeta = efectos::carpeta();
  auto anotar = [&](const char *estado, long hecho, long total, const std::string &e) {
    escribirEstado("efectos-bajada", std::string("id = ") + id + "\nestado = " + estado + "\npid = " + std::to_string((long)::getpid()) +
                                         "\nhecho = " + std::to_string(hecho) + "\ntotal = " + std::to_string(total) +
                                         "\nerror = " + e + "\nhora = " + std::to_string((long)std::time(nullptr)) + "\n");
  };
  auto falla = [&](const std::string &e) {
    anotar("fallo", 0, 0, e);
    if (err) *err = e;
    return false;
  };
  if (!f) return falla("no hay fuente " + id);
  if (carpeta.empty()) return falla("no hay carpeta de ReShade");
  /* una a la vez */
  if (const Bajada b = leerBajada(); b.enMarcha) return falla("ya hay una bajada en marcha (" + b.id + ")");
  anotar("bajando", 0, 0, "");
  const std::string tmp = carpetaTmp();
  const Local l = indexar(carpeta);
  Plan p = planear(*f, l, tmp);
  if (!p.error.empty()) return falla(p.error);
  /* todo a una carpeta aparte y comprobado; si algo no cuadra, no se pone nada */
  const std::string lote = tmp + "/" + id;
  std::error_code ec;
  fs::remove_all(lote, ec);
  long hecho = 0;
  for (const Plan::Pieza &pz : p.bajar) {
    const std::string destino = lote + "/" + pz.rel;
    fs::create_directories(fs::path(destino).parent_path(), ec);
    const std::string e = bajarUrl(urlArchivo(*f, pz.r.ruta), destino);
    std::string contenido;
    if (!e.empty()) return falla(pz.r.ruta + ": " + e);
    if (!leer(destino, contenido) || blobDe(contenido) != pz.r.sha) return falla(pz.r.ruta + ": no es el que dice GitHub");
    anotar("bajando", ++hecho, (long)p.bajar.size(), "");
  }
  /* y a su sitio: solo lo que no esta; nunca se pisa uno que ya haya (si aparecio mientras, se deja el que hay). Cada uno
     se apunta en efectos-bajados: es lo unico que limpiarDuplicados puede borrar */
  int puestos = 0;
  std::ofstream bajados(rutaEstadoOdyssey("efectos-bajados"), std::ios::app);
  for (const Plan::Pieza &pz : p.bajar) {
    const fs::path destino = fs::path(carpeta) / pz.rel;
    if (fs::exists(destino, ec)) continue;
    fs::create_directories(destino.parent_path(), ec);
    const fs::path nuevo = destino.string() + ".odysseyvk-nuevo";
    fs::copy_file(fs::path(lote) / pz.rel, nuevo, fs::copy_options::overwrite_existing, ec);
    if (ec) return falla(pz.rel + ": " + ec.message());
    fs::rename(nuevo, destino, ec);
    if (ec) return falla(pz.rel + ": " + ec.message());
    bajados << id << '\t' << pz.rel << '\t' << pz.r.sha << '\n';
    puestos++;
  }
  bajados.close();
  fs::remove_all(lote, ec);
  std::printf("[efectos] %s: %d nuevos puestos en %s (%d con un nombre que ya tenias, sin bajar)\n", id.c_str(), puestos, carpeta.c_str(),
              p.conflictos);
  limpiarDuplicados(true);
  /* y lo que queda (nada, si todo fue bien) */
  Plan despues = planear(*f, indexar(carpeta), tmp);
  fs::remove(tmp + "/arbol-" + id + ".json", ec);
  anotarFuente(id, despues);
  anotar("lista", hecho, (long)p.bajar.size(), "");
  return true;
}

int limpiarDuplicados(bool decir) {
  /* (v1.18) Rodolfo: «delete duplicated downloaded files». De lo que bajo OdysseyVK (efectos-bajados), lo que tenga otro
     igual (mismo contenido) o con su nombre (en Shaders/ o en Textures/) se borra: lo mas nuevo primero, asi entre dos
     bajados se queda el de antes. Lo tuyo no se toca nunca */
  const std::string carpeta = efectos::carpeta();
  if (carpeta.empty()) return 0;
  std::vector<std::string> lineas;
  {
    std::ifstream f(rutaEstadoOdyssey("efectos-bajados"));
    std::string ln;
    while (std::getline(f, ln))
      if (!ln.empty()) lineas.push_back(ln);
  }
  if (lineas.empty()) return 0;
  Local l = indexar(carpeta);
  std::vector<bool> quedan(lineas.size(), true);
  int borrados = 0;
  for (size_t k = lineas.size(); k-- > 0;) {
    std::vector<std::string> c;
    std::stringstream ss(lineas[k]);
    std::string x;
    while (std::getline(ss, x, '\t')) c.push_back(x);
    if (c.size() < 2) {
      quedan[k] = false;
      continue;
    }
    const std::string &rel = c[1];
    const auto it = l.shaDe.find(rel);
    if (it == l.shaDe.end()) {   /* ya no esta: fuera de la lista */
      quedan[k] = false;
      continue;
    }
    const std::string sha = it->second, clave = claveNombre(rel);
    if (l.porSha[sha].size() < 2 && l.porNombre[clave].size() < 2) continue;
    std::error_code ec;
    const fs::path ruta = fs::path(carpeta) / rel;
    fs::remove(ruta, ec);
    if (ec) continue;
    /* y sus carpetas, si quedan vacias (sin pasar de Shaders/ o Textures/) */
    for (fs::path d = ruta.parent_path(); d.filename() != "Shaders" && d.filename() != "Textures" && fs::is_empty(d, ec); d = d.parent_path())
      fs::remove(d, ec);
    auto quitar = [&](std::vector<std::string> &v) { v.erase(std::remove(v.begin(), v.end(), rel), v.end()); };
    quitar(l.porSha[sha]), quitar(l.porNombre[clave]);
    l.shaDe.erase(rel);
    quedan[k] = false;
    borrados++;
    if (decir) std::printf("[efectos] borrado %s (bajado de %s): ya habia otro igual o con su nombre\n", rel.c_str(), c[0].c_str());
  }
  std::string txt;
  for (size_t k = 0; k < lineas.size(); k++)
    if (quedan[k]) txt += lineas[k] + "\n";
  escribirEstado("efectos-bajados", txt);
  return borrados;
}

}  // namespace efectos
