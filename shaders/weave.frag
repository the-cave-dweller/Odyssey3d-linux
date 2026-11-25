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
   el desplazamiento relleno; lo hace src/hoyos.cpp. Con ODYSSEYVK_HOYOS=0 los dos canales traen el
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
    vec4 tun6;     /* medición: x temblor de la propia fase de la trama, en ciclos (0 = apagado) */
    vec4 tun7;     /* capas: x/y escala de lo de atras / lo de delante (0 o 1 = sin escalar) ·
                      z/w desplazamiento fijo de esa banda, en puntos de panel (0 = nada) */
    vec4 tun8;     /* agrandado guiado del mapa (v0.62): x activo · y parecido de color (RGB 0..1) ·
                      z ancho del peso por distancia en puntos del mapa · w compresion suave en los
                      topes (v0.64, 1 = puesta) */
    vec4 tun9;     /* paralaje de movimiento (v0.67): x cuanto se ha movido la cabeza, en medias
                      distancias entre ojos (0 = quieta; ya trae su ganancia y su tope de +-1) */
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

/* --- agrandado guiado del mapa (v0.62, el `jbu` del conversor de Windows) -----------------
   El mapa llega a 518 x 518 y se estira a 3840 x 2160 (7,4 x 4,2 px por punto). Con la cubica el
   canto de un objeto queda en escalera de ~4 filas y, en la franja de transicion, el desplazamiento
   cambia poco a poco y estira el color en horizontal: el peine de los bordes del microfono que vio
   Rodolfo (25-nov). Aqui cada pixel mira los 3 x 3 puntos del mapa que tiene alrededor y los pesa por
   distancia y por cuanto se parece el color que ESE punto va a mostrar (la imagen en su sitio menos su
   desplazamiento) al color que va a mostrar el pixel (con el desplazamiento grueso de la cubica). Asi
   el canto del mapa cae justo en el canto de la imagen. Simulado antes con el cuadro real 3840 x 2160 y
   su mapa (scratchpad sim_jbu.py): el borde del microfono deja de salir escalonado. */
/* --- compresion suave en los topes (v0.64, el dspr.compress de Windows) ------------------
   Leido del ensamblador NV de la cache del Hub (glfx/binary/dspr.compress.bin): fuera del centro,
   s' = c + (s - c) / (1 + |s - c| k), con k por lado. Aqui el centro es el plano de apoyo (s = 0,
   pendiente 1: el sujeto no se deforma) y k sale de pedir que el punto mas lejano posible de cada lado
   caiga justo en el tope. Antes era un clamp: con "3D Salida" en 50 aplastaba contra el tope el 23,8 %
   de la imagen de media (56 % en el peor de 22 cuadros reales), y todo eso quedaba en UN plano: se
   perdia la diferencia entre fondo y mas fondo, o entre frente y mas frente. Comprimida, cada
   profundidad conserva su sitio. `smaxF`/`smaxB` = desplazamiento mas grande posible delante/detras. */
float compSuave(float s, float arriba, float abajo, float smaxF, float smaxB) {
    if (P.tun8.w != 1.0)
        return s;
    if (s > 0.0) {
        float k = smaxF > arriba ? (1.0 / arriba - 1.0 / smaxF) : 0.0;
        return s / (1.0 + s * k);
    }
    float k = smaxB > abajo ? (1.0 / abajo - 1.0 / smaxB) : 0.0;
    return s / (1.0 - s * k);
}
float desplazGuia(float d, float gF, float gN) {
    float s = (d - P.tun4.z) * P.tun4.y * (d < P.tun4.z ? gF : gN);
    float arriba = P.tun5.y > 0.0 ? P.tun5.y / P.res.x : 1.0;
    float abajo = P.tun5.z > 0.0 ? P.tun5.z / P.res.x : 1.0;
    float smaxF = max(0.0, 1.0 - P.tun4.z) * P.tun4.y * gN;
    float smaxB = max(0.0, P.tun4.z) * P.tun4.y * gF;
    return clamp(compSuave(s, arriba, abajo, smaxF, smaxB), -abajo, arriba);
}
/* Paralaje de movimiento (v0.67). Cada ojo es una camara en la posicion p (izquierdo -1, derecho +1, en
   medias distancias entre ojos) y un punto con desplazamiento s se lee en q + p s. Si la cabeza se mueve
   h, las dos camaras se mueven con ella: p = h -+ 1. Lo de delante de la pantalla se corre al reves que la
   cabeza y lo de detras con ella, como al asomarse a una ventana. La separacion ENTRE ojos sigue siendo
   2 s: no gasta presupuesto de la lente ni anade diafonia; lo que crece es el estirado en los cantos. */
