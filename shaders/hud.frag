#version 450
/* La máscara del atlas (R8) decide cuánta tinta va en cada píxel del cuadro. */
layout(set = 0, binding = 0) uniform sampler2D atlas;

layout(location = 0) in vec2 fuv;
layout(location = 1) in vec4 fcol;

layout(location = 0) out vec4 o;

void main()
{
    float m = texture(atlas, fuv).r;
    o = vec4(fcol.rgb, fcol.a * m);
}
