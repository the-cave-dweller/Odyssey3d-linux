/* depth.cpp — envoltorio minimo del modelo de profundidad. Esta todo aqui dentro para que el
   resto del programa no dependa de ONNX Runtime: si el proyecto se compila sin la biblioteca,
   init() avisa y el conversor simplemente no esta disponible. */
#include "depth.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <dlfcn.h>
#include <unordered_map>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_ORT
#include "util.h"
#include <onnxruntime_cxx_api.h>
#endif

namespace {

/* Ruta normal del modelo, la que usa el instalador y el aviso por consola. La misma convencion
   usa el boton 1 / 2 de la pastilla para saber donde esta "la nuestra". */
std::string defaultModelPath() { return rutaRedNuestra(); }

/* Muestreo bilineal sobre la imagen de origen (RGBA, primera fila arriba). */
void sampleBilinear(const uint8_t *src, int w, int h, float *dstPlanar, int sz) {
  const float sx = (float)w / (float)sz, sy = (float)h / (float)sz;
  const size_t plane = (size_t)sz * (size_t)sz;
  for (int y = 0; y < sz; y++) {
    float fy = ((float)y + 0.5f) * sy - 0.5f;
    if (fy < 0.0f) fy = 0.0f;
    if (fy > (float)(h - 1)) fy = (float)(h - 1);
    const int y0 = (int)fy, y1 = y0 + 1 < h ? y0 + 1 : y0;
    const float wy = fy - (float)y0;
    for (int x = 0; x < sz; x++) {
      float fx = ((float)x + 0.5f) * sx - 0.5f;
      if (fx < 0.0f) fx = 0.0f;
      if (fx > (float)(w - 1)) fx = (float)(w - 1);
      const int x0 = (int)fx, x1 = x0 + 1 < w ? x0 + 1 : x0;
      const float wx = fx - (float)x0;
      const uint8_t *p00 = src + ((size_t)y0 * (size_t)w + (size_t)x0) * 4;
      const uint8_t *p01 = src + ((size_t)y0 * (size_t)w + (size_t)x1) * 4;
      const uint8_t *p10 = src + ((size_t)y1 * (size_t)w + (size_t)x0) * 4;
      const uint8_t *p11 = src + ((size_t)y1 * (size_t)w + (size_t)x1) * 4;
      const size_t o = (size_t)y * (size_t)sz + (size_t)x;
      for (int c = 0; c < 3; c++) {
        const float a = p00[c] * (1.0f - wx) + p01[c] * wx;
        const float b = p10[c] * (1.0f - wx) + p11[c] * wx;
        const float v = (a * (1.0f - wy) + b * wy) / 255.0f;
        dstPlanar[(size_t)c * plane + o] = v;
      }
    }
  }
}

/* Media en una ventana cuadrada de radio r, con pasada separable (suma deslizante): lo que hace
   falta para el filtro guiado, y sale barato a 518 x 518. */
void caja(const float *entra, float *temp, float *sale, int n, int r) {
  const float inv = 1.0f / (float)(2 * r + 1);
  for (int y = 0; y < n; y++) {                      /* filas */
    const float *fila = entra + (size_t)y * n;
    float *o = temp + (size_t)y * n;
    float suma = 0.0f;
    for (int x = -r; x <= r; x++) suma += fila[x < 0 ? 0 : (x >= n ? n - 1 : x)];
    for (int x = 0; x < n; x++) {
      o[x] = suma * inv;
      const int fuera = x - r, nuevo = x + r + 1;
      suma -= fila[fuera < 0 ? 0 : fuera];
      suma += fila[nuevo >= n ? n - 1 : nuevo];
    }
  }
  for (int x = 0; x < n; x++) {                      /* columnas */
    float suma = 0.0f;
    for (int y = -r; y <= r; y++) suma += temp[(size_t)(y < 0 ? 0 : (y >= n ? n - 1 : y)) * n + x];
    for (int y = 0; y < n; y++) {
      sale[(size_t)y * n + x] = suma * inv;
      const int viejo = y - r, nuevo = y + r + 1;
      suma -= temp[(size_t)(viejo < 0 ? 0 : viejo) * n + x];
      suma += temp[(size_t)(nuevo >= n ? n - 1 : nuevo) * n + x];
    }
  }
}

/* Filtro guiado (He et al., 2010): suaviza el mapa de profundidad pero parandose en los bordes de
   la foto. Es el que usa el programa de Samsung (radio 8, epsilon 0,04) y es lo que evita que el
   relieve se coma los cantos de los objetos o los haga temblar. */
void filtroGuiado(const float *guia, const float *prof, float *fuera, int n, int r, float eps,
                  float *trabajo) {
  const size_t plane = (size_t)n * (size_t)n;
  float *mI = trabajo, *mP = trabajo + plane, *cII = trabajo + 2 * plane, *cIP = trabajo + 3 * plane,
        *a = trabajo + 4 * plane, *b = trabajo + 5 * plane, *aux = trabajo + 6 * plane;
  caja(guia, aux, mI, n, r);
  caja(prof, aux, mP, n, r);
  for (size_t i = 0; i < plane; i++) aux[i] = guia[i] * guia[i];
  caja(aux, trabajo + 7 * plane, cII, n, r);
  for (size_t i = 0; i < plane; i++) aux[i] = guia[i] * prof[i];
  caja(aux, trabajo + 7 * plane, cIP, n, r);
  for (size_t i = 0; i < plane; i++) {
    const float var = cII[i] - mI[i] * mI[i];
    const float aa = (cIP[i] - mI[i] * mP[i]) / (var + eps);
    a[i] = aa;
    b[i] = mP[i] - aa * mI[i];
  }
  float *mA = mI, *mB = mP;              /* reusar huecos: ya no hacen falta las medias */
  caja(a, aux, mA, n, r);
  caja(b, aux, mB, n, r);
  for (size_t i = 0; i < plane; i++) fuera[i] = mA[i] * guia[i] + mB[i];
}

/* Dilatar lo cercano unos puntos: tras suavizar, los objetos encogen y el canto queda corrido hacia
   dentro. Radio 3 puntos a 518 de ancho, que es lo que equivale la cifra de Samsung (0,006). */
void dilatar(float *mapa, float *aux, int n, int r) {
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      float m = mapa[(size_t)y * n + x];
      for (int k = -r; k <= r; k++) {
        const int xx = x + k;
        if (xx < 0 || xx >= n) continue;
        const float v = mapa[(size_t)y * n + (size_t)xx];
        if (v > m) m = v;
      }
      aux[(size_t)y * n + x] = m;
    }
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      float m = aux[(size_t)y * n + x];
      for (int k = -r; k <= r; k++) {
        const int yy = y + k;
        if (yy < 0 || yy >= n) continue;
        const float v = aux[(size_t)yy * n + (size_t)x];
        if (v > m) m = v;
      }
      mapa[(size_t)y * n + x] = m;
    }
}

}  // namespace

