/* overlay.cpp — cliente de wlr-layer-shell que pinta la tarjeta de aviso del hub por
   encima de cualquier ventana, incluidas las de pantalla completa.

   Dos cosas la hacen posible:
     · la capa OVERLAY del protocolo wlr-layer-shell, que en KWin va por encima de las
       ventanas a pantalla completa (una ventana normal siempre las tapa, y el centro de
       notificaciones de KDE ni se ve);
     · el teclado NO se intercepta, así que el reproductor sigue recibiendo espacio,
       flechas, etc. mientras la tarjeta está puesta.

   El dibujo se hace en la CPU: se reutiliza la misma geometría, el mismo atlas de
   fuentes y los mismos colores que la interfaz Vulkan (vkui.cpp), así que la tarjeta es
   idéntica a la que se ve dentro del visor. Cada cuadro de la interfaz es un rectángulo
   alineado que multiplica su color por una máscara del atlas, así que el rasterizador
   que hace falta son cuatro líneas. */
#include "overlay.h"

#include "glutil.h"
#include "vkui.h"

#include "xdg-output-unstable-v1-client-protocol.h"

#ifdef HAVE_FRACTIONAL_SCALE
#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#endif
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <wayland-client.h>

#include <linux/input-event-codes.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

/* ---------------- implementación ---------------- */

struct Overlay::Impl {
  /* protocolos */
  wl_display *dpy = nullptr;
  wl_registry *registry = nullptr;
  wl_compositor *comp = nullptr;
  wl_shm *shm = nullptr;
  zwlr_layer_shell_v1 *shell = nullptr;
  zxdg_output_manager_v1 *xdgMgr = nullptr;
  wl_seat *seat = nullptr;
  wl_pointer *pointer = nullptr;
#ifdef HAVE_FRACTIONAL_SCALE
  wp_fractional_scale_manager_v1 *escalaMgr = nullptr;
  wp_viewporter *viewporter = nullptr;
  wp_viewport *viewport = nullptr;
  wp_fractional_scale_v1 *escalaSuperf = nullptr;
#endif

  /* monitores que anuncia el compositor */
  struct Monitor {
    wl_output *obj = nullptr;
    uint32_t global = 0;                      /* nombre del global: así se sabe si desaparece */
    int32_t x = 0, y = 0, ancho = 0, alto = 0;   /* en píxeles lógicos */
    int anchoFis = 0, altoFis = 0;               /* en píxeles reales */
    int anchoLog = 0, altoLog = 0;               /* tamaño lógico anunciado por xdg_output */
    zxdg_output_v1 *xdg = nullptr;
    int escala = 1;
    uint32_t transformacion = 0;
    std::string nombre, modelo;
    bool vivo = true;
  };
  std::vector<Monitor *> monitores;
  Monitor *elegido = nullptr;
  wl_output *capaEn = nullptr;   /* monitor para el que se creó la capa actual */
  int ofrecidoAncho = 0;         /* area util que ofrecio el compositor: delata donde cayo */
  int ofrecidoAlto = 0;
  int intentosColocacion = 0;    /* reintentos hechos para colocar la capa bien */
  bool avisoColocacion = false;  /* hay aviso pendiente para quien lleve el servicio */
  bool reconocido = false;       /* el monitor elegido es claramente el panel del Odyssey */
  int anchoE = 0, altoE = 0;      /* tamaño real del monitor elegido, en píxeles */
  double escala = 1.0;            /* 1.0, 1.5, 2.0 ... */

  /* la superficie de la tarjeta */
  wl_surface *surf = nullptr;
  zwlr_layer_surface_v1 *layer = nullptr;
  bool hayConfigure = false, cerrada = false;
  int supAncho = 0, supAlto = 0;  /* tamaño lógico que nos asigna el compositor */
  int bufAncho = 0, bufAlto = 0;  /* mapa en píxeles reales */
  int margenDer = 0, margenAb = 0;
  double ox = 0.0, oy = 0.0;      /* esquina de la tarjeta en píxeles del monitor */
  double escalaGeom = 0.0;        /* escala con la que se calculo el recorte actual */
  bool conexionCaida = false;     /* el compositor cerró la conexión de la capa */

  /* mapas de memoria para los cuadros (doble buffer: uno en pantalla, otro dibujando) */
  struct Bloque {
    wl_shm_pool *pozo = nullptr;
    wl_buffer *buf = nullptr;
    uint8_t *mapa = nullptr;
    size_t tam = 0;
    int fd = -1;
    int ancho = 0, alto = 0;
    bool libre = false;
  };
  std::vector<Bloque *> bloques;
  std::vector<float> acum;        /* RGBA premultiplicado, para mezclar como hace la GPU */

  Hud hud;
  bool atlasListo = false;
  bool tarjetaPuesta = false;
  int golpeVisto = Hud::HitNone;
  double ratonX = -1.0, ratonY = -1.0;
  bool ratonDentro = false;
  std::string accion;
  std::string titulo, cuerpo;
  std::string aviso;
  std::string pistaMonitor;

  ~Impl() {
    limpiaTarjeta();
    liberaBloques(true);
#ifdef HAVE_FRACTIONAL_SCALE
    if (escalaSuperf) wp_fractional_scale_v1_destroy(escalaSuperf);
    if (viewport) wp_viewport_destroy(viewport);
    if (viewporter) wp_viewporter_destroy(viewporter);
    if (escalaMgr) wp_fractional_scale_manager_v1_destroy(escalaMgr);
#endif
    if (surf) wl_surface_destroy(surf);
    if (pointer) wl_pointer_destroy(pointer);
    if (seat) wl_seat_destroy(seat);
    for (Monitor *m : monitores)
      if (m->obj) wl_output_destroy(m->obj);   /* los zxdg_output los destruye el compositor */
    if (shell) zwlr_layer_shell_v1_destroy(shell);
    if (shm) wl_shm_destroy(shm);
    if (comp) wl_compositor_destroy(comp);
    if (registry) wl_registry_destroy(registry);
    if (dpy) wl_display_disconnect(dpy);
    for (Monitor *m : monitores) delete m;
    monitores.clear();
  }

  /* ---------- memoria compartida ---------- */

