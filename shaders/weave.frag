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
layout(set = 0, binding = 4) uniform sampler2D uDepth;  /* profundidad (R8G8: .r ojo izq, .g ojo der, 255 = cerca).
   Cada canal es el mismo mapa puesto ya en el sitio de destino de ese ojo y con el hueco que deja
   el desplazamiento relleno; lo hace src/hoyos.cpp. Con ODYSSEY3D_HOYOS=0 los dos canales traen el
   mapa en crudo y el resultado es el de la v0.35. */

layout(set = 0, binding = 3) uniform WeaveParams {
    vec2 v2c[4];   /* campo v2 en TL, TR, BL, BR */
    vec2 v3c[4];   /* campo v3 en TL, TR, BL, BR */
    vec3 v4c[4];   /* rampa de fase por canal en TL, TR, BL, BR (entero restado) */
    vec4 res;      /* x, y de la ventana · z pendiente del filtro · w crosstalk estático */
    vec4 tun1;     /* x crosstalk dinámico · y contraste · z escala de corrA · w convergencia */
    vec4 tun2;     /* x desfase de fase · y paso de lente · z tramado activo · w invertir ojos */
    vec4 tun3;     /* x estado del indicador · y disposición (0 lado a lado, 1 arriba/abajo) · z invertir Y · w profundidad */
    vec4 tun4;     /* conversor 2D/3D: x activo · y cuanto se desplaza cada ojo · z plano de referencia · w copia pura */
    vec4 tun5;     /* conversor fino: x temblor del desplazamiento en pixeles · y/z topes por ojo en pixeles (0 = sin tope) */
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

/* --- mapa de profundidad: interpolacion cubica con pendiente de la propia base ----------
   El mapa llega de 518 x 518 y la pantalla es de 3840, o sea un punto del mapa cubre casi 8
   pixeles. Con el filtro lineal de siempre el relieve sale quebrado -- los puntos del mapa se
   notan como peldanos -- y la pendiente, medida a un punto de distancia, da saltos de golpe: por
   ahi se recortaba el canto y se perdian los bordes de los objetos. Con la base cubica de
   Catmull-Rom (que pasa por los puntos) la superficie sale continua, y la pendiente sale de la
   derivada de esa misma base: las 16 lecturas del mapa se hacen una sola vez. */
vec4 crBase(float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5 * vec4(-t3 + 2.0 * t2 - t, 3.0 * t3 - 5.0 * t2 + 2.0, -3.0 * t3 + 4.0 * t2 + t, t3 - t2);
}
vec4 crDeriv(float t) {
    float t2 = t * t;
    return 0.5 * vec4(-3.0 * t2 + 4.0 * t - 1.0, 9.0 * t2 - 10.0 * t, -9.0 * t2 + 8.0 * t + 1.0, 3.0 * t2 - 2.0 * t);
}
/* devuelve en .x/.y el valor del ojo izquierdo y cuanto cambia por punto de mapa en horizontal,
   y en .z/.w lo mismo para el ojo derecho. Los dos canales vienen en la misma textura, asi que las
   16 lecturas del mapa no se duplican: cada lectura trae los dos valores. */
vec4 mapaCubico(vec2 uv) {
    ivec2 tam = textureSize(uDepth, 0);
    vec2 p = uv * vec2(tam) - 0.5;
    ivec2 i = ivec2(floor(p));
    vec2 f = p - vec2(i);
    vec4 ax = crBase(f.x), ex = crDeriv(f.x), ay = crBase(f.y);
    vec4 v = vec4(0.0);
    for (int r = 0; r < 4; r++) {
        int y = clamp(i.y + r - 1, 0, tam.y - 1);
        for (int c = 0; c < 4; c++) {
            int x = clamp(i.x + c - 1, 0, tam.x - 1);
            vec2 d = texelFetch(uDepth, ivec2(x, y), 0).rg;
            v.x += ay[r] * ax[c] * d.x;
            v.y += ay[r] * ex[c] * d.x;
            v.z += ay[r] * ax[c] * d.y;
            v.w += ay[r] * ex[c] * d.y;
        }
    }
    return v;
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

    /* pase de copia pura (lo usa el conversor para alimentarse a si mismo): ni campo de la lente
       ni trama, la imagen tal cual. Con el campo de la lente los desplazamientos subpixel del
       tramado se convierten en franjas de colores y la red ve una imagen falsa. */
    if (P.tun4.w == 1.0) {
        vec2 qc = vec2(u, v);
        if (P.tun3.z == 1.0)
            qc.y = 1.0 - qc.y;          /* la fuente puede venir con la primera fila abajo */
        o = vec4(texture(uSrc, qc).rgb, 1.0);
        return;
    }

    /* pase 2D: sin tramar (la lente y el scaler pueden seguir activos). Aqui se muestrea la
       imagen ENTERA: si se usara srcHalf se veria solo media pantalla ampliada al doble, que es
       exactamente el "se ve enorme y cortado" con un video que no tiene dos vistas. */
    if (P.tun2.z == 0.0) {
        vec2 q2 = fuv;
        if (P.tun3.z == 1.0)
            q2.y = 1.0 - q2.y;          /* la fuente puede venir con la primera fila abajo */
        o = vec4(texture(uSrc, q2).rgb, 1.0);
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
    vec3 L, R;
    if (P.tun4.x == 1.0) {
        /* Video normal (una sola imagen): el mapa de profundidad dice cuanto hay que desplazar
           cada ojo. Lo que esta en el plano de referencia (tun4.z) no se mueve; lo que esta por
           delante sale hacia fuera de la pantalla y el fondo hacia dentro. */
        float pendienteI, pendienteD;
        vec4 mp = mapaCubico(fuv);
        pendienteI = abs(mp.y);
        pendienteD = abs(mp.w);
        /* En los cantos del mapa sale borroso: alli desplazar de mas estira el color en peine.
           Donde la profundidad cambia rapido en horizontal se recorta el desplazamiento, asi que
           el canto se queda en su sitio en vez de babearse. CantoSuave es cuanto cambio de mapa por
           punto hace falta para no podar nada: por debajo de ahi el recorte cae en proportion, y el
           tope de 0,10 es que un canto nunca conserva menos de una decima del relieve. Viene por
           tun5.w (ODYSSEY3D_CANTO); 0 significa el de fabrica. Ojo con la pareja: el relleno de huecos
           (src/hoyos.cpp) pone el mapa mas filo en el canto, y un mapa con mas filo hace que esta
           poda corte mas. CantoSuave bajo = canto con mas relieve; alto = canto mas quieto. */
        const float CantoSuave = P.tun5.w > 0.0 ? P.tun5.w : 0.004;
        float cantoI = clamp(CantoSuave / max(pendienteI, 0.00001), 0.10, 1.0);
        float cantoD = clamp(CantoSuave / max(pendienteD, 0.00001), 0.10, 1.0);
        float sI = (mp.x - P.tun4.z) * P.tun4.y * cantoI;
        float sD = (mp.z - P.tun4.z) * P.tun4.y * cantoD;
        /* El programa de Samsung mueve el desplazamiento de a saltos y en las zonas llanas (pared,
           cielo) se ven bandas: por eso le suma un temblor de 0,25 a 0,4 px al mapa. Aqui se suma
           igual, pero solo donde el mapa esta llano (canto = 1), que es justo donde la banda se ve
           y donde el temblor no se nota porque el color de fondo es parejo. */
        if (P.tun5.x != 0.0) {
            float r = fract(sin(dot(floor(px), vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
            sI += r * (P.tun5.x / P.res.x) * cantoI;
            sD += r * (P.tun5.x / P.res.x) * cantoD;
        }
        /* topes por ojo del programa de Samsung: -9,98 px para adentro y +12,67 px para afuera
           (0 en tun5.y/z = sin tope, que es como venia funcionando) */
        if (P.tun5.y > 0.0 || P.tun5.z > 0.0) {
            float arriba = P.tun5.y > 0.0 ? P.tun5.y / P.res.x : 1.0;
            float abajo = P.tun5.z > 0.0 ? P.tun5.z / P.res.x : 1.0;
            sI = clamp(sI, -abajo, arriba);
            sD = clamp(sD, -abajo, arriba);
        }
        vec2 q4 = fuv;
        if (P.tun3.z == 1.0)
            q4.y = 1.0 - q4.y;                  /* la fuente puede venir con la primera fila abajo */
        /* lo que esta por delante del plano de referencia va hacia fuera en la pantalla: en la
           vista izquierda el contenido se corre a la derecha y en la derecha a la izquierda (se
           muestrea al reves de hacia donde se mueve) */
        L = texture(uSrc, q4 + vec2(-sI + cnv, 0.0)).rgb;
        R = texture(uSrc, q4 + vec2(sD + cnv, 0.0)).rgb;
        if (P.tun2.w == 1.0) { vec3 t = L; L = R; R = t; }
    } else if (P.tun4.w == 2.0) {
        /* conversor puesto pero aun sin mapa de profundidad: los dos ojos ven lo mismo, o sea la
           imagen normal y plana. Es lo que se ve medio segundo al encender el conversor y si la red
           falla; si cayera a la rama de lado a lado se veria la pantalla partida en dos. */
        vec2 q3 = fuv;
        if (P.tun3.z == 1.0)
            q3.y = 1.0 - q3.y;
        L = texture(uSrc, q3 + vec2(cnv, 0.0)).rgb;
        R = L;
    } else {
        L = srcHalf(fuv + vec2(cnv + dep, 0.0), lx);
        R = srcHalf(fuv + vec2(cnv - dep, 0.0), rx);
    }
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
