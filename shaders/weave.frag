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
   mapa en crudo y el resultado es el de la v0.35.
   Con los cantos finos (v0.89, tun10.y) es el mapa tal cual sale de la red, en .r, y si tun10.w = 1
   trae ademas en .gba el color de la imagen que lo dio (la copia pequena que vio la red). */
layout(set = 0, binding = 5) uniform sampler2D uFino;   /* cantos finos: la profundidad de la fuente ya
   afinada, a tamano de pantalla, que deja hecha el pase de afinado (tun4.w = 3) antes de la trama.
   Va en dos canales: .r los 8 bits altos y .g los 8 bajos (r + g/255); en .b, cuanto de objeto
   tiene el pixel segun su color (0 = fondo, 1 = objeto, solo junto a los cantos). Solo vale con
   tun10.z = 1. */
layout(set = 0, binding = 6) uniform sampler2D uSal;    /* la saliencia de la ultima estimacion (v1.12), una
   celda cada 8 puntos del mapa, 1 = el punto que mas pesa al elegir el plano. Solo la lee la vista de
   depuracion (tun11.y); sin ella es la imagen negra de reserva. */

layout(set = 0, binding = 3) uniform WeaveParams {
    vec2 v2c[4];   /* campo v2 en TL, TR, BL, BR */
    vec2 v3c[4];   /* campo v3 en TL, TR, BL, BR */
    vec3 v4c[4];   /* rampa de fase por canal en TL, TR, BL, BR (entero restado) */
    vec4 res;      /* x, y de la ventana · z pendiente del filtro · w crosstalk estático */
    vec4 tun1;     /* x crosstalk dinámico · y contraste · z escala de corrA · w convergencia */
    vec4 tun2;     /* x desfase de fase · y paso de lente · z tramado activo · w invertir ojos */
    vec4 tun3;     /* x filas de abajo en 2D (fraccion del alto, video dividido) · y disposición (0 lado a lado, 1 arriba/abajo) · z invertir Y · w profundidad */
    vec4 tun4;     /* conversor 2D/3D: x activo · y cuanto se desplaza cada ojo · z plano de referencia ·
                      w 1 = copia pura · 2 = conversor puesto pero aun sin mapa · 3 = pase de afinado (v0.89) */
    vec4 tun5;     /* conversor fino: x temblor del desplazamiento en pixeles · y/z topes por ojo en pixeles (0 = sin tope) */
    vec4 tun6;     /* medición: x temblor de la propia fase de la trama, en ciclos (0 = apagado) */
    vec4 tun7;     /* capas: x/y escala de lo de atras / lo de delante (0 o 1 = sin escalar) ·
                      z/w desplazamiento fijo de esa banda, en puntos de panel (0 = nada) */
    vec4 tun8;     /* agrandado guiado del mapa (v0.62): x activo · y parecido de color (RGB 0..1) ·
                      z ancho del peso por distancia en puntos del mapa · w compresion suave en los
                      topes (v0.64, 1 = puesta) */
    vec4 tun9;     /* paralaje de movimiento (v0.67): x cuanto se ha movido la cabeza, en medias
                      distancias entre ojos (0 = quieta; ya trae su ganancia y su tope de +-1) ·
                      realce del sujeto (v0.68): y profundidad del sujeto en el mapa (< 0 = usar el
                      plano de apoyo) · z fuerza (0 = apagado) · w filas de arriba en 2D (como tun3.x) */
    vec4 tun10;    /* video dividido suavizado (v0.83): x = 1 la fuente trae dos imagenes lado a lado y
                      el conversor trabaja solo con la izquierda, estirada al ancho entero ·
                      y = 1 cantos finos (v0.89): el mapa viene tal cual sale de la red (sin repartir
                      por ojo) y la vista de cada ojo se BUSCA punto a punto, ver buscaVista ·
                      z = 1 la profundidad afinada ya esta hecha en uFino (ver profFuente) ·
                      w = 1 el mapa trae en .gba el color de la imagen que lo dio */
    vec4 tun11;    /* x = 1: las filas de la barra del reproductor se ensenan enteras y tal cual, como
                      de la v0.81 a la v0.89 (`barra_2d = entera`), en vez de la franja nueva (v0.90) ·
                      y = 1: la vista de depuracion de la profundidad (v1.12, ver mas abajo) */
} P;

layout(location = 0) in vec2 fuv;
layout(location = 0) out vec4 o;

/* La imagen fuente tal como la ve el conversor (y el 2D plano). De normal es el cuadro entero. Con
   tun10.x = 1 el cuadro trae dos imagenes lado a lado (un video dividido) y aqui solo existe la
   izquierda, estirada al ancho entero: asi el conversor de video normal le pone SU relieve, que nunca
   pasa del presupuesto de este panel, a un video cuyo relieve propio se sale (v0.83). El tope de la
   derecha evita que el filtro lineal traiga la primera columna de la otra imagen. */
vec3 fuente(vec2 q)
{
    if (P.tun10.x == 1.0)
        q.x = min(clamp(q.x, 0.0, 1.0) * 0.5, 0.5 - 0.5 / float(textureSize(uSrc, 0).x));
    return texture(uSrc, q).rgb;
}

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
/* ---- cantos finos (v0.89) ---------------------------------------------------------------------
   Hasta la v0.88 el mapa se repartia por ojo en la CPU, a su tamano (518 puntos: uno cada 7,4
   pixeles de pantalla), y el sombreador leia de ahi la profundidad de cada pixel de cada ojo. El
   canto de un objeto cercano quedaba entonces colocado con la precision del MAPA, distinta en cada
   ojo. MEDIDO con una escena de canto conocido: junto al canto, una tira de 10..12 pixeles
   (1,6..1,9 mm) equivocada en cada ojo (el borde del objeto repetido en uno, comido en el otro), que
   con la cabeza ladeada llegaba a 30..36 pixeles (4,7..5,6 mm). Es el «fantasma de 1 a 1,5 mm junto
   a algunos planos al mover la cabeza» que vio Rodolfo.

   Ahora el mapa llega tal cual (es la profundidad de la imagen FUENTE) y para cada pixel de cada ojo
   se busca que punto de la fuente cae ahi, como quien mira un relieve desde un lado: se avanza
   desde lo mas cercano hacia el fondo hasta dar con la superficie, y gana lo que este delante. Asi
   el canto de cada ojo es el mismo canto de la imagen, corrido lo que le toca, y los dos ojos
   coinciden. Donde un objeto, al correrse, destapa fondo que la imagen no trae, se rellena con el
   fondo de al lado visto en espejo (no con el borde del objeto repetido ni estirando una raya). */

/* El desplazamiento completo que le toca a una profundidad `d` del mapa: el relieve, el empuje fijo
   por capa y los topes por ojo con su compresion suave. Es la «altura» del relieve que se recorre. */
