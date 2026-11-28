#include "capture.h"

#include "zkde-screencast-unstable-v1-client-protocol.h"

#include <pipewire/pipewire.h>
#include <spa/buffer/buffer.h>
#include <spa/param/video/format-utils.h>
#include <wayland-client.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <mutex>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {
void initPipeWireOnce() {
  static std::once_flag once;
  std::call_once(once, [] { pw_init(nullptr, nullptr); });
}
}  // namespace

struct WindowCapture::Impl {
  /* ---- Wayland ---- */
  wl_display *dpy = nullptr;
  wl_registry *registry = nullptr;
  zkde_screencast_unstable_v1 *cast = nullptr;
  zkde_screencast_stream_unstable_v1 *stream = nullptr;
  std::string uuid;
  std::chrono::steady_clock::time_point createdAt;

  /* ---- PipeWire ---- */
  pw_thread_loop *loop = nullptr;
  pw_core *core = nullptr;
  pw_stream *pwStream = nullptr;
  pw_registry *pwRegistry = nullptr;
  spa_hook pwHook{};
  spa_hook pwRegHook{};
  struct Port { uint32_t globalId; uint32_t nodeId; std::string name; };
  std::vector<Port> ports;
  uint64_t objectSerial = 0;
  uint32_t nodeId = 0;
  uint32_t ourNodeId = 0;
  pw_proxy *linkProxy = nullptr;
  bool linkMade = false;
  int fmtW = 0, fmtH = 0;
  uint64_t fmtModifier = 0;
  uint32_t fmtFourcc = 0;
  int lastFd = -1, lastOffset = 0;
  int ownFd = -1;  /* copia nuestra del descriptor del fotograma */
  int lastMaxsize = 0;
  uint64_t frameCount = 0;
  bool lastDmabuf = false;
  bool soloMemoria = false;   /* (v1.18) ver capture.h */
  std::string fmtName;
  std::string nodeName, nodeSerial, nodeClass, pwState, streamClass = "Stream/Input/Video";
  bool created = false;
  bool pwStarted = false;

  /* ---- fotograma ---- */
  mutable std::mutex mtx;
  std::vector<uint8_t> frame;
  int w = 0, h = 0, stride = 0;
  bool shmSeen = false, dmabufSeen = false;
  std::string note;

  ~Impl() { closeAll(); }

  void setNote(const std::string &s) {
    std::lock_guard<std::mutex> g(mtx);
    note = s;
  }

  void closeAll() {
    if (ownFd >= 0) { ::close(ownFd); ownFd = -1; }
    if (loop) pw_thread_loop_lock(loop);
    if (pwStream) {
      pw_stream_destroy(pwStream);
      pwStream = nullptr;
    }
    if (linkProxy) {
      pw_proxy_destroy(linkProxy);
      linkProxy = nullptr;
    }
    if (pwRegistry) {
      pw_proxy_destroy(reinterpret_cast<pw_proxy *>(pwRegistry));
      pwRegistry = nullptr;
    }
    if (core) {
      pw_core_disconnect(core);
      core = nullptr;
    }
    if (loop) {
      pw_thread_loop_unlock(loop);
      pw_thread_loop_stop(loop);
      pw_thread_loop_destroy(loop);
      loop = nullptr;
    }
    if (stream) {
      zkde_screencast_stream_unstable_v1_destroy(stream);
      stream = nullptr;
    }
    if (cast) {
      zkde_screencast_unstable_v1_destroy(cast);
      cast = nullptr;
    }
    if (registry) {
      wl_registry_destroy(registry);
      registry = nullptr;
    }
    /* Empezar de cero: el contador de enlace y el nodo anterior se quedaban guardados, asi que
       maybeLink creia que ya habia enlace y el flujo nuevo nunca se conectaba. Era la razon de
       que la imagen se quedara congelada despues de un corte (reintentos que no reanimaban nada).
       Faltaba pwStarted, y con esa bandera puesta el dano era total: readyToConnect() dice "ya
       estoy conectado" y connectToNode() no volvia a ejecutarse nunca, asi que el flujo nuevo de
       PipeWire no se creaba ni despues de muchos reintentos. Medido el 23-nov: tras un
       minimizado devuelto, el contador del visor se clavo en 3361 fotogramas mientras
       `ctl flujo` medía 65,6 fps en la misma ventana; o sea el video si estaba, lo que no
       existia era nuestro flujo. */
    linkMade = false;
    pwStarted = false;
    nodeId = 0;
    ourNodeId = 0;
    nodeSerial.clear();
    nodeName.clear();
    ports.clear();
    created = false;
    pwState.clear();
    note.clear();
    if (dpy) {
      wl_display_disconnect(dpy);
      dpy = nullptr;
    }
  }

