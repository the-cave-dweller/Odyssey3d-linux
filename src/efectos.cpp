/* efectos.cpp — (v1.18) los efectos de ReShade: ajustes, la carpeta de Rodolfo, describir y compilar. Ver efectos.h */
#include "efectos.h"

#include "ajuste.h"
#include "util.h"

#include "effect_codegen.hpp"
#include "effect_module.hpp"
#include "effect_parser.hpp"
#include "effect_preprocessor.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace efectos {

/* ---- la carpeta ---- */
std::string carpeta() {
  const auto sc = ajuste::leerServeConf();
  if (auto it = sc.find("efectos_carpeta"); it != sc.end() && !it->second.empty() && fs::is_directory(it->second))
    return it->second;
  std::string conf;
  if (const char *x = std::getenv("XDG_CONFIG_HOME"); x && *x) conf = x;
  else if (const char *h = std::getenv("HOME")) conf = std::string(h) + "/.config";
  for (const char *rel : {"/vkdaedalus/reshade", "/vkBasalt-overlay/reshade", "/vkBasalt/reshade"})
    if (fs::is_directory(conf + rel)) return conf + rel;
  return {};
}

static std::vector<std::string> subcarpetas(const std::string &base) {
  std::vector<std::string> v;
  std::error_code ec;
  if (!fs::is_directory(base, ec)) return v;
  v.push_back(base);
  for (fs::recursive_directory_iterator it(base, fs::directory_options::skip_permission_denied, ec), fin; it != fin;
       it.increment(ec)) {
    if (ec) break;
    if (it->is_directory(ec)) v.push_back(it->path().string());
  }
  std::sort(v.begin() + 1, v.end());
  return v;
}
std::vector<std::string> carpetasSombreadores() {
  const std::string c = carpeta();
  return c.empty() ? std::vector<std::string>{} : subcarpetas(c + "/Shaders");
}
std::vector<std::string> carpetasTexturas() {
  const std::string c = carpeta();
  return c.empty() ? std::vector<std::string>{} : subcarpetas(c + "/Textures");
}

std::map<std::string, std::string> disponibles() {
  std::map<std::string, std::string> m;
  for (const std::string &d : carpetasSombreadores()) {   /* Shaders/ primero: el de mas cerca gana */
    std::error_code ec;
    for (const auto &e : fs::directory_iterator(d, ec)) {
      if (!e.is_regular_file(ec) || e.path().extension() != ".fx") continue;
      m.emplace(e.path().stem().string(), e.path().string());
    }
  }
  return m;
}

std::string rutaTextura(const std::string &nombre) {
  if (!nombre.empty() && nombre[0] == '/') return fileExists(nombre) ? nombre : std::string();
  for (const std::string &d : carpetasTexturas())
    if (fileExists(d + "/" + nombre)) return d + "/" + nombre;
  /* algunas packs las dejan junto a los sombreadores */
  for (const std::string &d : carpetasSombreadores())
    if (fileExists(d + "/" + nombre)) return d + "/" + nombre;
  return {};
}

/* ---- los ajustes ---- */
const std::string *Efecto::valor(const std::string &clave) const {
  for (const auto &kv : valores)
    if (kv.first == clave) return &kv.second;
  return nullptr;
}
void Efecto::fijar(const std::string &clave, const std::string &v) {
  for (auto &kv : valores)
    if (kv.first == clave) {
      kv.second = v;
      return;
    }
  valores.push_back({clave, v});
}
Efecto *Ajustes::buscar(const std::string &nombre) {
  for (Efecto &e : lista)
    if (e.nombre == nombre) return &e;
  return nullptr;
}
bool Ajustes::algoQueAplicar() const {
  if (!puestos) return false;
  for (const Efecto &e : lista)
    if (e.activo) return true;
  return false;
}

