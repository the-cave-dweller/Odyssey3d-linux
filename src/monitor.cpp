/* (v1.17) lo que se puede vigilar junto al cuadrito de los FPS: ver monitor.h */
#include "monitor.h"

#include "util.h"

#include <dlfcn.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>

namespace monitor {
namespace {

/* NVML, cargada al vuelo la primera vez (sin ella, ninguna NVIDIA). Solo lo que se usa, con sus firmas de nvml.h */
struct Nvml {
  bool lista = false;
  int (*init)() = nullptr;
  int (*cuenta)(unsigned *) = nullptr;
  int (*porIndice)(unsigned, void **) = nullptr;
  int (*nombre)(void *, char *, unsigned) = nullptr;
  int (*uuid)(void *, char *, unsigned) = nullptr;
  int (*uso)(void *, unsigned *) = nullptr;   /* nvmlUtilization_t: {gpu, memory} */
  int (*temp)(void *, int, unsigned *) = nullptr;
  int (*memoria)(void *, unsigned long long *) = nullptr;   /* nvmlMemory_t: {total, free, used} */
  int (*consumo)(void *, unsigned *) = nullptr;   /* mW */
};

Nvml &nvml() {
  static Nvml n = [] {
    Nvml x;
    void *lib = dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!lib) return x;
    x.init = (int (*)())dlsym(lib, "nvmlInit_v2");
    x.cuenta = (int (*)(unsigned *))dlsym(lib, "nvmlDeviceGetCount_v2");
    x.porIndice = (int (*)(unsigned, void **))dlsym(lib, "nvmlDeviceGetHandleByIndex_v2");
    x.nombre = (int (*)(void *, char *, unsigned))dlsym(lib, "nvmlDeviceGetName");
    x.uuid = (int (*)(void *, char *, unsigned))dlsym(lib, "nvmlDeviceGetUUID");
    x.uso = (int (*)(void *, unsigned *))dlsym(lib, "nvmlDeviceGetUtilizationRates");
    x.temp = (int (*)(void *, int, unsigned *))dlsym(lib, "nvmlDeviceGetTemperature");
    x.memoria = (int (*)(void *, unsigned long long *))dlsym(lib, "nvmlDeviceGetMemoryInfo");
    x.consumo = (int (*)(void *, unsigned *))dlsym(lib, "nvmlDeviceGetPowerUsage");
    x.lista = x.init && x.cuenta && x.porIndice && x.nombre && x.uuid && x.init() == 0;
    return x;
  }();
  return n;
}

std::string corto(std::string n) {
  for (const char *p : {"NVIDIA GeForce ", "NVIDIA ", "AMD Radeon ", "AMD "})
    if (n.rfind(p, 0) == 0) return n.substr(std::strlen(p));
  return n;
}

/* el procesador: la temperatura del sensor que la da (k10temp/zenpower en AMD, coretemp en Intel), buscada una vez */
std::string rutaTempCpu() {
  static const std::string r = [] {
    namespace fs = std::filesystem;
    std::error_code ec;
    for (const fs::directory_entry &h : fs::directory_iterator("/sys/class/hwmon", ec)) {
      std::string nombre;
      if (!readSmallFile(h.path().string() + "/name", nombre)) continue;
      nombre = strTrim(nombre);
      if (nombre == "k10temp" || nombre == "zenpower" || nombre == "coretemp") return h.path().string() + "/temp1_input";
    }
    return std::string();
  }();
  return r;
}

}  // namespace

