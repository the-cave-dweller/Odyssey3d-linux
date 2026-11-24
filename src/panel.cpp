#include "panel.h"
#include "util.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

static std::string stripAnsi(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\x1b') {
      while (i < s.size() && s[i] != 'm' && !(s[i] >= 'A' && s[i] <= 'Z')) i++;
      continue;
    }
    o.push_back(s[i]);
  }
  return o;
}

static std::string ksd(const std::string &args) {
  std::string out;
  runCapture("kscreen-doctor " + args + " 2>&1", out);
  return stripAnsi(out);
}

bool Panel::available() {
  std::string out;
  int rc = runCapture("command -v kscreen-doctor >/dev/null 2>&1 && echo yes || echo no", out);
  return rc == 0 && out.find("yes") != std::string::npos;
}

const char *Panel::rotationWord(int r) {
  /* tabla verificada contra kscreen-doctor (ver panel.h): 1 none, 2 left, 4 inverted, 8 right */
  switch (r) {
    case RotLeft: return "left";
    case RotInverted: return "inverted";
    case RotRight: return "right";
    case RotNone: return "none";
    default: return nullptr; /* valor desconocido: mejor no tocar la rotacion */
  }
}

static double hzOf(const std::string &res) {
  size_t at = res.find('@');
  if (at == std::string::npos) return 0;
  return atof(res.c_str() + at + 1);
}

static std::vector<PanelState> parseAll(const std::string &text) {
  std::vector<PanelState> all;
  std::istringstream is(text);
  std::string raw;
  PanelState *cur = nullptr;
  while (std::getline(is, raw)) {
    std::string line = raw;
    size_t a = line.find_first_not_of(" \t");
    if (a == std::string::npos) continue;
    line = line.substr(a);
    if (line.rfind("Output:", 0) == 0) {
      all.emplace_back();
      cur = &all.back();
      /* "Output: 1 DP-4 <uuid>" */
      std::istringstream ss(line.substr(7));
      std::string tok;
      ss >> tok; /* número */
      ss >> cur->name;
      continue;
    }
    if (!cur) continue;
    if (line.rfind("Modes:", 0) == 0) {
      std::istringstream ss(line.substr(6));
      std::string tok;
      while (ss >> tok) {
        size_t colon = tok.find(':');
        if (colon == std::string::npos) continue;
        std::string res = tok.substr(colon + 1);
        bool current = false;
        for (char c : res)
          if (c == '*') current = true;
        std::string clean;
        for (char c : res)
          if (c != '*' && c != '!') clean.push_back(c);
        cur->modes.emplace_back(atoi(tok.substr(0, colon).c_str()), clean);
        if (current) {
          cur->modeIndex = atoi(tok.substr(0, colon).c_str());
          cur->modeRes = clean;
        }
      }
    } else if (line.rfind("Scale:", 0) == 0) {
      cur->scale = atof(line.c_str() + 6);
    } else if (line.rfind("Rotation:", 0) == 0) {
      cur->rotation = atoi(line.c_str() + 9);
    } else if (line.rfind("Vrr:", 0) == 0) {
      cur->vrr = strTrim(line.substr(4));
    } else if (line.rfind("HDR:", 0) == 0) {
      cur->hdr = line.find("enabled") != std::string::npos;
    }
  }
  return all;
}

std::vector<std::string> Panel::outputs(std::string *err) {
  std::string text = ksd("-o");
  if (text.empty() && err)
    *err = "kscreen-doctor no devolvió nada (¿sesión KDE/Wayland? revisa WAYLAND_DISPLAY y "
           "XDG_RUNTIME_DIR)";
  std::vector<std::string> names;
  for (const auto &o : parseAll(text))
    if (!o.name.empty()) names.push_back(o.name);
  return names;
}

bool Panel::query(const std::string &out, PanelState &st, std::string *err) {
  std::string text = ksd("-o");
  for (const auto &o : parseAll(text)) {
    if (o.name == out) {
      st = o;
      return true;
    }
  }
  if (err)
    *err = "output '" + out + "' no encontrado. Salidas: " + [&] {
             std::string s;
             for (const auto &o2 : parseAll(text)) {
               if (!s.empty()) s += ", ";
               s += o2.name;
             }
             return s;
           }();
  return false;
}

int Panel::pickMode(const PanelState &st, double preferHz) {
  int best = -1;            /* -1 = este monitor no tiene el modo nativo del Odyssey */
  double bestDiff = 1e9;
  for (const auto &m : st.modes) {
    double hz = hzOf(m.second);
    if (hz <= 0) continue;
    if (m.second.rfind("3840x2160@", 0) != 0) continue; /* solo el modo nativo del panel */
    double diff = preferHz > 0 ? std::abs(hz - preferHz) : std::abs(hz - 120.0);
    if (hz > preferHz + 0.5 && preferHz > 0) diff += 100.0; /* penaliza pasarse de la tasa pedida */
    if (diff < bestDiff) {
      bestDiff = diff;
      best = m.first;
    }
  }
  return best;
}

