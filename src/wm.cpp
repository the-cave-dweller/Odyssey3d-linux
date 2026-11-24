#include "wm.h"

#include "plasma-window-management-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#include <wayland-client.h>

#include <algorithm>
#include <cstring>
#include <poll.h>

/* ---------------- utilidades de estado ---------------- */

const char *WindowInfo::stateName(uint32_t s) {
  switch (s) {
    case Active: return "activa";
    case Minimized: return "minimizada";
    case Maximized: return "maximizada";
    case Fullscreen: return "pantalla completa";
    case KeepAbove: return "encima";
    case KeepBelow: return "debajo";
    case OnAllDesktops: return "todos los escritorios";
    case DemandsAttention: return "pide atencion";
    case SkipTaskbar: return "sin barra de tareas";
    case ExcludeFromCapture: return "excluida de captura";
    default: return nullptr;
  }
}

std::string WindowInfo::stateList(uint32_t s) {
  static const uint32_t kAll[] = {Active,   Minimized, Maximized,  Fullscreen,   KeepAbove,
                                  KeepBelow, OnAllDesktops, DemandsAttention, SkipTaskbar,
                                  ExcludeFromCapture};
  std::string out;
  for (uint32_t f : kAll)
    if (s & f) {
      if (!out.empty()) out += ", ";
      out += stateName(f);
    }
  if (out.empty()) out = "ninguno";
  return out;
}

/* ---------------- implementación ---------------- */

struct WindowMonitor::Impl {
  struct Entry {
    org_kde_plasma_window *obj = nullptr;
    WindowInfo info;
  };

  wl_display *dpy = nullptr;
  wl_registry *registry = nullptr;
  org_kde_plasma_window_management *mgr = nullptr;
  std::vector<Entry *> entries; /* en orden de aparición */
  std::function<void()> onChange;
  std::string err;

  /* Salidas del compositor. El nombre ("DP-4") no lo da wl_output: lo pide xdg_output, y con el
     wl_output en la mano podemos mandarle una ventana a ESA salida. */
  struct Out {
    wl_output *obj = nullptr;
    zxdg_output_v1 *xdg = nullptr;
    std::string name;
    int x = 0, y = 0;         /* sitio en el espacio global del compositor */
    int ancho = 0, alto = 0;  /* tamano logico (con la escala del escritorio aplicada) */
  };
  std::vector<Out> outs;
  zxdg_output_manager_v1 *xdgMgr = nullptr;

  Out *outByName(const std::string &nombre) {
    for (Out &o : outs)
      if (o.name == nombre) return &o;
    return nullptr;
  }
  Out *outByObject(wl_output *o) {
    for (Out &m : outs)
      if (m.obj == o) return &m;
    return nullptr;
  }
  /* Enlazamos xdg_output sobre una salida ya enlazada (llega en cualquier orden). */
  void etiquetaSalida(Out &o) {
    if (o.xdg || !xdgMgr || !o.obj) return;
    o.xdg = zxdg_output_manager_v1_get_xdg_output(xdgMgr, o.obj);
    if (o.xdg) zxdg_output_v1_add_listener(o.xdg, &Impl::xdgListener, this);
  }

  ~Impl() {
    for (Entry *e : entries)
      if (e->obj) org_kde_plasma_window_destroy(e->obj);
    for (Entry *e : entries) delete e;
    /* los zxdg_output los retira el compositor al cerrar la conexión */
    for (Out &o : outs)
      if (o.obj) wl_output_destroy(o.obj);
    if (xdgMgr) zxdg_output_manager_v1_destroy(xdgMgr);
    if (mgr) org_kde_plasma_window_management_destroy(mgr);
    if (registry) wl_registry_destroy(registry);
    if (dpy) wl_display_disconnect(dpy);
  }

  Entry *byObject(org_kde_plasma_window *o) {
    for (Entry *e : entries)
      if (e->obj == o) return e;
    return nullptr;
  }
  Entry *byUuid(const std::string &uuid) {
    for (Entry *e : entries)
      if (e->info.uuid == uuid) return e;
    return nullptr;
  }
  Entry *byInternalId(uint32_t id) {
    for (Entry *e : entries)
      if (e->info.id == id) return e;
    return nullptr;
  }

