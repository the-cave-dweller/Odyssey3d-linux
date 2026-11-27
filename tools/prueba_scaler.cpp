/* Test SG_IO status handling without a monitor:
   c++ -std=c++17 -Isrc tools/prueba_scaler.cpp src/scaler.cpp -Wl,--wrap=ioctl -o /tmp/test-scaler
   /tmp/test-scaler */
#include "scaler.h"
#include <scsi/sg.h>
#include <cstdarg>
#include <cstdio>

static int scsi, host, driver, resid;
extern "C" int __wrap_ioctl(int, unsigned long cmd, ...) {
  if (cmd != SG_IO) return -1;
  va_list args;
  va_start(args, cmd);
  auto *h = va_arg(args, sg_io_hdr_t *);
  va_end(args);
  h->status = scsi;
  h->host_status = host;
  h->driver_status = driver;
  h->resid = resid;
  return 0;
}

int main() {
  Scaler scaler;
  if (!scaler.open("/dev/null")) return 1;
  /* A successful syscall does not mean the device acknowledged the command. */
  for (int i = 0; i < 6; ++i) {
    scsi = i == 1 ? 2 : 0;
    host = i == 2 ? 3 : 0;
    driver = i == 3 ? 4 : 0;
    resid = i == 4 ? 512 : (i == 5 ? 1 : 0);
    int reportedScsi = -1, reportedHost = -1;
    if (scaler.set3d(true, &reportedScsi, &reportedHost) != (i == 0) ||
        reportedScsi != scsi || reportedHost != host) {
      std::fprintf(stderr, "unexpected result for status=%d host=%d driver=%d resid=%d\n", scsi, host, driver, resid);
      return 1;
    }
  }
  return 0;
}