  static Bloque *nuevoBloque(wl_shm *shm, int w, int h, std::string *err) {
    const int stride = w * 4;
    const size_t tam = (size_t)stride * (size_t)h;
    char plantilla[] = "/dev/shm/odyssey3d-XXXXXX";
    int fd = ::mkstemp(plantilla);
    if (fd < 0) {
      char plantilla2[] = "/tmp/odyssey3d-XXXXXX";
      fd = ::mkstemp(plantilla2);
      if (fd < 0) {
        if (err) *err = "no se pudo crear el mapa de memoria";
        return nullptr;
      }
    }
    if (::ftruncate(fd, (off_t)tam) != 0) {
      ::close(fd);
      if (err) *err = "no se pudo dar tamaño al mapa de memoria";
      return nullptr;
    }
    void *p = ::mmap(nullptr, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
      ::close(fd);
      if (err) *err = "no se pudo mapear la memoria compartida";
      return nullptr;
    }
    wl_shm_pool *pozo = wl_shm_create_pool(shm, fd, (int32_t)tam);
    if (!pozo) {
      ::munmap(p, tam);
      ::close(fd);
      if (err) *err = "el compositor rechazó el charco de memoria compartida";
      return nullptr;
    }
    wl_buffer *b = wl_shm_pool_create_buffer(pozo, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
    if (!b) {
      wl_shm_pool_destroy(pozo);
      ::munmap(p, tam);
      ::close(fd);
      if (err) *err = "el compositor rechazó el lienzo";
      return nullptr;
    }
    Bloque *bl = new Bloque();
    bl->pozo = pozo;
    bl->buf = b;
    bl->mapa = static_cast<uint8_t *>(p);
    bl->tam = tam;
    bl->fd = fd;
    bl->ancho = w;
    bl->alto = h;
    std::memset(bl->mapa, 0, tam);
    wl_buffer_add_listener(b, &bufferListener, bl);
    return bl;
  }

  static void bufferReleased(void *data, struct wl_buffer *buf) {
    Bloque *bl = static_cast<Bloque *>(data);
    if (bl) bl->libre = true;   /* ya la puede reutilizar el siguiente cuadro */
    (void)buf;
  }

  static const struct wl_buffer_listener bufferListener;

  void liberaBloques(bool todos) {
    for (size_t i = 0; i < bloques.size();) {
      Bloque *bl = bloques[i];
      if (!todos && !bl->libre) {
        ++i;
        continue;
      }
      if (bl->buf) wl_buffer_destroy(bl->buf);
      if (bl->pozo) wl_shm_pool_destroy(bl->pozo);
      if (bl->mapa) ::munmap(bl->mapa, bl->tam);
      if (bl->fd >= 0) ::close(bl->fd);
      delete bl;
      bloques.erase(bloques.begin() + (long)i);
    }
  }

  /* ---------- muestreo del atlas de fuentes y máscaras ---------- */

  float muestraAtlas(float u, float v) const {
    const std::vector<uint8_t> &a = hud.atlasPixels();
    const int aw = hud.atlasW(), ah = hud.atlasH();
    if (a.empty() || aw <= 0 || ah <= 0) return 1.0f;
    float fx = u * (float)aw - 0.5f, fy = v * (float)ah - 0.5f;
    int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
    float tx = fx - (float)x0, ty = fy - (float)y0;
    auto tex = [&](int x, int y) -> float {
      x = std::max(0, std::min(aw - 1, x));
      y = std::max(0, std::min(ah - 1, y));
      return (float)a[(size_t)y * (size_t)aw + (size_t)x] / 255.0f;
    };
    const float a00 = tex(x0, y0), a10 = tex(x0 + 1, y0);
    const float a01 = tex(x0, y0 + 1), a11 = tex(x0 + 1, y0 + 1);
    return (a00 * (1.0f - tx) + a10 * tx) * (1.0f - ty) + (a01 * (1.0f - tx) + a11 * tx) * ty;
  }

  /* ---------- dibujo de la tarjeta ---------- */

  /* Alto del escritorio en puntos logicos. El monitor es 3840x2160 fisicos pero con la escala
     del escritorio al 150% el escritorio mide 1440 de alto: la tarjeta se mide ahi, si no sale
     un 50% mas grande que cualquier ventana normal. */
  int regionCuadrada[4] = {-1, -1, -1, -1};  /* la ultima caja de entrada mandada a la capa */

  float altoLogico() const { return (float)altoE / (escala > 0.25 ? escala : 1.0); }

  /* El lienzo de la capa llega hasta el borde de la pantalla y lo que no es tarjeta es
     transparente, pero sin decirle nada al compositor se pica igual: se tragaba los clics y la
     rueda que caian encima. La region de entrada deja la capa pinchable solo sobre la tarjeta; el
     resto del lienzo deja pasar el raton a la ventana de atras. Las coordenadas son logicas del
     lienzo (pixeles reales partido por la escala que anuncio el compositor). */
  void poneRegionEntrada() {
    if (!surf || !comp) return;
    float ax = 0.0f, ay = 0.0f, aw = 0.0f, ah = 0.0f;
    const double e = escala > 0.25 ? escala : 1.0;
    int caja[4] = {0, 0, 0, 0};
    if (tarjetaPuesta && hud.askBounds(&ax, &ay, &aw, &ah)) {
      caja[0] = (int)std::floor((ax - (float)ox) / e);
      caja[1] = (int)std::floor((ay - (float)oy) / e);
      caja[2] = (int)std::ceil(aw / e);
      caja[3] = (int)std::ceil(ah / e);
    }
    if (caja[0] == regionCuadrada[0] && caja[1] == regionCuadrada[1] && caja[2] == regionCuadrada[2] &&
        caja[3] == regionCuadrada[3])
      return;  /* la misma caja que el cuadro anterior: no hay nada que mandar */
    struct wl_region *r = wl_compositor_create_region(comp);
    if (!r) return;
    if (caja[2] > 0 && caja[3] > 0) wl_region_add(r, caja[0], caja[1], caja[2], caja[3]);
    wl_surface_set_input_region(surf, r);  /* caja vacia = aqui no se pica nada: pasa todo */
    wl_region_destroy(r);
    std::printf("[capa] region de entrada: %s\n",
                (caja[2] > 0 && caja[3] > 0) ? "solo la tarjeta" : "ninguna (el aviso no esta puesto)");
    std::fflush(stdout);
    regionCuadrada[0] = caja[0]; regionCuadrada[1] = caja[1];
    regionCuadrada[2] = caja[2]; regionCuadrada[3] = caja[3];
  }

  void pintar() {
    if (bufAncho <= 0 || bufAlto <= 0) return;
    if (supAncho <= 0) supAncho = bufAncho;
    if (supAlto <= 0) supAlto = bufAlto;
    liberaBloques(false);
    /* la disposición es determinista: misma ventana, misma tarjeta, mismos píxeles */
    hud.begin(anchoE, altoE, altoLogico());
    if (tarjetaPuesta && std::fabs(hud.scale() - escalaGeom) > 0.01) {
      /* el compositor anuncio su escala despues de montar la ventana: la tarjeta se dibuja
         de otro tamano y hay que volver a recortar, si no la letra se sale de las cajas */
      std::printf("[capa] la escala del compositor paso de %.2f a %.2f: la tarjeta se reajusta\n",
                  escalaGeom, hud.scale());
      ajustaGeometria();
    }
    if (std::getenv("ODYSSEY3D_DEBUG_UI"))
      std::printf("[depurar capa] al dibujar: hud %.2f | recorte ox=%.0f buf=%dx%d\n",
                  hud.scale(), ox, bufAncho, bufAlto);
    std::vector<HudVertex> vv = hud.vertices();

    const size_t npx = (size_t)bufAncho * (size_t)bufAlto;
    acum.assign(npx * 4u, 0.0f);

    for (size_t i = 0; i + 5 < vv.size(); i += 6) {
      float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
      float u0 = 1e30f, v0 = 1e30f, u1 = -1e30f, v1 = -1e30f;
      for (int k = 0; k < 6; k++) {
        const HudVertex &q = vv[i + (size_t)k];
        x0 = std::min(x0, q.x);
        y0 = std::min(y0, q.y);
        x1 = std::max(x1, q.x);
        y1 = std::max(y1, q.y);
        u0 = std::min(u0, q.u);
        v0 = std::min(v0, q.v);
        u1 = std::max(u1, q.u);
        v1 = std::max(v1, q.v);
      }
      const float r = vv[i].r, g = vv[i].g, b = vv[i].b, al = vv[i].a;
      /* recorte contra la ventana de la tarjeta */
      float ax0 = std::max(0.0f, x0 - (float)ox), ay0 = std::max(0.0f, y0 - (float)oy);
      float ax1 = std::min((float)bufAncho, x1 - (float)ox), ay1 = std::min((float)bufAlto, y1 - (float)oy);
      if (ax1 <= ax0 || ay1 <= ay0) continue;
      if (x1 <= x0 || y1 <= y0) continue;
      const float du = u1 - u0, dv = v1 - v0;
      const float ancho = x1 - x0, alto = y1 - y0;
      const int px0 = (int)std::floor(ax0), py0 = (int)std::floor(ay0);
      const int px1 = std::min(bufAncho, (int)std::ceil(ax1)), py1 = std::min(bufAlto, (int)std::ceil(ay1));
      for (int py = py0; py < py1; py++) {
        const float fy = ((float)py + 0.5f - (y0 - (float)oy)) / alto;
        const float v = v0 + fy * dv;
        float *fila = acum.data() + ((size_t)py * (size_t)bufAncho + (size_t)px0) * 4u;
        for (int px = px0; px < px1; px++) {
          const float fx = ((float)px + 0.5f - (x0 - (float)ox)) / ancho;
          const float m = muestraAtlas(u0 + fx * du, v);
          const float a = al * m;
          if (a <= 0.002f) {
            fila += 4;
            continue;
          }
          const float unoMenos = 1.0f - a;
          fila[0] = r * a + fila[0] * unoMenos;
          fila[1] = g * a + fila[1] * unoMenos;
          fila[2] = b * a + fila[2] * unoMenos;
          fila[3] = a + fila[3] * unoMenos;
          fila += 4;
        }
      }
    }

    /* ARGB8888 sin premultiplicar (así lo pide wl_shm) */
    Bloque *bl = nullptr;
    std::string err;
    for (Bloque *candidato : bloques)
      if (candidato->libre && candidato->ancho == bufAncho && candidato->alto == bufAlto) {
        bl = candidato;
        break;
      }
    if (!bl) {
      /* si cambió el tamaño de la tarjeta, los lienzos viejos ya no sirven */
      liberaBloques(false);   /* sueltos pero del tamaño viejo: fuera */
      bl = nuevoBloque(shm, bufAncho, bufAlto, &err);
      if (!bl) {
        aviso = err;
        return;
      }
      bloques.push_back(bl);
    }
    uint32_t *pinta = reinterpret_cast<uint32_t *>(bl->mapa);
    const float *fuente = acum.data();
    for (size_t p = 0; p < npx; p++) {
      const float A = fuente[p * 4u + 3u];
      if (A <= 0.0001f) {
        pinta[p] = 0u;
        continue;
      }
      const float inv = 1.0f / A;
      auto canal = [&](float c) -> uint32_t {
        float s = c * inv;
        if (s < 0.0f) s = 0.0f;
        if (s > 1.0f) s = 1.0f;
        return (uint32_t)std::lround(s * 255.0f);
      };
      const uint32_t aa = (uint32_t)std::lround(std::min(1.0f, A) * 255.0f);
      pinta[p] = (aa << 24) | (canal(fuente[p * 4u]) << 16) | (canal(fuente[p * 4u + 1u]) << 8) |
                 canal(fuente[p * 4u + 2u]);
    }

    if (surf && !(layer && !hayConfigure)) {
      /* enganchar un lienzo antes de la primera confirmacion de tamano es un error de
         protocolo: el compositor cierra la conexion de la capa y desde ahi la tarjeta ya no
         vuelve a aparecer nunca. Pasaba al rehacer la capa, que es lo que dejaba el aviso
         "tarjeta puesta" en el registro pero nada en la pantalla. */
      poneRegionEntrada();  /* va en el mismo envio que el lienzo */
      wl_surface_attach(surf, bl->buf, 0, 0);
      wl_surface_damage(surf, 0, 0, supAncho, supAlto);
      wl_surface_commit(surf);
    }
  }

  /* ---------- colocación de la tarjeta ---------- */

  /* Recorte del lienzo y tamano de la ventana de la tarjeta, sacados de la disposicion que
     calculo el hud. Se vuelve a llamar al dibujar: el compositor anuncia su escala despues de
     montar la ventana (aqui paso de 1,50 a 1,80) y con la escala nueva la tarjeta se dibuja
     mas grande; si el recorte se queda con la escala vieja, la letra sale cortada por la
     izquierda y se sale de sus cajas. */
  bool ajustaGeometria() {
    float ax = 0, ay = 0, aw = 0, ah = 0;
    if (!hud.askBounds(&ax, &ay, &aw, &ah)) return false;
    const float holgura = 4.0f * (float)std::max(1.0, escala);   /* para el borde suave */
    ax -= holgura;
    ay -= holgura;
    aw += 2.0f * holgura;
    ah += 2.0f * holgura;
    if (ax < 0.0f) {
      aw += ax;
      ax = 0.0f;
    }
    if (ay < 0.0f) {
      ah += ay;
      ay = 0.0f;
    }
    aw = std::min(aw, (float)anchoE - ax);
    ah = std::min(ah, (float)altoE - ay);
    /* El lienzo llega hasta el borde de la pantalla y la separacion con el borde se deja
       DENTRO del lienzo, en zona transparente. Con `set_margin` el compositor multiplica el
       margen por su escala, que no siempre coincide con la nuestra, y el sobrante se lo comia
       a la tarjeta: salia encogida y con la letra fuera de las cajas. */
    const int nAncho = (int)std::lround((float)anchoE - ax);
    const int nAlto = (int)std::lround((float)altoE - ay);
    if (nAncho < 16 || nAlto < 16) return false;
    const bool cambio = (nAncho != bufAncho || nAlto != bufAlto);
    ox = ax;
    oy = ay;
    bufAncho = nAncho;
    bufAlto = nAlto;
    escalaGeom = hud.scale();
    margenDer = margenAb = 0;
    supAncho = aQuiere((double)bufAncho);
    supAlto = aQuiere((double)bufAlto);
    if (layer && cambio) {
      zwlr_layer_surface_v1_set_size(layer, (uint32_t)supAncho, (uint32_t)supAlto);
#ifdef HAVE_FRACTIONAL_SCALE
      if (viewport) wp_viewport_set_destination(viewport, supAncho, supAlto);
#endif
    }
    return true;
  }

  int32_t aQuiere(double pixelesReales) const {
    return (int32_t)std::lround(pixelesReales / std::max(0.25, escala));
  }

  void quitaEscuchaLayer() {
    if (layer) {
      zwlr_layer_surface_v1_destroy(layer);
      layer = nullptr;
      capaEn = nullptr;
    }
    ofrecidoAncho = ofrecidoAlto = 0;
    hayConfigure = false;   /* la capa nueva tiene que esperar su propia confirmacion */
  }

  void limpiaTarjeta() {
    quitaEscuchaLayer();
    if (surf) {
      wl_surface_attach(surf, nullptr, 0, 0);
      wl_surface_commit(surf);
    }
    tarjetaPuesta = false;
  }

  static std::string minusc(std::string v) {
    for (char &c : v) c = (char)std::tolower(c);
    return v;
  }
  /* El panel del Odyssey es el único que puede hacer 3D, así que la tarjeta tiene que
     caer SIEMPRE ahí. Bastaba "el monitor de más píxeles" mientras solo había uno, pero
     con dos pantallas conectadas (o cuando el compositor vuelve a anunciar la salida
     después de cambiar el modo de la lente) eso ponía la tarjeta en el monitor
     equivocado. Estas palabras reconocen el panel; si no cuadran, se arregla con
     pantalla=DP-4 en ~/.config/odyssey3d/serve.conf o con ODYSSEY3D_PANTALLA=DP-4. */
  static bool esPanelOdyssey(const Monitor &m) {
    const std::string n = minusc(m.nombre), d = minusc(m.modelo);
    for (const char *aguja : {"odyssey", "syncmaster", "g90xf", "g90xc", "g90"})
      if (d.find(aguja) != std::string::npos || n.find(aguja) != std::string::npos) return true;
    return false;
  }
  static long areaDe(const Monitor &m) {
    const long a = m.anchoFis > 0 ? m.anchoFis : m.ancho;
    const long b = m.altoFis > 0 ? m.altoFis : m.alto;
    return a * b;
  }

  void eligeMonitor() {
    if (pistaMonitor.empty()) {
      if (const char *e = std::getenv("ODYSSEY3D_PANTALLA"); e && e[0]) pistaMonitor = e;
    }
    Monitor *porPista = nullptr, *porPanel = nullptr, *elGrande = nullptr;
    for (Monitor *m : monitores) {
      if (!m->vivo || !m->obj) continue;
      if (!elGrande || areaDe(*m) > areaDe(*elGrande)) elGrande = m;
      if (esPanelOdyssey(*m) && (!porPanel || areaDe(*m) > areaDe(*porPanel))) porPanel = m;
      if (!pistaMonitor.empty()) {
        const std::string bus = minusc(m->nombre), aguja = minusc(pistaMonitor);
        if (bus.find(aguja) != std::string::npos || minusc(m->modelo).find(aguja) != std::string::npos)
          if (!porPista || areaDe(*m) > areaDe(*porPista)) porPista = m;
      }
    }
    /* la pista manda; si no coincide con nada (la pantalla se acaba de anunciar y todavia
       no trae nombre) se sigue con el panel Odyssey o con el de mas pixeles, y nunca con el
       primero de la lista: asi era como la tarjeta se colaba en el otro monitor */
    elegido = porPista ? porPista : (porPanel ? porPanel : elGrande);
    reconocido = porPista || porPanel;
    if (!elegido && !monitores.empty()) {
      for (Monitor *m : monitores)
        if (m->obj) { elegido = m; break; }
    }
    if (elegido) {
      anchoE = elegido->anchoFis > 0 ? elegido->anchoFis : elegido->ancho;
      altoE = elegido->altoFis > 0 ? elegido->altoFis : elegido->alto;
      /* La escala real es píxeles reales entre píxeles lógicos: con el escritorio al
         150 % el wl_output anuncia la entera de arriba (2) y todo saldría descuadrado. */
      escala = 0.0;
      if (elegido->anchoLog > 0 && elegido->anchoFis > 0)
        escala = (double)elegido->anchoFis / (double)elegido->anchoLog;
      if (escala <= 0.25) escala = (double)std::max(1, elegido->escala);
    }
  }

  /* un error de protocolo (tamano o lienzo mal puestos) cierra la conexion del compositor:
     sin avisar de esto la tarjeta "se pone" segun el registro pero no vuelve a aparecer
     jamas, que es exactamente lo que pasaba cuando la tarjeta dejaba de salir */
  void marcaConexionCaida() {
    if (conexionCaida) return;
    conexionCaida = true;
    aviso = "el compositor cerró la conexión de la capa superpuesta";
  }

  int esperar(int ms) {
    if (!dpy) return -1;
    if (wl_display_get_error(dpy) != 0) {
      marcaConexionCaida();
      return -1;
    }
    while (wl_display_prepare_read(dpy) != 0) wl_display_dispatch_pending(dpy);
    wl_display_flush(dpy);
    struct pollfd p{wl_display_get_fd(dpy), POLLIN, 0};
    const int n = ::poll(&p, 1, ms);
    if (n > 0) {
      wl_display_read_events(dpy);
      return wl_display_dispatch_pending(dpy);
    }
    wl_display_cancel_read(dpy);
    return 0;
  }

  /* ---------- escuchas ---------- */

  static void registryGlobal(void *data, wl_registry *reg, uint32_t name, const char *iface,
                             uint32_t version) {
    Impl *s = static_cast<Impl *>(data);
    if (!std::strcmp(iface, wl_compositor_interface.name))
      s->comp = static_cast<wl_compositor *>(
          wl_registry_bind(reg, name, &wl_compositor_interface, std::min<uint32_t>(version, 4)));
    else if (!std::strcmp(iface, wl_shm_interface.name))
      s->shm = static_cast<wl_shm *>(wl_registry_bind(reg, name, &wl_shm_interface, std::min<uint32_t>(version, 1)));
    else if (!std::strcmp(iface, zwlr_layer_shell_v1_interface.name))
      s->shell = static_cast<zwlr_layer_shell_v1 *>(
          wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, std::min<uint32_t>(version, 1)));
    else if (!std::strcmp(iface, wl_seat_interface.name)) {
      s->seat = static_cast<wl_seat *>(wl_registry_bind(reg, name, &wl_seat_interface, std::min<uint32_t>(version, 6)));
      if (s->seat) {
        s->pointer = wl_seat_get_pointer(s->seat);
        if (s->pointer) wl_pointer_add_listener(s->pointer, &s->pointerListener, s);
      }
    } else if (!std::strcmp(iface, zxdg_output_manager_v1_interface.name)) {
      s->xdgMgr = static_cast<zxdg_output_manager_v1 *>(
          wl_registry_bind(reg, name, &zxdg_output_manager_v1_interface, std::min<uint32_t>(version, 2)));
    } else if (!std::strcmp(iface, wl_output_interface.name)) {
      Monitor *m = new Monitor();
      m->global = name;
      m->obj = static_cast<wl_output *>(
          wl_registry_bind(reg, name, &wl_output_interface, std::min<uint32_t>(version, 4)));
      if (m->obj) wl_output_add_listener(m->obj, &s->outputListener, m);
      /* una pantalla anunciada tardé (otra conectada, un cambio de modo de la lente):
         su tamaño lógico se pide ahora mismo, si no la tarjeta saldría a la escala mala */
      if (m->obj && s->xdgMgr && !m->xdg) {
        m->xdg = zxdg_output_manager_v1_get_xdg_output(s->xdgMgr, m->obj);
        if (m->xdg) zxdg_output_v1_add_listener(m->xdg, &s->xdgListener, m);
      }
      s->monitores.push_back(m);
    }
#ifdef HAVE_FRACTIONAL_SCALE
    else if (!std::strcmp(iface, wp_fractional_scale_manager_v1_interface.name))
      s->escalaMgr = static_cast<wp_fractional_scale_manager_v1 *>(wl_registry_bind(
          reg, name, &wp_fractional_scale_manager_v1_interface, std::min<uint32_t>(version, 1)));
    else if (!std::strcmp(iface, wp_viewporter_interface.name))
      s->viewporter = static_cast<wp_viewporter *>(
          wl_registry_bind(reg, name, &wp_viewporter_interface, std::min<uint32_t>(version, 1)));
#endif
  }
  static void registryGlobalRemove(void *data, wl_registry *, uint32_t name) {
    Impl *s = static_cast<Impl *>(data);
    for (Monitor *m : s->monitores) {
      if (m->global != name) continue;
      /* el compositor ya destruyó el objeto del lado del servidor: el cliente tiene que
         soltar el suyo y dejar de contar esa pantalla */
      m->vivo = false;
      const bool capaAqui = s->capaEn && m->obj && s->capaEn == m->obj;
      if (m->obj) { wl_output_destroy(m->obj); m->obj = nullptr; }
      if (s->elegido == m) s->elegido = nullptr;
      if (capaAqui) {
        /* la capa estaba puesta en esa pantalla: se quita y en el siguiente aviso se
           pondrá en la que siga en pie */
        s->quitaEscuchaLayer();
        if (s->layer) { zwlr_layer_surface_v1_destroy(s->layer); s->layer = nullptr; }
        s->capaEn = nullptr;
        s->tarjetaPuesta = false;
      }
      return;
    }
  }

