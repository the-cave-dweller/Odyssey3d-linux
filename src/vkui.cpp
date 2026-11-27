/* vkui.cpp — atlas (fuentes + máscaras), disposición de la interfaz y manejo del ratón. */
#include "vkui.h"

#include <thread>
#include "idioma.h"
#include "rendimiento.h"
#include "logo_mascara.inc"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#ifdef HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif
/* (v1.16) Barlow, dentro del programa (cmake/embed.cmake, desde assets/fuentes) */
#include "kBarlowNormal.h"
#include "kBarlowSemi.h"

namespace {
const char *kFontPaths[] = {
    "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/TTF/NotoSans-Regular.ttf",
    "/usr/share/fonts/TTF/LiberationSans-Regular.ttf", "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
};
constexpr int kFontPx = 26;   /* tamaño horneado del atlas; luego se escala el cuadro */
/* el tipo de los idiomas chino y japones (v1.07): DejaVu no trae esas letras. Es una coleccion
   (.ttc) con una cara por idioma; se escoge la que diga JP o SC en su nombre. */
const char *kCjkPaths[] = {
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/google-noto-sans-cjk-fonts/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.otf",
    "/usr/share/fonts/TTF/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
};

unsigned nextCp(const std::string &s, size_t &i) {
  unsigned char c = (unsigned char)s[i];
  if (c < 0x80) { i++; return c; }
  if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
    unsigned v = ((unsigned)(c & 0x1F) << 6) | (unsigned)(s[i + 1] & 0x3F);
    i += 2;
    return v;   /* si el atlas no lo tiene, abajo se dibuja como un espacio */
  }
  if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
    unsigned v = ((unsigned)(c & 0x0F) << 12) | ((unsigned)(s[i + 1] & 0x3F) << 6) | (unsigned)(s[i + 2] & 0x3F);
    i += 3;
    return v;   /* antes esto saltaba a "?" el euro, la raya y las comillas bonitas */
  }
  i++;
  return '?';
}

/* cobertura de un cuadrado redondeado: 1 dentro, 0 fuera, banda de 1 píxel */
uint8_t roundedCover(int px, int py, int size, float radius) {
  float c = (float)size * 0.5f;
  float inner = c - radius;
  float dx = std::fabs(px + 0.5f - c) - inner;
  float dy = std::fabs(py + 0.5f - c) - inner;
  float ox = std::max(dx, 0.0f), oy = std::max(dy, 0.0f);
  /* distancia con signo del rectangulo redondeado: dentro es negativo. Faltaba restar el
     radio, asi que la zona opaca quedaba encogida un radio por cada lado y la caja dibujada
     salia mas pequena que la caja de verdad: la letra de las opciones se veia fuera de su
     recuadro (y la pastilla y las tarjetas del visor tambien salian recortadas). */
  float dist = std::hypot(ox, oy) + std::min(std::max(dx, dy), 0.0f) - radius;
  float a = 0.5f - dist;
  if (a <= 0.0f) return 0;
  if (a >= 1.0f) return 255;
  return (uint8_t)(a * 255.0f);
}

/* de una distancia con signo (negativa dentro) a cobertura, con banda de 1 pixel */
uint8_t cubre(float dist) {
  float a = 0.5f - dist;
  if (a <= 0.0f) return 0;
  if (a >= 1.0f) return 255;
  return (uint8_t)(a * 255.0f);
}

/* (v1.16) un rotulo de seccion en MAYUSCULAS, en frase («TARJETAS GRÁFICAS» → «Tarjetas gráficas»), como los grupos de
   vkDaedalus. Quedan como estan las siglas y lo que lleva cifras (DLSS, HUD, 3D...); Steam y gamescope, con su nombre */
std::string enFrase(const std::string &t) {
  static const char *kSiglas[] = {"DLSS", "HUD", "FPS", "GPU", "VRR", "IA", "AI", "LUT", "SBS", "MAKO", "NVIDIA", "AMD", "CPU", "RAM"};
  static const std::pair<const char *, const char *> kNombres[] = {{"STEAM", "Steam"}, {"GAMESCOPE", "Gamescope"}, {"ODYSSEYVK", "OdysseyVK"}};
  std::string out;
  size_t i = 0;
  bool primera = true;
  char sep = 0;
  while (i < t.size()) {
    size_t j = t.find_first_of(" -", i);   /* el guion tambien separa: «3D-ПРОСМОТРЩИК» */
    if (j == std::string::npos) j = t.size();
    const char este = j < t.size() ? t[j] : 0;
    std::string p = t.substr(i, j - i);
    bool tal = std::any_of(p.begin(), p.end(), [](unsigned char c) { return std::isdigit(c); });
    for (const char *sg : kSiglas) tal = tal || p == sg;
    std::string nombre;
    for (const auto &nm : kNombres)
      if (p == nm.first) nombre = nm.second;
    if (!nombre.empty()) {
      p = nombre;
    } else if (!tal) {
      std::string q;
      bool inicio = true;
      for (size_t k = 0; k < p.size();) {
        const unsigned char c = (unsigned char)p[k];
        const bool deja = primera && inicio;
        if (c < 0x80) {
          q += deja ? (char)c : (char)std::tolower(c);
          k++;
        } else if (c == 0xC3 && k + 1 < p.size()) {   /* latino-1: À..Þ → à..þ (menos ×) */
          const unsigned char d = (unsigned char)p[k + 1];
          q += (char)c;
          q += (char)(!deja && d >= 0x80 && d <= 0x9E && d != 0x97 ? d + 0x20 : d);
          k += 2;
        } else if (c == 0xD0 && k + 1 < p.size() && !deja) {   /* cirilico: А..П → а..п, Р..Я → р..я, Ё → ё */
          const unsigned char d = (unsigned char)p[k + 1];
          if (d >= 0x90 && d <= 0x9F) q += (char)0xD0, q += (char)(d + 0x20);
          else if (d >= 0xA0 && d <= 0xAF) q += (char)0xD1, q += (char)(d - 0x20);
          else if (d == 0x81) q += (char)0xD1, q += (char)0x91;
          else q += (char)c, q += (char)d;
          k += 2;
        } else {
          const int largo = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
          q += p.substr(k, (size_t)largo);
          k += (size_t)largo;
        }
        inicio = false;
      }
      p = q;
    }
    if (sep) out += sep;
    out += p;
    sep = este;
    primera = false;
    i = j + 1;
  }
  return out;
}

float distSegmento(float px, float py, float ax, float ay, float bx, float by) {
  const float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
  float t = (wx * vx + wy * vy) / std::max(1e-6f, vx * vx + vy * vy);
  t = std::max(0.0f, std::min(1.0f, t));
  return std::hypot(px - (ax + t * vx), py - (ay + t * vy));
}
}  // namespace

bool Hud::buildAtlas(std::string *err) {
  /* con chino o japones entran ~300 letras mas de 26 px: el atlas crece al doble de alto */
  const int lengua = idioma::actual();
  /* (v1.16) el doble de alto: entra Barlow en sus dos pesos (MEDIDO: el latino-1 de DejaVu y las mascaras
     llenaban hasta 313 de 512; con ruso 340, con japones 582 de 1024) */
  aw_ = 1024;
  ah_ = idioma::esCjk(lengua) ? 1536 : 1024;
  extra_.clear();
  for (Glyph &g : glyphs_) g = Glyph{};
  for (auto &tabla : glyphsB_)
    for (Glyph &g : tabla) g = Glyph{};
  hayBarlow_ = false;
  hasText_ = false;
  atlas_.assign((size_t)aw_ * (size_t)ah_, 0);
  shelfX_ = 1;
  shelfY_ = 1;
  shelfH_ = 0;

  auto place = [&](int w, int h, int *x, int *y) -> bool {
    if (shelfX_ + w + 1 > aw_) {
      shelfX_ = 1;
      shelfY_ += shelfH_ + 1;
      shelfH_ = 0;
    }
    if (shelfY_ + h + 1 > ah_) return false;
    *x = shelfX_;
    *y = shelfY_;
    shelfX_ += w + 1;
    shelfH_ = std::max(shelfH_, h);
    return true;
  };
  auto put = [&](int x, int y, int w, int h, const uint8_t *src) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++) atlas_[(size_t)(y + j) * (size_t)aw_ + (size_t)(x + i)] = src[(size_t)j * (size_t)w + (size_t)i];
  };
  /* deja una mascara en el atlas y apunta su caja EN PIXELES del atlas (mask9 e icono los convierten) */
  auto subir = [&](int w, int h, const std::vector<uint8_t> &m, Rect *dst) -> bool {
    int x, y;
    if (!place(w, h, &x, &y)) return false;
    put(x, y, w, h, m.data());
    *dst = {(float)x, (float)y, (float)w, (float)h};
    return true;
  };

  /* 1) píxel blanco: cualquier relleno liso sale de aquí */
  {
    /* 4x4 y no 2x2: el relleno liso se muestrea ENTRE los dos pixeles del centro. Con el bloque de
       2x2 la muestra caia a caballo con el borde negro del atlas y cada relleno salia degradado
       (75 % arriba a la izquierda, 100 % abajo a la derecha): se veia en las cabecitas de los
       deslizadores y en el fondo de la pastilla. */
    const std::vector<uint8_t> w4(16, 255);
    if (!subir(4, 4, w4, &white_)) { if (err) *err = "atlas sin sitio para el píxel blanco"; return false; }
  }

  /* 2) máscaras. La de píldora (radio = mitad del lado) hace el interruptor, la bola y cualquier
     circulo; la de tarjeta trae un radio de 24 sobre 96 para que las esquinas de la tarjeta de la
     orilla salgan limpias aunque se estiren al 200 % (con la de 8 sobre 48 de antes se emborronaban). */
  bool cupo = true;
  for (int which = 0; which < 2; which++) {
    const int size = 96;
    const float radius = which == 0 ? 48.0f : 24.0f;
    std::vector<uint8_t> m((size_t)size * (size_t)size);
    for (int j = 0; j < size; j++)
      for (int i = 0; i < size; i++) m[(size_t)j * (size_t)size + (size_t)i] = roundedCover(i, j, size, radius);
    maskRad_[which] = radius;
    cupo = subir(size, size, m, which == 0 ? &pill_ : &card_) && cupo;
  }
  /* 2b) los dibujos de la tarjeta (v0.78). Con cuadros alineados no sale una diagonal limpia: el aspa
     de cerrar se hacia con ocho cuadritos en escalera. Ahora cada signo es una mascara con su borde
     suavizado: el aspa, la palomita de la casilla, el anillo del "?" y el `hueco` (todo lo que queda
     FUERA de un circulo: cada cuarto sirve para redondear una esquina de un dibujo ya pintado). */
  {
    const int n = 64;
    std::vector<uint8_t> cerrar((size_t)n * n), palomita((size_t)n * n), anillo((size_t)n * n), hueco((size_t)n * n),
        flecha((size_t)n * n);
    for (int j = 0; j < n; j++)
      for (int i = 0; i < n; i++) {
        const float x = (float)i + 0.5f, y = (float)j + 0.5f;
        const size_t p = (size_t)j * (size_t)n + (size_t)i;
        cerrar[p] = cubre(std::min(distSegmento(x, y, 17, 17, 47, 47), distSegmento(x, y, 17, 47, 47, 17)) - 3.4f);
        /* la flecha hacia arriba del boton de actualizar (v0.95): el palo y las dos alas */
        flecha[p] = cubre(std::min(distSegmento(x, y, 32, 14, 32, 50),
                                   std::min(distSegmento(x, y, 32, 14, 15, 31), distSegmento(x, y, 32, 14, 49, 31))) - 3.6f);
        palomita[p] = cubre(std::min(distSegmento(x, y, 13, 34, 26, 47), distSegmento(x, y, 26, 47, 51, 19)) - 4.4f);
        const float rc = std::hypot(x - 32.0f, y - 32.0f);
        anillo[p] = cubre(std::fabs(rc - 27.0f) - 2.6f);
        hueco[p] = cubre(32.0f - rc);
      }
    cupo = subir(n, n, cerrar, &icoCerrar_) && cupo;
    cupo = subir(n, n, flecha, &icoFlecha_) && cupo;
    cupo = subir(n, n, palomita, &icoPalomita_) && cupo;
    cupo = subir(n, n, anillo, &icoAnillo_) && cupo;
    cupo = subir(n, n, hueco, &hueco_) && cupo;
    /* el engranaje (v0.84): un disco de ocho dientes con su agujero, para abrir los ajustes */
    std::vector<uint8_t> engranaje((size_t)n * n);
    for (int j = 0; j < n; j++)
      for (int i = 0; i < n; i++) {
        const float x = (float)i + 0.5f - 32.0f, y = (float)j + 0.5f - 32.0f;
        const float r = std::hypot(x, y), t = std::cos(8.0f * std::atan2(y, x));
        const float borde = 21.0f + 7.0f * std::max(0.0f, std::min(1.0f, (t + 0.15f) / 0.5f));
        engranaje[(size_t)j * (size_t)n + (size_t)i] = cubre(std::max(r - borde, 9.5f - r));
      }
    cupo = subir(n, n, engranaje, &icoEngranaje_) && cupo;
    /* el globo del boton del idioma (v1.07): el contorno, el ecuador, dos paralelos y un meridiano */
    std::vector<uint8_t> globo((size_t)n * n);
    for (int j = 0; j < n; j++)
      for (int i = 0; i < n; i++) {
        const float x = (float)i + 0.5f - 32.0f, y = (float)j + 0.5f - 32.0f;
        const float r = std::hypot(x, y);
        float d = std::fabs(r - 26.0f) - 2.4f;
        if (r < 26.0f) {
          d = std::min(d, std::fabs(y) - 2.0f);
          d = std::min(d, std::fabs(std::fabs(y) - 13.0f) - 1.8f);
          d = std::min(d, std::fabs(x) - 2.0f);
          const float e = std::hypot(x / 12.0f, y / 26.0f);   /* el meridiano de canto */
          d = std::min(d, std::fabs(e - 1.0f) * 12.0f - 2.0f);
        }
        globo[(size_t)j * (size_t)n + (size_t)i] = cubre(d);
      }
    cupo = subir(n, n, globo, &icoGlobo_) && cupo;
    /* el rayo del modo rendimiento (rama experimental): un poligono relleno, con 4x4 muestras por
       punto para el borde suave */
    /* y la flechita que dice que el boton del rayo despliega un menu: una V gruesa, igual de rellena */
    auto poligono = [&](const float (*P)[2], int np, Rect *dest) {
      auto dentro = [&](float x, float y) {
        bool c = false;
        for (int a = 0, b = np - 1; a < np; b = a++)
          if (((P[a][1] > y) != (P[b][1] > y)) &&
              (x < (P[b][0] - P[a][0]) * (y - P[a][1]) / (P[b][1] - P[a][1]) + P[a][0]))
            c = !c;
        return c;
      };
      std::vector<uint8_t> m((size_t)n * n);
      for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
          int k = 0;
          for (int sy = 0; sy < 4; sy++)
            for (int sx = 0; sx < 4; sx++)
              if (dentro((float)i + (sx + 0.5f) / 4.0f, (float)j + (sy + 0.5f) / 4.0f)) k++;
          m[(size_t)j * n + i] = (uint8_t)(k * 255 / 16);
        }
      return subir(n, n, m, dest);
    };
    static const float kRayo[][2] = {{38, 4}, {14, 36}, {29, 36}, {24, 60}, {50, 26}, {35, 26}, {44, 4}};
    static const float kBajar[][2] = {{8, 22}, {16, 14}, {32, 30}, {48, 14}, {56, 22}, {32, 46}};
    static const float kDerecha[][2] = {{22, 8}, {46, 32}, {22, 56}, {14, 48}, {30, 32}, {14, 16}};
    cupo = poligono(kRayo, 7, &icoRayo_) && cupo;
    cupo = poligono(kBajar, 6, &icoBajar_) && cupo;
    cupo = poligono(kDerecha, 6, &icoDerecha_) && cupo;
    /* el pin del panel: cabeza, collar y aguja */
    static const float kPin[][2] = {{22, 6}, {42, 6}, {42, 30}, {50, 30}, {50, 37}, {35, 37}, {32, 60}, {29, 37}, {14, 37}, {14, 30}, {22, 30}};
    cupo = poligono(kPin, 11, &icoPin_) && cupo;
    /* el chip «AI» del Foco automatico (rama experimental): minimalista, de trazos redondos */
    {
      std::vector<uint8_t> chip((size_t)n * n);
      for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
          const float x = (float)i + 0.5f, y = (float)j + 0.5f;
          /* el contorno: un cuadrado redondeado de 12 a 52 */
          const float qx = std::fabs(x - 32.0f) - (20.0f - 9.0f), qy = std::fabs(y - 32.0f) - (20.0f - 9.0f);
          const float caja = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - 9.0f;
          float d = std::fabs(caja) - 2.6f;
          for (float k : {22.0f, 32.0f, 42.0f}) {   /* las patitas */
            d = std::min(d, distSegmento(x, y, k, 11.0f, k, 4.5f) - 2.4f);
            d = std::min(d, distSegmento(x, y, k, 53.0f, k, 59.5f) - 2.4f);
            d = std::min(d, distSegmento(x, y, 11.0f, k, 4.5f, k) - 2.4f);
            d = std::min(d, distSegmento(x, y, 53.0f, k, 59.5f, k) - 2.4f);
          }
          /* la A y la I */
          d = std::min(d, distSegmento(x, y, 22.5f, 42.0f, 28.0f, 22.5f) - 2.4f);
          d = std::min(d, distSegmento(x, y, 28.0f, 22.5f, 33.5f, 42.0f) - 2.4f);
          d = std::min(d, distSegmento(x, y, 24.8f, 35.0f, 31.2f, 35.0f) - 2.2f);
          d = std::min(d, distSegmento(x, y, 40.5f, 22.5f, 40.5f, 42.0f) - 2.4f);
          chip[(size_t)j * n + i] = cubre(d);
        }
      cupo = subir(n, n, chip, &icoChip_) && cupo;
    }
    /* el logo del programa (rama experimental): sus puntos, de assets/ (logo_mascara.inc) */
    {
      std::vector<uint8_t> logo(kLogoMascara, kLogoMascara + kLogoLado * kLogoLado);
      cupo = subir(kLogoLado, kLogoLado, logo, &icoLogo_) && cupo;
    }
    /* la llave del Manual (rama experimental): el mango en diagonal y la cabeza abierta arriba a la derecha */
    {
      std::vector<uint8_t> llave((size_t)n * n);
      const float cx = 42.0f, cy = 22.0f, ux = 0.7071f, uy = -0.7071f;
      for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
          const float x = (float)i + 0.5f, y = (float)j + 0.5f;
          const float mango = distSegmento(x, y, 15.0f, 49.0f, 35.0f, 29.0f) - 5.0f;
          const float qx = x - cx, qy = y - cy;
          const float disco = std::hypot(qx, qy) - 13.0f;
          const float largo = qx * ux + qy * uy, ancho = std::fabs(qx * -uy + qy * ux);
          const float ranura = std::max(-largo - 2.0f, ancho - 4.8f);   /* negativa dentro de la boca */
          const float cabeza = std::max(disco, -ranura);
          llave[(size_t)j * n + i] = cubre(std::min(mango, cabeza));
        }
      cupo = subir(n, n, llave, &icoLlave_) && cupo;
    }
    /* el encendido del 3D (v1.12): el aro abierto por arriba y su raya, el simbolo de siempre */
    {
      std::vector<uint8_t> enc((size_t)n * n);
      for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
          const float x = (float)i + 0.5f, y = (float)j + 0.5f;
          const float dx = x - 32.0f, dy = y - 34.0f;
          float d = std::fabs(std::hypot(dx, dy) - 19.0f) - 3.2f;
          /* la abertura de arriba: fuera del aro lo que cae a menos de ~40 grados de la vertical */
          const float ang = std::atan2(dx, -dy);   /* 0 arriba */
          if (std::fabs(ang) < 0.70f) d = std::max(d, 3.0f);
          d = std::min(d, distSegmento(x, y, 32.0f, 8.0f, 32.0f, 30.0f) - 3.2f);
          enc[(size_t)j * n + i] = cubre(d);
        }
      cupo = subir(n, n, enc, &icoEncender_) && cupo;
    }
    /* (v1.16) los de las teselas al estilo de vkDaedalus, de trazo fino y redondo como los suyos */
    auto dibujo = [&](auto distancia, Rect *dst) {
      std::vector<uint8_t> m((size_t)n * n);
      for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) m[(size_t)j * n + i] = cubre(distancia((float)i + 0.5f, (float)j + 0.5f));
      cupo = subir(n, n, m, dst) && cupo;
    };
    auto cajaRedonda = [](float x, float y, float cx, float cy, float mx, float my, float r) {
      const float qx = std::fabs(x - cx) - (mx - r), qy = std::fabs(y - cy) - (my - r);
      return std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
    };
    /* el cubo del 3D: el hexagono y la Y de dentro */
    dibujo([&](float x, float y) {
      static const float v[6][2] = {{32, 7}, {54, 19.5f}, {54, 44.5f}, {32, 57}, {10, 44.5f}, {10, 19.5f}};
      float d = 1e9f;
      for (int k = 0; k < 6; k++) d = std::min(d, distSegmento(x, y, v[k][0], v[k][1], v[(k + 1) % 6][0], v[(k + 1) % 6][1]));
      d = std::min(d, distSegmento(x, y, 32, 32, 32, 57));
      d = std::min(d, distSegmento(x, y, 32, 32, 10, 19.5f));
      d = std::min(d, distSegmento(x, y, 32, 32, 54, 19.5f));
      return d - 2.4f;
    }, &icoCubo_);
    /* el mando de Juegos: el cuerpo, la cruz y dos botones */
    dibujo([&](float x, float y) {
      float d = std::fabs(cajaRedonda(x, y, 32, 34, 24, 13, 11)) - 2.4f;
      d = std::min(d, distSegmento(x, y, 20, 28, 20, 40) - 2.2f);
      d = std::min(d, distSegmento(x, y, 14, 34, 26, 34) - 2.2f);
      d = std::min(d, std::hypot(x - 41, y - 31) - 3.2f);
      d = std::min(d, std::hypot(x - 47, y - 37) - 3.2f);
      return d;
    }, &icoMando_);
    /* el enchufe de Integraciones: las dos patas, el cuerpo y el cable */
    dibujo([&](float x, float y) {
      float d = std::fabs(cajaRedonda(x, y, 32, 32, 13, 10, 5)) - 2.4f;
      d = std::min(d, distSegmento(x, y, 26, 8, 26, 22) - 2.4f);
      d = std::min(d, distSegmento(x, y, 38, 8, 38, 22) - 2.4f);
      d = std::min(d, distSegmento(x, y, 32, 42, 32, 57) - 2.4f);
      return d;
    }, &icoEnchufe_);
    /* los deslizadores de Ajustes: tres rayas con su bola, como el de Effects de vkDaedalus */
    dibujo([&](float x, float y) {
      float d = 1e9f;
      const float fila[3] = {17, 32, 47}, bola[3] = {42, 24, 46};
      for (int k = 0; k < 3; k++) {
        d = std::min(d, distSegmento(x, y, 9, fila[k], 55, fila[k]) - 2.2f);
        d = std::min(d, std::hypot(x - bola[k], y - fila[k]) - 5.2f);
      }
      return d;
    }, &icoDeslizadores_);
    /* (v1.18) el de FG: un cuadro de verdad, otro generado detras (a trazos) y la flecha entre los dos */
    dibujo([&](float x, float y) {
      float d = std::fabs(cajaRedonda(x, y, 22, 34, 12, 14, 3)) - 2.4f;
      const float atras = std::fabs(cajaRedonda(x, y, 44, 30, 12, 14, 3)) - 2.4f;
      d = std::min(d, std::fmod(x + y + 64.0f, 8.0f) < 5.0f ? atras : 1e9f);
      d = std::min(d, distSegmento(x, y, 18, 34, 30, 34) - 2.2f);
      d = std::min(d, distSegmento(x, y, 30, 34, 25, 29) - 2.2f);
      d = std::min(d, distSegmento(x, y, 30, 34, 25, 39) - 2.2f);
      return d;
    }, &icoFg_);
    /* (v1.18) el de los efectos de ReShade: dos destellos de cuatro puntas, uno grande y uno chico. La forma es la de
       sqrt|x| + sqrt|y| = sqrt(R) (un astroide); su distancia, la del valor partido por su pendiente */
    dibujo([&](float x, float y) {
      auto destello = [](float px, float py, float r) {
        auto f = [&](float a, float b) { return std::sqrt(std::fabs(a)) + std::sqrt(std::fabs(b)) - std::sqrt(r); };
        const float v = f(px, py), gx = (f(px + 0.5f, py) - f(px - 0.5f, py)), gy = (f(px, py + 0.5f) - f(px, py - 0.5f));
        return v / std::max(0.02f, std::hypot(gx, gy));
      };
      return std::min(destello(x - 27.0f, y - 37.0f, 22.0f), destello(x - 48.0f, y - 15.0f, 10.0f));
    }, &icoEfectos_);
    /* la cabeza del seguimiento: la cabeza y los hombros */
    dibujo([&](float x, float y) {
      float d = std::fabs(std::hypot(x - 32, y - 22) - 10.0f) - 2.4f;
      const float hombros = std::fabs(std::hypot((x - 32) * 0.82f, y - 58) - 20.0f) - 2.4f;
      d = std::min(d, y > 56.0f ? 1e9f : hombros);
      return d;
    }, &icoCabeza_);
    /* las barras de los FPS (el de Performance): el eje y tres barras */
    dibujo([&](float x, float y) {
      float d = std::min(distSegmento(x, y, 10, 10, 10, 54), distSegmento(x, y, 10, 54, 54, 54)) - 2.2f;
      d = std::min(d, distSegmento(x, y, 22, 44, 22, 34) - 3.0f);
      d = std::min(d, distSegmento(x, y, 34, 44, 34, 22) - 3.0f);
      d = std::min(d, distSegmento(x, y, 46, 44, 46, 28) - 3.0f);
      return d;
    }, &icoBarras_);
    /* las capas del HUD (el de Shaders): un rombo y dos medios rombos debajo */
    dibujo([&](float x, float y) {
      float d = 1e9f;
      d = std::min(d, distSegmento(x, y, 32, 10, 54, 22));
      d = std::min(d, distSegmento(x, y, 54, 22, 32, 34));
      d = std::min(d, distSegmento(x, y, 32, 34, 10, 22));
      d = std::min(d, distSegmento(x, y, 10, 22, 32, 10));
      d = std::min(d, std::min(distSegmento(x, y, 10, 32, 32, 44), distSegmento(x, y, 32, 44, 54, 32)));
      d = std::min(d, std::min(distSegmento(x, y, 10, 42, 32, 54), distSegmento(x, y, 32, 54, 54, 42)));
      return d - 2.3f;
    }, &icoCapas_);
    /* la flecha de volver: hacia la izquierda, con el palo y las dos alas de la de actualizar */
    dibujo([&](float x, float y) {
      return std::min(distSegmento(x, y, 14, 32, 50, 32), std::min(distSegmento(x, y, 14, 32, 31, 15), distSegmento(x, y, 14, 32, 31, 49))) - 3.6f;
    }, &icoAtras_);
  }
  /* 2c) el paisaje de las dos teselas: dos siluetas (la sierra del fondo, de picos, y las lomas de
     delante, redondas). Cada mascara cubre el dibujo entero, asi que se pinta encima del cielo con
     la misma caja y basta cambiarle el color. */
  {
    const int mw = 240, mh = 136;
    std::vector<uint8_t> lejos((size_t)mw * mh), cerca((size_t)mw * mh);
    static const float pico[6][2] = {{0.0f, 0.70f}, {0.20f, 0.40f}, {0.36f, 0.64f},
                                     {0.60f, 0.30f}, {0.84f, 0.66f}, {1.0f, 0.52f}};
    for (int i = 0; i < mw; i++) {
      const float fx = ((float)i + 0.5f) / (float)mw;
      float sierra = pico[5][1];
      for (int k = 0; k < 5; k++)
        if (fx >= pico[k][0] && fx <= pico[k + 1][0]) {
          const float t = (fx - pico[k][0]) / (pico[k + 1][0] - pico[k][0]);
          sierra = pico[k][1] + t * (pico[k + 1][1] - pico[k][1]);
          break;
        }
      const float a = (fx - 0.24f) / 0.20f, b = (fx - 0.86f) / 0.22f;
      const float loma = 0.88f - 0.22f * std::exp(-a * a) - 0.15f * std::exp(-b * b);
      for (int j = 0; j < mh; j++) {
        const float y = (float)j + 0.5f;
        lejos[(size_t)j * mw + (size_t)i] = cubre(sierra * (float)mh - y);
        cerca[(size_t)j * mw + (size_t)i] = cubre(loma * (float)mh - y);
      }
    }
    cupo = subir(mw, mh, lejos, &monteLejos_) && cupo;
    cupo = subir(mw, mh, cerca, &monteCerca_) && cupo;
  }
  if (!cupo) { if (err) *err = "atlas sin sitio para las máscaras"; return false; }

  /* 3) fuentes */
#ifdef HAVE_FREETYPE
  FT_Library lib = nullptr;
  FT_Face face = nullptr;
  const char *path = nullptr;
  if (FT_Init_FreeType(&lib) == 0) {
    for (const char *p : kFontPaths) {
      if (access(p, R_OK) != 0) continue;
      if (FT_New_Face(lib, p, 0, &face) == 0) { path = p; break; }
    }
  }
  if (face) {
    FT_Set_Pixel_Sizes(face, 0, kFontPx);
    asc_ = (float)face->size->metrics.ascender / 64.0f;
    bool okAll = true;
    /* 0 = glifo puesto, 1 = el tipo no lo tiene, -1 = el atlas se llenó */
    auto cargarDe = [&](FT_Face cara, unsigned cp, Glyph &gl) -> int {
      if (FT_Get_Char_Index(cara, cp) == 0) return 1;
      if (FT_Load_Char(cara, cp, FT_LOAD_RENDER) != 0) return 1;
      FT_GlyphSlot g = cara->glyph;
      int w = (int)g->bitmap.width, h = (int)g->bitmap.rows;
      /* el espacio no trae dibujo pero si su avance: sin apuntarlo, cada espacio media medio
         cuadratin (13 px de 26) y las frases salian con las palabras separadas de mas */
      gl.adv = (int)(g->advance.x / 64);
      if (!w || !h) return 1;
      int x, y;
      if (!place(w, h, &x, &y)) return -1;
      put(x, y, w, h, g->bitmap.buffer);
      gl.w = w, gl.h = h;
      gl.bx = (int)g->bitmap_left;
      gl.by = (int)g->bitmap_top;
      gl.adv = (int)(g->advance.x / 64);
      gl.u0 = (float)x / (float)aw_;
      gl.v0 = (float)y / (float)ah_;
      gl.u1 = (float)(x + w) / (float)aw_;
      gl.v1 = (float)(y + h) / (float)ah_;
      gl.ok = true;
      return 0;
    };
    auto cargar = [&](unsigned cp, Glyph &gl) -> int { return cargarDe(face, cp, gl); };
    auto ordenaExtra = [&]() {
      std::sort(extra_.begin(), extra_.end(),
                [](const std::pair<unsigned, Glyph> &a, const std::pair<unsigned, Glyph> &b) { return a.first < b.first; });
    };
    for (unsigned cp = 32; cp < 256; cp++) {
      if (cargar(cp, glyphs_[cp]) < 0) { okAll = false; break; }   /* atlas lleno */
    }
    /* fuera del rango latino-1: los signos que mas salen en titulos de videos */
    for (unsigned cp : {0x2014u, 0x2013u, 0x2026u, 0x2018u, 0x2019u, 0x201Cu, 0x201Du, 0x20ACu, 0x00A9u, 0x2122u, 0x21BAu}) {
      Glyph gl;
      if (cargar(cp, gl) != 0) continue;
      extra_.emplace_back(cp, gl);
    }
    /* los del idioma puesto (v1.07): la ligadura francesa, las letras chinas y japonesas y los
       nombres de los siete idiomas del selector. Lo que DejaVu no tenga sale del tipo CJK. */
    {
      FT_Face cjk = nullptr;
      bool cjkBuscada = false;
      const char *cjkRuta = nullptr;
      const char *quiero = lengua == idioma::ZH ? " SC" : " JP";
      auto abreCjk = [&]() {
        cjkBuscada = true;
        for (const char *p : kCjkPaths) {
          if (access(p, R_OK) != 0) continue;
          FT_Face f0 = nullptr;
          if (FT_New_Face(lib, p, 0, &f0) != 0) continue;
          long caras = f0->num_faces, elegida = 0;
          for (long k = 0; k < caras; k++) {
            FT_Face fk = nullptr;
            if (FT_New_Face(lib, p, k, &fk) != 0) continue;
            const std::string fam = fk->family_name ? fk->family_name : "";
            FT_Done_Face(fk);
            if (fam.size() >= 3 && fam.compare(fam.size() - 3, 3, quiero) == 0) { elegida = k; break; }
          }
          FT_Done_Face(f0);
          if (FT_New_Face(lib, p, elegida, &cjk) == 0) { cjkRuta = p; break; }
          cjk = nullptr;
        }
        if (cjk) FT_Set_Pixel_Sizes(cjk, 0, kFontPx);
      };
      int faltan = 0, puestos = 0;
      ordenaExtra();
      for (unsigned cp : idioma::signosDe(lengua)) {
        if (glifo(cp)) continue;
        Glyph gl;
        int r = cargarDe(face, cp, gl);
        if (r == 1) {
          if (!cjkBuscada) abreCjk();
          r = cjk ? cargarDe(cjk, cp, gl) : 1;
        }
        if (r < 0) { okAll = false; break; }
        if (r != 0) { faltan++; continue; }
        extra_.emplace_back(cp, gl);
        puestos++;
      }
      ordenaExtra();
      if (cjk) {
        std::printf("[interfaz] idioma %s · %d signos más, los que faltaban de %s\n", idioma::codigo(lengua), puestos,
                    cjkRuta);
        FT_Done_Face(cjk);
      }
      if (faltan > 0 && idioma::esCjk(lengua))
        std::fprintf(stderr, "[aviso] faltan %d letras de %s: instala el tipo Noto Sans CJK (noto-fonts-cjk)\n", faltan,
                     idioma::nombre(lengua));
    }
    /* (v1.16) Barlow en sus dos pesos, del programa mismo: solo el latino-1 (lo que no traiga sale de la de antes) */
    {
      const unsigned char *datos[2] = {kBarlowNormal, kBarlowSemi};
      const unsigned largos[2] = {kBarlowNormal_len, kBarlowSemi_len};
      int puestas = 0;
      for (int k = 0; k < 2 && okAll; k++) {
        FT_Face fb = nullptr;
        if (FT_New_Memory_Face(lib, datos[k], (FT_Long)largos[k], 0, &fb) != 0) continue;
        FT_Set_Pixel_Sizes(fb, 0, kFontPx);
        ascB_[k] = (float)fb->size->metrics.ascender / 64.0f;
        for (unsigned cp = 32; cp < 256; cp++)
          if (cargarDe(fb, cp, glyphsB_[k][cp]) < 0) { okAll = false; break; }
        FT_Done_Face(fb);
        puestas++;
      }
      hayBarlow_ = puestas == 2 && okAll;
    }
    hasText_ = true;
    std::printf("[interfaz] fuente %s a %d px en atlas %dx%d (lleno hasta %d)%s\n", path, kFontPx, aw_, ah_,
                shelfY_ + shelfH_, hayBarlow_ ? " · y Barlow" : "");
    FT_Done_Face(face);
    if (lib) FT_Done_FreeType(lib);
    if (!okAll) std::fprintf(stderr, "[aviso] el atlas de fuentes se llenó: algunas letras no se dibujan\n");
  } else {
    if (lib) FT_Done_FreeType(lib);
  }