#ifdef HAVE_ORT
namespace {
struct OrtImpl {
  Ort::Env env;
  Ort::SessionOptions so{};
  Ort::Session *ses = nullptr;
  Ort::AllocatorWithDefaultOptions alloc;
  std::string inN, outN;   /* nombres de la entrada y la salida, copiados: viven lo que nosotros */
  std::vector<float> in;
  OrtImpl() : env(ORT_LOGGING_LEVEL_ERROR, "odyssey3d") {}
  ~OrtImpl() { delete ses; }
};
}  // namespace
#endif

/* Los mensajes de ONNX Runtime vienen con rutas de compilacion y plantillas de C++ completas. Para
   el registro basta el fallo concreto: "CUDA failure 101: invalid device ordinal" o la primera linea
   del resto. */
static std::string resumenOrt(const std::string &m) {
  size_t k = m.find("CUDA failure");
  std::string salida;
  if (k != std::string::npos) {
    size_t fin = m.find(" ; ", k);
    salida = m.substr(k, fin == std::string::npos ? std::string::npos : fin - k);
  } else {
    size_t fin = m.find('\n');
    salida = m.substr(0, fin == std::string::npos ? std::string::npos : fin);
  }
  if (salida.size() > 220)
    salida = salida.substr(0, 220) + "...";
  return salida;
}

/* Cuanta memoria libre queda en cada tarjeta. nvidia-smi responde "0, 246" por linea; si no esta
   (AMD, o sin el utilitario) devuelve vacio y no se mira nada. Sirve para no abrir la red donde no
   cabe: con la tarjeta reventada la creacion de la sesion revienta y el conversor no arranca. */