  static const struct wl_registry_listener registryListener;

  static void outGeometry(void *data, wl_output *, int32_t x, int32_t y, int32_t, int32_t, int32_t,
                          const char *, const char *, int32_t transform) {
    Monitor *m = static_cast<Monitor *>(data);
    m->x = x;
    m->y = y;
    m->transformacion = (uint32_t)transform;
  }
  static void outMode(void *data, wl_output *, uint32_t, int32_t w, int32_t h, int32_t) {
    Monitor *m = static_cast<Monitor *>(data);
    if ((long)w * (long)h > (long)m->anchoFis * (long)m->altoFis) {
      m->anchoFis = (int)w;
      m->altoFis = (int)h;
    }
  }
  static void outDone(void *data, wl_output *) {
    Monitor *m = static_cast<Monitor *>(data);
    m->vivo = true;
  }
  static void outScale(void *data, wl_output *, int32_t f) {
    Monitor *m = static_cast<Monitor *>(data);
    m->escala = f > 0 ? (int)f : 1;
  }
  static void outName(void *data, wl_output *, const char *nombre) {
    Monitor *m = static_cast<Monitor *>(data);
    if (nombre) m->nombre = nombre;
  }
  static void outDescription(void *data, wl_output *, const char *desc) {
    Monitor *m = static_cast<Monitor *>(data);
    if (desc) m->modelo = desc;
  }

