#version 450
/* Prueba: muestrear tal cual la imagen importada del compositor.
   Si esto sale bien, el DMA-BUF con modificador de NVIDIA se puede leer en Vulkan. */
layout(set = 0, binding = 0) uniform sampler2D srcImg;
layout(set = 0, binding = 1) uniform Info {
    vec2 res;
} u;

layout(location = 0) in vec2 fuv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 c = texture(srcImg, fuv);
    /* marca en la esquina superior izquierda: si se ve, el trazado Vulkan funciona.
       Su tamaño sale de la resolución, así que el bloque uniforme se usa de verdad. */
    vec2 m = max(u.res * 0.02, vec2(24.0));
    if (gl_FragCoord.x < m.x && gl_FragCoord.y < m.y)
        c.rgb = mix(c.rgb, vec3(1.0, 0.0, 1.0), 0.5);
    outColor = vec4(c.rgb, 1.0);
}