float vistaIzq() { return -1.0 + P.tun9.x; }
float vistaDer() { return 1.0 + P.tun9.x; }
vec3 colorVisto(vec2 uvPantalla, float s, bool izq) {
    vec2 q = uvPantalla;
    if (P.tun3.z == 1.0)
        q.y = 1.0 - q.y;
    return texture(uSrc, q + vec2((izq ? vistaIzq() : vistaDer()) * s + P.tun1.w, 0.0)).rgb;
}
vec2 mapaGuiado(vec2 uv, vec2 d0, float gF, float gN) {
    ivec2 tam = textureSize(uDepth, 0);
    vec2 p = uv * vec2(tam) - 0.5;
    ivec2 c = ivec2(floor(p + 0.5));
    vec3 gI = colorVisto(uv, desplazGuia(d0.x, gF, gN), true);
    vec3 gD = colorVisto(uv, desplazGuia(d0.y, gF, gN), false);
    float kr = 1.0 / (2.0 * P.tun8.y * P.tun8.y);
    float ke = 1.0 / (2.0 * P.tun8.z * P.tun8.z);
    vec2 num = vec2(0.0), den = vec2(0.0);
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            ivec2 q = clamp(c + ivec2(dx, dy), ivec2(0), tam - 1);
            vec2 dq = texelFetch(uDepth, q, 0).rg;
            vec2 uq = (vec2(q) + 0.5) / vec2(tam);
            vec2 e = vec2(q) - p;
            float esp = exp(-dot(e, e) * ke);
            vec3 aI = gI - colorVisto(uq, desplazGuia(dq.x, gF, gN), true);
            vec3 aD = gD - colorVisto(uq, desplazGuia(dq.y, gF, gN), false);
            /* el 1e-4 es para que un pixel cuyo color no se parece a ningun vecino (ruido, un brillo)
               no divida por cero: cae al peso por distancia, o sea a un bilineal suave */
            float wI = esp * (exp(-dot(aI, aI) * kr) + 1e-4);
            float wD = esp * (exp(-dot(aD, aD) * kr) + 1e-4);
            num += vec2(wI * dq.x, wD * dq.y);
            den += vec2(wI, wD);
        }
    }
    return num / den;
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

    /* Sonda de taller (ODYSSEYVK_PRUEBA, hueco tun6.w; 0 = normal, no cambia nada). Sirve para ver
       en que punto del calculo una imagen con contenido se vuelve una lama de un solo color:
       1 muestra del ojo izquierdo · 2 muestra del derecho · 3 la misma textura con la coordenada
       del pase 2D · 4 peso del filtro · 6 fase base · 7 campo v2 y base · 8 rampa v4 · 9 corrA,
       corrB y rsq. Cada sonda se mira con el volcado (--dump), sin tocar el panel. */
    if (P.tun6.w == 6.0) { o = vec4(fract(base), 0.0, 0.0, 1.0); return; }
    if (P.tun6.w == 7.0) { o = vec4(fract(v2.x), fract(v2.y), fract(base), 1.0); return; }
    if (P.tun6.w == 8.0) { o = vec4(fract(v4), 1.0); return; }
    if (P.tun6.w == 9.0) { o = vec4(corrA, corrB, rsq, 1.0); return; }
    if (P.tun6.w == 3.0) { o = vec4(texture(uSrc, vec2(u, v)).rgb, 1.0); return; }

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
           tun5.w (ODYSSEYVK_CANTO); 0 significa el de fabrica. Ojo con la pareja: el relleno de huecos
           (src/hoyos.cpp) pone el mapa mas filo en el canto, y un mapa con mas filo hace que esta
           poda corte mas. CantoSuave bajo = canto con mas relieve; alto = canto mas quieto. */
        const float CantoSuave = P.tun5.w > 0.0 ? P.tun5.w : 0.004;
        float cantoI = clamp(CantoSuave / max(pendienteI, 0.00001), 0.10, 1.0);
        float cantoD = clamp(CantoSuave / max(pendienteD, 0.00001), 0.10, 1.0);
        /* `relI`/`relD`: cuanto del desplazamiento se aplica. Con la cubica es el recorte de canto;
           con el agrandado guiado el canto ya cae en su sitio y no hay franja borrosa que podar, asi
           que va entero. `cantoI`/`cantoD` siguen mandando sobre el temblor (solo en lo llano). */
        float relI = cantoI, relD = cantoD;
        /* Capas por profundidad: lo de detras del plano de apoyo y lo de delante se pueden
           estirar por separado (tun7.x y tun7.y; 0 en el hueco = sin escalar, que es como venia).
           El lado se mira sobre el mapa ya muestreado hacia cada ojo, asi que un objeto que cruza
           el plano se estira distinto en cada ojo: es el mismo criterio de las bandas near/far del
           conversor de Windows (near_diff_scale / far_diff_scale), no una clasificacion de objetos.
           En el plano mismo el desplazamiento es 0 por los dos lados, asi que el cambio es continuo:
           lo unico que se rompe es la pendiente, que es justo lo que se quiere controlar. */
        const float gFondo = P.tun7.x > 0.0 ? P.tun7.x : 1.0;
        const float gFrente = P.tun7.y > 0.0 ? P.tun7.y : 1.0;
        if (P.tun8.x == 1.0) {
            vec2 g = mapaGuiado(fuv, vec2(mp.x, mp.z), gFondo, gFrente);
            mp.x = g.x;
            mp.z = g.y;
            relI = 1.0;
            relD = 1.0;
        }
        float sI = (mp.x - P.tun4.z) * P.tun4.y * relI * (mp.x < P.tun4.z ? gFondo : gFrente);
        float sD = (mp.z - P.tun4.z) * P.tun4.y * relD * (mp.z < P.tun4.z ? gFondo : gFrente);
        /* Empuje por capa (tun7.z para lo de atras, tun7.w para lo de delante), en puntos de
           panel: un desplazamiento FIJO que se suma al relieve, para llevar una banda entera a un
           plano concreto. Es el far_diff_popup de scene_dspr y el depth_offset de la capa overlay
           de degraph en la config de Windows. El lado se decide sobre el desplazamiento ya
           calculado y se interpola en una zona suave de 1,5 puntos alrededor del plano de apoyo en
           vez de conmutar de golpe, para que un objeto que cruza el plano no se rompa: lo que
           cambia es el plano al que va cada capa, no la continuidad. Va ANTES de los topes por
           ojo, que siguen mandando. */
        if (P.tun7.z != 0.0 || P.tun7.w != 0.0) {
            const float Blando = 1.5;
            float bandaI = 0.5 + 0.5 * smoothstep(-Blando, Blando, sI * P.res.x);
            float bandaD = 0.5 + 0.5 * smoothstep(-Blando, Blando, sD * P.res.x);
            sI += mix(P.tun7.z, P.tun7.w, bandaI) / P.res.x;
            sD += mix(P.tun7.z, P.tun7.w, bandaD) / P.res.x;
        }
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
            /* primero la compresion suave (si esta puesta), y el clamp queda solo de red de seguridad
               para lo que sume el empuje fijo por capa o el temblor */
            float smaxF = max(0.0, 1.0 - P.tun4.z) * P.tun4.y * gFrente;
            float smaxB = max(0.0, P.tun4.z) * P.tun4.y * gFondo;
            sI = clamp(compSuave(sI, arriba, abajo, smaxF, smaxB), -abajo, arriba);
            sD = clamp(compSuave(sD, arriba, abajo, smaxF, smaxB), -abajo, arriba);
        }
        vec2 q4 = fuv;
        if (P.tun3.z == 1.0)
            q4.y = 1.0 - q4.y;                  /* la fuente puede venir con la primera fila abajo */
        /* lo que esta por delante del plano de referencia va hacia fuera en la pantalla: en la
           vista izquierda el contenido se corre a la derecha y en la derecha a la izquierda (se
           muestrea al reves de hacia donde se mueve) */
        L = texture(uSrc, q4 + vec2(vistaIzq() * sI + cnv, 0.0)).rgb;
        R = texture(uSrc, q4 + vec2(vistaDer() * sD + cnv, 0.0)).rgb;
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
    /* Patron de un solo ojo (tecla `b`, v0.59; 12 = blanco a la vista izquierda, 13 = a la
       derecha). Se pisan las dos vistas DESPUES de elegirlas y ANTES del contraste, el crosstalk y
       el filtro, asi que la trama que sale es la de siempre con el contenido mas duro posible: un
       ojo recibe todo blanco y el otro todo negro. Mirando con un solo ojo, cualquier error del
       paso escrito contra la lente fisica se ve como bandas diagonales blanco/negro; cuando el paso
       cuadra, la pantalla entera sale de un solo tono. `F` (invertir) tambien da la vuelta aqui. */
    if (P.tun6.w == 12.0 || P.tun6.w == 13.0) {
        bool blancoIzq = (P.tun6.w == 12.0) != (P.tun2.w == 1.0);
        L = vec3(blancoIzq ? 1.0 : 0.0);
        R = vec3(blancoIzq ? 0.0 : 1.0);
    }
    if (P.tun6.w == 1.0) { o = vec4(L, 1.0); return; }
    if (P.tun6.w == 2.0) { o = vec4(R, 1.0); return; }
    L = (L - 0.5) * P.tun1.y + 0.5;
    R = (R - 0.5) * P.tun1.y + 0.5;
    vec3 dRL = R - L;
    float xt = ((1.0 + dot(v3, v3)) * v2.y * v2.y) * P.tun1.x + P.res.w;
    /* Seguro: el divisor se anula si xt llega a 1 y eso suelta un infinito que se propaga a los
       dos ojos (cuadro plano, sin imagen). Con los numeros de fabrica xt ronda 0,013 y las teclas
       de crosstalk lo mueven de 0,002 en 0,002, asi que este tope no se roza nunca. */
    xt = clamp(xt, -4.0, 0.90);
    xt = xt / (1.0 - xt);
    xt = clamp(xt, -12.0, 12.0);
    vec3 Lp = L - xt * dRL;
    vec3 Rp = R + xt * dRL;
    /* sondas de taller: 5 = el factor de crosstalk (con +1 para poder ver el signo) ·
       10 = la vista izquierda ya corregida · 11 = la derecha corregida */
    if (P.tun6.w == 5.0) { o = vec4(clamp(xt * 0.5 + 0.5, 0.0, 1.0), clamp(xt, 0.0, 1.0), clamp(-xt, 0.0, 1.0), 1.0); return; }
    if (P.tun6.w == 10.0) { o = vec4(Lp, 1.0); return; }
    if (P.tun6.w == 11.0) { o = vec4(Rp, 1.0); return; }

    vec3 outc;
    /* Temblor de la propia fase (ODYSSEYVK_FASE_TEMBLOR, en ciclos; 0 = como antes).
       Con la pendiente de filtro de fabrica (10) el paso entre las dos vistas es casi un
       escalon, y sus armonicas caen por aliasing a 4,34 · 8,17 · 10,73 px de panel, o sea
       0,75 · 1,4 · 1,9 mm: unas franjas verticales finisimas por toda la pantalla, que se
       ven aunque no haya releve ninguno. Moviendo la fase de cada punto +-0,1 de ciclo esas
       armonicas bajan unas cinco veces y la fundamental (la que hace el 3D) casi no se toca.
       El numero es el mismo para los tres canales, para que el temblor no ensucie el color. */
    float jr = 0.0;
    if (P.tun6.x != 0.0)
        jr = (fract(sin(dot(floor(px), vec2(39.3467, 11.1355))) * 24634.6345) - 0.5) * P.tun6.x;
    /* Rampa fina aparte (ODYSSEYVK_RAMPA_X / _Y, ciclos de porteadora por pixel de panel).
       Apretar o aflojar el paso de la rejilla escrita sin tocar lo demas. La rampa de la lente
       (v4) lleva metidos tambien los desfases entre los canales rojo, verde y azul, y escalarla
       entera con "paso" corre esos desfases y afloja un poco la inclinacion. Midiendo el cuadro
       escrito con FFT (tools/paso_rejilla.py, cuadro solido 3840x2160):
         rampa 0          -> kx 0,4473  ky -0,1598  periodo 2,2358 px = 0,34744 mm
         rampa -0,00277   -> kx 0,4500  ky -0,1598  periodo 2,2224 px = 0,34536 mm
         paso 0,995       -> kx 0,4500  ky -0,1589  periodo 2,2224 px = 0,34536 mm
       Los dos aciertan el paso (el de Windows medido en foto es 2,2217 px = 0,34530 mm) pero la
       rampa deja la inclinacion exacta y el paso la afloja 0,0009. La respuesta es 1:1: cada
       0,001 de rampa mueve 0,001 el kx medido.
       El numero va con el signo contrario al ajuste que se busca en el periodo medido: la
       porteadora que sale por la pantalla es el espejo de la escrita, asi que para ACORTAR el
       periodo (subir kx) hay que RESTAR rampa en x. px.y es hacia abajo en Vulkan; la copia GL
       del instrumento de laboratorio lo compensa para que el mismo numero signifique lo mismo. */
    float rampa = (P.tun6.y != 0.0 || P.tun6.z != 0.0)
                ? fract(P.tun6.y * px.x + P.tun6.z * px.y) : 0.0;
    for (int c = 0; c < 3; c++) {
        float phase = fract(base + v4[c] * P.tun2.y + P.tun2.x - 0.25 + jr + rampa);
        float t = 2.0 * phase - 1.0;
        float w = clamp(-abs(t) * P.res.z + P.res.z * 0.5 + 0.5, 0.0, 1.0);
        if (P.tun6.w == 4.0) { outc[c] = w; continue; }
        outc[c] = mix(Rp[c], Lp[c], w);
    }
    o = vec4(outc, 1.0);
}
