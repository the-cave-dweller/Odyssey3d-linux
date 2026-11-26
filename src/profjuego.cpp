/* profjuego.cpp — ver profjuego.h y capa/protocolo.h */
#include "profjuego.h"

#include "../capa/protocolo.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
std::string nombreShm() {
  if (const char *e = std::getenv("ODYSSEYVK_PROFUNDIDAD_SHM")) return e;
  return "/odysseyvk-profundidad-" + std::to_string((long)getuid());
}
uint64_t monotonoNs() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}
double segundos() { return (double)monotonoNs() / 1e9; }
/* los VkFormat de profundidad (sin incluir Vulkan aqui) */
const char *nombreFormato(uint32_t f) {
  switch (f) {
    case 124: return "D16_UNORM";
    case 125: return "X8_D24_UNORM";
    case 126: return "D32_SFLOAT";
    case 128: return "D16_UNORM_S8_UINT";
    case 129: return "D24_UNORM_S8_UINT";
    case 130: return "D32_SFLOAT_S8_UINT";
    default: return "formato desconocido";
  }
}
}  // namespace

ProfundidadJuego::~ProfundidadJuego() { cerrar(); }

void ProfundidadJuego::cerrar() {
  if (base_) munmap(base_, tam_);
  if (fd_ >= 0) ::close(fd_);
  base_ = nullptr;
  fd_ = -1;
  tam_ = 0;
  inodo_ = 0;
  ultimoSeq_ = 0;
}

