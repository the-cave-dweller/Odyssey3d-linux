#version 450
/* Cuads de la interfaz (la pastilla del hub, los cursores, el texto).
   inPos va en píxeles con (0,0) arriba a la izquierda; en Vulkan la Y del marco
   también crece hacia abajo, así que no hay que invertir nada. */
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inCol;

layout(push_constant) uniform Push {
    vec2 res;
} push;

layout(location = 0) out vec2 fuv;
layout(location = 1) out vec4 fcol;

void main()
{
    fuv = inUv;
    fcol = inCol;
    gl_Position = vec4(inPos.x / push.res.x * 2.0 - 1.0,
                       inPos.y / push.res.y * 2.0 - 1.0,
                       0.0, 1.0);
}