#endif
  if (!hasText_)
    std::fprintf(stderr, "[aviso] sin fuentes (FreeType o los tipos del sistema no están): la interfaz "
                         "se dibuja sin etiquetas\n");
  (void)err;
  return true;
}

/* ------------------------------------------------------------ dibujado ------------------------------------------------------------ */

void Hud::quad(Rect r, Rect uv, float cr, float cg, float cb, float ca) {
  /* con el recorte puesto (la lista del panel, que corre) el cuadro se corta contra su caja y las
     coordenadas del atlas se cortan en la misma proporcion */
  if (hayRecorte_) {
    const float x0 = std::max(r.x, recorte_.x), y0 = std::max(r.y, recorte_.y);
    const float x1 = std::min(r.x + r.w, recorte_.x + recorte_.w), y1 = std::min(r.y + r.h, recorte_.y + recorte_.h);
    if (x1 <= x0 || y1 <= y0 || r.w <= 0.0f || r.h <= 0.0f) return;
    const float fx0 = (x0 - r.x) / r.w, fx1 = (x1 - r.x) / r.w, fy0 = (y0 - r.y) / r.h, fy1 = (y1 - r.y) / r.h;
    uv = {uv.x + uv.w * fx0, uv.y + uv.h * fy0, uv.w * (fx1 - fx0), uv.h * (fy1 - fy0)};
    r = {x0, y0, x1 - x0, y1 - y0};
  }
  /* (v1.16) la interfaz que sube y se hace opaca al abrirse (dibujaPanel lo pone y lo quita) */
  r.y += desplazY_;
  ca *= alfaGlobal_;
  if (ca <= 0.001f) return;   /* invisible: el primer cuadro de la aparicion (todo a 0) no recorre la pantalla entera */
  /* dos triángulos en forma de tira: 6 vértices */
  const float X[6] = {r.x, r.x + r.w, r.x, r.x + r.w, r.x, r.x + r.w};
  const float Y[6] = {r.y, r.y, r.y + r.h, r.y + r.h, r.y + r.h, r.y};
  const float U[6] = {uv.x, uv.x + uv.w, uv.x, uv.x + uv.w, uv.x, uv.x + uv.w};
  const float V[6] = {uv.y, uv.y, uv.y + uv.h, uv.y + uv.h, uv.y + uv.h, uv.y};
  for (int i = 0; i < 6; i++) v_.push_back({X[i] * sc_, Y[i] * sc_, U[i], V[i], cr, cg, cb, ca});
}

void Hud::solid(float x, float y, float w, float h, float cr, float cg, float cb, float ca) {
  Rect r{x, y, w, h};
  Rect uv{white_.x, white_.y, white_.w, white_.h};
  /* solo el centro del bloque blanco, para no interpolar con el borde ni con el vecino */
  uv.x = (white_.x + 1.5f) / (float)aw_;
  uv.y = (white_.y + 1.5f) / (float)ah_;
  uv.w = 1.0f / (float)aw_;
  uv.h = 1.0f / (float)ah_;
  quad(r, uv, cr, cg, cb, ca);
}

/* 9 porciones: las esquinas conservan el radio y el centro se estira. */
void Hud::mask9(Rect r, int which, float radius, float cr, float cg, float cb, float ca) {
  const Rect &m = which == 0 ? pill_ : card_;
  float msz = m.w;
  float rad = std::min(radius, std::min(r.w, r.h) * 0.5f);
  /* la esquina se recorta de la mascara con el radio PROPIO de la mascara (un cuarto de circulo
     completo en la píldora) y se estira hasta `rad` de pantalla; si se recortara con `rad / msz`
     se sacaria una cuña del arco y la forma saldria puntiaguda */
  float fr = std::min(maskRad_[which] / msz, 0.5f);   /* radio en unidades de la máscara */
  float u0 = m.x / (float)aw_, v0 = m.y / (float)ah_;
  float du = msz / (float)aw_, dv = msz / (float)ah_;
  float l = u0, m0 = u0 + fr * du, r1 = u0 + (1.0f - fr) * du;
  float t = v0, n0 = v0 + fr * dv, b1 = v0 + (1.0f - fr) * dv;
  float cx = std::min(rad, r.w), cy = std::min(rad, r.h);
  struct Part { float x, y, w, h, u, v, uw, vh; };
  const float Xs[3] = {r.x, r.x + cx, r.x + r.w - cx};
  const float Xw[3] = {cx, r.w - 2.0f * cx, cx};
  const float Us[3] = {l, m0, r1};
  const float Uw[3] = {fr * du, (1.0f - 2.0f * fr) * du, fr * du};
  const float Ys[3] = {r.y, r.y + cy, r.y + r.h - cy};
  const float Yh[3] = {cy, r.h - 2.0f * cy, cy};
  const float Vs[3] = {t, n0, b1};
  const float Vh[3] = {fr * dv, (1.0f - 2.0f * fr) * dv, fr * dv};
  for (int j = 0; j < 3; j++)
    for (int i = 0; i < 3; i++) {
      if (Xw[i] <= 0.0f || Yh[j] <= 0.0f) continue;
      quad({Xs[i], Ys[j], Xw[i], Yh[j]}, {Us[i], Vs[j], Uw[i], Vh[j]}, cr, cg, cb, ca);
    }
}

/* Una mascara entera (o un trozo: fx0..fx1, fy0..fy1 en fracciones de la mascara) estirada sobre
   la caja. Media celda hacia dentro para no mezclar con la mascara vecina del atlas. */
void Hud::icono(Rect r, const Rect &m, float cr, float cg, float cb, float ca, float fx0, float fy0, float fx1,
                float fy1) {
  const float x0 = m.x + 0.5f + fx0 * (m.w - 1.0f), x1 = m.x + 0.5f + fx1 * (m.w - 1.0f);
  const float y0 = m.y + 0.5f + fy0 * (m.h - 1.0f), y1 = m.y + 0.5f + fy1 * (m.h - 1.0f);
  quad(r, {x0 / (float)aw_, y0 / (float)ah_, (x1 - x0) / (float)aw_, (y1 - y0) / (float)ah_}, cr, cg, cb, ca);
}

/* Cielo, sol, sierra y lomas. En el video dividido cada mitad lleva la imagen entera aplastada a la
   mitad, como llega de verdad. Las esquinas se redondean tapandolas con el color de lo que hay
   detras (por eso se le pasa). */
void Hud::paisaje(Rect r, float apretado, float luz, float radio, const float *fondo, const float *tinte) {
  /* (v1.16) con `tinte`, del color del modo (los dos botones del tipo de video en la pastilla: el azul de Neural o el
     ambar del rendimiento; Rodolfo: «can the desktop buttons also have the yellow and blue colors?»); sin el, morado */
  float cielo[3] = {0.42f, 0.33f, 0.88f}, sol[3] = {0.82f, 0.76f, 1.0f}, lejos[3] = {0.31f, 0.23f, 0.70f}, cerca[3] = {0.21f, 0.15f, 0.50f};
  if (tinte)
    for (int k = 0; k < 3; k++)
      cielo[k] = tinte[k] * 0.72f, sol[k] = tinte[k] * 0.4f + 0.6f, lejos[k] = tinte[k] * 0.50f, cerca[k] = tinte[k] * 0.32f;
  solid(r.x, r.y, r.w, r.h, cielo[0] * luz, cielo[1] * luz, cielo[2] * luz, 1.0f);
  const float d = r.h * 0.25f;
  icono({r.x + r.w * 0.80f - d * apretado * 0.5f, r.y + r.h * 0.13f, d * apretado, d}, pill_, sol[0] * luz,
        sol[1] * luz, sol[2] * luz, 1.0f);
  icono(r, monteLejos_, lejos[0] * luz, lejos[1] * luz, lejos[2] * luz, 1.0f);
  icono(r, monteCerca_, cerca[0] * luz, cerca[1] * luz, cerca[2] * luz, 1.0f);
  if (radio <= 0.0f) return;
  const Rect esq[4] = {{r.x, r.y, radio, radio},
                       {r.x + r.w - radio, r.y, radio, radio},
                       {r.x, r.y + r.h - radio, radio, radio},
                       {r.x + r.w - radio, r.y + r.h - radio, radio, radio}};
  for (int k = 0; k < 4; k++) {
    const float fx = (k & 1) ? 0.5f : 0.0f, fy = (k & 2) ? 0.5f : 0.0f;
    icono(esq[k], hueco_, fondo[0], fondo[1], fondo[2], 1.0f, fx, fy, fx + 0.5f, fy + 0.5f);
  }
}

float Hud::muestra(float u, float v) const {
  if (atlas_.empty() || aw_ <= 0 || ah_ <= 0) return 1.0f;
  float fx = u * (float)aw_ - 0.5f, fy = v * (float)ah_ - 0.5f;
  int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
  float tx = fx - (float)x0, ty = fy - (float)y0;
  auto tex = [&](int x, int y) -> float {
    x = std::max(0, std::min(aw_ - 1, x));
    y = std::max(0, std::min(ah_ - 1, y));
    return (float)atlas_[(size_t)y * (size_t)aw_ + (size_t)x] / 255.0f;
  };
  const float a00 = tex(x0, y0), a10 = tex(x0 + 1, y0);
  const float a01 = tex(x0, y0 + 1), a11 = tex(x0 + 1, y0 + 1);
  return (a00 * (1.0f - tx) + a10 * tx) * (1.0f - ty) + (a01 * (1.0f - tx) + a11 * tx) * ty;
}

/* Lo mismo que hace Vulkan con los vertices, pero en la CPU: lo usa la tarjeta de la orilla (que
   vive en una capa de Wayland y no tiene GPU) y `ctl hud --png`, que deja la interfaz en un archivo
   para mirarla sin el panel. `acum` sale en RGBA de coma flotante con el color YA multiplicado por
   su opacidad. Un color por cuadro y sin espejos: la interfaz no usa otra cosa. */
void Hud::pintarCpu(float ox, float oy, int ancho, int alto, std::vector<float> &acum) const {
  /* (v1.16) en varios hilos, por franjas de filas: cada hilo pone a cero su franja y pinta en ella todo, en el mismo
     orden, asi que cada punto sale igual. MEDIDO en 4K a pantalla completa: 20 ms por cuadro en un hilo */
  acum.resize((size_t)ancho * (size_t)alto * 4u);
  /* (v1.14) cada caratula se pinta en su turno: despues de lo que se dibujo antes que ella y debajo de lo que vino
     despues. Antes iban todas al final, encima de todo, y tapaban la ayuda que sale al pasar el raton (MEDIDO por
     Rodolfo: la caratula de Red Dead Redemption 2 sobre la ayuda del boton Copiar del pie de Juegos) */
  auto pintaCuadros = [&](size_t desde, size_t hasta, int banda0, int banda1) {
  for (size_t i = desde; i + 5 < hasta; i += 6) {
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    float u0 = 1e30f, v0 = 1e30f, u1 = -1e30f, v1 = -1e30f;
    for (int k = 0; k < 6; k++) {
      const HudVertex &q = v_[i + (size_t)k];
      x0 = std::min(x0, q.x);
      y0 = std::min(y0, q.y);
      x1 = std::max(x1, q.x);
      y1 = std::max(y1, q.y);
      u0 = std::min(u0, q.u);
      v0 = std::min(v0, q.v);
      u1 = std::max(u1, q.u);
      v1 = std::max(v1, q.v);
    }
    const float r = v_[i].r, g = v_[i].g, b = v_[i].b, al = v_[i].a;
    const float ax0 = std::max(0.0f, x0 - ox), ay0 = std::max(0.0f, y0 - oy);
    const float ax1 = std::min((float)ancho, x1 - ox), ay1 = std::min((float)alto, y1 - oy);
    if (ax1 <= ax0 || ay1 <= ay0) continue;
    if (x1 <= x0 || y1 <= y0) continue;
    const float du = u1 - u0, dv = v1 - v0;
    const float w = x1 - x0, h = y1 - y0;
    /* La regla de la GPU: se pinta el pixel cuyo CENTRO cae dentro del cuadro. Antes se pintaban
       tambien los que el cuadro solo rozaba, y en esos la coordenada del atlas se salia medio pixel
       de la mascara y traia a la vecina: una raya a la derecha del interruptor y un marco tenue
       alrededor de cada letra. */
    const int px0 = std::max(0, (int)std::ceil(ax0 - 0.5f)), py0 = std::max(0, (int)std::ceil(ay0 - 0.5f));
    const int px1 = std::min(ancho, (int)std::ceil(ax1 - 0.5f)), py1 = std::min(alto, (int)std::ceil(ay1 - 0.5f));
    if (std::max(py0, banda0) >= std::min(py1, banda1)) continue;   /* fuera de esta franja */
    /* cuadro liso (el pixel blanco, o el centro de una mascara: sus cuatro esquinas del todo
       opacas): una sola muestra para todo el cuadro. El fondo del panel de ajustes son dos millones
       de pixeles y muestrearlos uno a uno costaba la mitad del pintado. */
    const float c00 = muestra(u0, v0), c10 = muestra(u1, v0), c01 = muestra(u0, v1), c11 = muestra(u1, v1);
    const bool liso = (du * (float)aw_ <= 1.01f && dv * (float)ah_ <= 1.01f) ||
                      (c00 >= 0.999f && c10 >= 0.999f && c01 >= 0.999f && c11 >= 0.999f);
    for (int py = std::max(py0, banda0); py < std::min(py1, banda1); py++) {
      const float fy = ((float)py + 0.5f - (y0 - oy)) / h;
      const float v = v0 + fy * dv;
      float *fila = acum.data() + ((size_t)py * (size_t)ancho + (size_t)px0) * 4u;
      for (int px = px0; px < px1; px++) {
        const float fx = ((float)px + 0.5f - (x0 - ox)) / w;
        const float a = al * (liso ? c00 : muestra(u0 + fx * du, v));
        if (a > 0.002f) {
          const float unoMenos = 1.0f - a;
          fila[0] = r * a + fila[0] * unoMenos;
          fila[1] = g * a + fila[1] * unoMenos;
          fila[2] = b * a + fila[2] * unoMenos;
          fila[3] = a + fila[3] * unoMenos;
        }
        fila += 4;
      }
    }
  }
  };
  /* una caratula, con las esquinas redondeadas por cuenta propia */
  auto pintaImagen = [&](const ImagenCpu &im, int banda0, int banda1) {
    if (!im.px || im.w <= 0 || im.h <= 0) return;
    const float cx0 = im.caja.x * sc_, cy0 = im.caja.y * sc_, cw = im.caja.w * sc_, ch = im.caja.h * sc_;
    const float rx0 = std::max(cx0, im.recorte.x * sc_), ry0 = std::max(cy0, im.recorte.y * sc_);
    const float rx1 = std::min(cx0 + cw, (im.recorte.x + im.recorte.w) * sc_);
    const float ry1 = std::min(cy0 + ch, (im.recorte.y + im.recorte.h) * sc_);
    const int px0 = std::max(0, (int)std::ceil(rx0 - ox - 0.5f)), py0 = std::max(0, (int)std::ceil(ry0 - oy - 0.5f));
    const int px1 = std::min(ancho, (int)std::ceil(rx1 - ox - 0.5f)), py1 = std::min(alto, (int)std::ceil(ry1 - oy - 0.5f));
    const float rad = im.radio * sc_;
    for (int py = std::max(py0, banda0); py < std::min(py1, banda1); py++) {
      const float y = (float)py + 0.5f + oy - cy0;
      const float fy = std::max(0.0f, std::min((float)im.h - 1.001f, y / ch * (float)im.h - 0.5f));
      const int iy = (int)fy;
      const float ty = fy - (float)iy;
      for (int px = px0; px < px1; px++) {
        const float x = (float)px + 0.5f + ox - cx0;
        /* cobertura del rectangulo redondeado */
        const float dx = std::max(0.0f, std::max(rad - x, x - (cw - rad))), dy = std::max(0.0f, std::max(rad - y, y - (ch - rad)));
        const float a = std::max(0.0f, std::min(1.0f, rad + 0.5f - std::hypot(dx, dy))) * im.alfa;
        if (a <= 0.0f) continue;
        const float fx = std::max(0.0f, std::min((float)im.w - 1.001f, x / cw * (float)im.w - 0.5f));
        const int ix = (int)fx;
        const float tx = fx - (float)ix;
        const uint8_t *p00 = im.px + ((size_t)iy * im.w + ix) * 4u, *p10 = p00 + 4, *p01 = p00 + (size_t)im.w * 4u, *p11 = p01 + 4;
        float *d = acum.data() + ((size_t)py * (size_t)ancho + (size_t)px) * 4u;
        float col[3];
        for (int c = 0; c < 3; c++)
          col[c] = ((p00[c] * (1 - tx) + p10[c] * tx) * (1 - ty) + (p01[c] * (1 - tx) + p11[c] * tx) * ty) / 255.0f;
        if (im.gris) {   /* (v1.16) su luminancia, oscurecida */
          const float l = (0.299f * col[0] + 0.587f * col[1] + 0.114f * col[2]) * 0.5f;
          col[0] = col[1] = col[2] = l;
        }
        for (int c = 0; c < 3; c++) d[c] = col[c] * a + d[c] * (1.0f - a);
        d[3] = a + d[3] * (1.0f - a);
      }
    }
  };
  auto franja = [&](int banda0, int banda1) {
    std::fill(acum.begin() + (ptrdiff_t)((size_t)banda0 * (size_t)ancho * 4u), acum.begin() + (ptrdiff_t)((size_t)banda1 * (size_t)ancho * 4u), 0.0f);
    size_t hecho = 0;
    for (const ImagenCpu &im : imagenes_) {
      const size_t hasta = std::min(im.orden, v_.size());
      if (hasta > hecho) pintaCuadros(hecho, hasta, banda0, banda1), hecho = hasta;
      pintaImagen(im, banda0, banda1);
    }
    pintaCuadros(hecho, v_.size(), banda0, banda1);
  };
  static const bool unHilo = std::getenv("ODYSSEYVK_PINTA_HILOS") && std::getenv("ODYSSEYVK_PINTA_HILOS")[0] == '1';   /* pruebas */
  const unsigned hilos = unHilo ? 1u : (size_t)ancho * (size_t)alto >= 1000000u ? std::max(1u, std::min(8u, std::thread::hardware_concurrency())) : 1u;
  if (hilos <= 1 || alto < 64) {
    franja(0, alto);
    return;
  }
  std::vector<std::thread> trabajo;
  const int alto1 = (alto + (int)hilos - 1) / (int)hilos;
  for (unsigned k = 1; k < hilos; k++) {
    const int b0 = std::min(alto, (int)k * alto1), b1 = std::min(alto, ((int)k + 1) * alto1);
    if (b0 < b1) trabajo.emplace_back(franja, b0, b1);
  }
  franja(0, std::min(alto, alto1));
  for (std::thread &t : trabajo) t.join();
}

const Hud::Glyph *Hud::glifo(unsigned cp) const {
  /* (v1.16) con Barlow puesta, la suya si la tiene */
  if (fuente_ > 0 && hayBarlow_ && cp < 256) {
    const Glyph &b = glyphsB_[fuente_ - 1][cp];
    if (b.ok || b.adv > 0) return &b;
  }
  if (cp < 256) return &glyphs_[cp];
  /* `extra_` va ordenado (el atlas lo ordena al final): con ~300 letras chinas ya no vale mirar una a una */
  auto it = std::lower_bound(extra_.begin(), extra_.end(), cp,
                             [](const std::pair<unsigned, Glyph> &e, unsigned c) { return e.first < c; });
  if (it != extra_.end() && it->first == cp) return &it->second;
  return nullptr;   /* fuera de la tabla: se dibuja como un espacio */
}

float Hud::textWidth(const std::string &s, float size) const {
  float sc = size / (float)kFontPx;
  float wsum = 0.0f;
  size_t i = 0;
  while (i < s.size()) {
    unsigned cp = nextCp(s, i);
    const Glyph *gp = glifo(cp);
    wsum += (gp && (gp->ok || gp->adv > 0)) ? (float)gp->adv : (float)kFontPx * 0.5f;
  }
  return wsum * sc;
}

float Hud::text(const std::string &s, float x, float y, float size, float cr, float cg, float cb, float ca,
                bool center) {
  float sc = size / (float)kFontPx;
  float pen = center ? x - textWidth(s, size) * 0.5f : x;
  float top0 = y;
  const float asc = fuente_ > 0 && hayBarlow_ ? ascB_[fuente_ - 1] : asc_;   /* (v1.16) la linea base de la que escribe */
  size_t i = 0;
  while (i < s.size()) {
    unsigned cp = nextCp(s, i);
    const Glyph *gp = glifo(cp);
    if (!gp || !gp->ok) { pen += (gp && gp->adv > 0 ? (float)gp->adv : (float)kFontPx * 0.5f) * sc; continue; }
    const Glyph &g = *gp;
    /* el glifo cuelga de la línea base: su borde superior está a asc_-bearingY */
    Rect r{pen + (float)g.bx * sc, top0 + (asc - (float)g.by) * sc, (float)g.w * sc, (float)g.h * sc};
    /* el guion y el guion bajo miden 2 px en el atlas: en letra chica la caja quedaba de menos de un
       punto y no tocaba ningun centro de pixel (salia «the cave dweller» sin guiones) */
    if (r.h < 1.3f) {
      r.y -= (1.3f - r.h) * 0.5f;
      r.h = 1.3f;
    }
    /* (v1.16) lo mismo de ancho: la «l» de Barlow mide 2 px en el atlas y a 11,5 puntos se perdia («No insta ada») */
    if (r.w < 1.3f) {
      r.x -= (1.3f - r.w) * 0.5f;
      r.w = 1.3f;
    }
    quad(r, {g.u0, g.v0, g.u1 - g.u0, g.v1 - g.v0}, cr, cg, cb, ca);
    pen += (float)g.adv * sc;
  }
  return pen;
}

float Hud::textoCabe(const std::string &s, float x, float y, float size, float maxAncho, float cr, float cg, float cb,
                     float ca, bool center) {
  const float w = textWidth(s, size);
  if (maxAncho <= 0.0f || w <= maxAncho) return text(s, x, y, size, cr, cg, cb, ca, center);
  const float chico = size * 0.80f;
  if (w * 0.80f <= maxAncho) {
    const float t = size * maxAncho / w;
    /* bajada a la misma linea base: la letra mas chica se baja lo que encoge */
    return text(s, x, y + (size - t) * 0.55f, t, cr, cg, cb, ca, center);
  }
  std::string corto;
  const float reserva = textWidth("…", chico);
  size_t i = 0;
  while (i < s.size()) {
    const size_t antes = i;
    nextCp(s, i);
    if (textWidth(corto + s.substr(antes, i - antes), chico) > maxAncho - reserva) break;
    corto += s.substr(antes, i - antes);
  }
  return text(corto + "…", x, y + (size - chico) * 0.55f, chico, cr, cg, cb, ca, center);
}

void Hud::toggle3D() { st_.weaveOn = !st_.weaveOn; }
void Hud::toggleOverlay() { st_.overlayOn = !st_.overlayOn; }
void Hud::nudgeDepth(int d) { st_.depth = std::max(0, std::min(100, st_.depth + d)); }
void Hud::nudgePopout(int d) { st_.popout = std::max(-50, std::min(50, st_.popout + d)); }

/* (v1.13) el cuadrito de los FPS del juego, con su orilla derecha en `xDer`. (v1.17) Tambien con la pastilla oculta,
   en la esquina de arriba a la derecha y pegado a las dos orillas (`orilla`), hasta que se apague en los ajustes
   (Rodolfo: «no quiero que se esconda, sino que se quede al frente y se mueva a la orilla de la pantalla») */
void Hud::dibujaCuadroFps(float xDer, float ph, bool compacta, bool orilla, const float accent[3], const float ink[4],
                          const float dim[4]) {
  const float y0 = 0.0f;
  /* (v1.17) filas: las de los FPS (si hay un juego) y luego lo que se eligio vigilar en la tesela del cuadrito; cada una
     con su rotulo y sus valores en columnas (el numero en blanco, la unidad chica y gris, como siempre) */
  struct Seg { std::string num, uni; };
  struct Fila { std::string rot; std::vector<Seg> segs; };
  std::vector<Fila> filas;
  const bool dlss = st_.fpsModo == 1;
  char nj[16], nc[16];
  std::snprintf(nj, sizeof nj, "%d", (int)std::lround(st_.fpsJuego));
  std::snprintf(nc, sizeof nc, "%d", (int)std::lround(st_.fpsCapa));
  /* (v1.18) con el contador de MAKO, sus tres filas en lugar de Juego y Capa, que son los mismos reales (Rodolfo: «pon reales
     generados y en pantalla») */
  /* (v1.18) con el contador, lo medido; sin DLSS y con el tiempo por cuadro elegido, lo que llega a la pantalla repartido con
     el multiplicador del perfil de MAKO del juego (sin generacion, todos reales) */
  const bool cuentaMedida = st_.fg.reales > 0.5f;
  const bool cuentaEstimada = !cuentaMedida && st_.fpsModo == 2 && st_.fg.tiempoCuadro && st_.fpsJuego > 0.5f;
  const bool cuenta = cuentaMedida || cuentaEstimada;
  float cReales = st_.fg.reales, cGenerados = st_.fg.generados;
  if (cuentaEstimada) {
    const int m = st_.fg.enJuego && st_.fg.puesta ? std::max(1, st_.fg.multiplicador) : 1;
    cReales = st_.fpsJuego / (float)m, cGenerados = st_.fpsJuego - cReales;
  }
  if (st_.fpsModo != 0 && !cuenta) {
    filas.push_back({idioma::tr("Juego"), {{nj, "fps"}}});
    if (dlss) filas.push_back({idioma::tr("Capa"), {{nc, "fps"}}});
    /* (v1.18) con la generacion de cuadros de MAKO en el juego: lo que llega a la pantalla. MEDIDO con el cargador de
       Vulkan: MAKO va por debajo de la capa de OdysseyVK, asi que «Juego» (los cuadros que ve la capa) son los de verdad y
       los generados solo se ven aqui, en los que llegan al visor por la captura */
    if (dlss && st_.fg.enJuego && st_.fg.puesta && st_.fg.enPantalla > 0.5f && st_.fg.reales <= 0.5f) {
      char np[16];
      std::snprintf(np, sizeof np, "%d", (int)std::lround(st_.fg.enPantalla));
      filas.push_back({idioma::tr("En pantalla"), {{np, "fps"}}});
    }
  }
  /* (v1.18) con el contador de MAKO (la capa encima de MAKO y el contador debajo, como el de vkDaedalus): los cuadros del
     juego y los que MAKO genera, cada segundo */
  if (cuenta) {
    char nr[16], ng[16];
    std::snprintf(nr, sizeof nr, "%d", (int)std::lround(cReales));
    std::snprintf(ng, sizeof ng, "%d", (int)std::lround(cGenerados));
    filas.push_back({idioma::tr("Reales"), {{nr, "fps"}}});
    filas.push_back({idioma::tr("Generados"), {{ng, "fps"}}});
    /* y la suma, lo que sale de MAKO (Rodolfo: «no me da la cuenta»): con x3, cada real son tres en pantalla, uno real y dos
       generados */
    char nt[16];
    std::snprintf(nt, sizeof nt, "%d", (int)std::lround(cReales + cGenerados));
    filas.push_back({idioma::tr("En pantalla"), {{nt, "fps"}}});
  }
  for (const State::FilaMonitor &m : st_.monitor) {
    Fila f{m.rotulo, {}};
    for (const auto &v : m.valores) f.segs.push_back({v.first, v.second});
    filas.push_back(f);
  }
  if (filas.empty()) return;
  const std::string titulo = dlss ? std::string("DLSS")
                             : (st_.fpsModo == 0 && cuenta ? std::string("MAKO")
                                                           : std::string(idioma::tr(st_.fpsModo == 2 ? "Modelo de serie" : "Monitor")));
  const float tamR = 12.0f, tamN = 14.0f, tamF = 10.5f, borde = 14.0f, hueco = 10.0f;
  float wRot = 0.0f;
  size_t nCol = 0;
  for (const Fila &f : filas) wRot = std::max(wRot, textWidth(f.rot, tamR)), nCol = std::max(nCol, f.segs.size());
  std::vector<float> wNum(nCol, textWidth("000", tamN)), wUni(nCol, 0.0f);
  for (const Fila &f : filas)
    for (size_t j = 0; j < f.segs.size(); j++)
      wNum[j] = std::max(wNum[j], textWidth(f.segs[j].num, tamN)), wUni[j] = std::max(wUni[j], textWidth(f.segs[j].uni, tamF));
  float wCols = 0.0f;
  for (size_t j = 0; j < nCol; j++) wCols += wNum[j] + 4.0f + wUni[j] + (j + 1 < nCol ? hueco : 0.0f);
  const float bw = std::max(borde + wRot + 14.0f + wCols + borde, borde + textWidth(titulo, 10.0f) + borde);
  const float paso = compacta ? 16.0f : 22.0f, y1 = compacta ? 20.0f : 28.0f;
  const float alto = std::max(ph, y1 + paso * (float)filas.size() + (compacta ? 4.0f : 6.0f));
  const float bx = xDer - bw;
  if (bx < 8.0f) return;
  /* (v1.16) del mismo alto y color que la pastilla, pegado arriba (v1.17: mas alto si hay mas filas) */
  mask9({bx, y0 - 8.0f, bw + (orilla ? 8.0f : 0.0f), alto + 8.0f}, 1, 8.0f, Hud::canal(pal_.barra, 0), Hud::canal(pal_.barra, 1),
        Hud::canal(pal_.barra, 2), 0.94f);
  cuadroFps_ = {bx, y0, bw + (orilla ? 8.0f : 0.0f), alto};   /* (v1.18) su clic derecho: la generacion de cuadros */
  text(titulo, bx + borde, y0 + (compacta ? 5.0f : 9.0f), 10.0f, accent[0], accent[1], accent[2], 1.0f);
  for (size_t i = 0; i < filas.size(); i++) {
    /* una sola fila (los FPS sin DLSS), a media altura como antes */
    const float y = filas.size() == 1 ? y0 + (compacta ? 27.0f : 39.0f) : y0 + y1 + paso * (float)i;
    text(filas[i].rot, bx + borde, y + 1.5f, tamR, dim[0], dim[1], dim[2], dim[3]);
    float x = bx + bw - borde - wCols;
    for (size_t j = 0; j < nCol; j++) {
      if (j < filas[i].segs.size()) {
        const Seg &sg = filas[i].segs[j];
        text(sg.num, x + wNum[j] - textWidth(sg.num, tamN), y, tamN, ink[0], ink[1], ink[2], ink[3]);
        text(sg.uni, x + wNum[j] + 4.0f, y + 3.0f, tamF, dim[0], dim[1], dim[2], dim[3]);
      }
      x += wNum[j] + 4.0f + wUni[j] + hueco;
    }
  }
}

