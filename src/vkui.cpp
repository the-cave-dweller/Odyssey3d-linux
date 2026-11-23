/* vkui.cpp — atlas (fuentes + máscaras), disposición de la interfaz y manejo del ratón. */
#include "vkui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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
    return v < 256 ? v : '?';
  }
  if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
    unsigned v = ((unsigned)(c & 0x0F) << 12) | ((unsigned)(s[i + 1] & 0x3F) << 6) | (unsigned)(s[i + 2] & 0x3F);
    i += 3;
    return v < 256 ? v : '?';
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
  float dist = std::hypot(ox, oy) + std::min(std::max(dx, dy), 0.0f);
  float a = 0.5f - dist;
  if (a <= 0.0f) return 0;
  if (a >= 1.0f) return 255;
  return (uint8_t)(a * 255.0f);
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
  auto uvOf = [&](int x, int y, int w, int h, Rect *r) {
    r->x = (float)x / (float)aw_;
    r->y = (float)y / (float)ah_;
    r->w = (float)w / (float)aw_;
    r->h = (float)h / (float)ah_;
  };
  auto put = [&](int x, int y, int w, int h, const uint8_t *src) {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++) atlas_[(size_t)(y + j) * (size_t)aw_ + (size_t)(x + i)] = src[(size_t)j * (size_t)w + (size_t)i];
  };

  /* 1) píxel blanco: cualquier relleno liso sale de aquí */
  {
    int x, y;
    uint8_t w4[4] = {255, 255, 255, 255};
    if (!place(2, 2, &x, &y)) { if (err) *err = "atlas sin sitio para el píxel blanco"; return false; }
    put(x, y, 2, 2, w4);
    white_ = {(float)x, (float)y, 2.0f, 2.0f};   /* en píxeles del atlas */
  }

  /* 2) máscara de píldora (radio = mitad del lado) y de tarjeta (radio suave) */
  for (int which = 0; which < 2; which++) {
    int size = which == 0 ? 96 : 48;
    float radius = which == 0 ? 48.0f : 8.0f;
    std::vector<uint8_t> m((size_t)size * (size_t)size);
    for (int j = 0; j < size; j++)
      for (int i = 0; i < size; i++) m[(size_t)j * (size_t)size + (size_t)i] = roundedCover(i, j, size, radius);
    int x, y;
    if (!place(size, size, &x, &y)) { if (err) *err = "atlas sin sitio para las máscaras"; return false; }
    put(x, y, size, size, m.data());
    Rect &dst = which == 0 ? pill_ : card_;
    uvOf(x, y, size, size, &dst);
    dst.x = (float)x;   /* guardamos píxeles del atlas: mask9 los convierte */
    dst.y = (float)y;
    dst.w = (float)size;
    dst.h = (float)size;
  }

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
    for (unsigned cp = 32; cp < 256; cp++) {
      if (cp == 32) continue;
      if (FT_Load_Char(face, cp, FT_LOAD_RENDER) != 0) continue;
      FT_GlyphSlot g = face->glyph;
      int w = (int)g->bitmap.width, h = (int)g->bitmap.rows;
      if (!w || !h) continue;
      int x, y;
      if (!place(w, h, &x, &y)) { okAll = false; break; }
      put(x, y, w, h, g->bitmap.buffer);
      Glyph &gl = glyphs_[cp];
      gl.w = w, gl.h = h;
      gl.bx = (int)g->bitmap_left;
      gl.by = (int)g->bitmap_top;
      gl.adv = (int)(g->advance.x / 64);
      gl.u0 = (float)x / (float)aw_;
      gl.v0 = (float)y / (float)ah_;
      gl.u1 = (float)(x + w) / (float)aw_;
      gl.v1 = (float)(y + h) / (float)ah_;
      gl.ok = true;
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
  /* el cuadro del blanco se encoge media celda para no interpolar con el vecino */
  uv.x = (white_.x + 0.25f) / (float)aw_;
  uv.y = (white_.y + 0.25f) / (float)ah_;
  uv.w = 0.5f / (float)aw_;
  uv.h = 0.5f / (float)ah_;
  quad(r, uv, cr, cg, cb, ca);
}