  /* ------- eventos del flujo de KWin (orden del XML) ------- */
  static void stClosed(void *data, zkde_screencast_stream_unstable_v1 *) {
    static_cast<Impl *>(data)->setNote("el compositor cerró el flujo");
  }
  static void stCreated(void *data, zkde_screencast_stream_unstable_v1 *, uint32_t node) {
    Impl *s = static_cast<Impl *>(data);
    s->nodeId = node;
    s->created = true;
    s->createdAt = std::chrono::steady_clock::now();
  }
  static void stFailed(void *data, zkde_screencast_stream_unstable_v1 *, const char *error) {
    static_cast<Impl *>(data)->setNote("el compositor rechazó el flujo: " +
                                       std::string(error ? error : "?"));
  }
  static void stSerial(void *data, zkde_screencast_stream_unstable_v1 *, uint32_t hi, uint32_t lo) {
    Impl *s = static_cast<Impl *>(data);
    s->objectSerial = (static_cast<uint64_t>(hi) << 32) | lo;
  }
  static const zkde_screencast_stream_unstable_v1_listener streamListener;

  /* ------- registro de Wayland ------- */
  static void onGlobal(void *data, wl_registry *reg, uint32_t name, const char *iface, uint32_t version) {
    Impl *s = static_cast<Impl *>(data);
    if (!s->cast && std::strcmp(iface, "zkde_screencast_unstable_v1") == 0)
      s->cast = static_cast<zkde_screencast_unstable_v1 *>(
          wl_registry_bind(reg, name, &zkde_screencast_unstable_v1_interface, version < 6u ? version : 6u));
  }
  static void onGlobalRemove(void *, wl_registry *, uint32_t) {}

  /* ------- PipeWire ------- */
  bool startPipeWire() {
    if (loop) return core != nullptr;
    initPipeWireOnce();
    loop = pw_thread_loop_new("odysseyvk-captura", nullptr);
    if (!loop) {
      setNote("no se pudo crear el bucle de PipeWire");
      return false;
    }
    pw_thread_loop_start(loop);
    pw_thread_loop_lock(loop);
    pw_context *ctx = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
    if (ctx) core = pw_context_connect(ctx, nullptr, 0);
    if (core) {
      pwRegistry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
      static const pw_registry_events re = {PW_VERSION_REGISTRY_EVENTS, &Impl::pwGlobal, &Impl::pwGlobalRemove};
      if (pwRegistry) pw_registry_add_listener(pwRegistry, &pwRegHook, &re, this);
    }
    pw_thread_loop_unlock(loop);
    if (!core) setNote("no hay conexión con el servidor de PipeWire");
    return core != nullptr;
  }

  bool readyToConnect() {
    if (!created || pwStarted) return false;
    {
      std::lock_guard<std::mutex> g(mtx);
      if (!nodeSerial.empty() || !nodeName.empty()) return true;
    }
    return std::chrono::steady_clock::now() - createdAt > std::chrono::milliseconds(600);
  }

