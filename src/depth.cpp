/* depth.cpp — envoltorio minimo del modelo de profundidad. Esta todo aqui dentro para que el
   resto del programa no dependa de ONNX Runtime: si el proyecto se compila sin la biblioteca,
   init() avisa y el conversor simplemente no esta disponible. */
#include "depth.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <dlfcn.h>
#include <unordered_map>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_ORT
#include "util.h"
#include <onnxruntime_cxx_api.h>
#endif

namespace {

/* Ruta normal del modelo, la que usa el instalador y el aviso por consola. La misma convencion
   usa el boton 1 / 2 de la pastilla para saber donde esta "la nativa". */
std::string defaultModelPath() { return rutaRedNuestra(); }

/* Cuanta memoria libre hace falta para abrir una red en la grafica, y cuanto de colchon se deja
   entre lo que anuncia nvidia-smi y lo que de verdad queda cuando la sesion reclama su contexto.
   Medido el 24-nov a las 04:02 con tools/prueba_idayvuelta.cpp, sin tomar el panel: la red de
   Samsung ocupa 678 MB de memoria de grafica (la grafica 0 paso de 1667 a 989 MB libres) y la
   nativa unos 400. Al soltar una sesion no devuelve toda: se quedan unos 300 MB del contexto de
   CUDA y de cuDNN que el proceso ya pago, y eso es lo que hay que tener en cuenta.
   Con el numero viejo (1100 + 450 = 1550 fijos) el boton 1 casi nunca devolvia a la Samsung: con
   1291 MB libres pediamos 1550, no abria, y al volver atras hasta la red que si estaba andando se
   iba a la CPU (179 ms por mapa en lugar de 16). El colchon se mueve con ODYSSEYVK_COLCHON (MB)
   por alguna vez se viera un abort por la tarjeta llena. */
const long kSamsungMiB = 700;   /* lo que ocupa mde_* / disp_* en la grafica (medido 678) */
const long kNativaMiB = 480;    /* lo que ocupa depth_anything_v2_small (medido unos 400) */
const long kColchonMiB = 300;   /* lo que el navegador o el compositor pueden pedir entre medias */

/* La red de Samsung es la que se llama mde_* o disp_* (la de Depth Anything se llama otra cosa).
   Solo abre en la grafica: intentarla por CPU se cuelga, medido el 24-nov con mde_large.onnx mas de
   4 minutos quemando CPU sin llegar a crear la sesion, y en el visor acababa en abort (el nucleo del
   proceso se fue arriba con el panel en modo 3D y sin video). La nativa si va por CPU: 1 s en abrir
   y 190 ms por mapa. */
bool esRedDeSamsung(const std::string &ruta) {
  const size_t p2 = ruta.find_last_of('/');
  std::string b = p2 == std::string::npos ? ruta : ruta.substr(p2 + 1);
  for (char &c : b)
    c = (char)std::tolower((unsigned char)c);
  return b.rfind("mde_", 0) == 0 || b.rfind("disp_", 0) == 0;
}

/* Lo que tiene que anunciar libre nvidia-smi para abrir ESTA red en la grafica: lo que ocupa la red
   de Samsung o la nativa, mas el colchon. Cada red es un mundo, por eso se mira el nombre. */
long requisitoMiB(const std::string &ruta) {
  long colchon = kColchonMiB;
  if (const char *c = entornoOdyssey("ODYSSEYVK_COLCHON")) {   /* por si la tarjeta anda muy llena */
    const long v = std::atol(c);
    if (v >= 0 && v <= 2000) colchon = v;
  }
  return (esRedDeSamsung(ruta) ? kSamsungMiB : kNativaMiB) + colchon;
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

/* Media en una ventana cuadrada de radio r, con pasada separable (suma deslizante): lo que hace
   falta para el filtro guiado, y sale barato a 518 x 518. */
void caja(const float *entra, float *temp, float *sale, int n, int r) {
  const float inv = 1.0f / (float)(2 * r + 1);
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
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
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
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
        *a = trabajo + 4 * plane, *b = trabajo + 5 * plane, *iia = trabajo + 6 * plane,
        *ipa = trabajo + 7 * plane, *aux = trabajo + 8 * plane;
  /* los productos guia x guia y guia x prof viven en su propio hueco: asi no hay que copiar uno
     encima del otro y las dos cajas de arriba pueden solaparse en las hebras */
  caja(guia, aux, mI, n, r);
  caja(prof, aux, mP, n, r);
  /* ojo: la fila de paso (aux) nunca puede ser la misma bufeta que la entrada de la caja, porque
     la suma deslizante escribe por delante de donde todavia lee. */
  for (size_t i = 0; i < plane; i++) iia[i] = guia[i] * guia[i];
  caja(iia, aux, cII, n, r);
  for (size_t i = 0; i < plane; i++) ipa[i] = guia[i] * prof[i];
  caja(ipa, aux, cIP, n, r);
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
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
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
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
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
/* --- los dos planos ---------------------------------------------------------------
   Aqui se mira si el cuadro tiene de verdad dos grupos de profundidad (un fondo y unos primeros
   planos) y, si los hay, cada grupo recibe un valor propio: la imagen pasa de ser una lamina
   ondulada a ser dos laminas con su paralaje, que es como lo resuelve el Nintendo 3DS (el
   contenido trae dos capas y la pantalla solo escala, con un presupuesto fijo por ojo).

   El corte sale del propio histograma con la prueba de Otsu: se busca el punto que deja los dos
   grupos mas separados entre si. No hay un umbral escrito a mano ni que aprenderse el cuadro, y
   manana sirve igual sobre el recorte de `degraph` o sobre otra red, solo mira los numeros que
   ya hay en el mapa. */
Planos buscarPlanos(const float *mapa, int n) {
  Planos p;
  const int NB = 32;
  if (!mapa || n <= 0) return p;
  float hist[NB] = {0.0f};
  for (int i = 0; i < n; i++) {
    const float v = mapa[i] < 0.0f ? 0.0f : (mapa[i] > 1.0f ? 1.0f : mapa[i]);
    hist[(int)(v * 31.99f)] += 1.0f;
  }
  double total = 0.0, mediaTot = 0.0;
  for (int i = 0; i < NB; i++) {
    total += hist[i];
    mediaTot += hist[i] * ((double)i + 0.5);
  }
  if (total <= 0.0) return p;
  mediaTot /= total;
  int corte = -1;
  double mejor = -1.0, acumP = 0.0, acumM = 0.0;
  for (int t = 0; t < NB - 1; t++) {
    acumP += hist[t] / total;
    if (acumP <= 0.0) continue;
    acumM += hist[t] * ((double)t + 0.5) / total;
    const double detras = 1.0 - acumP;
    if (detras <= 0.0) break;
    const double m0 = acumM / acumP;
    const double m1 = (mediaTot - acumM) / detras;
    const double gan = acumP * detras * (m0 - m1) * (m0 - m1);
    if (gan > mejor) { mejor = gan; corte = t; }
  }
  if (corte < 0) return p;
  /* El valor de cada lamina es la MEDIA del grupo, no su pico: el pico salta con el ruido de la
     red y la media se sostiene cuando el cuadro respira. */
  double nA = 0.0, sA = 0.0, nB = 0.0, sB = 0.0;
  for (int i = 0; i < NB; i++) {
    if (hist[i] <= 0.0f) continue;
    if (i <= corte) { nA += hist[i]; sA += hist[i] * ((double)i + 0.5); }
    else { nB += hist[i]; sB += hist[i] * ((double)i + 0.5); }
  }
  if (nA <= 0.0 || nB <= 0.0) return p;
  p.fondo = (float)((sA / nA) / (double)NB);
  p.frente = (float)((sB / nB) / (double)NB);
  p.fraccion = (float)(nB / total);
  /* Un plano fijo o un pasillo tambien sacan un corte, pero con las dos laminas pegadas o con una
     de ellas comiendose casi todo el cuadro no hay dos planos que enseñar: mejor no tocar nada. */
  if (p.frente - p.fondo < 0.15f || p.fraccion < 0.06f || p.fraccion > 0.94f) return p;
  p.valido = true;
  return p;
}

/* Acercar cada punto al valor de su lamina con la fuerza `dureza` (0 = dejar el mapa tal cual,
   1 = laminas planas). Entre una y otra no se corta de golpe: se cruza en una franja suave,
   porque un tajo en el canto del recorte se ve y, encima, ese borde es el que parpadea cuando el
   mapa de la red tiembla. Devuelve la parte del cuadro que cae en la lamina delantera.

   `muestras` >= 1 cambia solo el cruce: en lugar de una rampa continua entre las dos laminas hay
   escalones, con una muestra intermedia por cada «muestras» pedido (1 = atras, medio, delante).
   Cada muestra tiene su MESETA, que es lo que se ve solido en el centro de la imagen, y el paso de
   una a otra se sigue haciendo con el suave de siempre pero en una franja angosta (`ancho`), que es
   lo que mantiene el canto sin tajo y sin parpadeo cuando el mapa de la red respira. Con 0 queda
   exactamente la rampa continua de antes. */
float aplicarPlanos(float *mapa, int n, const Planos &p, float dureza, int muestras) {
  if (!mapa || !p.valido || dureza <= 0.0f || p.frente <= p.fondo || n <= 0) return 0.0f;
  float margen = (p.frente - p.fondo) * 0.33f;
  if (margen > 0.05f) margen = 0.05f;
  const float a = p.fondo + margen, b = p.frente - margen;
  const float medio = 0.5f * (p.fondo + p.frente);
  /* niveles = las dos laminas mas las muestras intermedias (3 con una muestra, 5 con tres) */
  const int niveles = muestras >= 1 ? (muestras > 3 ? 5 : muestras + 2) : 0;
  const float ancho = 0.18f;   /* medio ancho del cruce entre muestras, en unidades de salto */
  const float escala = (float)(niveles - 1);
  int delante = 0;
#ifdef _OPENMP
#pragma omp parallel for schedule(static) reduction(+:delante)
#endif
  for (int i = 0; i < n; i++) {
    float t;
    if (b <= a) {
      t = mapa[i] > medio ? 1.0f : 0.0f;   /* laminas muy pegadas: no hay franja donde cruzar */
    } else {
      t = (mapa[i] - a) / (b - a);
      t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
      t = t * t * (3.0f - 2.0f * t);       /* el suave de siempre, sin saltos en el borde */
    }
    if (niveles >= 3) {
      /* del suave continuo al escalon: en que tramo estoy y cuanto llevo recorrido */
      const float q = t * escala;
      float j = std::floor(q);
      if (j > escala - 1.0f) j = escala - 1.0f;   /* el ultimo nivel se alcanza con t = 1 */
      const float f = q - j;
      float s = (f - 0.5f) / ancho + 0.5f;
      s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
      s = s * s * (3.0f - 2.0f * s);
      t = (j + s) / escala;
    }
    if (t > 0.5f) delante++;
    const float lamina = p.fondo + (p.frente - p.fondo) * t;
    mapa[i] += (lamina - mapa[i]) * dureza;
  }
  return (float)delante / (float)n;
}


#ifdef HAVE_ORT
namespace {
struct OrtImpl {
  Ort::Env env;
  Ort::SessionOptions so{};
  Ort::Session *ses = nullptr;
  Ort::AllocatorWithDefaultOptions alloc;
  std::string inN, outN;   /* nombres de la entrada y la salida, copiados: viven lo que nosotros */
  std::vector<float> in;
  OrtImpl() : env(ORT_LOGGING_LEVEL_ERROR, "odysseyvk") {}
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

/* Cuanta memoria de sistema queda disponible (MemAvailable de /proc/meminfo). Con la CPU por
   delante la red se queda con unos cientos de megabytes; si la maestra esta casi llena la
   creacion de la sesion revienta dentro de ONNX Runtime y no se puede capturar con try/catch. */
static long ramDisponibleMiB() {
  std::ifstream in("/proc/meminfo");
  std::string k;
  long mib = -1;
  while (in >> k) {
    if (k == "MemAvailable:") {
      in >> mib;
      break;
    }
    std::getline(in, k);
  }
  return mib;
}

bool DepthNet::init(const std::string &model, std::string *err, int gpu) {
  if (quemada_) {
    if (err)
      *err = "ONNX Runtime ya fallo al abrir una sesion en este proceso: no se vuelve a intentar "
             "(abortaria por dentro y el visor se iria arriba)";
    return false;
  }
  const std::string path = model.empty() ? defaultModelPath() : model;
  gpu_ = gpu < 0 ? 0 : gpu;
  if (const char *lim = entornoOdyssey("ODYSSEYVK_LIMPIEZA"))   /* 0 deja el mapa en crudo: para comparar */
    limpieza_ = lim[0] != '0';
  struct stat stt {};
  if (stat(path.c_str(), &stt) != 0) {
    if (err)
      *err = "no esta el modelo de profundidad en " + path +
             "\n    bajarlo con:  mkdir -p ~/.local/share/odysseyvk/modelos && curl -fLo \"" + path +
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
  /* Los umbrales de memoria (kSamsungMiB, kNativaMiB, kColchonMiB) estan arriba, con requisitoMiB().
     Colchon: nvidia-smi dice "libre", pero entre que miramos y abre la sesion el navegador pide un
     poco mas y la sesion CUDA reclama su contexto antes que la red. Sin colchon el "si cabe" resulta
     mentira: medido el 23-nov, con 1,2 GB anunciados como libres la creacion de la sesion abortó
     dentro de ONNX Runtime y el visor se fue arriba dejando el monitor en modo 3D sin video. */
  bool sinGpu = false;
  if (const char *g = entornoOdyssey("ODYSSEYVK_GPU"))   /* -1 fuerza la CPU: para probar ese camino */
    gpu_ = std::atoi(g);
  if (gpu_ < 0)
    sinGpu = true;
  {
    const std::vector<long> libres = vramLibreMiB();
    if (!sinGpu && !libres.empty()) {
      long mia = gpu_ < (int)libres.size() ? libres[(size_t)gpu_] : -1;
      int mejor = gpu_;
      long mejorMiB = mia;
      for (size_t i = 0; i < libres.size(); i++)
        if (libres[i] > mejorMiB) { mejorMiB = libres[i]; mejor = (int)i; }
      const long falta = requisitoMiB(path);
      if (mia >= 0 && mia < falta) {
        if (mejor != gpu_ && mejorMiB >= falta) {
          std::printf("[conversor] la grafica %d tiene solo %ld MB libres: la red se va a la grafica %d "
                      "(%ld MB libres)\n", gpu_, mia, mejor, mejorMiB);
          std::fflush(stdout);
          gpu_ = mejor;
        } else {
          std::printf("[conversor] no hay %ld MB libres en ninguna grafica (esta red, con el colchon, "
                      "pide %ld): la red se hara con la CPU, mas despacio\n", mejorMiB, falta);
          std::fflush(stdout);
          sinGpu = true;
        }
      }
    }
  }
  /* La red de Samsung no va por CPU ni por asomo: se cuelga creando la sesion (medido el 24-nov con
     mde_large.onnx: mas de 4 minutos quemando CPU sin acabarla) y acaba en abort, y el visor se iba
     arriba dejando el panel en modo 3D sin video. No se intenta: se dice lo que hace falta. */
  if (sinGpu && esRedDeSamsung(path)) {
    if (err)
      *err = std::string("la red de Samsung no puede abrirse por CPU: se cuelga creando la sesion ") +
             "(medido: mas de 4 minutos sin acabarla) y acaba abortando, asi que no se intenta. "
             "Hacen falta " +
             std::to_string(requisitoMiB(path)) +
             " MB libres en una grafica; suéltalos cerrando lo que ocupe la grafica, o usa la red "
             "nativa con el boton 2 (esa si va por CPU, a unos 190 ms por mapa).";
    return false;
  }
  OrtImpl *im = new OrtImpl();
  im->so.SetIntraOpNumThreads(4);
  im->so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  /* Poco margen: con el navegador y el compositor por delante la tarjeta suele estar casi llena.
     Con el arenal de memoria apagada el pico baja bastante (el modelo pequeno no necesita 1 GB
     reservados de golpe), y aun asi se le deja un techo por si acaso. */
  if (sinGpu) {
    /* Ya decidimos que en la grafica no hay sitio: no hay que intentar CUDA por la puerta vieja,
       que era lo que pasaba (el aviso decia "se hara con la CPU" y despues se abria CUDA igual, y
       ahi abortaba ONNX Runtime). */
    dev_ = "CPU";
  } else {
    try {
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
  }
  if (dev_ == "CPU") {
    const long mib = ramDisponibleMiB();
    if (mib >= 0 && mib < 1500) {
      std::printf("[conversor] la memoria de sistema esta justa (%ld MB disponibles): el conversor se "
                  "queda apagado; cerrar el navegador o el juego libera sitio\n", mib);
      std::fflush(stdout);
      if (err) *err = "no hay memoria de sistema libre para abrir la red por CPU";
      quemada_ = true;
      delete im;
      return false;
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
    const std::string donde = dev_;
    dev_ = "nada";
    delete im;
    /* Aqui se corta el intento: no se abre otra sesion en este proceso. Medido el 23-nov con las dos
       tarjetas casi llenas: al fallar la sesion CUDA el reintento abortaba dentro de
       libonnxruntime (la pila entera dentro de la libreria), el visor moria sin tiempo de devolver
       el panel y el monitor se quedaba con la rejilla 3D sobre el escritorio. Sin reintento el
       video se ve normal, sin relieve, y el panel se devuelve bien. */
    if (donde == "NVIDIA") quemada_ = true;   /* con CUDA tocado, otra sesion en este proceso revienta */
    if (err)
      *err = (donde == "CPU")
                 ? "el modelo no abrio en la CPU: " + fallo
                 : ("el modelo no abrio en la grafica " + std::to_string(gpu_) + " (" + fallo +
                    "). No se reintenta: despues de un intento CUDA fallido ONNX Runtime aborta por "
                    "dentro y el visor se quedaba sin tiempo de devolver el panel. El video se ve sin "
                    "relieve; suelta memoria de la grafica y en la siguiente sesion vuelve a convertir");
    return false;
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
  /* Aviso aparte para la red de Samsung: si ahora mismo no cabe en ninguna grafica, soltar la sesion
     en curso no siempre le deja sitio (el contexto de CUDA no se devuelve del todo), y el intento por
     CPU es lo que hunde el visor. Se intenta igual, porque a lo mejor si cabe, pero se avisa. */
  if (esRedDeSamsung(nueva)) {
    const std::vector<long> libres = vramLibreMiB();
    long mejor = -1;
    for (long m : libres)
      if (m > mejor) mejor = m;
    const long falta = requisitoMiB(nueva);
    if (mejor >= 0 && mejor < falta)
      std::printf("[conversor] la grafica esta justa (%ld MB libres en la mejor, esta red pide %ld): si "
                  "no cabe, la red de Samsung NO se abrira por CPU\n", mejor, falta);
  }
  /* Se suelta la sesion vieja ANTES de abrir la nueva: es lo que devuelve la memoria de la
     grafica, y sin eso la segunda red no cabe y se va a la CPU. */
  cierra();
  prev_.clear();
  haveScale_ = false;
  focoListo_ = false;              /* y el plano de apoyo de la otra red tampoco vale */
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

/* ---------------- saliencia al estilo del Hub de Windows (v0.65) ----------------
   Leida del ensamblador NV de su cache (glfx/binary/depth.salc_dspr.bin y depth.salc_merge.bin):
     contraste centro-entorno de un campo F = media de |F_s - F_c| en los seis pares de niveles de mip
       (c, s) = (3,6) (4,6) (4,7) (5,7) (5,8) (6,8)            [salc.level.center 3 / surround 6]
     saliencia = (0,2 D + 0,6 contraste(D) + 0,1 bordes + 0,1 contraste(Lab)) * smootherstep(1 - r/rmax)
   con r la distancia al centro y rmax la media diagonal. Lo que mas pesa (0,6) es lo que se despega en
   PROFUNDIDAD de lo que lo rodea: el sujeto, no lo que tiene mas bordes. El Lab se aproxima aqui con
   luminancia y dos canales oponentes. */
namespace {

struct Piramide {
  std::vector<std::vector<float>> nivel;
  std::vector<int> tam;
};

/* medias 2x2 hasta `niveles` (nivel 0 = n x n) */
void hacerPiramide(const float *src, int n, int niveles, Piramide &p) {
  p.nivel.assign(1, std::vector<float>(src, src + (size_t)n * n));
  p.tam.assign(1, n);
  for (int l = 1; l < niveles; l++) {
    const int a = p.tam.back(), b = a > 1 ? (a + 1) / 2 : 1;
    const std::vector<float> &q = p.nivel.back();
    std::vector<float> r((size_t)b * b);
    for (int y = 0; y < b; y++)
      for (int x = 0; x < b; x++) {
        const int x0 = std::min(2 * x, a - 1), x1 = std::min(2 * x + 1, a - 1);
        const int y0 = std::min(2 * y, a - 1), y1 = std::min(2 * y + 1, a - 1);
        r[(size_t)y * b + x] = 0.25f * (q[(size_t)y0 * a + x0] + q[(size_t)y0 * a + x1] +
                                        q[(size_t)y1 * a + x0] + q[(size_t)y1 * a + x1]);
      }
    p.nivel.push_back(std::move(r));
    p.tam.push_back(b);
  }
}

/* el nivel l muestreado (bilineal) en la posicion (u, v) en 0..1 */
float enNivel(const Piramide &p, int l, float u, float v) {
  const int t = p.tam[(size_t)l];
  const std::vector<float> &q = p.nivel[(size_t)l];
  float fx = u * (float)t - 0.5f, fy = v * (float)t - 0.5f;
  fx = fx < 0.0f ? 0.0f : (fx > (float)(t - 1) ? (float)(t - 1) : fx);
  fy = fy < 0.0f ? 0.0f : (fy > (float)(t - 1) ? (float)(t - 1) : fy);
  const int x0 = (int)fx, y0 = (int)fy, x1 = std::min(x0 + 1, t - 1), y1 = std::min(y0 + 1, t - 1);
  const float ax = fx - (float)x0, ay = fy - (float)y0;
  return (q[(size_t)y0 * t + x0] * (1.0f - ax) + q[(size_t)y0 * t + x1] * ax) * (1.0f - ay) +
         (q[(size_t)y1 * t + x0] * (1.0f - ax) + q[(size_t)y1 * t + x1] * ax) * ay;
}

float centroEntorno(const Piramide &p, float u, float v) {
  static const int pares[6][2] = {{3, 6}, {4, 6}, {4, 7}, {5, 7}, {5, 8}, {6, 8}};
  float s = 0.0f;
  for (const auto &cs : pares) s += std::fabs(enNivel(p, cs[1], u, v) - enNivel(p, cs[0], u, v));
  return s / 6.0f;
}

/* percentil 99 de una lista (para llevar cada senal a 0..1 sin que un punto raro mande) */
float p99(std::vector<float> v) {
  if (v.empty()) return 1.0f;
  const size_t k = (size_t)((double)(v.size() - 1) * 0.99);
  std::nth_element(v.begin(), v.begin() + (long)k, v.end());
  return v[k] > 1e-6f ? v[k] : 1e-6f;
}

}  // namespace

bool DepthNet::estimate(const uint8_t *rgba, int w, int h, std::vector<uint8_t> &out, std::string *err) {
#ifdef HAVE_ORT
  if (!ready_ || !rgba || w < 2 || h < 2) return false;
  OrtImpl *im = (OrtImpl *)impl_;
  /* Las pasadas del filtro van en cuatro hebras: a 518 x 518 baja de 4,0 ms a 1,7 ms y no hace
     falta mas (con las treinta y dos hebras de la maquina el gasto de repartir se come la ganancia).
     Se puede cambiar con OMP_NUM_THREADS sin recompilar. */
#ifdef _OPENMP
  static const bool hebrasPuestas = [] {
    if (!std::getenv("OMP_NUM_THREADS")) omp_set_num_threads(4);
    return true;
  }();
  (void)hebrasPuestas;
#endif
  const auto t0 = std::chrono::steady_clock::now();
  const size_t plane = (size_t)sz_ * (size_t)sz_;
  sampleBilinear(rgba, w, h, im->in.data(), sz_);
  /* guia para limpiar el mapa despues: la luminancia de la foto, tomada antes de normalizarla
     para la red (es la que dice donde estan los cantos de los objetos) */
  guia_.resize(plane);
  opRG_.resize(plane);
  opBY_.resize(plane);
  for (size_t i = 0; i < plane; i++) {
    const float r = im->in[i], g = im->in[plane + i], b = im->in[2 * plane + i];
    guia_[i] = 0.299f * r + 0.587f * g + 0.114f * b;
    opRG_[i] = r - g;               /* canales oponentes: la parte de color del Lab de la saliencia */
    opBY_[i] = b - 0.5f * (r + g);
  }
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
  const auto tRed0 = std::chrono::steady_clock::now();
  try {
    outs = im->ses->Run(Ort::RunOptions{nullptr}, &inN, &tin, 1, &outN, 1);
  } catch (const std::exception &e) {
    if (err) *err = std::string("el modelo fallo: ") + resumenOrt(e.what());
    return false;
  }
  const auto tRed1 = std::chrono::steady_clock::now();
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

  /* Los tres seguimientos SUELTOS van por TIEMPO, no por estimacion (v0.69): la escala del mapa (0,25),
     el plano de apoyo y las posiciones de las dos laminas (0,08). Sus pesos se pusieron mirando el panel
     cuando salian ~6 estimaciones por segundo (el hilo que dibuja se paraba 75 ms en cada una, ver
     crearBufferDeLectura en vkweave.cpp). Arreglado eso salen 20 o mas, y con los mismos pesos por
     estimacion el plano y la escala correrian tres veces mas deprisa detras de cada cambio.
     `porTiempo(w)` da el peso equivalente para el tiempo que de verdad paso: con una estimacion cada
     1/6 s es w tal cual. Son numeros sueltos, sin el bucle del mapa de mas abajo. */
  double pasoEst = kPasoRef;
  {
    const double ahoraS =
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (pasoFijo_ > 0.0) pasoEst = pasoFijo_;
    else if (tUltima_ >= 0.0) pasoEst = ahoraS - tUltima_;
    tUltima_ = ahoraS;
    pasoEst = pasoEst < 0.004 ? 0.004 : (pasoEst > 0.5 ? 0.5 : pasoEst);
  }
  const float expoT = (float)(pasoEst / kPasoRef);
  auto porTiempo = [expoT](float w) { return 1.0f - std::pow(1.0f - w, expoT); };

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
    const float a = porTiempo(0.25f);   /* la escala nueva entra poco a poco: el relieve no baila */
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

  /* nueve huecos de trabajo: cinco medias del filtro guiado, dos productos (guia x guia y
     guia x prof) y una fila de paso para cada caja. Si se anade otra pasada, subir aqui. */
  std::vector<float> mapa(plane), limpio(plane), salida(plane), trabajo((size_t)9 * plane);
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
  /* OJO: este peso se queda POR ESTIMACION a proposito (v0.69). El mapa anterior que se mezcla aqui es
     la SALIDA ya procesada (filtro guiado, dilatado, relieve local, dos planos), y todo eso se vuelve a
     aplicar encima en cada vuelta: lo que se ve es el punto de equilibrio de ese bucle, y depende de
     este peso. Pasarlo a "por tiempo" cambia el equilibrio (medido: 10 niveles de media de diferencia a
     18 estimaciones por segundo). Con el peso fijo, una imagen quieta da el mismo mapa a cualquier
     ritmo; lo unico que cambia con mas estimaciones es que el mapa llega antes a su sitio. */
  const float q = 1.0f - smooth_;
  if (suavizar) {
    for (size_t i = 0; i < plane; i++)
      limpio[i] = mapa[i] * q + ((float)prev_[i] / 255.0f) * smooth_;
  } else {
    limpio = mapa;
  }

  const auto tFiltro0 = std::chrono::steady_clock::now();
  /* limpiar el mapa siguiendo los bordes de la foto y dilatar lo cercano unos puntos: con esto los
     cantos de los objetos quedan en su sitio y el relieve deja de temblar cuadro a cuadro. Con
     ODYSSEYVK_LIMPIEZA=0 se apaga, para comparar en el panel la misma red con el mapa en crudo. */
  if (limpieza_) {
    filtroGuiado(guia_.data(), limpio.data(), salida.data(), sz_, 8, 0.04f, trabajo.data());
    dilatar(salida.data(), mapa.data(), sz_, 3);
  } else {
    salida = limpio;
  }

  /* Relieve local (v0.66), contra el «efecto carton»: las redes dan mapas suaves y una cara o un animal
     salen como un recorte plano a cierta distancia. Se separa el mapa en una BASE (la forma gruesa de
     cada objeto) y el DETALLE (lo que varia dentro del objeto), y se le suma una parte del detalle:
     D' = D + alfa (D - base). La base sale del filtro guiado del mapa CONSIGO MISMO (radio 48,
     epsilon 0,002): los escalones grandes del mapa (mas de ~0,05, los cantos entre objetos) se quedan
     en la base, asi que no se inflan; con un desenfoque normal se haria un foso alrededor de cada
     objeto. Es la tecnica de «depth unsharp masking» (Luft, Colditz y Deussen, 2006) con el filtro que
     respeta cantos. ODYSSEYVK_RELIEVE_LOCAL (o `relieve_local` en serve.conf) cambia alfa; 0 lo apaga.
     MEDIDO (22 cuadros sueltos, alfa 1, radio 48): la pendiente dentro de los objetos sube un 11 %, el
     salto en los cantos un 6 % y un 2,5 % de puntos se pega a 0 o 1. Es poco: estas redes dan mapas
     lisos por dentro y no se puede realzar un relieve que no traen. Con alfa 2: 19 % / 12 % / 4,5 %. */
  static const float kRelieveLocal = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_RELIEVE_LOCAL");
    std::string s = e ? e : "";
    for (char &c : s)
      if (c == ',') c = '.';
    const float v = s.empty() ? 1.0f : (float)std::atof(s.c_str());
    return (v >= 0.0f && v <= 3.0f) ? v : 1.0f;
  }();
  /* radio y epsilon de la base, para el taller (ODYSSEYVK_RELIEVE_LOCAL_RADIO / _EPS) */
  static const int kRlRadio = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_RELIEVE_LOCAL_RADIO");
    const int v = e ? std::atoi(e) : 48;
    return (v >= 2 && v <= 128) ? v : 48;
  }();
  static const float kRlEps = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_RELIEVE_LOCAL_EPS");
    const float v = e ? (float)std::atof(e) : 0.002f;
    return (v > 0.0f && v < 1.0f) ? v : 0.002f;
  }();
  if (kRelieveLocal > 0.0f) {
    filtroGuiado(salida.data(), salida.data(), limpio.data(), sz_, kRlRadio, kRlEps, trabajo.data());
    for (size_t i = 0; i < plane; i++) {
      const float v = salida[i] + kRelieveLocal * (salida[i] - limpio[i]);
      salida[i] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
  }

  const auto tPlano0 = std::chrono::steady_clock::now();
  /* plano de apoyo: el pico del histograma ponderado (mas peso en el centro y en los cantos, que es
     donde suele estar el sujeto). Con el apoyo ahi el sujeto se queda en el plano de la pantalla y
     el relieve se abre detras, que es como menos cansa la vista. De suelo se usa la mediana, para
     que nunca quede mas de media imagen por delante de la pantalla: ahi es donde molesta a los ojos. */
  {
    float hist[32] = {0.0f};
    float total = 0.0f;
    /* La saliencia del Hub elige el sujeto. PUESTA por defecto desde v0.66: Rodolfo comparo en el
       panel `saliencia = no` contra `si` (25-nov) y se quedo con `si`. ODYSSEYVK_SALIENCIA=0 (o
       `saliencia = no` en serve.conf) vuelve al peso de antes (bordes y un escalon al centro). */
    static const bool kSaliencia = [] {
      const char *e = entornoOdyssey("ODYSSEYVK_SALIENCIA");
      return !e || !(e[0] == '0' || e[0] == 'n');
    }();
    if (kSaliencia) {
      /* rejilla de cada 8 puntos del mapa (64 x 64): el nivel 3 de la piramide ya mide 65, asi que mas
         fino no aporta, y con cada 4 la saliencia costaba ~6 ms por estimacion */
      Piramide pD, pL, pA, pB;
      hacerPiramide(salida.data(), sz_, 9, pD);
      hacerPiramide(guia_.data(), sz_, 9, pL);
      hacerPiramide(opRG_.data(), sz_, 9, pA);
      hacerPiramide(opBY_.data(), sz_, 9, pB);
      const int paso = 8, g = (sz_ - 2) / paso + 1;
      std::vector<float> vD, vCS, vE, vLab, vW;
      vD.reserve((size_t)g * g); vCS.reserve((size_t)g * g); vE.reserve((size_t)g * g);
      vLab.reserve((size_t)g * g); vW.reserve((size_t)g * g);
      std::vector<int> vBin;
      vBin.reserve((size_t)g * g);
      const float cx = 0.5f * (float)sz_, cy = 0.5f * (float)sz_, rmax = std::sqrt(cx * cx + cy * cy);
      for (int y = 1; y < sz_ - 1; y += paso)
        for (int x = 1; x < sz_ - 1; x += paso) {
          const size_t o = (size_t)y * (size_t)sz_ + (size_t)x;
          const float u = ((float)x + 0.5f) / (float)sz_, v = ((float)y + 0.5f) / (float)sz_;
          vD.push_back(salida[o]);
          vCS.push_back(centroEntorno(pD, u, v));
          vE.push_back(std::fabs(guia_[o + 1] - guia_[o - 1]) + std::fabs(guia_[o + sz_] - guia_[o - sz_]));
          vLab.push_back(centroEntorno(pL, u, v) + centroEntorno(pA, u, v) + centroEntorno(pB, u, v));
          float t = 1.0f - std::sqrt(((float)x - cx) * ((float)x - cx) + ((float)y - cy) * ((float)y - cy)) / rmax;
          t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
          vW.push_back(t * t * t * (t * (6.0f * t - 15.0f) + 10.0f));   /* smootherstep, como el Hub */
          vBin.push_back((int)(salida[o] * 31.99f));
        }
      const float nCS = p99(vCS), nE = p99(vE), nLab = p99(vLab);
      for (size_t i = 0; i < vD.size(); i++) {
        const float s = (0.2f * vD[i] + 0.6f * std::min(vCS[i] / nCS, 1.0f) + 0.1f * std::min(vE[i] / nE, 1.0f) +
                         0.1f * std::min(vLab[i] / nLab, 1.0f)) * vW[i];
        const float wgt = 0.02f + s;   /* un piso: que el histograma nunca quede vacio */
        hist[vBin[i]] += wgt;
        total += wgt;
      }
    } else {
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
    /* 0,06 por estimacion (el focus.iir.weight del Hub es 0,02: mas lento). ODYSSEYVK_FOCO_IIR o
       `foco_iir` en serve.conf lo cambian (v0.65). */
    static const float kFocoIir = [] {
      const char *e = entornoOdyssey("ODYSSEYVK_FOCO_IIR");
      std::string s = e ? e : "";
      for (char &c : s)
        if (c == ',') c = '.';
      const float v = s.empty() ? 0.06f : (float)std::atof(s.c_str());
      return (v > 0.0f && v <= 1.0f) ? v : 0.06f;
    }();
    /* la primera estimacion salta directa al plano (v0.66): antes salia de 0,5 y tardaba uno o dos
       segundos en llegar, y en una foto suelta el apoyo que se informaba no era el de la foto */
    /* Pivote (v0.66): con el plano de la pantalla EN el sujeto, casi todo queda detras y solo se usa
       el presupuesto de la lente hacia dentro (9,98 px por ojo); el de hacia fuera (12,67 px) queda
       vacio. Con pivote p el plano se retrasa p de camino hacia el fondo: el sujeto sale y el fondo se
       hunde, y se usan los dos lados. ODYSSEYVK_PIVOTE / `pivote` en serve.conf; 0 = como antes. */
    static const float kPivote = [] {
      const char *e = entornoOdyssey("ODYSSEYVK_PIVOTE");
      std::string s = e ? e : "";
      for (char &c : s)
        if (c == ',') c = '.';
      const float v = s.empty() ? 0.0f : (float)std::atof(s.c_str());
      return (v >= 0.0f && v <= 0.9f) ? v : 0.0f;
    }();
    pivote_ = kPivote;
    const bool salto = corte_ || !focoListo_;
    focoListo_ = true;
    if (salto || std::fabs(nuevo - focus_) >= 0.01f)
      focus_ += (nuevo - focus_) * (salto ? 1.0f : porTiempo(kFocoIir));
  }

  /* Los dos planos. Con dureza 0 (lo de siempre, y lo que arranca el programa) este bloque no se
     ejecuta ni una vez. Cuando esta subido se buscan los dos grupos en el mapa ya limpio, se siguen
     sus posiciones en el tiempo (0,08 por cuadro, de golpe en cada cambio de plano) y luego se acerca
     cada punto a su lamina. Seguir las posiciones y no tomar las de cada cuadro es lo que evita que
     la costura baile con el ruido de la red; el mapa de salida se cuantiza aqui, antes de subirlo,
     asi que el sombreador y sus topes por ojo (+12,67 hacia fuera / -9,98 hacia dentro) no cambian. */
  if (dureza_ > 0.0f) {
    static char bufPl[96];
    char bufMd[40];
    const Planos visto = buscarPlanos(salida.data(), sz_ * sz_);
    if (visto.valido) {
      if (pFondo_ < 0.0f || corte_) {
        pFondo_ = visto.fondo;
        pFrente_ = visto.frente;
      } else if (std::fabs(visto.fondo - pFondo_) >= 0.004f || std::fabs(visto.frente - pFrente_) >= 0.004f) {
        pFondo_ += (visto.fondo - pFondo_) * porTiempo(0.08f);
        pFrente_ += (visto.frente - pFrente_) * porTiempo(0.08f);
      }
      if (pFrente_ - pFondo_ < 0.10f) {
        pFondo_ = visto.fondo;      /* se pegaron: tomar tal cual antes de aplicar */
        pFrente_ = visto.frente;
      }
      Planos seguidas;
      seguidas.valido = true;
      seguidas.fondo = pFondo_;
      seguidas.frente = pFrente_;
      fracFrente_ = aplicarPlanos(salida.data(), sz_ * sz_, seguidas, dureza_, muestras_);
    } else {
      pFondo_ = -1.0f;
      pFrente_ = -1.0f;
      fracFrente_ = 0.0f;
    }
    static const bool medirPl = entornoOdyssey("ODYSSEYVK_MEDIR") != nullptr;
    static long npl = 0;
    if (medirPl && ++npl % 30 == 1) {
      if (muestras_ >= 1)
        std::snprintf(bufMd, sizeof bufMd, " · %d muestra%s intermedia%s", muestras_,
                      muestras_ > 1 ? "s" : "", muestras_ > 1 ? "s" : "");
      else
        bufMd[0] = '\0';
      std::printf("[planos] dureza %.2f%s · %s\n", dureza_, bufMd,
                  visto.valido ? (std::snprintf(bufPl, sizeof bufPl, "atras %.2f · delante %.2f · "
                                                               "%.1f%% del cuadro por delante",
                                                pFondo_, pFrente_, 100.0f * fracFrente_), bufPl)
                               : "un solo plano claro (no se toca nada)");
    }
  }

  const auto tPlano1 = std::chrono::steady_clock::now();
  out.resize(plane);
  for (size_t i = 0; i < plane; i++) {
    const float v = salida[i] < 0.0f ? 0.0f : (salida[i] > 1.0f ? 1.0f : salida[i]);
    out[i] = (uint8_t)(v * 255.0f + 0.5f);
  }
  prev_ = out;
  const auto tfin = std::chrono::steady_clock::now();
  ms_ = std::chrono::duration<double, std::milli>(tfin - t0).count();
  msRed_ = std::chrono::duration<double, std::milli>(tRed1 - tRed0).count();
  msPost_ = std::chrono::duration<double, std::milli>(tfin - tRed1).count();
  msIir_ = std::chrono::duration<double, std::milli>(tFiltro0 - tRed1).count();
  msFilt_ = std::chrono::duration<double, std::milli>(tPlano0 - tFiltro0).count();
  msPlano_ = std::chrono::duration<double, std::milli>(tPlano1 - tPlano0).count();
  msSubida_ = std::chrono::duration<double, std::milli>(tfin - tPlano1).count();
  /* ODYSSEYVK_MEDIR=1 enseña el reparto del cuadro sin sentarse delante del panel: hace falta
     para decidir si conviene bajar el lienzo de disparidad (ellos trabajan el mapa a 384x160) o si
     lo que manda es la red y el resto da igual. */
  static const bool medir = entornoOdyssey("ODYSSEYVK_MEDIR") != nullptr;
  static long nmed = 0;
  if (medir && ++nmed % 30 == 1)
    std::printf("[medir] mapa %d x %d por %s · red %.1f · suavizado %.1f · limpiar y dilatar %.1f · "
                "plano de apoyo %.1f · subir %.1f ms · total %.1f ms\n",
                sz_, sz_, dev_.c_str(), msRed_, msIir_, msFilt_, msPlano_, msSubida_, ms_);
  return true;
#else
  (void)rgba, (void)w, (void)h, (void)out;
  if (err) *err = "sin ONNX Runtime";
  return false;
#endif
}

/* ---------------- rotulos y subtitulos: apoyar el texto en la pantalla ----------------
   Hace falta porque la red de profundidad no sabe que un subtitulo es un dibujo pegado a la
   pantalla: ve un parche de texto de mucho contraste y le da la profundidad del fondo (o lo saca
   hacia fuera), y ahi aparece el deslizamiento. La palanca es geometria pura: un punto con
   desplazamiento de 12 px por ojo queda a ~2 cm del panel y barre ~30 px en la pantalla cuando la
   cabeza se mueve 15 cm. Poniendo esas zonas en el plano de apoyo el desplazamiento sale cero, el
   texto se queda donde esta y ademas se ve mejor, porque deja de estar desparejado entre los ojos.

   Los umbrales van en unidades del mapa (518 x 518), no de la pantalla: a esa escala una linea de
   subtitulo mide de 6 a 14 puntos y el trazo 2 o 3. ODYSSEYVK_TEXTO_CONTRASTE y
   ODYSSEYVK_TEXTO_FINO sirven para comparar en el panel sin recompilar. */
float anclarTexto(const uint8_t *rgba, int w, int h, std::vector<uint8_t> &mapa, float plano,
                  int celdaPx, std::vector<uint8_t> *mascara, int *cwOut, int *chOut,
                  MemoriaTexto *memoria) {
  if (!rgba || w < 32 || h < 32 || mapa.size() != (size_t)w * (size_t)h)
    return 0.0f;
  static const float kContraste = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_TEXTO_CONTRASTE");
    return e ? (float)std::atof(e) : 55.0f;
  }();
  static const float kFino = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_TEXTO_FINO");
    return e ? (float)std::atof(e) : 0.26f;
  }();
  const float kDensidad = 0.13f;      // parte de la celda que son bordes
  const float kDensidadMax = 0.65f;   // ...pero sin llenar la celda: eso es textura, no letras
  const float kLlano = 0.25f;         // y alrededor tiene que quedar fondo liso (lo que no tiene
                                      // una tela ni un matorral: ahi no hay un solo hueco despejado)
  const float kTrazoMin = 0.10f;      // el trazo tiene que ser fino...
  const float kTrazoMax = 0.70f;      // ...y dejar sitio al fondo
  const int celda = celdaPx < 4 ? 4 : (celdaPx > 16 ? 16 : celdaPx);
  const int cw = (w + celda - 1) / celda, ch = (h + celda - 1) / celda;
  if (cwOut) *cwOut = cw;
  if (chOut) *chOut = ch;

  /* gradiente y segundo orden de toda la foto: el texto es lo unico que tiene bordes fuertes y a la
     vez alternancia rapida en las dos direcciones */
  std::vector<float> lum((size_t)w * h), gx((size_t)w * h), gy((size_t)w * h), lp((size_t)w * h);
  for (int y = 0; y < h; y++) {
    const uint8_t *p = rgba + ((size_t)y * w) * 4;
    for (int x = 0; x < w; x++)
      lum[(size_t)y * w + x] = 0.299f * p[x * 4] + 0.587f * p[x * 4 + 1] + 0.114f * p[x * 4 + 2];
  }
  for (int y = 0; y < h; y++) {
    const int yu = y > 0 ? y - 1 : 0, yd = y + 1 < h ? y + 1 : h - 1;
    for (int x = 0; x < w; x++) {
      const int xl = x > 0 ? x - 1 : 0, xr = x + 1 < w ? x + 1 : w - 1;
      const size_t i = (size_t)y * w + x;
      const float c = lum[i], d = lum[(size_t)y * w + xr], e = lum[(size_t)y * w + xl];
      const float u = lum[(size_t)yu * w + x], b = lum[(size_t)yd * w + x];
      gx[i] = d - e;
      gy[i] = b - u;
      lp[i] = std::fabs(4.0f * c - d - e - u - b);
    }
  }

  /* Medidas de cada celda. Todavia no se decide nada: la decision mira tambien la vecindad, que es
     donde se ve si el detalle esta suelto sobre fondo liso (un rotulo) o si llena todo (una tela,
     un matorral, una grava). */
  const size_t celdas = (size_t)cw * ch;
  std::vector<float> cont(celdas, 0.0f), dens(celdas, 0.0f), llan(celdas, 0.0f), fina(celdas, 0.0f),
      traz(celdas, 0.0f);
  /* quieta: los trazos de la celda estan donde estaban en el mapa anterior (ver MemoriaTexto) */
  static const bool kConMemoria = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_TEXTO_MEMORIA");
    return !(e && e[0] == '0');
  }();
  if (!kConMemoria) memoria = nullptr;
  const bool hayAnterior = memoria && memoria->w == w && memoria->h == h && memoria->lum.size() == lum.size();
  const float *antes = hayAnterior ? memoria->lum.data() : nullptr;
  std::vector<uint8_t> quieta(celdas, 0);
  for (int cy = 0; cy < ch; cy++) {
    const int y0 = cy * celda, y1 = std::min(h, y0 + celda);
    for (int cx = 0; cx < cw; cx++) {
      const int x0 = cx * celda, x1 = std::min(w, x0 + celda);
      float sG = 0.0f, sL = 0.0f, mn = 1e9f, mx = -1e9f;
      int n = 0;
      for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
          const size_t i = (size_t)y * w + x;
          sG += std::fabs(gx[i]) + std::fabs(gy[i]);
          sL += lp[i];
          const float c = lum[i];
          if (c < mn) mn = c;
          if (c > mx) mx = c;
          n++;
        }
      if (n < 4)
        continue;
      const float contraste = mx - mn;
      /* Los umbrales de borde van referidos al contraste de la celda, pero con un piso y un techo:
         una celda lisa tiene que contar como lisa, y si no el vecino despejado de un rotulo no suma
         y el detector no lo reconoce. */
      const float umbral = std::max(24.0f, 0.32f * contraste);
      const float flojo = std::min(14.0f, std::max(2.0f, 0.12f * contraste));
      int densos = 0, claros = 0, llanos = 0;
      float movido = 0.0f;   /* cuanto cambiaron los puntos de trazo desde el mapa anterior */
      for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
          const size_t i = (size_t)y * w + x;
          const float g = std::fabs(gx[i]) + std::fabs(gy[i]);
          if (g > umbral) {
            densos++;
            if (antes) movido += std::fabs(lum[i] - antes[i]);
          }
          if (g < flojo)
            llanos++;
          if (lum[i] > 0.5f * (mn + mx))
            claros++;
        }
      const size_t q = (size_t)cy * cw + cx;
      /* Quieta = los trazos cambiaron menos del 12 % del contraste (con un piso de 6 niveles por la
         compresion del video). Una textura que se corre 0,3 puntos del mapa ya cambia el 20 %. */
      if (antes && densos >= 4 && movido / (float)densos <= std::max(6.0f, 0.12f * contraste)) quieta[q] = 1;
      cont[q] = contraste;
      dens[q] = (float)densos / (float)n;
      llan[q] = (float)llanos / (float)n;
      fina[q] = (sL / (float)n) / std::max(24.0f, contraste);
      traz[q] = (float)claros / (float)n;
    }
  }

  /* Media de la vecindad (3 x 3 celdas, unos 24 puntos del mapa). Un subitulo deja alrededor partes
     despejadas y un fondo texturado no: ahi es donde se separan, midiendo celda por celda no se
     pueden separar porque las dos tienen el mismo contraste y los mismos bordes. */
  std::vector<float> vDens(celdas, 0.0f), vLlan(celdas, 0.0f), vFina(celdas, 0.0f),
      vTraz(celdas, 0.0f);
  for (size_t q = 0; q < celdas; q++) {
    const int cy = (int)(q / cw), cx = (int)(q % cw);
    float sD = 0.0f, sL2 = 0.0f, sF = 0.0f, sT = 0.0f;
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        const int yy = cy + dy, xx = cx + dx;
        if (yy < 0 || yy >= ch || xx < 0 || xx >= cw)
          continue;
        const size_t r = (size_t)yy * cw + xx;
        sD += dens[r], sL2 += llan[r], sF += fina[r], sT += traz[r];
        n++;
      }
    vDens[q] = sD / (float)n, vLlan[q] = sL2 / (float)n, vFina[q] = sF / (float)n,
    vTraz[q] = sT / (float)n;
  }

  /* Y ahora si, la decision. En la banda de abajo (subtitulos) y en la de arriba (cintillos y
     relojes) se bajan los umbrales, que es donde vive casi todo el texto de un video. */
  std::vector<float> peso(celdas, 0.0f);
  for (size_t q = 0; q < celdas; q++) {
    const int cy = (int)(q / cw);
    const float centro = (cy + 0.5f) / (float)ch;
    const float banda = (centro > 0.76f || centro < 0.12f) ? 0.80f : 1.0f;
    if (cont[q] < kContraste * banda)
      continue;
    if (dens[q] < kDensidad * banda || dens[q] > kDensidadMax)
      continue;
    /* la finura del detalle se mira celda por celda (es la sena del trazo), y lo que se mide en la
       vecindad es si alrededor hay fondo despejado: en una banda de color solido la media de la
       vecindad de la finura cae y el rotulo se perderia */
    if (vLlan[q] < kLlano || fina[q] < kFino * banda || vTraz[q] < kTrazoMin || vTraz[q] > kTrazoMax)
      continue;
    peso[q] = 1.0f;
  }

  /* Forma (v0.60). Celda por celda, la cara (lentes, nariz, boca) y la rejilla de un microfono pasan
     todos los filtros de arriba: detalle fino y de mucho contraste con fondo liso al lado. Medido en un
     cuadro real (`texto-detect` sobre /tmp/rampa-chico.ppm, 25-nov): la cara salia marcada en una
     columna de 3-4 celdas de ancho por 18 de alto y el microfono en un bloque de ~9 x 16, y los dos se
     aplastaban contra la pantalla, con escalones de celda (8 puntos del mapa = 33 px de pantalla) en
     sus bordes. Un rotulo, en cambio, es una franja ancha y baja: aun con dos o tres lineas no pasa de
     ~10 celdas de alto (80 puntos del mapa, el 15 % del cuadro). Se queda la celda cuya racha
     horizontal (marcadas seguidas en su fila, perdonando un hueco) mide al menos kRachaMin y que,
     ademas, es el doble de ancha que su racha vertical o no pasa de kAltoMax de alto. (Solo con "el
     doble de ancha" se perdian los tramos cortos de un subtitulo de dos lineas: medido en
     pruebas-texto/t1.) ODYSSEYVK_TEXTO_RACHA cambia kRachaMin (0 = sin filtro de forma). */
  const int kAltoMax = 10;
  static const int kRachaMin = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_TEXTO_RACHA");
    return e ? std::atoi(e) : 4;   /* 4: barrido 3..6 con el cuadro real y pruebas-texto (ver CAMBIOS v0.60) */
  }();
  if (kRachaMin > 0) {
    auto marcada = [&](int x, int y) {
      return x >= 0 && x < cw && y >= 0 && y < ch && peso[(size_t)y * cw + x] > 0.0f;
    };
    /* largo de la racha que pasa por (x, y) en la direccion (dx, dy), perdonando huecos de una celda */
    auto racha = [&](int x, int y, int dx, int dy) {
      int n = 1;
      for (int s = -1; s <= 1; s += 2) {
        int cx = x, cy = y;
        for (;;) {
          const int nx = cx + s * dx, ny = cy + s * dy;
          if (marcada(nx, ny)) { cx = nx; cy = ny; n++; continue; }
          if (marcada(nx + s * dx, ny + s * dy)) { cx = nx + s * dx; cy = ny + s * dy; n += 2; continue; }
          break;
        }
      }
      return n;
    };
    std::vector<float> forma(celdas, 0.0f);
    for (int cy = 0; cy < ch; cy++)
      for (int cx = 0; cx < cw; cx++) {
        if (!marcada(cx, cy)) continue;
        const int h = racha(cx, cy, 1, 0), v = racha(cx, cy, 0, 1);
        if (h >= kRachaMin && (h >= 2 * v || v <= kAltoMax)) forma[(size_t)cy * cw + cx] = 1.0f;
      }
    peso.swap(forma);
  }

  /* Memoria (v0.74, ver MemoriaTexto). La celda se da por rotulo en este mapa si el detector la
     marca Y sus trazos estan quietos; con eso, un paso bajo (0,5 por mapa) con histeresis: entra
     al pasar de 0,7 (dos mapas seguidos), sale al bajar de 0,3 (dos seguidos sin verse), y en la
     banda de abajo entra al primero. El primer mapa de una sesion no apoya nada: no hay anterior
     con el que comparar. ODYSSEYVK_TEXTO_MEMORIA=0 vuelve a decidir cada mapa de cero. */
  if (memoria) {
    if (memoria->cw != cw || memoria->ch != ch) {
      memoria->cw = cw;
      memoria->ch = ch;
      memoria->confianza.assign(celdas, 0.0f);
      memoria->puesta.assign(celdas, 0);
    }
    /* la forma se decidio con todas las marcadas; lo quieto se mira en la celda y sus vecinas, que
       un glifo cae entre dos celdas y la racha perdona huecos */
    for (size_t q = 0; q < celdas; q++) {
      const int cy = (int)(q / cw), cx = (int)(q % cw);
      bool visto = false;
      if (peso[q] > 0.0f) {
        int quietas = 0, marcadas = 0;
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) {
            const int yy = cy + dy, xx = cx + dx;
            if (yy < 0 || yy >= ch || xx < 0 || xx >= cw) continue;
            const size_t r = (size_t)yy * cw + xx;
            if (peso[r] > 0.0f) {
              marcadas++;
              if (quieta[r]) quietas++;
            }
          }
        visto = quieta[q] && 2 * quietas > marcadas;
      }
      const float centro = ((float)cy + 0.5f) / (float)ch;
      float &c = memoria->confianza[q];
      uint8_t &puesta = memoria->puesta[q];
      /* subtitulo: al primer mapa quieto, pero solo en una celda que venia limpia (un renglon
         nuevo). La que acaba de soltarse y vuelve a marcarse es una dudosa: que haga cola. */
      if (visto && centro > 0.76f && !puesta && c < 0.1f) c = 1.0f;
      else c += 0.5f * ((visto ? 1.0f : 0.0f) - c);
      if (!puesta && c >= 0.7f) puesta = 1;
      else if (puesta && c <= 0.3f) puesta = 0;
    }
    for (size_t q = 0; q < celdas; q++) peso[q] = memoria->puesta[q] ? 1.0f : 0.0f;
    memoria->lum = lum;   /* para el mapa que viene */
    memoria->w = w;
    memoria->h = h;
  }

  /* dilatar una celda (un glifo suele cacer de una celda a otra) y despues difuminar, para que el
     apoyo entre despacio y no deje un canto duro en el relieve */
  std::vector<float> dil((size_t)cw * ch, 0.0f);
  for (int cy = 0; cy < ch; cy++)
    for (int cx = 0; cx < cw; cx++) {
      float m = 0.0f;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          const int yy = cy + dy, xx = cx + dx;
          if (yy < 0 || yy >= ch || xx < 0 || xx >= cw)
            continue;
          m = std::max(m, peso[(size_t)yy * cw + xx]);
        }
      dil[(size_t)cy * cw + cx] = m;
    }
  std::vector<float> suave((size_t)cw * ch, 0.0f);
  for (int cy = 0; cy < ch; cy++)
    for (int cx = 0; cx < cw; cx++) {
      float s = 0.0f;
      int n = 0;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          const int yy = cy + dy, xx = cx + dx;
          if (yy < 0 || yy >= ch || xx < 0 || xx >= cw)
            continue;
          s += dil[(size_t)yy * cw + xx];
          n++;
        }
      suave[(size_t)cy * cw + cx] = s / (float)n;
    }

  /* y el mapa, hacia el plano de apoyo con el peso de la mascara. El peso se interpola entre los
     centros de las celdas (v0.60): leido a celda entera dejaba escalones de 8 puntos del mapa en el
     relieve, que en pantalla son bloques de 33 px con el canto en sierra. */
  const float destino = plano * 255.0f;
  float tocadas = 0.0f;
  auto pesoEn = [&](int cx, int cy) {
    cx = cx < 0 ? 0 : (cx >= cw ? cw - 1 : cx);
    cy = cy < 0 ? 0 : (cy >= ch ? ch - 1 : cy);
    return suave[(size_t)cy * cw + cx];
  };
  for (int y = 0; y < h; y++) {
    const float fy = ((float)y + 0.5f) / (float)celda - 0.5f;
    const int cy0 = (int)std::floor(fy);
    const float ty = fy - (float)cy0;
    for (int x = 0; x < w; x++) {
      const float fx = ((float)x + 0.5f) / (float)celda - 0.5f;
      const int cx0 = (int)std::floor(fx);
      const float tx = fx - (float)cx0;
      const float p = (pesoEn(cx0, cy0) * (1.0f - tx) + pesoEn(cx0 + 1, cy0) * tx) * (1.0f - ty) +
                      (pesoEn(cx0, cy0 + 1) * (1.0f - tx) + pesoEn(cx0 + 1, cy0 + 1) * tx) * ty;
      if (p <= 0.002f)
        continue;
      const size_t i = (size_t)y * w + x;
      const float v = (float)mapa[i];
      mapa[i] = (uint8_t)(v + (destino - v) * p + 0.5f);
    }
  }
  for (float p : suave)
    if (p >= 0.5f)
      tocadas += 1.0f;
  if (mascara) {
    mascara->assign((size_t)cw * ch, 0);
    for (size_t i = 0; i < suave.size(); i++)
      (*mascara)[i] = (uint8_t)(suave[i] * 255.0f + 0.5f);
  }
  return tocadas / (float)((size_t)cw * ch);
}