  /* El compositor avisa de cada ventana con un id interno; el objeto que da detalles
     hay que pedirlo con get_window / get_window_by_uuid. KWin manda las dos variantes,
     así que si ya teníamos la ventana nos quedamos con la primera y tiramos el duplicado. */
  void track(org_kde_plasma_window *obj, uint32_t id, const char *uuid) {
    if (!obj) return;
    if (Entry *e = byInternalId(id)) {
      org_kde_plasma_window_destroy(obj);
      if (uuid && e->info.uuid.empty()) e->info.uuid = uuid;
      return;
    }
    Entry *e = new Entry();
    e->obj = obj;
    e->info.id = id;
    if (uuid) e->info.uuid = uuid;
    entries.push_back(e);
    org_kde_plasma_window_add_listener(obj, &windowListener, this);
    changed();
  }

  void drop(org_kde_plasma_window *obj) {
    Entry *e = byObject(obj);
    if (!e) return;
    org_kde_plasma_window_destroy(e->obj);
    e->obj = nullptr;
    entries.erase(std::find(entries.begin(), entries.end(), e));
    delete e;
    changed();
  }
  void changed() { if (onChange) onChange(); }

  /* ------- escuchas del gestor ------- */
  static void mgrShowDesktop(void *, org_kde_plasma_window_management *, uint32_t) {}
  static void mgrStacking(void *, org_kde_plasma_window_management *, struct wl_array *) {}
  static void mgrStackingUuid(void *, org_kde_plasma_window_management *, const char *) {}
  static void mgrStacking2(void *, org_kde_plasma_window_management *) {}
  static void mgrWindow(void *data, org_kde_plasma_window_management *mgr, uint32_t id) {
    Impl *s = static_cast<Impl *>(data);
    if (org_kde_plasma_window *obj = org_kde_plasma_window_management_get_window(mgr, id))
      s->track(obj, id, nullptr);
  }
  static void mgrWindowWithUuid(void *data, org_kde_plasma_window_management *mgr, uint32_t id,
                                const char *uuid) {
    Impl *s = static_cast<Impl *>(data);
    if (org_kde_plasma_window *obj = org_kde_plasma_window_management_get_window_by_uuid(mgr, uuid))
      s->track(obj, id, uuid);
  }

  /* ------- orden de apilamiento (respuesta a get_stacking_order) ------- */
  struct Stack {
    std::vector<std::string> *uuids = nullptr;
    bool done = false;
  };
  static void stackWindow(void *data, org_kde_plasma_stacking_order *, const char *uuid) {
    Stack *s = static_cast<Stack *>(data);
    if (s && s->uuids && uuid) s->uuids->push_back(uuid);
  }
  static void stackDone(void *data, org_kde_plasma_stacking_order *) {
    Stack *s = static_cast<Stack *>(data);
    if (s) s->done = true;
  }
  static const org_kde_plasma_stacking_order_listener stackListener;

  /* ------- nombre de cada salida ------- */
  static void xdgNombre(void *data, zxdg_output_v1 *obj, const char *nombre) {
    Impl *s = static_cast<Impl *>(data);
    for (Out &o : s->outs)
      if (o.xdg == obj) {
        o.name = nombre ? nombre : "";
        return;
      }
  }
  static void xdgPosicion(void *data, zxdg_output_v1 *obj, int32_t x, int32_t y) {
    Impl *s = static_cast<Impl *>(data);
    for (Out &o : s->outs)
      if (o.xdg == obj) {
        o.x = x;
        o.y = y;
        return;
      }
  }
  static void xdgTamano(void *data, zxdg_output_v1 *obj, int32_t w, int32_t h) {
    Impl *s = static_cast<Impl *>(data);
    for (Out &o : s->outs)
      if (o.xdg == obj) {
        o.ancho = w;
        o.alto = h;
        return;
      }
  }
  static void xdgHueco(void *, zxdg_output_v1 *, const char *) {}
  static void xdgTermina(void *, zxdg_output_v1 *) {}
  static const zxdg_output_v1_listener xdgListener;

  /* libwayland aborta si falta la función de un evento, así que declaramos
     vacíos explícitos para lo que no nos interesa. */
  static void noOp(void *, org_kde_plasma_window *) {}
  static void noOpStr(void *, org_kde_plasma_window *, const char *) {}
  static void noOpU32(void *, org_kde_plasma_window *, uint32_t) {}
  static void noOpI32(void *, org_kde_plasma_window *, int32_t) {}
  static void noOpGeom(void *, org_kde_plasma_window *, int32_t, int32_t, uint32_t, uint32_t) {}
  static void noOpParent(void *, org_kde_plasma_window *, org_kde_plasma_window *) {}
  static void noOpMenu(void *, org_kde_plasma_window *, const char *, const char *) {}

