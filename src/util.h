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

/* Preferencia de la red del conversor 2D a 3D, guardada en ~/.config/odyssey3d/red-elegida:
   1 = la red de Samsung (mde_*.onnx), 2 = la nativa (depth_anything_v2_small.onnx).
   leerRedPreferida() devuelve 0 si todavia no se eligio ninguna. */
std::string rutaConfigOdyssey(const char *nombre);
/* Ruta dentro de ~/.local/state/odyssey3d: lo que esta pasando ahora mismo (la instantanea del
   panel, la senal de que entregamos la pantalla a proposito), no una preferencia. */
std::string rutaEstadoOdyssey(const char *nombre);
int leerRedPreferida();
void grabarRedPreferida(int red);
/* Ruta por convencion de la red nativa (la que baja tools/descargar-modelo.sh). */
/* A que boton (1 o 2) pertenece un .onnx por como se llama: las redes que trajo el programa de
   Samsung son mde_... o Disp_...; la nuestra se llama depth_anything_v2_small.onnx. */
int slotRed(const std::string &ruta);
std::string rutaRedNuestra();

/* True si el archivo existe y se puede leer. */
bool fileExists(const std::string &path);

/* Lee un archivo binario completo (calibraciones, correctores). False si falta. */
bool readBinaryFile(const std::string &path, std::vector<uint8_t> &out);

/* Ejecuta un comando y captura stdout+stderr. Devuelve el código de salida. */
int runCapture(const std::string &cmd, std::string &out);

/* Forma "aaaa-bbbb" a partir de un entero 0xNNNN. */
std::string usbIdToHex(uint16_t v);

/* Asegura XDG_RUNTIME_DIR + WAYLAND_DISPLAY en el entorno del proceso.
   Devuelve el nombre del socket ("wayland-1") o vacío si no hay sesión Wayland. */
std::string ensureWaylandEnv();

#endif
