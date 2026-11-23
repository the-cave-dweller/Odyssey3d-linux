# embed.cmake — mete un fichero binario (SPIR-V generado) dentro de una cabecera C++.
# Uso: cmake -DSRC=a.spv -DNAME=aSpv -DDST=a_sp.h -P embed.cmake
if(NOT SRC OR NOT DST OR NOT NAME)
  message(FATAL_ERROR "embed.cmake necesita SRC, DST y NAME")
endif()

file(READ "${SRC}" datos HEX)
file(SIZE "${SRC}" n)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," datos "${datos}")

get_filename_component(nombre_espaciado "${NAME}" NAME)
file(WRITE "${DST}"
"/* generado automaticamente desde ${SRC}; no editar */
#ifndef ODYSSEY_EMBED_${nombre_espaciado}_H
#define ODYSSEY_EMBED_${nombre_espaciado}_H
static const unsigned char ${NAME}[] = {
${datos}0};
static const unsigned ${NAME}_len = ${n};
#endif
")
