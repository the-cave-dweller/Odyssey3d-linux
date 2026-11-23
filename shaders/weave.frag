#version 450
/* Tramado (weave) del Odyssey 3D, port del pixel shader del hub (validado en GL).
   Se pasa en un bloque uniforme los valores del campo de la lente en las cuatro esquinas
   (TL, TR, BL, BR) y se reconstruye el campo por píxel: el campo es afín, así que la
   interpolación bilineal de las esquinas es exacta.

   base  = v2.x*inversesqrt(1+|v3|^2) - 2*v2.y*(corrB-0.5) + corrA
   fase  = fract(base + v4[c]*pitch + align - 0.25)
   peso  = filtro triangular entre la vista derecha y la izquierda                        */
layout(set = 0, binding = 0) uniform sampler2D uSrc;    /* estéreo: lado a lado o arriba/abajo */
layout(set = 0, binding = 1) uniform sampler2D uCorrA;  /* mapa de corrección A (R8) */
layout(set = 0, binding = 2) uniform sampler2D uCorrB;  /* mapa de corrección B (R8) */

layout(set = 0, binding = 3) uniform WeaveParams {
    vec2 v2c[4];   /* campo v2 en TL, TR, BL, BR */
    vec2 v3c[4];   /* campo v3 en TL, TR, BL, BR */
    vec3 v4c[4];   /* rampa de fase por canal en TL, TR, BL, BR (entero restado) */
    vec4 res;      /* x, y de la ventana · z pendiente del filtro · w crosstalk estático */
    vec4 tun1;     /* x crosstalk dinámico · y contraste · z escala de corrA · w convergencia */
    vec4 tun2;     /* x desfase de fase · y paso de lente · z tramado activo · w invertir ojos */
    vec4 tun3;     /* x estado del indicador · y disposición (0 lado a lado, 1 arriba/abajo) · z invertir Y · w profundidad */
} P;

layout(location = 0) in vec2 fuv;
layout(location = 0) out vec4 o;

/* Una mitad del estéreo. fuv.y=0 es arriba de la pantalla. */
vec3 srcHalf(vec2 uv, float which)
{
    vec2 s = (P.tun3.y == 0.0) ? vec2(which + uv.x * 0.5, uv.y)     /* lado a lado: 0=izq, 0.5=der */
                               : vec2(uv.x, which + uv.y * 0.5);     /* arriba/abajo: 0=abajo, 0.5=arriba */
    if (P.tun3.z == 1.0)
        s.y = 1.0 - s.y;
    return texture(uSrc, s).rgb;
}

void main()
{
    vec2 px = gl_FragCoord.xy;                 /* en Vulkan (0,0) también está arriba a la izquierda */
    float u = px.x / P.res.x;
    float v = px.y / P.res.y;

    /* interpolación bilineal de las cuatro esquinas (exacta: el campo es afín) */
    vec2 w00 = vec2((1.0 - u) * (1.0 - v));
    vec2 w10 = vec2(u * (1.0 - v));
    vec2 w01 = vec2((1.0 - u) * v);
    vec2 w11 = vec2(u * v);
    vec2 v2 = P.v2c[0] * w00.x + P.v2c[1] * w10.x + P.v2c[2] * w01.x + P.v2c[3] * w11.x;
    vec2 v3 = P.v3c[0] * w00.x + P.v3c[1] * w10.x + P.v3c[2] * w01.x + P.v3c[3] * w11.x;
    vec3 v4 = P.v4c[0] * w00.y + P.v4c[1] * w10.y + P.v4c[2] * w01.y + P.v4c[3] * w11.y;

    /* pase 2D: sin tramar (la lente y el scaler pueden seguir activos) */
    if (P.tun2.z == 0.0) {
        o = vec4(srcHalf(fuv, 0.0), 1.0);
        return;
    }

    /* indicador de estado del hardware, esquina superior izquierda */
    if (px.x < 36.0 && px.y < 36.0) {
        o = (P.tun3.x == 3.0) ? vec4(0.0, 1.0, 0.0, 1.0)
                              : (P.tun3.x == 0.0 ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(1.0, 0.6, 0.0, 1.0));
        return;
    }

    vec2 suv = vec2(u, v);
    float corrA = texture(uCorrA, suv).r * P.tun1.z;
    float corrB = texture(uCorrB, suv).r;
    float rsq = inversesqrt(1.0 + dot(v3, v3));
    float base = v2.x * rsq - 2.0 * v2.y * (corrB - 0.5) + corrA;

    float lx = (P.tun2.w == 1.0) ? 0.0 : 0.5;   /* orden de ojos validado en el Hub */
    float rx = (P.tun2.w == 1.0) ? 0.5 : 0.0;
    /* "3D Depth" separa las dos vistas en sentidos opuestos (más paralaje);
       "3D Popout" corre el par entero (mueve el plano de convergencia). */
    float dep = P.tun3.w;
    float cnv = P.tun1.w;
    vec3 L = srcHalf(fuv + vec2(cnv + dep, 0.0), lx);
    vec3 R = srcHalf(fuv + vec2(cnv - dep, 0.0), rx);
    L = (L - 0.5) * P.tun1.y + 0.5;
    R = (R - 0.5) * P.tun1.y + 0.5;
    vec3 dRL = R - L;
    float xt = ((1.0 + dot(v3, v3)) * v2.y * v2.y) * P.tun1.x + P.res.w;
    xt = xt / (1.0 - xt);
    vec3 Lp = L - xt * dRL;
    vec3 Rp = R + xt * dRL;

    vec3 outc;
    for (int c = 0; c < 3; c++) {
        float phase = fract(base + v4[c] * P.tun2.y + P.tun2.x - 0.25);
        float t = 2.0 * phase - 1.0;
        float w = clamp(-abs(t) * P.res.z + P.res.z * 0.5 + 0.5, 0.0, 1.0);
        outc[c] = mix(Rp[c], Lp[c], w);
    }
    o = vec4(outc, 1.0);
}