  static void xdgPosicion(void *data, zxdg_output_v1 *, int32_t x, int32_t y) {
    Monitor *m = static_cast<Monitor *>(data);
    m->x = x;
    m->y = y;
  }
  static void xdgTamano(void *data, zxdg_output_v1 *, int32_t w, int32_t h) {
    Monitor *m = static_cast<Monitor *>(data);
    m->anchoLog = (int)w;
    m->altoLog = (int)h;
  }
  static void xdgNombre(void *data, zxdg_output_v1 *, const char *nombre) {
    Monitor *m = static_cast<Monitor *>(data);
    if (nombre && m->nombre.empty()) m->nombre = nombre;
  }
  static void xdgDescripcion(void *data, zxdg_output_v1 *, const char *desc) {
    Monitor *m = static_cast<Monitor *>(data);
    if (desc && m->modelo.empty()) m->modelo = desc;
  }
  static void xdgTermina(void *data, zxdg_output_v1 *) {
    /* ya llegaron todos los datos de este monitor */
    Monitor *m = static_cast<Monitor *>(data);
    (void)m;
  }

  static const struct zxdg_output_v1_listener xdgListener;
  static const struct wl_output_listener outputListener;

  static void layerConfigure(void *data, zwlr_layer_surface_v1 *, uint32_t serial, uint32_t w, uint32_t h) {
    Impl *s = static_cast<Impl *>(data);
    s->ofrecidoAncho = (int)w;
    s->ofrecidoAlto = (int)h;
    /* Lo que ofrece el compositor es el area util de la pantalla donde puso la capa, no el
       tamano de la tarjeta, y ademas lo da en SU escala (aqui ofrecio 668x224 para una
       ventana nuestra de 800x267: los mismos pixeles contados con escala 1,80 en vez de
       1,50). Recortarse a esa cifra encogia la tarjeta y la letra salia fuera de sus cajas,
       asi que solo se guarda como huella de la pantalla y no se toca el tamano pedido. */
    (void)w;
    (void)h;
    s->hayConfigure = true;
    zwlr_layer_surface_v1_ack_configure(s->layer, serial);
#ifdef HAVE_FRACTIONAL_SCALE
    if (s->viewport && s->supAncho > 0 && s->supAlto > 0)
      wp_viewport_set_destination(s->viewport, s->supAncho, s->supAlto);
#endif
  }
  static void layerClosed(void *data, zwlr_layer_surface_v1 *) {
    Impl *s = static_cast<Impl *>(data);
    s->layer = nullptr;   /* el compositor ya lo destruyó */
    s->tarjetaPuesta = false;
    s->cerrada = true;
  }