  void connectToNode() {
    if (pwStarted || !created) return;
    if (!startPipeWire()) return;
    pw_thread_loop_lock(loop);
    std::string target, targetKind;
    {
      std::lock_guard<std::mutex> g(mtx);
      if (!nodeSerial.empty()) { target = nodeSerial; targetKind = "serial"; }
      else if (!nodeName.empty()) { target = nodeName; targetKind = "nombre"; }
    }
    pw_properties *props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CLASS, streamClass.c_str(),
                                             PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_NODE_DONT_RECONNECT, "true",
                                             nullptr);
    if (!target.empty()) pw_properties_set(props, PW_KEY_TARGET_OBJECT, target.c_str());
    pwStream = pw_stream_new(core, "odysseyvk", props);
    if (!pwStream) {
      pw_thread_loop_unlock(loop);
      setNote("no se pudo crear el flujo de PipeWire");
      return;
    }
    /* orden: version, destroy, state_changed, control_info, io_changed, param_changed,
       add_buffer, remove_buffer, process, drained, command, trigger_done */
    static const pw_stream_events ev = {PW_VERSION_STREAM_EVENTS,
                                        nullptr,
                                        &Impl::pwStateChanged,
                                        nullptr,
                                        nullptr,
                                        &Impl::pwParamChanged,
                                        &Impl::pwAddBuffer,
                                        &Impl::pwRemoveBuffer,
                                        &Impl::pwProcess,
                                        &Impl::pwDrained,
                                        nullptr,
                                        nullptr};
    pw_stream_add_listener(pwStream, &pwHook, &ev, this);
    const uint32_t tid = target.empty() ? nodeId : PW_ID_ANY;
    /* (v1.18) en memoria: el formato, dicho sin modificador (asi el compositor no manda DMA-BUF) */
    uint8_t almacen[1024];
    struct spa_pod_builder pb = SPA_POD_BUILDER_INIT(almacen, sizeof almacen);
    const struct spa_pod *params[1] = {nullptr};
    if (soloMemoria) {
      struct spa_rectangle tam = SPA_RECTANGLE(1920, 1080), tamMin = SPA_RECTANGLE(1, 1), tamMax = SPA_RECTANGLE(16384, 16384);
      struct spa_fraction fr = SPA_FRACTION(60, 1), frMin = SPA_FRACTION(0, 1), frMax = SPA_FRACTION(1000, 1);
      params[0] = (const struct spa_pod *)spa_pod_builder_add_object(
          &pb, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
          SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
          SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
          SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(5, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA,
                                                          SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA),
          SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&tam, &tamMin, &tamMax),
          SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&fr, &frMin, &frMax));
    }
    const int r = pw_stream_connect(pwStream, PW_DIRECTION_INPUT, tid, PW_STREAM_FLAG_MAP_BUFFERS, soloMemoria ? params : nullptr,
                                    soloMemoria ? 1 : 0);
    pwStarted = (r >= 0);
    if (pwStarted) ourNodeId = pw_stream_get_node_id(pwStream);
    if (r < 0) setNote(std::string("el nodo de captura rechazó la conexión: ") + std::strerror(-r));
    pw_thread_loop_unlock(loop);
  }

  /* registro de PipeWire: el nodo del compositor y los puertos que hay que enlazar */
  static void pwGlobal(void *data, uint32_t id, uint32_t, const char *type, uint32_t, const spa_dict *props) {
    Impl *s = static_cast<Impl *>(data);
    if (!props || !type) return;
    const char *nodeIdProp = spa_dict_lookup(props, "node.id");
    const char *portName = spa_dict_lookup(props, "port.name");
    if (!portName) portName = spa_dict_lookup(props, "port.id");
    std::string t = type;
    if (t.find("Port") != std::string::npos && nodeIdProp && portName) {
      Port p;
      p.globalId = id;
      p.nodeId = static_cast<uint32_t>(std::strtoul(nodeIdProp, nullptr, 10));
      p.name = portName;
      std::lock_guard<std::mutex> g(s->mtx);
      s->ports.push_back(p);
      return;
    }
    if (id != s->nodeId) return;
    std::string info = std::string("nodo ") + t;
    for (uint32_t i = 0; i < props->n_items; ++i) {
      const char *k = props->items[i].key;
      const char *v = props->items[i].value;
      if (!k || !v) continue;
      if (!std::strcmp(k, PW_KEY_MEDIA_CLASS) || !std::strcmp(k, "node.name") ||
          !std::strcmp(k, PW_KEY_OBJECT_SERIAL) || !std::strcmp(k, PW_KEY_MEDIA_NAME))
        info += std::string(" [") + k + "=" + v + "]";
    }
    std::lock_guard<std::mutex> g(s->mtx);
    if (const char *v = spa_dict_lookup(props, "node.name")) s->nodeName = v;
    if (const char *v = spa_dict_lookup(props, PW_KEY_OBJECT_SERIAL)) s->nodeSerial = v;
    if (const char *v = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS)) s->nodeClass = v;
    s->note = info;
  }
  static void pwGlobalRemove(void *, uint32_t) {}

  /* el gestor de sesión no enlaza solos los flujos de vídeo: hay que pedir el enlace */
  void maybeLink() {
    if (linkMade || !core || !nodeId) return;
    if (ourNodeId == 0 || ourNodeId == PW_ID_ANY) {
      pw_thread_loop_lock(loop);
      ourNodeId = pw_stream_get_node_id(pwStream);
      pw_thread_loop_unlock(loop);
    }
    if (ourNodeId == 0 || ourNodeId == PW_ID_ANY) return;
    uint32_t outPort = 0, inPort = 0;
    {
      std::lock_guard<std::mutex> g(mtx);
      for (const Port &p : ports) {
        if (p.nodeId == nodeId && p.name.compare(0, 6, "output") == 0) outPort = p.globalId;
        if (p.nodeId == ourNodeId && p.name.compare(0, 5, "input") == 0) inPort = p.globalId;
      }
    }
    if (!outPort || !inPort) return;
    char b[16], e[16];
    std::snprintf(b, sizeof b, "%u", outPort);
    std::snprintf(e, sizeof e, "%u", inPort);
    pw_properties *props = pw_properties_new(PW_KEY_LINK_OUTPUT_PORT, b, PW_KEY_LINK_INPUT_PORT, e, nullptr);
    pw_thread_loop_lock(loop);
    linkProxy = static_cast<pw_proxy *>(
        pw_core_create_object(core, "link-factory", PW_TYPE_INTERFACE_Link, 0, &props->dict, 0));
    pw_properties_free(props);
    pw_thread_loop_unlock(loop);
    linkMade = linkProxy != nullptr;
    if (!linkMade) setNote("el servidor de PipeWire rechazó el enlace entre el compositor y nosotros");
  }

  static void pwStateChanged(void *data, pw_stream_state, pw_stream_state state, const char *error) {
    Impl *s = static_cast<Impl *>(data);
    std::lock_guard<std::mutex> g(s->mtx);
    s->pwState = pw_stream_state_as_string(state);
    if (error) s->note = std::string("error del flujo de PipeWire: ") + error;
  }
  static void pwParamChanged(void *data, uint32_t id, const spa_pod *param) {
    Impl *s = static_cast<Impl *>(data);
    if (!param || id != SPA_PARAM_Format) return;
    spa_video_info_raw vi{};
    if (spa_format_video_raw_parse(param, &vi) < 0) return;
    std::lock_guard<std::mutex> g(s->mtx);
    s->fmtW = static_cast<int>(vi.size.width);
    s->fmtH = static_cast<int>(vi.size.height);
    s->fmtModifier = vi.modifier;
    s->fmtFourcc = drmFourccFor(vi.format);
    char cc[8];
    const uint32_t f = vi.format;
    if (f >= 0x20202020u && f <= 0x7e7e7e7eu)
      std::snprintf(cc, sizeof cc, "%c%c%c%c", int(f >> 24) & 0xff, int(f >> 16) & 0xff, int(f >> 8) & 0xff,
                    int(f) & 0xff);
    else
      std::snprintf(cc, sizeof cc, "#%u", f);
    s->fmtName = cc;
    std::fprintf(stderr, "[captura] formato del flujo: %s %dx%d modificador 0x%016llx\n", cc, s->fmtW,
                 s->fmtH, (unsigned long long)s->fmtModifier);
  }
  static void pwAddBuffer(void *, pw_buffer *) {}
  static void pwRemoveBuffer(void *, pw_buffer *) {}
  static void pwDrained(void *data) {
    static_cast<Impl *>(data)->setNote("el flujo de PipeWire se quedó sin datos");
  }
  /* fourcc de DRM: los caracteres van en orden inverso al de la memoria */
  static uint32_t fourccOf(char a, char b, char c, char d) {
    return static_cast<uint32_t>(static_cast<unsigned char>(a)) |
           (static_cast<uint32_t>(static_cast<unsigned char>(b)) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(c)) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(d)) << 24);
  }
  static uint32_t drmFourccFor(enum spa_video_format f) {
    switch (f) {
      case SPA_VIDEO_FORMAT_BGRA: return fourccOf('X', 'R', '2', '4'); /* memoria B,G,R,X */
      case SPA_VIDEO_FORMAT_BGRx: return fourccOf('X', 'R', '2', '4');
      case SPA_VIDEO_FORMAT_RGBA: return fourccOf('X', 'B', '2', '4'); /* memoria R,G,B,X */
      case SPA_VIDEO_FORMAT_RGBx: return fourccOf('X', 'B', '2', '4');
      case SPA_VIDEO_FORMAT_xRGB: return fourccOf('X', 'B', 'G', 'R');
      case SPA_VIDEO_FORMAT_xBGR: return fourccOf('X', 'R', 'G', 'B');
      case SPA_VIDEO_FORMAT_ABGR: return fourccOf('A', 'R', '2', '4');
      case SPA_VIDEO_FORMAT_ARGB: return fourccOf('A', 'B', 'G', 'R');
      case SPA_VIDEO_FORMAT_NV12: return fourccOf('N', 'V', '1', '2');
      case SPA_VIDEO_FORMAT_BGR:  return fourccOf('0', '0', 'B', 'A'); /* RGB888 */
      default: return 0;
    }
  }

  static void pwProcess(void *data) {
    Impl *s = static_cast<Impl *>(data);
    if (!s->pwStream) return;
    pw_buffer *b = pw_stream_dequeue_buffer(s->pwStream);
    if (!b) return;
    spa_buffer *buf = b->buffer;
    if (buf && buf->n_datas > 0) {
      if (buf->n_datas > 1) {
        static int avisan = 3;
        if (avisan-- > 0)
          std::fprintf(stderr, "[captura] el fotograma trae %u planos (no es RGBA directo)\n", buf->n_datas);
      }
      spa_data &d = buf->datas[0];
      const size_t sz = d.chunk && d.chunk->size ? d.chunk->size : d.maxsize;
      const int chunkStride = d.chunk ? static_cast<int>(d.chunk->stride) : 0;
      auto deriveSize = [&](size_t bytes) {
        if (s->fmtW > 0 && s->fmtH > 0) {
          s->w = static_cast<int>(s->fmtW);
          s->h = static_cast<int>(s->fmtH);
          if (s->stride <= 0) s->stride = s->w * 4;
        }
        if (s->w <= 0 && s->stride > 4 && bytes % static_cast<size_t>(s->stride) == 0) {
          s->w = s->stride / 4;
          s->h = static_cast<int>(bytes / static_cast<size_t>(s->stride));
        }
        if (s->w <= 0 && s->stride <= 0) { /* adivinamos un fotograma cuadrado de 4 bytes */
          const size_t px = bytes / 4;
          const int side = static_cast<int>(std::sqrt(static_cast<double>(px)) + 0.5);
          if (side * side * 4 == static_cast<int>(bytes)) { s->w = s->h = side; s->stride = side * 4; }
        }
      };
      if (d.data) {
        const uint8_t *src = static_cast<const uint8_t *>(d.data) + (d.chunk ? d.chunk->offset : 0);
        std::lock_guard<std::mutex> g(s->mtx);
        s->frame.assign(src, src + sz);
        s->stride = chunkStride;
        deriveSize(sz);
        s->shmSeen = true;
        s->lastDmabuf = false;
        s->lastFd = -1;
      } else if (d.fd >= 0) {
        /* DMA-BUF de la GPU: NO se toca con la CPU. Con memoria bloque-lineal (NVIDIA/AMD) el
           mapeo devuelve basura y copiar 30 MB por fotograma congela el proceso. Aquí sólo se
           guardan los metadatos para importarlos en OpenGL con eglCreateImageKHR. */
        /* El descriptor es de PipeWire: en cuanto devolvemos el búfer el compositor puede
           cerrarlo, y entonces importarlo falla. Nos quedamos con una copia nuestra. */
        int dup = fcntl(d.fd, F_DUPFD_CLOEXEC, 0);
        std::lock_guard<std::mutex> g(s->mtx);
        if (s->ownFd >= 0) ::close(s->ownFd);
        s->ownFd = dup;
        s->dmabufSeen = true;
        s->lastDmabuf = true;
        s->lastFd = dup >= 0 ? dup : static_cast<int>(d.fd);
        s->lastOffset = static_cast<int>(d.mapoffset);
        s->lastMaxsize = static_cast<int>(d.maxsize);
        s->stride = chunkStride;
        deriveSize(sz);
      }
      {
        std::lock_guard<std::mutex> g(s->mtx);
        ++s->frameCount;
      }
    }
    pw_stream_queue_buffer(s->pwStream, b);
  }
};

