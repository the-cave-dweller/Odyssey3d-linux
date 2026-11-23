/* lista los globales que anuncia el compositor Wayland (para saber qué extensiones
   se pueden usar: captura de ventana, layer-shell, foreign toplevel, etc.) */
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

static void on_global(void *data, struct wl_registry *reg, uint32_t id, const char *iface, uint32_t ver) {
  (void)data;
  printf("%-46s version %u\n", iface, ver);
  (void)iface;
}
static const struct wl_registry_listener rl = {on_global, NULL};

int main(void) {
  struct wl_display *d = wl_display_connect(NULL);
  if (!d) { fprintf(stderr, "no se pudo conectar al compositor\n"); return 1; }
  struct wl_registry *r = wl_display_get_registry(d);
  wl_registry_add_listener(r, &rl, NULL);
  wl_display_roundtrip(d);
  wl_display_roundtrip(d);
  wl_registry_destroy(r);
  wl_display_disconnect(d);
  return 0;
}