  static const struct zwlr_layer_surface_v1_listener layerListener;

#ifdef HAVE_FRACTIONAL_SCALE
  static void escalaSuperficie(void *data, wp_fractional_scale_v1 *, uint32_t nueva) {
    Impl *s = static_cast<Impl *>(data);
    s->escala = (double)nueva / 100.0;
  }
  static const struct wp_fractional_scale_v1_listener escalaListener;
#endif

  static void ptrEnter(void *data, wl_pointer *, uint32_t, wl_surface *, wl_fixed_t sx, wl_fixed_t sy) {
    Impl *s = static_cast<Impl *>(data);
    s->ratonDentro = true;
    s->ratonX = s->ox + wl_fixed_to_double(sx) * s->escala;
    s->ratonY = s->oy + wl_fixed_to_double(sy) * s->escala;
    s->hud.setMouse((float)s->ratonX, (float)s->ratonY);
  }
  static void ptrLeave(void *data, wl_pointer *, uint32_t, wl_surface *) {
    Impl *s = static_cast<Impl *>(data);
    s->ratonDentro = false;
    s->ratonX = s->ratonY = -1.0;
    s->hud.setMouse(-1.0f, -1.0f);
    if (s->tarjetaPuesta && s->golpeVisto != Hud::HitNone) {
      s->golpeVisto = Hud::HitNone;
      s->pintar();
    }
  }
  static void ptrMotion(void *data, wl_pointer *, uint32_t, wl_fixed_t sx, wl_fixed_t sy) {
    Impl *s = static_cast<Impl *>(data);
    if (!s->tarjetaPuesta) return;
    s->ratonX = s->ox + wl_fixed_to_double(sx) * s->escala;
    s->ratonY = s->oy + wl_fixed_to_double(sy) * s->escala;
    s->hud.setMouse((float)s->ratonX, (float)s->ratonY);
    /* sólo se redibuja si cambia el botón que hay bajo el ratón */
    const Hud::Hit h = s->hud.hit((float)s->ratonX, (float)s->ratonY);
    if (h != s->golpeVisto) {
      s->golpeVisto = h;
      s->pintar();
    }
  }
  static void ptrButton(void *data, wl_pointer *, uint32_t, uint32_t, uint32_t boton, uint32_t estado) {
    Impl *s = static_cast<Impl *>(data);
    if (boton != BTN_LEFT || estado != WL_POINTER_BUTTON_STATE_PRESSED || !s->tarjetaPuesta) return;
    const Hud::Hit h = s->hud.hit((float)s->ratonX, (float)s->ratonY);
    switch (h) {
      case Hud::HitAskSbs: s->accion = "sbs"; break;
      case Hud::HitAskTop: s->accion = "tb"; break;
      case Hud::HitAskFlat: s->accion = "flat"; break;
      case Hud::HitAskConv: s->accion = "conv"; break;
      case Hud::HitAskClose: s->accion = "close"; break;
      case Hud::HitAskDont:
        s->hud.state().remember = !s->hud.state().remember;
        s->pintar();
        break;
      default: break;
    }
  }
  static void ptrAxis(void *, wl_pointer *, uint32_t, uint32_t, wl_fixed_t) {}
  static void ptrFrame(void *, wl_pointer *) {}
  static void ptrAxisSource(void *, wl_pointer *, uint32_t) {}
  static void ptrAxisStop(void *, wl_pointer *, uint32_t, uint32_t) {}
  static void ptrAxisDiscrete(void *, wl_pointer *, uint32_t, int32_t) {}

