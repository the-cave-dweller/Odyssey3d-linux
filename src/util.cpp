#include "util.h"

#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>

std::string strTrim(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && (unsigned char)s[a] <= ' ') a++;
  while (b > a && (unsigned char)s[b - 1] <= ' ') b--;
  return s.substr(a, b - a);
}

std::string strLower(std::string s) {
  for (char &c : s)
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  return s;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::vector<uint8_t> unhex(const std::string &s) {
  std::vector<uint8_t> out;
  int hi = -1;
  for (char c : s) {
    int v = hexVal(c);
    if (v < 0) continue; /* ignora separadores */
    if (hi < 0) {
      hi = v;
    } else {
      out.push_back((uint8_t)((hi << 4) | v));
      hi = -1;
    }
  }
  return out;
}

std::string toHexUpper(const uint8_t *p, size_t n) {
  static const char *D = "0123456789ABCDEF";
  std::string s;
  s.reserve(2 * n);
  for (size_t i = 0; i < n; i++) {
    s.push_back(D[p[i] >> 4]);
    s.push_back(D[p[i] & 15]);
  }
  return s;
}

std::string toHexUpper(const std::vector<uint8_t> &v) {
  return toHexUpper(v.data(), v.size());
}

bool readSmallFile(const std::string &path, std::string &out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

bool fileExists(const std::string &path) {
  return !path.empty() && access(path.c_str(), R_OK) == 0;
}

bool readBinaryFile(const std::string &path, std::vector<uint8_t> &out) {
  out.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  const std::streamoff n = f.tellg();
  if (n <= 0 || n > (std::streamoff)64 * 1024 * 1024) return false;
  f.seekg(0, std::ios::beg);
  out.resize((size_t)n);
  f.read((char *)out.data(), n);
  if (!f) {
    out.clear();
    return false;
  }
  return true;
}

int runCapture(const std::string &cmd, std::string &out) {
  std::array<char, 4096> buf{};
  FILE *p = popen(cmd.c_str(), "r");
  if (!p) return -1;
  while (fgets(buf.data(), (int)buf.size(), p)) out += buf.data();
  int rc = pclose(p);
  return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}

std::string usbIdToHex(uint16_t v) {
  char b[8];
  snprintf(b, sizeof b, "%04x", v);
  return b;
}

#include <cstdlib>
#include <filesystem>
#include <unistd.h>

std::string ensureWaylandEnv() {
  const char *wd = getenv("WAYLAND_DISPLAY");
  if (wd && *wd) return wd;
  const char *xdg = getenv("XDG_RUNTIME_DIR");
  std::string dir = (xdg && *xdg) ? xdg : ("/run/user/" + std::to_string((long long)getuid()));
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return {};
  std::vector<std::string> found;
  for (const auto &e : std::filesystem::directory_iterator(dir, ec)) {
    std::string n = e.path().filename().string();
    if (n.rfind("wayland-", 0) == 0 && std::filesystem::is_socket(e.path(), ec)) found.push_back(n);
  }
  if (found.empty()) return {};
  std::sort(found.begin(), found.end());
  setenv("WAYLAND_DISPLAY", found[0].c_str(), 1);
  setenv("XDG_RUNTIME_DIR", dir.c_str(), 1);
  return found[0];
}