static std::vector<long> vramLibreMiB() {
  std::vector<long> libres;
  FILE *p = popen("nvidia-smi --query-gpu=index,memory.free --format=csv,noheader,nounits 2>/dev/null", "r");
  if (!p) return libres;
  char linea[128];
  while (std::fgets(linea, sizeof linea, p)) {
    long idx = -1, ml = -1;
    if (std::sscanf(linea, "%ld, %ld", &idx, &ml) == 2 && idx >= 0 && idx < 16) {
      if ((int)libres.size() <= idx) libres.resize((size_t)idx + 1, -1);
      libres[(size_t)idx] = ml;
    }
  }
  pclose(p);
  return libres;
}

bool DepthNet::init(const std::string &model, std::string *err, int gpu) {
  const std::string path = model.empty() ? defaultModelPath() : model;
  gpu_ = gpu < 0 ? 0 : gpu;
  if (const char *lim = std::getenv("ODYSSEY3D_LIMPIEZA"))   /* 0 deja el mapa en crudo: para comparar */
    limpieza_ = lim[0] != '0';
  struct stat stt {};
  if (stat(path.c_str(), &stt) != 0) {
    if (err)
      *err = "no esta el modelo de profundidad en " + path +
             "\n    bajarlo con:  mkdir -p ~/.local/share/odyssey3d/modelos && curl -fLo \"" + path +
             "\" https://huggingface.co/onnx-community/depth-anything-v2-small/resolve/main/model.onnx";
    return false;
  }
#ifdef HAVE_ORT
  /* El complemento de CUDA de ONNX Runtime no encuentra solo los sibolos de cuDNN: hay que tenerlas
     cargadas en el ambito global antes de abrirlo (probado con onnxruntime 1.29 + cudnn 9.28). */
  for (const char *lib : {"libcudnn.so.9", "libcudnn_cnn.so.9", "libcudnn_adv.so.9", "libcudnn_graph.so.9"})
    dlopen(lib, RTLD_NOW | RTLD_GLOBAL);
  /* Elegir donde corre la red: si en la tarjeta pedida no caben los ~1,1 GB que necesita (contexto
     de CUDA + modelo + activaciones), buscar la que mas memoria libre tenga. Y si en ninguna cabe,
     ir directos a la CPU antes de que la creacion de la sesion reviente. */
  const long kNecesitoMiB = 1100;
  bool sinGpu = false;
  {
    const std::vector<long> libres = vramLibreMiB();
    if (!libres.empty()) {
      long mia = gpu_ < (int)libres.size() ? libres[(size_t)gpu_] : -1;
      int mejor = gpu_;
      long mejorMiB = mia;
      for (size_t i = 0; i < libres.size(); i++)
        if (libres[i] > mejorMiB) { mejorMiB = libres[i]; mejor = (int)i; }
      if (mia >= 0 && mia < kNecesitoMiB) {
        if (mejor != gpu_ && mejorMiB >= kNecesitoMiB) {
          std::printf("[conversor] la grafica %d tiene solo %ld MB libres: la red se va a la grafica %d "
                      "(%ld MB libres)\n", gpu_, mia, mejor, mejorMiB);
          std::fflush(stdout);
          gpu_ = mejor;
        } else {
          std::printf("[conversor] no hay %ld MB libres en ninguna grafica: la red se hara con la CPU, "
                      "mas despacio\n", kNecesitoMiB);
          std::fflush(stdout);
          sinGpu = true;
        }
      }
    }
  }
  OrtImpl *im = new OrtImpl();
  im->so.SetIntraOpNumThreads(4);
  im->so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  /* Poco margen: con el navegador y el compositor por delante la tarjeta suele estar casi llena.
     Con el arenal de memoria apagada el pico baja bastante (el modelo pequeno no necesita 1 GB
     reservados de golpe), y aun asi se le deja un techo por si acaso. */
  try {
    if (sinGpu) throw std::runtime_error("sin sitio en la grafica");
    std::unordered_map<std::string, std::string> co;
    co["device_id"] = std::to_string(gpu_);
    co["gpu_mem_limit"] = "550000000";
    co["enable_mem_arena"] = "0";
    co["cudnn_conv_algo_search"] = "heuristic";
    im->so.AppendExecutionProvider("CUDA", co);
    dev_ = "NVIDIA";
  } catch (const std::exception &e) {
    try {
      OrtCUDAProviderOptions o{};
      o.device_id = gpu_;
      o.gpu_mem_limit = 550ull << 20;
      o.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchHeuristic;
      im->so.AppendExecutionProvider_CUDA(o);
      dev_ = "NVIDIA";
    } catch (const std::exception &e2) {
      std::printf("[conversor] la tarjeta no se pudo usar (%s): se hara con la CPU, mas despacio\n", e2.what());
      dev_ = "CPU";
    }
  }
  auto abrir = [](OrtImpl *q, const std::string &ruta, int sz) {
    q->ses = new Ort::Session(q->env, ruta.c_str(), q->so);
    q->inN = q->ses->GetInputNameAllocated(0, q->alloc).get();
    q->outN = q->ses->GetOutputNameAllocated(0, q->alloc).get();
    q->in.assign((size_t)3 * (size_t)sz * (size_t)sz, 0.0f);
    std::vector<int64_t> shape{1, 3, sz, sz};
    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tin = Ort::Value::CreateTensor<float>(mem, q->in.data(), q->in.size(), shape.data(), shape.size());
    const char *inN = q->inN.c_str(), *outN = q->outN.c_str();
    q->ses->Run(Ort::RunOptions{nullptr}, &inN, &tin, 1, &outN, 1);   /* el primero vale para calentar */
  };
  std::string fallo;
  try {
    abrir(im, path, sz_);
  } catch (const std::exception &e) {
    fallo = resumenOrt(e.what());
    if (dev_ == "CPU") {   /* ya no queda donde intentar */
      if (err) *err = "el modelo no abrio: " + fallo;
      delete im;
      return false;
    }
    /* La tarjeta dijo que no: ultima oportunidad, la CPU. Va lento (cientos de ms por cuadro), pero
       el relieve sale y se puede usar con --cada alto. */
    std::printf("[conversor] la grafica no dio para abrir el modelo (%s): se intentara con la CPU\n",
                fallo.c_str());
    std::fflush(stdout);
    delete im;
    im = new OrtImpl();
    im->so.SetIntraOpNumThreads(6);
    im->so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    dev_ = "CPU";
    try {
      abrir(im, path, sz_);
    } catch (const std::exception &e2) {
      if (err) *err = "el modelo no abrio: " + resumenOrt(e2.what());
      delete im;
      return false;
    }
  }
  impl_ = im;
  ready_ = true;
  ruta_ = path;
  std::printf("[conversor] modelo %s (%d x %d) con %s\n", path.c_str(), sz_, sz_, dev_.c_str());
  return true;
#else
  (void)path;
  if (err) *err = "el programa esta compilado sin ONNX Runtime: el conversor 2D a 3D no esta disponible";
  return false;
#endif
}