  static const struct wl_pointer_listener pointerListener;
};

const struct wl_registry_listener Overlay::Impl::registryListener = {
    Impl::registryGlobal, Impl::registryGlobalRemove};

const struct zxdg_output_v1_listener Overlay::Impl::xdgListener = {
    Impl::xdgPosicion, Impl::xdgTamano, Impl::xdgTermina, Impl::xdgNombre, Impl::xdgDescripcion};

const struct wl_output_listener Overlay::Impl::outputListener = {
    Impl::outGeometry, Impl::outMode, Impl::outDone, Impl::outScale, Impl::outName, Impl::outDescription};

const struct zwlr_layer_surface_v1_listener Overlay::Impl::layerListener = {Impl::layerConfigure,
                                                                            Impl::layerClosed};

const struct wl_pointer_listener Overlay::Impl::pointerListener = {
    Impl::ptrEnter, Impl::ptrLeave, Impl::ptrMotion, Impl::ptrButton, Impl::ptrAxis,
    Impl::ptrFrame, Impl::ptrAxisSource, Impl::ptrAxisStop, Impl::ptrAxisDiscrete};

const struct wl_buffer_listener Overlay::Impl::bufferListener = {Impl::bufferReleased};

#ifdef HAVE_FRACTIONAL_SCALE
const struct wp_fractional_scale_v1_listener Overlay::Impl::escalaListener = {Impl::escalaSuperficie};
#endif

