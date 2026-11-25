/* prueba_profundidad.cpp — comprobar que el modelo de profundidad cabe en el presupuesto de
   cuadros: mide cuanto tarda una inferencia con la tarjeta NVIDIA y con la CPU.
   g++ -O2 tools/prueba_profundidad.cpp -I/usr/include/onnxruntime -lonnxruntime -o /tmp/pp
   /tmp/pp ~/.local/share/odysseyvk/modelos/depth_anything_v2_small.onnx [cuda|cpu]           */
#include <onnxruntime_cxx_api.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: prueba_profundidad MODELO.onnx [cuda|cpu]\n");
    return 2;
  }
  const bool cuda = argc < 3 || std::strcmp(argv[2], "cuda") == 0;
  Ort::Env env(ORT_LOGGING_LEVEL_ERROR, "odysseyvk");
  Ort::SessionOptions so{};
  so.SetIntraOpNumThreads(4);
  so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  if (cuda) {
    try {
      OrtCUDAProviderOptions o{};
      o.gpu_mem_limit = 1ull << 30;
      o.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
      so.AppendExecutionProvider_CUDA(o);
      std::printf("[modelo] usando la tarjeta NVIDIA\n");
    } catch (const std::exception &e) {
      std::printf("[modelo] la tarjeta no sirvio (%s): pasa a la CPU\n", e.what());
    }
  }
  std::string path = argv[1];
  Ort::Session ses(env, path.c_str(), so);
  Ort::AllocatorWithDefaultOptions alloc;
  /* los nombres viven en una cadena asignada por la propia biblioteca: hay que guardar el
     puntero inteligente, si no el nombre se libera y la ejecucion recibe basura */
  Ort::AllocatedStringPtr inP = ses.GetInputNameAllocated(0, alloc);
  Ort::AllocatedStringPtr outP = ses.GetOutputNameAllocated(0, alloc);
  const char *inName = inP.get();
  const char *outName = outP.get();
  std::printf("[modelo] entrada \"%s\"  salida \"%s\"\n", inName, outName);
  for (size_t i = 0; i < ses.GetInputCount(); i++) {
    Ort::ConstTensorTypeAndShapeInfo info = ses.GetInputTypeInfo(i).GetTensorTypeAndShapeInfo();
    std::vector<int64_t> sh = info.GetShape();
    std::printf("[modelo] forma de entrada:");
    for (int64_t v : sh) std::printf(" %lld", (long long)v);
    std::printf("\n");
  }

  const int W = 518, H = 518;
  std::vector<float> in((size_t)3 * H * W, 0.0f);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t base = (size_t)y * W + x;
      in[base] = 0.45f + 0.2f * std::sin(x / 40.0f);
      in[(size_t)H * W + base] = 0.45f + 0.2f * std::sin(y / 40.0f);
      in[2 * (size_t)H * W + base] = 0.45f;
    }
  std::vector<int64_t> shape{1, 3, H, W};
  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value tin = Ort::Value::CreateTensor<float>(mem, in.data(), in.size(), shape.data(), shape.size());
  const char *inNames[] = {inName};
  const char *outNames[] = {outName};
  for (int i = 0; i < 3; i++) ses.Run(Ort::RunOptions{nullptr}, inNames, &tin, 1, outNames, 1);
  auto t0 = std::chrono::steady_clock::now();
  std::vector<Ort::Value> outs = ses.Run(Ort::RunOptions{nullptr}, inNames, &tin, 1, outNames, 1);
  for (int i = 0; i < 9; i++) outs = ses.Run(Ort::RunOptions{nullptr}, inNames, &tin, 1, outNames, 1);
  auto t1 = std::chrono::steady_clock::now();
  Ort::Value &tout = outs[0];
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / 10.0;
  auto oinfo = tout.GetTensorTypeAndShapeInfo();
  std::vector<int64_t> osh = oinfo.GetShape();
  const size_t n = oinfo.GetElementCount();
  const float *d = tout.GetTensorData<float>();
  float lo = d[0], hi = d[0];
  for (size_t i = 1; i < n; i++) {
    if (d[i] < lo) lo = d[i];
    if (d[i] > hi) hi = d[i];
  }
  std::printf("[modelo] forma de salida:");
  for (int64_t v : osh) std::printf(" %lld", (long long)v);
  std::printf("\n[modelo] %d cuadros por segundo (%.1f ms por cuadro), valores %.3f..%.3f\n", (int)(1000.0 / ms),
              ms, lo, hi);
  return 0;
}