void DepthNet::cierra() {
#ifdef HAVE_ORT
  if (impl_) { delete static_cast<OrtImpl *>(impl_); impl_ = nullptr; }
#endif
  ready_ = false;
}

DepthNet::~DepthNet() { cierra(); }

bool DepthNet::cambiar(const std::string &model, std::string *err) {
  const std::string nueva = model.empty() ? defaultModelPath() : model;
  if (!ruta_.empty() && nueva == ruta_) return true;   /* ya esta abierta esa red */
  const std::string vieja = ruta_;
  const int gpu = gpu_;
  const float sm = smooth_;
  const bool lim = limpieza_;
  /* Se suelta la sesion vieja ANTES de abrir la nueva: es lo que devuelve la memoria de la
     grafica, y sin eso la segunda red no cabe y se va a la CPU. */
  cierra();
  prev_.clear();
  haveScale_ = false;
  cuadrosDesdeCorte_ = 0;          /* la otra red no comparte el suavizado anterior */
  std::string e;
  if (init(nueva, &e, gpu)) {
    smooth_ = sm;
    limpieza_ = lim;
    return true;
  }
  if (!vieja.empty()) {
    std::string e2;
    if (init(vieja, &e2, gpu)) {
      smooth_ = sm;
      limpieza_ = lim;
      if (err) *err = "no abrio " + nueva + ": " + e + "\n    se sigue con la anterior (" + vieja + ")";
      return false;
    }
  }
  if (err) *err = e;
  return false;
}