/* ---------------- API pública ---------------- */

Overlay::Overlay() : d(new Impl()) {}

Overlay::~Overlay() = default;

bool Overlay::open(std::string *err) {
  d->dpy = wl_display_connect(nullptr);
  if (!d->dpy) {
    if (err) *err = "no hay sesión de Wayland a la que conectarse (WAYLAND_DISPLAY)";
    return false;
  }
  d->registry = wl_display_get_registry(d->dpy);
  wl_registry_add_listener(d->registry, &Impl::registryListener, d.get());
  wl_display_roundtrip(d->dpy);
  /* los monitores ya están enlazados: pedimos su tamaño lógico ahora, así no depende
     del orden en que el compositor anuncia los protocolos */
  if (d->xdgMgr) {
    for (Impl::Monitor *m : d->monitores)
      if (m->obj && !m->xdg) {
        m->xdg = zxdg_output_manager_v1_get_xdg_output(d->xdgMgr, m->obj);
        if (m->xdg) zxdg_output_v1_add_listener(m->xdg, &Impl::xdgListener, m);
      }
  }
  wl_display_roundtrip(d->dpy);
  wl_display_roundtrip(d->dpy);   /* los detalles de cada monitor llegan en varias rondas */
  d->eligeMonitor();

  if (!d->shell || !d->comp || !d->shm) {
    if (err) {
      std::string falta;
      if (!d->shell) falta += " wlr-layer-shell";
      if (!d->comp) falta += " compositor";
      if (!d->shm) falta += " memoria compartida";
      *err = "el compositor no ofrece:" + falta;
    }
    return false;
  }
  if (d->anchoE <= 0 || d->altoE <= 0) {
    if (err) *err = "no se conoció el tamaño de ningún monitor";
    return false;
  }
  d->surf = wl_compositor_create_surface(d->comp);
#ifdef HAVE_FRACTIONAL_SCALE
  if (d->escalaMgr) {
    d->escalaSuperf = wp_fractional_scale_manager_v1_get_fractional_scale(d->escalaMgr, d->surf);
    if (d->escalaSuperf) wp_fractional_scale_v1_add_listener(d->escalaSuperf, &Impl::escalaListener, d.get());
  }
  if (d->viewporter) d->viewport = wp_viewporter_get_viewport(d->viewporter, d->surf);
  wl_display_roundtrip(d->dpy);
#endif
  std::string e;
  d->atlasListo = d->hud.buildAtlas(&e);
  if (!d->atlasListo && err) *err = "la interfaz saldrá sin texto: " + e;
  return true;
}

void Overlay::setPantalla(const std::string &pista) {
  if (!d) return;
  d->pistaMonitor = pista;
  d->eligeMonitor();
}

bool Overlay::pantallaReconocida() const { return d && d->reconocido; }

bool Overlay::available() const { return d && d->shell && d->comp && d->shm && d->surf; }

int Overlay::fd() const { return d->dpy ? wl_display_get_fd(d->dpy) : -1; }

int Overlay::dispatchPending() {
  if (!d->dpy) return -1;
  const int r = wl_display_dispatch_pending(d->dpy);
  if (wl_display_get_error(d->dpy) != 0) d->marcaConexionCaida();
  return r;
}

int Overlay::waitAndDispatch(int ms) {
  if (!d->dpy) return -1;
  while (wl_display_prepare_read(d->dpy) != 0) wl_display_dispatch_pending(d->dpy);
  wl_display_flush(d->dpy);
  struct pollfd p{wl_display_get_fd(d->dpy), POLLIN, 0};
  int n = ::poll(&p, 1, ms);
  if (n > 0) {
    wl_display_read_events(d->dpy);
    const int r = wl_display_dispatch_pending(d->dpy);
    if (wl_display_get_error(d->dpy) != 0) d->marcaConexionCaida();
    return r;
  }
  wl_display_cancel_read(d->dpy);
  return 0;
}