  /* ------- escuchas de cada ventana ------- */
  static void winTitle(void *data, org_kde_plasma_window *obj, const char *title) {
    Entry *e = static_cast<Impl *>(data)->byObject(obj);
    if (e) e->info.title = title ? title : "";
  }
  static void winAppId(void *data, org_kde_plasma_window *obj, const char *appId) {
    Entry *e = static_cast<Impl *>(data)->byObject(obj);
    if (e) e->info.appId = appId ? appId : "";
  }
  static void winState(void *data, org_kde_plasma_window *obj, uint32_t flags) {
    Impl *s = static_cast<Impl *>(data);
    Entry *e = s->byObject(obj);
    if (!e || e->info.states == flags) return;
    bool wasFull = e->info.fullscreen();
    e->info.states = flags;
    if (wasFull != e->info.fullscreen()) s->changed();
  }
  static void winUnmapped(void *data, org_kde_plasma_window *obj) {
    static_cast<Impl *>(data)->drop(obj);
  }
  static void winMapped(void *data, org_kde_plasma_window *obj) {
    Entry *e = static_cast<Impl *>(data)->byObject(obj);
    if (e) e->info.mapped = true;
  }
  static void winGeometry(void *data, org_kde_plasma_window *obj, int32_t x, int32_t y, uint32_t w,
                          uint32_t h) {
    Entry *e = static_cast<Impl *>(data)->byObject(obj);
    if (!e) return;
    e->info.x = x; e->info.y = y; e->info.w = w; e->info.h = h;
  }
  static void winPid(void *data, org_kde_plasma_window *obj, uint32_t pid) {
    Entry *e = static_cast<Impl *>(data)->byObject(obj);
    if (e) e->info.pid = pid;
  }

  /* ------- registro del compositor ------- */
  static void onGlobal(void *data, wl_registry *registry, uint32_t name, const char *iface, uint32_t version);
  static void onGlobalRemove(void *data, wl_registry *registry, uint32_t name);

  /* Huecos en este orden: title, app_id, state, virtual_desktop, themed_icon, unmapped,
     initial_state, parent, geometry, icon, pid, vd_entered, vd_left, menu, activity_in,
     activity_out, resource_name, client_geometry, mapped */
  static const org_kde_plasma_window_listener windowListener;
};

const org_kde_plasma_window_listener WindowMonitor::Impl::windowListener = {
    /* title_changed        */ &Impl::winTitle,
    /* app_id_changed       */ &Impl::winAppId,
    /* state_changed        */ &Impl::winState,
    /* virtual_desktop      */ &Impl::noOpI32,
    /* themed_icon_name     */ &Impl::noOpStr,
    /* unmapped             */ &Impl::winUnmapped,
    /* initial_state        */ &Impl::noOp,
    /* parent_window        */ &Impl::noOpParent,
    /* geometry             */ &Impl::winGeometry,
    /* icon_changed         */ &Impl::noOp,
    /* pid_changed          */ &Impl::winPid,
    /* vd_entered           */ &Impl::noOpStr,
    /* vd_left              */ &Impl::noOpStr,
    /* application_menu     */ &Impl::noOpMenu,
    /* activity_entered     */ &Impl::noOpStr,
    /* activity_left        */ &Impl::noOpStr,
    /* resource_name        */ &Impl::noOpStr,
    /* client_geometry      */ &Impl::noOpGeom,
    /* mapped               */ &Impl::winMapped};

const org_kde_plasma_stacking_order_listener WindowMonitor::Impl::stackListener = {
    /* window */ &Impl::stackWindow,
    /* done   */ &Impl::stackDone};

/* Huecos en este orden: logical_position, logical_size, done, name, description */
const zxdg_output_v1_listener WindowMonitor::Impl::xdgListener = {
    /* logical_position */ &Impl::xdgPosicion,
    /* logical_size     */ &Impl::xdgTamano,
    /* done             */ &Impl::xdgTermina,
    /* name             */ &Impl::xdgNombre,
    /* description      */ &Impl::xdgHueco};

void WindowMonitor::Impl::onGlobalRemove(void *, wl_registry *, uint32_t name) {
  /* Si desaparece un global (por ejemplo se reinicia KWin) la conexión deja de servir:
     el servicio lo nota al poco porque el descriptor da error. */
  (void)name;
}

