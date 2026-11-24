#include "weave.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>

static const char *VERT = R"GLSL(#version 330
in vec2 pos; in vec2 uv; in vec2 av2; in vec2 av3; in vec3 av4;
out vec2 fuv; out vec2 v2; out vec2 v3; out vec3 v4;
void main(){ gl_Position = vec4(pos, 0.0, 1.0); fuv = uv; v2 = av2; v3 = av3; v4 = av4; }
)GLSL";

/* Port exacto del pixel shader del weave (validado contra la captura del hub):
   base = v2.x*rsqrt(1+|v3|^2) - 2*v2.y*(corrB-0.5) + corrA ;  fase = fract(base + v4[c] + 0.25) */
static const char *FRAG = R"GLSL(#version 330
uniform sampler2D uSrc, uCorrA, uCorrB;
uniform vec2 uRes;
uniform float uFS, uXTalkFac, uXTalkDyn, uContrast, uCorrAScale, uConv, uAlign, uPitch;
uniform float uRampaX, uRampaY;
uniform int uWeave, uSrcSwapLR, uHudOk, uLayout, uSrcFlipY;
in vec2 fuv; in vec2 v2; in vec2 v3; in vec3 v4;
out vec4 o;
vec3 srcHalf(vec2 uv, float which){        // una mitad del estereo (uv.y=0 arriba en pantalla)
    vec2 s = (uLayout==0) ? vec2(which + uv.x*0.5, uv.y)                     // lado a lado: 0=izq, 0.5=der
                          : vec2(uv.x, which + uv.y*0.5);                    // arriba/abajo: 0=abajo, 0.5=arriba
    if(uSrcFlipY==1) s.y = 1.0 - s.y;      // la textura de origen se llena con FLIP_Y=1 (arriba de la imagen en v=1)
    return texture(uSrc, s).rgb;
}
void main(){
    if(uWeave==0){ o = vec4(srcHalf(fuv, 0.0), 1.0); return; }   // pase 2D (vista izquierda)
    if(gl_FragCoord.x < 36.0 && gl_FragCoord.y > uRes.y - 36.0){   // indicador de estado (esquina superior izquierda)
        o = (uHudOk==3) ? vec4(0.0,1.0,0.0,1.0) : (uHudOk==0 ? vec4(1.0,0.0,0.0,1.0) : vec4(1.0,0.6,0.0,1.0));
        return;
    }
    vec2 suv = vec2(gl_FragCoord.x/uRes.x, 1.0 - gl_FragCoord.y/uRes.y);
    float corrA = texture(uCorrA, suv).r * uCorrAScale;
    float corrB = texture(uCorrB, suv).r;
    float rsq = inversesqrt(1.0 + dot(v3, v3));
    float base = v2.x*rsq - 2.0*v2.y*(corrB - 0.5) + corrA;
    float lx = (uSrcSwapLR==1) ? 0.0 : 0.5;   // orden de ojos validado en el Hub
    float rx = (uSrcSwapLR==1) ? 0.5 : 0.0;
    vec3 L = srcHalf(fuv, lx);
    vec3 R = srcHalf(uLayout==0 ? (fuv + vec2(uConv, 0.0)) : (fuv + vec2(0.0, -uConv)), rx);
    L = (L-0.5)*uContrast + 0.5; R = (R-0.5)*uContrast + 0.5;
    vec3 dRL = R - L;
    float xt = ((1.0 + dot(v3, v3)) * v2.y*v2.y) * uXTalkDyn + uXTalkFac;
    xt = xt/(1.0 - xt);
    vec3 Lp = L - xt*dRL; vec3 Rp = R + xt*dRL;
    /* Rampa fina de fase, igual para los tres canales (ODYSSEY3D_RAMPA_X / _Y, ciclos por pixel
       de panel). Mueve solo el paso de la rejilla escrita; los numeros y el por que estan en
       shaders/weave.frag y tools/paso_rejilla.py. El signo va al reves del ajuste que se busca en
       la porteadora medida: la que sale por la pantalla es el espejo de la escrita, asi que para
       subir el kx medido de 0,4473 a 0,4501 hace falta rampa_x = -0,00283. El y va al reves que en
       el camino Vulkan porque gl_FragCoord.y apunta hacia arriba y px.y del shader real, abajo. */
    float rampa = (uRampaX != 0.0 || uRampaY != 0.0)
                ? fract(uRampaX * gl_FragCoord.x + uRampaY * (uRes.y - gl_FragCoord.y)) : 0.0;
    vec3 outc;
    for(int c=0;c<3;c++){
        float phase = fract(base + v4[c]*uPitch + uAlign - 0.25 + rampa);
        float t = 2.0*phase - 1.0;
        float w = clamp(-abs(t)*uFS + uFS*0.5 + 0.5, 0.0, 1.0);
        outc[c] = mix(Rp[c], Lp[c], w);
    }
    o = vec4(outc, 1.0);
}
)GLSL";