float desplazTotal(float d, float gF, float gN) {
    float s = (d - P.tun4.z) * P.tun4.y * (d < P.tun4.z ? gF : gN);
    if (P.tun7.z != 0.0 || P.tun7.w != 0.0) {
        float banda = 0.5 + 0.5 * smoothstep(-1.5, 1.5, s * P.res.x);
        s += mix(P.tun7.z, P.tun7.w, banda) / P.res.x;
    }
    if (P.tun5.y > 0.0 || P.tun5.z > 0.0) {
        float arriba = P.tun5.y > 0.0 ? P.tun5.y / P.res.x : 1.0;
        float abajo = P.tun5.z > 0.0 ? P.tun5.z / P.res.x : 1.0;
        float smaxF = max(0.0, 1.0 - P.tun4.z) * P.tun4.y * gN;
        float smaxB = max(0.0, P.tun4.z) * P.tun4.y * gF;
        s = clamp(compSuave(s, arriba, abajo, smaxF, smaxB), -abajo, arriba);
    }
    return s;
}

/* El color de la fuente en `uv` (coordenadas de pantalla: se ocupa de la fila de arriba) y su media
   en un cuadro de unos `r` pixeles de pantalla alrededor (cuatro lecturas en aspa). La media quita
   de en medio el grano y la textura fina, que es lo que confunde al comparar colores punto a punto,
   y no mueve el canto: el punto medio de un canto borroso sigue cayendo donde estaba. */
vec3 colorFuente(vec2 uv) {
    vec2 q = uv;
    if (P.tun3.z == 1.0)
        q.y = 1.0 - q.y;
    return fuente(q);
}
vec3 colorMedio(vec2 uv, vec2 r) {
    vec2 t = r / P.res.xy;
    return 0.25 * (colorFuente(uv + vec2(-t.x, -t.y)) + colorFuente(uv + vec2(t.x, -t.y)) +
                   colorFuente(uv + vec2(-t.x, t.y)) + colorFuente(uv + vec2(t.x, t.y)));
}

/* La profundidad de la fuente en `uv`, con el canto en su sitio.

   El mapa llega a 518 x 518: un punto cada 7,4 x 4,2 pixeles, y en el canto de un objeto la red (y el
   filtro que lo limpia) dejan una rampa de dos o tres puntos, o sea 15 a 22 pixeles de transicion
   centrados MAS O MENOS en el canto de la imagen (MEDIDO en 8 cuadros reales, 741 cantos, sin el
   dilatado: mediana a 1,9 pixeles del canto del color, el 80 % entre -0,9 y +6,1). Desplazada tal
   cual, esa rampa estira el borde del objeto y el fondo de al lado como una goma, y lo que queda
   mal puesto se ve doble al comparar los dos ojos.

   Aqui, donde el mapa da un salto (mas de ~2 pixeles de desplazamiento entre dos puntos vecinos), la
   rampa se cambia por un escalon: cada pixel se queda o con el fondo o con el objeto, y lo decide
   SU COLOR. Se miran los 4 x 4 puntos del mapa de alrededor; cada uno vota por su lado (los de en
   medio de la rampa casi no votan: no son ni una cosa ni otra) con mas fuerza cuanto mas cerca esta
   y cuanto mas se parece el color de la imagen ahi al del pixel. Asi el escalon cae en el canto de
   la IMAGEN, pixel a pixel, y a cada lado queda el valor llano de ese lado. Donde el color no
   distingue (fondo y objeto del mismo tono) decide la mitad de la rampa, que ahi da igual. En lo
   llano y en las pendientes suaves no cambia nada: sale la interpolacion de siempre. */