const zkde_screencast_stream_unstable_v1_listener WindowCapture::Impl::streamListener = {
    &Impl::stClosed, &Impl::stCreated, &Impl::stFailed, &Impl::stSerial};

/* ---------------- API pública ---------------- */

WindowCapture::WindowCapture() : d(new Impl()) {}
WindowCapture::~WindowCapture() = default;

bool WindowCapture::start(const std::string &uuid, std::string *err) {
  d->uuid = uuid;
  d->dpy = wl_display_connect(nullptr);
  if (!d->dpy) {
    err_ = "no se pudo conectar con el compositor";
    if (err) *err = err_;
    return false;
  }
  d->registry = wl_display_get_registry(d->dpy);
  static const wl_registry_listener rl = {&Impl::onGlobal, &Impl::onGlobalRemove};
  wl_registry_add_listener(d->registry, &rl, d.get());
  wl_display_roundtrip(d->dpy);
  if (!d->cast) {
    err_ = "el compositor no ofrece zkde_screencast_unstable_v1 (hace falta KDE/KWin)";
    if (err) *err = err_;
    return false;
  }
  d->stream = zkde_screencast_unstable_v1_stream_window(d->cast, uuid.c_str(),
                                                        ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_HIDDEN);
  zkde_screencast_stream_unstable_v1_add_listener(d->stream, &Impl::streamListener, d.get());
  wl_display_flush(d->dpy);
  return true;
}