bool Weave::init(std::string *err) {
  prog_ = glLinkProgram(VERT, FRAG, err);
  if (!prog_) return false;
  uRes_ = glGetUniformLocation(prog_, "uRes");
  uFS_ = glGetUniformLocation(prog_, "uFS");
  uXTF_ = glGetUniformLocation(prog_, "uXTalkFac");
  uXTD_ = glGetUniformLocation(prog_, "uXTalkDyn");
  uContrast_ = glGetUniformLocation(prog_, "uContrast");
  uCAS_ = glGetUniformLocation(prog_, "uCorrAScale");
  uConv_ = glGetUniformLocation(prog_, "uConv");
  uAlign_ = glGetUniformLocation(prog_, "uAlign");
  uPitch_ = glGetUniformLocation(prog_, "uPitch");
  uRampaX_ = glGetUniformLocation(prog_, "uRampaX");
  uRampaY_ = glGetUniformLocation(prog_, "uRampaY");
  uWeave_ = glGetUniformLocation(prog_, "uWeave");
  uSwap_ = glGetUniformLocation(prog_, "uSrcSwapLR");
  uHud_ = glGetUniformLocation(prog_, "uHudOk");
  uLayout_ = glGetUniformLocation(prog_, "uLayout");
  uFlipY_ = glGetUniformLocation(prog_, "uSrcFlipY");
  uCorrA_ = glGetUniformLocation(prog_, "uCorrA");
  uCorrB_ = glGetUniformLocation(prog_, "uCorrB");
  uSrc_ = glGetUniformLocation(prog_, "uSrc");

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, 4 * 11 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
  /* 11 floats por vértice: pos2 uv2 v2(2) v3(2) v4(3) */
  const GLsizei st = 11 * sizeof(float);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, st, (void *)0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (void *)(2 * sizeof(float)));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, st, (void *)(4 * sizeof(float)));
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, st, (void *)(6 * sizeof(float)));
  glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, st, (void *)(8 * sizeof(float)));
  for (int i = 0; i < 5; i++) glEnableVertexAttribArray(i);
  glBindVertexArray(0);
  glCheckErr("Weave::init");

  /* correcciones neutras hasta que se carguen las del panel */
  std::vector<uint8_t> a(64 * 64, 0), b(64 * 64, 128);
  corrA_ = glTexFromR8(64, 64, a.data(), true);
  corrB_ = glTexFromR8(64, 64, b.data(), true);
  return true;
}