vec2 afinar(vec2 uv, float gF, float gN) {
    vec2 tam = vec2(textureSize(uDepth, 0));
    vec2 p = uv * tam - 0.5;
    vec2 base = floor(p);
    vec2 f = p - base;
    /* 16 puntos con 4 lecturas: cada una trae un cuadro de 2 x 2 (w = arriba izq, z = arriba der,
       x = abajo izq, y = abajo der) */
    vec4 g00 = textureGather(uDepth, (base + vec2(0.0, 0.0)) / tam, 0);
    vec4 g10 = textureGather(uDepth, (base + vec2(2.0, 0.0)) / tam, 0);
    vec4 g01 = textureGather(uDepth, (base + vec2(0.0, 2.0)) / tam, 0);
    vec4 g11 = textureGather(uDepth, (base + vec2(2.0, 2.0)) / tam, 0);
    float D[16];
    D[0] = g00.w;  D[1] = g00.z;  D[2] = g10.w;  D[3] = g10.z;
    D[4] = g00.x;  D[5] = g00.y;  D[6] = g10.x;  D[7] = g10.y;
    D[8] = g01.w;  D[9] = g01.z;  D[10] = g11.w; D[11] = g11.z;
    D[12] = g01.x; D[13] = g01.y; D[14] = g11.x; D[15] = g11.y;
    float bil = mix(mix(D[5], D[6], f.x), mix(D[9], D[10], f.x), f.y);
    float H[16];
    float lo = 1e9, hi = -1e9, dlo = 1.0, dhi = 0.0;
    for (int i = 0; i < 16; i++) {
        H[i] = desplazTotal(D[i], gF, gN) * P.res.x;      /* en pixeles de pantalla */
        lo = min(lo, H[i]);
        hi = max(hi, H[i]);
        dlo = min(dlo, D[i]);
        dhi = max(dhi, D[i]);
    }
    /* El salto mas grande entre dos puntos vecinos del mapa, contando menos los que quedan lejos del
       pixel (a dos puntos ya no cuentan): asi la zona de canto empieza y acaba poco a poco, sin
       cuadricula. */
    float salto = 0.0;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            vec2 e1 = vec2(float(c) - 0.5, float(r) - 1.0) - f;          /* par de lado */
            vec2 e2 = vec2(float(r) - 1.0, float(c) - 0.5) - f;          /* par de arriba abajo */
            salto = max(salto, abs(H[r * 4 + c + 1] - H[r * 4 + c]) * (1.0 - smoothstep(1.2, 2.0, length(e1))));
            salto = max(salto, abs(H[(c + 1) * 4 + r] - H[c * 4 + r]) * (1.0 - smoothstep(1.2, 2.0, length(e2))));
        }
    }
    float zona = smoothstep(2.0, 3.0, salto);
    if (zona <= 0.0)
        return vec2(bil, 0.0);
    /* Solo se corta una rampa ESTRECHA (rama experimental, con el «alcance del relieve»). El umbral de
       arriba va en pixeles de desplazamiento, y con el alcance x2,88 tambien lo pasan rampas anchas:
       el canto desenfocado de un portatil que en el mapa baja en 8 a 12 puntos. Ahi la ventana de
       4 x 4 no llega a ver el llano de ningun lado, el valor «de cada lado» es un punto de la rampa
       que cambia al mover la ventana, y el corte salia a dientes (MEDIDO en una fila: 62 62 106 111 73
       132 132 89 160, sube y baja) y el canto recto se veia torcido en cada ojo («las lineas rectas
       del fantasma se curvearon», Rodolfo, 26-nov). Se mira la fila y la columna del pixel en 8
       puntos (2 mas a cada lado) y el ancho de la rampa = todo lo que cambia / el salto mas grande
       entre vecinos: un canto de 2 o 3 puntos da 2..3,5; una rampa pareja de 8, 7. Se corta entero hasta
       3 y nada desde 4 (con 4..5,5 quedaban la mitad de los dientes: el canto del portatil, de 5
       puntos en su parte baja). MEDIDO a x2,88: escalones de 8 niveles 6022 -> 2531 y exceso de
       sube-y-baja 62,3 -> 13,9; a x1, 1117 -> 1105 escalones (los cortes de verdad siguen). */
    {
        vec4 hL = textureGather(uDepth, (base + vec2(-2.0, 1.0)) / tam, 0);
        vec4 hR = textureGather(uDepth, (base + vec2(4.0, 1.0)) / tam, 0);
        vec4 vT = textureGather(uDepth, (base + vec2(1.0, -2.0)) / tam, 0);
        vec4 vB = textureGather(uDepth, (base + vec2(1.0, 4.0)) / tam, 0);
        float fila[8] = float[8](mix(hL.w, hL.x, f.y), mix(hL.z, hL.y, f.y), mix(D[4], D[8], f.y), mix(D[5], D[9], f.y),
                                 mix(D[6], D[10], f.y), mix(D[7], D[11], f.y), mix(hR.w, hR.x, f.y), mix(hR.z, hR.y, f.y));
        float col[8] = float[8](mix(vT.w, vT.z, f.x), mix(vT.x, vT.y, f.x), mix(D[1], D[2], f.x), mix(D[5], D[6], f.x),
                                mix(D[9], D[10], f.x), mix(D[13], D[14], f.x), mix(vB.w, vB.z, f.x), mix(vB.x, vB.y, f.x));
        float fLo = 1e9, fHi = -1e9, fSalto = 0.0, cLo = 1e9, cHi = -1e9, cSalto = 0.0;
        float fAntes = 0.0, cAntes = 0.0;
        for (int i = 0; i < 8; i++) {
            float hf = desplazTotal(fila[i], gF, gN) * P.res.x;
            float hc = desplazTotal(col[i], gF, gN) * P.res.x;
            fLo = min(fLo, hf); fHi = max(fHi, hf);
            cLo = min(cLo, hc); cHi = max(cHi, hc);
            if (i > 0) {
                fSalto = max(fSalto, abs(hf - fAntes));
                cSalto = max(cSalto, abs(hc - cAntes));
            }
            fAntes = hf;
            cAntes = hc;
        }
        /* el ancho en la direccion donde el mapa salta mas */
        float ancho = fSalto >= cSalto ? (fHi - fLo) / max(fSalto, 1e-4) : (cHi - cLo) / max(cSalto, 1e-4);
        zona *= 1.0 - smoothstep(3.0, 4.0, ancho);
        if (zona <= 0.0)
            return vec2(bil, 0.0);
    }
    vec2 pxMapa = P.res.xy / tam;                      /* pixeles de pantalla por punto del mapa */
    vec3 c0 = colorMedio(uv, vec2(1.2));
    float rango = max(hi - lo, 1e-4);
    /* El color y el valor llano de cada lado: la media de SUS puntos del mapa, pesando por cercania
       y contando casi solo los que estan claramente de ese lado (los de en medio de la rampa son
       mezcla de los dos y tirarian del fondo hacia el objeto y al reves). */
    vec3 cF = vec3(0.0), cB = vec3(0.0);
    float nF = 0.0, nB = 0.0, sF = 0.0, sB = 0.0;
    vec3 centro[4];                    /* el color de los cuatro puntos que rodean al pixel */
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            vec2 q = vec2(float(c) - 1.0, float(r) - 1.0);          /* el punto, contado desde `base` */
            vec2 e = q - f;
            /* peso por distancia, que llega a cero justo donde el punto sale de la ventana de 4 x 4 */
            float w = exp(-0.3 * dot(e, e)) * (1.0 - smoothstep(1.5, 2.0, abs(e.x))) * (1.0 - smoothstep(1.5, 2.0, abs(e.y)));
            /* El color de ese punto del mapa. Si el mapa lo trae (tun10.w), es el de la imagen que
               dio ESE mapa: el mapa se renueva unas 6 veces por segundo y entre uno y otro la
               imagen sigue moviendose; con los colores de ENTONCES para cada lado y el pixel de
               AHORA, el canto sigue al objeto aunque el mapa vaya atrasado (hasta un punto del
               mapa, 7 pixeles). Si no lo trae (mapa de archivo), se lee de la imagen de ahora. */
            vec3 cq = P.tun10.w == 1.0
                          ? texelFetch(uDepth, clamp(ivec2(base + q), ivec2(0), ivec2(tam) - 1), 0).gba
                          : colorMedio((base + q + 0.5) / tam, 0.25 * pxMapa);
            if (r >= 1 && r <= 2 && c >= 1 && c <= 2)
                centro[(r - 1) * 2 + (c - 1)] = cq;
            float t = (H[r * 4 + c] - lo) / rango;
            float tf = t * t * t, tb = (1.0 - t) * (1.0 - t) * (1.0 - t);
            tf *= tf;
            tb *= tb;
            cF += w * tf * cq;
            sF += w * tf * D[r * 4 + c];
            nF += w * tf;
            cB += w * tb * cq;
            sB += w * tb * D[r * 4 + c];
            nB += w * tb;
        }
    }
    cF /= max(nF, 1e-6);
    cB /= max(nB, 1e-6);
    /* Cuanto tiene el pixel de objeto y cuanto de fondo, por su COLOR: su sitio en la linea que va
       del color del fondo al del objeto (0 = fondo, 1 = objeto). La orilla de un objeto no es una
       raya: en un video es una mezcla de varios pixeles (mas ancha cuanto mas desenfocado), y esta
       cuenta dice en que punto de esa mezcla esta cada pixel. */
    vec3 eje = cF - cB;
    float contraste = dot(eje, eje);
    float alfa = clamp(dot(c0 - cB, eje) / max(contraste, 1e-5), 0.0, 1.0);
    /* Lo que dice el mapa por su cuenta (0 = en el fondo, 1 = en el objeto, 0,5 = a mitad de rampa).
       El color manda cerca de la rampa del mapa y deja de mandar lejos de ella (a punto y medio):
       un brillo del objeto que se parece al fondo no lo saca del objeto, ni una mancha del fondo
       lo mete. Y si los dos lados son del mismo color, manda el mapa. */
    float tm = clamp((desplazTotal(bil, gF, gN) * P.res.x - lo) / rango, 0.0, 1.0);
    float lejos = 2.0 * tm - 1.0;
    lejos *= lejos;
    float manda = smoothstep(0.0016, 0.0144, contraste) * (1.0 - lejos * lejos);
    /* El corte: un escalon de verdad, sin valores intermedios, a lo largo de una linea de nivel de
       esa mezcla (el pixel se va con el objeto en cuanto tiene 3 partes de 10 de el: el objeto se
       lleva su orilla, que es preferible a que la deje atras). Al seguir una linea de nivel del
       color, el corte sale liso aunque el mapa venga a escalones, y el color que queda a cada lado
       del corte es el mismo en todas las filas: MEDIDO en un hombro desenfocado, donde el corte
       anterior (por votos de parecido) caia a media orilla y a saltos de una fila a otra, y eso se
       veia como una raya dentada junto al canto. */
    float mezcla = mix(tm, alfa, manda);
    /* Con el «alcance del relieve» T > 1 (rama experimental, 26-nov) el umbral baja a 0,3 / T: el objeto
       se lleva casi toda su orilla. Con 0,3 los pixeles de orilla con menos de 3 partes de 10 de objeto
       se quedaban en el fondo y, al otro lado del hueco (ahora relleno con fondo limpio, ver
       rellenoHueco), se veian como una raya fina oscura que repetia el contorno a 15-20 pixeles. Lo que
       el objeto se lleva de mas es casi todo color de fondo, y queda pegado al relleno, que es fondo. */
    float umbralCorte = 0.3 / (P.tun5.y > 0.0 ? max(1.0, P.tun5.y / 12.67) : 1.0);
    float duro = mezcla > umbralCorte ? sF / max(nF, 1e-6) : sB / max(nB, 1e-6);
    /* Cuando NO se corta, aunque el mapa de un salto:
       - Si los dos lados son del mismo color no hay canto que seguir, y un corte puesto a ojo en
         mitad de un dibujo se veria como un desgarro: se deja la rampa suave del mapa.
       - Si el color del pixel no cuadra con lo que el mapa creia que habia aqui (ni es el de este
         sitio en la imagen que dio el mapa, ni cae entre el color del fondo y el del objeto), el
         mapa es de OTRA imagen: pasa el quinto de segundo que sigue a un cambio de plano, hasta que
         llega el mapa nuevo. Cortar ahi seria repartir escalones al azar por la pantalla. (Un
         objeto que se mueve si cuadra: donde estaba el fondo ahora hay color de objeto, o al reves,
         y ahi el corte sigue al objeto.) */
    float fiable = 1.0;
    if (P.tun10.w == 1.0) {
        vec3 aqui = mix(mix(centro[0], centro[1], f.x), mix(centro[2], centro[3], f.x), f.y);
        vec3 ahora = colorMedio(uv, 0.5 * pxMapa);       /* el color de ahora, a la escala del mapa */
        float igual = 1.0 - smoothstep(0.06, 0.16, length(ahora - aqui));
        float enLinea = 1.0 - smoothstep(0.08, 0.2, length(c0 - (cB + alfa * eje)));
        fiable = max(igual, enLinea);
    }
    zona *= fiable * smoothstep(0.0016, 0.0144, contraste);
    /* el segundo valor es esa mezcla (0 = fondo, 1 = objeto): le sirve a rellenoHueco para saber si
       un corte parte un canto nitido o pasa por mitad de un canto borroso */
    return vec2(mix(bil, duro, zona), mezcla * zona);
}