void WindowCapture::setStreamClass(const std::string &pwClass) { d->streamClass = pwClass; }

void WindowCapture::stop() { d->closeAll(); }

bool WindowCapture::running() const { return d->dpy != nullptr; }

int WindowCapture::fd() const { return d->dpy ? wl_display_get_fd(d->dpy) : -1; }

void WindowCapture::pump() {
  if (!d->dpy) return;
  /* patrón no bloqueante: reservar la lectura, vaciar lo saliente, mirar si hay
     datos entrantes y sólo entonces leerlos (el fotograma llega por PipeWire) */
  while (wl_display_prepare_read(d->dpy) != 0) wl_display_dispatch_pending(d->dpy);
  wl_display_flush(d->dpy);
  struct pollfd pfd{wl_display_get_fd(d->dpy), POLLIN, 0};
  if (poll(&pfd, 1, 0) > 0)
    wl_display_read_events(d->dpy);
  else
    wl_display_cancel_read(d->dpy);
  while (wl_display_dispatch_pending(d->dpy) > 0) { /* vaciar */ }
  if (d->readyToConnect()) d->connectToNode();
  if (d->pwStarted) d->maybeLink();
}

bool WindowCapture::streamCreated() const { return d->created; }

bool WindowCapture::framesFlowing() const {
  std::lock_guard<std::mutex> g(d->mtx);
  return d->frameCount > 0;
}