/* (v1.18) el menu de la generacion de cuadros, debajo del cuadrito de los FPS y con su orilla derecha: como el de vkDaedalus,
   el perfil de MAKO de este juego (ver mako.h). Cinco filas de botones; con algo que no deja (sin MAKO, sin juego), en gris
   y el pie dice por que */
void Hud::dibujaMenuFg(const float accent[3], const float ink[4], const float dim[4]) {
  for (auto &fila : fgOpc_)
    for (Rect &r : fila) r = {-10.0f, -10.0f, 0.0f, 0.0f};
  menuFg_ = {-10.0f, -10.0f, 0.0f, 0.0f};
  if (!st_.menuFg || cuadroFps_.w <= 0.0f) return;
  const State::Fg &g = st_.fg;
  const bool vale = g.mako && !g.exe.empty();
  struct Fila {
    std::string rot;
    std::vector<std::string> opc;
    int elegida;
  };
  static const int kFlujo[4] = {50, 75, 85, 100};
  int flujo = 0;
  for (int k = 1; k < 4; k++)
    if (std::abs(g.flujo - kFlujo[k]) < std::abs(g.flujo - kFlujo[flujo])) flujo = k;
  /* (v1.18) sin fila de tarjeta: MAKO no genera en otra que la del juego («Cross-GPU Frame Generation is unsupported», sus
     documentos). La del modelo: LSFG (el de Lossless Scaling) o MAKO FG (el suyo), cada uno solo si esta */
  const std::vector<Fila> filas = {
      {idioma::tr("Generación"), {idioma::tr("Apagada"), idioma::tr("Encendida")}, g.puesta ? 1 : 0},
      {idioma::tr("Modelo"), {"LSFG", "MAKO FG"}, g.modelo},
      {idioma::tr("Multiplicador"), {"x2", "x3", "x4"}, g.multiplicador - 2},
      {idioma::tr("Flujo"), {"50 %", "75 %", "85 %", "100 %"}, flujo},
      {idioma::tr("Modo rendimiento"), {idioma::tr("No"), idioma::tr("Sí")}, g.rendimiento ? 1 : 0}};
  auto disponible = [&](size_t f, size_t k) { return f != 1 || (k == 0 ? g.lsfg : g.generador); };
  const float mw = 440.0f, pad = 16.0f, filaAlto = 38.0f, rotW = 150.0f, tam = 12.0f;
  /* (v1.18) sin MAKO FG, su boton debajo de las opciones (Rodolfo: «offer an install button»): lo baja y lo instala sin root
     (ver mako.h); mientras baja, el mismo boton es la barra de lo que lleva */
  const bool botonInstalar = g.mako && (!g.generador || g.instalando);
  const size_t nFilas = filas.size() + (botonInstalar ? 1 : 0);
  std::string pie = g.aviso;
  if (pie.empty()) {
    if (!g.mako) pie = idioma::tr("MAKO no está instalado: la generación de cuadros es su capa de Vulkan (mako-render).");
    else if (g.instalando)
      pie = idioma::tr("Bajando MAKO FG V1.24 de Hugging Face: cada archivo se comprueba (SHA-256) antes de ponerlo. No pide root.");
    else if (g.exe.empty()) pie = idioma::tr("Sin un juego debajo: la generación de cuadros va en el juego, no en el vídeo.");
    else if (!g.generador && !g.lsfg) pie = idioma::tr("MAKO no tiene con qué generar: instala MAKO FG con el botón (no pide root) o pon Lossless Scaling en Steam.");
    else if (g.modelo == 1 && !g.generador) pie = idioma::tr("MAKO FG no está instalado: instálalo con el botón (no pide root); mientras, LSFG ya genera.");
    else if (g.modelo == 0 && !g.lsfg) pie = idioma::tr("Lossless Scaling no está en Steam: elige MAKO FG.");
    else if (!g.enJuego) pie = idioma::tr("MAKO no está cargado en este juego: pon ENABLE_MAKO=1 en Integraciones. Lo que elijas se guarda para cuando lo esté.");
    else if (g.modelo == 1 && !g.generadorJuego)
      pie = idioma::tr("MAKO FG está instalado, pero este juego se abrió sin él: vale al volver a abrirlo (Integraciones se lo da con Steam cerrado).");
    else pie = idioma::tr("Se guarda en el perfil de MAKO de este juego y vale en marcha; cambiar de modelo, al volver a abrir el juego.");
    if (botonInstalar && !g.instalando)
      pie += std::string(" ") + idioma::tr("MAKO FG V1.24 es de uso no comercial (licencia CC BY-NC 4.0).");
  }
  const std::vector<std::string> lineasPie = renglones(pie, 11.5f, mw - 2.0f * pad);
  const float alto = 40.0f + filaAlto * (float)nFilas + 6.0f + 16.0f * (float)lineasPie.size() + 12.0f;
  /* con el cuadrito pegado a la orilla (la pastilla oculta), el menu se queda a 8 de ella: se ven sus esquinas */
  const float der = std::min(cuadroFps_.x + cuadroFps_.w, (float)W_ / sc_ - 8.0f);
  const float mx0 = std::max(8.0f, der - mw), my0 = cuadroFps_.y + cuadroFps_.h + 6.0f;
  menuFg_ = {mx0, my0, mw, alto};
  mask9(menuFg_, 1, 8.0f, Hud::canal(pal_.tarjeta, 0), Hud::canal(pal_.tarjeta, 1), Hud::canal(pal_.tarjeta, 2), 0.97f);
  solid(mx0, my0, mw, 2.0f, accent[0], accent[1], accent[2], 0.9f);
  text(idioma::tr("GENERACIÓN DE CUADROS · MAKO"), mx0 + pad, my0 + 13.0f, 11.0f, accent[0], accent[1], accent[2], 1.0f);
  if (!g.exe.empty()) {
    const float we = std::min(mw * 0.42f, textWidth(g.exe, 10.5f));
    textoCabe(g.exe, mx0 + mw - pad - we, my0 + 14.0f, 10.5f, we, dim[0], dim[1], dim[2], dim[3]);
  }
  for (size_t f = 0; f < filas.size() && f < 6; f++) {
    const float y = my0 + 38.0f + filaAlto * (float)f;
    textoCabe(filas[f].rot, mx0 + pad, y + 8.0f, tam, rotW - 8.0f, dim[0], dim[1], dim[2], dim[3]);
    const size_t n = std::min<size_t>(filas[f].opc.size(), 8);
    const float x0b = mx0 + pad + rotW, wTot = mw - pad - (x0b - mx0), bw = (wTot - 6.0f * (float)(n - 1)) / (float)n;
    for (size_t k = 0; k < n; k++) {
      Rect r{x0b + (float)k * (bw + 6.0f), y + 2.0f, bw, 28.0f};
      const bool puede = vale && disponible(f, k);
      fgOpc_[f][k] = puede ? r : Rect{-10.0f, -10.0f, 0.0f, 0.0f};
      const bool elegida = (int)k == filas[f].elegida, hov = puede && r.inside(mx_, my_);
      if (elegida && puede) mask9(r, 0, 4.0f, accent[0], accent[1], accent[2], 0.95f);
      else mask9(r, 0, 4.0f, 1.0f, 1.0f, 1.0f, !puede ? 0.04f : (hov ? 0.16f : 0.07f));
      const float a = !puede ? 0.40f : (elegida || hov ? 1.0f : 0.85f);
      textoCabe(filas[f].opc[k], r.x + bw * 0.5f, r.y + 7.0f, 12.0f, bw - 10.0f, elegida && puede ? 0.06f : ink[0],
                elegida && puede ? 0.10f : ink[1], elegida && puede ? 0.16f : ink[2], a, true);
    }
  }
  if (botonInstalar) {
    const float y = my0 + 38.0f + filaAlto * (float)filas.size();
    const Rect r{mx0 + pad, y + 2.0f, mw - 2.0f * pad, 28.0f};
    std::string rotulo;
    if (g.instalando) {
      const float a = std::max(0.0f, std::min(1.0f, g.avance));
      mask9(r, 0, 4.0f, 1.0f, 1.0f, 1.0f, 0.07f);
      if (a > 0.0f) mask9({r.x, r.y, std::max(8.0f, r.w * a), r.h}, 0, 4.0f, accent[0], accent[1], accent[2], 0.55f);
      rotulo = idioma::trf("Bajando MAKO FG… {} de {} MB", {std::to_string((long)std::lround(a * (float)g.megas)), std::to_string(g.megas)});
      textoCabe(rotulo, r.x + r.w * 0.5f, r.y + 7.0f, 12.0f, r.w - 10.0f, ink[0], ink[1], ink[2], 1.0f, true);
    } else {
      fgOpc_[5][0] = r;
      const bool hov = r.inside(mx_, my_);
      mask9(r, 0, 4.0f, accent[0], accent[1], accent[2], hov ? 1.0f : 0.85f);
      rotulo = idioma::trf("Instalar MAKO FG · {} MB", {std::to_string(g.megas)});
      textoCabe(rotulo, r.x + r.w * 0.5f, r.y + 7.0f, 12.0f, r.w - 10.0f, 0.06f, 0.10f, 0.16f, 1.0f, true);
    }
  }
  float yp = my0 + 38.0f + filaAlto * (float)nFilas + 6.0f;
  for (const std::string &l : lineasPie) {
    text(l, mx0 + pad, yp, 11.5f, dim[0], dim[1], dim[2], dim[3]);
    yp += 16.0f;
  }
}

bool Hud::cuadroFpsBounds(float *x, float *y, float *w, float *h) const {
  if (cuadroFps_.w <= 0.0f) return false;
  *x = cuadroFps_.x * sc_, *y = cuadroFps_.y * sc_, *w = cuadroFps_.w * sc_, *h = cuadroFps_.h * sc_;
  return true;
}

bool Hud::menuFgBounds(float *x, float *y, float *w, float *h) const {
  if (!st_.menuFg || menuFg_.w <= 0.0f) return false;
  *x = menuFg_.x * sc_, *y = menuFg_.y * sc_, *w = menuFg_.w * sc_, *h = menuFg_.h * sc_;
  return true;
}