/* 9 porciones: las esquinas conservan el radio y el centro se estira. */
void Hud::mask9(Rect r, int which, float radius, float cr, float cg, float cb, float ca) {
  const Rect &m = which == 0 ? pill_ : card_;
  float msz = m.w;
  float rad = std::min(radius, std::min(r.w, r.h) * 0.5f);
  float fr = std::min(rad / msz, 0.49f);           /* radio en unidades de la máscara */
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

float Hud::textWidth(const std::string &s, float size) const {
  float sc = size / (float)kFontPx;
  float wsum = 0.0f;
  size_t i = 0;
  while (i < s.size()) {
    unsigned cp = nextCp(s, i);
    const Glyph &g = glyphs_[cp < 256 ? cp : '?'];
    wsum += g.ok ? (float)g.adv : (float)kFontPx * 0.5f;
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
    const Glyph &g = glyphs_[cp < 256 ? cp : '?'];
    if (!g.ok) { pen += (float)kFontPx * 0.5f * sc; continue; }
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
  W_ = w;
  H_ = h;
  /* La interfaz se mide en puntos logicos del escritorio y luego se escala al pixel real del
     panel. Antes el escalon salia del alto fisico (2160 -> x2), y con el escritorio al 150% la
     tarjeta y la pastilla salian la mitad mas grandes de lo que se ve en cualquier ventana.
     Con el alto logico a la vista el escalon es simplemente la relacion de pixeles: 1,5 aqui.
     Si nadie lo pasa, se conserva el criterio viejo de un lienzo de ~1080 px de alto. */
  const float relacion = altoLogico > 0.0f ? (float)h / altoLogico : 0.0f;
  sc_ = relacion > 0.25f ? std::max(1.0f, std::min(2.0f, relacion))
                         : std::max(1.0f, std::min(2.0f, (float)h / 1080.0f));
  const int vw = (int)std::lround((float)w / sc_), vh = (int)std::lround((float)h / sc_);
  if (!st_.overlayOn && !st_.ask) return;

  const float accent[3] = {0.20f, 0.78f, 0.35f};
  const float ink[4] = {0.92f, 0.93f, 0.95f, 1.0f};
  const float dim[4] = {0.68f, 0.70f, 0.74f, 0.85f};

  /* ---- pastilla superior derecha ---- */
  if (st_.overlayOn) {
    float pw = std::min(660.0f, (float)vw - 48.0f);
    float ph = 84.0f;
    float x0 = (float)vw - 24.0f - pw, y0 = 24.0f;
    pillPanel_ = {x0, y0, pw, ph};
    mask9(pillPanel_, 0, 24.0f, 0.12f, 0.13f, 0.15f, 0.78f);
    /* acento verde arriba, como la app oficial cuando el 3D está puesto */
    float ac = st_.weaveOn ? 1.0f : 0.25f;
    solid(x0 + 18.0f, y0 + 8.0f, pw - 36.0f, 2.0f, accent[0], accent[1], accent[2], 0.55f * ac);

    btn3D_ = {x0 + 18.0f, y0 + 26.0f, 150.0f, 34.0f};
    btnOv_ = {x0 + 178.0f, y0 + 26.0f, 160.0f, 34.0f};
    bool hov3 = btn3D_.inside(mx_, my_), hovO = btnOv_.inside(mx_, my_);
    float bg = hov3 ? 0.30f : 0.22f;
    mask9(btn3D_, 1, 8.0f, st_.weaveOn ? accent[0] : bg, st_.weaveOn ? accent[1] : bg, st_.weaveOn ? accent[2] : bg,
          st_.weaveOn ? 0.92f : 0.95f);
    mask9(btnOv_, 1, 8.0f, hovO ? 0.30f : 0.22f, hovO ? 0.32f : 0.24f, hovO ? 0.34f : 0.27f, 0.95f);
    text(st_.weaveOn ? "Apagar 3D" : "Encender 3D", btn3D_.x + 12.0f, btn3D_.y + 8.0f, 15.0f, ink[0], ink[1], ink[2],
         1.0f);
    text(st_.overlayOn ? "Ocultar capa" : "Mostrar capa", btnOv_.x + 12.0f, btnOv_.y + 8.0f, 15.0f, ink[0], ink[1],
         ink[2], 1.0f);
    text("Ctrl+Shift+1", btn3D_.x, btn3D_.y + 44.0f, 11.0f, dim[0], dim[1], dim[2], dim[3]);
    text("Ctrl+Shift+2", btnOv_.x, btnOv_.y + 44.0f, 11.0f, dim[0], dim[1], dim[2], dim[3]);
    /* el aviso de Tab va en la pastilla: es la tecla que deja el raton y el teclado libres para
       lo que hay detras (nos minimizamos y el panel vuelve a 2D) */
    text("Tab  entrega la pantalla", btnOv_.x + 76.0f, btnOv_.y + 44.0f, 11.0f, dim[0], dim[1], dim[2], dim[3]);

    auto slider = [&](Rect *track, Rect *knob, const char *label, const char *keys, float frac, const char *value) {
      float tx = track->x, ty = track->y;
      text(label, tx, ty - 20.0f, 12.0f, dim[0], dim[1], dim[2], dim[3]);
      text(value, tx + track->w - textWidth(value, 12.0f), ty - 20.0f, 12.0f, ink[0], ink[1], ink[2], ink[3]);
      solid(tx, ty, track->w, 4.0f, 0.30f, 0.32f, 0.35f, 0.9f);
      float kx = tx + frac * track->w;
      solid(tx, ty, kx - tx, 4.0f, accent[0], accent[1], accent[2], 0.95f);
      float kr = 9.0f;
      mask9({kx - kr, ty + 2.0f - kr, 2.0f * kr, 2.0f * kr}, 0, kr, 0.95f, 0.96f, 0.98f, 1.0f);
      *knob = {kx - kr, ty + 2.0f - kr, 2.0f * kr, 2.0f * kr};
      text(keys, tx, ty + 18.0f, 11.0f, dim[0], dim[1], dim[2], dim[3]);
    };
    float dfrac = (float)st_.depth / 100.0f;
    float pfrac = (float)(st_.popout + 50) / 100.0f;
    depTrack_ = {x0 + 360.0f, y0 + 34.0f, 120.0f, 4.0f};
    popTrack_ = {x0 + 510.0f, y0 + 34.0f, 120.0f, 4.0f};
    slider(&depTrack_, &depKnob_, "3D Profundidad", "Ctrl+Shift+3 / 4", dfrac, std::to_string(st_.depth).c_str());
    slider(&popTrack_, &popKnob_, "3D Salida", "Ctrl+Shift+5 / 6", pfrac, std::to_string(st_.popout).c_str());

    /* indicador pequeño del estado de la lente y el scaler */
    const char *hw = st_.hud == 3 ? "lente y scaler activos" : (st_.hud == 0 ? "sin lente ni scaler" : "hardware parcial");
    float dotc[3] = {1.0f, 0.2f, 0.2f};
    if (st_.hud == 3) { dotc[0] = accent[0]; dotc[1] = accent[1]; dotc[2] = accent[2]; }
    else if (st_.hud != 0) { dotc[0] = 1.0f; dotc[1] = 0.6f; dotc[2] = 0.0f; }
    mask9({x0 + 8.0f, y0 + 8.0f, 6.0f, 6.0f}, 0, 3.0f, dotc[0], dotc[1], dotc[2], 1.0f);
    text(hw, x0 + 20.0f, y0 - 16.0f, 11.0f, dim[0], dim[1], dim[2], dim[3]);
  }

  /* ---- aviso abajo a la derecha ---- */
  if (st_.ask) {
    float aw = std::min(520.0f, (float)vw - 48.0f), ah = 216.0f;
    float ax = (float)vw - 24.0f - aw, ay = (float)vh - 24.0f - ah;
    askPanel_ = {ax, ay, aw, ah};
    mask9(askPanel_, 1, 14.0f, 0.06f, 0.07f, 0.08f, 0.92f);
    /* el titulo lo pone el servicio (la pelicula que da pantalla completa); si viene
       vacio se usa el texto de la aplicacion oficial */
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
    text(encaja(askTitulo_.empty() ? "¿Convertir el vídeo actual a 3D?" : askTitulo_, 19.0f, aw - 92.0f), ax + 24.0f,
         ay + 22.0f, 19.0f, ink[0], ink[1], ink[2], 1.0f);
    text(encaja(askCuerpo_.empty() ? "Elige el formato de la imagen que estás viendo." : askCuerpo_, 13.0f,
                aw - 48.0f),
         ax + 24.0f, ay + 52.0f, 13.0f, dim[0], dim[1], dim[2], dim[3]);
    float tw = (aw - 48.0f - 24.0f) / 3.0f;
    askSbs_ = {ax + 24.0f, ay + 86.0f, tw, 78.0f};
    askTop_ = {ax + 24.0f + tw + 12.0f, ay + 86.0f, tw, 78.0f};
    askFlat_ = {ax + 24.0f + 2.0f * (tw + 12.0f), ay + 86.0f, tw, 78.0f};
    Rect tiles[3] = {askSbs_, askTop_, askFlat_};
    const char *names[3] = {"Lado a lado", "Arriba y abajo", "2D plano"};
    const char *subs[3] = {"estéreo horizontal", "estéreo vertical", "sin profundidad"};
    for (int i = 0; i < 3; i++) {
      bool hov = tiles[i].inside(mx_, my_);
      mask9(tiles[i], 1, 10.0f, hov ? 0.20f : 0.15f, hov ? 0.22f : 0.16f, hov ? 0.24f : 0.18f, 0.98f);
      if (hov) mask9(tiles[i], 1, 10.0f, accent[0], accent[1], accent[2], 0.35f);
      /* muestra del formato: dos mitades separadas */
      float ix = tiles[i].x + tiles[i].w * 0.5f - 22.0f, iy = tiles[i].y + 14.0f;
      if (i == 0) {
        solid(ix, iy, 20.0f, 14.0f, 0.85f, 0.86f, 0.9f, 0.9f);
        solid(ix + 24.0f, iy, 20.0f, 14.0f, 0.55f, 0.58f, 0.66f, 0.9f);
      } else if (i == 1) {
        solid(ix, iy, 20.0f, 6.0f, 0.85f, 0.86f, 0.9f, 0.9f);
        solid(ix, iy + 8.0f, 20.0f, 6.0f, 0.55f, 0.58f, 0.66f, 0.9f);
      } else {
        solid(ix, iy, 44.0f, 14.0f, 0.7f, 0.72f, 0.78f, 0.9f);
      }
      text(names[i], tiles[i].x + 10.0f, tiles[i].y + 38.0f, 14.0f, ink[0], ink[1], ink[2], 1.0f);
      text(subs[i], tiles[i].x + 10.0f, tiles[i].y + 58.0f, 11.0f, dim[0], dim[1], dim[2], dim[3]);
    }
    askDont_ = {ax + 24.0f, ay + 180.0f, 20.0f, 20.0f};
    mask9(askDont_, 1, 5.0f, 0.25f, 0.27f, 0.30f, 1.0f);
    if (st_.remember)
      solid(askDont_.x + 4.0f, askDont_.y + 4.0f, 12.0f, 12.0f, accent[0], accent[1], accent[2], 1.0f);
    text("Guardar el tipo y no volver a preguntar", askDont_.x + 30.0f, askDont_.y + 2.0f, 13.0f, dim[0], dim[1],
         dim[2], dim[3]);
    askClose_ = {ax + aw - 44.0f, ay + 18.0f, 24.0f, 24.0f};
    bool hc = askClose_.inside(mx_, my_);
    float cw = hc ? 1.0f : 0.75f;
    float cx0 = askClose_.x + 4.0f, cy0 = askClose_.y + 11.0f;
    /* la erre dibujada con dos barras giradas no es posible con cuadros alineados: dos rombos */
    for (int s = 0; s < 2; s++)
      for (int i = 0; i < 8; i++) {
        float t = (float)i / 7.0f;
        float px = cx0 + t * 16.0f;
        float py = s == 0 ? cy0 - 7.0f + t * 14.0f : cy0 + 7.0f - t * 14.0f;
        solid(px, py, 3.0f, 3.0f, cw, cw, cw, 1.0f);
      }
  }

  /* ---- línea de estado abajo a la izquierda ---- */
  if (!info_.empty()) {
    float tw = textWidth(info_, 13.0f);
    solid(16.0f, (float)h - 34.0f, tw + 16.0f, 24.0f, 0.05f, 0.06f, 0.07f, 0.65f);
    text(info_, 24.0f, (float)h - 28.0f, 13.0f, ink[0], ink[1], ink[2], 0.85f);
  }
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

Hud::Hit Hud::hit(float mxa, float mya) const {
  const float mx = mxa / sc_, my = mya / sc_;
  if (st_.ask) {
    if (askClose_.inside(mx, my)) return HitAskClose;
    if (askSbs_.inside(mx, my)) return HitAskSbs;
    if (askTop_.inside(mx, my)) return HitAskTop;
    if (askFlat_.inside(mx, my)) return HitAskFlat;
    if (askDont_.inside(mx, my)) return HitAskDont;
  }
  if (!st_.overlayOn) return HitNone;
  if (btn3D_.inside(mx, my)) return Hit3D;
  if (btnOv_.inside(mx, my)) return HitOverlay;
  float pad = 10.0f;
  Rect dt = {depTrack_.x - pad, depTrack_.y - pad, depTrack_.w + 2.0f * pad, depTrack_.h + 2.0f * pad};
  Rect pt = {popTrack_.x - pad, popTrack_.y - pad, popTrack_.w + 2.0f * pad, popTrack_.h + 2.0f * pad};
  if (dt.inside(mx, my)) return HitDepth;
  if (pt.inside(mx, my)) return HitPopout;
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