void Overlay::showAsk(const std::string &titulo, const std::string &cuerpo) {
  if (!available()) return;
  Impl &s = *d;
  s.titulo = titulo;
  s.cuerpo = cuerpo;
  s.hud.state().ask = true;
  s.hud.state().overlayOn = false;
  s.hud.setAskText(s.titulo, s.cuerpo);
  s.hud.begin(s.anchoE, s.altoE, s.altoLogico());

  /* El escritorio puede haber cambiado desde que arrancó el servicio (otra pantalla
     conectada, el modo 3D de la lente, una suspensión): se vuelve a elegir el monitor
     y, si la capa estaba puesta en otro, se rehace para que la tarjeta no acabe en la
     pantalla equivocada. */
  s.eligeMonitor();
  for (int esperarNombre = 0; esperarNombre < 4 && s.elegido &&
                               (s.elegido->nombre.empty() || s.elegido->anchoFis <= 0); ++esperarNombre) {
    /* la pantalla se acaba de anunciar y todavia no trae nombre ni modos: sin esos datos la
       eleccion sale a ciegas y puede caer en el monitor equivocado */
    s.esperar(60);
    s.eligeMonitor();
  }
  if (s.layer && s.capaEn != (s.elegido ? s.elegido->obj : nullptr))
    s.quitaEscuchaLayer();   /* destruye la capa: se volvera a crear sobre el monitor bueno */

  s.hud.begin(s.anchoE, s.altoE, s.altoLogico());   /* por si el monitor cambio de tamano */
  if (!s.ajustaGeometria()) return;
  if (std::getenv("ODYSSEY3D_DEBUG_UI"))
    std::printf("[depurar capa] lienzo %dx%d escala %.2f hud %.2f | recorte ox=%.0f oy=%.0f buf=%dx%d | ventana %dx%d\n",
                s.anchoE, s.altoE, s.escala, s.hud.scale(), s.ox, s.oy, s.bufAncho, s.bufAlto,
                s.supAncho, s.supAlto);

  if (!s.layer) {
    s.layer = zwlr_layer_shell_v1_get_layer_surface(s.shell, s.surf, s.elegido ? s.elegido->obj : nullptr,
                                                    ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "odyssey3d");
    if (!s.layer) {
      d->aviso = "el compositor rechazó la capa superpuesta";
      return;
    }
    zwlr_layer_surface_v1_add_listener(s.layer, &Impl::layerListener, d.get());
    s.capaEn = s.elegido ? s.elegido->obj : nullptr;
  }
  zwlr_layer_surface_v1_set_anchor(s.layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                                ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
  zwlr_layer_surface_v1_set_margin(s.layer, 0, 0, 0, 0);   /* la separacion vive dentro del lienzo */
  zwlr_layer_surface_v1_set_exclusive_zone(s.layer, -1);
  zwlr_layer_surface_v1_set_size(s.layer, (uint32_t)s.supAncho, (uint32_t)s.supAlto);
#ifdef HAVE_FRACTIONAL_SCALE
  if (s.viewport) wp_viewport_set_destination(s.viewport, s.supAncho, s.supAlto);
#endif
  s.hayConfigure = false;
  wl_surface_commit(s.surf);

  /* el compositor tiene que confirmar el tamaño antes de poner el lienzo encima */
  for (int intento = 0; intento < 100 && !s.hayConfigure && !s.cerrada; ++intento)
    if (s.esperar(50) < 0) break;
  s.tarjetaPuesta = true;
  s.cerrada = false;
  s.pintar();
}

void Overlay::hideAsk() {
  if (!d) return;
  d->limpiaTarjeta();
}

bool Overlay::askVisible() const { return d->tarjetaPuesta; }

bool Overlay::conexionCaida() const { return d && d->conexionCaida; }

bool Overlay::remember() const { return d && d->hud.state().remember; }

void Overlay::setRemember(bool v) {
  if (!d) return;
  d->hud.state().remember = v;
  if (d->tarjetaPuesta) d->pintar();
}

std::string Overlay::takeAction() {
  std::string out = d->accion;
  d->accion.clear();
  return out;
}

bool Overlay::dumpPng(const std::string &ruta, std::string *err) {
  if (!d->tarjetaPuesta || d->bufAncho <= 0 || d->bufAlto <= 0) {
    if (err) *err = "no hay nada dibujado que volcar";
    return false;
  }
  const uint32_t *pinta = nullptr;
  for (Impl::Bloque *b : d->bloques)
    if (b->buf) {
      pinta = reinterpret_cast<const uint32_t *>(b->mapa);
      break;
    }
  if (!pinta) {
    if (err) *err = "no hay lienzo que volcar";
    return false;
  }
  std::vector<uint8_t> rgba((size_t)d->bufAncho * (size_t)d->bufAlto * 4u);
  for (size_t p = 0; p < rgba.size() / 4u; p++) {
    const uint32_t v = pinta[p];
    rgba[p * 4u + 0u] = (uint8_t)((v >> 16) & 0xFF);
    rgba[p * 4u + 1u] = (uint8_t)((v >> 8) & 0xFF);
    rgba[p * 4u + 2u] = (uint8_t)(v & 0xFF);
    rgba[p * 4u + 3u] = (uint8_t)((v >> 24) & 0xFF);
  }
  if (!glPngSave(ruta, d->bufAncho, d->bufAlto, rgba.data())) {
    if (err) *err = "no se pudo escribir " + ruta;
    return false;
  }
  return true;
}

std::vector<std::string> Overlay::pantallas() const {
  std::vector<std::string> out;
  if (!d) return out;
  for (const Impl::Monitor *m : d->monitores) {
    if (!m->vivo || !m->obj) continue;
    std::string linea = m->nombre.empty() ? "(sin nombre)" : m->nombre;
    if (!m->modelo.empty()) linea += " (" + m->modelo + ")";
    linea += " ";
    linea += std::to_string(m->anchoFis > 0 ? m->anchoFis : m->ancho);
    linea += "x";
    linea += std::to_string(m->altoFis > 0 ? m->altoFis : m->alto);
    out.push_back(linea);
  }
  return out;
}

/* El compositor manda en el configure el area util de la pantalla donde puso la capa. Si esa
   area es claramente MAS grande que la ventana que pedimos, la capa esta en otra pantalla: se
   rehace sobre la pantalla pedida (dos intentos como mucho) y se guarda el motivo. */
bool Overlay::corrigeColocacion() {
  if (!d || !d->layer || !d->tarjetaPuesta || d->intentosColocacion >= 2 || !d->elegido) return false;
  if (d->ofrecidoAncho <= 0 || d->ofrecidoAlto <= 0 || d->ofrecidoAncho > 8192) return false;
  /* Una oferta MAS GRANDE que la tarjeta pedida es la huella del area util de otra pantalla.
     Una mas pequena es el compositor ajustando margenes o contando en su propia escala: eso
     no significa que la capa este en otro monitor. */
  if (d->ofrecidoAncho <= d->supAncho + 8 && d->ofrecidoAlto <= d->supAlto + 8) return false;
  const int intentos = ++d->intentosColocacion;
  char texto[240];
  std::snprintf(texto, sizeof texto,
                "el compositor puso la capa en otra pantalla: ofrecio %dx%d y la tarjeta pide "
                "%dx%d para %s; se intenta de nuevo (%d de 2)",
                d->ofrecidoAncho, d->ofrecidoAlto, d->supAncho, d->supAlto,
                d->elegido->nombre.c_str(), intentos);
  d->aviso = texto;
  err_ = texto;
  d->avisoColocacion = true;
  const std::string titulo = d->titulo, cuerpo = d->cuerpo;
  d->quitaEscuchaLayer();
  showAsk(titulo, cuerpo);
  d->intentosColocacion = intentos;
  return true;
}

bool Overlay::hayAvisoColocacion() {
  if (!d || !d->avisoColocacion) return false;
  d->avisoColocacion = false;
  return true;
}

std::string Overlay::outputName() const {
  if (!d->elegido) return "ninguno";
  std::string out = d->elegido->nombre;
  if (!d->elegido->modelo.empty()) out += " (" + d->elegido->modelo + ")";
  out += " " + std::to_string(d->anchoE) + "x" + std::to_string(d->altoE);
  return out;
}

double Overlay::scale() const { return d ? d->escala : 1.0; }