void WindowMonitor::Impl::onGlobal(void *data, wl_registry *registry, uint32_t name, const char *iface,
                                   uint32_t version) {
  Impl *s = static_cast<Impl *>(data);
  if (!std::strcmp(iface, wl_output_interface.name)) {
    Out o;
    o.obj = static_cast<wl_output *>(
        wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 4u)));
    if (o.obj) {
      s->outs.push_back(o);
      s->etiquetaSalida(s->outs.back());
    }
    return;
  }
  if (!std::strcmp(iface, zxdg_output_manager_v1_interface.name)) {
    if (!s->xdgMgr) {
      s->xdgMgr = static_cast<zxdg_output_manager_v1 *>(wl_registry_bind(
          registry, name, &zxdg_output_manager_v1_interface, std::min(version, 2u)));
      for (Out &o : s->outs) s->etiquetaSalida(o);
    }
    return;
  }
  if (s->mgr || std::strcmp(iface, "org_kde_plasma_window_management") != 0) return;
  if (version < 13) {
    s->err = "el compositor ofrece org_kde_plasma_window_management pero es viejo (hace falta 13 o mas)";
    return;
  }
  s->mgr = static_cast<org_kde_plasma_window_management *>(
      wl_registry_bind(registry, name, &org_kde_plasma_window_management_interface, std::min(version, 21u)));
  /* huecos: show_desktop_changed, window, stacking_order_changed, stacking_order_uuid_changed,
     window_with_uuid, stacking_order_changed_2 */
  static const org_kde_plasma_window_management_listener ml = {
      /* show_desktop_changed      */ &Impl::mgrShowDesktop,
      /* window                    */ &Impl::mgrWindow,
      /* stacking_order_changed    */ &Impl::mgrStacking,
      /* stacking_order_uuid       */ &Impl::mgrStackingUuid,
      /* window_with_uuid          */ &Impl::mgrWindowWithUuid,
      /* stacking_order_changed_2  */ &Impl::mgrStacking2};
  org_kde_plasma_window_management_add_listener(s->mgr, &ml, s);
}

/* ---------------- API pública ---------------- */

WindowMonitor::WindowMonitor() : d(new Impl()) {}
WindowMonitor::~WindowMonitor() = default;

void WindowMonitor::setOnChange(std::function<void()> fn) { d->onChange = std::move(fn); }

bool WindowMonitor::available() const { return d->mgr != nullptr; }

int WindowMonitor::fd() const { return d->dpy ? wl_display_get_fd(d->dpy) : -1; }

int WindowMonitor::dispatchPending() { return d->dpy ? wl_display_dispatch_pending(d->dpy) : -1; }

void WindowMonitor::flush() const {
  if (d->dpy) wl_display_flush(d->dpy);
}

int WindowMonitor::waitAndDispatch(int timeoutMs) {
  if (!d->dpy) return -1;
  wl_display_flush(d->dpy);
  struct pollfd p{wl_display_get_fd(d->dpy), POLLIN, 0};
  if (poll(&p, 1, timeoutMs) <= 0) return 0;
  return wl_display_dispatch(d->dpy);
}

void WindowMonitor::settle(int quietMs, int maxRounds) {
  for (int i = 0; i < maxRounds; ++i)
    if (waitAndDispatch(quietMs) <= 0) break;
}

std::vector<WindowInfo> WindowMonitor::windows() const {
  std::vector<WindowInfo> out;
  out.reserve(d->entries.size());
  for (const Impl::Entry *e : d->entries) out.push_back(e->info);
  return out;
}

bool WindowMonitor::find(const std::string &uuid, WindowInfo &out) const {
  for (const Impl::Entry *e : d->entries)
    if (e->info.uuid == uuid) {
      out = e->info;
      return true;
    }
  return false;
}

bool WindowMonitor::fullscreenCandidate(WindowInfo &out, const std::vector<std::string> &ignoreAppIds) const {
  const Impl::Entry *best = nullptr;
  for (const Impl::Entry *e : d->entries) {
    if (!e->info.usable()) continue;
    if (std::find(ignoreAppIds.begin(), ignoreAppIds.end(), e->info.appId) != ignoreAppIds.end()) continue;
    if (!best || (e->info.active() && !best->info.active())) best = e;
  }
  if (!best) return false;
  out = best->info;
  return true;
}

bool WindowMonitor::setState(const std::string &uuid, uint32_t flags, bool on, std::string *err) {
  if (!d->mgr) {
    if (err) *err = "el compositor no ofrece el protocolo de ventanas";
    return false;
  }
  Impl::Entry *e = d->byUuid(uuid);
  if (!e || !e->obj) {
    if (err) *err = "no se encontró la ventana " + uuid;
    return false;
  }
  org_kde_plasma_window_set_state(e->obj, flags, on ? flags : 0);
  flush();
  return true;
}

