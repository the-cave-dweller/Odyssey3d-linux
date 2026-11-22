/* util.h — pequeñas ayudas compartidas (texto, hex, sysfs). */
#ifndef ODYSSEY_UTIL_H
#define ODYSSEY_UTIL_H

#include <cstdint>
#include <string>
#include <vector>

std::string strTrim(const std::string &s);
std::string strLower(std::string s);
std::vector<uint8_t> unhex(const std::string &s);
std::string toHexUpper(const uint8_t *p, size_t n);
std::string toHexUpper(const std::vector<uint8_t> &v);

/* Lee un archivo pequeño (sysfs, config) en una cadena. False si no existe. */
bool readSmallFile(const std::string &path, std::string &out);

/* Ejecuta un comando y captura stdout+stderr. Devuelve el código de salida. */
int runCapture(const std::string &cmd, std::string &out);

/* Forma "aaaa-bbbb" a partir de un entero 0xNNNN. */
std::string usbIdToHex(uint16_t v);

/* Asegura XDG_RUNTIME_DIR + WAYLAND_DISPLAY en el entorno del proceso.
   Devuelve el nombre del socket ("wayland-1") o vacío si no hay sesión Wayland. */
std::string ensureWaylandEnv();

#endif
