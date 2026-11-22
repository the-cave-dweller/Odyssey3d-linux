#include "panel.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
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
  switch (r) {
    case 1: return "left";
    case 2: return "inverted";
    case 3: return "right";
    default: return "none";
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
  int best = st.modeIndex;
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
  args << "output." << out << ".mode." << modeIndex << " output." << out << ".scale." << scale
       << " output." << out << ".rotation." << rotationWord(rotation);
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
  args << "output." << st.name << ".mode." << st.modeIndex << " output." << st.name << ".scale." << st.scale
       << " output." << st.name << ".rotation." << rotationWord(st.rotation);
  std::string r = ksd(args.str());
  if (st.hdr) ksd("output." + st.name + ".hdr.enable");
  if (st.vrr == "Always")
    ksd("output." + st.name + ".vrr.always");
  else if (st.vrr == "Automatic")
    ksd("output." + st.name + ".vrr.automatic");
  return r.find("Unable to parse") == std::string::npos;
}

std::string Panel::resolveOutput(const std::string &want) {
  if (!want.empty()) return want;
  for (const std::string &o : outputs()) {
    PanelState st{};
    if (!query(o, st)) continue;
    if (pickMode(st, 1e9) >= 0) return o;
  }
  std::vector<std::string> all = outputs();
  return all.empty() ? std::string{} : all.front();
}

std::string Panel::modeName(const PanelState &st, int modeIndex) {
  for (const auto &m : st.modes)
    if (m.first == modeIndex) return m.second;
  return {};
}
