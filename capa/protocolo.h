/* protocolo.h — la profundidad del juego entre la capa (dentro del juego) y el visor de OdysseyVK
   (v1.13, fase 2). Lo incluyen los dos: capa/odysseyvk_capa.cpp escribe, src/profjuego.cpp lee.

   Memoria compartida `/dev/shm/odysseyvk-profundidad-<uid>` (ODYSSEYVK_PROFUNDIDAD_SHM la cambia, para
   probar): una cabecera de 4096 bytes y tres huecos. La capa importa TODA la region como memoria de
   Vulkan (VK_EXT_external_memory_host) y la tarjeta del juego escribe en ella directamente, en el
   mismo buffer de comandos de DLSS, en este orden y con barreras entre cada paso:
     1. hueco.seqAntes = seq        (vkCmdUpdateBuffer)
     2. los datos de la profundidad  (vkCmdCopyImageToBuffer, aspecto de profundidad)
     3. hueco.seqDespues = seq      (vkCmdUpdateBuffer)
     4. cabecera.ultimoSeq = seq    (vkCmdUpdateBuffer)
   con hueco = seq % huecos. El visor lee ultimoSeq, va a su hueco, lee seqDespues, copia los datos y
   lee seqAntes: si los dos son seq, el cuadro esta entero (si la tarjeta ya hubiera empezado a pisar
   ese hueco, seqAntes valdria seq + huecos).

   Lo que escribe la CPU de la capa (una vez, al crear): magia, version, pid, medidas, formato. Lo que
   escribe el visor: `lectorLatido` (CLOCK_MONOTONIC en ns) en cada mapa; la capa solo copia mientras
   el latido tenga menos de 2 s, asi que un juego sin el 3D puesto no paga la copia.

   Version 2 (fase 3c): detras de los huecos de la profundidad, otros tres con LA MASCARA DEL HUD: la
   imagen donde el juego dibuja su HUD (MEDIDO en Cyberpunk: 3840x2160 RGBA8, limpiada a transparente
   en cada cuadro), reducida en la tarjeta del juego a 960x540 RGBA8 (cada punto, la media de 4x4); su
   alfa es el HUD. Mismo orden de escrituras y la misma comprobacion, con `hudUltimoSeq`. Si el juego
   no tiene esa imagen, `hudUltimoSeq` se queda en 0. */
#pragma once
#include <stdint.h>

#define ODVK_PROF_MAGIA 0x4f564450u   /* "PDVO" */
#define ODVK_PROF_VERSION 2u
#define ODVK_PROF_HUECOS 3u
#define ODVK_PROF_CABECERA 4096u
#define ODVK_PROF_DATOS 64u           /* los datos de cada hueco empiezan aqui; seqAntes va en el 0 */
#define ODVK_HUD_ANCHO 960u
#define ODVK_HUD_ALTO 540u

typedef struct OdvkProfCabecera {
  uint32_t magia;
  uint32_t version;
  int32_t pid;            /* el proceso del juego */
  uint32_t ancho, alto;   /* de la profundidad (el tamano de render de DLSS) */
  uint32_t formato;       /* VkFormat de la imagen: D32_SFLOAT, D24_UNORM_S8_UINT, D16_UNORM... */
  uint32_t bytesPunto;    /* 4 (D32, D24 con su byte de arriba) o 2 (D16) */
  uint32_t huecos;
  uint64_t tamHueco;      /* bytes de cada hueco: 64 + datos + 8 (seqDespues), redondeado a 4096 */
  uint64_t ultimoSeq;     /* lo escribe la tarjeta; 0 = todavia nada */
  uint64_t lectorLatido;  /* lo escribe el visor */
  /* version 2: la mascara del HUD */
  uint32_t hudAncho, hudAlto;            /* ODVK_HUD_ANCHO x ODVK_HUD_ALTO */
  uint32_t hudBytesPunto;                /* 4: RGBA8, el alfa es el HUD */
  uint32_t hudHuecos;                    /* ODVK_PROF_HUECOS */
  uint64_t hudInicio;                    /* donde empieza su primer hueco, desde el principio de la region */
  uint64_t hudTamHueco;                  /* 64 + datos + 8, redondeado */
  uint64_t hudUltimoSeq;                 /* lo escribe la tarjeta; 0 = no hay (o no se encontro) HUD */
  uint32_t hudFuenteAncho, hudFuenteAlto;  /* la imagen del HUD del juego, para el registro */
} OdvkProfCabecera;

/* donde va cada cosa dentro de la region */
static inline uint64_t odvkHueco(const OdvkProfCabecera *c, uint64_t i) {
  return ODVK_PROF_CABECERA + i * c->tamHueco;
}
static inline uint64_t odvkSeqDespues(const OdvkProfCabecera *c) {
  return c->tamHueco - 8;   /* dentro del hueco */
}
static inline uint64_t odvkHudHueco(const OdvkProfCabecera *c, uint64_t i) {
  return c->hudInicio + i * c->hudTamHueco;
}