Ajustes deFabrica() {
  /* los favoritos de Rodolfo, tal cual su vkdaedalus.conf (Rodolfo: «Take this values as reference for this effects as they
     are my favorites and I want the same default slider positions»). Sus `@RESHADE_DEPTH_INPUT_IS_MIRRORED` y `@PI` eran
     de vkDaedalus: la profundidad aqui la pone el visor, y PI ya lo define CRT.fx */
  Ajustes a;
  a.puestos = false;
  Efecto crt{"CRT", false, {{"Resolution", "8"}, {"Gamma", "2.493"}, {"MonitorGamma", "2.465"}, {"Brightness", "2.192"},
                            {"ScanlineGaussian", "false"}, {"CurvatureRadius", "0"}, {"CornerSize", "0.001"},
                            {"ViewerDistance", "4"}, {"Angle[0]", "0.005"}, {"Angle[1]", "0.029"}, {"Overscan", "1"}}, {}};
  Efecto dpx{"DPX", false, {{"Saturation", "2.252"}, {"Colorfulness", "0.274"}, {"Strength", "0.079"}}, {}};
  Efecto hdr{"FakeHDR", true, {{"HDRPower", "0.958"}, {"radius1", "0.883"}}, {}};
  Efecto lut{"LUT-RA", true, {}, {}};
  Efecto sharp{"RGBSharpening", true, {}, {}};
  a.lista = {crt, dpx, hdr, lut, sharp};
  return a;
}

std::string rutaAjustes(const std::string &appid) {
  /* Rodolfo: «name it effects, for the config folder» */
  return rutaConfigOdyssey("effects") + "/" + (appid.empty() ? std::string("all") : appid) + ".conf";
}

static std::vector<std::string> partir(const std::string &s, char sep) {
  std::vector<std::string> v;
  std::string t;
  std::stringstream ss(s);
  while (std::getline(ss, t, sep))
    if (!strTrim(t).empty()) v.push_back(strTrim(t));
  return v;
}

static bool leerArchivo(const std::string &ruta, Ajustes &a) {
  std::ifstream f(ruta);
  if (!f) return false;
  a = Ajustes{};
  std::vector<std::string> orden, apagados;
  std::map<std::string, Efecto> porNombre;
  std::string l;
  while (std::getline(f, l)) {
    l = strTrim(l);
    if (l.empty() || l[0] == '#') continue;
    const size_t ig = l.find('=');
    if (ig == std::string::npos) continue;
    const std::string k = strTrim(l.substr(0, ig)), v = strTrim(l.substr(ig + 1));
    if (k == "puestos") a.puestos = v == "si" || v == "1" || v == "true";
    else if (k == "effects") orden = partir(v, ':');
    else if (k == "disabledEffects") apagados = partir(v, ':');
    else if (const size_t at = k.find('@'); at != std::string::npos && at > 0) porNombre[k.substr(0, at)].defs.push_back({k.substr(at + 1), v});
    else if (const size_t pt = k.find('.'); pt != std::string::npos && pt > 0) porNombre[k.substr(0, pt)].valores.push_back({k.substr(pt + 1), v});
    /* `CRT = /ruta/CRT.fx` (lo de vkBasalt): la ruta no hace falta, se busca por nombre en la carpeta */
  }
  for (const std::string &n : orden) {
    Efecto e = porNombre.count(n) ? porNombre[n] : Efecto{};
    e.nombre = n;
    e.activo = std::find(apagados.begin(), apagados.end(), n) == apagados.end();
    a.lista.push_back(std::move(e));
  }
  return true;
}

Ajustes cargar(const std::string &appid) {
  Ajustes a;
  if (!appid.empty() && leerArchivo(rutaAjustes(appid), a)) {
    a.propio = true;
    return a;
  }
  if (leerArchivo(rutaAjustes(""), a)) return a;
  return deFabrica();
}