bool DepthNet::estimate(const uint8_t *rgba, int w, int h, std::vector<uint8_t> &out, std::string *err) {
#ifdef HAVE_ORT
  if (!ready_ || !rgba || w < 2 || h < 2) return false;
  OrtImpl *im = (OrtImpl *)impl_;
  const auto t0 = std::chrono::steady_clock::now();
  const size_t plane = (size_t)sz_ * (size_t)sz_;
  sampleBilinear(rgba, w, h, im->in.data(), sz_);
  /* guia para limpiar el mapa despues: la luminancia de la foto, tomada antes de normalizarla
     para la red (es la que dice donde estan los cantos de los objetos) */
  guia_.resize(plane);
  for (size_t i = 0; i < plane; i++)
    guia_[i] = 0.299f * im->in[i] + 0.587f * im->in[plane + i] + 0.114f * im->in[2 * plane + i];
  /* normalizacion de la foto: la misma con la que se entreno el modelo */
  static const float mean[3] = {0.485f, 0.456f, 0.406f}, sd[3] = {0.229f, 0.224f, 0.225f};
  for (int c = 0; c < 3; c++) {
    const float m = mean[c], s = sd[c];
    float *p = im->in.data() + (size_t)c * plane;
    for (size_t i = 0; i < plane; i++) p[i] = (p[i] - m) / s;
  }
  std::vector<int64_t> shape{1, 3, sz_, sz_};
  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value tin =
      Ort::Value::CreateTensor<float>(mem, im->in.data(), im->in.size(), shape.data(), shape.size());
  const char *inN = im->inN.c_str(), *outN = im->outN.c_str();
  std::vector<Ort::Value> outs;
  try {
    outs = im->ses->Run(Ort::RunOptions{nullptr}, &inN, &tin, 1, &outN, 1);
  } catch (const std::exception &e) {
    if (err) *err = std::string("el modelo fallo: ") + resumenOrt(e.what());
    return false;
  }
  const float *d = outs[0].GetTensorData<float>();
  auto info = outs[0].GetTensorTypeAndShapeInfo();
  const size_t n = info.GetElementCount();
  if (!n) return false;

  /* el modelo dice cuanto de cerca esta cada punto, en unidades suyas: lo llevamos a 0..1 con el
     minimo y el maximo, mirando solo una muestra de puntos para que un pixel raro no mande. */
  float lo = d[0], hi = d[0];
  size_t taken = 0;
  for (size_t i = 0; i < n; i += 17) {
    if (d[i] < lo) lo = d[i];
    if (d[i] > hi) hi = d[i];
    taken++;
  }
  if (taken < 8) {
    lo = hi = d[0];
    for (size_t i = 1; i < n; i++) {
      if (d[i] < lo) lo = d[i];
      if (d[i] > hi) hi = d[i];
    }
  }
  if (hi - lo < 1e-4f) hi = lo + 1e-4f;
  const float invCrudo = 1.0f / (hi - lo);

  /* cambio de plano: si el mapa nuevo no se parece nada al anterior, la escala que llevamos
     acumulada ya no vale y hay que reiniciarla de golpe (si no, los pocos cuadros siguientes salen
     con el rango del plano viejo: el relieve se ve apagado o quemado). Las cifras son las del
     programa de Samsung: diferencia media de 0,12 y descanso de 15 cuadros entre reinicios. */
  float dif = 0.0f;
  size_t tomadas = 0;
  if (prev_.size() == plane && n == plane) {
    for (size_t i = 0; i < n; i += 41) {
      dif += std::fabs((d[i] - lo) * invCrudo * 255.0f - (float)prev_[i]);
      tomadas++;
    }
    if (tomadas) dif /= (float)tomadas * 255.0f;
  }
  corte_ = tomadas > 0 && dif > 0.12f && cuadrosDesdeCorte_ >= 15;
  if (corte_) {
    cuadrosDesdeCorte_ = 0;
    cortes_++;
    haveScale_ = false;
  } else {
    cuadrosDesdeCorte_++;
  }

  if (!haveScale_) {
    loS_ = lo, hiS_ = hi;
    haveScale_ = true;
  } else {
    const float a = 0.25f;   /* la escala nueva entra poco a poco: el relieve no baila */
    loS_ += (lo - loS_) * a;
    hiS_ += (hi - hiS_) * a;
    if (hiS_ - loS_ < 1e-4f) hiS_ = loS_ + 1e-4f;
  }
  const float inv = 1.0f / (hiS_ - loS_);

  auto shp = info.GetShape();
  int ow = sz_, oh = sz_;
  if (shp.size() >= 2) {
    ow = (int)shp[shp.size() - 1];
    oh = (int)shp[shp.size() - 2];
  }
  if ((size_t)ow * (size_t)oh != n) ow = oh = 0;   /* respondio a otra resolucion: remuestreamos */

  std::vector<float> mapa(plane), limpio(plane), salida(plane), trabajo((size_t)8 * plane);
  if (ow == 0) {
    for (int y = 0; y < sz_; y++) {
      const int sy = (int)((float)y * (float)oh / (float)sz_);
      for (int x = 0; x < sz_; x++) {
        const int sx = (int)((float)x * (float)ow / (float)sz_);
        float v = (d[(size_t)(sy < oh ? sy : oh - 1) * (size_t)ow + (sx < ow ? sx : ow - 1)] - loS_) * inv;
        mapa[(size_t)y * (size_t)sz_ + (size_t)x] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
      }
    }
  } else {
    for (size_t i = 0; i < plane; i++) {
      float v = (d[i] - loS_) * inv;
      mapa[i] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
  }

  /* en el tiempo: el mapa nuevo entra con el peso que pide el usuario, salvo en un cambio de plano,
     que se toma tal cual para no arrastrar el cuadro anterior sobre el plano nuevo. */
  const bool suavizar = prev_.size() == plane && smooth_ > 0.0f && !corte_;
  const float q = 1.0f - smooth_;
  if (suavizar) {
    for (size_t i = 0; i < plane; i++)
      limpio[i] = mapa[i] * q + ((float)prev_[i] / 255.0f) * smooth_;
  } else {
    limpio = mapa;
  }

  /* limpiar el mapa siguiendo los bordes de la foto y dilatar lo cercano unos puntos: con esto los
     cantos de los objetos quedan en su sitio y el relieve deja de temblar cuadro a cuadro. Con
     ODYSSEY3D_LIMPIEZA=0 se apaga, para comparar en el panel la misma red con el mapa en crudo. */
  if (limpieza_) {
    filtroGuiado(guia_.data(), limpio.data(), salida.data(), sz_, 8, 0.04f, trabajo.data());
    dilatar(salida.data(), mapa.data(), sz_, 3);
  } else {
    salida = limpio;
  }

  /* plano de apoyo: el pico del histograma ponderado (mas peso en el centro y en los cantos, que es
     donde suele estar el sujeto). Con el apoyo ahi el sujeto se queda en el plano de la pantalla y
     el relieve se abre detras, que es como menos cansa la vista. De suelo se usa la mediana, para
     que nunca quede mas de media imagen por delante de la pantalla: ahi es donde molesta a los ojos. */
  {
    float hist[32] = {0.0f};
    float total = 0.0f;
    for (int y = 1; y < sz_ - 1; y++) {
      for (int x = 1; x < sz_ - 1; x++) {
        const size_t o = (size_t)y * (size_t)sz_ + (size_t)x;
        const float gx = std::fabs(guia_[o + 1] - guia_[o - 1]) + std::fabs(guia_[o + sz_] - guia_[o - sz_]);
        float w = 0.15f + 5.0f * gx;
        if (w > 1.0f) w = 1.0f;
        const bool dentro = x >= sz_ * 2 / 10 && x < sz_ * 8 / 10 && y >= sz_ * 2 / 10 && y < sz_ * 8 / 10;
        w *= dentro ? 1.0f : 0.45f;      /* sesgo suave al centro: un sujeto apartado tambien ancla */
        const int bin = (int)(salida[o] * 31.99f);
        hist[bin] += w;
        total += w;
      }
    }
    int mejor = 0;
    for (int i = 1; i < 32; i++)
      if (hist[i] > hist[mejor]) mejor = i;
    int mediana = 31;
    if (total > 0.0f) {
      float acum = 0.0f;
      for (int i = 0; i < 32; i++) {
        acum += hist[i];
        if (acum >= 0.5f * total) { mediana = i; break; }
      }
    }
    float nuevo = ((float)(mejor > mediana ? mejor : mediana) + 0.5f) / 32.0f;
    if (corte_ || std::fabs(nuevo - focus_) >= 0.01f)
      focus_ += (nuevo - focus_) * (corte_ ? 1.0f : 0.06f);   /* Samsung usa 0,02; 0,06 responde antes */
  }

  out.resize(plane);
  for (size_t i = 0; i < plane; i++) {
    const float v = salida[i] < 0.0f ? 0.0f : (salida[i] > 1.0f ? 1.0f : salida[i]);
    out[i] = (uint8_t)(v * 255.0f + 0.5f);
  }
  prev_ = out;
  ms_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return true;
#else
  (void)rgba, (void)w, (void)h, (void)out;
  if (err) *err = "sin ONNX Runtime";
  return false;
#endif
}
