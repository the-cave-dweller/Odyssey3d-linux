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
#include <onnxruntime_cxx_api.h>
#endif

namespace {

/* Ruta normal del modelo, la que usa el instalador y el aviso por consola. */
std::string defaultModelPath() {
  const char *home = std::getenv("HOME");
  std::string base = home ? home : "/home";
  return base + "/.local/share/odyssey3d/modelos/depth_anything_v2_small.onnx";
}

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
  std::printf("[conversor] modelo %s (%d x %d) con %s\n", path.c_str(), sz_, sz_, dev_.c_str());
  return true;
#else
  (void)path;
  if (err) *err = "el programa esta compilado sin ONNX Runtime: el conversor 2D a 3D no esta disponible";
  return false;
#endif
}

bool DepthNet::estimate(const uint8_t *rgba, int w, int h, std::vector<uint8_t> &out, std::string *err) {
#ifdef HAVE_ORT
  if (!ready_ || !rgba || w < 2 || h < 2) return false;
  OrtImpl *im = (OrtImpl *)impl_;
  const auto t0 = std::chrono::steady_clock::now();
  const size_t plane = (size_t)sz_ * (size_t)sz_;
  sampleBilinear(rgba, w, h, im->in.data(), sz_);
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

  out.resize(plane);
  const bool suavizar = prev_.size() == plane && smooth_ > 0.0f;
  const float q = 1.0f - smooth_;
  const bool mismoTamano = (n == plane);
  auto shp = info.GetShape();
  int ow = sz_, oh = sz_;
  if (shp.size() >= 2) {
    ow = (int)shp[shp.size() - 1];
    oh = (int)shp[shp.size() - 2];
  }
  if (!mismoTamano && (ow < 2 || oh < 2 || (size_t)ow * (size_t)oh != n)) ow = oh = 0;
  if (mismoTamano || ow == 0) {
    for (size_t i = 0; i < plane; i++) {
      float v = (i < n ? (d[i] - loS_) * inv : 0.0f);
      if (v < 0.0f) v = 0.0f;
      if (v > 1.0f) v = 1.0f;
      int b = (int)(v * 255.0f + 0.5f);
      if (suavizar) b = (int)(b * q + (int)prev_[i] * smooth_ + 0.5f);
      out[i] = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));
    }
  } else {
    /* si el modelo respondio a otra resolucion, la llevamos a la nuestra con muestreo simple */
    for (int y = 0; y < sz_; y++) {
      int sy = (int)((float)y * (float)oh / (float)sz_);
      if (sy >= oh) sy = oh - 1;
      for (int x = 0; x < sz_; x++) {
        int sx = (int)((float)x * (float)ow / (float)sz_);
        if (sx >= ow) sx = ow - 1;
        float v = (d[(size_t)sy * (size_t)ow + (size_t)sx] - loS_) * inv;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        const size_t o = (size_t)y * (size_t)sz_ + (size_t)x;
        int b = (int)(v * 255.0f + 0.5f);
        if (suavizar) b = (int)(b * q + (int)prev_[o] * smooth_ + 0.5f);
        out[o] = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));
      }
    }
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
