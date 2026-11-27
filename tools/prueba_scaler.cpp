/* tools/prueba_scaler.cpp — (v1.16, de la PR #2 de Noa3) lo que Scaler::set3d da por bueno o por fallo segun lo que
   responda SG_IO, sin monitor: el ioctl se cambia por uno de mentira y el «dispositivo» es /dev/null.
     c++ -std=c++17 -Isrc tools/prueba_scaler.cpp src/scaler.cpp -Wl,--wrap=ioctl -o prueba-scaler && ./prueba-scaler
   Un fallo del monitor (status), del adaptador (host) o del conductor (driver) tiene que salir como fallo; los bytes
   que no tomo (resid) se apuntan pero no lo tumban (ver scaler.cpp). */
#include "scaler.h"

#include <scsi/sg.h>

#include <cstdarg>
#include <cstdio>

static int scsi, host, driver, resid, llamadas;
extern "C" int __wrap_ioctl(int, unsigned long cmd, ...) {
  if (cmd != SG_IO) return -1;
  ++llamadas;
  va_list args;
  va_start(args, cmd);
  auto *h = va_arg(args, sg_io_hdr_t *);
  va_end(args);
  h->status = (unsigned char)scsi;
  h->host_status = (unsigned short)host;
  h->driver_status = (unsigned short)driver;
  h->resid = resid;
  return 0;
}

int main() {
  Scaler scaler;
  if (!scaler.open("/dev/null")) return 1;
  struct Caso { int scsi, host, driver, resid; bool bien; } casos[] = {
      {0, 0, 0, 0, true},     /* todo bien */
      {2, 0, 0, 0, false},    /* CHECK CONDITION del monitor */
      {0, 3, 0, 0, false},    /* el adaptador (p. ej. un timeout) */
      {0, 0, 4, 0, false},    /* el conductor */
      {0, 0, 0, 512, true},   /* no tomo nada: se apunta, no tumba (hasta medirlo en el monitor) */
      {0, 0, 0, 1, true},     /* le falto un byte: igual */
  };
  int fallos = 0;
  for (const Caso &c : casos) {
    scsi = c.scsi, host = c.host, driver = c.driver, resid = c.resid;
    int vistoScsi = -1, vistoHost = -1;
    const bool r = scaler.set3d(true, &vistoScsi, &vistoHost);
    const bool ok = r == c.bien && vistoScsi == c.scsi && vistoHost == c.host && scaler.ultimoResid() == c.resid;
    std::printf("%s status=%d host=%d driver=%d resid=%d -> %s%s%s\n", ok ? "bien " : "MAL  ", c.scsi, c.host, c.driver,
                c.resid, r ? "aceptado" : "fallo", r ? "" : ": ", r ? "" : scaler.lastError().c_str());
    fallos += ok ? 0 : 1;
  }
  std::printf("%d llamadas a SG_IO, %d fallos de la prueba\n", llamadas, fallos);
  return fallos ? 1 : 0;
}