uint64_t WindowCapture::frameCount() const {
  std::lock_guard<std::mutex> g(d->mtx);
  return d->frameCount;
}

bool WindowCapture::dmabufOnly() const {
  std::lock_guard<std::mutex> g(d->mtx);
  return d->dmabufSeen && !d->shmSeen;
}

bool WindowCapture::copyFrame(std::vector<uint8_t> &out, int &w, int &h, int &stride) {
  std::lock_guard<std::mutex> g(d->mtx);
  if (d->frame.empty()) {
    if (d->lastDmabuf && d->note.empty())
      d->note = "los fotogramas llegan en DMA-BUF: hay que usarlos en la GPU (ctl capture-gl)";
    return false;
  }
  out = d->frame;
  w = d->w;
  h = d->h;
  stride = d->stride;
  return true;
}

void WindowCapture::soloMemoria(bool si) { d->soloMemoria = si; }

bool WindowCapture::lastFrame(CapturedFrame &out) const {
  std::lock_guard<std::mutex> g(d->mtx);
  if (d->frame.empty() && !(d->lastDmabuf && d->lastFd >= 0)) return false;
  out.valid = true;
  out.dmabuf = d->lastDmabuf;
  out.w = d->w;
  out.h = d->h;
  out.stride = d->stride;
  out.fd = d->lastFd;
  out.offset = d->lastOffset;
  out.modifier = d->fmtModifier;
  out.fourcc = d->fmtFourcc;
  return true;
}

