/* frame.h — un fotograma tal y como lo entrega el compositor: un DMA-BUF y sus metadatos.
   Vale igual para OpenGL (eglCreateImageKHR) que para Vulkan (importar el fd con el
   modificador DRM explícito). */
#ifndef ODYSSEY_FRAME_H
#define ODYSSEY_FRAME_H

#include <cstdint>
#include <string>

struct GpuFrame {
  int fd = -1;
  int w = 0, h = 0;
  int stride = 0; /* bytes por línea */
  int offset = 0;
  uint64_t modifier = 0;
  uint32_t fourcc = 0;
  bool dmabuf = false;
  bool valid() const { return fd >= 0 && w > 0 && h > 0 && fourcc != 0; }
};

/* CuatroCC DRM: los caracteres van en orden inverso al de la memoria. */
constexpr uint32_t fourccOf(char a, char b, char c, char d) {
  return (uint32_t)(uint8_t)a | ((uint32_t)(uint8_t)b << 8) | ((uint32_t)(uint8_t)c << 16) |
         ((uint32_t)(uint8_t)d << 24);
}

inline std::string fourccText(uint32_t f) {
  char b[5] = {static_cast<char>(f & 0xff), static_cast<char>((f >> 8) & 0xff), static_cast<char>((f >> 16) & 0xff),
               static_cast<char>((f >> 24) & 0xff), 0};
  return b;
}

#endif