bool Panel::apply(const std::string &out, int modeIndex, double scale, int rotation, bool disableHdr,
                  std::string *err, bool dryRun) {
  std::ostringstream args;
  args << "output." << out << ".mode." << modeIndex << " output." << out << ".scale." << scale;
  if (const char *rot = rotationWord(rotation))
    args << " output." << out << ".rotation." << rot;
  if (dryRun) {
    if (err) *err = "kscreen-doctor " + args.str();
    return true;
  }
  std::string r = ksd(args.str());
  /* VRR y HDR se intentan aparte: si la opción no existe en esta versión de KDE, no debe tumbar lo demás. */
  ksd("output." + out + ".vrr.never");
  ksd("output." + out + ".vrr.disable");
  if (disableHdr) ksd("output." + out + ".hdr.disable");
  if (!r.empty() && r.find("Unable to parse") != std::string::npos) {
    if (err) *err = r;
    return false;
  }
  return true;
}

bool Panel::restore(const PanelState &st, std::string *err) {
  if (st.name.empty()) return true;
  std::ostringstream args;
  args << "output." << st.name << ".mode." << st.modeIndex << " output." << st.name << ".scale." << st.scale;
  if (const char *rot = rotationWord(st.rotation))
    args << " output." << st.name << ".rotation." << rot;
  std::string r = ksd(args.str());
  if (st.hdr) ksd("output." + st.name + ".hdr.enable");
  if (st.vrr == "Always")
    ksd("output." + st.name + ".vrr.always");
  else if (st.vrr == "Automatic")
    ksd("output." + st.name + ".vrr.automatic");
  return r.find("Unable to parse") == std::string::npos;
}

/* ------------- instantanea en disco (por si el programa muere a mitad) ------------- */

static std::string stateDir() {
  const char *xdg = std::getenv("XDG_STATE_HOME");
  if (xdg && *xdg) return std::string(xdg) + "/odyssey3d";
  const char *home = std::getenv("HOME");
  std::string base = (home && *home) ? std::string(home) : std::string(".");
  return base + "/.local/state/odyssey3d";
}

static std::string snapshotPath() { return stateDir() + "/panel.state"; }

bool Panel::saveSnapshot(const PanelState &st, std::string *path, std::string *err) {
  std::string p = snapshotPath();
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(p).parent_path(), ec);
  std::ofstream os(p, std::ios::trunc);
  if (!os) {
    if (err) *err = "no se pudo escribir " + p;
    return false;
  }
  char num[32];
  std::snprintf(num, sizeof(num), "%g", st.scale);
  os << "output=" << st.name << "\n"
     << "mode=" << st.modeIndex << " " << st.modeRes << "\n"
     << "scale=" << num << "\n"
     << "rotation=" << st.rotation << "\n"
     << "vrr=" << st.vrr << "\n"
     << "hdr=" << (st.hdr ? 1 : 0) << "\n";
  os.close();
  if (path) *path = p;
  return true;
}

bool Panel::loadSnapshot(PanelState &st, std::string *path, std::string *err) {
  std::string p = snapshotPath();
  if (path) *path = p;
  std::ifstream is(p);
  if (!is) {
    if (err) *err = "no hay instantanea en " + p;
    return false;
  }
  std::string line;
  while (std::getline(is, line)) {
    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = line.substr(0, eq), val = line.substr(eq + 1);
    if (key == "output")
      st.name = val;
    else if (key == "mode") {
      std::istringstream ss(val);
      ss >> st.modeIndex;
      std::getline(ss, st.modeRes);
      st.modeRes = strTrim(st.modeRes);
    } else if (key == "scale")
      st.scale = atof(val.c_str());
    else if (key == "rotation")
      st.rotation = atoi(val.c_str());
    else if (key == "vrr")
      st.vrr = val;
    else if (key == "hdr")
      st.hdr = val == "1";
  }
  if (st.name.empty()) {
    if (err) *err = "instantanea invalida en " + p;
    return false;
  }
  return true;
}

bool Panel::clearSnapshot(std::string *path) {
  std::string p = snapshotPath();
  if (path) *path = p;
  std::error_code ec;
  std::filesystem::remove(std::filesystem::path(p), ec);
  return !ec;
}

std::string Panel::resolveOutput(const std::string &want) {
  if (!want.empty()) return want;
  /* Solo vale un monitor con el modo nativo del Odyssey (3840x2160): antes se devolvia el
     primero de la lista aunque no lo tuviera, y con dos monitores conectados el visor acababa
     en el monitor normal, donde la trama de puntos se ve como una imagen borrosa. */
  std::string porNombre, porModo;
  for (const std::string &o : outputs()) {
    PanelState st{};
    if (!query(o, st)) continue;
    if (pickMode(st, 1e9) < 0) continue;
    std::string pista = o;
    for (char &c : pista) c = (char)std::tolower((unsigned char)c);
    const bool esOdyssey = pista.find("odyssey") != std::string::npos ||
                           pista.find("syncmaster") != std::string::npos ||
                           pista.find("g90") != std::string::npos;
    if (esOdyssey && porNombre.empty()) porNombre = o;
    if (porModo.empty()) porModo = o;
  }
  if (!porNombre.empty() || !porModo.empty()) return porNombre.empty() ? porModo : porNombre;
  std::fprintf(stderr, "[aviso] ningun monitor conectado tiene modo 3840x2160: el 3D necesita el "
                       "panel del Odyssey, conectaolo o apaga el otro monitor\n");
  return std::string{};
}

std::string Panel::modeName(const PanelState &st, int modeIndex) {
  for (const auto &m : st.modes)
    if (m.first == modeIndex) return m.second;
  return {};
}
