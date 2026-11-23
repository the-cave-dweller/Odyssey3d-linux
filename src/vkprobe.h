/* vkprobe.h — dibujar un paso Vulkan completo fuera de la pantalla y leerlo de vuelta.
   Sirve para comprobar, sin ventana ni monitor 3D delante, que el DMA-BUF del compositor
   se importa y se puede muestrear; luego se reutiliza para verificar la trama del panel. */
#ifndef ODYSSEY_VKPROBE_H
#define ODYSSEY_VKPROBE_H

#include "frame.h"
#include "vkdev.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

struct VkPass {
  const unsigned char *vert = nullptr;
  unsigned vertLen = 0;
  const unsigned char *frag = nullptr;
  unsigned fragLen = 0;
  VkImageView src = VK_NULL_HANDLE; /* textura de entrada (binding 0) */
  VkImage srcImage = VK_NULL_HANDLE; /* su imagen, para la barrera de layout */
  const void *ubo = nullptr;        /* binding 1; si es null se manda un bloque a cero */
  size_t uboSize = 0;
  int outW = 0, outH = 0;
  int vertices = 3; /* 3 = triangulo que cubre la pantalla, 4 = cuadro (tira de triangulos) */
  VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
};

/* Ejecuta el paso y devuelve los pixels (fila 0 arriba, RGBA). */
bool vkPassToRgba(VkDev &dev, const VkPass &pass, std::vector<uint8_t> &rgba, std::string *err = nullptr);

/* Importa un fotograma del compositor y lo muestrea tal cual.
   `info` rellena texto de diagnóstico (formato, modificador, memoria). */
bool vkProbeFrame(VkDev &dev, const GpuFrame &f, std::vector<uint8_t> &rgba, std::string &info,
                  std::string *err = nullptr);

#endif