std::string WindowCapture::stateText() const {
  std::lock_guard<std::mutex> g(d->mtx);
  std::string s = d->pwState.empty() ? "sin estado" : d->pwState;
  if (!d->note.empty()) s += " · " + d->note;
  return s;
}

std::string WindowCapture::describe() const {
  std::lock_guard<std::mutex> g(d->mtx);
  std::string s = "nodo PipeWire: " + std::to_string(d->nodeId);
  s += d->created ? " (creado)" : " (esperando)";
  if (!d->nodeClass.empty()) s += " clase " + d->nodeClass;
  if (!d->nodeName.empty()) s += " nombre " + d->nodeName;
  if (!d->nodeSerial.empty()) s += " serial " + d->nodeSerial;
  s += d->pwStarted ? ", flujo conectado" : ", flujo sin conectar";
  if (!d->pwState.empty()) s += " (" + d->pwState + ")";
  if (d->linkMade) s += ", enlazado";
  else s += ", sin enlazar";
  s += " | nuestro nodo " + std::to_string(d->ourNodeId) + ", puertos vistos " + std::to_string(d->ports.size());
  for (const Impl::Port &p : d->ports)
    if (p.nodeId == d->nodeId || p.nodeId == d->ourNodeId)
      s += " [" + p.name + "@" + std::to_string(p.nodeId) + "=" + std::to_string(p.globalId) + "]";
  if (!d->fmtName.empty()) s += ", formato " + d->fmtName + " " + std::to_string(d->fmtW) + "x" + std::to_string(d->fmtH);
  {
    char buf[160];
    std::snprintf(buf, sizeof buf, ", modificador 0x%llx, cuatrocc %c%c%c%c, paso %d",
                  static_cast<unsigned long long>(d->fmtModifier),
                  static_cast<char>(d->fmtFourcc & 0xff), static_cast<char>((d->fmtFourcc >> 8) & 0xff),
                  static_cast<char>((d->fmtFourcc >> 16) & 0xff), static_cast<char>((d->fmtFourcc >> 24) & 0xff),
                  d->stride);
    s += buf;
  }
  if (d->shmSeen)
    s += ", en memoria compartida " + std::to_string(d->w) + "x" + std::to_string(d->h) + " paso " +
         std::to_string(d->stride);
  if (d->dmabufSeen) s += ", en dmabuf";
  if (!d->frame.empty()) s += ", último fotograma " + std::to_string(d->frame.size()) + " bytes";
  if (!d->note.empty()) s += " | " + d->note;
  return s;
}