void Hud::begin(int w, int h, float altoLogico) {
  v_.clear();
  imagenes_.clear();
  W_ = w;
  H_ = h;
  /* La interfaz se dibuja en un lienzo virtual y luego se escala al pixel real del panel.
     Con tamano automatico: la tarjeta del hub usa el alto logico del escritorio (aqui 1440, o
     sea x1,5) y el visor usa el alto fisico (2160, o sea x2), porque en el modo 3D el panel se
     queda con escala 1 y si el visor siguiera el tamano logico la pastilla saldria a media altura.
     Con --ui-escala o Ctrl+Shift+7 / 8 el tamano se fija a mano y manda sobre lo demas. */
  const float relacion = altoLogico > 0.0f ? (float)h / altoLogico : 0.0f;
  sc_ = scUser_ > 0.25f ? std::max(0.5f, std::min(3.0f, scUser_))
        : relacion > 0.25f ? std::max(1.0f, std::min(2.0f, relacion))
                           : std::max(1.0f, std::min(2.0f, (float)h / 1080.0f));
  const int vw = (int)std::lround((float)w / sc_), vh = (int)std::lround((float)h / sc_);
  if (!st_.overlayOn && !st_.ask && !st_.panel && !cuadroFps()) return;
  pal_ = paleta();   /* (v1.16) la del modo, para todo lo de este cuadro */

  /* v0.79: el acento es el morado del paisaje de la tarjeta (antes verde) y el fondo de la pastilla
     es el gris casi negro de la tarjeta: las dos piezas se ven de la misma familia. */
  /* rama experimental: la pastilla va en azul claro (el del chip «AI») y en ambar la de rendimiento (el
     color del rayo). La tarjeta de la orilla sigue en morado: se vuelve a poner antes de pintarla. */
  /* (v1.16) la pastilla con la paleta soft de vkDaedalus, como el resto (Rodolfo: «adapta el estilo cuadrado al del
     resto de la aplicacion»): su morado con la red y su ambar en el modo rendimiento */
  /* el azul de la cápsula Neural para la red (Rodolfo: «el boton de encendido no tiene el azul de neural»: el encendido y
     la capsula van del mismo) y el ambar de vkDaedalus en el modo rendimiento */
  float accent[3] = {st_.rendimiento ? 0xDE / 255.0f : 0.40f, st_.rendimiento ? 0xAA / 255.0f : 0.72f,
                     st_.rendimiento ? 0x7C / 255.0f : 1.0f};
  float ink[4] = {0.93f, 0.94f, 0.96f, 1.0f};
  float dim[4] = {0.68f, 0.70f, 0.74f, 0.90f};

  /* ---- pastilla superior derecha (v0.78: una sola fila) ----
     Lo que se toca de verdad mientras se ve un video: el interruptor del 3D, los dos deslizadores y,
     si el video entro por la tarjeta de la orilla, la casilla de «no volver a preguntar». Salieron
     el boton de la red (se usa siempre la de Samsung, que es la que mejor mide; las teclas 1 / 2 la
     siguen cambiando), la fila de Facetracking (informaba, no mandaba: si la camara falla lo dice
     la linea de abajo a la izquierda), el letrero de la lente encima de la pastilla y los avisos
     de teclas repartidos, que ahora caben en un renglon. Sigue RECTA, como la pidio Rodolfo. */
  chkRecordar_ = {-10.0f, -10.0f, 0.0f, 0.0f};
  btnEngranaje_ = {-10.0f, -10.0f, 0.0f, 0.0f};
  btnNormal_ = btnDividido_ = btnSuave_ = {-10.0f, -10.0f, 0.0f, 0.0f};
  for (Rect &b : btnPre_) b = {-10.0f, -10.0f, 0.0f, 0.0f};
  btnRayo_ = menuRend_ = btnFoco_ = btnAM_[0] = btnAM_[1] = {-10.0f, -10.0f, 0.0f, 0.0f};
  for (int k = 0; k < 4; k++) rendTrack_[k] = rendFila_[k] = {-10.0f, -10.0f, 0.0f, 0.0f};
  btnRed_ = {-10.0f, -10.0f, 0.0f, 0.0f};   /* ya no hay boton de red */
  cuadroFps_ = menuFg_ = {-10.0f, -10.0f, 0.0f, 0.0f};   /* (v1.18) */
  /* (v1.17) la pastilla oculta: el cuadrito de los FPS se queda, en la esquina */
  if (!st_.overlayOn && !st_.panel && cuadroFps()) {
    fuente_ = hayBarlow_ ? 1 : 0;
    dibujaCuadroFps((float)vw, st_.legado ? 76.0f : 56.0f, !st_.legado, true, accent, ink, dim);
    dibujaMenuFg(accent, ink, dim);
  }
  if (st_.overlayOn) {
    const bool leg = st_.legado;
    fuente_ = hayBarlow_ ? 1 : 0;   /* (v1.16) la letra del resto de la aplicacion */
    const float aire = 16.0f, anchoDes = 140.0f;
    const char *rotRec = idioma::tr("No volver a preguntar");
    const float anchoRec = 18.0f + 8.0f + textWidth(rotRec, 12.5f);
    /* el bloque de la derecha: arriba los dos botones de icono (que tipo de 3D), debajo la casilla.
       Mide lo que la casilla con su letra; sin casilla, lo justo para los dos botones. */
    /* el hub del modo normal (rama experimental): conversor con la red. Como el de rendimiento: el
       interruptor, un boton con el chip «AI» que dice Auto o Manual y su menu, y el engranaje. Los
       deslizadores, el tipo de video y la casilla pasan al menu. */
    /* (v1.12) la pastilla de Profundidad y Salida se fue («el tercero ya no», Rodolfo: los deslizadores
       ya estan en el menu de Manual): fuera del modo de legado, la pastilla es siempre esta. Sin conversor
       (video dividido tal cual, o sin red) no hay Auto que valga: el boton dice Manual y su menu va directo
       a los deslizadores. */
    const bool hubNormal = !leg && !st_.rendimiento;
    const bool conAuto = st_.focoVisible;
    const bool autoPuesto = st_.focoAuto && conAuto;
    const std::string nombreFoco = idioma::tr(autoPuesto ? "Auto" : "Manual");
    const bool hayModo = st_.modoVisible && !leg && !st_.rendimiento && !hubNormal;
    /* el modo rendimiento: un boton con el rayo, el preajuste puesto y la flechita de su menu */
    const char *nombrePre[3] = {idioma::tr("Suave"), idioma::tr("Medio"), idioma::tr("Intenso")};
    const std::string nombreRend = st_.preajuste >= 0 && st_.preajuste < 3 ? nombrePre[st_.preajuste] : idioma::tr("A mano");
    float anchoRendimiento = 0.0f;
    /* (v1.16) la segunda capsula, sin icono (Rodolfo: «solo manten los iconos de la izquierda, no los repitas en la
       segunda capsula»): el texto y la flechita */
    if (st_.rendimiento) anchoRendimiento = 14.0f + textWidth(nombreRend, 13.0f) + 10.0f + 12.0f + 12.0f;
    /* v1.07: en chino o japones la casilla es corta y los dos botones no cabian: el bloque mide al menos 128 */
    const float anchoBloque = st_.rendimiento ? anchoRendimiento
                              : hubNormal ? 14.0f + textWidth(nombreFoco, 13.0f) + 10.0f + 12.0f + 12.0f
                              : (st_.recordarVisible ? std::max(anchoRec, hayModo ? 128.0f : 0.0f) : (hayModo ? 128.0f : 0.0f));
    /* (v1.12) a la izquierda, el boton redondo de encendido (antes el interruptor ON/OFF) y, en la pastilla
       del video normal (con la red o en rendimiento), el interruptor Neural | Rendimiento del panel: el
       modo se cambia ahi mismo, en marcha. Mide lo mismo en los dos extremos, como en el panel. */
    const bool hubConv = (hubNormal || st_.rendimiento) && !leg;
    const float anchoEnc = 32.0f;
    const char *nNeural = idioma::tr("Neural"), *nRendModo = idioma::tr("Rendimiento");
    const float wModo = std::max(28.0f, std::max(textWidth(nNeural, 13.0f), textWidth(nRendModo, 13.0f)));
    const float anchoModo = hubConv ? 4.0f + 26.0f + 10.0f + wModo + 14.0f : 0.0f;
    const float anchoSw = anchoEnc + (hubConv ? 12.0f + anchoModo : 0.0f);
    /* en el modo rendimiento y en el hub normal no hay deslizadores en la pastilla: el boton va junto al
       interruptor */
    float pw = (st_.rendimiento || hubNormal) ? aire + anchoSw + aire
                                              : aire + anchoSw + 26.0f + anchoDes + 24.0f + anchoDes + aire;
    if (anchoBloque > 0.0f) pw += 26.0f + anchoBloque;
    if (st_.engranaje) pw += 14.0f + 30.0f;   /* el engranaje de los ajustes, al final */
    /* (v1.16) los dos botones del tipo de video, junto al engranaje (Rodolfo: «pon los dos botones junto a la
       herramienta»), en la pastilla con la red y en la de rendimiento; antes iban en el menu Relieve */
    const bool modosEnPastilla = (hubNormal || st_.rendimiento) && !leg && st_.modoVisible;
    if (modosEnPastilla) pw += 2.0f * 44.0f + 6.0f + 12.0f;
    /* las teclas, en un renglon. Si la pastilla sale angosta, la letra se achica en vez de salirse. */
    /* en el modo rendimiento las teclas de Profundidad y Salida no hacen nada: no salen */
    const std::string teclas =
        (st_.rendimiento || (hubNormal && autoPuesto))
            ? (!st_.teclasRendimiento.empty() ? st_.teclasRendimiento
                                              : std::string(idioma::tr("Ctrl+Shift+  1 el 3D · F1 oculta · Esc sale")))
        : !st_.teclas.empty() ? st_.teclas
                              : idioma::trf("Ctrl+Shift+  1 el 3D · 3/4 {} · 5/6 salida · F1 oculta · Esc sale",
                                            {idioma::tr(leg ? "plano" : "profundidad")});
    /* la pastilla compacta (rendimiento, hub normal) se ensancha lo justo para que el renglon se lea */
    if (st_.rendimiento || hubNormal)
      pw = std::max(pw, 2.0f * aire + textWidth(teclas, 9.5f) + (st_.engranaje ? 44.0f : 0.0f));

    pw = std::min(pw, (float)vw - 48.0f);
    /* (v1.16) del alto de la barra del panel (56), salvo el modo de legado, que lleva deslizadores (Rodolfo:
       «integralo a la barra de titulo en cuanto a tamaño, el ancho dejalo asi»); el renglon de las teclas pasa a la
       ayuda que sale al pasar el raton */
    const bool compacta = !leg;
    const float ph = compacta ? 56.0f : 76.0f;
    const float dy = compacta ? -5.0f : 0.0f;   /* lo de dentro, centrado en el alto nuevo */
    panelW_ = pw;
    panelH_ = ph;
    /* (v1.16) otra vez arriba a la derecha (Rodolfo: «devuelve el pill a la orilla superior derecha»), pegada a las
       dos orillas como un trozo de barra de titulo: del color de la barra del panel, con la esquina de abajo a la
       izquierda redonda. Sus menus se abren hacia abajo */
    const float x0 = (float)vw - pw, y0 = 0.0f;
    pillPanel_ = {x0, y0, pw, ph};
    mask9({x0, y0 - 8.0f, pw + 8.0f, ph + 8.0f}, 1, 8.0f, Hud::canal(pal_.barra, 0), Hud::canal(pal_.barra, 1), Hud::canal(pal_.barra, 2), 0.94f);

    /* (v1.13) el cuadrito de DLSS a la izquierda de la pastilla (Rodolfo: «junto al HUB quiero que en un cuadro
       pequeño indiques los FPS del layer y los del juego»): los cuadros que presenta el juego y las profundidades
       que manda la capa por segundo (con la generacion de cuadros, los del juego son el doble). Solo se mira: queda
       fuera de la region de entrada y el raton lo atraviesa. `fps_juego = no` en los ajustes lo quita. */
    /* (v1.13) sin DLSS, con la red sobre un juego, el mismo cuadrito dice «Modelo de serie» y solo los FPS del juego
       (los cuadros nuevos que llegan; Rodolfo: «en los fps que no son de dlss solo deja los del juego»), en un
       renglon a media altura */
    /* (v1.18) ya no junto a la pastilla: solo con ella oculta, en la esquina (Rodolfo: «only show fps overlay once the hub
       pill is hidden»). Con la pastilla a la vista, ni el cuadrito ni su menu */

    /* ---- el encendido del 3D (v1.12) ----
       Un boton redondo (antes el interruptor ON/OFF, «en vez de on y off un boton redondo de apagado»,
       Rodolfo): encendido, del color de la pastilla con el simbolo en blanco; apagado, gris. Manda el
       mismo golpe que el interruptor (Hit3D). */
    sw3D_ = {x0 + aire, y0 + 17.0f + dy, anchoEnc, 32.0f};
    swKnob_ = {-10.0f, -10.0f, 0.0f, 0.0f};
    {
      const bool hov3 = sw3D_.inside(mx_, my_);
      const Rect ico{sw3D_.x + 6.0f, sw3D_.y + 6.0f, 20.0f, 20.0f};
      if (st_.weaveOn) {
        const float t = hov3 ? 1.12f : 1.0f;
        mask9(sw3D_, 0, 4.0f, std::min(1.0f, accent[0] * t), std::min(1.0f, accent[1] * t), std::min(1.0f, accent[2] * t), 0.95f);
        icono(ico, icoEncender_, 1.0f, 1.0f, 1.0f, 1.0f);
      } else {
        const float g = hov3 ? 0.32f : 0.24f;
        mask9(sw3D_, 0, 4.0f, g, g + 0.02f, g + 0.05f, 0.95f);
        icono(ico, icoEncender_, 0.80f, 0.82f, 0.86f, hov3 ? 1.0f : 0.85f);
      }
    }
    /* ---- el interruptor Neural | Rendimiento (v1.12), el mismo del panel ----
       Azul con el chip a la izquierda (con la red) o ambar con el rayo a la derecha (sin red); un clic en
       cualquier parte lo pasa al otro extremo, en marcha (HitModoRed). */
    pillModo_ = {-10.0f, -10.0f, 0.0f, 0.0f};
    if (hubConv) {
      pillModo_ = {sw3D_.x + sw3D_.w + 12.0f, y0 + 17.0f + dy, anchoModo, 32.0f};
      const bool rend = st_.rendimiento, hovM = pillModo_.inside(mx_, my_);
      const float c[3] = {accent[0], accent[1], accent[2]};   /* (v1.16) el morado o el ambar de la paleta soft */
      const float t = hovM ? 1.08f : 1.0f;
      mask9(pillModo_, 0, 4.0f, std::min(1.0f, c[0] * t), std::min(1.0f, c[1] * t), std::min(1.0f, c[2] * t), 0.95f);
      const Rect bolaM{rend ? pillModo_.x + pillModo_.w - 4.0f - 26.0f : pillModo_.x + 4.0f, pillModo_.y + 3.0f, 26.0f, 26.0f};
      mask9(bolaM, 0, 4.0f, 0.97f, 0.97f, 0.99f, 1.0f);
      icono({bolaM.x + 4.0f, bolaM.y + 4.0f, 18.0f, 18.0f}, rend ? icoRayo_ : icoChip_, c[0] * 0.85f, c[1] * 0.85f, c[2] * 0.85f, 1.0f);
      const char *nombreModo = rend ? nRendModo : nNeural;
      const float huecoM = wModo - textWidth(nombreModo, 13.0f);
      const float xtM = (rend ? pillModo_.x + 14.0f : bolaM.x + bolaM.w + 10.0f) + 0.5f * huecoM;
      text(nombreModo, xtM, pillModo_.y + 8.0f, 13.0f, 0.08f, 0.09f, 0.11f, 1.0f);
    }

    auto slider = [&](Rect *track, Rect *knob, const char *label, float frac, const std::string &value) {
      const float tx = track->x, ty = track->y;
      text(label, tx, ty - 23.0f, 12.0f, dim[0], dim[1], dim[2], dim[3]);
      text(value, tx + track->w - textWidth(value, 12.0f), ty - 23.0f, 12.0f, ink[0], ink[1], ink[2], ink[3]);
      solid(tx, ty, track->w, 4.0f, 0.30f, 0.32f, 0.35f, 0.9f);
      const float kx = tx + frac * track->w;
      solid(tx, ty, kx - tx, 4.0f, accent[0], accent[1], accent[2], 0.95f);
      const float kr = 8.0f;   /* cabecita cuadrada, como el resto de la pastilla */
      solid(kx - kr, ty + 2.0f - kr, 2.0f * kr, 2.0f * kr, 0.95f, 0.96f, 0.98f, 1.0f);
      *knob = {kx - kr, ty + 2.0f - kr, 2.0f * kr, 2.0f * kr};
    };
    /* En modo de legado el de la izquierda es el plano que se queda quieto en la pantalla (lo que la
       ventana de ajustes llama `apoyo`); con el conversor es cuanta profundidad saca la red. */
    if (!st_.rendimiento && !hubNormal) {
      depTrack_ = {sw3D_.x + anchoSw + 26.0f, y0 + 41.0f, anchoDes, 4.0f};
      popTrack_ = {depTrack_.x + anchoDes + 24.0f, y0 + 41.0f, anchoDes, 4.0f};
      slider(&depTrack_, &depKnob_, leg ? idioma::tr("Plano de apoyo") : idioma::tr("Profundidad"), (float)st_.depth / 100.0f,
             std::to_string(st_.depth));
      slider(&popTrack_, &popKnob_, idioma::tr("Salida"), (float)(st_.popout + 50) / 100.0f, std::to_string(st_.popout));
    } else {
      depTrack_ = popTrack_ = depKnob_ = popKnob_ = {-10.0f, -10.0f, 0.0f, 0.0f};
    }

    const float bloqueX = (st_.rendimiento || hubNormal) ? sw3D_.x + anchoSw + 26.0f : popTrack_.x + anchoDes + 26.0f;
    const float sitioEngranaje = st_.engranaje ? 44.0f : 0.0f;
    const bool cabeBloque = anchoBloque > 0.0f && bloqueX + anchoBloque + sitioEngranaje + aire <= x0 + pw + 0.5f;
    /* ---- el engranaje: abre el panel de ajustes por encima del video (v0.84) ---- */
    if (st_.engranaje) {
      btnEngranaje_ = {x0 + pw - aire - 30.0f, y0 + (ph - 30.0f) * 0.5f, 30.0f, 30.0f};
      const bool hov = btnEngranaje_.inside(mx_, my_);
      if (hov) mask9(btnEngranaje_, 0, 4.0f, 1.0f, 1.0f, 1.0f, 0.14f);
      icono({btnEngranaje_.x + 4.0f, btnEngranaje_.y + 4.0f, 22.0f, 22.0f}, icoEngranaje_, ink[0], ink[1], ink[2],
            hov ? 1.0f : 0.78f);
    }

    /* ---- los dos botones de icono: video normal y video dividido ----
       Sin letra, con el dibujo de las teselas de la tarjeta. El que esta puesto sale encendido y
       con filo; el otro, apagado. Se pica el otro y el visor cambia de tipo de 3D sin salir.
         - una imagen entera = video normal (el conversor le pone el relieve)
         - el otro es el video dividido. De normal va SUAVIZADO (una imagen con la raya a trazos en
           medio: se usa una sola de sus dos imagenes y el relieve lo pone el conversor); si en los
           ajustes del programa se quito el suavizado, lleva las dos imagenes separadas y teje el
           video con el relieve que trae. Desde v0.84 no hay tercer boton: el suavizado solo se
           quita en los ajustes (asi lo pidio Rodolfo). */
    /* (v1.16) el dibujo llena el boton entero, sin el filo blanco, con las esquinas de 4 del encendido (Rodolfo: «make
       them fill their square and remove the white square border, make the borders as the on and off button one»):
       el puesto, encendido; el otro, apagado como el encendido sin 3D. `bajo` es el color de detras (la barra de la
       pastilla o la tarjeta del menu), para redondear las esquinas */
    auto dibujaModos = [&](Rect b0, Rect b1, const float *bajo) {
      btnNormal_ = b0;
      btnDividido_ = b1;
      const Rect bs[2] = {btnNormal_, btnDividido_};
      for (int i = 0; i < 2; i++) {
        const Rect &b = bs[i];
        const bool puesto = i == 0 ? st_.modo3d == 1 : st_.modo3d >= 2, hov = b.inside(mx_, my_);
        const float luz = puesto ? 1.0f : (hov ? 0.62f : 0.42f);
        /* el dibujo del segundo sigue a lo que esta puesto de verdad si ya es un dividido (2 = tal
           cual, 3 = suavizado); si no, a lo que haria al picarlo */
        const bool suave = st_.modo3d == 3 || (st_.modo3d != 2 && st_.suavizado);
        if (i == 0 || suave) {
          paisaje(b, 1.0f, luz, 4.0f, bajo, accent);
          if (i == 1)   /* la imagen entera con la raya del corte a trazos: viene partida, se ve de una pieza */
            for (int k = 0; k < 4; k++)
              solid(b.x + b.w * 0.5f - 1.0f, b.y + 3.0f + (float)k * (b.h - 6.0f) / 4.0f, 2.0f, (b.h - 6.0f) / 8.0f + 1.5f, luz, luz, luz, 1.0f);
        } else {
          const float mitad = (b.w - 3.0f) * 0.5f;
          paisaje({b.x, b.y, mitad, b.h}, 0.5f, luz, 4.0f, bajo, accent);
          paisaje({b.x + mitad + 3.0f, b.y, mitad, b.h}, 0.5f, luz, 4.0f, bajo, accent);
        }
      }
    };
    if (modosEnPastilla) {
      const float bw = 44.0f, bh = 32.0f, by = y0 + (ph - bh) * 0.5f;   /* del alto del encendido */
      const float derM = st_.engranaje ? btnEngranaje_.x - 12.0f : x0 + pw - aire;
      const float barra[3] = {Hud::canal(pal_.barra, 0), Hud::canal(pal_.barra, 1), Hud::canal(pal_.barra, 2)};
      dibujaModos({derM - 2.0f * bw - 6.0f, by, bw, bh}, {derM - bw, by, bw, bh}, barra);
    }
    if (cabeBloque && hayModo) {
      const float bw = (anchoBloque - 8.0f) * 0.5f, bh = 30.0f;
      const float by = st_.recordarVisible ? y0 + 9.0f : y0 + 18.0f;
      const float barra[3] = {Hud::canal(pal_.barra, 0), Hud::canal(pal_.barra, 1), Hud::canal(pal_.barra, 2)};
      dibujaModos({bloqueX, by, bw, bh}, {bloqueX + bw + 8.0f, by, bw, bh}, barra);
    }

    /* ---- el modo rendimiento: el boton del rayo (rama experimental) ----
       Dice el preajuste puesto («A mano» si los valores no son los de ninguno) y despliega debajo
       de la pastilla el menu del modo. Abierto, va relleno de ambar. */
    if (cabeBloque && st_.rendimiento) {
      const float by = y0 + 18.0f + dy, bh = 30.0f;
      btnRayo_ = {bloqueX, by, anchoBloque, bh};
      const bool abierto = st_.menuRendimiento, hov = btnRayo_.inside(mx_, my_);
      if (abierto) mask9(btnRayo_, 0, 4.0f, accent[0], accent[1], accent[2], 0.95f);
      else mask9(btnRayo_, 0, 4.0f, Hud::canal(hov ? pal_.controlHov : pal_.control, 0), Hud::canal(hov ? pal_.controlHov : pal_.control, 1), Hud::canal(hov ? pal_.controlHov : pal_.control, 2), 1.0f);
      const float t[3] = {abierto ? 0.10f : ink[0], abierto ? 0.08f : ink[1], abierto ? 0.04f : ink[2]};
      text(nombreRend, bloqueX + 14.0f, by + 6.5f, 13.0f, t[0], t[1], t[2], abierto || hov ? 1.0f : 0.90f);
      icono({btnRayo_.x + btnRayo_.w - 24.0f, by + 9.0f, 12.0f, 12.0f}, icoBajar_, t[0], t[1], t[2], abierto || hov ? 1.0f : 0.80f);
    }
    /* ---- el hub normal: el boton del chip «AI» (rama experimental) ----
       El chip solo, en su trazo azul claro (sin fondo propio), «Auto» o «Manual» y la flechita del menu.
       Abierto, el boton se tiñe del azul en vez de rellenarse. */
    if (cabeBloque && hubNormal) {
      const float by = y0 + 18.0f + dy, bh = 30.0f;
      btnFoco_ = {bloqueX, by, anchoBloque, bh};
      const bool abierto = st_.menuNormal, hov = btnFoco_.inside(mx_, my_);
      if (abierto) mask9(btnFoco_, 0, 4.0f, accent[0], accent[1], accent[2], hov ? 0.36f : 0.28f);
      else mask9(btnFoco_, 0, 4.0f, Hud::canal(hov ? pal_.controlHov : pal_.control, 0), Hud::canal(hov ? pal_.controlHov : pal_.control, 1), Hud::canal(hov ? pal_.controlHov : pal_.control, 2), 1.0f);
      /* (v1.16) sin el chip ni la llave: el icono va solo en la capsula de la izquierda */
      text(nombreFoco, bloqueX + 14.0f, by + 6.5f, 13.0f, ink[0], ink[1], ink[2], abierto || hov ? 1.0f : 0.90f);
      icono({btnFoco_.x + btnFoco_.w - 24.0f, by + 9.0f, 12.0f, 12.0f}, icoBajar_, ink[0], ink[1], ink[2], abierto || hov ? 1.0f : 0.80f);
    }

    /* ---- «no volver a preguntar», dentro del video ----
       Es la misma casilla de la tarjeta de la orilla. Marcada, esta aplicacion pasa directa al 3D la
       proxima vez (con el tipo que este puesto arriba); se desmarca aqui mismo y el servicio vuelve
       a preguntar. Solo sale cuando el video llego por el servicio, que es quien guarda la eleccion. */
    auto dibujaRecordar = [&](Rect caja) {
      chkRecordar_ = {caja.x - 6.0f, caja.y - 5.0f, anchoRec + 12.0f, caja.h + 10.0f};
      const bool hov = chkRecordar_.inside(mx_, my_);
      const float c = hov ? 1.0f : 0.86f;
      solid(caja.x, caja.y, caja.w, caja.h, c, c, c + 0.02f, 1.0f);
      if (st_.recordar)
        icono({caja.x + 2.0f, caja.y + 2.0f, caja.w - 4.0f, caja.h - 4.0f}, icoPalomita_, 0.07f, 0.08f, 0.10f, 1.0f);
      else
        solid(caja.x + 2.0f, caja.y + 2.0f, caja.w - 4.0f, caja.h - 4.0f, 0.03f, 0.03f, 0.035f, 1.0f);
      text(rotRec, caja.x + 26.0f, caja.y + (caja.h - 16.0f) * 0.5f, 12.5f, ink[0], ink[1], ink[2], hov ? 1.0f : 0.88f);
    };
    if (cabeBloque && st_.recordarVisible && !st_.rendimiento && !hubNormal)
      dibujaRecordar(hayModo ? Rect{bloqueX, y0 + 49.0f, 16.0f, 16.0f} : Rect{bloqueX, y0 + 24.0f, 18.0f, 18.0f});

    float cabe = pw - 2.0f * aire - sitioEngranaje;
    /* con la casilla de «no volver a preguntar» debajo de los botones, el renglon no pasa de ella
       (v1.09: con atajos cambiados el renglon es largo y se metia debajo de su letra) */
    if (cabeBloque && st_.recordarVisible && !st_.rendimiento && !hubNormal) cabe = std::min(cabe, bloqueX - 14.0f - (x0 + aire));
    const float tamTeclas = std::min(10.5f, 10.5f * cabe / std::max(1.0f, textWidth(teclas, 10.5f)));
    if (!compacta) text(teclas, x0 + aire, y0 + 58.0f, tamTeclas, dim[0], dim[1], dim[2], 0.72f);
    /* (v1.16) en la pastilla de 56 las teclas salen al pasar el raton por ella (sin un menu abierto) */
    /* (v1.16) con el aviso del momento colgado debajo de la pastilla («vídeo normal» al picar su boton), la ayuda va
       debajo de el y no encima (Rodolfo, con una foto de las dos montadas: «correct this overlap») */
    const float bajoAviso = !info_.empty() && st_.overlayOn ? 30.0f : 0.0f;
    if (compacta && pillPanel_.inside(mx_, my_) && !st_.menuNormal && !st_.menuRendimiento &&
        !(pillModo_.w > 0.0f && pillModo_.inside(mx_, my_))) {
      const std::vector<std::string> lineas = {teclas};
      const float wT = textWidth(teclas, 12.5f) + 24.0f;
      cajaAyuda(lineas, std::max(8.0f, x0 + pw - 8.0f - wT), y0 + ph + 8.0f + bajoAviso, 12.5f);
    }
    /* (v1.12) la ayuda del interruptor Neural | Rendimiento de la pastilla, como la del panel */
    if (pillModo_.w > 0.0f && pillModo_.inside(mx_, my_) && !st_.menuNormal && !st_.menuRendimiento) {
      const std::vector<std::string> lineas = renglones(
          idioma::tr(st_.rendimiento ? "La profundidad sale de la propia imagen en el procesador (~9 ms por mapa), sin tocar la tarjeta: más basta, pero más estable. La pastilla lleva un rayo con su menú (los preajustes y los ajustes finos), y la frecuencia variable queda como en el escritorio."
                                     : "La red Depth Anything: la forma de verdad de cada objeto. Ocupa la tarjeta: ~1,1 GB y 14-18 ms por mapa."),
          12.5f, 420.0f);
      cajaAyuda(lineas, std::min(pillModo_.x, (float)vw - 24.0f - 440.0f), pillPanel_.y + pillPanel_.h + 8.0f + bajoAviso, 12.5f);
    }

    /* ---- el menu del rayo (rama experimental) ----
       Cuelga debajo de la pastilla, pegado a su orilla derecha: los tres preajustes y los cuatro
       deslizadores finos del modo rendimiento, que se ponen en marcha al moverlos. Al pasar el raton
       por un preajuste o por una fila, a su izquierda sale la caja con lo que cambia. Recto, como la
       pastilla. */
    if (st_.rendimiento && st_.menuRendimiento && btnRayo_.w > 0.0f) {
      const float mw = std::min(400.0f, (float)vw - 48.0f), pad = 16.0f, filaAlto = 50.0f;
      const char *nota = idioma::tr("Volver a la red: el interruptor Neural | Rendimiento del panel");
      const float mh = 78.0f + 4.0f * filaAlto + 26.0f;
      const float mx0 = x0 + pw - mw, my0 = y0 + ph + 6.0f;   /* (v1.16) otra vez debajo, pegado a su orilla derecha */
      menuRend_ = {mx0, my0, mw, mh};
      mask9({mx0, my0, mw, mh}, 1, 8.0f, Hud::canal(pal_.tarjeta, 0), Hud::canal(pal_.tarjeta, 1), Hud::canal(pal_.tarjeta, 2), 0.97f);   /* (v1.16) la tarjeta de vkDaedalus */
      solid(mx0, my0, mw, 2.0f, accent[0], accent[1], accent[2], 0.9f);
      text(idioma::tr("MODO RENDIMIENTO"), mx0 + pad, my0 + 14.0f, 11.0f, accent[0], accent[1], accent[2], 1.0f);
      std::string ayuda;
      float ayudaY = 0.0f;
      /* los tres preajustes, en una fila */
      const std::string ayudaPre[3] = {
          idioma::tr("Mezcla 25 % · relieve x0,75 · topes x2 · paralaje x1,5. Hasta ~32 px entre los ojos y casi nada aplastado: para sesiones largas."),
          idioma::tr("Mezcla 35 % · relieve x1 · topes x2,75 · paralaje x2. Hasta ~43 px entre los ojos y casi nada aplastado: el de fábrica."),
          idioma::tr("Mezcla 50 % · relieve x1,5 · topes x2,75 · paralaje x3. Hasta ~52 px entre los ojos; un 20 % del cuadro toca el tope. Más 3D, cansa antes.")};
      const float bw = (mw - 2.0f * pad - 12.0f) / 3.0f;
      for (int k = 0; k < 3; k++) {
        btnPre_[k] = {mx0 + pad + (float)k * (bw + 6.0f), my0 + 36.0f, bw, 30.0f};
        const bool puesto = st_.preajuste == k, hov = btnPre_[k].inside(mx_, my_);
        if (puesto) mask9(btnPre_[k], 0, 4.0f, accent[0], accent[1], accent[2], 0.95f);
        else mask9(btnPre_[k], 0, 4.0f, 1.0f, 1.0f, 1.0f, hov ? 0.16f : 0.07f);
        textoCabe(nombrePre[k], btnPre_[k].x + bw * 0.5f, btnPre_[k].y + 7.0f, 12.5f, bw - 12.0f, puesto ? 0.10f : ink[0],
                  puesto ? 0.08f : ink[1], puesto ? 0.04f : ink[2], puesto || hov ? 1.0f : 0.85f, true);
        if (hov) ayuda = ayudaPre[k], ayudaY = btnPre_[k].y;
      }
      /* los cuatro deslizadores: mezcla, fuerza, alcance y paralaje (rangos en rendimiento.h) */
      const char *rotulo[4] = {idioma::tr("Mezcla: suelo ↔ imagen"), idioma::tr("Fuerza del relieve"),
                               idioma::tr("Alcance del relieve"), idioma::tr("Paralaje de la cabeza")};
      const std::string ayudaFila[4] = {
          idioma::tr("Hacia 0 %: un plano inclinado, estable, igual en cada cuadro. Hacia 100 %: lo cercano se separa más del fondo, pero también sale hacia delante lo nítido aunque esté lejos (los subtítulos)."),
          idioma::tr("Más fuerza = más 3D. Pero si la fuerza pasa del alcance, el relieve se aplasta contra el tope y se ve como recortes planos: súbelo junto con el alcance."),
          idioma::tr("Los topes por ojo (+12,67 / -9,98 px de fábrica), multiplicados. Más alcance deja más 3D sin aplastar; demasiado cansa la vista (unos 98 px entre los ojos ya molestaban)."),
          idioma::tr("Con `paralaje = 0,5` en serve.conf, x2 es la geometría real. Más exagera el efecto de ventana al mover la cabeza; menos lo calma.")};
      auto conComa = [](float v, int dec) {
        char b[32];
        std::snprintf(b, sizeof b, "%.*f", dec, (double)v);
        for (char *q = b; *q; q++)
          if (*q == '.') *q = idioma::decimal();
        return std::string(b);
      };
      for (int k = 0; k < 4; k++) {
        const float fy = my0 + 78.0f + (float)k * filaAlto;
        rendFila_[k] = {mx0, fy, mw, filaAlto};
        const rendimiento::Rango &r = rendimiento::kRangos[k];
        const float v = st_.rendValor[k];
        const float frac = std::max(0.0f, std::min(1.0f, (v - r.lo) / (r.hi - r.lo)));
        const std::string valor = k == 0 ? std::to_string((int)std::lround(v * 100.0f)) + " %"
                                         : "x" + conComa(v, k == 3 ? 1 : 2);
        const bool hov = rendFila_[k].inside(mx_, my_);
        if (hov) solid(mx0, fy, mw, filaAlto, 1.0f, 1.0f, 1.0f, 0.04f);
        const float vw2 = textWidth(valor, 12.0f);
        textoCabe(rotulo[k], mx0 + pad, fy + 6.0f, 12.0f, mw - 2.0f * pad - vw2 - 12.0f, dim[0], dim[1], dim[2], dim[3]);
        text(valor, mx0 + mw - pad - vw2, fy + 6.0f, 12.0f, ink[0], ink[1], ink[2], ink[3]);
        rendTrack_[k] = {mx0 + pad + 8.0f, fy + 32.0f, mw - 2.0f * pad - 16.0f, 4.0f};
        const Rect &t = rendTrack_[k];
        solid(t.x, t.y, t.w, 4.0f, 0.30f, 0.32f, 0.35f, 0.9f);
        const float kx = t.x + frac * t.w, kr = 8.0f;
        solid(t.x, t.y, kx - t.x, 4.0f, accent[0], accent[1], accent[2], 0.95f);
        solid(kx - kr, t.y + 2.0f - kr, 2.0f * kr, 2.0f * kr, 0.95f, 0.96f, 0.98f, 1.0f);
        if (hov && ayuda.empty()) ayuda = ayudaFila[k], ayudaY = fy;
      }
      textoCabe(nota, mx0 + pad, my0 + mh - 22.0f, 11.0f, mw - 2.0f * pad, dim[0], dim[1], dim[2], 0.70f);
      /* la caja de ayuda, a la izquierda del menu (o debajo, si a la izquierda no cabe) */
      if (!ayuda.empty()) {
        const float tam = 12.5f;
        const std::vector<std::string> lineas = renglones(ayuda, tam, 360.0f);
        float aw = 0.0f;
        for (const std::string &l : lineas) aw = std::max(aw, textWidth(l, tam));
        aw += 24.0f;
        const float ah = 14.0f + 19.0f * (float)lineas.size();
        float ax = mx0 - 10.0f - aw, ay = std::min(ayudaY, (float)vh - 12.0f - ah);
        if (ax < 12.0f) ax = std::max(12.0f, mx0 + mw - aw), ay = my0 + mh + 8.0f;
        cajaAyuda(lineas, ax, ay, tam);
      }
    }

    /* ---- el menu del hub normal (rama experimental) ----
       Cuelga debajo de la pastilla como el del rayo: Auto | Manual; en Manual, la Profundidad y la Salida;
       en Auto, lo que se calculo (el plano de la saliencia y la Salida que da el mayor relieve sin aplastar
       la imagen). Debajo, el tipo de video y la casilla de «no volver a preguntar». */
    if (hubNormal && st_.menuNormal && btnFoco_.w > 0.0f) {
      const float mw = std::min(400.0f, (float)vw - 48.0f), pad = 16.0f, filaAlto = 50.0f, tam = 12.0f;
      const float mx0 = x0 + pw - mw;   /* (v1.16) otra vez debajo de la pastilla, a su derecha */
      const bool manual = !autoPuesto;
      std::vector<std::string> nota;
      if (!manual)
        nota = renglones(idioma::tr("La saliencia elige el plano de la pantalla en cada escena; el alcance se ajusta a la distancia de tus ojos (0,3° de paralaje) y la salida se calcula para el mayor relieve sin aplastar la imagen contra él."),
                         tam, mw - 2.0f * pad);
      /* sin conversor no sale la fila Auto | Manual: los deslizadores empiezan arriba */
      const float filaAM = conAuto ? 42.0f : 0.0f;
      float alto = 36.0f + filaAM + (manual ? 2.0f * filaAlto : 18.0f * (float)nota.size() + 30.0f);
      if (st_.modoVisible && !modosEnPastilla) alto += 46.0f;
      if (st_.recordarVisible) alto += 32.0f;
      alto += 8.0f;
      const float my0 = y0 + ph + 6.0f;
      menuRend_ = {mx0, my0, mw, alto};
      mask9({mx0, my0, mw, alto}, 1, 8.0f, Hud::canal(pal_.tarjeta, 0), Hud::canal(pal_.tarjeta, 1), Hud::canal(pal_.tarjeta, 2), 0.97f);   /* (v1.16) la tarjeta de vkDaedalus */
      solid(mx0, my0, mw, 2.0f, accent[0], accent[1], accent[2], 0.9f);
      text(idioma::tr("RELIEVE"), mx0 + pad, my0 + 14.0f, 11.0f, accent[0], accent[1], accent[2], 1.0f);
      std::string ayuda;
      float ayudaY = 0.0f;
      const char *opc[2] = {idioma::tr("Auto"), idioma::tr("Manual")};
      const float bw = (mw - 2.0f * pad - 6.0f) * 0.5f;
      for (int k = 0; k < 2 && conAuto; k++) {
        btnAM_[k] = {mx0 + pad + (float)k * (bw + 6.0f), my0 + 36.0f, bw, 30.0f};
        const bool puesto = (k == 0) == st_.focoAuto, hov = btnAM_[k].inside(mx_, my_);
        if (puesto) mask9(btnAM_[k], 0, 4.0f, accent[0], accent[1], accent[2], 0.95f);
        else mask9(btnAM_[k], 0, 4.0f, 1.0f, 1.0f, 1.0f, hov ? 0.16f : 0.07f);
        textoCabe(opc[k], btnAM_[k].x + bw * 0.5f, btnAM_[k].y + 7.0f, 12.5f, bw - 12.0f, puesto ? 0.06f : ink[0],
                  puesto ? 0.10f : ink[1], puesto ? 0.16f : ink[2], puesto || hov ? 1.0f : 0.85f, true);
        if (hov) {
          ayuda = idioma::tr(k == 0 ? "Auto: en cada escena la saliencia busca lo que más llama la atención (una cara, un objeto) y lo deja en el plano de la pantalla, nítido. El alcance se ajusta a la distancia de tus ojos para no pasar de 0,3° de paralaje y la salida se calcula para el mayor relieve que cabe sin aplastar la imagen. Sigue al contenido y salta en los cortes de escena."
                                    : "Manual: tú eliges el plano que queda en la pantalla (Profundidad) y cuánto sale y se hunde la imagen (Salida). Al pasar de Auto a Manual se quedan los valores que se estaban calculando.");
          ayudaY = btnAM_[k].y;
        }
      }
      float y = my0 + 36.0f + filaAM;
      if (manual) {
        /* los dos deslizadores de siempre, ahora en el menu */
        auto fila = [&](Rect *pista, const char *rotulo, float frac, const std::string &valor) {
          const float vw2 = textWidth(valor, 12.0f);
          textoCabe(rotulo, mx0 + pad, y + 6.0f, 12.0f, mw - 2.0f * pad - vw2 - 12.0f, dim[0], dim[1], dim[2], dim[3]);
          text(valor, mx0 + mw - pad - vw2, y + 6.0f, 12.0f, ink[0], ink[1], ink[2], ink[3]);
          *pista = {mx0 + pad + 8.0f, y + 32.0f, mw - 2.0f * pad - 16.0f, 4.0f};
          const Rect &t = *pista;
          solid(t.x, t.y, t.w, 4.0f, 0.30f, 0.32f, 0.35f, 0.9f);
          const float kx = t.x + std::max(0.0f, std::min(1.0f, frac)) * t.w, kr = 8.0f;
          solid(t.x, t.y, kx - t.x, 4.0f, accent[0], accent[1], accent[2], 0.95f);
          solid(kx - kr, t.y + 2.0f - kr, 2.0f * kr, 2.0f * kr, 0.95f, 0.96f, 0.98f, 1.0f);
          y += filaAlto;
        };
        fila(&depTrack_, idioma::tr("Profundidad"), (float)st_.depth / 100.0f, std::to_string(st_.depth));
        fila(&popTrack_, idioma::tr("Salida"), (float)(st_.popout + 50) / 100.0f, std::to_string(st_.popout));
      } else {
        for (size_t k = 0; k < nota.size(); k++)
          text(nota[k], mx0 + pad, y + 18.0f * (float)k, tam, dim[0], dim[1], dim[2], 0.90f);
        y += 18.0f * (float)nota.size() + 6.0f;
        char alc[16];
        std::snprintf(alc, sizeof alc, "%d%c%02d", st_.focoAlcance / 100, idioma::decimal(), st_.focoAlcance % 100);
        text(idioma::trf("Plano {} · Relieve {} px · Alcance x{}", {std::to_string(st_.focoPlano), std::to_string(st_.focoRelieve), alc}),
             mx0 + pad, y, 12.5f, ink[0], ink[1], ink[2], 0.95f);
        y += 24.0f;
      }
      if (st_.modoVisible && !modosEnPastilla) {
        text(idioma::tr("Tipo de vídeo"), mx0 + pad, y + 13.0f, 12.0f, dim[0], dim[1], dim[2], dim[3]);
        const float bw2 = 64.0f, bh2 = 30.0f;
        const float tarjetaM[3] = {Hud::canal(pal_.tarjeta, 0), Hud::canal(pal_.tarjeta, 1), Hud::canal(pal_.tarjeta, 2)};
        dibujaModos({mx0 + mw - pad - 2.0f * bw2 - 8.0f, y + 6.0f, bw2, bh2}, {mx0 + mw - pad - bw2, y + 6.0f, bw2, bh2}, tarjetaM);
        y += 46.0f;
      }
      if (st_.recordarVisible) dibujaRecordar({mx0 + pad, y + 8.0f, 16.0f, 16.0f});
      /* la ayuda de Auto o Manual, a la izquierda del menu (o debajo, si no cabe) */
      if (!ayuda.empty()) {
        const float t2 = 12.5f;
        const std::vector<std::string> lineas = renglones(ayuda, t2, 360.0f);
        float aw = 0.0f;
        for (const std::string &l : lineas) aw = std::max(aw, textWidth(l, t2));
        aw += 24.0f;
        const float ah = 14.0f + 19.0f * (float)lineas.size();
        float ax = mx0 - 10.0f - aw, ay = std::min(ayudaY, (float)vh - 12.0f - ah);
        if (ax < 12.0f) ax = std::max(12.0f, mx0 + mw - aw), ay = my0 + alto + 8.0f;
        cajaAyuda(lineas, ax, ay, t2);
      }
    }
  }

  accent[0] = 0.46f, accent[1] = 0.37f, accent[2] = 0.92f;   /* la tarjeta, en su morado */
  /* (v1.16) las tarjetas, al estilo de vkDaedalus como el panel (Rodolfo: «Yes, the cards too»): su paleta soft, Barlow,
     esquinas de 8 (la tarjeta) y de 4 (botones), el boton principal en su morado fuerte. Con el panel de antes
     (ODYSSEYVK_PANEL_VIEJO=1) se quedan como eran */
  const bool estiloD = st_.marco;
  auto hexD = [](uint32_t c, int k) { return (float)((c >> (16 - 8 * k)) & 0xFFu) / 255.0f; };
  if (estiloD)
    for (int k = 0; k < 3; k++) accent[k] = hexD(pal_.acento, k), ink[k] = hexD(0xE9ECEFu, k), dim[k] = hexD(0xA3ACB6u, k);
  auto Rd = [estiloD](float rr) { return estiloD ? std::min(rr, 4.0f) : rr; };
  auto fondoTarjeta = [&](float ax, float ay, float aw, float ah) {
    if (estiloD) {
      mask9({ax, ay, aw, ah}, 1, 8.0f, hexD(pal_.filo, 0), hexD(pal_.filo, 1), hexD(pal_.filo, 2), 1.0f);
      mask9({ax + 1.0f, ay + 1.0f, aw - 2.0f, ah - 2.0f}, 1, 7.0f, hexD(pal_.tarjeta, 0), hexD(pal_.tarjeta, 1), hexD(pal_.tarjeta, 2), 0.97f);
    } else {
      mask9({ax, ay, aw, ah}, 1, 18.0f, 0.34f, 0.35f, 0.38f, 0.90f);
      mask9({ax + 1.0f, ay + 1.0f, aw - 2.0f, ah - 2.0f}, 1, 17.0f, 0.0f, 0.0f, 0.0f, 0.85f);
    }
  };
  /* un boton: el principal en el morado fuerte (6E4E8C) con letra blanca, el otro en el gris de los controles */
  auto botonD = [&](const Rect &b, bool principal, bool hov) {
    if (principal) mask9(b, 0, 4.0f, Hud::canal(hov ? pal_.fuerteHov : pal_.fuerte, 0), Hud::canal(hov ? pal_.fuerteHov : pal_.fuerte, 1), Hud::canal(hov ? pal_.fuerteHov : pal_.fuerte, 2), 1.0f);
    else mask9(b, 0, 4.0f, hexD(hov ? pal_.controlHov : pal_.control, 0), hexD(hov ? pal_.controlHov : pal_.control, 1), hexD(hov ? pal_.controlHov : pal_.control, 2), 1.0f);
  };
  fuente_ = estiloD && hayBarlow_ ? 1 : 0;
  const int fuenteTitulo = estiloD && hayBarlow_ ? 2 : 0;
  /* la letra chica: Barlow a 11,5 queda por debajo de la mas chica de vkDaedalus (12) y se lee floja */
  const float chica = estiloD ? 12.5f : 11.5f;
  /* ---- la tarjeta de la orilla (abajo a la derecha) ----
     v0.78: se parece a la de la aplicacion oficial, que es la que Rodolfo paso en foto: tarjeta
     oscura de esquinas redondas, la pregunta arriba, dos teselas grandes con su dibujo (video normal
     y video dividido), la casilla de guardar abajo y el "?" y el aspa arriba a la derecha. El «2D
     plano» ya no es una tesela: cerrar la tarjeta deja el video como esta, que es lo mismo. */
  askTop_ = {-10.0f, -10.0f, 0.0f, 0.0f};    /* arriba y abajo: solo por linea de orden (--tb) */
  askFlat_ = {-10.0f, -10.0f, 0.0f, 0.0f};
  askActualizar_ = askLuego_ = askNoAvisar_ = {-10.0f, -10.0f, 0.0f, 0.0f};
  if (st_.ask && !st_.askActualizacion.empty()) {
    /* la tarjeta de la actualizacion (v0.95): la misma tarjeta de la orilla, con otro contenido */
    askConv_ = askSbs_ = askDont_ = askAyuda_ = {-10.0f, -10.0f, 0.0f, 0.0f};
    const float aw = std::min(500.0f, (float)vw - 48.0f), ah = 186.0f;
    /* (v1.16) con la pastilla abajo a la derecha, la tarjeta va encima de ella */
    const float ax = (float)vw - 24.0f - aw,
                ay = (float)vh - 24.0f - ah;   /* (v1.16) la pastilla volvio arriba */
    askPanel_ = {ax, ay, aw, ah};
    fondoTarjeta(ax, ay, aw, ah);
    const float borde = 26.0f;
    askClose_ = {ax + aw - 18.0f - 30.0f, ay + 16.0f, 30.0f, 30.0f};
    const bool hc = askClose_.inside(mx_, my_);
    if (hc) mask9(askClose_, 0, Rd(15.0f), 1.0f, 1.0f, 1.0f, 0.14f);
    icono({askClose_.x + 5.0f, askClose_.y + 5.0f, 20.0f, 20.0f}, icoCerrar_, ink[0], ink[1], ink[2], hc ? 1.0f : 0.80f);
    /* el circulo amarillo con la flecha, como el boton del panel, y el titulo a su lado */
    mask9({ax + borde, ay + 20.0f, 28.0f, 28.0f}, 0, 14.0f, 0.98f, 0.76f, 0.18f, 1.0f);
    icono({ax + borde + 6.0f, ay + 26.0f, 16.0f, 16.0f}, icoFlecha_, 0.16f, 0.12f, 0.04f, 1.0f);
    fuente_ = fuenteTitulo;
    textoCabe(idioma::tr("Hay una versión nueva de OdysseyVK"), ax + borde + 38.0f, ay + 24.0f, 17.0f, aw - borde - 38.0f - 60.0f, ink[0],
              ink[1], ink[2], 1.0f);
    fuente_ = estiloD && hayBarlow_ ? 1 : 0;
    textoCabe(idioma::trf("OdysseyVK {} en GitHub · tienes la {}", {st_.askActualizacion, st_.version.substr(st_.version.rfind(' ') + 1)}),
              ax + borde, ay + 60.0f, 13.0f, aw - 2.0f * borde, ink[0], ink[1], ink[2], 0.92f);
    textoCabe(idioma::tr("Se instala sola (pide tu clave) y el vídeo o juego que haya en 3D se reabre."),
              ax + borde, ay + 80.0f, chica, aw - 2.0f * borde, dim[0], dim[1], dim[2], 0.85f);
    /* Actualizar y Luego, dos pildoras; y abajo, chico, «No avisar de actualizaciones» */
    const float by = ay + 108.0f;
    /* el ancho sigue a la palabra: «Mettre à jour» o «Aktualisieren» no caben donde «Actualizar» */
    const float wA = std::max(150.0f, textWidth(idioma::tr("Actualizar"), 14.0f) + 44.0f);
    const float wL = std::max(92.0f, textWidth(idioma::tr("Luego"), 14.0f) + 40.0f);
    askActualizar_ = {ax + borde, by, wA, 34.0f};
    askLuego_ = {ax + borde + wA + 12.0f, by, wL, 34.0f};
    const bool hA = askActualizar_.inside(mx_, my_), hL = askLuego_.inside(mx_, my_);
    if (estiloD) botonD(askActualizar_, true, hA);
    else mask9(askActualizar_, 0, 17.0f, accent[0], accent[1], accent[2], hA ? 1.0f : 0.92f);
    text(idioma::tr("Actualizar"), askActualizar_.x + askActualizar_.w * 0.5f, by + 8.0f, 14.0f, 1.0f, 1.0f, 1.0f, 1.0f, true);
    if (estiloD) botonD(askLuego_, false, hL);
    else mask9(askLuego_, 0, 17.0f, 1.0f, 1.0f, 1.0f, hL ? 0.18f : 0.10f);
    text(idioma::tr("Luego"), askLuego_.x + askLuego_.w * 0.5f, by + 8.0f, 14.0f, ink[0], ink[1], ink[2], hL ? 1.0f : 0.85f, true);
    const char *noAvisar = idioma::tr("No avisar de actualizaciones");
    askNoAvisar_ = {ax + borde - 4.0f, ay + ah - 36.0f, textWidth(noAvisar, chica) + 8.0f, 22.0f};
    const bool hN = askNoAvisar_.inside(mx_, my_);
    if (hN) mask9(askNoAvisar_, 0, Rd(11.0f), 1.0f, 1.0f, 1.0f, 0.10f);
    text(noAvisar, ax + borde, ay + ah - 31.0f, chica, dim[0], dim[1], dim[2], hN ? 1.0f : 0.70f);
  } else if (st_.ask && !st_.askAviso.empty()) {
    /* un aviso (v1.00): titulo con el anillo de informacion, renglones y «Entendido» */
    askConv_ = askSbs_ = askDont_ = askAyuda_ = askActualizar_ = askNoAvisar_ = {-10.0f, -10.0f, 0.0f, 0.0f};
    const float aw = std::min(520.0f, (float)vw - 48.0f);
    const float ah = 118.0f + 20.0f * (float)st_.askAvisoLineas.size();
    /* (v1.16) con la pastilla abajo a la derecha, la tarjeta va encima de ella */
    const float ax = (float)vw - 24.0f - aw,
                ay = (float)vh - 24.0f - ah;   /* (v1.16) la pastilla volvio arriba */
    askPanel_ = {ax, ay, aw, ah};
    fondoTarjeta(ax, ay, aw, ah);
    const float borde = 26.0f;
    askClose_ = {ax + aw - 18.0f - 30.0f, ay + 16.0f, 30.0f, 30.0f};
    const bool hc = askClose_.inside(mx_, my_);
    if (hc) mask9(askClose_, 0, Rd(15.0f), 1.0f, 1.0f, 1.0f, 0.14f);
    icono({askClose_.x + 5.0f, askClose_.y + 5.0f, 20.0f, 20.0f}, icoCerrar_, ink[0], ink[1], ink[2], hc ? 1.0f : 0.80f);
    const float ai[3] = {estiloD ? accent[0] : accent[0] + 0.22f, estiloD ? accent[1] : accent[1] + 0.25f, estiloD ? accent[2] : 1.0f};
    icono({ax + borde, ay + 22.0f, 24.0f, 24.0f}, icoAnillo_, ai[0], ai[1], ai[2], 0.95f);
    text("i", ax + borde + 12.0f, ay + 26.0f, 14.0f, ai[0], ai[1], ai[2], 0.95f, true);
    fuente_ = fuenteTitulo;
    textoCabe(st_.askAviso, ax + borde + 34.0f, ay + 24.0f, 16.0f, aw - borde - 34.0f - 60.0f, ink[0], ink[1], ink[2], 1.0f);
    fuente_ = estiloD && hayBarlow_ ? 1 : 0;
    float ly = ay + 58.0f;
    for (const std::string &l : st_.askAvisoLineas) {
      textoCabe(l, ax + borde, ly, 12.5f, aw - 2.0f * borde, ink[0], ink[1], ink[2], 0.90f);
      ly += 20.0f;
    }
    /* con cuenta atras (v1.12), el boton dice cuanto le queda: «Entendido (3)» */
    const std::string entendido = st_.askAvisoCuenta > 0
                                      ? std::string(idioma::tr("Entendido")) + " (" + std::to_string(st_.askAvisoCuenta) + ")"
                                      : std::string(idioma::tr("Entendido"));
    askLuego_ = {ax + borde, ay + ah - 48.0f, std::max(120.0f, textWidth(entendido, 14.0f) + 40.0f), 32.0f};
    const bool hL = askLuego_.inside(mx_, my_);
    if (estiloD) botonD(askLuego_, true, hL);
    else mask9(askLuego_, 0, 16.0f, accent[0], accent[1], accent[2], hL ? 1.0f : 0.92f);
    text(entendido, askLuego_.x + askLuego_.w * 0.5f, askLuego_.y + 7.5f, 14.0f, 1.0f, 1.0f, 1.0f, 1.0f, true);
  } else if (st_.ask) {
    /* (v1.16) la pregunta de la primera vez de una aplicacion de video, como la pastilla (Rodolfo, de la PR #2 de Noa3:
       «it must be a popup first so that the overlay appears only after selecting ... and remembering, please make a
       consistent option to the pill»): arriba a la derecha y pegada a las orillas, del color de la barra y con la
       esquina de abajo a la izquierda redonda; el titulo y la ventana, los botones 3D (el del encendido de la
       pastilla) y 2D y el aspa; debajo «Recordar» (puesto de fabrica) y donde se cambia. Antes, abajo a la derecha,
       las dos teselas de video normal y dividido: eso ya lo mira solo el visor */
    const float aw = std::min(500.0f, (float)vw - 24.0f), ah = 104.0f;
    const float ax = (float)vw - aw, ay = 0.0f;
    askPanel_ = {ax, ay, aw, ah};
    askSbs_ = askAyuda_ = {-10.0f, -10.0f, 0.0f, 0.0f};
    mask9({ax, ay - 8.0f, aw + 8.0f, ah + 8.0f}, 1, 8.0f, Hud::canal(pal_.barra, 0), Hud::canal(pal_.barra, 1),
          Hud::canal(pal_.barra, 2), 0.96f);
    auto encaja = [&](const std::string &s, float tam, float maxAncho) {
      if (textWidth(s, tam) <= maxAncho) return s;
      std::string out;
      float ancho = 0.0f;
      const float reserva = textWidth("..", tam);
      size_t k = 0;
      while (k < s.size()) {
        const size_t antes = k;
        nextCp(s, k);
        const float w = textWidth(s.substr(antes, k - antes), tam);
        if (ancho + w > maxAncho - reserva) break;
        ancho += w;
        out += s.substr(antes, k - antes);
      }
      return out + "..";
    };
    const float aire = 16.0f;
    /* de derecha a izquierda: el aspa, el 2D y el 3D, en la fila de arriba (del alto de la pastilla) */
    askClose_ = {ax + aw - 8.0f - 32.0f, ay + 12.0f, 32.0f, 32.0f};
    {
      const bool hc = askClose_.inside(mx_, my_);
      if (hc) mask9(askClose_, 0, 4.0f, hexD(pal_.control, 0), hexD(pal_.control, 1), hexD(pal_.control, 2), 1.0f);
      icono({askClose_.x + 8.0f, askClose_.y + 8.0f, 16.0f, 16.0f}, icoCerrar_, ink[0], ink[1], ink[2], hc ? 1.0f : 0.75f);
    }
    askFlat_ = {askClose_.x - 10.0f - 64.0f, ay + 12.0f, 64.0f, 32.0f};
    askConv_ = {askFlat_.x - 8.0f - 84.0f, ay + 12.0f, 84.0f, 32.0f};
    {
      const bool h3 = askConv_.inside(mx_, my_), h2 = askFlat_.inside(mx_, my_);
      const float t = h3 ? 1.12f : 1.0f;   /* el 3D, como el encendido puesto de la pastilla */
      mask9(askConv_, 0, 4.0f, std::min(1.0f, accent[0] * t), std::min(1.0f, accent[1] * t), std::min(1.0f, accent[2] * t), 0.95f);
      icono({askConv_.x + 10.0f, askConv_.y + 6.0f, 20.0f, 20.0f}, icoEncender_, 1.0f, 1.0f, 1.0f, 1.0f);
      fuente_ = fuenteTitulo;
      text("3D", askConv_.x + 38.0f, askConv_.y + 7.0f, 15.0f, 1.0f, 1.0f, 1.0f, 1.0f);
      /* el 2D, en el gris de los controles de la paleta del modo (con su tinte azul o ambar) */
      mask9(askFlat_, 0, 4.0f, hexD(h2 ? pal_.controlHov : pal_.control, 0), hexD(h2 ? pal_.controlHov : pal_.control, 1),
            hexD(h2 ? pal_.controlHov : pal_.control, 2), 1.0f);
      text("2D", askFlat_.x + askFlat_.w * 0.5f, askFlat_.y + 7.0f, 15.0f, ink[0], ink[1], ink[2], h2 ? 1.0f : 0.85f, true);
    }
    /* el titulo y, debajo, la ventana por la que se pregunta (la pone el servicio) */
    const float anchoTexto = askConv_.x - 12.0f - (ax + aire);
    text(encaja(askTitulo_.empty() ? std::string(idioma::tr("¿Ver en 3D?")) : askTitulo_, 15.0f, anchoTexto), ax + aire, ay + 10.0f,
         15.0f, ink[0], ink[1], ink[2], 1.0f);
    fuente_ = estiloD && hayBarlow_ ? 1 : 0;
    if (!askCuerpo_.empty())
      text(encaja(askCuerpo_, chica, anchoTexto), ax + aire, ay + 31.0f, chica, dim[0], dim[1], dim[2], 0.90f);
    /* la fila de abajo: «Recordar» (se pica en la caja o en su letra) y donde se cambia lo recordado */
    mask9({ax + aire, ay + 56.0f, aw - 2.0f * aire, 1.0f}, 0, 0.0f, hexD(pal_.filo, 0), hexD(pal_.filo, 1), hexD(pal_.filo, 2), 0.8f);
    const std::string rotulo = idioma::tr("Recordar para esta aplicación");
    const Rect caja{ax + aire, ay + 70.0f, 18.0f, 18.0f};
    askDont_ = {caja.x - 4.0f, caja.y - 6.0f, 18.0f + 10.0f + textWidth(rotulo, 13.0f) + 8.0f, 30.0f};
    const bool hd = askDont_.inside(mx_, my_);
    if (st_.remember) {
      mask9(caja, 1, 4.0f, accent[0], accent[1], accent[2], 1.0f);
      icono({caja.x + 2.0f, caja.y + 2.0f, 14.0f, 14.0f}, icoPalomita_, 0.07f, 0.06f, 0.10f, 1.0f);
    } else {
      mask9(caja, 1, 4.0f, hexD(hd ? pal_.filoCasillaHov : pal_.filoCasilla, 0), hexD(hd ? pal_.filoCasillaHov : pal_.filoCasilla, 1),
            hexD(hd ? pal_.filoCasillaHov : pal_.filoCasilla, 2), 1.0f);
      mask9({caja.x + 1.5f, caja.y + 1.5f, caja.w - 3.0f, caja.h - 3.0f}, 1, 3.0f, hexD(pal_.pozo, 0), hexD(pal_.pozo, 1),
            hexD(pal_.pozo, 2), 1.0f);
    }
    text(rotulo, caja.x + 28.0f, caja.y + 0.5f, 13.0f, ink[0], ink[1], ink[2], hd ? 1.0f : 0.90f);
    const float xNota = askDont_.x + askDont_.w + 12.0f;
    const std::string nota = idioma::tr("se cambia en 3D → Aplicaciones que no preguntan");
    const float wNota = std::min(textWidth(nota, chica - 1.0f), ax + aw - aire - xNota);
    if (wNota > 40.0f)
      text(encaja(nota, chica - 1.0f, wNota), ax + aw - aire - wNota, caja.y + 1.5f, chica - 1.0f, dim[0], dim[1], dim[2], 0.75f);
  }
  fuente_ = 0;   /* (v1.16) la letra de siempre para lo que sigue (el aviso bajo la pastilla) */

  if (st_.panel) dibujaPanel(vw, vh);

  /* ---- el aviso del momento, colgado debajo de la pastilla ----
     Antes era una linea abajo a la izquierda, y estaba colocada con el alto REAL de la ventana en
     vez del virtual: en el panel de 2160 (escala 2) caia fuera de la pantalla y no se veia nunca.
     Ahora va pegada bajo la pastilla, que es donde se esta mirando cuando se toca algo, y solo
     sale cuando hay algo que decir (un fallo de la lente o de la camara, o la respuesta a la
     casilla); se quita sola. */
  if (!info_.empty() && st_.overlayOn) {
    const float tam = 12.0f, tw = textWidth(info_, tam);
    const float der = pillPanel_.x + pillPanel_.w;
    const float arriba = menuRend_.w > 0.0f ? menuRend_.y + menuRend_.h + 6.0f : pillPanel_.y + pillPanel_.h + 6.0f;
    solid(der - tw - 20.0f, arriba, tw + 20.0f, 24.0f, 0.0f, 0.0f, 0.0f, 0.82f);
    text(info_, der - tw - 10.0f, arriba + 5.0f, tam, ink[0], ink[1], ink[2], 0.92f);
  }
}

