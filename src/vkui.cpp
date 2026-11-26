/* vkui.cpp — atlas (fuentes + máscaras), disposición de la interfaz y manejo del ratón. */
#include "vkui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#ifdef HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace {
const char *kFontPaths[] = {
    "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/TTF/NotoSans-Regular.ttf",
    "/usr/share/fonts/TTF/LiberationSans-Regular.ttf", "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
};
constexpr int kFontPx = 26;   /* tamaño horneado del atlas; luego se escala el cuadro */

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

float distSegmento(float px, float py, float ax, float ay, float bx, float by) {
  const float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
  float t = (wx * vx + wy * vy) / std::max(1e-6f, vx * vx + vy * vy);
  t = std::max(0.0f, std::min(1.0f, t));
  return std::hypot(px - (ax + t * vx), py - (ay + t * vy));
}
}  // namespace

bool Hud::buildAtlas(std::string *err) {
  aw_ = 1024;
  ah_ = 512;
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
    auto cargar = [&](unsigned cp, Glyph &gl) -> int {
      if (FT_Load_Char(face, cp, FT_LOAD_RENDER) != 0) return 1;
      FT_GlyphSlot g = face->glyph;
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
    for (unsigned cp = 32; cp < 256; cp++) {
      if (cargar(cp, glyphs_[cp]) < 0) { okAll = false; break; }   /* atlas lleno */
    }
    /* fuera del rango latino-1: los signos que mas salen en titulos de videos */
    for (unsigned cp : {0x2014u, 0x2013u, 0x2026u, 0x2018u, 0x2019u, 0x201Cu, 0x201Du, 0x20ACu, 0x00A9u, 0x2122u}) {
      Glyph gl;
      if (cargar(cp, gl) != 0) continue;
      extra_.emplace_back(cp, gl);
    }
    hasText_ = true;
    std::printf("[interfaz] fuente %s a %d px en atlas %dx%d\n", path, kFontPx, aw_, ah_);
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
void Hud::paisaje(Rect r, float apretado, float luz, float radio, const float *fondo) {
  solid(r.x, r.y, r.w, r.h, 0.42f * luz, 0.33f * luz, 0.88f * luz, 1.0f);
  const float d = r.h * 0.25f;
  icono({r.x + r.w * 0.80f - d * apretado * 0.5f, r.y + r.h * 0.13f, d * apretado, d}, pill_, 0.82f * luz,
        0.76f * luz, 1.0f * luz, 1.0f);
  icono(r, monteLejos_, 0.31f * luz, 0.23f * luz, 0.70f * luz, 1.0f);
  icono(r, monteCerca_, 0.21f * luz, 0.15f * luz, 0.50f * luz, 1.0f);
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
  acum.assign((size_t)ancho * (size_t)alto * 4u, 0.0f);
  for (size_t i = 0; i + 5 < v_.size(); i += 6) {
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
    /* cuadro liso (el pixel blanco, o el centro de una mascara: sus cuatro esquinas del todo
       opacas): una sola muestra para todo el cuadro. El fondo del panel de ajustes son dos millones
       de pixeles y muestrearlos uno a uno costaba la mitad del pintado. */
    const float c00 = muestra(u0, v0), c10 = muestra(u1, v0), c01 = muestra(u0, v1), c11 = muestra(u1, v1);
    const bool liso = (du * (float)aw_ <= 1.01f && dv * (float)ah_ <= 1.01f) ||
                      (c00 >= 0.999f && c10 >= 0.999f && c01 >= 0.999f && c11 >= 0.999f);
    for (int py = py0; py < py1; py++) {
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
  /* las caratulas: encima de todo lo anterior, con las esquinas redondeadas por cuenta propia */
  for (const ImagenCpu &im : imagenes_) {
    if (!im.px || im.w <= 0 || im.h <= 0) continue;
    const float cx0 = im.caja.x * sc_, cy0 = im.caja.y * sc_, cw = im.caja.w * sc_, ch = im.caja.h * sc_;
    const float rx0 = std::max(cx0, im.recorte.x * sc_), ry0 = std::max(cy0, im.recorte.y * sc_);
    const float rx1 = std::min(cx0 + cw, (im.recorte.x + im.recorte.w) * sc_);
    const float ry1 = std::min(cy0 + ch, (im.recorte.y + im.recorte.h) * sc_);
    const int px0 = std::max(0, (int)std::ceil(rx0 - ox - 0.5f)), py0 = std::max(0, (int)std::ceil(ry0 - oy - 0.5f));
    const int px1 = std::min(ancho, (int)std::ceil(rx1 - ox - 0.5f)), py1 = std::min(alto, (int)std::ceil(ry1 - oy - 0.5f));
    const float rad = im.radio * sc_;
    for (int py = py0; py < py1; py++) {
      const float y = (float)py + 0.5f + oy - cy0;
      const float fy = std::max(0.0f, std::min((float)im.h - 1.001f, y / ch * (float)im.h - 0.5f));
      const int iy = (int)fy;
      const float ty = fy - (float)iy;
      for (int px = px0; px < px1; px++) {
        const float x = (float)px + 0.5f + ox - cx0;
        /* cobertura del rectangulo redondeado */
        const float dx = std::max(0.0f, std::max(rad - x, x - (cw - rad))), dy = std::max(0.0f, std::max(rad - y, y - (ch - rad)));
        const float a = std::max(0.0f, std::min(1.0f, rad + 0.5f - std::hypot(dx, dy)));
        if (a <= 0.0f) continue;
        const float fx = std::max(0.0f, std::min((float)im.w - 1.001f, x / cw * (float)im.w - 0.5f));
        const int ix = (int)fx;
        const float tx = fx - (float)ix;
        const uint8_t *p00 = im.px + ((size_t)iy * im.w + ix) * 4u, *p10 = p00 + 4, *p01 = p00 + (size_t)im.w * 4u, *p11 = p01 + 4;
        float *d = acum.data() + ((size_t)py * (size_t)ancho + (size_t)px) * 4u;
        for (int c = 0; c < 3; c++) {
          const float col = ((p00[c] * (1 - tx) + p10[c] * tx) * (1 - ty) + (p01[c] * (1 - tx) + p11[c] * tx) * ty) / 255.0f;
          d[c] = col * a + d[c] * (1.0f - a);
        }
        d[3] = a + d[3] * (1.0f - a);
      }
    }
  }
}

const Hud::Glyph *Hud::glifo(unsigned cp) const {
  if (cp < 256) return &glyphs_[cp];
  for (const std::pair<unsigned, Glyph> &e : extra_)
    if (e.first == cp) return &e.second;
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
  size_t i = 0;
  while (i < s.size()) {
    unsigned cp = nextCp(s, i);
    const Glyph *gp = glifo(cp);
    if (!gp || !gp->ok) { pen += (gp && gp->adv > 0 ? (float)gp->adv : (float)kFontPx * 0.5f) * sc; continue; }
    const Glyph &g = *gp;
    /* el glifo cuelga de la línea base: su borde superior está a asc_-bearingY */
    Rect r{pen + (float)g.bx * sc, top0 + (asc_ - (float)g.by) * sc, (float)g.w * sc, (float)g.h * sc};
    quad(r, {g.u0, g.v0, g.u1 - g.u0, g.v1 - g.v0}, cr, cg, cb, ca);
    pen += (float)g.adv * sc;
  }
  return pen;
}

void Hud::toggle3D() { st_.weaveOn = !st_.weaveOn; }
void Hud::toggleOverlay() { st_.overlayOn = !st_.overlayOn; }
void Hud::nudgeDepth(int d) { st_.depth = std::max(0, std::min(100, st_.depth + d)); }
void Hud::nudgePopout(int d) { st_.popout = std::max(-50, std::min(50, st_.popout + d)); }

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
  if (!st_.overlayOn && !st_.ask && !st_.panel) return;

  /* v0.79: el acento es el morado del paisaje de la tarjeta (antes verde) y el fondo de la pastilla
     es el gris casi negro de la tarjeta: las dos piezas se ven de la misma familia. */
  const float accent[3] = {0.46f, 0.37f, 0.92f};
  const float ink[4] = {0.93f, 0.94f, 0.96f, 1.0f};
  const float dim[4] = {0.68f, 0.70f, 0.74f, 0.90f};

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
  btnRed_ = {-10.0f, -10.0f, 0.0f, 0.0f};   /* ya no hay boton de red */
  if (st_.overlayOn) {
    const bool leg = st_.legado;
    const float aire = 16.0f, anchoSw = 100.0f, anchoDes = 140.0f;
    const char *rotRec = "No volver a preguntar";
    const float anchoRec = 18.0f + 8.0f + textWidth(rotRec, 12.5f);
    /* el bloque de la derecha: arriba los dos botones de icono (que tipo de 3D), debajo la casilla.
       Mide lo que la casilla con su letra; sin casilla, lo justo para los dos botones. */
    const bool hayModo = st_.modoVisible && !leg;
    const float anchoBloque = st_.recordarVisible ? anchoRec : (hayModo ? 128.0f : 0.0f);
    float pw = aire + anchoSw + 26.0f + anchoDes + 24.0f + anchoDes + aire;
    if (anchoBloque > 0.0f) pw += 26.0f + anchoBloque;
    if (st_.engranaje) pw += 14.0f + 30.0f;   /* el engranaje de los ajustes, al final */
    pw = std::min(pw, (float)vw - 48.0f);
    const float ph = 76.0f;
    panelW_ = pw;
    panelH_ = ph;
    const float x0 = (float)vw - 24.0f - pw, y0 = 24.0f;
    pillPanel_ = {x0, y0, pw, ph};
    solid(pillPanel_.x, pillPanel_.y, pw, ph, 0.115f, 0.12f, 0.135f, 0.90f);

    /* ---- el interruptor del 3D ----
       ON con la bola a la izquierda y la letra a su derecha; OFF con la bola a la derecha y la letra
       a la izquierda: el dibujo que paso Rodolfo. Se pica en cualquier parte de la pista. */
    sw3D_ = {x0 + aire, y0 + 17.0f, anchoSw, 32.0f};
    const bool hov3 = sw3D_.inside(mx_, my_);
    if (st_.weaveOn) {
      const float t = hov3 ? 1.12f : 1.0f;   /* al pasar el raton por encima, un poco mas viva */
      mask9(sw3D_, 0, 16.0f, accent[0] * t, accent[1] * t, accent[2] * t, 0.95f);
    } else {
      const float g = hov3 ? 0.32f : 0.24f;
      mask9(sw3D_, 0, 16.0f, g, g + 0.02f, g + 0.05f, 0.95f);
    }
    const float bola = 24.0f, margen = 4.0f;
    const float bolaY = sw3D_.y + (sw3D_.h - bola) * 0.5f;
    swKnob_ = st_.weaveOn ? Rect{sw3D_.x + margen, bolaY, bola, bola}
                          : Rect{sw3D_.x + sw3D_.w - margen - bola, bolaY, bola, bola};
    mask9(swKnob_, 0, bola * 0.5f, 0.96f, 0.97f, 0.99f, 1.0f);
    const char *palabra = st_.weaveOn ? "ON" : "OFF";
    const float anchoLetra = textWidth(palabra, 15.0f);
    const float letraX = st_.weaveOn ? swKnob_.x + bola + 12.0f : swKnob_.x - 12.0f - anchoLetra;
    text(palabra, letraX, sw3D_.y + 8.0f, 15.0f, 1.0f, 1.0f, 1.0f, 1.0f);

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
    depTrack_ = {sw3D_.x + anchoSw + 26.0f, y0 + 41.0f, anchoDes, 4.0f};
    popTrack_ = {depTrack_.x + anchoDes + 24.0f, y0 + 41.0f, anchoDes, 4.0f};
    slider(&depTrack_, &depKnob_, leg ? "Plano de apoyo" : "Profundidad", (float)st_.depth / 100.0f,
           std::to_string(st_.depth));
    slider(&popTrack_, &popKnob_, "Salida", (float)(st_.popout + 50) / 100.0f, std::to_string(st_.popout));

    const float bloqueX = popTrack_.x + anchoDes + 26.0f;
    const float sitioEngranaje = st_.engranaje ? 44.0f : 0.0f;
    const bool cabeBloque = anchoBloque > 0.0f && bloqueX + anchoBloque + sitioEngranaje + aire <= x0 + pw + 0.5f;
    /* ---- el engranaje: abre el panel de ajustes por encima del video (v0.84) ---- */
    if (st_.engranaje) {
      btnEngranaje_ = {x0 + pw - aire - 30.0f, y0 + 23.0f, 30.0f, 30.0f};
      const bool hov = btnEngranaje_.inside(mx_, my_);
      if (hov) mask9(btnEngranaje_, 0, 15.0f, 1.0f, 1.0f, 1.0f, 0.14f);
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
    if (cabeBloque && hayModo) {
      const float bw = (anchoBloque - 8.0f) * 0.5f, bh = 30.0f;
      const float by = st_.recordarVisible ? y0 + 9.0f : y0 + 18.0f;
      btnNormal_ = {bloqueX, by, bw, bh};
      btnDividido_ = {bloqueX + bw + 8.0f, by, bw, bh};
      const Rect bs[2] = {btnNormal_, btnDividido_};
      for (int i = 0; i < 2; i++) {
        const Rect &b = bs[i];
        const bool puesto = i == 0 ? st_.modo3d == 1 : st_.modo3d >= 2, hov = b.inside(mx_, my_);
        const float fondo[3] = {puesto ? 0.26f : (hov ? 0.23f : 0.17f), puesto ? 0.25f : (hov ? 0.235f : 0.175f),
                                puesto ? 0.36f : (hov ? 0.27f : 0.20f)};
        if (puesto) solid(b.x - 1.5f, b.y - 1.5f, b.w + 3.0f, b.h + 3.0f, ink[0], ink[1], ink[2], 0.85f);
        solid(b.x, b.y, b.w, b.h, fondo[0], fondo[1], fondo[2], 1.0f);
        const float luz = puesto ? 1.0f : (hov ? 0.80f : 0.55f);
        const float iw = 42.0f, ih = 22.0f, ix = b.x + (b.w - iw) * 0.5f, iy = b.y + (b.h - ih) * 0.5f;
        /* el dibujo del segundo sigue a lo que esta puesto de verdad si ya es un dividido (2 = tal
           cual, 3 = suavizado); si no, a lo que haria al picarlo */
        const bool suave = st_.modo3d == 3 || (st_.modo3d != 2 && st_.suavizado);
        if (i == 0) {
          paisaje({ix, iy, iw, ih}, 1.0f, luz, 0.0f, fondo);
        } else if (!suave) {
          const float mitad = (iw - 3.0f) * 0.5f;
          paisaje({ix, iy, mitad, ih}, 0.5f, luz, 0.0f, fondo);
          paisaje({ix + mitad + 3.0f, iy, mitad, ih}, 0.5f, luz, 0.0f, fondo);
        } else {
          /* la imagen entera con la raya del corte a trazos: viene partida, se ve de una pieza */
          paisaje({ix, iy, iw, ih}, 1.0f, luz, 0.0f, fondo);
          for (int k = 0; k < 4; k++)
            solid(ix + iw * 0.5f - 1.0f, iy + 1.0f + (float)k * 5.5f, 2.0f, 3.5f, luz, luz, luz, 1.0f);
        }
      }
    }

    /* ---- «no volver a preguntar», dentro del video ----
       Es la misma casilla de la tarjeta de la orilla. Marcada, esta aplicacion pasa directa al 3D la
       proxima vez (con el tipo que este puesto arriba); se desmarca aqui mismo y el servicio vuelve
       a preguntar. Solo sale cuando el video llego por el servicio, que es quien guarda la eleccion. */
    if (cabeBloque && st_.recordarVisible) {
      const Rect caja = hayModo ? Rect{bloqueX, y0 + 49.0f, 16.0f, 16.0f} : Rect{bloqueX, y0 + 24.0f, 18.0f, 18.0f};
      chkRecordar_ = {caja.x - 6.0f, caja.y - 5.0f, anchoRec + 12.0f, caja.h + 10.0f};
      const bool hov = chkRecordar_.inside(mx_, my_);
      const float c = hov ? 1.0f : 0.86f;
      solid(caja.x, caja.y, caja.w, caja.h, c, c, c + 0.02f, 1.0f);
      if (st_.recordar)
        icono({caja.x + 2.0f, caja.y + 2.0f, caja.w - 4.0f, caja.h - 4.0f}, icoPalomita_, 0.07f, 0.08f, 0.10f, 1.0f);
      else
        solid(caja.x + 2.0f, caja.y + 2.0f, caja.w - 4.0f, caja.h - 4.0f, 0.115f, 0.12f, 0.135f, 1.0f);
      text(rotRec, caja.x + 26.0f, caja.y + (caja.h - 16.0f) * 0.5f, 12.5f, ink[0], ink[1], ink[2], hov ? 1.0f : 0.88f);
    }

    /* las teclas, en un renglon. Si la pastilla sale angosta, la letra se achica en vez de salirse. */
    const std::string teclas = std::string("Ctrl+Shift+  1 el 3D · 3/4 ") + (leg ? "plano" : "profundidad") +
                               " · 5/6 salida · F1 oculta · Esc sale";
    const float cabe = pw - 2.0f * aire - sitioEngranaje;
    const float tamTeclas = std::min(10.5f, 10.5f * cabe / std::max(1.0f, textWidth(teclas, 10.5f)));
    text(teclas, x0 + aire, y0 + 58.0f, tamTeclas, dim[0], dim[1], dim[2], 0.72f);
  }

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
    const float ax = (float)vw - 24.0f - aw, ay = (float)vh - 24.0f - ah;
    askPanel_ = {ax, ay, aw, ah};
    mask9(askPanel_, 1, 18.0f, 0.34f, 0.35f, 0.38f, 0.90f);
    mask9({ax + 1.0f, ay + 1.0f, aw - 2.0f, ah - 2.0f}, 1, 17.0f, 0.115f, 0.12f, 0.135f, 0.98f);
    const float borde = 26.0f;
    askClose_ = {ax + aw - 18.0f - 30.0f, ay + 16.0f, 30.0f, 30.0f};
    const bool hc = askClose_.inside(mx_, my_);
    if (hc) mask9(askClose_, 0, 15.0f, 1.0f, 1.0f, 1.0f, 0.14f);
    icono({askClose_.x + 5.0f, askClose_.y + 5.0f, 20.0f, 20.0f}, icoCerrar_, ink[0], ink[1], ink[2], hc ? 1.0f : 0.80f);
    /* el circulo amarillo con la flecha, como el boton del panel, y el titulo a su lado */
    mask9({ax + borde, ay + 20.0f, 28.0f, 28.0f}, 0, 14.0f, 0.98f, 0.76f, 0.18f, 1.0f);
    icono({ax + borde + 6.0f, ay + 26.0f, 16.0f, 16.0f}, icoFlecha_, 0.16f, 0.12f, 0.04f, 1.0f);
    text("Hay una versión nueva de OdysseyVK", ax + borde + 38.0f, ay + 24.0f, 17.0f, ink[0], ink[1], ink[2], 1.0f);
    text("OdysseyVK " + st_.askActualizacion + " en GitHub · tienes la " + st_.version.substr(st_.version.rfind(' ') + 1),
         ax + borde, ay + 60.0f, 13.0f, ink[0], ink[1], ink[2], 0.92f);
    text("Se instala sola (pide tu clave) y el vídeo o juego que haya en 3D se reabre.",
         ax + borde, ay + 80.0f, 11.5f, dim[0], dim[1], dim[2], 0.85f);
    /* Actualizar y Luego, dos pildoras; y abajo, chico, «No avisar de actualizaciones» */
    const float by = ay + 108.0f;
    askActualizar_ = {ax + borde, by, 150.0f, 34.0f};
    askLuego_ = {ax + borde + 162.0f, by, 92.0f, 34.0f};
    const bool hA = askActualizar_.inside(mx_, my_), hL = askLuego_.inside(mx_, my_);
    mask9(askActualizar_, 0, 17.0f, accent[0], accent[1], accent[2], hA ? 1.0f : 0.92f);
    text("Actualizar", askActualizar_.x + askActualizar_.w * 0.5f, by + 8.0f, 14.0f, 1.0f, 1.0f, 1.0f, 1.0f, true);
    mask9(askLuego_, 0, 17.0f, 1.0f, 1.0f, 1.0f, hL ? 0.18f : 0.10f);
    text("Luego", askLuego_.x + askLuego_.w * 0.5f, by + 8.0f, 14.0f, ink[0], ink[1], ink[2], hL ? 1.0f : 0.85f, true);
    const char *noAvisar = "No avisar de actualizaciones";
    askNoAvisar_ = {ax + borde - 4.0f, ay + ah - 36.0f, textWidth(noAvisar, 11.5f) + 8.0f, 22.0f};
    const bool hN = askNoAvisar_.inside(mx_, my_);
    if (hN) mask9(askNoAvisar_, 0, 11.0f, 1.0f, 1.0f, 1.0f, 0.10f);
    text(noAvisar, ax + borde, ay + ah - 31.0f, 11.5f, dim[0], dim[1], dim[2], hN ? 1.0f : 0.70f);
  } else if (st_.ask) {
    const float aw = std::min(500.0f, (float)vw - 48.0f), ah = 316.0f;
    const float ax = (float)vw - 24.0f - aw, ay = (float)vh - 24.0f - ah;
    askPanel_ = {ax, ay, aw, ah};
    mask9(askPanel_, 1, 18.0f, 0.34f, 0.35f, 0.38f, 0.90f);   /* el filo claro */
    mask9({ax + 1.0f, ay + 1.0f, aw - 2.0f, ah - 2.0f}, 1, 17.0f, 0.115f, 0.12f, 0.135f, 0.98f);
    auto encaja = [&](const std::string &s, float tam, float maxAncho) {
      if (textWidth(s, tam) <= maxAncho) return s;
      std::string out, col;
      float ancho = 0.0f, reserva = textWidth("..", tam);
      size_t i = 0;
      while (i < s.size()) {
        size_t antes = i;
        nextCp(s, i);
        col = s.substr(antes, i - antes);
        float w = textWidth(col, tam);
        if (ancho + w > maxAncho - reserva) break;
        ancho += w;
        out += col;
      }
      return out + "..";
    };
    const float borde = 26.0f;

    /* los dos signos de arriba a la derecha: el "?" explica las dos opciones mientras el raton esta
       encima (no abre nada: cambia los dos renglones de debajo del titulo), el aspa cierra */
    askClose_ = {ax + aw - 18.0f - 30.0f, ay + 16.0f, 30.0f, 30.0f};
    askAyuda_ = {askClose_.x - 34.0f, ay + 16.0f, 30.0f, 30.0f};
    const bool hc = askClose_.inside(mx_, my_), ha = askAyuda_.inside(mx_, my_);
    if (hc) mask9(askClose_, 0, 15.0f, 1.0f, 1.0f, 1.0f, 0.14f);
    icono({askClose_.x + 5.0f, askClose_.y + 5.0f, 20.0f, 20.0f}, icoCerrar_, ink[0], ink[1], ink[2],
          hc ? 1.0f : 0.80f);
    if (ha) mask9(askAyuda_, 0, 15.0f, 1.0f, 1.0f, 1.0f, 0.14f);
    icono({askAyuda_.x + 4.0f, askAyuda_.y + 4.0f, 22.0f, 22.0f}, icoAnillo_, ink[0], ink[1], ink[2],
          ha ? 1.0f : 0.80f);
    text("?", askAyuda_.x + 15.0f, askAyuda_.y + 7.0f, 14.0f, ink[0], ink[1], ink[2], ha ? 1.0f : 0.80f, true);

    /* la pregunta. El titulo lo puede poner quien llama; de fabrica es la de la aplicacion oficial.
       Debajo, en letra chica, la ventana por la que se pregunta (la pone el servicio). */
    const float anchoTitulo = askAyuda_.x - 10.0f - (ax + borde);
    text(encaja(askTitulo_.empty() ? "¿Qué tipo de vídeo estás viendo?" : askTitulo_, 18.0f, anchoTitulo),
         ax + borde, ay + 22.0f, 18.0f, ink[0], ink[1], ink[2], 1.0f);
    if (ha) {
      text("Vídeo normal: una sola imagen; se convierte a 3D.", ax + borde, ay + 50.0f, 12.5f, ink[0], ink[1],
           ink[2], 0.95f);
      text("Vídeo dividido: trae dos imágenes, una al lado de la otra.", ax + borde, ay + 69.0f, 12.5f, ink[0],
           ink[1], ink[2], 0.95f);
    } else {
      text("Elige la opción correcta para la conversión 3D.", ax + borde, ay + 49.0f, 13.5f, ink[0], ink[1],
           ink[2], 0.92f);
      if (!askCuerpo_.empty())
        text(encaja(askCuerpo_, 11.5f, aw - 2.0f * borde), ax + borde, ay + 71.0f, 11.5f, dim[0], dim[1], dim[2],
             0.80f);
    }

    /* las dos teselas, en el orden de la foto: el video normal a la izquierda */
    const float tw = (aw - 2.0f * borde - 14.0f) * 0.5f, th = 156.0f, ty = ay + 98.0f;
    askConv_ = {ax + borde, ty, tw, th};
    askSbs_ = {ax + borde + tw + 14.0f, ty, tw, th};
    const Rect tiles[2] = {askConv_, askSbs_};
    const char *names[2] = {"Vídeo normal", "Vídeo dividido"};
    const char *subs[2] = {"una sola imagen", "dos imágenes lado a lado"};
    for (int i = 0; i < 2; i++) {
      const Rect &t = tiles[i];
      const bool hov = t.inside(mx_, my_);
      const float fondo[3] = {hov ? 0.27f : 0.195f, hov ? 0.28f : 0.20f, hov ? 0.315f : 0.225f};
      if (hov) mask9({t.x - 1.5f, t.y - 1.5f, t.w + 3.0f, t.h + 3.0f}, 1, 13.5f, ink[0], ink[1], ink[2], 0.75f);
      mask9(t, 1, 12.0f, fondo[0], fondo[1], fondo[2], 1.0f);
      const float iw = std::min(128.0f, t.w - 36.0f), ih = 72.0f;
      const float ix = t.x + (t.w - iw) * 0.5f, iy = t.y + 20.0f;
      if (i == 0) {
        paisaje({ix, iy, iw, ih}, 1.0f, 1.0f, 7.0f, fondo);
      } else {
        const float mitad = (iw - 5.0f) * 0.5f;
        paisaje({ix, iy, mitad, ih}, 0.5f, 1.0f, 7.0f, fondo);
        paisaje({ix + mitad + 5.0f, iy, mitad, ih}, 0.5f, 1.0f, 7.0f, fondo);
      }
      const float fs = tw < 150.0f ? 12.5f : 15.0f, fss = tw < 150.0f ? 10.0f : 11.5f;
      text(encaja(names[i], fs, t.w - 16.0f), t.x + t.w * 0.5f, t.y + 104.0f, fs, ink[0], ink[1], ink[2], 1.0f,
           true);
      text(encaja(subs[i], fss, t.w - 16.0f), t.x + t.w * 0.5f, t.y + 127.0f, fss, dim[0], dim[1], dim[2], dim[3],
           true);
    }

    /* la casilla: se pica en la caja o en su letra */
    const char *rotulo = "Guardar el tipo de vídeo y no volver a preguntar";
    const Rect caja{ax + borde, ty + th + 18.0f, 20.0f, 20.0f};
    askDont_ = {caja.x - 4.0f, caja.y - 6.0f, std::min(aw - 2.0f * borde, 20.0f + 10.0f + textWidth(rotulo, 13.0f)) + 8.0f,
                32.0f};
    const bool hd = askDont_.inside(mx_, my_);
    const float c = hd ? 1.0f : 0.86f;
    mask9(caja, 1, 5.0f, c, c, c + 0.02f, 1.0f);
    if (st_.remember)
      icono({caja.x + 2.0f, caja.y + 2.0f, 16.0f, 16.0f}, icoPalomita_, 0.07f, 0.08f, 0.10f, 1.0f);
    else
      mask9({caja.x + 2.0f, caja.y + 2.0f, 16.0f, 16.0f}, 1, 3.5f, 0.115f, 0.12f, 0.135f, 1.0f);
    text(encaja(rotulo, 13.0f, aw - 2.0f * borde - 30.0f), caja.x + 30.0f, caja.y + 2.0f, 13.0f, ink[0], ink[1],
         ink[2], hd ? 1.0f : 0.90f);
  }

  if (st_.panel) dibujaPanel(vw, vh);

  /* ---- el aviso del momento, colgado debajo de la pastilla ----
     Antes era una linea abajo a la izquierda, y estaba colocada con el alto REAL de la ventana en
     vez del virtual: en el panel de 2160 (escala 2) caia fuera de la pantalla y no se veia nunca.
     Ahora va pegada bajo la pastilla, que es donde se esta mirando cuando se toca algo, y solo
     sale cuando hay algo que decir (un fallo de la lente o de la camara, o la respuesta a la
     casilla); se quita sola. */
  if (!info_.empty() && st_.overlayOn) {
    const float tam = 12.0f, tw = textWidth(info_, tam);
    const float der = pillPanel_.x + pillPanel_.w, arriba = pillPanel_.y + pillPanel_.h + 6.0f;
    solid(der - tw - 20.0f, arriba, tw + 20.0f, 24.0f, 0.115f, 0.12f, 0.135f, 0.90f);
    text(info_, der - tw - 10.0f, arriba + 5.0f, tam, ink[0], ink[1], ink[2], 0.92f);
  }
}

/* ---------------------------------------------------------------------------------------------
   El panel de ajustes (v0.84): una tarjeta translucida de esquinas redondas en el centro de la
   pantalla, encima de lo que haya (el escritorio, o el video en 3D). Arriba el nombre, las pestanas
   y el aspa; en medio la lista de filas, que corre con la rueda; abajo la frase de lo ultimo que se
   cambio. Los colores son los de la tarjeta de la orilla y la pastilla.
   --------------------------------------------------------------------------------------------- */
void Hud::dibujaPanel(int vw, int vh) {
  const float accent[3] = {0.46f, 0.37f, 0.92f};
  const float ink[4] = {0.93f, 0.94f, 0.96f, 1.0f};
  const float dim[4] = {0.68f, 0.70f, 0.74f, 0.90f};
  const float pw = std::min(820.0f, (float)vw - 48.0f), ph = std::min(600.0f, (float)vh - 48.0f);
  const float px = std::floor(((float)vw - pw) * 0.5f), py = std::floor(((float)vh - ph) * 0.5f);
  panelCaja_ = {px, py, pw, ph};
  mask9(panelCaja_, 1, 22.0f, 0.42f, 0.43f, 0.50f, 0.80f);                                   /* el filo */
  mask9({px + 1.0f, py + 1.0f, pw - 2.0f, ph - 2.0f}, 1, 21.0f, 0.085f, 0.09f, 0.11f, 0.86f);   /* translucido */

  /* cabecera: nombre, pestanas en pildora y el aspa */
  const float tx0 = text("OdysseyVK", px + 28.0f, py + 24.0f, 20.0f, ink[0], ink[1], ink[2], 1.0f);
  panelPestanas_.clear();
  float tx = tx0 + 26.0f;
  for (size_t i = 0; i < st_.pestanas.size(); i++) {
    const float w = textWidth(st_.pestanas[i], 14.0f) + 32.0f;
    const Rect t{tx, py + 20.0f, w, 32.0f};
    panelPestanas_.push_back(t);
    const bool puesta = (int)i == st_.pestana, hov = t.inside(mx_, my_);
    if (puesta) mask9(t, 0, 16.0f, accent[0], accent[1], accent[2], 0.95f);
    else if (hov) mask9(t, 0, 16.0f, 1.0f, 1.0f, 1.0f, 0.10f);
    text(st_.pestanas[i], t.x + t.w * 0.5f, t.y + 7.0f, 14.0f, ink[0], ink[1], ink[2], puesta || hov ? 1.0f : 0.72f,
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
  const bool ha = panelActualizar_.inside(mx_, my_);
  std::string rotuloActualizar;
  if (!st_.actualizacion.empty()) {
    mask9(panelActualizar_, 0, 16.0f, 0.98f, 0.76f, 0.18f, ha ? 1.0f : 0.92f);
    icono({panelActualizar_.x + 7.0f, panelActualizar_.y + 7.0f, 18.0f, 18.0f}, icoFlecha_, 0.16f, 0.12f, 0.04f, 1.0f);
    rotuloActualizar = "Hay una actualización pendiente: OdysseyVK " + st_.actualizacion + ". Pica para instalarla.";
  } else {
    if (ha) mask9(panelActualizar_, 0, 16.0f, 1.0f, 1.0f, 1.0f, 0.14f);
    if (st_.alDia) {
      icono({panelActualizar_.x + 7.0f, panelActualizar_.y + 7.0f, 18.0f, 18.0f}, icoFlecha_, 0.36f, 0.86f, 0.46f, ha ? 1.0f : 0.9f);
      rotuloActualizar = "OdysseyVK está al día. Pica para volver a comprobarlo.";
    } else {
      icono({panelActualizar_.x + 7.0f, panelActualizar_.y + 7.0f, 18.0f, 18.0f}, icoFlecha_, ink[0], ink[1], ink[2], ha ? 0.8f : 0.4f);
      rotuloActualizar = "Aún no se ha buscado ninguna actualización. Pica para buscarla.";
    }
  }
  solid(px + 20.0f, py + 68.0f, pw - 40.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.08f);

  /* el cuerpo: las filas, recortadas contra su caja y corridas lo que diga la rueda */
  panelCuerpo_ = {px + 12.0f, py + 76.0f, pw - 24.0f, ph - 76.0f - 44.0f};
  const float izq = panelCuerpo_.x + 16.0f, der = panelCuerpo_.x + panelCuerpo_.w - 22.0f;
  auto alto = [](const ItemPanel &it) {
    switch (it.tipo) {
      case ItemPanel::Seccion: return 38.0f;
      case ItemPanel::Opciones: return 54.0f;
      case ItemPanel::Fila: return 42.0f;
      case ItemPanel::Juego: return 56.0f;
      case ItemPanel::Nota: return 32.0f;
      case ItemPanel::Info: return it.nota.empty() ? 36.0f : 54.0f;
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
        sitio[i] = {izq + (float)col * (anchoCarta + hueco), filaY, anchoCarta, altoCarta};
        col = (col + 1) % kCols;
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
  for (size_t i = 0; i < st_.items.size(); i++) {
    const ItemPanel &it = st_.items[i];
    const float h = sitio[i].h, y = panelCuerpo_.y - st_.corrida + sitio[i].y;
    ZonaItem &z = zonas_[i];
    z.caja = {sitio[i].x, y, sitio[i].w, h};
    if (y + h < panelCuerpo_.y || y > panelCuerpo_.y + panelCuerpo_.h) {   /* fuera de la vista */
      z.caja = {-10.0f, -10.0f, 0.0f, 0.0f};
      continue;
    }
    const bool hov = dentro && z.caja.inside(mx_, my_);
    const bool picable = it.tipo == ItemPanel::Interruptor || it.tipo == ItemPanel::Juego;
    if (hov && it.tipo != ItemPanel::Juego && (picable || it.tipo == ItemPanel::Fila || it.tipo == ItemPanel::Opciones || it.tipo == ItemPanel::Deslizador))
      mask9({z.caja.x, y + 2.0f, z.caja.w, h - 4.0f}, 1, 10.0f, 1.0f, 1.0f, 1.0f, picable ? 0.07f : 0.035f);
    const float yr = it.nota.empty() ? y + (h - 20.0f) * 0.5f : y + 10.0f;
    switch (it.tipo) {
      case ItemPanel::Seccion:
        text(it.rotulo, izq, y + 17.0f, 11.5f, accent[0] + 0.22f, accent[1] + 0.25f, 1.0f, 0.95f);
        break;
      case ItemPanel::Nota:
        text(it.rotulo, izq, y + 8.0f, 12.5f, dim[0], dim[1], dim[2], 0.85f);
        break;
      case ItemPanel::Info: {
        const Rect an{izq, y + 9.0f, 16.0f, 16.0f};
        icono(an, icoAnillo_, accent[0] + 0.22f, accent[1] + 0.25f, 1.0f, 0.95f);
        text("i", an.x + 8.0f, an.y + 2.0f, 11.0f, accent[0] + 0.22f, accent[1] + 0.25f, 1.0f, 0.95f, true);
        text(it.rotulo, izq + 24.0f, y + 9.0f, 12.5f, ink[0], ink[1], ink[2], 0.92f);
        if (!it.nota.empty()) text(it.nota, izq + 24.0f, y + 29.0f, 12.0f, dim[0], dim[1], dim[2], 0.85f);
        break;
      }
      case ItemPanel::Boton: {
        text(it.rotulo, izq, yr, 15.0f, ink[0], ink[1], ink[2], 1.0f);
        if (!it.nota.empty()) text(it.nota, izq, y + 34.0f, 11.5f, dim[0], dim[1], dim[2], 0.85f);
        const float wb = textWidth(it.valorTexto, 13.5f) + 30.0f;
        const Rect b{der - wb, y + (h - 30.0f) * 0.5f, wb, 30.0f};
        z.subs.push_back(b);
        const bool hb = dentro && b.inside(mx_, my_);
        /* `valor` apagado = boton que no se puede picar ahora (una descarga en marcha): sale gris */
        if (!it.valor) mask9(b, 0, 15.0f, 0.28f, 0.29f, 0.33f, 0.98f);
        else mask9(b, 0, 15.0f, accent[0], accent[1], accent[2], hb ? 1.0f : 0.92f);
        text(it.valorTexto, b.x + b.w * 0.5f, b.y + 6.5f, 13.5f, 1.0f, 1.0f, 1.0f, it.valor ? 1.0f : 0.70f, true);
        break;
      }
      case ItemPanel::Interruptor: {
        text(it.rotulo, izq, yr, 15.0f, ink[0], ink[1], ink[2], 1.0f);
        if (!it.nota.empty()) text(it.nota, izq, y + 34.0f, 11.5f, dim[0], dim[1], dim[2], 0.85f);
        const Rect sw{der - 46.0f, y + (h - 26.0f) * 0.5f, 46.0f, 26.0f};
        if (it.valor) mask9(sw, 0, 13.0f, accent[0], accent[1], accent[2], 0.98f);
        else mask9(sw, 0, 13.0f, 0.28f, 0.29f, 0.33f, 0.98f);
        mask9({it.valor ? sw.x + sw.w - 23.0f : sw.x + 3.0f, sw.y + 3.0f, 20.0f, 20.0f}, 0, 10.0f, 0.97f, 0.97f, 0.99f, 1.0f);
        break;
      }
      case ItemPanel::Opciones: {
        text(it.rotulo, izq, y + 17.0f, 15.0f, ink[0], ink[1], ink[2], 1.0f);
        float total = 6.0f;
        for (const std::string &o : it.opciones) total += textWidth(o, 13.5f) + 26.0f;
        const Rect caja{der - total, y + 10.0f, total, 34.0f};
        mask9(caja, 0, 17.0f, 0.17f, 0.175f, 0.21f, 0.98f);
        float ox = caja.x + 3.0f;
        for (size_t k = 0; k < it.opciones.size(); k++) {
          const float w = textWidth(it.opciones[k], 13.5f) + 26.0f;
          const Rect o{ox, caja.y + 3.0f, w, 28.0f};
          z.subs.push_back(o);
          const bool puesta = (int)k == it.elegido, ho = dentro && o.inside(mx_, my_);
          if (puesta) mask9(o, 0, 14.0f, accent[0], accent[1], accent[2], 0.98f);
          else if (ho) mask9(o, 0, 14.0f, 1.0f, 1.0f, 1.0f, 0.10f);
          text(it.opciones[k], o.x + o.w * 0.5f, o.y + 5.5f, 13.5f, ink[0], ink[1], ink[2], puesta || ho ? 1.0f : 0.70f,
               true);
          ox += w;
        }
        break;
      }
      case ItemPanel::Deslizador: {
        text(it.rotulo, izq, yr, 15.0f, ink[0], ink[1], ink[2], 1.0f);
        if (!it.nota.empty()) text(it.nota, izq, y + 34.0f, 11.5f, dim[0], dim[1], dim[2], 0.85f);
        const float anchoPista = 220.0f, cy = y + h * 0.5f;
        z.pista = {der - 70.0f - anchoPista, cy - 2.0f, anchoPista, 4.0f};
        const float f = std::max(0.0f, std::min(1.0f, it.frac)), kx = z.pista.x + f * anchoPista;
        mask9(z.pista, 0, 2.0f, 0.30f, 0.31f, 0.36f, 0.98f);
        mask9({z.pista.x, z.pista.y, kx - z.pista.x, 4.0f}, 0, 2.0f, accent[0], accent[1], accent[2], 1.0f);
        mask9({kx - 9.0f, cy - 9.0f, 18.0f, 18.0f}, 0, 9.0f, 0.97f, 0.97f, 0.99f, 1.0f);
        text(it.valorTexto, der - textWidth(it.valorTexto, 13.5f), cy - 9.0f, 13.5f, ink[0], ink[1], ink[2], 0.95f);
        break;
      }
      case ItemPanel::Fila: {
        text(it.rotulo, izq, y + 11.0f, 14.5f, ink[0], ink[1], ink[2], 1.0f);
        text(it.valorTexto, izq + 230.0f, y + 12.0f, 13.0f, dim[0], dim[1], dim[2], 0.90f);
        if (it.quitar) {
          const Rect q{der - 28.0f, y + 7.0f, 28.0f, 28.0f};
          z.subs.push_back(q);
          const bool hq = dentro && q.inside(mx_, my_);
          if (hq) mask9(q, 0, 14.0f, 1.0f, 1.0f, 1.0f, 0.14f);
          icono({q.x + 7.0f, q.y + 7.0f, 14.0f, 14.0f}, icoCerrar_, ink[0], ink[1], ink[2], hq ? 1.0f : 0.55f);
        }
        break;
      }
      case ItemPanel::Juego: {
        /* una carta: la caratula (o, si Steam no la tiene, una ficha de color con la inicial), el
           nombre debajo y a su derecha la pastillita 3D / 2D. Se pica la carta y arranca; la
           pastillita cambia si ese juego entra en 3D solo. */
        const Rect cara{z.caja.x, y, z.caja.w, z.caja.w * 1.5f};
        if (hov) mask9({cara.x - 3.0f, cara.y - 3.0f, cara.w + 6.0f, cara.h + 6.0f}, 1, 13.0f, accent[0], accent[1], accent[2], 1.0f);
        if (it.img && it.imgW > 0 && it.imgH > 0) {
          imagenes_.push_back({cara, panelCuerpo_, it.img->data(), it.imgW, it.imgH, 10.0f});
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
          mask9(cara, 1, 10.0f, 0.22f + 0.38f * cr, 0.20f + 0.34f * cg, 0.30f + 0.42f * cb, 1.0f);
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
        text(nombre, cara.x + 2.0f, cara.y + cara.h + 13.0f, 13.0f, ink[0], ink[1], ink[2], hov ? 1.0f : 0.90f);
        const bool hc2 = dentro && chip.inside(mx_, my_);
        if (it.valor) mask9(chip, 0, 12.0f, accent[0], accent[1], accent[2], hc2 ? 1.0f : 0.92f);
        else mask9(chip, 0, 12.0f, 0.30f, 0.31f, 0.36f, hc2 ? 1.0f : 0.92f);
        const Rect bola{it.valor ? chip.x + chip.w - 22.0f : chip.x + 2.0f, chip.y + 2.0f, 20.0f, 20.0f};
        mask9(bola, 0, 10.0f, 0.97f, 0.97f, 0.99f, 1.0f);
        /* la letra de lo que esta puesto va en la bola; la otra, tenue, en el hueco que deja (v0.92:
           las dos a la misma altura y la bola a 2 px de cada borde; antes quedaba 3 arriba y 1 abajo) */
        text(it.valor ? "3D" : "2D", bola.x + bola.w * 0.5f, chip.y + 6.0f, 10.5f, 0.12f, 0.12f, 0.16f, 1.0f, true);
        text(it.valor ? "2D" : "3D", it.valor ? chip.x + 19.0f : chip.x + chip.w - 19.0f, chip.y + 6.0f, 10.5f, 1.0f, 1.0f, 1.0f, 0.55f, true);
        break;
      }
    }
  }
  hayRecorte_ = false;
  /* la barrita de la derecha, solo si hay mas lista que caja */
  if (tope > 0.0f) {
    const float largo = std::max(30.0f, panelCuerpo_.h * panelCuerpo_.h / contenidoAlto_);
    const float sy = panelCuerpo_.y + (panelCuerpo_.h - largo) * (st_.corrida / tope);
    mask9({panelCuerpo_.x + panelCuerpo_.w - 7.0f, sy, 4.0f, largo}, 0, 2.0f, 1.0f, 1.0f, 1.0f, 0.28f);
  }
  /* el pie */
  solid(px + 20.0f, py + ph - 42.0f, pw - 40.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.08f);
  if (!st_.pie.empty()) text(st_.pie, px + 28.0f, py + ph - 30.0f, 12.5f, ink[0], ink[1], ink[2], 0.88f);
  if (!st_.version.empty())
    text(st_.version, px + pw - 28.0f - textWidth(st_.version, 12.0f), py + ph - 30.0f, 12.0f, dim[0], dim[1], dim[2],
         0.75f);
  /* el rotulo del boton de actualizar, al pasar por encima: por encima de todo lo demas */
  if (ha && !rotuloActualizar.empty()) {
    const float w = textWidth(rotuloActualizar, 12.5f) + 24.0f;
    const Rect r{panelCerrar_.x + panelCerrar_.w - w, panelActualizar_.y + panelActualizar_.h + 8.0f, w, 30.0f};
    mask9(r, 0, 10.0f, 0.10f, 0.10f, 0.13f, 0.97f);
    text(rotuloActualizar, r.x + 12.0f, r.y + 8.0f, 12.5f, ink[0], ink[1], ink[2], 0.95f);
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
  if (!panelCaja_.inside(mx, my)) return HitNone;
  if (panelCerrar_.inside(mx, my)) return HitPanelCerrar;
  if (panelActualizar_.inside(mx, my)) return HitPanelActualizar;
  for (size_t i = 0; i < panelPestanas_.size(); i++)
    if (panelPestanas_[i].inside(mx, my)) {
      if (sub) *sub = (int)i;
      return HitPanelPestana;
    }
  if (my < panelCaja_.y + 68.0f) return HitPanelTitulo;
  if (!panelCuerpo_.inside(mx, my)) return HitPanel;
  for (size_t i = 0; i < zonas_.size() && i < st_.items.size(); i++) {
    const ZonaItem &z = zonas_[i];
    if (z.caja.w <= 0.0f || !z.caja.inside(mx, my)) continue;
    const ItemPanel &it = st_.items[i];
    if (item) *item = (int)i;
    switch (it.tipo) {
      case ItemPanel::Interruptor:
        return HitPanelItem;
      case ItemPanel::Juego:
        if (!z.subs.empty() && z.subs[0].inside(mx, my) && sub) *sub = 1;
        return HitPanelItem;
      case ItemPanel::Opciones:
        for (size_t k = 0; k < z.subs.size(); k++)
          if (z.subs[k].inside(mx, my)) {
            if (sub) *sub = (int)k;
            return HitPanelItem;
          }
        return HitPanel;
      case ItemPanel::Deslizador: {
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
  if (item < 0 || (size_t)item >= zonas_.size()) return 0.0f;
  const Rect &p = zonas_[(size_t)item].pista;
  return std::max(0.0f, std::min(1.0f, (mxa / sc_ - p.x) / std::max(1.0f, p.w)));
}

void Hud::correr(float cuanto) {
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
  if (x) *x = pillPanel_.x * sc_;
  if (y) *y = pillPanel_.y * sc_;
  if (w) *w = pillPanel_.w * sc_;
  if (h) *h = pillPanel_.h * sc_;
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
  const float der = (pillPanel_.x + pillPanel_.w) * sc_;
  if (x) *x = der - ancho;
  if (y) *y = pillPanel_.y * sc_;
  if (w) *w = ancho;
  if (h) *h = pillPanel_.h * sc_;
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
    if (askSbs_.inside(mx, my)) return HitAskSbs;
    if (askDont_.inside(mx, my)) return HitAskDont;
  }
  if (!st_.overlayOn) return HitNone;
  if (sw3D_.inside(mx, my)) return Hit3D;   /* el interruptor ON/OFF ; se pica en toda la pista */
  float pad = 10.0f;
  Rect dt = {depTrack_.x - pad, depTrack_.y - pad, depTrack_.w + 2.0f * pad, depTrack_.h + 2.0f * pad};
  Rect pt = {popTrack_.x - pad, popTrack_.y - pad, popTrack_.w + 2.0f * pad, popTrack_.h + 2.0f * pad};
  if (dt.inside(mx, my)) return HitDepth;
  if (pt.inside(mx, my)) return HitPopout;
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
    st_.popout = std::max(-50, std::min(50, (int)std::lround(f * 100.0f)));
  }
}