/* La profundidad afinada de la fuente en `uv`. De normal ya esta hecha: el pase de afinado la dejo
   en uFino, un valor por pixel de pantalla (asi `afinar`, que es cara, se hace UNA vez por pixel y
   no las treinta y tantas que la pide la busqueda de cada ojo: MEDIDO, 21 ms por cuadro sin el pase
   contra ~1 con el). Se lee sin interpolar: entre un pixel y el de al lado hay un escalon limpio,
   que es lo que necesita buscaVista para saber cuando ha dado con el costado de un objeto. */
float profFuente(vec2 uv, float gF, float gN) {
    if (P.tun10.z == 1.0) {
        ivec2 tam = textureSize(uFino, 0);
        vec2 e = texelFetch(uFino, clamp(ivec2(floor(uv * vec2(tam))), ivec2(0), tam - 1), 0).rg;
        return e.r + e.g / 255.0;
    }
    /* Sin el pase de afinado (no pasa con el conversor puesto y el mapa a punto): el mapa tal cual,
       bilineal. Aqui se llamaba antes a `afinar`, y como la busqueda de cada ojo pide la profundidad
       unas treinta veces, el compilador de NVIDIA metia `afinar` entera en cada sitio: con lo que se le
       anadio el 26-nov (el ancho de la rampa) el visor tardaba 18,7 s en arrancar sin la cache de
       sombreadores, y la primera sesion tras instalar no llegaba a abrirse («no inicio», Rodolfo). */
    vec2 tam = vec2(textureSize(uDepth, 0));
    vec2 p = uv * tam - 0.5;
    vec2 b = floor(p), f = p - b;
    vec4 g = textureGather(uDepth, (b + 1.0) / tam, 0);       /* w = (b), z = (b + x), x = (b + y), y = (b + 1) */
    return mix(mix(g.w, g.z, f.x), mix(g.x, g.y, f.x), f.y);
}

/* La vista de un ojo en `uv`. `cam` es donde esta ese ojo (-1 el izquierdo, +1 el derecho, mas lo que
   se haya movido la cabeza): un punto de la fuente en x con desplazamiento s se ve en x - cam*s.
   Devuelve (cuanto hay que correrse en x para leer el color, la profundidad de lo que se ve, y 1 si
   es un hueco rellenado). */
vec3 buscaVista(vec2 uv, float cam, float gF, float gN) {
    if (abs(cam) < 0.01) {
        return vec3(0.0, profFuente(uv, gF, gN), 0.0);
    }
    float arriba = P.tun5.y > 0.0 ? P.tun5.y / P.res.x : 1.0;
    float abajo = P.tun5.z > 0.0 ? P.tun5.z / P.res.x : 1.0;
    float empuje = max(abs(P.tun7.z), abs(P.tun7.w)) / P.res.x;
    float sHi = min(arriba, max(0.0, 1.0 - P.tun4.z) * P.tun4.y * gN + empuje);
    float sLo = -min(abajo, max(0.0, P.tun4.z) * P.tun4.y * gF + empuje);
    float rango = max(sHi - sLo, 1e-6);
    /* un paso cada dos pixeles de pantalla: no se salta ningun canto de mas de dos pixeles */
    int n = int(clamp(ceil(abs(cam) * rango * P.res.x * 0.5), 2.0, 26.0));
    float paso = rango / float(n);
    float sA = sHi, hA = desplazTotal(profFuente(uv + vec2(cam * sA, 0.0), gF, gN), gF, gN);
    float sB = sA, hB = hA;
    bool dado = hA >= sA;
    for (int k = 1; k <= n && !dado; k++) {
        sB = sHi - float(k) * paso;
        hB = desplazTotal(profFuente(uv + vec2(cam * sB, 0.0), gF, gN), gF, gN);
        if (hB >= sB) {
            dado = true;
        } else {
            sA = sB;
            hA = hB;
        }
    }
    if (!dado) {
        return vec3(cam * hB, profFuente(uv + vec2(cam * hB, 0.0), gF, gN), 0.0);   /* no deberia pasar: el fondo siempre para */
    }
    for (int i = 0; i < 5; i++) {      /* afinar entre el ultimo punto libre y el primero tapado */
        float sM = 0.5 * (sA + sB);
        float hM = desplazTotal(profFuente(uv + vec2(cam * sM, 0.0), gF, gN), gF, gN);
        if (hM >= sM) {
            sB = sM;
            hB = hM;
        } else {
            sA = sM;
            hA = hM;
        }
    }
    /* Si a un lado del punto encontrado esta el fondo y al otro el objeto (un salto de mas de 2,5
       pixeles de desplazamiento en un tramo de centesimas de pixel), no se ha dado con una
       superficie sino con el COSTADO del objeto: aqui el objeto, al correrse, destapo fondo que la
       imagen no trae. Se devuelve donde esta el fondo pegado a ese canto (la ultima muestra libre
       de la busqueda, a centesimas de pixel de la pared), la ALTURA de ese fondo y que parte del
       hueco se lleva recorrida (casi 0 junto al fondo, 1 junto al objeto); el color lo pone
       rellenoHueco. */
    if (hB - hA > 2.5 / P.res.x) {
        /* y, en el mismo numero, el ancho del hueco en pixeles (de 4 en 4: parte + 4 x ancho) */
        float anchoPx = abs(cam) * (hB - hA) * P.res.x;
        return vec3(cam * sA, hA, clamp((sB - hA) / (hB - hA), 1e-4, 1.0) + 4.0 * floor(anchoPx + 0.5));
    }
    /* una superficie: el desplazamiento es justo su altura (en lo llano sale exacto, sin el pequeno
       sesgo del ultimo tramo de la busqueda) */
    float s = clamp(hB, sB, sA);
    return vec3(cam * s, profFuente(uv + vec2(cam * s, 0.0), gF, gN), 0.0);
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
    return fuente(q + vec2((izq ? vistaIzq() : vistaDer()) * s + P.tun1.w, 0.0));
}
/* --- realce de detalle en el sujeto (v0.68, el inpaint.unsharp_mask del Hub) -----------------
   Leido de su ensamblador NV: detalle = imagen - imagen desenfocada (nivel 2 de mip, 4 x 4 puntos);
   una puerta de ruido smoothstep(t/2, t, |detalle|) con t = 0,05 (1 + luminancia/2); se suma con fuerza
   0,4; y todo eso solo cerca del sujeto: se apaga con smoothstep(0,2, 0,3, distancia al foco). Un sujeto
   mas nitido que el fondo se despega de el: es una senal de profundidad de las de foto (enfoque
   selectivo), no de las de dos ojos. Aqui no hay mips de la fuente: el desenfoque son cuatro lecturas
   bilineales a +-1,5 puntos, que juntas cubren un cuadro de 5 x 5 sin la cruz del centro.
   `c` es el color ya leido en `q` (la coordenada de ESE ojo) y `d` su profundidad en el mapa. */