std::vector<std::string> Hud::renglones(const std::string &s, float tam, float maxW) const {
  std::vector<std::string> lineas;
  std::string linea, palabra;
  auto cierra = [&](const std::string &w) {
    const std::string prueba = linea.empty() ? w : linea + " " + w;
    if (textWidth(prueba, tam) <= maxW || linea.empty()) {
      /* una «palabra» sin espacios mas ancha que la caja (chino, japones): se parte por letras */
      if (linea.empty() && textWidth(w, tam) > maxW) {
        std::string trozo;
        size_t i = 0;
        while (i < w.size()) {
          const size_t a = i;
          nextCp(w, i);
          const std::string c = w.substr(a, i - a);
          if (textWidth(trozo + c, tam) > maxW && !trozo.empty()) {
            lineas.push_back(trozo);
            trozo.clear();
          }
          trozo += c;
        }
        linea = trozo;
      } else {
        linea = prueba;
      }
    } else {
      lineas.push_back(linea);
      linea.clear();
      if (textWidth(w, tam) > maxW) {
        std::string trozo;
        size_t i = 0;
        while (i < w.size()) {
          const size_t a = i;
          nextCp(w, i);
          const std::string c = w.substr(a, i - a);
          if (textWidth(trozo + c, tam) > maxW && !trozo.empty()) {
            lineas.push_back(trozo);
            trozo.clear();
          }
          trozo += c;
        }
        linea = trozo;
      } else {
        linea = w;
      }
    }
  };
  for (char ch : s) {
    if (ch == ' ') {
      if (!palabra.empty()) cierra(palabra);
      palabra.clear();
    } else {
      palabra += ch;
    }
  }
  if (!palabra.empty()) cierra(palabra);
  if (!linea.empty()) lineas.push_back(linea);
  return lineas;
}

void Hud::cajaAyuda(const std::vector<std::string> &lineas, float x, float y, float tam) {
  const float ink[4] = {0.93f, 0.94f, 0.96f, 1.0f};
  float w = 0.0f;
  for (const std::string &l : lineas) w = std::max(w, textWidth(l, tam));
  w += 24.0f;
  const float h = 14.0f + 19.0f * (float)lineas.size();
  mask9({x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f}, 0, 10.0f, 0.42f, 0.43f, 0.50f, 0.85f);
  mask9({x, y, w, h}, 0, 9.0f, 0.0f, 0.0f, 0.0f, 0.90f);
  for (size_t k = 0; k < lineas.size(); k++)
  text(lineas[k], x + 12.0f, y + 8.0f + 19.0f * (float)k, tam, ink[0], ink[1], ink[2], 0.95f);
}

/* ---------------------------------------------------------------------------------------------
   El panel de ajustes (v0.84): una tarjeta translucida de esquinas redondas en el centro de la
   pantalla, encima de lo que haya (el escritorio, o el video en 3D). Arriba el nombre, las pestanas
   y el aspa; en medio la lista de filas, que corre con la rueda; abajo la frase de lo ultimo que se
   cambio. Los colores son los de la tarjeta de la orilla y la pastilla.
   --------------------------------------------------------------------------------------------- */
