/* bususuario.h — abrir el bus de sesion del usuario aunque el programa vaya en «modo seguro».
   (rama experimental, 26-nov) Con `setcap cap_sys_nice+ep` en el binario (para la prioridad alta de
   la cola del visor en la tarjeta), glibc arranca el proceso en modo seguro (AT_SECURE) y
   sd_bus_open_user lee DBUS_SESSION_BUS_ADDRESS y XDG_RUNTIME_DIR con secure_getenv: le salen
   vacias y falla. MEDIDO: el visor dijo «sin atajos globales (no hay bus del usuario)». Aqui, si falla,
   se abre a mano con la direccion de siempre: el bus es nuestro, del mismo usuario. */
#pragma once

#ifdef HAVE_SDBUS
#include <cstdlib>
#include <string>
#include <systemd/sd-bus.h>

inline int abrirBusUsuario(sd_bus **bus) {
  int r = sd_bus_open_user(bus);
  if (r >= 0) return r;
  std::string dir;
  if (const char *a = std::getenv("DBUS_SESSION_BUS_ADDRESS"); a && *a) dir = a;
  else if (const char *x = std::getenv("XDG_RUNTIME_DIR"); x && *x) dir = std::string("unix:path=") + x + "/bus";
  if (dir.empty()) return r;
  sd_bus *b = nullptr;
  if (sd_bus_new(&b) < 0) return r;
  if (sd_bus_set_address(b, dir.c_str()) < 0 || sd_bus_set_bus_client(b, 1) < 0 || sd_bus_start(b) < 0) {
    sd_bus_unref(b);
    return r;
  }
  *bus = b;
  return 0;
}
#endif
