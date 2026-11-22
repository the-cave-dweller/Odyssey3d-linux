#include "testsrc.h"

static const char *VERT = R"GLSL(#version 330
in vec2 pos;
void main(){ gl_Position = vec4(pos, 0.0, 1.0); }
)GLSL";

static const char *FRAG = R"GLSL(#version 330
uniform vec2 uRes; uniform float uT; uniform ivec2 uOff;
out vec4 o;
vec3 hsv(float h){
    vec3 c = clamp(abs(mod(h*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0, 1.0);
    return vec3(0.25)+0.75*c;
}
void main(){
    float hw = uRes.x*0.5;
    bool leftHalf = gl_FragCoord.x < hw;
    vec2 p = vec2(leftHalf ? gl_FragCoord.x : gl_FragCoord.x - hw, gl_FragCoord.y);
    float s = leftHalf ? 1.0 : -1.0;          // el otro ojo, desplazado en sentido contrario
    p += vec2(s*float(uOff.x), float(uOff.y));

    vec3 col = vec3(0.05,0.05,0.06);
    // rejilla de referencia en el plano de la pantalla
    vec2 g = mod(p, vec2(96.0));
    if(g.x < 2.0 || g.y < 2.0) col = vec3(0.20,0.20,0.22);

    // escalera de profundidad: 9 cuadros, cada uno con otra disparidad (uno oscila)
    for(int i=0;i<9;i++){
        float fi = float(i);
        float disp = (fi-4.0)*14.0 + (i==4 ? 12.0*sin(uT*0.9) : 0.0);
        vec2 c = vec2(hw*(fi+0.5)/9.0, uRes.y*0.55);
        c.x += s*disp;
        if(abs(p.x-c.x)<66.0 && abs(p.y-c.y)<66.0) col = hsv(fi/9.0);
    }

    // crucero en el plano de la pantalla (se fusiona justo en el cristal)
    vec2 cc = vec2(hw*0.5, uRes.y*0.86);
    if(abs(p.x-cc.x)<3.0 && abs(p.y-cc.y)<40.0) col = vec3(1.0);
    if(abs(p.y-cc.y)<3.0 && abs(p.x-cc.x)<40.0) col = vec3(1.0);

    // dos marcas verticales con disparidad fuerte (una por ojo): deben fusionarse por delante
    float near = 34.0;
    if(abs(p.x-(hw*0.5 - s*near))<3.0 && abs(p.y-uRes.y*0.16)<50.0) col = vec3(1.0,1.0,1.0);

    // barras R/G/B sin disparidad: sirven para ver el alineado por canal de la trama
    float by = 70.0, bh = 44.0;
    if(p.y>by && p.y<by+bh){
        float t = (p.x - hw*0.30)/(hw*0.40);
        if(t>0.0 && t<1.0){
            if(t<0.333) col = vec3(1.0,0.0,0.0);
            else if(t<0.666) col = vec3(0.0,1.0,0.0);
            else col = vec3(0.0,0.0,1.0);
        }
    }
    o = vec4(col,1.0);
}
)GLSL";

bool TestSrc::init(std::string *err) {
  prog_ = glLinkProgram(VERT, FRAG, err);
  if (!prog_) return false;
  uRes_ = glGetUniformLocation(prog_, "uRes");
  uT_ = glGetUniformLocation(prog_, "uT");
  uOff_ = glGetUniformLocation(prog_, "uOff");
  static const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
  glGenVertexArrays(1, &vao_);
  GLuint vbo = 0;
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);
  glDeleteBuffers(1, &vbo);
  glCheckErr("TestSrc::init");
  return true;
}

void TestSrc::destroy() {
  if (prog_) {
    glDeleteProgram(prog_);
    prog_ = 0;
  }
  if (vao_) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  fbo_.destroy();
}

void TestSrc::createFbo(int w, int h) { fbo_.create(w, h); }

GLuint TestSrc::render(double t, int xoffPx, int yoffPx) {
  if (!fbo_.fbo) return 0;
  fbo_.bind();
  glDisable(GL_DEPTH_TEST);
  glUseProgram(prog_);
  glUniform2f(uRes_, (float)fbo_.w, (float)fbo_.h);
  glUniform1f(uT_, (float)t);
  glUniform2i(uOff_, xoffPx, yoffPx);
  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
  fbo_.unbind();
  return fbo_.tex;
}