void Hud::dibujaPanel(int vw, int vh) {
  vistaRect_ = {0.0f, 0.0f, 0.0f, 0.0f}, corteVista_ = 0.0f;   /* (v1.18) la pone la vista previa de los efectos */
  lienzoW_ = (float)vw, lienzoH_ = (float)vh;
  /* (v1.16) al estilo de vkDaedalus (State::marco): su paleta «soft» (la que tiene puesta Rodolfo en vkDaedalus), su
     letra (Barlow) y sus medidas (overlay/frame_layout.hpp: un lienzo de diseno de 1280 x 720, la barra de 56, el
     contenido de 980 en el centro, las teselas desde 88 y la tarjeta desde 304) */
  const bool marco = st_.marco;
  auto hex = [](uint32_t c, int k) { return (float)((c >> (16 - 8 * k)) & 0xFFu) / 255.0f; };
  float accent[3] = {st_.acentoPanel[0], st_.acentoPanel[1], st_.acentoPanel[2]};
  float ink[4] = {0.93f, 0.94f, 0.96f, 1.0f};
  float dim[4] = {0.68f, 0.70f, 0.74f, 0.90f};
  if (marco)
    for (int k = 0; k < 3; k++) accent[k] = hex(pal_.acento, k), ink[k] = hex(0xE9ECEFu, k), dim[k] = hex(0xA3ACB6u, k);
  const bool acentoClaro = 0.299f * accent[0] + 0.587f * accent[1] + 0.114f * accent[2] > 0.62f;
  const float tinta[3] = {acentoClaro ? 0.08f : ink[0], acentoClaro ? 0.09f : ink[1], acentoClaro ? 0.11f : ink[2]};
  fuente_ = marco && hayBarlow_ ? 1 : 0;
  /* (v1.16) las esquinas: las de vkDaedalus son de 4 puntos; el panel de antes, en pildora */
  auto R = [marco](float r) { return marco ? std::min(r, 4.0f) : r; };
  float px = 0.0f, py = 0.0f, pw = (float)vw, ph = (float)vh;
  if (!marco) {
    /* el tamano: el de fabrica (820 x 600) o el que se le dio arrastrando la esquina, sin salirse */
    pw = st_.panelAncho > 0.0f ? std::max(560.0f, std::min(st_.panelAncho, (float)vw - 48.0f)) : std::min(820.0f, (float)vw - 48.0f);
    ph = st_.panelAlto > 0.0f ? std::max(400.0f, std::min(st_.panelAlto, (float)vh - 48.0f)) : std::min(600.0f, (float)vh - 48.0f);
    px = std::floor(((float)vw - pw) * 0.5f), py = std::floor(((float)vh - ph) * 0.5f);
  }
  panelCaja_ = {px, py, pw, ph};
  bool hfij = false, ha = false;
  const bool hi = false;
  std::string rotuloActualizar;
  Rect anclaActualizar{-10.0f, -10.0f, 0.0f, 0.0f};   /* (v1.16) de donde cuelga su rotulo: la flecha, o la tesela de la Version */
  const int lengua = idioma::actual();
  Rect pieCaja{px, py + ph - 44.0f, pw, 44.0f};   /* el pie: abajo del panel, o de la tarjeta */
  if (marco) {
    const Rect fuera{-10.0f, -10.0f, 0.0f, 0.0f};
    panelFijar_ = panelLogo_ = panelIdioma_ = panelModo_ = panelEsquina_ = fuera;
    /* el marco: la pantalla entera, o en el escritorio una ventana de 1012 (el contenido de 980 y sus margenes) con la
       barra, las teselas y la tarjeta dentro, mas juntas (sin la fila de avisos de vkDaedalus) */
    const bool ventana = st_.marcoVentana;
    const float s = ventana ? 1.0f : std::max(0.5f, std::min(1.0f, std::min((float)vw / 1280.0f, (float)vh / 720.0f)));
    const float a = std::max(0.0f, std::min(1.0f, st_.aparicion));
    /* (v1.16) lo de en medio, de 1160 (antes 980): las dos mitades de la red de profundidad caben con su letra (Rodolfo:
       «make the main hub a bit bigger if needed»). En el lienzo minimo de 1280 quedan 60 de margen a cada lado */
    const float anchoC = 1160.0f;
    Rect F{0.0f, 0.0f, (float)vw, (float)vh};
    float arribaT = 88.0f, arribaC = 304.0f;
    if (ventana) {
      const float fw = std::min(anchoC + 32.0f, (float)vw - 24.0f), fh = std::min(880.0f, (float)vh - 48.0f);
      F = {std::floor(((float)vw - fw) * 0.5f), std::floor(((float)vh - fh) * 0.5f), fw, fh};
      arribaT = 72.0f, arribaC = 288.0f;
      px = F.x, py = F.y, pw = F.w, ph = F.h;
      panelCaja_ = F;
    }
    /* el velo: no se mueve, solo se oscurece al abrirse (el de vkDaedalus es 5,7,10); en la ventana, su fondo y su filo */
    /* (v1.18) la vista previa de los efectos va en la columna, como la de depuracion */
    const bool enColumna = st_.depuracion || st_.vistaEfectos;
    if (!ventana && !enColumna) solid(0.0f, 0.0f, (float)vw, (float)vh, 5.0f / 255.0f, 7.0f / 255.0f, 10.0f / 255.0f, 0.62f * a);
    /* lo demas sale de la pastilla (Rodolfo: «la herramienta desplace el resto de la interfaz hacia la pantalla»; y con
       la pastilla otra vez arriba, «quiero que presionar la herramientita saque el menu»): baja desde arriba y se hace
       opaco */
    alfaGlobal_ = a;
    desplazY_ = -(1.0f - a) * (1.0f - a) * 90.0f * s;
    auto relleno = [&](Rect r, float radio, uint32_t c, float al) {
      if (radio <= 0.0f) solid(r.x, r.y, r.w, r.h, hex(c, 0), hex(c, 1), hex(c, 2), al);
      else mask9(r, 1, radio, hex(c, 0), hex(c, 1), hex(c, 2), al);
    };
    /* (v1.16) la vista de depuracion (Rodolfo: «once clicked move both lists to the left side of the overlay, half on top
       and the other on the bottom and disappear the settings while the debug button is active»): sin velo, sin barra,
       sin teselas ni tarjeta; una columna a la izquierda con sus filas (la casilla de la vista y lo que rinde la red
       arriba, las redes de siempre en la mitad de arriba y las de video en la de abajo) y el pie. Lo demas deja ver el
       video, y la region de entrada (panelCaja_) se queda en la columna: fuera de ella el raton va a lo de debajo */
    if (enColumna && !ventana) {
      const float m = 16.0f * s, cw = std::min(500.0f * s, (float)vw * 0.42f);
      const Rect col{m, m, cw, (float)vh - 2.0f * m};
      panelCaja_ = col;
      panelCerrar_ = panelVolver_ = panelActualizar_ = fuera;
      panelPestanas_.assign(st_.pestanas.size(), fuera);
      panelTeselas_.assign(st_.teselas.size(), fuera);
      panelTeselaFlecha_.assign(st_.teselas.size(), fuera);
      panelMenuAncla_ = fuera;
      relleno(col, 8.0f * s, pal_.filo, 1.0f);
      relleno({col.x + 1.0f, col.y + 1.0f, col.w - 2.0f, col.h - 2.0f}, 7.0f * s, pal_.tarjeta, 0.95f);
      panelCuerpo_ = {col.x + 6.0f * s, col.y + 8.0f * s, col.w - 12.0f * s, col.h - 8.0f * s - 44.0f};
      pieCaja = {col.x, col.y + col.h - 44.0f, col.w, 44.0f};
      /* (v1.18) la vista previa de los efectos: el visor pinta el juego en toda la pantalla con sus efectos, entero (Rodolfo:
         «on the preview tab dont split it in half, just show it full now»: sin raya, corte 0). Con el interruptor general
         apagado sale sin ellos, y abajo, en medio de lo que se ve, lo dice (arriba estan la pastilla y el cuadrito de FPS) */
      if (st_.vistaEfectos && st_.vistaEnVivo) {
        vistaRect_ = {0.0f, 0.0f, (float)vw, (float)vh};
        corteVista_ = 0.0f;
        if (!st_.efectosPuestos) {
          const std::string t = idioma::tr("Efectos apagados");
          const float cx = (col.x + col.w + (float)vw) * 0.5f, tam = 14.0f * s, tw = textWidth(t, tam) + 28.0f * s;
          const Rect r{std::floor(cx - tw * 0.5f), (float)vh - m - 30.0f * s, tw, 30.0f * s};
          relleno(r, 15.0f * s, pal_.tarjeta, 0.88f);
          text(t, cx, r.y + (r.h - tam) * 0.5f, tam, ink[0], ink[1], ink[2], 1.0f, true);
        }
      }
    } else {
    /* la barra de arriba: el logo y el nombre; a la derecha, la actualizacion y el aspa */
    if (ventana) {
      relleno(F, 8.0f, pal_.filo, 1.0f);
      relleno({F.x + 1.0f, F.y + 1.0f, F.w - 2.0f, F.h - 2.0f}, 7.0f, pal_.ventana, 0.98f);
      relleno({F.x + 1.0f, F.y + 1.0f, F.w - 2.0f, 56.0f}, 7.0f, pal_.barra, 1.0f);   /* la barra, con las esquinas de arriba */
      relleno({F.x + 1.0f, F.y + 28.0f, F.w - 2.0f, 29.0f}, 0.0f, pal_.barra, 1.0f);
    } else {
      relleno({0.0f, 0.0f, (float)vw, 56.0f * s}, 0.0f, pal_.barra, 0.97f);
    }
    {
      const Rect baldosa{F.x + 16.0f * s, F.y + 17.0f * s, 22.0f * s, 22.0f * s};
      mask9(baldosa, 1, 5.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
      icono({baldosa.x + s, baldosa.y + s, 20.0f * s, 20.0f * s}, icoLogo_, 14.0f / 255.0f, 68.0f / 255.0f, 158.0f / 255.0f, 1.0f);
      fuente_ = hayBarlow_ ? 2 : 0;
      text("OdysseyVK", baldosa.x + baldosa.w + 12.0f * s, F.y + (56.0f - 21.0f) * 0.5f * s, 21.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
      fuente_ = hayBarlow_ ? 1 : 0;
    }
    panelCerrar_ = {F.x + F.w - 16.0f * s - 32.0f * s, F.y + 12.0f * s, 32.0f * s, 32.0f * s};
    /* (v1.17) sin la flecha de volver a la pastilla (Rodolfo: «quita la flecha»): cerrar ya devuelve el video, y la
       flecha ademas mandaba `capa = mostrar` justo cuando KWin le devuelve el foco al navegador */
    panelVolver_ = fuera;
    {
      const bool hc = panelCerrar_.inside(mx_, my_);
      if (hc) relleno(panelCerrar_, 4.0f * s, pal_.control, 1.0f);
      icono({panelCerrar_.x + 8.0f * s, panelCerrar_.y + 8.0f * s, 16.0f * s, 16.0f * s}, icoCerrar_, ink[0], ink[1], ink[2], hc ? 1.0f : 0.80f);
    }
    /* (v1.16) sin la flecha de actualizar junto al aspa (Rodolfo: «quita el boton de update de arriba, mismo
       comportamiento pero recuerda que ya lo movimos, ahora esta en la interfaz, pon la funcion a su boton»): lo hace
       la tesela de la Version, con su rotulo al pasar el raton y la flecha en ambar si hay una version nueva */
    panelActualizar_ = fuera;
    rotuloActualizar = !st_.actualizacion.empty()
                           ? idioma::trf("Hay una actualización pendiente: OdysseyVK {}. Pica para instalarla.", {st_.actualizacion})
                           : std::string(idioma::tr(st_.alDia ? "OdysseyVK está al día. Pica para volver a comprobarlo."
                                                              : "Aún no se ha buscado ninguna actualización. Pica para buscarla."));

    /* las teselas: en el centro las grandes, una por pestana (las que no salen, como Juegos sobre un juego, no
       ocupan sitio); a los lados las chicas, tres y tres */
    const float izqC = F.x + std::floor((F.w - anchoC * s) * 0.5f), arriba = F.y + arribaT * s;
    std::vector<int> vistas;
    /* (v1.18) en el orden de `ordenPestanas` si lo hay: Efectos (la 5) junto a Juegos sin cambiar los numeros de las demas */
    std::vector<int> orden = st_.ordenPestanas;
    if (orden.empty())
      for (size_t i = 0; i < st_.pestanas.size(); i++) orden.push_back((int)i);
    for (int i : orden)
      if (i >= 0 && (size_t)i < st_.pestanas.size() && (i == 0 || !st_.pestanas[(size_t)i].empty())) vistas.push_back(i);
    const int nT = std::max(1, (int)vistas.size());
    const float sitioC = anchoC - 2.0f * 200.0f - 2.0f * 16.0f;
    const float anchoT = std::min(200.0f, (sitioC - 2.0f * (float)(nT - 1)) / (float)nT);
    const float centroW = (float)nT * anchoT + 2.0f * (float)(nT - 1);
    const float centroX = F.x + std::floor((F.w - centroW * s) * 0.5f);
    panelPestanas_.assign(st_.pestanas.size(), fuera);
    const Rect *iconoPest[6] = {&icoCubo_, &icoMando_, &icoEnchufe_, &icoDeslizadores_, &icoFg_, &icoEfectos_};   /* (v1.18) y los efectos */
    auto nombrePest = [&](int i) { return i == 0 ? std::string(idioma::tr("3D")) : st_.pestanas[(size_t)i]; };
    for (int k = 0; k < (int)vistas.size(); k++) {
      const int i = vistas[(size_t)k];
      const Rect t{centroX + (float)k * (anchoT + 2.0f) * s, arriba, anchoT * s, 200.0f * s};
      panelPestanas_[(size_t)i] = t;
      const bool gris = (size_t)i < st_.pestanaGris.size() && st_.pestanaGris[(size_t)i];
      const bool puesta = i == st_.pestana, hov = t.inside(mx_, my_) && !gris;
      const float tenueT = gris ? 0.38f : 1.0f;
      /* solo las esquinas de fuera de la fila van redondas, como en vkDaedalus: aqui todas con poco radio */
      relleno(t, 6.0f * s, gris ? pal_.gris : (puesta ? pal_.suave : (hov ? pal_.control : pal_.elevado)), 0.97f);
      if (puesta) relleno({t.x, t.y + t.h - 3.0f * s, t.w, 3.0f * s}, 0.0f, pal_.acento, 1.0f);
      fuente_ = hayBarlow_ ? 2 : 0;
      textoCabe(nombrePest(i), t.x + t.w * 0.5f - std::min(t.w - 12.0f * s, textWidth(nombrePest(i), 18.0f * s)) * 0.5f, t.y + 16.0f * s,
                18.0f * s, t.w - 12.0f * s, 1.0f, 1.0f, 1.0f, (puesta || hov ? 1.0f : 0.92f) * tenueT);
      fuente_ = hayBarlow_ ? 1 : 0;
      if (i < 6)
        icono({t.x + (t.w - 56.0f * s) * 0.5f, t.y + 64.0f * s, 56.0f * s, 56.0f * s}, *iconoPest[i], puesta ? accent[0] : hex(0xC9D0D7u, 0),
              puesta ? accent[1] : hex(0xC9D0D7u, 1), puesta ? accent[2] : hex(0xC9D0D7u, 2), (puesta || hov ? 1.0f : 0.85f) * tenueT);
      /* en gris, la nota dice por que: solo se abre en el escritorio */
      const std::string nota = gris ? std::string(idioma::tr("En el escritorio"))
                                    : ((size_t)i < st_.pestanasNota.size() ? st_.pestanasNota[(size_t)i] : std::string());
      if (!nota.empty()) {
        const float w = std::min(t.w - 12.0f * s, textWidth(nota, 13.0f * s));
        textoCabe(nota, t.x + (t.w - w) * 0.5f, t.y + t.h - 34.0f * s, 13.0f * s, t.w - 12.0f * s, gris ? dim[0] : accent[0],
                  gris ? dim[1] : accent[1], gris ? dim[2] : accent[2], gris ? 0.7f : 1.0f);
      }
    }
    panelTeselas_.assign(st_.teselas.size(), fuera);
    panelTeselaFlecha_.assign(st_.teselas.size(), fuera);
    panelMenuAncla_ = fuera;
    for (size_t k = 0; k < st_.teselas.size() && k < 6; k++) {
      const Hud::State::Tesela &te = st_.teselas[k];
      const float tx = k < 3 ? izqC : izqC + (anchoC - 200.0f) * s;
      const Rect t{tx, arriba + (float)(k % 3) * 68.0f * s, 200.0f * s, 64.0f * s};
      panelTeselas_[k] = t;
      const bool hov = t.inside(mx_, my_) && !te.id.empty();
      /* (v1.16) la de la Version hace lo de la flecha que habia junto al aspa: su flecha en ambar con una version nueva,
         y entonces con el filo del boton «Actualizar a» de antes */
      const bool esVersion = te.id == "buscar_actualizacion", nueva = esVersion && !st_.actualizacion.empty();
      if (nueva) {
        relleno(t, 6.0f * s, pal_.filo, 1.0f);
        relleno({t.x + 1.0f, t.y + 1.0f, t.w - 2.0f, t.h - 2.0f}, 6.0f * s, hov ? pal_.control : pal_.barra, 1.0f);
      } else {
        relleno(t, 6.0f * s, hov ? pal_.control : pal_.elevado, 0.97f);
      }
      if (esVersion) {
        ha = hov && !st_.menuIdioma;
        anclaActualizar = t;
      }
      icono({t.x + 16.0f * s, t.y + 22.0f * s, 20.0f * s, 20.0f * s}, iconoTesela(te.icono), nueva ? hex(0xDEAA7Cu, 0) : hex(0xC9D0D7u, 0),
            nueva ? hex(0xDEAA7Cu, 1) : hex(0xC9D0D7u, 1), nueva ? hex(0xDEAA7Cu, 2) : hex(0xC9D0D7u, 2), hov || nueva ? 1.0f : 0.85f);
      /* (v1.17) con menu (el cuadrito de los FPS): una flechita a la derecha que lo abre */
      const float wFlecha = te.menu ? 30.0f * s : 0.0f;
      if (te.menu) {
        const Rect fl{t.x + t.w - wFlecha, t.y, wFlecha, t.h};
        if (k < panelTeselaFlecha_.size()) panelTeselaFlecha_[k] = fl;
        const bool hf = fl.inside(mx_, my_) || st_.teselaMenu == (int)k;
        if (hf) relleno({fl.x + 2.0f * s, fl.y + 8.0f * s, fl.w - 6.0f * s, fl.h - 16.0f * s}, 4.0f * s, pal_.controlHov, 1.0f);
        icono({fl.x + (fl.w - 14.0f * s) * 0.5f - 2.0f * s, fl.y + (fl.h - 14.0f * s) * 0.5f, 14.0f * s, 14.0f * s}, icoBajar_,
              ink[0], ink[1], ink[2], hf ? 1.0f : 0.70f);
        if (st_.teselaMenu == (int)k) panelMenuAncla_ = t;
      }
      textoCabe(te.titulo, t.x + 48.0f * s, t.y + 13.0f * s, 12.0f * s, t.w - 56.0f * s - wFlecha, dim[0], dim[1], dim[2], 1.0f);
      textoCabe(te.valor, t.x + 48.0f * s, t.y + 31.0f * s, 15.0f * s, t.w - 56.0f * s - wFlecha, te.puesta ? accent[0] : ink[0],
                te.puesta ? accent[1] : ink[1], te.puesta ? accent[2] : ink[2], 1.0f);
      if (te.id == "menu_idioma") panelIdioma_ = t;   /* la lista de idiomas cuelga de ella */
    }

    /* la tarjeta de la pestana puesta: su nombre arriba, las filas y el pie */
    const Rect tarjeta{izqC, F.y + arribaC * s, anchoC * s, std::max(160.0f, std::min(700.0f * s, F.h - arribaC * s - 16.0f * s))};
    relleno(tarjeta, 8.0f * s, pal_.tarjeta, 0.97f);
    fuente_ = hayBarlow_ ? 2 : 0;
    text(nombrePest(st_.pestana), tarjeta.x + 18.0f * s, tarjeta.y + 15.0f * s, 20.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
    fuente_ = hayBarlow_ ? 1 : 0;
    panelCuerpo_ = {tarjeta.x + 6.0f * s, tarjeta.y + 52.0f * s, tarjeta.w - 12.0f * s, tarjeta.h - 52.0f * s - 44.0f};
    pieCaja = {tarjeta.x, tarjeta.y + tarjeta.h - 44.0f, tarjeta.w, 44.0f};
    }   /* (v1.16) fin de lo que no sale con la vista de depuracion */
  } else {
    mask9(panelCaja_, 1, 22.0f, 0.42f, 0.43f, 0.50f, 0.80f);                                   /* el filo */
    mask9({px + 1.0f, py + 1.0f, pw - 2.0f, ph - 2.0f}, 1, 21.0f, 0.0f, 0.0f, 0.0f, 0.82f);   /* negro un poco transparente (26-nov; antes gris) */

    /* cabecera: el pin, el nombre, pestanas en pildora y el aspa */
    panelFijar_ = {px + 18.0f, py + 22.0f, 28.0f, 28.0f};
    hfij = panelFijar_.inside(mx_, my_) && !st_.menuIdioma;
    if (st_.fijado) mask9(panelFijar_, 0, 14.0f, accent[0], accent[1], accent[2], 0.95f);
    else if (hfij) mask9(panelFijar_, 0, 14.0f, 1.0f, 1.0f, 1.0f, 0.12f);
    icono({panelFijar_.x + 5.0f, panelFijar_.y + 5.0f, 18.0f, 18.0f}, icoPin_, ink[0], ink[1], ink[2],
          st_.fijado ? 1.0f : (hfij ? 0.95f : 0.55f));
    /* el logo y el nombre son la primera pestana (rama experimental; Rodolfo: «la primer pestaña que parta del
       mismo logo del programa»): la baldosa blanca con los puntos azules del icono y «OdysseyVK»; puesta, en
       una pildora del acento como las demas pestanas */
    {
      const float wLogo = 8.0f + 22.0f + 9.0f + textWidth("OdysseyVK", 17.0f) + 14.0f;
      panelLogo_ = {px + 50.0f, py + 20.0f, wLogo, 32.0f};
      const bool puesta = st_.pestana == 0, hov = panelLogo_.inside(mx_, my_) && !st_.menuIdioma;
      if (puesta) mask9(panelLogo_, 0, 16.0f, accent[0], accent[1], accent[2], 0.95f);
      else if (hov) mask9(panelLogo_, 0, 16.0f, 1.0f, 1.0f, 1.0f, 0.10f);
      const Rect baldosa{panelLogo_.x + 8.0f, panelLogo_.y + 5.0f, 22.0f, 22.0f};
      mask9(baldosa, 1, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);
      icono({baldosa.x + 1.0f, baldosa.y + 1.0f, 20.0f, 20.0f}, icoLogo_, 14.0f / 255.0f, 68.0f / 255.0f, 158.0f / 255.0f, 1.0f);
      text("OdysseyVK", baldosa.x + baldosa.w + 9.0f, panelLogo_.y + 6.0f, 17.0f, puesta ? tinta[0] : ink[0], puesta ? tinta[1] : ink[1],
           puesta ? tinta[2] : ink[2], puesta || hov ? 1.0f : 0.90f);
    }
    /* (v1.12) el interruptor Neural | Rendimiento ya no esta en la cabecera: vive en la pastilla del video,
       donde se cambia en marcha («tambien quita ya esta, la vieja pastilla», Rodolfo) */
    panelModo_ = {-10.0f, -10.0f, 0.0f, 0.0f};
    /* las pestanas, en una segunda fila y centradas (rama experimental) */
    panelPestanas_.clear();
    float tamPest = 14.0f;
    float tx = px;
    {
      float total = 0.0f;
      for (const std::string &p : st_.pestanas)
        if (!p.empty()) total += textWidth(p, tamPest) + 32.0f + 8.0f;
      total = std::max(0.0f, total - 8.0f);
      tx = px + std::max(20.0f, (pw - total) * 0.5f);
    }
    for (size_t i = 0; i < st_.pestanas.size(); i++) {
      /* una pestana sin nombre no se ensena (la de Juegos sobre un video o un juego en 3D); su caja queda
         fuera de la vista para que las demas conserven su numero */
      if (st_.pestanas[i].empty()) {
        panelPestanas_.push_back({-10.0f, -10.0f, 0.0f, 0.0f});
        continue;
      }
      const float w = textWidth(st_.pestanas[i], tamPest) + 32.0f;
      const Rect t{tx, py + 62.0f, w, 32.0f};
      panelPestanas_.push_back(t);
      const bool grisP = i < st_.pestanaGris.size() && st_.pestanaGris[i];   /* (v1.16) en gris: tenue y sin resaltar */
      const bool puesta = (int)i == st_.pestana, hov = t.inside(mx_, my_) && !grisP;
      if (puesta) mask9(t, 0, 16.0f, accent[0], accent[1], accent[2], 0.95f);
      else if (hov) mask9(t, 0, 16.0f, 1.0f, 1.0f, 1.0f, 0.10f);
      text(st_.pestanas[i], t.x + t.w * 0.5f, t.y + 7.0f + (14.0f - tamPest) * 0.5f, tamPest, puesta ? tinta[0] : ink[0],
           puesta ? tinta[1] : ink[1], puesta ? tinta[2] : ink[2], grisP ? 0.30f : (puesta || hov ? 1.0f : 0.72f),
           true);
      tx += w + 8.0f;
    }
    panelCerrar_ = {px + pw - 20.0f - 32.0f, py + 20.0f, 32.0f, 32.0f};
    const bool hc = panelCerrar_.inside(mx_, my_);
    if (hc) mask9(panelCerrar_, 0, 16.0f, 1.0f, 1.0f, 1.0f, 0.14f);
    icono({panelCerrar_.x + 6.0f, panelCerrar_.y + 6.0f, 20.0f, 20.0f}, icoCerrar_, ink[0], ink[1], ink[2],
          hc ? 1.0f : 0.78f);
    /* junto al aspa, el boton de la actualizacion (v0.95): un circulo amarillo con la flecha si hay una
       version nueva en GitHub; la flecha verde si esta al dia; gris si no se ha mirado. Al pasar por
       encima, un rotulo lo dice (se pinta al final, por encima de las filas). */
    panelActualizar_ = {panelCerrar_.x - 40.0f, py + 20.0f, 32.0f, 32.0f};
    ha = panelActualizar_.inside(mx_, my_);
    if (!st_.actualizacion.empty()) {
      mask9(panelActualizar_, 0, 16.0f, 0.98f, 0.76f, 0.18f, ha ? 1.0f : 0.92f);
      icono({panelActualizar_.x + 7.0f, panelActualizar_.y + 7.0f, 18.0f, 18.0f}, icoFlecha_, 0.16f, 0.12f, 0.04f, 1.0f);
      rotuloActualizar = idioma::trf("Hay una actualización pendiente: OdysseyVK {}. Pica para instalarla.", {st_.actualizacion});
    } else {
      if (ha) mask9(panelActualizar_, 0, 16.0f, 1.0f, 1.0f, 1.0f, 0.14f);
      if (st_.alDia) {
        icono({panelActualizar_.x + 7.0f, panelActualizar_.y + 7.0f, 18.0f, 18.0f}, icoFlecha_, 0.36f, 0.86f, 0.46f, ha ? 1.0f : 0.9f);
        rotuloActualizar = idioma::tr("OdysseyVK está al día. Pica para volver a comprobarlo.");
      } else {
        icono({panelActualizar_.x + 7.0f, panelActualizar_.y + 7.0f, 18.0f, 18.0f}, icoFlecha_, ink[0], ink[1], ink[2], ha ? 0.8f : 0.4f);
        rotuloActualizar = idioma::tr("Aún no se ha buscado ninguna actualización. Pica para buscarla.");
      }
    }
    /* el boton del idioma (v1.07) ya no esta en la cabecera (rama experimental; Rodolfo: «quita el pill
       del idioma, basta con tenerlo en otros»): el idioma se elige en Programa → Otros */
    panelIdioma_ = {-10.0f, -10.0f, 0.0f, 0.0f};
    solid(px + 20.0f, py + 106.0f, pw - 40.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.08f);   /* bajo las dos filas */

    /* el cuerpo: las filas, recortadas contra su caja y corridas lo que diga la rueda */
    panelCuerpo_ = {px + 12.0f, py + 114.0f, pw - 24.0f, ph - 114.0f - 44.0f};
  }
  const float izq0 = panelCuerpo_.x + 16.0f, der0 = panelCuerpo_.x + panelCuerpo_.w - 22.0f;
  /* (v1.15) la caja de una Linea: lo que mide de ancho (para partirla igual al medirla y al pintarla) */
  auto anchoLinea = [&](const ItemPanel &it) {
    const float wb = it.nota.empty() ? 0.0f : textWidth(it.nota, 12.5f) + 28.0f + 12.0f;
    return std::max(80.0f, (der0 - wb) - izq0);   /* las Linea van siempre a lo ancho */
  };
  auto alto = [&](const ItemPanel &it) {
    switch (it.tipo) {
      case ItemPanel::Linea: {
        const std::string &t = it.valorTexto.empty() ? it.rotulo : it.valorTexto;
        const size_t n = std::max<size_t>(1, renglones(t, 13.0f, anchoLinea(it) - 20.0f).size());
        return 22.0f + 19.0f * (float)n;
      }
      case ItemPanel::Texto: return 48.0f;
      case ItemPanel::Seccion: return 38.0f;
      case ItemPanel::Opciones: return 54.0f;
      case ItemPanel::Fila: return 42.0f;
      case ItemPanel::Juego: return 56.0f;
      case ItemPanel::Nota: return 32.0f;
      case ItemPanel::Info: return it.nota.empty() ? 36.0f : 54.0f;
      case ItemPanel::Atajo: return 44.0f;
      default: return it.nota.empty() ? 46.0f : 60.0f;
    }
  };
  /* donde va cada fila (y contada desde el principio de la lista). Los juegos no van en fila sino
     en rejilla de cinco caratulas (v0.88): los seguidos se van colocando de izquierda a derecha. */
  const int kCols = 5;
  const float hueco = 14.0f, anchoCarta = (panelCuerpo_.w - 32.0f - 10.0f - hueco * (float)(kCols - 1)) / (float)kCols;
  const float altoCarta = anchoCarta * 1.5f + 40.0f;
  std::vector<Rect> sitio(st_.items.size());
  {
    float yy = 0.0f, filaY = 0.0f;
    int col = 0;
    for (size_t i = 0; i < st_.items.size(); i++) {
      const ItemPanel &it = st_.items[i];
      if (it.tipo == ItemPanel::Juego) {
        if (col == 0) {
          filaY = yy + 6.0f;
          yy += altoCarta + hueco;
        }
        sitio[i] = {izq0 + (float)col * (anchoCarta + hueco), filaY, anchoCarta, altoCarta};
        col = (col + 1) % kCols;
      } else if (it.lado != 0) {
        /* (v1.16) las de media seguidas: la 1 a la izquierda y la 2 a la derecha, cada una con su altura; con la vista de
           depuracion, en la columna entera, la 1 arriba y la 2 desde la mitad (o debajo de la 1, si esa pasa la mitad) */
        col = 0;
        const float x0 = panelCuerpo_.x + 4.0f, w0 = panelCuerpo_.w - 14.0f, entre = 24.0f;
        const float wm = st_.depuracion ? w0 : (w0 - entre) * 0.5f;
        size_t j = i;
        while (j < st_.items.size() && st_.items[j].lado != 0) j++;
        float yIzq = yy;
        for (size_t k = i; k < j; k++)
          if (st_.items[k].lado == 1) sitio[k] = {x0, yIzq, wm, alto(st_.items[k])}, yIzq += alto(st_.items[k]);
        float yDer = st_.depuracion ? std::max(yIzq, std::max(yy, panelCuerpo_.h * 0.5f)) : yy;
        for (size_t k = i; k < j; k++)
          if (st_.items[k].lado != 1)
            sitio[k] = {st_.depuracion ? x0 : x0 + wm + entre, yDer, wm, alto(st_.items[k])}, yDer += alto(st_.items[k]);
        yy = std::max(yIzq, yDer);
        i = j - 1;
      } else {
        col = 0;
        sitio[i] = {panelCuerpo_.x + 4.0f, yy, panelCuerpo_.w - 14.0f, alto(it)};
        yy += alto(it);
      }
    }
    contenidoAlto_ = yy;
  }
  const float tope = std::max(0.0f, contenidoAlto_ - panelCuerpo_.h);
  st_.corrida = std::max(0.0f, std::min(tope, st_.corrida));
  zonas_.assign(st_.items.size(), ZonaItem{});
  recorte_ = panelCuerpo_;
  hayRecorte_ = true;
  const bool dentro = panelCuerpo_.inside(mx_, my_);
  Rect despCaja{-10.0f, -10.0f, 0.0f, 0.0f};   /* (v1.16) la caja del desplegable abierto, para colgarle su lista */
  for (size_t i = 0; i < st_.items.size(); i++) {
    const ItemPanel &it = st_.items[i];
    const float h = sitio[i].h, y = panelCuerpo_.y - st_.corrida + sitio[i].y;
    /* (v1.16) los margenes de esta fila, los de su sitio: las de media van en su mitad (las caratulas, los de siempre) */
    const float izq = it.tipo == ItemPanel::Juego ? izq0 : sitio[i].x + 12.0f;
    const float der = it.tipo == ItemPanel::Juego ? der0 : sitio[i].x + sitio[i].w - 12.0f;
    ZonaItem &z = zonas_[i];
    z.caja = {sitio[i].x, y, sitio[i].w, h};
    z.yCentro = y + h * 0.5f;
    if (y + h < panelCuerpo_.y || y > panelCuerpo_.y + panelCuerpo_.h) {   /* fuera de la vista */
      z.caja = {-10.0f, -10.0f, 0.0f, 0.0f};
      continue;
    }
    const bool hov = dentro && z.caja.inside(mx_, my_), hovFila = hov;
    const bool picable = (it.tipo == ItemPanel::Interruptor && !(it.apagado && !it.valor && !it.descargable)) || it.tipo == ItemPanel::Juego ||
                         it.tipo == ItemPanel::Atajo || (it.tipo == ItemPanel::Seccion && it.plegable);
    if (hov && it.tipo != ItemPanel::Juego && (picable || it.tipo == ItemPanel::Fila || it.tipo == ItemPanel::Opciones || it.tipo == ItemPanel::Deslizador) &&
        !(it.tipo == ItemPanel::Interruptor && it.apagado && !it.valor && !it.descargable))
      mask9({z.caja.x, y + 2.0f, z.caja.w, h - 4.0f}, 1, 10.0f, 1.0f, 1.0f, 1.0f, picable ? 0.07f : 0.035f);
    const float yr = it.nota.empty() ? y + (h - 20.0f) * 0.5f : y + 10.0f;
    /* (v1.15) lo propio de un juego en sus integraciones: un punto del acento delante del rotulo */
    if (it.propio) {
      const float py0 = it.tipo == ItemPanel::Opciones ? y + 24.0f : (it.tipo == ItemPanel::Texto ? y + h * 0.5f - 3.0f : yr + 7.0f);
      mask9({izq - 11.0f, py0, 6.0f, 6.0f}, 0, 3.0f, accent[0] + 0.22f, accent[1] + 0.25f, 1.0f, 1.0f);
    }
    switch (it.tipo) {
      case ItemPanel::Seccion:
        if (marco) {   /* (v1.16) como los grupos de vkDaedalus: en frase, seminegrita y clara */
          fuente_ = hayBarlow_ ? 2 : 0;
          text(it.talCual ? it.rotulo : enFrase(it.rotulo), izq, y + 13.0f, 15.0f, 1.0f, 1.0f, 1.0f, 0.95f);
          fuente_ = hayBarlow_ ? 1 : 0;
        } else {
          text(it.rotulo, izq, y + 17.0f, 11.5f, accent[0] + 0.22f, accent[1] + 0.25f, 1.0f, 0.95f);
        }
        /* plegable: la flechita a la derecha, hacia abajo si esta desplegada y hacia el lado si no */
        if (it.plegable)
          icono({der - 14.0f, y + 16.0f, 14.0f, 14.0f}, it.valor ? icoBajar_ : icoDerecha_, accent[0] + 0.22f,
                accent[1] + 0.25f, 1.0f, hov ? 1.0f : 0.80f);
        break;
      case ItemPanel::Nota:
        textoCabe(it.rotulo, izq, y + 8.0f, 12.5f, der - izq, dim[0], dim[1], dim[2], 0.85f);
        break;
      case ItemPanel::Info: {
        const Rect an{izq, y + 9.0f, 16.0f, 16.0f};
        icono(an, icoAnillo_, accent[0] + 0.22f, accent[1] + 0.25f, 1.0f, 0.95f);
        text("i", an.x + 8.0f, an.y + 2.0f, 11.0f, accent[0] + 0.22f, accent[1] + 0.25f, 1.0f, 0.95f, true);
        textoCabe(it.rotulo, izq + 24.0f, y + 9.0f, 12.5f, der - izq - 24.0f, ink[0], ink[1], ink[2], 0.92f);
        if (!it.nota.empty()) textoCabe(it.nota, izq + 24.0f, y + 29.0f, 12.0f, der - izq - 24.0f, dim[0], dim[1], dim[2], 0.85f);
        break;
      }
      case ItemPanel::Boton: {
        const float wb = textWidth(it.valorTexto, 13.5f) + 30.0f;
        textoCabe(it.rotulo, izq, yr, 15.0f, der - wb - 16.0f - izq, ink[0], ink[1], ink[2], 1.0f);
        if (!it.nota.empty()) textoCabe(it.nota, izq, y + 34.0f, 11.5f, der - wb - 16.0f - izq, dim[0], dim[1], dim[2], 0.85f);
        const Rect b{der - wb, y + (h - 30.0f) * 0.5f, wb, 30.0f};
        z.subs.push_back(b);
        const bool hb = dentro && b.inside(mx_, my_);
        /* `valor` apagado = boton que no se puede picar ahora (una descarga en marcha): sale gris */
        if (!it.valor) mask9(b, 0, R(15.0f), 0.28f, 0.29f, 0.33f, 0.98f);
        else if (it.peligro) mask9(b, 0, R(15.0f), 0.82f, 0.33f, 0.36f, hb ? 1.0f : 0.90f);
        else if (marco) mask9(b, 0, R(15.0f), Hud::canal(hb ? pal_.fuerteHov : pal_.fuerte, 0), Hud::canal(hb ? pal_.fuerteHov : pal_.fuerte, 1), Hud::canal(hb ? pal_.fuerteHov : pal_.fuerte, 2), 1.0f);
        else mask9(b, 0, R(15.0f), accent[0], accent[1], accent[2], hb ? 1.0f : 0.92f);
        {
          const bool sobreAcento = it.valor && !it.peligro && !marco;   /* (v1.16) en el morado fuerte, letra blanca */
          text(it.valorTexto, b.x + b.w * 0.5f, b.y + 6.5f, 13.5f, sobreAcento ? tinta[0] : 1.0f, sobreAcento ? tinta[1] : 1.0f,
               sobreAcento ? tinta[2] : 1.0f, it.valor ? 1.0f : 0.70f, true);
        }
        break;
      }
      case ItemPanel::Interruptor: {
        /* (v1.15) en gris: el rotulo y el interruptor apagados, y la pastilla a su izquierda */
        const float ap = it.apagado ? 0.42f : 1.0f;
        const bool casilla = marco && !it.pildora;   /* (v1.18) con `pildora`, el interruptor de pastilla tambien en el marco */
        /* (v1.16) al estilo de vkDaedalus, una casilla a la izquierda del rotulo en vez del interruptor */
        /* (v1.16) `centrado`: la casilla y su rotulo, juntos en medio de la fila */
        float izqI = it.centrado ? std::floor(sitio[i].x + (sitio[i].w - 30.0f - textWidth(it.rotulo, 15.0f)) * 0.5f) : izq;
        /* (v1.18) un efecto de la lista: el agarre de seis puntos antes de la flechita (se arrastra) y el aspita a la derecha
           del interruptor (lo quita) */
        if (it.arrastrable) {
          z.agarre = {izqI - 6.0f, y, 22.0f, h};
          const bool ha = (dentro && z.agarre.inside(mx_, my_)) || st_.moviendoFila == (int)i;
          for (int fy = 0; fy < 3; fy++)
            for (int fx = 0; fx < 2; fx++)
              mask9({izqI + 1.0f + (float)fx * 6.0f, yr + 3.0f + (float)fy * 6.0f, 3.5f, 3.5f}, 0, 1.75f, ink[0], ink[1], ink[2], ha ? 0.95f : 0.45f);
          izqI += 20.0f;
        }
        const float derSw = it.borrable || it.sitioAspa ? der - 30.0f : der;
        if (it.borrable) {
          z.aspa = {der - 24.0f, y + (h - 24.0f) * 0.5f, 24.0f, 24.0f};
          const bool hx = dentro && z.aspa.inside(mx_, my_);
          if (hx) mask9(z.aspa, 0, R(12.0f), 0.86f, 0.33f, 0.33f, 0.22f);
          icono({z.aspa.x + 6.0f, z.aspa.y + 6.0f, 12.0f, 12.0f}, icoCerrar_, hx ? 1.0f : ink[0], hx ? 0.55f : ink[1], hx ? 0.55f : ink[2],
                hx ? 1.0f : 0.55f);
        }
        const float izqT = casilla ? izqI + 30.0f : (it.plegable ? izqI + 24.0f : izqI);
        float derTexto = casilla ? derSw : derSw - 46.0f - 16.0f;
        if (!it.pastilla.empty()) {
          const float wp = textWidth(it.pastilla, 11.5f) + 20.0f;
          const Rect pa{(casilla ? derSw : derSw - 46.0f - 12.0f) - wp, y + (h - 22.0f) * 0.5f, wp, 22.0f};
          if (it.pastillaAcento) {   /* (v1.16) en azul: la red ya bajada */
            mask9(pa, 0, R(11.0f), accent[0], accent[1], accent[2], 0.22f);
            text(it.pastilla, pa.x + pa.w * 0.5f, pa.y + 4.5f, 11.5f, std::min(1.0f, accent[0] + 0.22f), std::min(1.0f, accent[1] + 0.22f),
                 std::min(1.0f, accent[2] + 0.22f), 1.0f, true);
          } else {
            mask9(pa, 0, R(11.0f), 0.30f, 0.31f, 0.36f, 0.95f);
            text(it.pastilla, pa.x + pa.w * 0.5f, pa.y + 4.5f, 11.5f, ink[0], ink[1], ink[2], 0.80f, true);
          }
          derTexto = pa.x - 12.0f;
        }
        /* (v1.18) plegable: la flechita delante del rotulo; la flechita y el rotulo pliegan (el sub 1), la pastilla enciende */
        if (it.plegable) {
          const Rect zona{izqI - 4.0f, y, derTexto - izqI + 4.0f, h};
          z.subs.push_back(zona);
          const bool hz = dentro && zona.inside(mx_, my_);
          icono({izqI, yr + 2.0f, 14.0f, 14.0f}, it.desplegada ? icoBajar_ : icoDerecha_, accent[0] + 0.22f, accent[1] + 0.25f, 1.0f,
                hz ? 1.0f : 0.80f);
        }
        textoCabe(it.rotulo, izqT, yr, 15.0f, derTexto - izqT, ink[0], ink[1], ink[2], ap);
        if (!it.nota.empty()) textoCabe(it.nota, izqT, y + 34.0f, 11.5f, derTexto - izqT, dim[0], dim[1], dim[2], 0.85f * (it.apagado ? 0.8f : 1.0f));
        if (casilla) {
          const Rect cas{izqI, yr + 1.0f, 18.0f, 18.0f};
          if (it.valor) {
            mask9(cas, 1, 4.0f, accent[0], accent[1], accent[2], ap);
            icono({cas.x + 2.0f, cas.y + 2.0f, 14.0f, 14.0f}, icoPalomita_, 0.07f, 0.06f, 0.10f, ap);
          } else {
            mask9(cas, 1, 4.0f, 0x3A / 255.0f, 0x42 / 255.0f, 0x4C / 255.0f, it.apagado ? 0.5f : 1.0f);
            mask9({cas.x + 1.5f, cas.y + 1.5f, cas.w - 3.0f, cas.h - 3.0f}, 1, 3.0f, 0x0C / 255.0f, 0x0F / 255.0f, 0x13 / 255.0f, 1.0f);
          }
          break;
        }
        const Rect sw{derSw - 46.0f, y + (h - 26.0f) * 0.5f, 46.0f, 26.0f};
        if (it.valor) mask9(sw, 0, R(13.0f), accent[0], accent[1], accent[2], 0.98f * ap);
        else mask9(sw, 0, R(13.0f), 0.28f, 0.29f, 0.33f, it.apagado ? 0.55f : 0.98f);
        mask9({it.valor ? sw.x + sw.w - 23.0f : sw.x + 3.0f, sw.y + 3.0f, 20.0f, 20.0f}, 0, R(10.0f), 0.97f, 0.97f, 0.99f, it.apagado ? 0.40f : 1.0f);
        break;
      }
      case ItemPanel::Desplegable: {
        /* (v1.16) la caja con lo elegido, su etiqueta y la flechita; abierto, la lista se pinta al final (por encima) */
        const Rect caja{izq - 4.0f, y + (h - 34.0f) * 0.5f, der - izq + 8.0f, 34.0f};
        z.subs.push_back(caja);
        const bool abierto = st_.desplegado == (int)i, hc = dentro && caja.inside(mx_, my_);
        const uint32_t fondoC = hc || abierto ? pal_.controlHov : pal_.control;
        mask9(caja, 0, R(6.0f), Hud::canal(fondoC, 0), Hud::canal(fondoC, 1), Hud::canal(fondoC, 2), 1.0f);
        float derT = caja.x + caja.w - 32.0f;
        if (it.elegido >= 0 && (size_t)it.elegido < it.ayudas.size() && !it.ayudas[(size_t)it.elegido].empty()) {
          const std::string &et = it.ayudas[(size_t)it.elegido];
          const float wp = textWidth(et, 11.5f) + 20.0f;
          const Rect pa{derT - wp, caja.y + 6.0f, wp, 22.0f};
          mask9(pa, 0, R(11.0f), accent[0], accent[1], accent[2], 0.22f);
          text(et, pa.x + pa.w * 0.5f, pa.y + 4.5f, 11.5f, std::min(1.0f, accent[0] + 0.22f), std::min(1.0f, accent[1] + 0.22f),
               std::min(1.0f, accent[2] + 0.22f), 1.0f, true);
          derT = pa.x - 10.0f;
        }
        textoCabe(it.valorTexto, caja.x + 12.0f, caja.y + 8.0f, 14.5f, derT - caja.x - 12.0f, ink[0], ink[1], ink[2], 1.0f);
        icono({caja.x + caja.w - 26.0f, caja.y + 9.0f, 16.0f, 16.0f}, icoBajar_, ink[0], ink[1], ink[2], hc || abierto ? 1.0f : 0.75f);
        if (abierto) despCaja = caja;
        break;
      }
      case ItemPanel::Atajo: {
        /* la combinacion en una tecla; esperando la nueva, la tecla se enciende */
        const float wc = std::max(130.0f, textWidth(it.valorTexto, 13.5f) + 32.0f);
        const Rect c{der - wc, y + (h - 30.0f) * 0.5f, wc, 30.0f};
        z.subs.push_back(c);
        const bool hc2 = dentro && c.inside(mx_, my_);
        if (it.valor) {
          mask9(c, 0, R(9.0f), accent[0], accent[1], accent[2], 1.0f);
        } else {
          mask9(c, 0, R(9.0f), 0.42f, 0.43f, 0.50f, hc2 ? 0.95f : 0.70f);
          mask9({c.x + 1.0f, c.y + 1.0f, c.w - 2.0f, c.h - 3.0f}, 0, R(8.0f), 0.15f, 0.155f, 0.19f, 1.0f);
        }
        text(it.valorTexto, c.x + c.w * 0.5f, c.y + 6.5f, 13.5f, ink[0], ink[1], ink[2], it.valor || hc2 ? 1.0f : 0.90f, true);
        float izqControl = c.x;
        if (it.quitar) {
          const Rect r{c.x - 8.0f - 30.0f, c.y, 30.0f, 30.0f};
          z.subs.push_back(r);
          const bool hr = dentro && r.inside(mx_, my_);
          if (hr) mask9(r, 0, R(15.0f), 1.0f, 1.0f, 1.0f, 0.14f);
          text("\u21BA", r.x + 15.0f, r.y + 5.0f, 16.0f, ink[0], ink[1], ink[2], hr ? 1.0f : 0.65f, true);
          izqControl = r.x;
        }
        textoCabe(it.rotulo, izq, y + (h - 20.0f) * 0.5f, 15.0f, izqControl - 14.0f - izq, ink[0], ink[1], ink[2], 1.0f);
        break;
      }
      case ItemPanel::Opciones: {
        float total = 6.0f;
        for (const std::string &o : it.opciones) total += textWidth(o, 13.5f) + 26.0f;
        const Rect caja{der - total, y + 10.0f, total, 34.0f};
        textoCabe(it.rotulo, izq, y + 17.0f, 15.0f, caja.x - 14.0f - izq, ink[0], ink[1], ink[2], 1.0f);
        mask9(caja, 0, R(17.0f), 0.17f, 0.175f, 0.21f, 0.98f);
        float ox = caja.x + 3.0f;
        for (size_t k = 0; k < it.opciones.size(); k++) {
          const float w = textWidth(it.opciones[k], 13.5f) + 26.0f;
          const Rect o{ox, caja.y + 3.0f, w, 28.0f};
          z.subs.push_back(o);
          const bool puesta = (int)k == it.elegido, ho = dentro && o.inside(mx_, my_);
          /* (v1.16) la puesta, como la de vkDaedalus: un fondo mas claro y letra blanca, no el acento */
          if (puesta && marco) mask9(o, 0, R(14.0f), Hud::canal(pal_.suave, 0), Hud::canal(pal_.suave, 1), Hud::canal(pal_.suave, 2), 1.0f);
          else if (puesta) mask9(o, 0, R(14.0f), accent[0], accent[1], accent[2], 0.98f);
          else if (ho) mask9(o, 0, R(14.0f), 1.0f, 1.0f, 1.0f, 0.10f);
          const bool sobreAc = puesta && !marco;
          text(it.opciones[k], o.x + o.w * 0.5f, o.y + 5.5f, 13.5f, sobreAc ? tinta[0] : ink[0], sobreAc ? tinta[1] : ink[1],
               sobreAc ? tinta[2] : ink[2], puesta || ho ? 1.0f : 0.70f, true);
          ox += w;
        }
        break;
      }
      case ItemPanel::Texto: {
        /* (v1.15) el campo: el rotulo a la izquierda y la caja a la derecha. Escribiendo, el borde del acento y la
           raya del cursor; lo que no cabe se corre para que el cursor se vea */
        /* los campos, en columna: el rotulo tiene su tercio (y se achica si no cabe) */
        const float wRot = (der - izq) * 0.30f;
        textoCabe(it.rotulo, izq, y + (h - 20.0f) * 0.5f, 15.0f, wRot, ink[0], ink[1], ink[2], 1.0f);
        const float cx0 = izq + wRot + 14.0f;
        const Rect caja{cx0, y + (h - 32.0f) * 0.5f, der - cx0, 32.0f};
        z.subs.push_back(caja);
        const bool editando = it.cursor >= 0, hcaja = dentro && caja.inside(mx_, my_);
        if (editando) mask9(caja, 0, R(9.0f), accent[0], accent[1], accent[2], 1.0f);
        else mask9(caja, 0, R(9.0f), 0.42f, 0.43f, 0.50f, hcaja ? 0.95f : 0.55f);
        mask9({caja.x + 1.5f, caja.y + 1.5f, caja.w - 3.0f, caja.h - 3.0f}, 0, R(8.0f), 0.10f, 0.105f, 0.13f, 1.0f);
        const float tam = 13.5f, tx0 = caja.x + 10.0f, util = caja.w - 20.0f, ty = caja.y + 7.5f;
        const std::string &t = it.valorTexto;
        if (t.empty() && !editando) {
          textoCabe(it.nota, tx0, ty, tam, util, dim[0], dim[1], dim[2], 0.50f);
        } else if (!editando) {
          textoCabe(t, tx0, ty, tam, util, ink[0], ink[1], ink[2], 0.95f);
        } else {
          const size_t cur = std::min((size_t)it.cursor, t.size());
          const float hasta = textWidth(t.substr(0, cur), tam);
          const float corre = hasta > util - 4.0f ? hasta - (util - 4.0f) : 0.0f;
          if (it.seleccion && !t.empty())
            mask9({tx0 - 3.0f, caja.y + 6.0f, std::min(textWidth(t, tam), util) + 6.0f, 20.0f}, 0, R(4.0f), accent[0], accent[1],
                  accent[2], 0.50f);
          const Rect antes = recorte_;
          const float x0 = std::max(antes.x, caja.x + 5.0f), x1 = std::min(antes.x + antes.w, caja.x + caja.w - 5.0f);
          const float y0 = std::max(antes.y, caja.y), y1 = std::min(antes.y + antes.h, caja.y + caja.h);
          recorte_ = {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
          text(t, tx0 - corre, ty, tam, ink[0], ink[1], ink[2], 0.97f);
          solid(tx0 - corre + hasta, caja.y + 7.0f, 1.5f, 18.0f, ink[0], ink[1], ink[2], 0.95f);
          recorte_ = antes;
        }
        break;
      }
      case ItemPanel::Linea: {
        /* (v1.15) la linea de Steam, partida en renglones, y su boton Copiar (recien copiado, verde) */
        const float wCaja = anchoLinea(it);
        const Rect caja{izq, y + 4.0f, wCaja, h - 8.0f};
        mask9(caja, 0, R(9.0f), 0.10f, 0.105f, 0.13f, 1.0f);
        const bool vacia = it.valorTexto.empty();
        const std::vector<std::string> ls = renglones(vacia ? it.rotulo : it.valorTexto, 13.0f, wCaja - 20.0f);
        for (size_t k = 0; k < ls.size(); k++)
          text(ls[k], caja.x + 10.0f, caja.y + 7.0f + 19.0f * (float)k, 13.0f, vacia ? dim[0] : ink[0], vacia ? dim[1] : ink[1],
               vacia ? dim[2] : ink[2], vacia ? 0.70f : 0.95f);
        if (!it.nota.empty()) {
          const float wb = textWidth(it.nota, 12.5f) + 28.0f;
          const Rect b{der - wb, y + 6.0f, wb, 26.0f};
          z.subs.push_back(b);
          const bool hb = dentro && b.inside(mx_, my_);
          if (it.valor) mask9(b, 0, R(13.0f), 0.27f, 0.68f, 0.42f, 0.95f);
          else mask9(b, 0, R(13.0f), accent[0], accent[1], accent[2], hb ? 1.0f : 0.92f);
          text(it.nota, b.x + wb * 0.5f, b.y + 5.5f, 12.5f, it.valor ? 1.0f : tinta[0], it.valor ? 1.0f : tinta[1],
               it.valor ? 1.0f : tinta[2], 1.0f, true);
        }
        break;
      }
      case ItemPanel::Deslizador: {
        const float anchoPista = 220.0f, cy = y + h * 0.5f;
        z.pista = {der - 70.0f - anchoPista, cy - 2.0f, anchoPista, 4.0f};
        /* (v1.16, PR #1) la flechita de volver a lo de fabrica, a la izquierda de la pista (como la de los atajos) */
        float izqControl = z.pista.x;
        if (it.fracDefecto >= 0.0f) {
          const Rect r{z.pista.x - 45.0f, cy - 15.0f, 30.0f, 30.0f};
          z.subs.push_back(r);
          const bool hr = dentro && r.inside(mx_, my_) && st_.arrastrando < 0;
          if (hr) mask9(r, 0, R(15.0f), 1.0f, 1.0f, 1.0f, 0.14f);
          text("\u21BA", r.x + 15.0f, r.y + 5.0f, 16.0f, ink[0], ink[1], ink[2], hr ? 1.0f : 0.65f, true);
          izqControl = r.x;
        }
        textoCabe(it.rotulo, izq, yr, 15.0f, izqControl - 18.0f - izq, ink[0], ink[1], ink[2], 1.0f);
        if (!it.nota.empty())
          textoCabe(it.nota, izq, y + 34.0f, 11.5f, izqControl - 18.0f - izq, dim[0], dim[1], dim[2], 0.85f);
        const float f = std::max(0.0f, std::min(1.0f, it.frac)), kx = z.pista.x + f * anchoPista;
        mask9(z.pista, 0, R(2.0f), 0.30f, 0.31f, 0.36f, 0.98f);
        mask9({z.pista.x, z.pista.y, kx - z.pista.x, 4.0f}, 0, R(2.0f), accent[0], accent[1], accent[2], 1.0f);
        /* la bola, redonda tambien en vkDaedalus (de 14) */
        if (marco) mask9({kx - 7.0f, cy - 7.0f, 14.0f, 14.0f}, 0, 7.0f, 0.97f, 0.97f, 0.99f, 1.0f);
        else mask9({kx - 9.0f, cy - 9.0f, 18.0f, 18.0f}, 0, 9.0f, 0.97f, 0.97f, 0.99f, 1.0f);
        /* (v1.16, PR #1) mientras se arrastra, el valor de donde esta la bola; si no, el guardado tal cual */
        const std::string valor = it.textoFraccion && (int)i == st_.arrastrando ? it.textoFraccion(f) : it.valorTexto;
        text(valor, der - textWidth(valor, 13.5f), cy - 9.0f, 13.5f, ink[0], ink[1], ink[2], 0.95f);
        break;
      }
      case ItemPanel::Fila: {
        textoCabe(it.rotulo, izq, y + 11.0f, 14.5f, 220.0f, ink[0], ink[1], ink[2], 1.0f);
        textoCabe(it.valorTexto, izq + 230.0f, y + 12.0f, 13.0f, der - izq - 230.0f - (it.quitar ? 36.0f : 0.0f), dim[0], dim[1],
                  dim[2], 0.90f);
        if (it.quitar) {
          const Rect q{der - 28.0f, y + 7.0f, 28.0f, 28.0f};
          z.subs.push_back(q);
          const bool hq = dentro && q.inside(mx_, my_);
          if (hq) mask9(q, 0, R(14.0f), 1.0f, 1.0f, 1.0f, 0.14f);
          icono({q.x + 7.0f, q.y + 7.0f, 14.0f, 14.0f}, icoCerrar_, ink[0], ink[1], ink[2], hq ? 1.0f : 0.55f);
        }
        break;
      }
      case ItemPanel::Juego: {
        /* una carta: la caratula (o, si Steam no la tiene, una ficha de color con la inicial), el
           nombre debajo y a su derecha la pastillita 3D / 2D. Se pica la carta y arranca; la
           pastillita cambia si ese juego entra en 3D solo. */
        const Rect cara{z.caja.x, y, z.caja.w, z.caja.w * 1.5f};
        /* (v1.16) dentro de un juego, las demas caratulas en gris y quietas: solo la de ese juego en color */
        const bool hov = hovFila && !it.gris;
        const float tenue = it.gris ? 0.40f : 1.0f;
        if (hov) mask9({cara.x - 3.0f, cara.y - 3.0f, cara.w + 6.0f, cara.h + 6.0f}, 1, R(13.0f), accent[0], accent[1], accent[2], 1.0f);
        if (it.img && it.imgW > 0 && it.imgH > 0) {
          /* (v1.16) con la interfaz subiendo, la caratula sube con ella */
          imagenes_.push_back({Rect{cara.x, cara.y + desplazY_, cara.w, cara.h},
                               Rect{panelCuerpo_.x, panelCuerpo_.y + desplazY_, panelCuerpo_.w, panelCuerpo_.h}, it.img->data(),
                               it.imgW, it.imgH, R(10.0f), v_.size(), alfaGlobal_, it.gris});
        } else {
          unsigned semilla = 2166136261u;
          for (unsigned char ch : it.rotulo) semilla = (semilla ^ ch) * 16777619u;
          const float tono = (float)(semilla % 360u) / 60.0f, xq = 1.0f - std::fabs(std::fmod(tono, 2.0f) - 1.0f);
          float cr = 0.0f, cg = 0.0f, cb = 0.0f;
          switch ((int)tono) {
            case 0: cr = 1, cg = xq; break;
            case 1: cr = xq, cg = 1; break;
            case 2: cg = 1, cb = xq; break;
            case 3: cg = xq, cb = 1; break;
            case 4: cr = xq, cb = 1; break;
            default: cr = 1, cb = xq; break;
          }
          if (it.gris) cr = cg = cb = 0.15f;
          mask9(cara, 1, R(10.0f), 0.22f + 0.38f * cr, 0.20f + 0.34f * cg, 0.30f + 0.42f * cb, 1.0f);
          size_t n = 0;
          nextCp(it.rotulo, n);
          text(it.rotulo.substr(0, n), cara.x + cara.w * 0.5f, cara.y + cara.h * 0.5f - 24.0f, 44.0f, 1.0f, 1.0f, 1.0f, 0.96f, true);
        }
        /* el nombre, cortado a lo que quepa, y a su derecha el interruptor 2D | 3D (v0.91): la bola
           a la izquierda es 2D (apagado), a la derecha 3D (encendido), con las dos letras dentro */
        const Rect chip{cara.x + cara.w - 58.0f, cara.y + cara.h + 8.0f, 58.0f, 24.0f};
        z.subs.push_back(chip);
        std::string nombre = it.rotulo;
        const float cabeN = cara.w - 66.0f;
        if (textWidth(nombre, 13.0f) > cabeN) {
          std::string corto;
          size_t k = 0;
          while (k < nombre.size()) {
            const size_t antes = k;
            nextCp(nombre, k);
            if (textWidth(corto + nombre.substr(antes, k - antes) + "..", 13.0f) > cabeN) break;
            corto += nombre.substr(antes, k - antes);
          }
          nombre = corto + "..";
        }
        text(nombre, cara.x + 2.0f, cara.y + cara.h + 13.0f, 13.0f, ink[0], ink[1], ink[2], (hov ? 1.0f : 0.90f) * tenue);
        const bool hc2 = dentro && chip.inside(mx_, my_) && !it.gris;
        if (it.valor) mask9(chip, 0, R(12.0f), accent[0], accent[1], accent[2], (hc2 ? 1.0f : 0.92f) * tenue);
        else mask9(chip, 0, R(12.0f), 0.30f, 0.31f, 0.36f, (hc2 ? 1.0f : 0.92f) * tenue);
        const Rect bola{it.valor ? chip.x + chip.w - 22.0f : chip.x + 2.0f, chip.y + 2.0f, 20.0f, 20.0f};
        mask9(bola, 0, R(10.0f), 0.97f, 0.97f, 0.99f, tenue);
        /* la letra de lo que esta puesto va en la bola; la otra, tenue, en el hueco que deja (v0.92:
           las dos a la misma altura y la bola a 2 px de cada borde; antes quedaba 3 arriba y 1 abajo) */
        text(it.valor ? "3D" : "2D", bola.x + bola.w * 0.5f, chip.y + 6.0f, 10.5f, 0.12f, 0.12f, 0.16f, tenue, true);
        text(it.valor ? "2D" : "3D", it.valor ? chip.x + 19.0f : chip.x + chip.w - 19.0f, chip.y + 6.0f, 10.5f, 1.0f, 1.0f, 1.0f, 0.55f * tenue, true);
        /* (v1.15) al pasar el raton, arriba a la derecha de la caratula, el engranaje de sus integraciones (Rodolfo:
           «un engranaje que aparezca al pasar el mouse donde te puse en la foto»). Es el sub 2 */
        /* (v1.18) y a su izquierda el de sus efectos de ReShade (Rodolfo: «Add a shaders icon to each game on the games tab»),
           el sub 3. Cada uno sale si tiene su ayuda: dentro de un juego, el engranaje no (Integraciones va en gris) y los
           efectos si, en el de ese juego */
        if (hov && !it.ayudas.empty()) {
          auto boton = [&](const Rect &b, const Rect &ico) {
            const bool hb = dentro && b.inside(mx_, my_);
            mask9(b, 0, R(15.0f), 0.0f, 0.0f, 0.0f, hb ? 0.88f : 0.66f);
            if (hb) mask9({b.x - 1.0f, b.y - 1.0f, b.w + 2.0f, b.h + 2.0f}, 0, R(16.0f), accent[0], accent[1], accent[2], 0.55f);
            icono({b.x + 6.0f, b.y + 6.0f, 18.0f, 18.0f}, ico, 1.0f, 1.0f, 1.0f, hb ? 1.0f : 0.88f);
          };
          const Rect eng{cara.x + cara.w - 38.0f, cara.y + 8.0f, 30.0f, 30.0f};
          const bool conEng = !it.ayudas[0].empty(), conEf = it.ayudas.size() > 1 && !it.ayudas[1].empty();
          const Rect efx{conEng ? eng.x - 36.0f : eng.x, eng.y, 30.0f, 30.0f};
          z.subs.push_back(conEng ? eng : Rect{-1e6f, -1e6f, 0.0f, 0.0f});
          z.subs.push_back(conEf ? efx : Rect{-1e6f, -1e6f, 0.0f, 0.0f});
          if (conEng) boton(eng, icoEngranaje_);
          if (conEf) boton(efx, icoEfectos_);
        }
        break;
      }
    }
  }
  /* (v1.18) arrastrando un efecto por su agarre: su bloque (la cabecera y sus ajustes) en el acento y una raya donde caeria */
  if (st_.moviendoFila >= 0 && (size_t)st_.moviendoFila < st_.items.size()) {
    const int bl = st_.items[(size_t)st_.moviendoFila].bloque;
    std::vector<size_t> cabezas;
    for (size_t i = 0; i < st_.items.size() && i < zonas_.size(); i++) {
      if (st_.items[i].bloque == bl && zonas_[i].caja.w > 0.0f) {
        const Rect &c = zonas_[i].caja;
        mask9({c.x, c.y + 1.0f, c.w, c.h - 2.0f}, 1, 6.0f, accent[0], accent[1], accent[2], 0.14f);
      }
      if (st_.items[i].arrastrable && (int)i != st_.moviendoFila) cabezas.push_back(i);
    }
    const int k = destinoFila();
    if (k >= 0 && !cabezas.empty()) {
      /* encima de la cabecera k (de las que quedan), o debajo de la ultima fila del ultimo bloque */
      float yRaya = 0.0f, xRaya = 0.0f, wRaya = 0.0f;
      if ((size_t)k < cabezas.size()) {
        const Rect &c = zonas_[cabezas[(size_t)k]].caja;
        yRaya = c.y, xRaya = c.x, wRaya = c.w;
      } else {
        const int ultimo = st_.items[cabezas.back()].bloque;
        for (size_t i = 0; i < st_.items.size() && i < zonas_.size(); i++)
          if (st_.items[i].bloque == ultimo && zonas_[i].caja.w > 0.0f) {
            const Rect &c = zonas_[i].caja;
            yRaya = std::max(yRaya, c.y + c.h), xRaya = c.x, wRaya = c.w;
          }
      }
      if (wRaya > 0.0f) mask9({xRaya + 4.0f, yRaya - 1.5f, wRaya - 8.0f, 3.0f}, 0, 1.5f, accent[0], accent[1], accent[2], 1.0f);
    }
  }
  hayRecorte_ = false;
  /* la ayuda de lo que hay bajo el raton (se pinta al final, por encima de todo) */
  std::string ayudaRaton;
  if (dentro && !st_.menuIdioma && st_.desplegado < 0 && st_.arrastrando < 0)   /* (v1.16, PR #1) arrastrando, sin ayuda encima; (v1.16) ni con un desplegable abierto */
    for (size_t i = 0; i < st_.items.size() && i < zonas_.size(); i++) {
      const ItemPanel &it = st_.items[i];
      const ZonaItem &z = zonas_[i];
      if (z.caja.w <= 0.0f || !z.caja.inside(mx_, my_)) continue;
      if (it.tipo == ItemPanel::Opciones && !it.ayudas.empty())
        for (size_t k = 0; k < z.subs.size() && k < it.ayudas.size(); k++)
          if (z.subs[k].inside(mx_, my_)) ayudaRaton = it.ayudas[k];
      /* (v1.15) el engranaje de una caratula: su ayuda va en ayudas[0] */
      if (it.tipo == ItemPanel::Juego && z.subs.size() > 1 && !it.ayudas.empty() && z.subs[1].inside(mx_, my_))
        ayudaRaton = it.ayudas[0];
      if (it.tipo == ItemPanel::Juego && z.subs.size() > 2 && it.ayudas.size() > 1 && z.subs[2].inside(mx_, my_))
        ayudaRaton = it.ayudas[1];   /* (v1.18) el de los efectos */
      /* (v1.16, PR #1) la flechita de un deslizador: a que valor vuelve */
      if (it.tipo == ItemPanel::Deslizador && it.textoFraccion && it.fracDefecto >= 0.0f && !z.subs.empty() && z.subs[0].inside(mx_, my_))
        ayudaRaton = idioma::trf("Volver a {}", {it.textoFraccion(it.fracDefecto)});
      if (ayudaRaton.empty()) ayudaRaton = it.ayuda;
      break;
    }
  /* la barrita de la derecha, solo si hay mas lista que caja */
  if (tope > 0.0f) {
    const float largo = std::max(30.0f, panelCuerpo_.h * panelCuerpo_.h / contenidoAlto_);
    const float sy = panelCuerpo_.y + (panelCuerpo_.h - largo) * (st_.corrida / tope);
    mask9({panelCuerpo_.x + panelCuerpo_.w - 7.0f, sy, 4.0f, largo}, 0, 2.0f, 1.0f, 1.0f, 1.0f, 0.28f);
  }
  /* el pie */
  solid(pieCaja.x + 20.0f, pieCaja.y + 2.0f, pieCaja.w - 40.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.08f);
  float pieMax = pieCaja.w - 56.0f - textWidth(st_.version, 12.0f) - 20.0f;
  /* (v1.14) en la pestana Juegos, en medio: el parametro de Steam y su boton Copiar (recien copiado, en verde y
     diciendo «Copiado» un momento); la frase del pie se queda a su izquierda */
  panelCopiar_ = {-10.0f, -10.0f, 0.0f, 0.0f};
  if (!st_.pieParametro.empty() && st_.pieInterruptor) {
    /* (v1.16) la casilla de ODYSSEYVK=1 para todos, en medio del pie: casilla y rotulo se pican juntos */
    const float tamP = 13.0f, ty = pieCaja.y + 14.0f;
    const float wP = textWidth(st_.pieParametro, tamP), grupo = 18.0f + 10.0f + wP;
    /* la casilla quieta (en el centro, menos un tanto): si cambia la cuenta de al lado, solo crece el rotulo */
    const float gx = std::max(pieCaja.x + 200.0f, pieCaja.x + pieCaja.w * 0.5f - 130.0f);
    panelCopiar_ = {gx - 8.0f, ty - 6.0f, grupo + 16.0f, 28.0f};
    const bool hb = panelCopiar_.inside(mx_, my_);
    if (hb) mask9(panelCopiar_, 0, R(4.0f), 1.0f, 1.0f, 1.0f, 0.06f);
    const Rect cas{gx, ty - 1.0f, 18.0f, 18.0f};
    if (st_.pieValor) {
      mask9(cas, 1, 4.0f, accent[0], accent[1], accent[2], 1.0f);
      icono({cas.x + 2.0f, cas.y + 2.0f, 14.0f, 14.0f}, icoPalomita_, 0.07f, 0.06f, 0.10f, 1.0f);
    } else {
      mask9(cas, 1, 4.0f, Hud::canal(pal_.filoCasilla, 0), Hud::canal(pal_.filoCasilla, 1), Hud::canal(pal_.filoCasilla, 2), 1.0f);
      mask9({cas.x + 1.5f, cas.y + 1.5f, cas.w - 3.0f, cas.h - 3.0f}, 1, 3.0f, Hud::canal(pal_.pozo, 0), Hud::canal(pal_.pozo, 1),
            Hud::canal(pal_.pozo, 2), 1.0f);
    }
    text(st_.pieParametro, gx + 28.0f, ty - 0.5f, tamP, ink[0], ink[1], ink[2], hb ? 1.0f : 0.90f);
    if (hb && !st_.pieAyuda.empty()) ayudaRaton = st_.pieAyuda;   /* el recordatorio, solo al pasar el raton */
    pieMax = std::min(pieMax, gx - (pieCaja.x + 28.0f) - 16.0f);
  } else if (!st_.pieParametro.empty()) {
    const float tamP = 13.0f, tamB = 12.5f, ty = pieCaja.y + 14.0f;
    const std::string &etq = st_.pieCopiado ? st_.pieCopiadoTexto : st_.pieCopiar;
    const float wP = textWidth(st_.pieParametro, tamP);
    const float wB = std::max(textWidth(st_.pieCopiar, tamB), textWidth(st_.pieCopiadoTexto, tamB)) + 28.0f;
    const float grupo = wP + 12.0f + wB;
    const float gx = pieCaja.x + (pieCaja.w - grupo) * 0.5f;
    text(st_.pieParametro, gx, ty - 0.5f, tamP, ink[0], ink[1], ink[2], 0.95f);
    panelCopiar_ = {gx + wP + 12.0f, ty - 5.5f, wB, 26.0f};
    const bool hb = panelCopiar_.inside(mx_, my_);
    if (st_.pieCopiado) mask9(panelCopiar_, 0, 13.0f, 0.27f, 0.68f, 0.42f, 0.95f);
    else mask9(panelCopiar_, 0, 13.0f, accent[0], accent[1], accent[2], hb ? 1.0f : 0.92f);
    text(etq, panelCopiar_.x + wB * 0.5f, ty, tamB, st_.pieCopiado ? 1.0f : tinta[0], st_.pieCopiado ? 1.0f : tinta[1],
         st_.pieCopiado ? 1.0f : tinta[2], 1.0f, true);
    if (hb && !st_.pieAyuda.empty()) ayudaRaton = st_.pieAyuda;
    pieMax = std::min(pieMax, gx - (pieCaja.x + 28.0f) - 16.0f);
  }
  if (!st_.pie.empty() && pieMax > 40.0f)
    textoCabe(st_.pie, pieCaja.x + 28.0f, pieCaja.y + 14.0f, 12.5f, pieMax, ink[0], ink[1], ink[2], 0.88f);
  if (!st_.version.empty())
    text(st_.version, pieCaja.x + pieCaja.w - 28.0f - textWidth(st_.version, 12.0f), pieCaja.y + 14.0f, 12.0f, dim[0], dim[1], dim[2],
         0.75f);
  /* la lista de los idiomas, colgada del boton y por encima de las filas */
  panelIdiomaOpc_.clear();
  if (st_.menuIdioma) {
    float wl = 0.0f;
    for (int k = 0; k < idioma::kLenguas; k++) wl = std::max(wl, textWidth(idioma::nombre(k), 14.0f));
    wl = std::max(wl + 92.0f, panelIdioma_.w);   /* codigo, nombre y la palomita sin pisarse */
    const float fila = 34.0f, alto = 12.0f + fila * (float)idioma::kLenguas;
    const Rect m{std::min(panelIdioma_.x, px + pw - 20.0f - wl), panelIdioma_.y + panelIdioma_.h + 8.0f, wl, alto};
    mask9({m.x - 1.0f, m.y - 1.0f, m.w + 2.0f, m.h + 2.0f}, 1, 13.0f, 0.42f, 0.43f, 0.50f, 0.85f);
    mask9(m, 1, 12.0f, 0.0f, 0.0f, 0.0f, 0.90f);
    for (int k = 0; k < idioma::kLenguas; k++) {
      const Rect o{m.x + 6.0f, m.y + 6.0f + fila * (float)k, m.w - 12.0f, fila};
      panelIdiomaOpc_.push_back(o);
      const bool puesta = k == lengua, ho = o.inside(mx_, my_);
      if (puesta) mask9(o, 0, 12.0f, accent[0], accent[1], accent[2], 0.95f);
      else if (ho) mask9(o, 0, 12.0f, 1.0f, 1.0f, 1.0f, 0.10f);
      std::string cod = idioma::codigo(k);
      for (char &ch : cod) ch = (char)std::toupper((unsigned char)ch);
      text(cod, o.x + 12.0f, o.y + 9.5f, 11.0f, ink[0], ink[1], ink[2], puesta || ho ? 0.85f : 0.55f);
      text(idioma::nombre(k), o.x + 40.0f, o.y + 7.5f, 14.0f, ink[0], ink[1], ink[2], puesta || ho ? 1.0f : 0.85f);
      if (puesta) icono({o.x + o.w - 26.0f, o.y + 9.0f, 16.0f, 16.0f}, icoPalomita_, 1.0f, 1.0f, 1.0f, 1.0f);
    }
  } else if (hi) {
    /* al pasar por encima del boton cerrado: «Idioma: Español», en el idioma puesto */
    const std::string r = std::string(idioma::tr("Idioma")) + ": " + idioma::nombre(lengua);
    const float w = textWidth(r, 12.5f) + 24.0f;
    const Rect rr{std::min(panelIdioma_.x, px + pw - 20.0f - w), panelIdioma_.y + panelIdioma_.h + 8.0f, w, 30.0f};
    mask9(rr, 0, 10.0f, 0.0f, 0.0f, 0.0f, 0.90f);
    text(r, rr.x + 12.0f, rr.y + 8.0f, 12.5f, ink[0], ink[1], ink[2], 0.95f);
  }
  /* (v1.16) el desplegable abierto: su lista, por encima de las filas y del pie. Debajo de la caja, o encima si no cabe;
     cada opcion con su etiqueta en azul (esta) o en gris (se baja al picarla) y la palomita en la puesta */
  panelDespOpc_.clear();
  panelDespCaja_ = {-10.0f, -10.0f, 0.0f, 0.0f};
  /* (v1.18) el desplegable por grupos (ItemPanel::grupos): en vez de colgar de su caja ocupa el cuerpo de la tarjeta, los
     grupos lado a lado (en la columna de la vista previa, uno encima del otro: la columna es la region de entrada), cada
     uno con su titulo y cuantas lleva, sus opciones en las columnas de ~200 puntos que quepan y, si no caben, los botones
     de correr arriba y abajo, que corren solos con el raton encima (autodesplazar) */
  {
    const Rect nada{-10.0f, -10.0f, 0.0f, 0.0f};
    int grupoRaton = -1, dirRaton = 0;
    for (int g = 0; g < 2; g++) despGrupo_[g] = despBoton_[g][0] = despBoton_[g][1] = nada;
    despCerrar_ = nada;
    const bool porGrupos = st_.desplegado >= 0 && (size_t)st_.desplegado < st_.items.size() &&
                           st_.items[(size_t)st_.desplegado].tipo == ItemPanel::Desplegable && !st_.items[(size_t)st_.desplegado].grupos.empty();
    if (porGrupos) {
      const ItemPanel &it = st_.items[(size_t)st_.desplegado];
      const bool columna = st_.depuracion || st_.vistaEfectos;
      /* en el panel, de orilla a orilla y hasta abajo (la region de entrada es la pantalla entera: hay sitio para mas
         columnas que en la tarjeta); en la columna, la columna */
      const Rect zona = columna ? Rect{panelCuerpo_.x, panelCuerpo_.y, panelCuerpo_.w - 4.0f, panelCuerpo_.h}
                                : Rect{panelCaja_.x + 24.0f, panelCuerpo_.y, panelCaja_.w - 48.0f,
                                       std::max(panelCuerpo_.h, panelCaja_.y + panelCaja_.h - panelCuerpo_.y - 24.0f)};
      panelDespCaja_ = zona;
      mask9(zona, 1, 8.0f, Hud::canal(pal_.filo, 0), Hud::canal(pal_.filo, 1), Hud::canal(pal_.filo, 2), 1.0f);
      mask9({zona.x + 1.0f, zona.y + 1.0f, zona.w - 2.0f, zona.h - 2.0f}, 1, 7.0f, Hud::canal(pal_.tarjeta, 0),
            Hud::canal(pal_.tarjeta, 1), Hud::canal(pal_.tarjeta, 2), 1.0f);
      panelDespOpc_.assign(it.opciones.size(), nada);
      /* el aspa: lo cierra (en la columna no hay fuera donde picar) */
      despCerrar_ = {zona.x + zona.w - 34.0f, zona.y + 8.0f, 26.0f, 26.0f};
      {
        const bool hc = despCerrar_.inside(mx_, my_);
        if (hc) mask9(despCerrar_, 0, R(13.0f), Hud::canal(pal_.controlHov, 0), Hud::canal(pal_.controlHov, 1), Hud::canal(pal_.controlHov, 2), 1.0f);
        icono({despCerrar_.x + 4.0f, despCerrar_.y + 4.0f, 18.0f, 18.0f}, icoCerrar_, ink[0], ink[1], ink[2], hc ? 1.0f : 0.75f);
      }
      const int nG = std::min<int>(2, (int)it.grupos.size());
      const float pad = 12.0f, entre = 16.0f, hTit = 30.0f, hBot = 26.0f, hCel = 32.0f;
      for (int g = 0; g < nG; g++) {
        const Rect pane = columna ? Rect{zona.x + pad, zona.y + pad + (float)g * ((zona.h - 2.0f * pad - entre * (float)(nG - 1)) / (float)nG + entre),
                                         zona.w - 2.0f * pad, (zona.h - 2.0f * pad - entre * (float)(nG - 1)) / (float)nG}
                                  : Rect{zona.x + pad + (float)g * ((zona.w - 2.0f * pad - entre * (float)(nG - 1)) / (float)nG + entre),
                                         zona.y + pad, (zona.w - 2.0f * pad - entre * (float)(nG - 1)) / (float)nG, zona.h - 2.0f * pad};
        despGrupo_[g] = pane;
        std::vector<size_t> ks;
        for (size_t k = 0; k < it.opciones.size(); k++)
          if (k < it.grupoOpc.size() && it.grupoOpc[k] == g) ks.push_back(k);
        fuente_ = hayBarlow_ ? 2 : 0;
        textoCabe(it.grupos[(size_t)g], pane.x + 4.0f, pane.y + 5.0f, 15.0f, pane.w - 60.0f, 1.0f, 1.0f, 1.0f, 0.95f);
        fuente_ = hayBarlow_ ? 1 : 0;
        const std::string cuantas = std::to_string(ks.size());
        const float bajoAspa = (columna ? g == 0 : g == nG - 1) ? 34.0f : 0.0f;   /* el grupo de arriba a la derecha deja sitio al aspa */
        text(cuantas, pane.x + pane.w - 4.0f - bajoAspa - textWidth(cuantas, 12.5f), pane.y + 7.0f, 12.5f, dim[0], dim[1], dim[2], 0.9f);
        const Rect rejilla{pane.x, pane.y + hTit + hBot, pane.w, std::max(hCel, pane.h - hTit - 2.0f * hBot)};
        const int cols = std::max(1, (int)(rejilla.w / 200.0f));
        const float wCel = rejilla.w / (float)cols;
        const float alto = (float)((ks.size() + (size_t)cols - 1) / (size_t)cols) * hCel;
        despTope_[g] = std::max(0.0f, alto - rejilla.h);
        float &corr = st_.despCorrida[g];
        corr = std::max(0.0f, std::min(despTope_[g], corr));
        /* los botones de correr: solo si se puede ir para ese lado */
        auto boton = [&](const Rect &b, int dir, bool puede) {
          if (!puede) return;
          despBoton_[g][dir > 0 ? 1 : 0] = b;
          const bool hb = b.inside(mx_, my_);
          if (hb) grupoRaton = g, dirRaton = dir;
          const uint32_t c = hb ? pal_.controlHov : pal_.control;
          mask9(b, 0, R(6.0f), Hud::canal(c, 0), Hud::canal(c, 1), Hud::canal(c, 2), 1.0f);
          icono({b.x + b.w * 0.5f - 8.0f, b.y + (b.h - 16.0f) * 0.5f, 16.0f, 16.0f}, dir < 0 ? icoFlecha_ : icoBajar_, ink[0], ink[1],
                ink[2], hb ? 1.0f : 0.75f);
        };
        boton({pane.x, pane.y + hTit, pane.w, hBot - 4.0f}, -1, corr > 0.5f);
        boton({pane.x, rejilla.y + rejilla.h + 4.0f, pane.w, hBot - 4.0f}, 1, corr < despTope_[g] - 0.5f);
        /* las opciones, recortadas a su rejilla */
        const Rect antes = recorte_;
        const bool habia = hayRecorte_;
        recorte_ = rejilla, hayRecorte_ = true;
        for (size_t j = 0; j < ks.size(); j++) {
          const size_t k = ks[j];
          const Rect o{rejilla.x + (float)(j % (size_t)cols) * wCel + 2.0f, rejilla.y + (float)(j / (size_t)cols) * hCel - corr + 2.0f,
                       wCel - 4.0f, hCel - 4.0f};
          if (o.y + o.h < rejilla.y || o.y > rejilla.y + rejilla.h) continue;
          const float y0 = std::max(o.y, rejilla.y), y1 = std::min(o.y + o.h, rejilla.y + rejilla.h);
          panelDespOpc_[k] = {o.x, y0, o.w, y1 - y0};
          const bool ho = panelDespOpc_[k].inside(mx_, my_) && dirRaton == 0;
          if (ho) mask9(o, 0, R(6.0f), Hud::canal(pal_.controlHov, 0), Hud::canal(pal_.controlHov, 1), Hud::canal(pal_.controlHov, 2), 1.0f);
          textoCabe(it.opciones[k], o.x + 10.0f, o.y + 7.0f, 14.0f, o.w - 20.0f, ink[0], ink[1], ink[2], ho ? 1.0f : 0.92f);
        }
        recorte_ = antes, hayRecorte_ = habia;
      }
    }
    if (grupoRaton != autoGrupo_ || dirRaton != autoDir_)
      autoDesde_ = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    autoGrupo_ = grupoRaton, autoDir_ = dirRaton;
  }
  if (st_.desplegado >= 0 && (size_t)st_.desplegado < st_.items.size() && despCaja.w > 0.0f &&
      st_.items[(size_t)st_.desplegado].tipo == ItemPanel::Desplegable && st_.items[(size_t)st_.desplegado].grupos.empty()) {
    const ItemPanel &it = st_.items[(size_t)st_.desplegado];
    const float hOp = 36.0f, hLista = hOp * (float)it.opciones.size() + 8.0f;
    /* dentro de la caja del panel (la region de entrada: en la vista de depuracion, la columna) */
    float yL = despCaja.y + despCaja.h + 4.0f;
    if (yL + hLista > panelCaja_.y + panelCaja_.h - 4.0f) yL = std::max(panelCaja_.y + 4.0f, despCaja.y - 4.0f - hLista);
    const Rect lista{despCaja.x, yL, despCaja.w, hLista};
    panelDespCaja_ = lista;
    mask9(lista, 1, 8.0f, Hud::canal(pal_.filo, 0), Hud::canal(pal_.filo, 1), Hud::canal(pal_.filo, 2), 1.0f);
    mask9({lista.x + 1.0f, lista.y + 1.0f, lista.w - 2.0f, lista.h - 2.0f}, 1, 7.0f, Hud::canal(pal_.tarjeta, 0),
          Hud::canal(pal_.tarjeta, 1), Hud::canal(pal_.tarjeta, 2), 0.99f);
    for (size_t k = 0; k < it.opciones.size(); k++) {
      const Rect o{lista.x + 4.0f, yL + 4.0f + (float)k * hOp, lista.w - 8.0f, hOp};
      panelDespOpc_.push_back(o);
      const int est = k < it.estadosOpc.size() ? it.estadosOpc[k] : 0;
      const bool ho = o.inside(mx_, my_) && est != 2;
      if (ho) mask9(o, 0, R(6.0f), Hud::canal(pal_.controlHov, 0), Hud::canal(pal_.controlHov, 1), Hud::canal(pal_.controlHov, 2), 1.0f);
      if ((int)k == it.elegido)
        icono({o.x + 10.0f, o.y + 10.0f, 16.0f, 16.0f}, icoPalomita_, accent[0], accent[1], accent[2], 1.0f);
      float derO = o.x + o.w - 10.0f;
      if (k < it.ayudas.size() && !it.ayudas[k].empty()) {
        const std::string &et = it.ayudas[k];
        const float wp = textWidth(et, 11.5f) + 20.0f;
        const Rect pa{derO - wp, o.y + 7.0f, wp, 22.0f};
        if (est == 0) {
          mask9(pa, 0, R(11.0f), accent[0], accent[1], accent[2], 0.22f);
          text(et, pa.x + pa.w * 0.5f, pa.y + 4.5f, 11.5f, std::min(1.0f, accent[0] + 0.22f), std::min(1.0f, accent[1] + 0.22f),
               std::min(1.0f, accent[2] + 0.22f), 1.0f, true);
        } else {
          mask9(pa, 0, R(11.0f), 0.30f, 0.31f, 0.36f, 0.95f);
          text(et, pa.x + pa.w * 0.5f, pa.y + 4.5f, 11.5f, ink[0], ink[1], ink[2], est == 2 ? 0.45f : 0.80f, true);
        }
        derO = pa.x - 10.0f;
      }
      textoCabe(it.opciones[k], o.x + 34.0f, o.y + 9.0f, 14.5f, derO - o.x - 34.0f, ink[0], ink[1], ink[2], est == 1 ? 0.6f : 1.0f);
    }
  }
  /* (v1.17) el menu de una tesela (el del cuadrito de los FPS): lo que se puede vigilar, con su casilla. Cuelga de la
     tesela, pegado a su orilla derecha si es de las de la derecha; se queda abierto al marcar */
  panelMenuOpc_.clear();
  if (st_.teselaMenu >= 0 && panelMenuAncla_.w > 0.0f && !st_.menuOpciones.empty()) {
    const float hOp = 34.0f;
    float wL = 0.0f;
    for (const std::string &o : st_.menuOpciones) wL = std::max(wL, textWidth(o, 14.0f));
    wL = std::max(panelMenuAncla_.w, 12.0f + 18.0f + 12.0f + wL + 18.0f);
    const float hL = hOp * (float)st_.menuOpciones.size() + 8.0f;
    const bool derecha = panelMenuAncla_.x + panelMenuAncla_.w * 0.5f > (float)vw * 0.5f;
    float xL = derecha ? panelMenuAncla_.x + panelMenuAncla_.w - wL : panelMenuAncla_.x;
    xL = std::max(8.0f, std::min(xL, (float)vw - 8.0f - wL));
    const float yL = std::min(panelMenuAncla_.y + panelMenuAncla_.h + 4.0f, std::max(8.0f, (float)vh - 8.0f - hL));
    const Rect lista{xL, yL, wL, hL};
    mask9(lista, 1, 8.0f, Hud::canal(pal_.filo, 0), Hud::canal(pal_.filo, 1), Hud::canal(pal_.filo, 2), 1.0f);
    mask9({lista.x + 1.0f, lista.y + 1.0f, lista.w - 2.0f, lista.h - 2.0f}, 1, 7.0f, Hud::canal(pal_.tarjeta, 0),
          Hud::canal(pal_.tarjeta, 1), Hud::canal(pal_.tarjeta, 2), 0.99f);
    for (size_t k = 0; k < st_.menuOpciones.size(); k++) {
      const Rect o{lista.x + 4.0f, yL + 4.0f + (float)k * hOp, lista.w - 8.0f, hOp};
      panelMenuOpc_.push_back(o);
      if (o.inside(mx_, my_))
        mask9(o, 0, R(6.0f), Hud::canal(pal_.controlHov, 0), Hud::canal(pal_.controlHov, 1), Hud::canal(pal_.controlHov, 2), 1.0f);
      const bool marcada = k < st_.menuMarcadas.size() && st_.menuMarcadas[k];
      const Rect cas{o.x + 8.0f, o.y + (hOp - 18.0f) * 0.5f, 18.0f, 18.0f};
      if (marcada) {
        mask9(cas, 1, 4.0f, accent[0], accent[1], accent[2], 1.0f);
        icono({cas.x + 2.0f, cas.y + 2.0f, 14.0f, 14.0f}, icoPalomita_, 0.07f, 0.06f, 0.10f, 1.0f);
      } else {
        mask9(cas, 1, 4.0f, 0.55f, 0.57f, 0.62f, 1.0f);
        mask9({cas.x + 1.5f, cas.y + 1.5f, cas.w - 3.0f, cas.h - 3.0f}, 1, 3.0f, Hud::canal(pal_.tarjeta, 0),
              Hud::canal(pal_.tarjeta, 1), Hud::canal(pal_.tarjeta, 2), 1.0f);
      }
      textoCabe(st_.menuOpciones[k], cas.x + 30.0f, o.y + 9.0f, 14.0f, o.w - 46.0f, ink[0], ink[1], ink[2], 1.0f);
    }
  }
  /* la ayuda del valor bajo el raton: una caja de hasta 460 puntos, partida en renglones */
  if (!ayudaRaton.empty()) {
    const float tam = 12.5f;
    const std::vector<std::string> lineas = renglones(ayudaRaton, tam, 460.0f);
    float w = 0.0f;
    for (const std::string &l : lineas) w = std::max(w, textWidth(l, tam));
    w += 24.0f;
    const float h = 14.0f + 19.0f * (float)lineas.size();
    float bx = std::min(mx_ + 14.0f, px + pw - 12.0f - w), by = my_ + 18.0f;
    if (by + h > py + ph - 8.0f) by = my_ - 10.0f - h;   /* sin sitio debajo: encima del raton */
    bx = std::max(px + 12.0f, bx);
    cajaAyuda(lineas, bx, by, tam);
  }
  /* la esquina de abajo a la derecha (rama experimental): seis puntitos en escalera; arrastrandola cambia
     el tamano del panel */
  if (!marco) {   /* (v1.16) a pantalla completa no hay tamano que cambiar */
    panelEsquina_ = {px + pw - 28.0f, py + ph - 28.0f, 24.0f, 24.0f};
    const bool he = panelEsquina_.inside(mx_, my_);
    const float c = he ? 0.95f : 0.55f;
    for (int fila = 0; fila < 3; fila++)
      for (int k = 0; k <= fila; k++)
        mask9({panelEsquina_.x + 18.0f - 5.0f * (float)k, panelEsquina_.y + 8.0f + 5.0f * (float)fila, 3.0f, 3.0f}, 0, 1.5f, c, c,
              c + 0.02f, 1.0f);
  }
  /* el rotulo del pin, al pasar por encima */
  if (hfij) {
    const std::string r = idioma::tr(st_.fijado ? "Fijado encima: un clic en el juego no cierra el panel. Pica para soltarlo."
                                                 : "Fijar encima: que un clic en el juego no cierre el panel.");
    const float w = textWidth(r, 12.5f) + 24.0f;
    const Rect rr{panelFijar_.x, panelFijar_.y + panelFijar_.h + 8.0f, w, 30.0f};
    mask9(rr, 0, 10.0f, 0.0f, 0.0f, 0.0f, 0.90f);
    text(r, rr.x + 12.0f, rr.y + 8.0f, 12.5f, ink[0], ink[1], ink[2], 0.95f);
  }
  /* el rotulo del boton de actualizar, al pasar por encima: por encima de todo lo demas */
  if (ha && !rotuloActualizar.empty() && !st_.menuIdioma) {
    const float w = textWidth(rotuloActualizar, 12.5f) + 24.0f;
    const Rect &an = anclaActualizar.w > 0.0f ? anclaActualizar : panelActualizar_;
    const float derR = anclaActualizar.w > 0.0f ? an.x + an.w : panelCerrar_.x + panelCerrar_.w;
    const Rect r{std::max(px + 20.0f, derR - w), an.y + an.h + 8.0f, w, 30.0f};
    mask9(r, 0, 10.0f, 0.0f, 0.0f, 0.0f, 0.90f);
    text(rotuloActualizar, r.x + 12.0f, r.y + 8.0f, 12.5f, ink[0], ink[1], ink[2], 0.95f);
  }
  fuente_ = 0;   /* (v1.16) lo demas (la pastilla, la tarjeta) con la letra de siempre, quieto y opaco */
  alfaGlobal_ = 1.0f;
  desplazY_ = 0.0f;
}

/* (v1.16) el icono de una tesela chica (State::Tesela::icono) */
const Hud::Rect &Hud::iconoTesela(int k) const {
  switch (k) {
    case 1: return icoChip_;
    case 2: return icoCapas_;
    case 3: return icoCabeza_;
    case 4: return icoBarras_;
    case 5: return icoFlecha_;
    case 6: return icoGlobo_;
    case 7: return icoCubo_;
    case 8: return icoEngranaje_;
    default: return icoAnillo_;
  }
}

bool Hud::panelBounds(float *x, float *y, float *w, float *h) const {
  if (!st_.panel || panelCaja_.w <= 0.0f) return false;
  if (x) *x = panelCaja_.x * sc_;
  if (y) *y = panelCaja_.y * sc_;
  if (w) *w = panelCaja_.w * sc_;
  if (h) *h = panelCaja_.h * sc_;
  return true;
}

Hud::Hit Hud::hitDetalle(float mxa, float mya, int *item, int *sub, float *frac) const {
  if (item) *item = -1;
  if (sub) *sub = 0;
  if (frac) *frac = 0.0f;
  if (!st_.panel) return hit(mxa, mya);
  const float mx = mxa / sc_, my = mya / sc_;
  /* la lista de idiomas cuelga por encima de todo (y puede salirse un poco por debajo de la cabecera) */
  if (st_.menuIdioma)
    for (size_t k = 0; k < panelIdiomaOpc_.size(); k++)
      if (panelIdiomaOpc_[k].inside(mx, my)) {
        if (sub) *sub = (int)k;
        return HitPanelIdiomaOpcion;
      }
  /* (v1.17) el menu de una tesela: 2000 + la opcion */
  if (st_.teselaMenu >= 0)
    for (size_t k = 0; k < panelMenuOpc_.size(); k++)
      if (panelMenuOpc_[k].inside(mx, my)) {
        if (sub) *sub = 2000 + (int)k;
        return HitPanelTesela;
      }
  /* (v1.16) las opciones del desplegable abierto: su fila y 100 + el numero de la opcion */
  if (st_.desplegado >= 0)
    for (size_t k = 0; k < panelDespOpc_.size(); k++)
      if (panelDespOpc_[k].inside(mx, my)) {
        if (item) *item = st_.desplegado;
        if (sub) *sub = 100 + (int)k;
        return HitPanelItem;
      }
  /* (v1.18) el resto de un desplegable por grupos (su fondo, sus titulos, sus botones de correr): el sub 50, que no hace
     nada (ni lo cierra) */
  if (st_.desplegado >= 0 && (size_t)st_.desplegado < st_.items.size() && !st_.items[(size_t)st_.desplegado].grupos.empty() &&
      panelDespCaja_.inside(mx, my)) {
    if (item) *item = st_.desplegado;
    /* el aspa 51; cada boton de correr el suyo (52 a 55: el panel repinta al cambiar lo que hay bajo el raton, y asi
       empieza a correr), y el fondo 50 */
    int s = despCerrar_.inside(mx, my) ? 51 : 50;
    for (int g = 0; g < 2; g++)
      for (int d = 0; d < 2; d++)
        if (despBoton_[g][d].inside(mx, my)) s = 52 + g * 2 + d;
    if (sub) *sub = s;
    return HitPanelItem;
  }
  if (!panelCaja_.inside(mx, my)) return HitNone;
  if (panelCerrar_.inside(mx, my)) return HitPanelCerrar;
  if (st_.marco && panelVolver_.inside(mx, my)) return HitPanelVolver;   /* (v1.16) */
  if (panelFijar_.inside(mx, my)) return HitPanelFijar;
  if (panelEsquina_.inside(mx, my)) return HitPanelRedimensionar;
  if (panelLogo_.inside(mx, my)) {   /* el logo: la primera pestana */
    if (sub) *sub = 0;
    return HitPanelPestana;
  }
  if (panelIdioma_.inside(mx, my)) return HitPanelIdioma;
  if (panelActualizar_.inside(mx, my)) return HitPanelActualizar;
  if (panelCopiar_.inside(mx, my)) return HitPanelCopiar;
  for (size_t i = 0; i < panelPestanas_.size(); i++)
    if (panelPestanas_[i].inside(mx, my)) {
      if (i < st_.pestanaGris.size() && st_.pestanaGris[i]) return HitPanel;   /* (v1.16) en gris: no se abre */
      if (sub) *sub = (int)i;
      return HitPanelPestana;
    }
  /* (v1.16) las teselas chicas, y a pantalla completa no se arrastra nada */
  if (st_.marco)
    for (size_t k = 0; k < panelTeselas_.size() && k < st_.teselas.size(); k++)
      if (panelTeselas_[k].inside(mx, my) && !st_.teselas[k].id.empty()) {
        /* (v1.17) su flechita abre o cierra su menu: 1000 + la tesela */
        if (sub) *sub = k < panelTeselaFlecha_.size() && panelTeselaFlecha_[k].inside(mx, my) ? 1000 + (int)k : (int)k;
        return HitPanelTesela;
      }
  if (!st_.marco && my < panelCaja_.y + 106.0f) return HitPanelTitulo;
  if (st_.marco && st_.marcoVentana && my < panelCaja_.y + 56.0f) return HitPanelTitulo;   /* (v1.16) la ventana, por su barra */
  if (!panelCuerpo_.inside(mx, my)) return HitPanel;
  for (size_t i = 0; i < zonas_.size() && i < st_.items.size(); i++) {
    const ZonaItem &z = zonas_[i];
    if (z.caja.w <= 0.0f || !z.caja.inside(mx, my)) continue;
    const ItemPanel &it = st_.items[i];
    if (item) *item = (int)i;
    switch (it.tipo) {
      case ItemPanel::Interruptor:   /* en gris no se enciende (si ya estaba puesto, si se puede apagar) */
        /* (v1.18) el agarre de un efecto (se arrastra) y su aspita (lo quita), aunque este en gris */
        if (it.arrastrable && z.agarre.inside(mx, my)) {
          if (sub) *sub = 2;
          return HitPanelItem;
        }
        if (it.borrable && z.aspa.inside(mx, my)) {
          if (sub) *sub = 3;
          return HitPanelItem;
        }
        /* (v1.18) la flechita y el rotulo de uno plegable: pliegan, aunque este en gris */
        if (it.plegable && !z.subs.empty() && z.subs[0].inside(mx, my)) {
          if (sub) *sub = 1;
          return HitPanelItem;
        }
        return it.apagado && !it.valor && !it.descargable ? HitPanel : HitPanelItem;
      case ItemPanel::Seccion:
        return it.plegable ? HitPanelItem : HitPanel;
      case ItemPanel::Juego:
        if (it.gris) return HitPanel;   /* (v1.16) en gris, quieta */
        if (!z.subs.empty() && z.subs[0].inside(mx, my) && sub) *sub = 1;
        else if (z.subs.size() > 1 && z.subs[1].inside(mx, my) && sub) *sub = 2;   /* (v1.15) el engranaje */
        else if (z.subs.size() > 2 && z.subs[2].inside(mx, my) && sub) *sub = 3;   /* (v1.18) los efectos */
        return HitPanelItem;
      case ItemPanel::Texto:   /* (v1.15) la fila entera pone a escribir en el campo */
        return HitPanelItem;
      case ItemPanel::Linea:
        if (!z.subs.empty() && z.subs[0].inside(mx, my)) {
          if (sub) *sub = 0;
          return HitPanelItem;
        }
        return HitPanel;
      case ItemPanel::Opciones:
        for (size_t k = 0; k < z.subs.size(); k++)
          if (z.subs[k].inside(mx, my)) {
            if (sub) *sub = (int)k;
            return HitPanelItem;
          }
        return HitPanel;
      case ItemPanel::Desplegable:   /* (v1.16) su caja: abrir o cerrar la lista (las opciones van arriba, antes) */
        return HitPanelItem;
      case ItemPanel::Deslizador: {
        if (it.fracDefecto >= 0.0f && !z.subs.empty() && z.subs[0].inside(mx, my)) {   /* (v1.16, PR #1) su flechita */
          if (sub) *sub = 1;
          if (frac) *frac = it.fracDefecto;
          return HitPanelItem;
        }
        const Rect p{z.pista.x - 12.0f, z.caja.y, z.pista.w + 24.0f, z.caja.h};
        if (!p.inside(mx, my)) return HitPanel;
        if (frac) *frac = std::max(0.0f, std::min(1.0f, (mx - z.pista.x) / std::max(1.0f, z.pista.w)));
        return HitPanelItem;
      }
      case ItemPanel::Fila:
        if (!z.subs.empty() && z.subs[0].inside(mx, my)) {
          if (sub) *sub = 1;
          return HitPanelItem;
        }
        return HitPanel;
      case ItemPanel::Atajo:
        /* la flecha de volver a la de fabrica es la 1; el resto de la fila, la tecla (0) */
        if (sub) *sub = (z.subs.size() > 1 && z.subs[1].inside(mx, my)) ? 1 : 0;
        return HitPanelItem;
      case ItemPanel::Boton:
        if (it.valor && !z.subs.empty() && z.subs[0].inside(mx, my)) {
          if (sub) *sub = 0;
          return HitPanelItem;
        }
        return HitPanel;
      default:
        return HitPanel;
    }
  }
  return HitPanel;
}

float Hud::fracDeslizador(int item, float mxa) const {
  if (item < 0 || (size_t)item >= st_.items.size()) return 0.0f;
  /* (v1.16, PR #1) una fila fuera de la vista no tiene pista: se queda donde estaba */
  if ((size_t)item >= zonas_.size() || zonas_[(size_t)item].pista.w <= 0.0f) return st_.items[(size_t)item].frac;
  const Rect &p = zonas_[(size_t)item].pista;
  return std::max(0.0f, std::min(1.0f, (mxa / sc_ - p.x) / std::max(1.0f, p.w)));
}

int Hud::destinoFila() const {
  if (st_.moviendoFila < 0) return -1;
  /* las cabeceras de los demas efectos que quedan por encima del raton: ese es el puesto */
  int k = 0;
  for (size_t i = 0; i < st_.items.size() && i < zonas_.size(); i++) {
    if (!st_.items[i].arrastrable || (int)i == st_.moviendoFila) continue;
    if (zonas_[i].yCentro < my_) k++;
  }
  return k;
}

bool Hud::vistaPantalla(float *x, float *y, float *w, float *h, float *corte) const {
  if (vistaRect_.w <= 0.0f || vistaRect_.h <= 0.0f || lienzoW_ <= 0.0f || lienzoH_ <= 0.0f) return false;
  *x = vistaRect_.x / lienzoW_, *y = vistaRect_.y / lienzoH_, *w = vistaRect_.w / lienzoW_, *h = vistaRect_.h / lienzoH_;
  *corte = corteVista_ / lienzoW_;
  return true;
}

bool Hud::autodesplazar(float dt) {
  if (autoDir_ == 0 || autoGrupo_ < 0 || autoGrupo_ > 1) return false;
  const double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() - autoDesde_;
  /* ni muy deprisa (Rodolfo: «just not extremely quick»): empieza a ~4 renglones por segundo y llega a ~10 */
  const float v = 140.0f + 180.0f * (float)std::min(1.0, std::max(0.0, t) / 1.5);
  float &c = st_.despCorrida[autoGrupo_];
  const float antes = c;
  c = std::max(0.0f, std::min(despTope_[autoGrupo_], c + (float)autoDir_ * v * std::max(0.0f, std::min(dt, 0.1f))));
  return c != antes;
}

void Hud::correr(float cuanto) {
  /* (v1.18) con un desplegable por grupos abierto, la rueda corre el grupo que tenga debajo (y la lista de detras, no) */
  if (st_.desplegado >= 0 && (size_t)st_.desplegado < st_.items.size() && !st_.items[(size_t)st_.desplegado].grupos.empty()) {
    for (int g = 0; g < 2; g++)
      if (despGrupo_[g].inside(mx_, my_)) st_.despCorrida[g] = std::max(0.0f, std::min(despTope_[g], st_.despCorrida[g] + cuanto));
    return;
  }
  const float tope = std::max(0.0f, contenidoAlto_ - panelCuerpo_.h);
  st_.corrida = std::max(0.0f, std::min(tope, st_.corrida + cuanto));
}

bool Hud::askBounds(float *x, float *y, float *w, float *h) const {
  if (!st_.ask || askPanel_.w <= 0.0f) return false;
  if (x) *x = askPanel_.x * sc_;
  if (y) *y = askPanel_.y * sc_;
  if (w) *w = askPanel_.w * sc_;
  if (h) *h = askPanel_.h * sc_;
  return true;
}

bool Hud::pillBounds(float *x, float *y, float *w, float *h) const {
  if (!st_.overlayOn || pillPanel_.w <= 0.0f) return false;
  /* con el menu del rayo abierto, la caja que lo abarca todo: la region de entrada lo necesita */
  float x0 = pillPanel_.x, y0 = pillPanel_.y, x1 = x0 + pillPanel_.w, y1 = y0 + pillPanel_.h;
  if (menuRend_.w > 0.0f) {
    x0 = std::min(x0, menuRend_.x);
    x1 = std::max(x1, menuRend_.x + menuRend_.w);
    y0 = std::min(y0, menuRend_.y);
    y1 = std::max(y1, menuRend_.y + menuRend_.h);
  }
  if (x) *x = x0 * sc_;
  if (y) *y = y0 * sc_;
  if (w) *w = (x1 - x0) * sc_;
  if (h) *h = (y1 - y0) * sc_;
  return true;
}

bool Hud::huecoDeLaPastilla(float *x, float *y, float *w, float *h) const {
  /* La punta derecha de donde esta la pastilla, o de donde estaba si esta ocultada. Con la
     pastilla oculta por Ctrl+Shift+F1 (o por `ctl capa ocultar`) la region de entrada del visor se
   quedaba vacia: sin region
     no llega ni un movimiento del puntero y no habia forma de hacer volver la pastilla sin
     teclado. Dejamos este rinconcito pinchable y el visor lo vigila: en cuanto entra el puntero,
     la pastilla reaparece sola. */
  if (pillPanel_.w <= 0.0f) return false;
  const float ancho = 170.0f * sc_;
  const float der = (pillPanel_.x + pillPanel_.w) * sc_;   /* (v1.16) otra vez la punta derecha: volvio arriba */
  if (x) *x = der - ancho;
  if (y) *y = pillPanel_.y * sc_;
  if (w) *w = ancho;
  /* (v1.16) una franja fina pegada a la orilla de arriba, no el alto entero de la pastilla (Rodolfo: «make the space
     for showing the hud with the mouse much lower»): el puntero se para en la orilla, asi que sigue siendo facil de
     alcanzar, y ya no salta al pasar por la esquina */
  if (h) *h = 6.0f * sc_;
  return true;
}

Hud::Hit Hud::hit(float mxa, float mya) const {
  const float mx = mxa / sc_, my = mya / sc_;
  if (st_.panel) return hitDetalle(mxa, mya, nullptr, nullptr, nullptr);
  if (st_.ask) {
    if (askClose_.inside(mx, my)) return HitAskClose;
    if (askActualizar_.inside(mx, my)) return HitAskActualizar;
    if (askLuego_.inside(mx, my)) return HitAskLuego;
    if (askNoAvisar_.inside(mx, my)) return HitAskNoAvisar;
    if (askAyuda_.inside(mx, my)) return HitAskAyuda;
    if (askConv_.inside(mx, my)) return HitAskConv;
    if (askFlat_.inside(mx, my)) return HitAskFlat;   /* (v1.16) el 2D de la pregunta */
    if (askSbs_.inside(mx, my)) return HitAskSbs;
    if (askDont_.inside(mx, my)) return HitAskDont;
  }
  /* (v1.18) el menu de la generacion de cuadros y el cuadrito (con la pastilla oculta tambien) */
  if (st_.menuFg && menuFg_.inside(mx, my)) {
    for (int f = 0; f < 6; f++)
      for (int k = 0; k < 8; k++)
        if (fgOpc_[f][k].w > 0.0f && fgOpc_[f][k].inside(mx, my)) return (Hit)(HitFgOpcion0 + f * 8 + k);
    return HitFgFondo;
  }
  if (cuadroFps_.w > 0.0f && cuadroFps_.inside(mx, my)) return HitFgCuadro;
  if (!st_.overlayOn) return HitNone;
  if (sw3D_.inside(mx, my)) return Hit3D;   /* el boton de encendido */
  if (pillModo_.w > 0.0f && pillModo_.inside(mx, my)) return HitModoRed;   /* Neural | Rendimiento, en marcha */
  float pad = 10.0f;
  /* el modo rendimiento: el boton del rayo y, abierto, su menu (los preajustes y cuatro deslizadores) */
  if (st_.rendimiento) {
    if (btnRayo_.inside(mx, my)) return HitRendimientoMenu;
    if (st_.menuRendimiento && menuRend_.inside(mx, my)) {
      for (int k = 0; k < 3; k++)
        if (btnPre_[k].inside(mx, my)) return (Hit)(HitPreajuste1 + k);
      for (int k = 0; k < 4; k++) {
        const Rect &t = rendTrack_[k];
        if (Rect{t.x - pad, t.y - 14.0f, t.w + 2.0f * pad, t.h + 26.0f}.inside(mx, my)) return (Hit)(HitRendMezcla + k);
      }
      return HitRendFondo;
    }
  } else {
    if (btnFoco_.inside(mx, my)) return HitFocoAuto;
    /* el menu del hub normal: Auto | Manual, los deslizadores (Manual), el tipo de video y la casilla */
    if (st_.menuNormal && menuRend_.inside(mx, my)) {   /* (v1.12) tambien sin conversor: solo Manual */
      if (btnAM_[0].inside(mx, my)) return HitNormalAuto;
      if (btnAM_[1].inside(mx, my)) return HitNormalManual;
      if (depTrack_.w > 0.0f && Rect{depTrack_.x - pad, depTrack_.y - 14.0f, depTrack_.w + 2.0f * pad, 26.0f}.inside(mx, my))
        return HitDepth;
      if (popTrack_.w > 0.0f && Rect{popTrack_.x - pad, popTrack_.y - 14.0f, popTrack_.w + 2.0f * pad, 26.0f}.inside(mx, my))
        return HitPopout;
      if (btnNormal_.inside(mx, my)) return HitModoNormal;
      if (btnDividido_.inside(mx, my)) return HitModoDividido;
      if (chkRecordar_.inside(mx, my)) return HitRecordar;
      return HitRendFondo;
    }
    Rect dt = {depTrack_.x - pad, depTrack_.y - pad, depTrack_.w + 2.0f * pad, depTrack_.h + 2.0f * pad};
    Rect pt = {popTrack_.x - pad, popTrack_.y - pad, popTrack_.w + 2.0f * pad, popTrack_.h + 2.0f * pad};
    if (depTrack_.w > 0.0f && dt.inside(mx, my)) return HitDepth;
    if (pt.inside(mx, my)) return HitPopout;
  }
  if (btnNormal_.inside(mx, my)) return HitModoNormal;
  if (btnDividido_.inside(mx, my)) return HitModoDividido;
  if (chkRecordar_.inside(mx, my)) return HitRecordar;
  if (btnEngranaje_.inside(mx, my)) return HitEngranaje;
  return HitNone;
}

void Hud::dragTo(Hit which, float mxa) {
  const float mx = mxa / sc_;
  if (which == HitDepth) {
    float f = (mx - depTrack_.x) / std::max(1.0f, depTrack_.w);
    st_.depth = std::max(0, std::min(100, (int)std::lround(f * 100.0f)));
  } else if (which == HitPopout) {
    float f = (mx - popTrack_.x) / std::max(1.0f, popTrack_.w);
    /* el deslizador va de -50 a +50 (se pinta en (salida + 50) / 100): antes se tomaba f * 100 y la mitad
       del recorrido ya daba 50, la bola saltaba al final y lo negativo no se alcanzaba con el raton */
    st_.popout = std::max(-50, std::min(50, (int)std::lround(f * 100.0f) - 50));
  } else if (which >= HitRendMezcla && which <= HitRendParalaje) {
    /* los deslizadores del menu del rayo: el visor ve el valor nuevo y lo pone en marcha */
    const int k = (int)which - (int)HitRendMezcla;
    const Rect &t = rendTrack_[k];
    const float f = std::max(0.0f, std::min(1.0f, (mx - t.x) / std::max(1.0f, t.w)));
    const rendimiento::Rango &r = rendimiento::kRangos[k];
    st_.rendValor[k] = rendimiento::ajusta(k, r.lo + f * (r.hi - r.lo));
  }
}