bool guardar(const std::string &appid, const Ajustes &a, std::string *err) {
  const std::string ruta = rutaAjustes(appid);
  std::error_code ec;
  fs::create_directories(fs::path(ruta).parent_path(), ec);
  std::ostringstream o;
  o << "# OdysseyVK: los efectos de ReShade " << (appid.empty() ? std::string("de todos los juegos") : "del juego " + appid)
    << " (los escribe el panel; como los de vkBasalt)\n";
  o << "puestos = " << (a.puestos ? "si" : "no") << "\n";
  std::string orden, apagados;
  for (const Efecto &e : a.lista) {
    orden += (orden.empty() ? "" : ":") + e.nombre;
    if (!e.activo) apagados += (apagados.empty() ? "" : ":") + e.nombre;
  }
  o << "effects = " << orden << "\n";
  o << "disabledEffects = " << apagados << "\n";
  for (const Efecto &e : a.lista) {
    o << "\n# " << e.nombre << "\n";
    for (const auto &kv : e.valores) o << e.nombre << "." << kv.first << " = " << kv.second << "\n";
    for (const auto &kv : e.defs) o << e.nombre << "@" << kv.first << " = " << kv.second << "\n";
  }
  const std::string tmp = ruta + ".nuevo";
  {
    std::ofstream f(tmp, std::ios::trunc);
    if (!f || !(f << o.str())) {
      if (err) *err = "no se pudo escribir " + tmp;
      return false;
    }
  }
  if (std::rename(tmp.c_str(), ruta.c_str()) != 0) {
    if (err) *err = "no se pudo poner " + ruta;
    return false;
  }
  return true;
}

bool quitarPropios(const std::string &appid) {
  if (appid.empty()) return false;
  return ::unlink(rutaAjustes(appid).c_str()) == 0;
}

