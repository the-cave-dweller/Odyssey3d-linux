#version 450
/* efx_prof.frag — (v1.18) la profundidad que leen los efectos de ReShade (efectos_vk.cpp), en dos pasadas.

   El mapa del 3D es cercania de 8 bits (255 = cerca), y los efectos esperan el bufer de profundidad de un juego.
   MEDIDO el 28-nov sobre un cuadro de Cyberpunk a 3840x2160 con su mapa de la red, pasandolo tal cual (estirado) y
   con el plano lejano de ReShade en 1:
     - DisplayDepth: las normales en curvas de nivel por todo el cuadro (cada salto de 1/255 es un escalon);
     - GloomAO: toca el 62 % del cuadro y dibuja esas mismas curvas sobre la calle;
     - MXAO y qUINT_mxao: tocan el 1,3 % (solo la mano, lo mas cercano): con las posiciones en un mundo de 0 a 1 su
       radio (2,5 unidades) lo abarca todo y no oscurecen nada.
   La entrada es el mapa del 3D o, con los cantos finos del visor y la fuente a tamano de pantalla, su profundidad ya
   afinada (la de uFino en weave.frag: el canto del mapa puesto en el canto de la imagen, en dos bytes), que llega con
   un cuadro de retraso porque los efectos corren antes que el afinado.
   Aqui:
     modo 0 (horizontal) y modo 1 (vertical): un suavizado que solo junta lo que esta a pocos niveles (los escalones
       de un mismo plano) y deja los cantos (saltos de mas de 16 niveles), y que al final no mueve el valor mas de un
       nivel: quita las terrazas sin correr nada de sitio;
     al acabar el modo 1: la cercania pasa a distancia (z = cerca / cercania, lo mas cercano a `cerca` unidades y el
       cielo en el plano lejano) y de ahi a lo que guardaria un juego con Z invertida, que es lo que ReShade.fxh
       deshace con RESHADE_DEPTH_INPUT_IS_REVERSED = 1 y RESHADE_DEPTH_LINEARIZATION_FAR_PLANE = 1000 (los de fabrica):
       GetLinearizedDepth devuelve z / lejos, como en un juego. */
layout(location = 0) in vec2 fuv;
layout(location = 0) out vec4 sale;

layout(set = 0, binding = 0) uniform sampler2D ent;

layout(push_constant) uniform Push {
    vec2 paso;     /* un pixel de la entrada, en la direccion de la pasada */
    float modo;    /* 0 = horizontal desde el mapa estirado · 1 = vertical y a profundidad de juego */
    float nivel;   /* un escalon del mapa: 1/255 */
    float cerca;   /* a cuantas unidades queda lo mas cercano (cercania 1) */
    float lejos;   /* el plano lejano: RESHADE_DEPTH_LINEARIZATION_FAR_PLANE */
    float dos;     /* 1 = la entrada es la profundidad afinada del 3D, en dos bytes (r + g/255: uFino en weave.frag) */
} pc;

const int RADIO = 12;
const float SIGMA2 = 2.0 * 6.0 * 6.0;

/* cuanto cuenta un vecino segun lo que se aparta del centro: entero hasta 8 niveles, nada desde 16. Ancho a proposito:
   con una ventana estrecha (se probo de 2,5 a 6) el promedio de una rampa empinada vuelve al valor del centro y la
   escalera se queda (MEDIDO: la calle cercana seguia con rayas en MXAO). Los cantos de verdad saltan mas de 16 */
float parecido(float d) { return 1.0 - smoothstep(8.0 * pc.nivel, 16.0 * pc.nivel, abs(d)); }

/* la cercania de la entrada del modo 0 (1 = cerca). El estirado lineal de antes vale tambien para los dos bytes: la suma
   r + g/255 es lineal en los dos */
float cercaniaEn(vec2 uv)
{
    vec2 t = texture(ent, uv).rg;
    return pc.dos > 0.5 ? t.r + t.g / 255.0 : t.r;
}

void main()
{
    if (pc.modo < 0.5) {
        float c = cercaniaEn(fuv);
        float s = 0.0, n = 0.0;
        for (int k = -RADIO; k <= RADIO; k++) {
            float v = cercaniaEn(fuv + pc.paso * float(k));
            float w = exp(-float(k * k) / SIGMA2) * parecido(v - c);
            s += v * w;
            n += w;
        }
        sale = vec4(s / n, c, 0.0, 1.0);   /* r = suavizado en horizontal · g = el original */
        return;
    }
    vec2 c = texture(ent, fuv).rg;
    float s = 0.0, n = 0.0;
    for (int k = -RADIO; k <= RADIO; k++) {
        vec2 v = texture(ent, fuv + pc.paso * float(k)).rg;
        float w = exp(-float(k * k) / SIGMA2) * parecido(v.g - c.g);
        s += v.r * w;
        n += w;
    }
    float cercania = clamp(c.g + clamp(s / n - c.g, -pc.nivel, pc.nivel), 0.0, 1.0);
    float z = pc.cerca / max(cercania, pc.cerca / pc.lejos);
    /* Z invertida con el plano cercano en 1 (el N de ReShade.fxh): guarda 1 - d, con d = z·F / (F + z·(F - 1)) */
    float r = (pc.lejos - z) / (pc.lejos + z * (pc.lejos - 1.0));
    sale = vec4(r, cercania, 0.0, 1.0);
}