vec3 realceSujeto(vec3 c, vec2 q, float d) {
    float fuerza = P.tun9.z;
    if (fuerza <= 0.0)
        return c;
    float suj = P.tun9.y >= 0.0 ? P.tun9.y : P.tun4.z;
    float lejos = smoothstep(0.2, 0.3, abs(d - suj));
    if (lejos >= 1.0)
        return c;                      /* fuera del sujeto no se gastan las cuatro lecturas */
    vec2 t = 1.5 / vec2(textureSize(uSrc, 0));
    if (P.tun10.x == 1.0)
        t.x *= 2.0;                    /* la media imagen esta estirada al doble: sus puntos miden el doble */
    vec3 borroso = 0.25 * (fuente(q + vec2(-t.x, -t.y)) + fuente(q + vec2(t.x, -t.y)) +
                           fuente(q + vec2(-t.x, t.y)) + fuente(q + vec2(t.x, t.y)));
    vec3 det = c - borroso;
    float umbral = 0.05 * (1.0 + dot(c, vec3(0.1495, 0.2935, 0.057)));
    float puerta = smoothstep(0.5 * umbral, umbral, length(det));
    vec3 realzado = clamp(c + det * puerta * fuerza, 0.0, 1.0);
    return mix(realzado, c, lejos);
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

/* El color de un hueco destapado (ver buscaVista). `uv` cae en el pixel de fondo pegado al corte,
   `lado` es hacia donde sigue el fondo (+1 o -1), `parte` que fraccion del hueco se lleva recorrida
   (0 junto al fondo, 1 junto al objeto) y `hFondo` la altura de ese fondo.

   Hay dos clases de corte y cada una pide su relleno:
   - El corte parte un canto NITIDO (a un lado fondo, al otro objeto): el hueco es fondo que la
     imagen no trae. Se rellena con el color del fondo pegado al corte, extendido y promediado con
     el de justo encima y debajo (para que cada fila no repita su ultimo pixel en rayas: el peine).
     Es, en pequeno, lo que hace el relleno del Hub (inpaint.push/pull).
   - El corte pasa por mitad de un canto BORROSO (un hombro desenfocado: la mezcla de color ocupa
     diez pixeles y a los dos lados del corte el color es casi el mismo). Ahi no hay fondo que
     inventar: el hueco se rellena pasando poco a poco del color de un lado al del otro, o sea que
     el degradado del canto sigue, solo que mas tendido. Con el color del fondo extendido quedaba
     un escalon a media orilla, y con el fondo en espejo o leido mas lejos, una raya oscura (MEDIDO:
     171 | 30 30 37 47 | 94 en vez de pasar de 171 a 104).
   Cual de las dos es lo dice la mezcla de color que el pase de afinado dejo en uFino.b: si a un
   lado y otro del corte se parece (menos de 0,2 de diferencia), el canto es borroso. */
vec3 rellenoHueco(vec2 uv, float lado, float parteYAncho, float hFondo, float gF, float gN) {
    vec2 pxl = 1.0 / P.res.xy;
    vec2 c = (floor(uv * P.res.xy) + 0.5) * pxl;                /* el centro del pixel de fondo pegado al corte */
    /* buscaVista manda en el mismo numero que parte del hueco se lleva y su ancho en pixeles */
    float anchoK = floor((parteYAncho - 1e-5) / 4.0);
    float parte = parteYAncho - 4.0 * anchoK;
    /* Con el «alcance del relieve» (rama experimental, 26-nov) el hueco es hasta 2,88 veces mas ancho
       y lo de abajo lo estiraba entero: en un canto desenfocado (la clavija de un violin contra una
       ventana) el pixel de fondo pegado al corte aun lleva hasta 3 partes de 10 del objeto, y una
       franja gris rayada de ~20 pixeles, a dientes de una fila a otra, rodeaba cada objeto cercano
       (con x1, ~8). Ahora, con alcance T > 1, el hueco se parte en dos: lo que tendria con x1 (su
       ultima parte, junto al objeto) se rellena como siempre, y el resto, junto al fondo, con fondo
       LIMPIO: el primero de los 6 pixeles siguientes hacia el fondo que ya no tiene nada del objeto. */
    float alcance = P.tun5.y > 0.0 ? max(1.0, P.tun5.y / 12.67) : 1.0;
    float tramo = anchoK / alcance;                              /* lo que mediria el hueco con x1 */
    float recorrido = parte * anchoK;                            /* pixeles desde el lado del fondo */
    bool partido = alcance > 1.01 && anchoK > tramo + 0.5;
    if (partido) {
        parte = clamp((recorrido - (anchoK - tramo)) / max(tramo, 1.0), 0.0, 1.0);
        if (P.tun10.z == 1.0) {
            ivec2 tam = textureSize(uFino, 0);
            vec2 cl = c;
            for (int k = 0; k < 6; k++) {
                ivec2 p = clamp(ivec2(floor(cl * vec2(tam))), ivec2(0), tam - 1);
                if (texelFetch(uFino, p, 0).b < 0.06)
                    break;
                cl += vec2(lado * pxl.x, 0.0);
            }
            c = cl;
        }
    }
    vec3 cFondo = colorFuente(c);
    float borroso = 0.0;
    if (P.tun10.z == 1.0) {
        ivec2 tam = textureSize(uFino, 0);
        ivec2 p = clamp(ivec2(floor(uv * vec2(tam))), ivec2(0), tam - 1);
        ivec2 q = clamp(p - ivec2(int(lado), 0), ivec2(0), tam - 1);     /* el pixel del otro lado del corte */
        borroso = 1.0 - smoothstep(0.2, 0.45, abs(texelFetch(uFino, q, 0).b - texelFetch(uFino, p, 0).b));
    }
    /* canto nitido: el fondo pegado al corte, promediado con sus vecinos de fondo */
    float ry = (1.0 + 2.0 * parte) * pxl.y;
    vec2 punto[3] = vec2[3](c + vec2(0.0, ry), c - vec2(0.0, ry), c + vec2(lado * 1.5 * pxl.x, 0.0));
    float peso0[3] = float[3](0.22, 0.22, 0.16);
    vec3 suma = 0.4 * cFondo;
    float peso = 0.4;
    for (int i = 0; i < 3; i++) {
        float h = desplazTotal(profFuente(punto[i], gF, gN), gF, gN);
        float w = abs(h - hFondo) <= 1.25 * pxl.x ? peso0[i] : 0.0;      /* solo cuenta si tambien es fondo */
        suma += w * colorFuente(punto[i]);
        peso += w;
    }
    vec3 nitido = suma / peso;
    if (borroso <= 0.0)
        return nitido;
    /* canto borroso: del color de este lado del corte al del otro (el otro lado es siempre el pixel
       pegado al corte, aunque el fondo se haya leido mas alla) */
    vec2 cCorte = (floor(uv * P.res.xy) + 0.5) * pxl;
    vec3 tendido = mix(cFondo, colorFuente(cCorte - vec2(lado * pxl.x, 0.0)), parte);
    return mix(nitido, tendido, borroso);
}

/* --- las franjas de la barra del reproductor (v0.90) -------------------------------------------
   En un video dividido, la barra del reproductor (y el titulo de arriba) va pintada UNA vez a lo
   ancho de la ventana, encima de las dos imagenes. Desde la v0.81 esas filas se ensenan enteras y
   sin partir, para que la barra salga de una pieza y este donde se pica con el raton; pero detras
   de la barra se veian las dos copias del video, a media anchura cada una («toman cuadros
   congelados del video que se ven fatal», Rodolfo, 25-nov).

   Ahora en la franja sigue EL VIDEO (la imagen izquierda sola, que es la que se esta viendo, en 2D
   y atenuada con un degradado desde el borde de dentro), y encima van solo los MANDOS del
   reproductor, recortados del cuadro entero y puestos donde estan de verdad. Mando es lo que
   cumple tres cosas (ver `mando`): brilla, destaca de lo que tiene alrededor y NO esta tambien en
   la otra mitad del cuadro; el video sale dos veces, una en cada mitad, y un icono o la hora, una
   sola. Con eso mismo se limpia el fondo: donde a la imagen izquierda le cayo encima un mando, se
   pone lo que ensena la otra mitad en ese sitio. */
float brillo(vec3 c) { return max(c.r, max(c.g, c.b)); }
vec3 cuadroEntero(vec2 uv) {
    vec2 q = uv;
    if (P.tun3.z == 1.0)
        q.y = 1.0 - q.y;
    return textureLod(uSrc, q, 0.0).rgb;
}
/* El brillo de lo que rodea a `uv` en el cuadro entero, sin mirar en su propia fila (la barra de
   progreso es una raya larga): seis lecturas a 90 x 40 y a 64 pixeles. */
float brilloAlrededor(vec2 uv) {
    vec2 px = 1.0 / P.res.xy;
    return (brillo(cuadroEntero(uv + vec2(-90.0, -40.0) * px)) + brillo(cuadroEntero(uv + vec2(90.0, -40.0) * px)) +
            brillo(cuadroEntero(uv + vec2(-90.0, 40.0) * px)) + brillo(cuadroEntero(uv + vec2(90.0, 40.0) * px)) +
            brillo(cuadroEntero(uv + vec2(0.0, -64.0) * px)) + brillo(cuadroEntero(uv + vec2(0.0, 64.0) * px))) / 6.0;
}
/* Cuanto de mando del reproductor tiene el punto `uv` del cuadro entero (0 = video, 1 = mando).
   `alrededor` es el brillo de su vecindad (brilloAlrededor), o un numero negativo para que se mida
   aqui: quien pregunta por varios puntos pegados lo mide una vez y lo pasa. */
float mando(vec2 uv, float alrededor) {
    vec3 c = cuadroEntero(uv);
    float b = brillo(c);
    /* 1) brilla: blanco o color vivo (el rojo de lo ya visto) */
    float llave = smoothstep(0.55, 0.80, b);
    if (llave <= 0.0)
        return 0.0;
    /* 2) destaca de lo que tiene alrededor. Una zona clara del video no destaca de si misma. */
    if (alrededor < 0.0)
        alrededor = brilloAlrededor(uv);
    float destaca = smoothstep(0.08, 0.22, b - alrededor);
    if (destaca <= 0.0)
        return 0.0;
    /* 3) no esta tambien en la otra mitad. Se busca a un lado y a otro hasta un 6 % del ancho del
          cuadro, que es lo que puede separar un video sus dos imagenes (MEDIDO en los de Rodolfo:
          hasta 58 mm, un 10 % del ancho de cada mitad). MEDIDO con un cuadro de prueba: sin esto, las
          rayas claras de un codigo de barras del video se colaban como mandos. */
    vec2 px = 1.0 / P.res.xy;
    float otra = uv.x < 0.5 ? uv.x + 0.5 : uv.x - 0.5;
    float dmin = 10.0;
    for (int k = -38; k <= 38; k++) {
        vec3 d = c - cuadroEntero(vec2(otra + float(k) * 6.0 * px.x, uv.y));
        dmin = min(dmin, dot(d, d));
    }
    float propio = smoothstep(0.012, 0.06, dmin);
    /* ...salvo que sea una raya larga y fina, que es la barra de progreso: esa cruza las dos mitades.
       Larga: sigue a 70 pixeles por algun lado. Fina: 18 pixeles por encima y por debajo el color ya
       es otro (se mira el color y no el brillo: MEDIDO, lo rojo de la barra sobre un video claro se
       apagaba a la mitad porque el papel de debajo brillaba casi tanto como ella). */
    float raya = 0.0;
    if (propio < 1.0) {
        float sigue = max(brillo(cuadroEntero(uv + vec2(-70.0, 0.0) * px)), brillo(cuadroEntero(uv + vec2(70.0, 0.0) * px)));
        vec3 a1 = c - cuadroEntero(uv + vec2(0.0, -18.0) * px), a2 = c - cuadroEntero(uv + vec2(0.0, 18.0) * px);
        raya = smoothstep(0.5, 0.7, sigue / b) * smoothstep(0.02, 0.08, min(dot(a1, a1), dot(a2, a2)));
    }
    return llave * destaca * max(propio, raya);
}
/* La imagen izquierda en el punto `qi` del cuadro entero, sin los mandos que le hayan caido encima:
   si ahi, o a dos o tres pixeles, hay un mando (a la imagen izquierda le cae media barra del
   reproductor, que al estirarla saldria deformada), se pone lo que ensena la otra mitad en ese sitio. */
vec3 fondoLimpio(vec2 qi) {
    const vec2 cerca[10] = vec2[10](vec2(1.5, 0.0), vec2(-1.5, 0.0), vec2(3.0, 0.0), vec2(-3.0, 0.0), vec2(0.0, 2.0),
                                    vec2(0.0, -2.0), vec2(2.2, 2.0), vec2(-2.2, -2.0), vec2(2.2, -2.0), vec2(-2.2, 2.0));
    vec2 px = 1.0 / P.res.xy;
    qi.x = min(clamp(qi.x, 0.0, 0.5), 0.5 - 0.5 / float(textureSize(uSrc, 0).x));
    vec3 c = cuadroEntero(qi);
    if (brillo(c) < 0.30)
        return c;          /* tan oscuro que ni el ni su borde son un mando: lo mas corriente bajo el velo */
    float alrededor = brilloAlrededor(qi);
    float tapado = mando(qi, alrededor);
    for (int k = 0; k < 10 && tapado < 0.3; k++)
        tapado = max(tapado, mando(qi + cerca[k] * px, alrededor));
    if (tapado > 0.0) {
        /* ...salvo que en la otra mitad tambien haya un mando en ese sitio (la barra de progreso cruza
           las dos, y con ella su boton): entonces, el video de unas filas mas arriba */
        vec2 qo = qi + vec2(0.5, 0.0);
        vec3 otro = mando(qo, -1.0) > 0.2 ? cuadroEntero(qi - vec2(0.0, 12.0 * px.y)) : cuadroEntero(qo);
        c = mix(c, otro, smoothstep(0.0, 0.3, tapado));
    }
    return c;
}
/* El color de la franja en `uv`. `fundido` va de 0 en el borde interior de la franja a 1 unos
   pixeles mas adentro: con el el video se atenua y se desenfoca poco a poco (un cristal
   esmerilado: asi lo que quede de los mandos estirados, o un trozo tomado del otro ojo, no se
   reconoce) y entran los mandos, nitidos. */
vec3 franjaBarra(vec2 uv, float fundido) {
    /* este punto de la imagen izquierda, en el cuadro entero: media anchura */
    vec2 qi = vec2(uv.x * 0.5, uv.y);
    vec2 paso = vec2(0.5, 1.0) / P.res.xy;             /* un pixel de pantalla, en el cuadro entero */
    float r = 10.0 * fundido;
    vec3 fondo = 0.16 * fondoLimpio(qi);
    for (int k = 0; k < 4; k++) {                       /* un anillo a media distancia y otro entero */
        float ang = 0.7854 + 1.5708 * float(k);
        fondo += 0.11 * fondoLimpio(qi + 0.5 * r * vec2(cos(ang), sin(ang)) * paso);
    }
    for (int k = 0; k < 8; k++) {
        float ang = 0.7854 * float(k);
        fondo += 0.05 * fondoLimpio(qi + r * vec2(cos(ang), sin(ang)) * paso);
    }
    fondo *= mix(1.0, 0.55, fundido);
    return mix(fondo, cuadroEntero(uv), mando(uv, -1.0) * fundido);
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
        o = vec4(fuente(qc), 1.0);     /* con tun10.x la red ve solo la imagen izquierda */
        return;
    }
    /* pase de afinado (v0.89, cantos finos): la profundidad de la fuente con el canto en su sitio, un
       valor por pixel. Sale partida en dos canales de 8 bits (alto y bajo) para no perder finura: con
       8 bits solos el desplazamiento iria a saltos de 0,18 pixeles. */
    if (P.tun4.w == 3.0) {
        vec2 af = afinar(vec2(u, v), P.tun7.x > 0.0 ? P.tun7.x : 1.0, P.tun7.y > 0.0 ? P.tun7.y : 1.0);
        float d = clamp(af.x, 0.0, 1.0);
        float alto = floor(d * 255.0);
        o = vec4(alto / 255.0, d * 255.0 - alto, af.y, 1.0);    /* .b = cuanto de objeto tiene el pixel */
        return;
    }

    /* Las filas donde el reproductor pinto su barra a lo ancho de la ventana (las encuentra el visor,
       ver src/barra.h): ahi los dos ojos ven el cuadro ENTERO, sin partir y sin correr, para que la
       barra salga de una pieza y este donde se pica con el raton. Vale para el video dividido tal
       cual y para el suavizado. */
    bool franja = (P.tun3.y == 0.0) && (fuv.y > 1.0 - P.tun3.x || fuv.y < P.tun9.w);
    /* Cuanto de franja es este punto: 0 en su borde interior, 1 a 24 pixeles (de 2160) mas adentro. En
       ese tramo cada ojo pasa poco a poco de su vista en 3D a la franja, que es la misma para los dos.
       Es corto a proposito: tiene que acabar antes del primer mando, porque en el fundido los mandos
       se verian tambien estirados, que es como los trae la vista en 3D. MEDIDO con la barra de
       progreso al 97 %: sus dos mitades son iguales, el detector no la ve y la franja empieza solo 27
       pixeles por encima de ella; con un fundido de 48 se veia el boton rojo dos veces. */
    float fundidoFranja = (!franja || P.tun11.x == 1.0) ? 0.0
        : smoothstep(0.0, 0.0111, fuv.y > 1.0 - P.tun3.x ? fuv.y - (1.0 - P.tun3.x) : P.tun9.w - fuv.y);

    /* pase 2D: sin tramar (la lente y el scaler pueden seguir activos). Aqui se muestrea la
       imagen ENTERA: si se usara srcHalf se veria solo media pantalla ampliada al doble, que es
       exactamente el "se ve enorme y cortado" con un video que no tiene dos vistas. */
    if (P.tun2.z == 0.0) {
        vec2 q2 = fuv;
        if (P.tun3.z == 1.0)
            q2.y = 1.0 - q2.y;          /* la fuente puede venir con la primera fila abajo */
        /* con el video dividido suavizado, el 2D plano es la imagen izquierda sola (un 2D de verdad,
           no las dos mitades); en las filas de la barra del reproductor, la franja con sus mandos.
           (En el 2D del video dividido tal cual se ve el cuadro entero, y ahi la barra ya sale de una pieza.) */
        vec3 plano = P.tun10.x == 1.0 ? fuente(q2) : texture(uSrc, q2).rgb;
        if (franja && P.tun10.x == 1.0)
            plano = P.tun11.x == 1.0 ? cuadroEntero(fuv) : mix(plano, franjaBarra(fuv, fundidoFranja), fundidoFranja);
        o = vec4(plano, 1.0);
        return;
    }

    /* Aqui estaba el cuadro de color de la esquina superior izquierda (verde con lente y scaler,
       naranja o rojo si faltaba alguno). Salio en v0.78: tapaba una esquina del video. Si la lente
       o el scaler no responden lo dice la linea de abajo a la izquierda de la pastilla. */

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

    /* Vista de depuracion de la profundidad (v1.12, Ctrl+Mayus+9): en vez de las dos vistas, la misma
       imagen a los dos ojos (se ve plana, sin el relieve). Gris = la profundidad que usa la trama, ya
       afinada y con los rotulos apoyados (blanco = cerca); naranja = donde pesa la saliencia, que es la
       que elige el plano en Auto (relativa al punto que mas pesa de esa estimacion); cian = lo que cae
       en el plano de la pantalla (tun4.z), lo que se queda quieto y nitido. */
    if (P.tun11.y == 1.0 && P.tun4.x == 1.0) {
        float d = P.tun10.y == 1.0 ? profFuente(fuv, 1.0, 1.0) : texture(uDepth, fuv).r;
        vec3 c = vec3(d);
        float sal = texture(uSal, fuv).r;
        c = mix(c, vec3(1.0, 0.45, 0.05) * (0.3 + 0.7 * d), 0.85 * smoothstep(0.15, 0.85, sal));   /* con su sombra */
        c = mix(c, vec3(0.0, 0.85, 1.0), 0.9 * (1.0 - smoothstep(0.008, 0.016, abs(d - P.tun4.z))));
        o = vec4(c, 1.0);
        return;
    }

    float lx = (P.tun2.w == 1.0) ? 0.0 : 0.5;   /* orden de ojos validado en el Hub */
    float rx = (P.tun2.w == 1.0) ? 0.5 : 0.0;
    /* "3D Depth" separa las dos vistas en sentidos opuestos (más paralaje);
       "3D Popout" corre el par entero (mueve el plano de convergencia). */
    float dep = P.tun3.w;
    float cnv = P.tun1.w;
    vec3 L, R;
    if (fundidoFranja >= 1.0 || (franja && P.tun11.x == 1.0)) {
        L = vec3(0.0);        /* franja de lleno: las vistas en 3D no hacen falta, se ponen abajo */
        R = L;
    } else if (P.tun4.x == 1.0 && P.tun10.y == 1.0) {
        /* Video normal con cantos finos (v0.89): la vista de cada ojo se busca sobre la profundidad
           de la fuente (ver buscaVista). El temblor de las zonas llanas se suma igual que antes,
           menos en los huecos rellenados. */
        float gFn = P.tun7.x > 0.0 ? P.tun7.x : 1.0;
        float gNn = P.tun7.y > 0.0 ? P.tun7.y : 1.0;
        /* sonda 15 (taller): la profundidad afinada de la fuente, tal cual */
        if (P.tun6.w == 15.0) {
            o = vec4(vec3(profFuente(fuv, gFn, gNn)), 1.0);
            return;
        }
        vec3 bI = buscaVista(fuv, vistaIzq(), gFn, gNn);
        vec3 bD = buscaVista(fuv, vistaDer(), gFn, gNn);
        /* sonda 14 (taller): cuanto se corre cada ojo (0,5 = nada; 1/80 por pixel) y los huecos */
        if (P.tun6.w == 14.0) {
            o = vec4(0.5 + bI.x * P.res.x / 80.0, 0.5 + bD.x * P.res.x / 80.0, 0.25 * step(1e-7, bI.z) + 0.5 * step(1e-7, bD.z) + 0.2 * profFuente(fuv, gFn, gNn), 1.0);
            return;
        }
        if (P.tun5.x != 0.0) {
            float r = (fract(sin(dot(floor(px), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * P.tun5.x / P.res.x;
            bI.x += vistaIzq() * r * (bI.z > 0.0 ? 0.0 : 1.0);
            bD.x += vistaDer() * r * (bD.z > 0.0 ? 0.0 : 1.0);
        }
        vec2 q6 = fuv;
        if (P.tun3.z == 1.0)
            q6.y = 1.0 - q6.y;
        /* SIN la «convergencia» (cnv). En el camino de antes el color de los dos ojos se leia corrido
           cnv (15 pixeles con «3D Salida» en 50) y el mapa no: mapa e imagen iban desencajados 2,4 mm
           en todos los cantos, y de ahi salia la tira doble junto a cada objeto (MEDIDO con la sonda
           14: el canto del mapa en x = 1400 y el del color en x = 1385). Correr los dos ojos lo mismo
           no cambia el relieve: solo mueve la imagen de lado, asi que aqui no se hace. */
        vec2 qI = q6 + vec2(bI.x, 0.0);
        vec2 qD = q6 + vec2(bD.x, 0.0);
        L = bI.z > 0.0 ? rellenoHueco(fuv + vec2(bI.x, 0.0), sign(vistaIzq()), bI.z, bI.y, gFn, gNn)
                       : realceSujeto(fuente(qI), qI, bI.y);
        R = bD.z > 0.0 ? rellenoHueco(fuv + vec2(bD.x, 0.0), sign(vistaDer()), bD.z, bD.y, gFn, gNn)
                       : realceSujeto(fuente(qD), qD, bD.y);
        /* sondas 16 y 17 (taller, rama experimental): la vista entera del ojo izquierdo o del derecho,
           antes de tejerla; para mirar los cantos a ojo */
        if (P.tun6.w == 16.0) { o = vec4(L, 1.0); return; }
        if (P.tun6.w == 17.0) { o = vec4(R, 1.0); return; }
        if (P.tun2.w == 1.0) { vec3 t = L; L = R; R = t; }
    } else if (P.tun4.x == 1.0) {
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
        vec2 qI = q4 + vec2(vistaIzq() * sI + cnv, 0.0);
        vec2 qD = q4 + vec2(vistaDer() * sD + cnv, 0.0);
        L = realceSujeto(fuente(qI), qI, mp.x);
        R = realceSujeto(fuente(qD), qD, mp.z);
        if (P.tun2.w == 1.0) { vec3 t = L; L = R; R = t; }
    } else if (P.tun4.w == 2.0) {
        /* conversor puesto pero aun sin mapa de profundidad: los dos ojos ven lo mismo, o sea la
           imagen normal y plana. Es lo que se ve medio segundo al encender el conversor y si la red
           falla; si cayera a la rama de lado a lado se veria la pantalla partida en dos. */
        vec2 q3 = fuv;
        if (P.tun3.z == 1.0)
            q3.y = 1.0 - q3.y;
        L = fuente(q3 + vec2(cnv, 0.0));
        R = L;
    } else {
        L = srcHalf(fuv + vec2(cnv + dep, 0.0), lx);
        R = srcHalf(fuv + vec2(cnv - dep, 0.0), rx);
    }
    /* las filas de la barra del reproductor: la franja (ver franjaBarra), a la que cada ojo llega
       fundiendose desde su vista en 3D */
    if (franja) {
        vec3 fb = franjaBarra(fuv, fundidoFranja);
        L = mix(L, fb, fundidoFranja);
        R = mix(R, fb, fundidoFranja);
    }
    if (franja && P.tun11.x == 1.0) {       /* la franja de antes: el cuadro entero, sin tocar */
        L = cuadroEntero(fuv);
        R = L;
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