long long firma(const std::string &appid) {
  long long f = 0;
  for (const std::string &r : {rutaAjustes(appid), rutaAjustes("")}) {
    struct stat st{};
    if (::stat(r.c_str(), &st) == 0) f = f * 1000003LL + (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec + 1;
  }
  return f;
}

/* ---- compilar ---- */
/* __RESHADE__ como el de vkDaedalus (6.8.0): el nivel de ReShade cuyo contrato cumple el compilador */
static void macrosBase(reshadefx::preprocessor &pp, int ancho, int alto) {
  pp.add_macro_definition("__RESHADE__", "60800");
  pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", "0");
  pp.add_macro_definition("__RESHADE_PERMUTATION__", "0");
  pp.add_macro_definition("__RENDERER__", "0x20000");   /* Vulkan */
  pp.add_macro_definition("__VENDOR__", "0x10de");
  pp.add_macro_definition("__DEVICE__", "0");
  pp.add_macro_definition("__APPLICATION__", "0");
  pp.add_macro_definition("BUFFER_WIDTH", std::to_string(ancho));
  pp.add_macro_definition("BUFFER_HEIGHT", std::to_string(alto));
  pp.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
  pp.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");
  pp.add_macro_definition("BUFFER_COLOR_FORMAT", "28");   /* rgba8 */
  pp.add_macro_definition("BUFFER_COLOR_DEPTH", "8");
  pp.add_macro_definition("BUFFER_COLOR_BIT_DEPTH", "8");
  pp.add_macro_definition("BUFFER_COLOR_SPACE", "1");     /* srgb */
  /* La profundidad: la del visor, 255 = cerca. Con IS_REVERSED, 1 - d, y con el plano lejano en 1 la linealizacion de
     ReShade.fxh (d / (F - d (F - 1))) la deja tal cual: 0 cerca, 1 lejos, como espera cada efecto */
  pp.add_macro_definition("RESHADE_DEPTH_INPUT_IS_REVERSED", "1");
  pp.add_macro_definition("RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN", "0");
  pp.add_macro_definition("RESHADE_DEPTH_INPUT_IS_MIRRORED", "0");
  pp.add_macro_definition("RESHADE_DEPTH_INPUT_IS_LOGARITHMIC", "0");
  pp.add_macro_definition("RESHADE_DEPTH_LINEARIZATION_FAR_PLANE", "1.0");
  pp.add_macro_definition("RESHADE_DEPTH_MULTIPLIER", "1");
  pp.append_string("#define tex2Doffset(s, coords, offset) tex2D(s, coords, offset)\n"
                   "#define tex2Dlodoffset(s, coords, offset) tex2Dlod(s, coords, offset)\n"
                   "#define tex2DgatherRoffset(s, coords, offset) tex2DgatherR(s, coords, offset)\n"
                   "#define tex2DgatherGoffset(s, coords, offset) tex2DgatherG(s, coords, offset)\n"
                   "#define tex2DgatherBoffset(s, coords, offset) tex2DgatherB(s, coords, offset)\n"
                   "#define tex2DgatherAoffset(s, coords, offset) tex2DgatherA(s, coords, offset)\n"
                   "#define tex2Dgather(s, t, c) tex2Dgather##c(s, t)\n"
                   "#define tex2Dgatheroffset(s, t, o, c) tex2Dgather##c(s, t, o)\n"
                   "#define tex2Dgather0 tex2DgatherR\n"
                   "#define tex2Dgather1 tex2DgatherG\n"
                   "#define tex2Dgather2 tex2DgatherB\n"
                   "#define tex2Dgather3 tex2DgatherA\n"
                   "#define ddx_fine(x) ddx(x)\n"
                   "#define ddy_fine(x) ddy(x)\n"
                   "#define ddx_coarse(x) ddx(x)\n"
                   "#define ddy_coarse(x) ddy(x)\n");
}

/* preprocesar y analizar: el modulo (con o sin SPIR-V) */
static std::shared_ptr<Compilado> compilarCon(const std::string &ruta, int ancho, int alto,
                                              const std::vector<std::pair<std::string, std::string>> &defs, bool conSpirv) {
  auto c = std::make_shared<Compilado>();
  try {
    reshadefx::preprocessor pp;
    macrosBase(pp, ancho, alto);
    for (const auto &[n, v] : defs) pp.add_macro_definition(n, v);
    for (const std::string &d : carpetasSombreadores()) pp.add_include_path(d);
    pp.add_include_path(fs::path(ruta).parent_path());
    if (!pp.append_file(ruta)) {
      c->error = "no se pudo leer: " + pp.errors();
      return c;
    }
    if (const std::string e = pp.errors(); e.find("error") != std::string::npos && e.find("preprocessor error:") != std::string::npos) {
      c->error = e;
      return c;
    }
    reshadefx::parser parser;
    /* SPIR-V de Vulkan, con informacion de depuracion, uniformes en su bufer (para cambiarlos en marcha), sin 16 bits, la Y
       del vertice dada la vuelta (la convencion de Vulkan) */
    std::unique_ptr<reshadefx::codegen> cg(reshadefx::create_codegen_spirv(true, true, false, false, true, false));
    if (!parser.parse(pp.output(), cg.get())) {
      c->error = parser.errors();
      return c;
    }
    c->avisos = parser.errors();
    c->modulo = std::make_shared<reshadefx::effect_module>(cg->module());
    if (conSpirv)
      for (const auto &ep : c->modulo->entry_points) {
        std::string bin, asmb, errs;
        if (!cg->assemble_code_for_entry_point(ep.first, bin, asmb, errs)) {
          c->error = "no se pudo armar " + ep.first + ": " + errs;
          return c;
        }
        std::vector<uint32_t> &w = c->spirv[ep.first];
        w.resize(bin.size() / 4);
        std::memcpy(w.data(), bin.data(), w.size() * 4);
      }
    c->ok = true;
  } catch (const std::exception &e) {
    c->error = e.what();
  }
  return c;
}

std::shared_ptr<Compilado> compilar(const std::string &ruta, int ancho, int alto,
                                    const std::vector<std::pair<std::string, std::string>> &defs) {
  return compilarCon(ruta, ancho, alto, defs, true);
}

/* ---- describir ---- */
static const reshadefx::annotation *anot(const std::vector<reshadefx::annotation> &v, const char *n) {
  for (const auto &a : v)
    if (a.name == n) return &a;
  return nullptr;
}
static float anotReal(const reshadefx::annotation &a, int i = 0) {
  if (a.type.is_floating_point()) return a.value.as_float[i];
  if (a.type.is_signed()) return (float)a.value.as_int[i];
  return (float)a.value.as_uint[i];
}

std::string claveDe(const Control &c, int componente) {
  return c.componentes > 1 ? c.nombre + "[" + std::to_string(componente) + "]" : c.nombre;
}

float valorDe(const Efecto &e, const Control &c, int componente) {
  if (const std::string *v = e.valor(claveDe(c, componente))) {
    if (*v == "true") return 1.0f;
    if (*v == "false") return 0.0f;
    return std::strtof(v->c_str(), nullptr);
  }
  return c.defecto[std::clamp(componente, 0, 3)];
}

/* la descripcion en texto, para guardarla (el panel no compila cada vez que se abre) */
static std::string aTexto(const Descripcion &d) {
  std::ostringstream o;
  o << "ok " << d.ok << " prof " << d.usaProfundidad << " comp " << d.computo << "\n";
  o << "error " << d.error << "\n";
  for (const Control &c : d.controles) {
    o << "control\n" << (int)c.tipo << " " << c.componentes << " " << c.minimo << " " << c.maximo << " " << c.paso << " "
      << c.color << " " << c.defecto[0] << " " << c.defecto[1] << " " << c.defecto[2] << " " << c.defecto[3] << "\n";
    o << c.nombre << "\n" << c.rotulo << "\n" << c.categoria << "\n";
    std::string ay = c.ayuda;
    std::replace(ay.begin(), ay.end(), '\n', ' ');
    o << ay << "\n" << c.opciones.size() << "\n";
    for (const std::string &op : c.opciones) o << op << "\n";
  }
  return o.str();
}
static bool deTexto(const std::string &s, Descripcion &d) {
  std::istringstream i(s);
  std::string w;
  if (!(i >> w >> d.ok >> w >> d.usaProfundidad >> w >> d.computo)) return false;
  std::getline(i, w);
  std::getline(i, w);
  d.error = w.size() > 6 ? w.substr(6) : std::string();
  while (std::getline(i, w)) {
    if (w != "control") continue;
    Control c;
    int tipo = 0;
    i >> tipo >> c.componentes >> c.minimo >> c.maximo >> c.paso >> c.color >> c.defecto[0] >> c.defecto[1] >> c.defecto[2] >>
        c.defecto[3];
    c.tipo = (Control::Tipo)tipo;
    std::getline(i, w);
    std::getline(i, c.nombre);
    std::getline(i, c.rotulo);
    std::getline(i, c.categoria);
    std::getline(i, c.ayuda);
    size_t n = 0;
    i >> n;
    std::getline(i, w);
    for (size_t k = 0; k < n && k < 256; k++) {
      std::getline(i, w);
      c.opciones.push_back(w);
    }
    d.controles.push_back(std::move(c));
  }
  return true;
}

bool usaProfundidad(const std::string &ruta) {
  static std::mutex m;
  static std::map<std::string, std::pair<long long, bool>> memoria;
  struct stat st{};
  if (::stat(ruta.c_str(), &st) != 0) return false;
  const long long marca = (long long)st.st_mtim.tv_sec * 1000003LL + st.st_size;
  {
    std::lock_guard<std::mutex> l(m);
    if (auto it = memoria.find(ruta); it != memoria.end() && it->second.first == marca) return it->second.second;
  }
  std::string t;
  {
    std::ifstream f(ruta, std::ios::binary);
    t.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }
  bool usa = false;
  for (const char *k : {"GetLinearizedDepth", "DepthBuffer", "linear_depth", "get_depth", "LinearizeDepth", "GetDepth(", "BUFFER_DEPTH"})
    usa = usa || t.find(k) != std::string::npos;
  /* la semantica `: DEPTH` (con o sin espacios) */
  for (size_t p = t.find("DEPTH"); !usa && p != std::string::npos; p = t.find("DEPTH", p + 5)) {
    size_t q = p;
    while (q > 0 && (t[q - 1] == ' ' || t[q - 1] == '\t')) q--;
    usa = q > 0 && t[q - 1] == ':';
  }
  std::lock_guard<std::mutex> l(m);
  memoria[ruta] = {marca, usa};
  return usa;
}

Descripcion describir(const std::string &ruta) {
  static std::mutex m;
  static std::map<std::string, std::pair<long long, Descripcion>> memoria;
  struct stat st{};
  if (::stat(ruta.c_str(), &st) != 0) {
    Descripcion d;
    d.error = "no existe " + ruta;
    return d;
  }
  /* la firma: el archivo y la version de este formato */
  const long long marca = (long long)st.st_mtim.tv_sec * 1000003LL + st.st_size * 7 + 4;
  {
    std::lock_guard<std::mutex> l(m);
    if (auto it = memoria.find(ruta); it != memoria.end() && it->second.first == marca) return it->second.second;
  }
  std::string cache;
  {
    std::string base;
    if (const char *x = std::getenv("XDG_CACHE_HOME"); x && *x) base = x;
    else if (const char *h = std::getenv("HOME")) base = std::string(h) + "/.cache";
    std::string nombre = ruta;
    std::replace(nombre.begin(), nombre.end(), '/', '_');
    cache = base + "/odysseyvk/effects/" + nombre + ".txt";
  }
  Descripcion d;
  std::string txt;
  bool hecho = false;
  if (readSmallFile(cache, txt)) {
    const size_t nl = txt.find('\n');
    if (nl != std::string::npos && txt.substr(0, nl) == std::to_string(marca)) hecho = deTexto(txt.substr(nl + 1), d);
  }
  if (!hecho) {
    d = Descripcion{};
    auto c = compilarCon(ruta, 1920, 1080, {}, false);
    if (!c->ok) {
      d.error = c->error;
    } else {
      const reshadefx::effect_module &mod = *c->modulo;
      d.ok = true;
      d.usaProfundidad = usaProfundidad(ruta);   /* lo que dice su texto (ver efectos.h) */
      for (const auto &tq : mod.techniques)
        for (const auto &p : tq.passes)
          if (!p.cs_entry_point.empty()) d.computo = true;
      for (const auto &u : mod.uniforms) {
        if (anot(u.annotations, "source")) continue;   /* los que llena el programa (tiempo, cuadros...) */
        if (!u.type.is_numeric() || u.type.is_matrix() || u.type.is_array()) continue;
        Control ct;
        ct.nombre = u.name;
        ct.componentes = (int)u.type.rows;
        if (u.type.is_boolean()) ct.tipo = Control::Bool;
        else if (u.type.is_integral()) ct.tipo = Control::Entero;
        if (const auto *a = anot(u.annotations, "ui_label")) ct.rotulo = a->value.string_data;
        if (ct.rotulo.empty()) ct.rotulo = u.name;
        if (const auto *a = anot(u.annotations, "ui_tooltip")) ct.ayuda = a->value.string_data;
        if (const auto *a = anot(u.annotations, "ui_category")) ct.categoria = a->value.string_data;
        std::string tipoUi;
        if (const auto *a = anot(u.annotations, "ui_type")) tipoUi = a->value.string_data;
        if (const auto *a = anot(u.annotations, "ui_min")) ct.minimo = anotReal(*a);
        if (const auto *a = anot(u.annotations, "ui_max")) ct.maximo = anotReal(*a);
        else if (ct.tipo == Control::Entero) ct.maximo = 16;
        if (const auto *a = anot(u.annotations, "ui_step")) ct.paso = anotReal(*a);
        if (tipoUi == "color") ct.color = true, ct.minimo = 0.0f, ct.maximo = std::max(ct.maximo, 1.0f);
        if ((tipoUi == "combo" || tipoUi == "radio" || tipoUi == "list") && ct.tipo != Control::Bool) {
          if (const auto *a = anot(u.annotations, "ui_items")) {
            const std::string &s = a->value.string_data;
            size_t i0 = 0;
            for (size_t i = 0; i <= s.size(); i++)
              if (i == s.size() || s[i] == '\0') {
                if (i > i0) ct.opciones.push_back(s.substr(i0, i - i0));
                i0 = i + 1;
              }
          }
          if (!ct.opciones.empty()) ct.tipo = Control::Lista;
        }
        if (u.has_initializer_value)
          for (int k = 0; k < ct.componentes && k < 4; k++)
            ct.defecto[k] = u.type.is_floating_point() ? u.initializer_value.as_float[k]
                            : u.type.is_signed()        ? (float)u.initializer_value.as_int[k]
                                                        : (float)u.initializer_value.as_uint[k];
        if (ct.maximo <= ct.minimo) ct.maximo = ct.minimo + 1.0f;
        d.controles.push_back(std::move(ct));
      }
    }
    std::error_code ec;
    fs::create_directories(fs::path(cache).parent_path(), ec);
    if (std::ofstream f(cache, std::ios::trunc); f) f << marca << "\n" << aTexto(d);
  }
  std::lock_guard<std::mutex> l(m);
  memoria[ruta] = {marca, d};
  return d;
}

/* ---- (v1.18) las fuentes publicas (ver efectos.h) ---- */
const std::vector<Fuente> &fuentesPublicas() {
  /* las de la lista del instalador de ReShade y las mas usadas; si GitHub no la encuentra (cambio de nombre), lo dice */
  static const std::vector<Fuente> f = {
      {"reshade-shaders", "ReShade (los de siempre)", "crosire/reshade-shaders", "Los efectos de la instalación de ReShade"},
      {"sweetfx", "SweetFX", "CeeJayDK/SweetFX", "LumaSharpen, Vibrance, Curves, DPX, Tonemap..."},
      {"quint", "qUINT", "martymcmodding/qUINT", "MXAO, Lightroom, SSR, Bloom..."},
      {"immerse", "iMMERSE", "martymcmodding/iMMERSE", "Lo gratuito de iMMERSE: MXAO, Sharpen, SMAA..."},
      {"prod80", "prod80", "prod80/prod80-ReShade-Repository", "Color, LUT, curvas, grano"},
      {"astrayfx", "AstrayFX", "BlueSkyDefender/AstrayFX", "Clarity, Smart Sharp, nitidez y AA"},
      {"depth3d", "Depth3D", "BlueSkyDefender/Depth3D", "3D estereoscópico y su vista de profundidad"},
      {"fxshaders", "FXShaders", "luluco250/FXShaders", "Bloom, desenfoque, grano, NeoBloom..."},
      {"fubax", "fubax", "Fubaxiusz/fubax-shaders", "Perfect Perspective, Chromakey, Filmic"},
      {"otisfx", "OtisFX", "FransBouma/OtisFX", "CinematicDOF, AdaptiveFog, Emphasize"},
      {"cshade", "CShade", "papadanku/CShade", "Bloom, AA, flujo óptico y más"},
      {"insane", "Insane-Shaders", "LordOfLunacy/Insane-Shaders", "Oilify, ReVeil, Halftone..."},
      {"dh", "DH", "AlucardDH/dh-reshade-shaders", "RTGI, UBR, color..."},
  };
  return f;
}

namespace {
std::map<std::string, std::string> pares(const std::string &ruta) {
  std::map<std::string, std::string> v;
  std::string txt;
  if (!readSmallFile(ruta, txt)) return v;
  std::istringstream in(txt);
  std::string l;
  while (std::getline(in, l)) {
    const size_t ig = l.find('=');
    if (ig != std::string::npos) v[strTrim(l.substr(0, ig))] = strTrim(l.substr(ig + 1));
  }
  return v;
}
bool vivo(long pid, const char *palabra) {
  std::string cmd;
  return pid > 0 && ::kill((pid_t)pid, 0) == 0 && readSmallFile("/proc/" + std::to_string(pid) + "/cmdline", cmd) &&
         cmd.find(palabra) != std::string::npos;
}
}  // namespace

std::map<std::string, EstadoFuente> leerFuentes() {
  /* una linea por fuente: id, nuevos, cambian, conflictos, total, bytes, hora, error (separados por tabuladores) */
  std::map<std::string, EstadoFuente> m;
  std::ifstream f(rutaEstadoOdyssey("efectos-fuentes"));
  std::string l;
  while (std::getline(f, l)) {
    if (l.empty() || l[0] == '#') continue;
    std::vector<std::string> c;
    std::stringstream ss(l);
    std::string x;
    while (std::getline(ss, x, '\t')) c.push_back(x);
    if (c.size() < 7) continue;
    EstadoFuente e;
    e.comprobada = true;
    e.nuevos = std::atoi(c[1].c_str()), e.cambian = std::atoi(c[2].c_str()), e.conflictos = std::atoi(c[3].c_str());
    e.total = std::atoi(c[4].c_str()), e.bytes = std::atoll(c[5].c_str()), e.hora = std::atol(c[6].c_str());
    if (c.size() > 7) e.error = c[7];
    m[c[0]] = e;
  }
  return m;
}

long horaComprobacion() { return std::atol(pares(rutaEstadoOdyssey("efectos-comprobando"))["hora"].c_str()); }

bool comprobando() {
  const auto v = pares(rutaEstadoOdyssey("efectos-comprobando"));
  return v.count("estado") && v.at("estado") == "comprobando" && vivo(std::atol(v.count("pid") ? v.at("pid").c_str() : "0"), "efectos");
}

Bajada leerBajada() {
  Bajada b;
  auto v = pares(rutaEstadoOdyssey("efectos-bajada"));
  b.id = v["id"], b.hecho = std::atol(v["hecho"].c_str()), b.total = std::atol(v["total"].c_str()), b.hora = std::atol(v["hora"].c_str());
  if (v["estado"] == "bajando") {
    b.enMarcha = vivo(std::atol(v["pid"].c_str()), "efectos");
    if (!b.enMarcha) b.error = "cortada";
  } else if (v["estado"] == "lista") {
    b.lista = true;
  } else if (v["estado"] == "fallo") {
    b.error = v["error"].empty() ? std::string("fallo") : v["error"];
  }
  return b;
}

bool lanzarOrden(const std::vector<std::string> &args, std::string *err) {
  /* el programa: este mismo si es `odysseyvk`; si no (el panel), el de su carpeta o el del PATH */
  std::string bin;
  {
    std::error_code ec;
    const fs::path yo = fs::read_symlink("/proc/self/exe", ec);
    if (!ec && yo.filename() == "odysseyvk") bin = yo.string();
    else if (!ec && fileExists((yo.parent_path() / "odysseyvk").string())) bin = (yo.parent_path() / "odysseyvk").string();
    else bin = "odysseyvk";
  }
  const std::string registro = rutaEstadoOdyssey("efectos-fuentes.log");
  const pid_t p = ::fork();
  if (p < 0) {
    if (err) *err = std::string("no se pudo arrancar: ") + std::strerror(errno);
    return false;
  }
  if (p == 0) {
    ::setsid();
    if (::fork() != 0) ::_exit(0);
    const int nulo = ::open("/dev/null", O_RDONLY);
    const int log = ::open(registro.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (nulo >= 0) ::dup2(nulo, 0);
    if (log >= 0) ::dup2(log, 1), ::dup2(log, 2);
    for (int fd = 3; fd < 1024; fd++) ::close(fd);
    std::vector<char *> av;
    av.push_back((char *)bin.c_str());
    av.push_back((char *)"efectos");
    for (const std::string &a : args) av.push_back((char *)a.c_str());
    av.push_back(nullptr);
    ::execvp(bin.c_str(), av.data());
    ::_exit(127);
  }
  int st = 0;
  while (::waitpid(p, &st, 0) < 0 && errno == EINTR) {
  }
  return true;
}

}  // namespace efectos