bool WindowMonitor::close(const std::string &uuid, std::string *err) {
  if (!d->mgr) {
    if (err) *err = "el compositor no ofrece el protocolo de ventanas";
    return false;
  }
  Impl::Entry *e = d->byUuid(uuid);
  if (!e || !e->obj) {
    if (err) *err = "no se encontró la ventana " + uuid;
    return false;
  }
  org_kde_plasma_window_close(e->obj);
  flush();
  return true;
}

std::vector<std::string> WindowMonitor::outputNames() const {
  std::vector<std::string> out;
  for (const Impl::Out &o : d->outs)
    if (!o.name.empty()) out.push_back(o.name);
  return out;
}

std::vector<WindowMonitor::Salida> WindowMonitor::outputs() const {
  std::vector<Salida> out;
  for (const Impl::Out &o : d->outs) {
    if (o.name.empty()) continue;
    Salida s;
    s.name = o.name;
    s.x = o.x;
    s.y = o.y;
    s.w = o.ancho;
    s.h = o.alto;
    out.push_back(s);
  }
  return out;
}

bool WindowMonitor::sendToOutput(const std::string &uuid, const std::string &outputName,
                                 std::string *err) {
  if (!d->mgr) {
    if (err) *err = "el compositor no ofrece el protocolo de ventanas";
    return false;
  }
  Impl::Entry *e = d->byUuid(uuid);
  if (!e || !e->obj) {
    if (err) *err = "no se encontró la ventana " + uuid;
    return false;
  }
  /* El nombre de la salida llega en eventos aparte: si aun no lo tenemos, bombeamos un rato. */
  Impl::Out *o = d->outByName(outputName);
  for (int intento = 0; !o && intento < 12; ++intento) {
    waitAndDispatch(50);
    o = d->outByName(outputName);
  }
  if (!o) {
    std::string Tenemos;
    for (const Impl::Out &m : d->outs) {
      if (!Tenemos.empty()) Tenemos += ", ";
      Tenemos += m.name.empty() ? "(sin nombre)" : m.name;
    }
    if (err) *err = "el compositor no ofrece la salida " + outputName + " (ofrece: " + Tenemos + ")";
    return false;
  }
  org_kde_plasma_window_send_to_output(e->obj, o->obj);
  flush();
  return true;
}

bool WindowMonitor::stackingOrder(std::vector<std::string> *uuids, std::string *err) {
  if (!d->mgr || !uuids) {
    if (err) *err = "el compositor no ofrece el protocolo de ventanas";
    return false;
  }
  Impl::Stack acum;
  acum.uuids = uuids;
  org_kde_plasma_stacking_order *pila = org_kde_plasma_window_management_get_stacking_order(d->mgr);
  if (!pila) {
    if (err) *err = "el compositor no devolvio el orden de apilamiento";
    return false;
  }
  org_kde_plasma_stacking_order_add_listener(pila, &Impl::stackListener, &acum);
  flush();
  for (int ronda = 0; ronda < 40 && !acum.done; ++ronda)
    if (waitAndDispatch(100) <= 0) break;
  org_kde_plasma_stacking_order_destroy(pila);
  if (!acum.done) {
    if (err) *err = "el compositor no termino de mandar el orden de apilamiento";
    return false;
  }
  return true;
}

bool WindowMonitor::open(std::string *err) {
  d->dpy = wl_display_connect(nullptr);
  if (!d->dpy) {
    std::string m = "no se pudo conectar al compositor Wayland (WAYLAND_DISPLAY no esta definido o no hay sesion grafica)";
    if (err) *err = m;
    err_ = m;
    return false;
  }
  d->registry = wl_display_get_registry(d->dpy);
  static const wl_registry_listener kRegistryListener = {&Impl::onGlobal, &Impl::onGlobalRemove};
  wl_registry_add_listener(d->registry, &kRegistryListener, d.get());
  wl_display_roundtrip(d->dpy);
  if (!d->mgr) {
    std::string m = d->err.empty() ? "el compositor no ofrece org_kde_plasma_window_management (esto es especifico de KWin/KDE)"
                                   : d->err;
    if (err) *err = m;
    err_ = m;
    return false;
  }
  wl_display_roundtrip(d->dpy); /* que lleguen las ventanas ya abiertas */
  return true;
}