void Weave::destroy() {
  if (prog_) {
    glDeleteProgram(prog_);
    prog_ = 0;
  }
  if (vbo_) {
    glDeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (vao_) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  if (corrA_) {
    glDeleteTextures(1, &corrA_);
    corrA_ = 0;
  }
  if (corrB_) {
    glDeleteTextures(1, &corrB_);
    corrB_ = 0;
  }
}

void Weave::setPanelSize(int w, int h) {
  W_ = w;
  H_ = h;
}

void Weave::setCorrectionsFromPng(const std::string &a, const std::string &b) {
  int wa = 0, ha = 0, wb = 0, hb = 0;
  std::vector<uint8_t> ra, rb;
  if (!pngLoadRGBA(a, wa, ha, ra) || !pngLoadRGBA(b, wb, hb, rb)) {
    std::fprintf(stderr, "[weave] sin mapas de corrección del panel (%s / %s): se usa corrección neutra\n",
                 a.c_str(), b.c_str());
    return;
  }
  std::vector<uint8_t> ga((size_t)wa * ha), gb((size_t)wb * hb);
  for (size_t i = 0; i < ga.size(); i++) ga[i] = ra[i * 4];
  for (size_t i = 0; i < gb.size(); i++) gb[i] = rb[i * 4];
  glDeleteTextures(1, &corrA_);
  glDeleteTextures(1, &corrB_);
  corrA_ = glTexFromR8(wa, ha, ga.data(), true);
  corrB_ = glTexFromR8(wb, hb, gb.data(), true);
  corrReal_ = true;
  std::fprintf(stderr, "[weave] mapas de corrección del panel cargados (%dx%d)\n", wa, ha);
}

CornerAttrs lensCorner(const LensField &field_, const EyePose &eye_, int W_, int H_, double ndcX, double ndcY) {
  const double u = (ndcX + 1.0) * 0.5; /* 0 = izquierda, 1 = derecha */
  const double v = (ndcY + 1.0) * 0.5; /* 0 = arriba, 1 = abajo  (y hacia abajo en pantalla) */
  const double px = u * W_ - 0.5;
  const double py = v * H_ - 0.5;
  const double eyeXPx = eye_.x / field_.pxPcm + W_ / 2.0;
  const double eyeYPx = eye_.y / field_.pxPcm;
  const double dx = eyeXPx - px;
  const double dy = eyeYPx - py;
  const double ezEff = eye_.z * (field_.ezEffRef / field_.optZ);

  CornerAttrs a{};
  a.v3[0] = (float)(dx * field_.pxPcm / ezEff);
  a.v3[1] = (float)(dy * field_.pxPcm / ezEff);
  const double proj = (dx + field_.slant * dy) * field_.pxPcm;
  a.v2[1] = (float)(proj / eye_.z);
  a.v2[0] = (float)(field_.fxOverFy * a.v2[1]);
  for (int c = 0; c < 3; c++)
    a.v4[c] = (float)(field_.c00[c] + field_.kx * (px + 0.5) + field_.ky * (py + 0.5));
  return a;
}

CornerAttrs Weave::fill(double ndcX, double ndcY) const { return lensCorner(field_, eye_, W_, H_, ndcX, ndcY); }

void Weave::refreshCorners() {
  /* TL, TR, BL, BR: posición NDC, uv y atributos (el NDC en y coincide con el orden de pantalla). */
  static const float quad[4][4] = {{-1.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 0.0f},
                                   {-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, -1.0f, 1.0f, 1.0f}};
  static const double ndc[4][2] = {{-1.0, -1.0}, {1.0, -1.0}, {-1.0, 1.0}, {1.0, 1.0}};
  CornerAttrs fa[4];
  float mn = 1e30f;
  for (int i = 0; i < 4; i++) {
    fa[i] = fill(ndc[i][0], ndc[i][1]);
    for (int c = 0; c < 3; c++) mn = mn < fa[i].v4[c] ? mn : fa[i].v4[c];
  }
  const int v4min = (int)std::floor(mn); /* solo importa frac(): restar el entero afina la precisión float32 */
  float data[4 * 11];
  for (int i = 0; i < 4; i++) {
    float *d = data + i * 11;
    d[0] = quad[i][0];
    d[1] = quad[i][1];
    d[2] = quad[i][2];
    d[3] = quad[i][3];
    d[4] = fa[i].v2[0];
    d[5] = fa[i].v2[1];
    d[6] = fa[i].v3[0];
    d[7] = fa[i].v3[1];
    d[8] = fa[i].v4[0] - v4min;
    d[9] = fa[i].v4[1] - v4min;
    d[10] = fa[i].v4[2] - v4min;
  }
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof data, data);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Weave::draw(GLuint srcTex) {
  glUseProgram(prog_);
  glViewport(0, 0, W_, H_);
  glUniform2f(uRes_, (float)W_, (float)H_);
  glUniform1f(uFS_, tune_.filterSlope);
  glUniform1f(uXTF_, tune_.xtalkFac);
  glUniform1f(uXTD_, tune_.xtalkDyn);
  glUniform1f(uContrast_, tune_.contrast);
  glUniform1f(uCAS_, tune_.corrAScale);
  glUniform1f(uConv_, tune_.conv);
  glUniform1f(uAlign_, tune_.align);
  glUniform1f(uPitch_, tune_.pitch);
  /* el visor real pasa la rampa por el bloque uniforme (src/vkweave.cpp); esta copia GL es el
     instrumento de laboratorio, asi que lee el mismo ajuste del entorno */
  {
    static const float rx = [] { const char *e = std::getenv("ODYSSEY3D_RAMPA_X"); return e ? (float)std::atof(e) : 0.0f; }();
    static const float ry = [] { const char *e = std::getenv("ODYSSEY3D_RAMPA_Y"); return e ? (float)std::atof(e) : 0.0f; }();
    if (uRampaX_ >= 0) glUniform1f(uRampaX_, rx);
    if (uRampaY_ >= 0) glUniform1f(uRampaY_, ry);
    if (std::getenv("ODYSSEY_UBO")) {
      GLint cur = 0, loc = -99; GLfloat back = -99.f;
      glGetIntegerv(GL_CURRENT_PROGRAM, &cur);
      loc = glGetUniformLocation(prog_, "uRampaX");
      glGetUniformfv(prog_, uRampaX_, &back);
      std::fprintf(stderr, "[ubo-gl] rx=%.6f locales %d %d prog=%u current=%d relink=%d readback=%.6f err=%d\n",
                   rx, (int)uRampaX_, (int)uRampaY_, (unsigned)prog_, (int)cur, (int)loc, back, (int)glGetError());
    }
  }
  glUniform1i(uWeave_, tune_.weave ? 1 : 0);
  glUniform1i(uSwap_, tune_.swapLR ? 1 : 0);
  glUniform1i(uHud_, hud_);
  glUniform1i(uLayout_, tune_.topBottom ? 1 : 0);
  glUniform1i(uFlipY_, srcFlipY_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, srcTex);
  glUniform1i(uSrc_, 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, corrA_);
  glUniform1i(uCorrA_, 1);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, corrB_);
  glUniform1i(uCorrB_, 2);
  glActiveTexture(GL_TEXTURE0);

  glBindVertexArray(vao_);
  glDisable(GL_DEPTH_TEST);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
}