bool ProfundidadJuego::abrir() {
  const std::string nombre = nombreShm();
  /* abierta: ¿sigue siendo la misma? (el juego la rehace si cambia el tamano, y la borra al cerrar) */
  if (base_) {
    struct stat st{};
    const std::string ruta = "/dev/shm" + nombre;
    if (::stat(ruta.c_str(), &st) == 0 && (uint64_t)st.st_ino == inodo_ && (size_t)st.st_size == tam_) return true;
    cerrar();
  }
  /* sin juego, no se mira mas de una vez por segundo */
  const double t = segundos();
  if (t - ultimoIntento_ < 1.0) return false;
  ultimoIntento_ = t;
  fd_ = shm_open(nombre.c_str(), O_RDWR, 0);
  if (fd_ < 0) return false;
  struct stat st{};
  if (fstat(fd_, &st) != 0 || (size_t)st.st_size < ODVK_PROF_CABECERA) {
    cerrar();
    return false;
  }
  tam_ = (size_t)st.st_size;
  inodo_ = (uint64_t)st.st_ino;
  void *p = mmap(nullptr, tam_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (p == MAP_FAILED) {
    base_ = nullptr;
    cerrar();
    return false;
  }
  base_ = (uint8_t *)p;
  return true;
}

bool ProfundidadJuego::leer(std::vector<float> &cercania, int lado) {
  if (lado < 2 || !abrir()) return false;
  volatile OdvkProfCabecera *c = (volatile OdvkProfCabecera *)base_;
  if (c->magia != ODVK_PROF_MAGIA || c->version != ODVK_PROF_VERSION) return false;
  /* el juego vive. Si murio sin borrarla (Cyberpunk sale sin destruir su dispositivo de Vulkan: MEDIDO, se
     quedaban 24 MB en /dev/shm), la borra el visor */
  if (c->pid <= 0 || (::kill((pid_t)c->pid, 0) != 0 && errno == ESRCH)) {
    cerrar();
    shm_unlink(nombreShm().c_str());
    return false;
  }
  c->lectorLatido = monotonoNs();   /* «leo»: la capa copia mientras esto tenga menos de 2 s */
  const uint32_t w = c->ancho, h = c->alto, bpp = c->bytesPunto, huecos = c->huecos;
  const uint64_t tamHueco = c->tamHueco;
  if (w < 2 || h < 2 || (bpp != 2 && bpp != 4) || huecos == 0 ||
      ODVK_PROF_CABECERA + (uint64_t)huecos * tamHueco > tam_ || ODVK_PROF_DATOS + (uint64_t)w * h * bpp + 8 > tamHueco)
    return false;
  const uint64_t seq = c->ultimoSeq;
  const double t = segundos();
  if (seq == 0) return false;
  if (seq != ultimoSeq_) {
    ultimoSeq_ = seq;
    cuandoCambio_ = t;
  } else if (t - cuandoCambio_ > 0.5) {
    return false;   /* nada nuevo en medio segundo: el juego esta en pausa o la capa dejo de copiar */
  }
  /* el hueco de ese numero, con su comprobacion (ver protocolo.h) */
  const uint8_t *hueco = base_ + ODVK_PROF_CABECERA + (seq % huecos) * tamHueco;
  std::atomic_thread_fence(std::memory_order_acquire);
  if (*(volatile const uint64_t *)(hueco + tamHueco - 8) != seq) return false;
  const size_t bytes = (size_t)w * h * bpp;
  crudo_.resize(bytes);
  std::memcpy(crudo_.data(), hueco + ODVK_PROF_DATOS, bytes);
  std::atomic_thread_fence(std::memory_order_acquire);
  if (*(volatile const uint64_t *)hueco != seq) return false;   /* la tarjeta ya lo estaba pisando */

  /* profundidad del dispositivo, 0..1 */
  const size_t n = (size_t)w * h;
  std::vector<float> dep(n);
  const uint32_t formato = c->formato;
  if (bpp == 2) {
    const uint16_t *s = (const uint16_t *)crudo_.data();
    for (size_t i = 0; i < n; i++) dep[i] = s[i] / 65535.0f;
  } else if (formato == 125 || formato == 129) {
    const uint32_t *s = (const uint32_t *)crudo_.data();
    for (size_t i = 0; i < n; i++) dep[i] = (s[i] & 0xFFFFFFu) / 16777215.0f;
  } else {
    std::memcpy(dep.data(), crudo_.data(), n * 4);
  }
  /* Z invertida o normal: el cielo se limpia a lo lejano (0 con Z invertida, 1 con la normal); sin
     cielo, el suelo (abajo) suele estar mas cerca que lo de arriba. Las dos son lineales en
     1/distancia, como la disparidad que da la red: no hacen falta los planos cercano y lejano. */
  size_t ceros = 0, unos = 0;
  for (size_t i = 0; i < n; i += 7) {
    ceros += dep[i] <= 0.0f;
    unos += dep[i] >= 1.0f;
  }
  bool invertida;
  if (std::max(ceros, unos) * 7 > n / 500 && ceros != unos) {
    invertida = ceros > unos;
  } else {
    double ab = 0.0, ar = 0.0;
    for (uint32_t x = 0; x < w; x += 4) ab += dep[(size_t)(h - 1 - h / 8) * w + x], ar += dep[(size_t)(h / 8) * w + x];
    invertida = ab > ar;
  }
  std::vector<float> muestra;
  muestra.reserve(n / 37 + 1);
  for (size_t i = 0; i < n; i += 37) {
    const float v = dep[i];
    const bool lejos = invertida ? v <= 0.0f : v >= 1.0f;
    if (!lejos) muestra.push_back(invertida ? v : 1.0f - v);
  }
  if (muestra.size() < 64) return false;   /* todo cielo: una pantalla de carga */
  const size_t k1 = muestra.size() / 100, k99 = muestra.size() - 1 - muestra.size() / 100;
  std::nth_element(muestra.begin(), muestra.begin() + (long)k1, muestra.end());
  const float lo = muestra[k1];
  std::nth_element(muestra.begin(), muestra.begin() + (long)k99, muestra.end());
  const float hi = std::max(muestra[k99], lo + 1e-9f);
  const float inv = 1.0f / (hi - lo);
  for (size_t i = 0; i < n; i++) {
    const float v = dep[i];
    const bool lejos = invertida ? v <= 0.0f : v >= 1.0f;
    const float m = ((invertida ? v : 1.0f - v) - lo) * inv;
    dep[i] = lejos ? 0.0f : (m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m));
  }
  /* a la rejilla del mapa, por areas (cada punto, la media de lo que cae dentro: una cosa fina no
     desaparece entre dos muestras): primero en horizontal, despues en vertical */
  const size_t L = (size_t)lado;
  fila_.assign((size_t)h * L, 0.0f);
  for (uint32_t y = 0; y < h; y++) {
    const float *src = &dep[(size_t)y * w];
    for (size_t x = 0; x < L; x++) {
      const double a = (double)x * w / L, b = (double)(x + 1) * w / L;
      size_t i0 = (size_t)a, i1 = std::min((size_t)std::ceil(b), (size_t)w);
      double s = 0.0, peso = 0.0;
      for (size_t i = i0; i < i1; i++) {
        const double p = std::min(b, (double)i + 1) - std::max(a, (double)i);
        s += src[i] * p;
        peso += p;
      }
      fila_[(size_t)y * L + x] = peso > 0.0 ? (float)(s / peso) : 0.0f;
    }
  }
  cercania.assign(L * L, 0.0f);
  for (size_t y = 0; y < L; y++) {
    const double a = (double)y * h / L, b = (double)(y + 1) * h / L;
    const size_t j0 = (size_t)a, j1 = std::min((size_t)std::ceil(b), (size_t)h);
    for (size_t x = 0; x < L; x++) {
      double s = 0.0, peso = 0.0;
      for (size_t j = j0; j < j1; j++) {
        const double p = std::min(b, (double)j + 1) - std::max(a, (double)j);
        s += fila_[j * L + x] * p;
        peso += p;
      }
      cercania[y * L + x] = peso > 0.0 ? (float)(s / peso) : 0.0f;
    }
  }
  desc_ = std::to_string(w) + "x" + std::to_string(h) + " " + nombreFormato(formato) + ", Z " +
          (invertida ? "invertida" : "normal");
  return true;
}
