#version 450
/* Triángulo que cubre toda la ventana, sin vértices en memoria (dynamic rendering).
   En Vulkan la Y del marco crece hacia abajo, así que fuv=(0,0) es la esquina
   superior izquierda: coincide con la convención de Wayland y con la nuestra. */
layout(location = 0) out vec2 fuv;

void main()
{
    vec2 p = vec2((gl_VertexIndex == 1) ? 3.0 : -1.0,
                  (gl_VertexIndex == 2) ? 3.0 : -1.0);
    fuv = (p + 1.0) * 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
