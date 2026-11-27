#pragma once
/* (v1.17) Lo que se puede vigilar junto al cuadrito de los FPS (Rodolfo: «un dropdown de otras cosas que se puedan
   monitorear, como uso de cada gpu (listada por separado), y algunas como las de mangohud»): cada tarjeta (su uso, y
   si se pide su temperatura, su memoria y su consumo), el procesador (uso y temperatura), la memoria del sistema y el
   tiempo de cada cuadro del juego.
   Las NVIDIA por NVML (libnvidia-ml, cargada al vuelo: sin contexto de CUDA ni memoria en la tarjeta; sin ella, nada),
   las AMD por lo que cuenta amdgpu en sysfs, el procesador y la memoria por /proc y hwmon. Nada de esto lanza procesos.
   Lo elegido va en serve.conf como `monitor = clave,clave,...` y el panel se lo manda al visor con la orden `monitor`. */
#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace monitor {

struct Tarjeta {
  std::string clave;      /* "gpu:GPU-…" (el UUID de NVML) o "gpu:amd:0000:03:00.0" */
  std::string nombre;     /* corto: «RTX 5080» */
  int uso = -1;           /* % */
  double tempC = -1.0;
  long usadaMiB = -1, totalMiB = -1;
  double vatios = -1.0;
};

struct Lectura {
  std::vector<Tarjeta> tarjetas;
  double cpu = -1.0;      /* % de todo el procesador, desde la lectura anterior */
  double cpuTempC = -1.0;
  long ramUsadaMiB = -1, ramTotalMiB = -1;
};

/* las claves de lo que no es una tarjeta */
inline const char *const kTemp = "temp", *const kVram = "vram", *const kVatios = "vatios", *const kCpu = "cpu",
                         *const kCpuTemp = "cputemp", *const kRam = "ram", *const kMs = "ms";

/* una lectura entera ahora mismo (la primera del procesador da -1: hace falta otra para el uso) */
Lectura leer();
/* `monitor = a,b,c` <-> el conjunto (vacio o «no» = nada) */
std::set<std::string> deTexto(const std::string &t);
std::string aTexto(const std::set<std::string> &s);

/* el muestreo del visor: un hilo que lee una vez por segundo mientras haya algo elegido */
class Muestreo {
public:
  ~Muestreo();
  void pedir(bool hayAlgo);   /* arranca el hilo la primera vez que hay algo elegido */
  Lectura ultima();
  long vuelta() const { return vuelta_.load(); }   /* cuantas lecturas lleva: cambia cuando hay una nueva */
private:
  std::thread hilo_;
  std::mutex m_;
  Lectura ultima_;
  std::atomic<bool> fin_{false}, quiere_{false};
  std::atomic<long> vuelta_{0};
};

}  // namespace monitor