Lectura leer() {
  Lectura l;
  Nvml &n = nvml();
  if (n.lista) {
    unsigned cuantas = 0;
    if (n.cuenta(&cuantas) == 0)
      for (unsigned i = 0; i < cuantas; i++) {
        void *d = nullptr;
        if (n.porIndice(i, &d) != 0) continue;
        Tarjeta t;
        char b[128] = {0};
        if (n.uuid(d, b, sizeof b) == 0) t.clave = std::string("gpu:") + b;
        std::memset(b, 0, sizeof b);
        if (n.nombre(d, b, sizeof b) == 0) t.nombre = corto(b);
        unsigned u[2] = {0, 0};
        if (n.uso && n.uso(d, u) == 0) t.uso = (int)u[0];
        unsigned c = 0;
        if (n.temp && n.temp(d, 0, &c) == 0) t.tempC = c;
        unsigned long long m[3] = {0, 0, 0};
        if (n.memoria && n.memoria(d, m) == 0) t.totalMiB = (long)(m[0] >> 20), t.usadaMiB = (long)(m[2] >> 20);
        unsigned mw = 0;
        if (n.consumo && n.consumo(d, &mw) == 0) t.vatios = mw / 1000.0;
        if (!t.clave.empty()) l.tarjetas.push_back(t);
      }
  }
  for (const TarjetaAmd &a : tarjetasAmd()) {
    Tarjeta t;
    t.clave = "gpu:amd:" + a.pci;
    t.nombre = corto(a.nombre.empty() ? std::string("AMD") : a.nombre);
    t.uso = a.ocupada, t.tempC = a.tempC, t.usadaMiB = a.usadaMiB, t.totalMiB = a.totalMiB;
    l.tarjetas.push_back(t);
  }
  /* el procesador: lo que estuvo ocupado desde la lectura anterior (la primera no da nada) */
  {
    static unsigned long long ocupAnt = 0, totalAnt = 0;
    std::ifstream f("/proc/stat");
    std::string cpu;
    unsigned long long v[8] = {0};
    if (f >> cpu && cpu == "cpu") {
      for (unsigned long long &x : v) f >> x;
      const unsigned long long total = v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6] + v[7];
      const unsigned long long ocup = total - v[3] - v[4];   /* sin idle ni iowait */
      if (totalAnt > 0 && total > totalAnt) l.cpu = 100.0 * (double)(ocup - ocupAnt) / (double)(total - totalAnt);
      ocupAnt = ocup, totalAnt = total;
    }
  }
  if (const std::string r = rutaTempCpu(); !r.empty()) {
    std::string t;
    if (readSmallFile(r, t)) l.cpuTempC = std::atof(t.c_str()) / 1000.0;
  }
  {
    std::ifstream f("/proc/meminfo");
    std::string clave;
    long kb = 0, total = -1, libre = -1;
    std::string unidad;
    while (f >> clave >> kb >> unidad) {
      if (clave == "MemTotal:") total = kb;
      else if (clave == "MemAvailable:") libre = kb;
    }
    if (total > 0 && libre >= 0) l.ramTotalMiB = total / 1024, l.ramUsadaMiB = (total - libre) / 1024;
  }
  return l;
}

std::set<std::string> deTexto(const std::string &t) {
  std::set<std::string> s;
  std::string campo;
  for (size_t i = 0; i <= t.size(); i++) {
    if (i == t.size() || t[i] == ',') {
      campo = strTrim(campo);
      if (!campo.empty() && campo != "no") s.insert(campo);
      campo.clear();
    } else {
      campo += t[i];
    }
  }
  return s;
}

std::string aTexto(const std::set<std::string> &s) {
  std::string t;
  for (const std::string &x : s) t += (t.empty() ? "" : ",") + x;
  return t;
}

Muestreo::~Muestreo() {
  fin_ = true;
  if (hilo_.joinable()) hilo_.join();
}

void Muestreo::pedir(bool hayAlgo) {
  quiere_ = hayAlgo;
  if (!hayAlgo || hilo_.joinable()) return;
  hilo_ = std::thread([this] {
    while (!fin_) {
      if (quiere_) {
        Lectura l = leer();
        {
          std::lock_guard<std::mutex> g(m_);
          ultima_ = std::move(l);
        }
        vuelta_++;
      }
      for (int k = 0; k < 10 && !fin_; k++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
}

Lectura Muestreo::ultima() {
  std::lock_guard<std::mutex> g(m_);
  return ultima_;
}

}  // namespace monitor
