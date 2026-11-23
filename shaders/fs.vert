#version 450
/* Triangulo que cubre toda la pantalla, sin vertices en memoria.
   fuv: (0,0) arriba a la izquierda, (1,1) abajo a la derecha (en Vulkan la y de la
   pantalla crece hacia abajo, asi que no hay que invertir nada). */
layout(location = 0) out vec2 fuv;

void main()
{
    vec2 p = vec2((gl_VertexIndex == 1) ? 3.0 : -1.0,
                  (gl_VertexIndex == 2) ? 3.0 : -1.0);
    fuv = (p + 1.0) * 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
