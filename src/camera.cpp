#include "camera.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>

static void say(std::string *err, const std::string &text) {
  if (err) *err = text;
}

static std::string leyerPequeno(const std::string &ruta) {
  char texto[128] = {};
  FILE *f = std::fopen(ruta.c_str(), "rb");
  if (!f) return std::string();
  if (std::fgets(texto, sizeof texto, f)) texto[std::strcspn(texto, "\r\n")] = 0;
  std::fclose(f);
  return texto;
}

uint32_t Camera::fourccCode(const std::string &fourcc) {
  if (fourcc.size() != 4) return 0;
  return (uint32_t)(unsigned char)fourcc[0] | ((uint32_t)(unsigned char)fourcc[1] << 8) |
         ((uint32_t)(unsigned char)fourcc[2] << 16) | ((uint32_t)(unsigned char)fourcc[3] << 24);
}

static std::string fourccName(uint32_t code) {
  char name[5] = {(char)(code & 0xff), (char)((code >> 8) & 0xff), (char)((code >> 16) & 0xff),
                  (char)((code >> 24) & 0xff), 0};
  return name;
}

/* Nombre del nodo sysfs (/dev/video7 -> video7). (v1.16, de la PR #2 de Noa3) sysfs usa videoN aunque la camara se
   haya elegido por su enlace estable de /dev/v4l/by-id/: se resuelve el enlace antes. La captura sigue abriendo la ruta
   puesta. Con el enlace, la camara estereo del monitor salia como una mono de 640x480 */
static std::string nombreNodo(const std::string &device) {
  char real[PATH_MAX];
  const std::string ruta = ::realpath(device.c_str(), real) ? std::string(real) : device;
  const size_t barra = ruta.find_last_of('/');
  return barra == std::string::npos ? ruta : ruta.substr(barra + 1);
}

bool Camera::usbSysfs(const std::string &device, std::string *dir) {
  const std::string nodo = nombreNodo(device);   /* (v1.16, PR #2) tambien por su enlace de /dev/v4l/by-id */
  const std::string enlace = "/sys/class/video4linux/" + nodo + "/device";
  char ruta[PATH_MAX] = {};
  if (!::realpath(enlace.c_str(), ruta)) return false;
  /* el enlace lleva a la interfaz (5-1.4.2.3:1.0); el padre es el dispositivo USB */
  std::string p(ruta);
  if (p.find(':') == std::string::npos) return false;
  const size_t i = p.rfind('/');
  if (i == std::string::npos) return false;
  p = p.substr(0, i);
  if (!std::strstr(p.c_str(), "/usb")) return false;
  if (dir) *dir = p;
  return true;
}

bool Camera::keepAwake(const std::string &device, std::string *nota) {
  std::string dir;
  if (!usbSysfs(device, &dir)) {
    if (nota) *nota = "no se encontro el dispositivo USB detras de " + device;
    return false;
  }
  const std::string archivo = leyerPequeno(dir + "/power/control");
  if (archivo == "on") {
    if (nota) *nota = "autosuspension ya apagada en " + dir.substr(dir.rfind('/') + 1);
    return true;
  }
  FILE *f = std::fopen((dir + "/power/control").c_str(), "w");
  if (!f) {
    if (nota) *nota = "no se pudo apagar la autosuspension (" + std::string(std::strerror(errno)) +
                      "): hace falta la regla udev o root";
    return false;
  }
  const bool escrito = std::fputs("on", f) >= 0;
  std::fclose(f);
  if (nota)
    *nota = escrito ? "autosuspension apagada en " + dir.substr(dir.rfind('/') + 1)
                    : "no se pudo escribir power/control";
  return escrito;
}

std::string Camera::powerState(const std::string &device) {
  std::string dir;
  if (!usbSysfs(device, &dir)) return "sin dispositivo USB detras";
  const std::string control = leyerPequeno(dir + "/power/control");
  const std::string estado = leyerPequeno(dir + "/power/runtime_status");
  return "control=" + (control.empty() ? "?" : control) + " estado=" + (estado.empty() ? "?" : estado);
}

bool Camera::open(const std::string &device, std::string *err) {
  close();
  int fd = ::open(device.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "no se pudo abrir %s: %s", device.c_str(), std::strerror(errno));
    say(err, buf);
    err_ = buf;
    return false;
  }
  v4l2_capability cap{};
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 || !(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "%s no es un nodo de captura de video", device.c_str());
    ::close(fd);
    say(err, buf);
    err_ = buf;
    return false;
  }
  fd_ = fd;
  path_ = device;
  err_.clear();
  note_.clear();
  keepAwake(device, &note_);
  return true;
}

void Camera::freeBuffers() {
  for (size_t i = 0; i < maps_.size(); i++)
    if (maps_[i] && maps_[i] != MAP_FAILED) munmap(maps_[i], sizes_[i]);
  maps_.clear();
  sizes_.clear();
  held_ = -1;
  if (fd_ >= 0) {
    v4l2_requestbuffers req{};
    req.count = 0;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(fd_, VIDIOC_REQBUFS, &req);
  }
}

void Camera::stop() {
  if (fd_ >= 0 && running_) {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd_, VIDIOC_STREAMOFF, &type);
  }
  running_ = false;
}

void Camera::close() {
  stop();
  freeBuffers();
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
  w_ = h_ = 0;
  mjpeg_ = false;
  path_.clear();
}

bool Camera::setFormat(int width, int height, const std::string &fourcc, int fps, std::string *err) {
  if (fd_ < 0) {
    say(err, "la camara no esta abierta");
    return false;
  }
  stop();
  freeBuffers();

  v4l2_format fmt{};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = (uint32_t)width;
  fmt.fmt.pix.height = (uint32_t)height;
  fmt.fmt.pix.pixelformat = fourccCode(fourcc);
  fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
    const int e = errno;
    char buf[420];
    std::snprintf(buf, sizeof buf, "VIDIOC_S_FMT %dx%d %s: %s", width, height, fourcc.c_str(),
                  std::strerror(e));
    if (e == EIO)
      std::strcat(buf, " (la interfaz de flujo no contesta a los controles UVC; la camara se queda "
                       "colgada y solo se levanta apagando y encendiendo el monitor)");
    say(err, buf);
    err_ = buf;
    return false;
  }
  if ((int)fmt.fmt.pix.width != width || (int)fmt.fmt.pix.height != height ||
      fmt.fmt.pix.pixelformat != fourccCode(fourcc)) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "el dispositivo cambio el formato a %dx%d %s", fmt.fmt.pix.width,
                  fmt.fmt.pix.height, fourccName(fmt.fmt.pix.pixelformat).c_str());
    say(err, buf);
    err_ = buf;
    return false;
  }

  /* decirle la tasa wanted: el conductor UVC usa el ancho de banda resultante
     para elegir el alt setting del flujo isocrono. */
  if (fps > 0) {
    v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = (uint32_t)fps;
    if (ioctl(fd_, VIDIOC_S_PARM, &parm) < 0) {
      char buf[200];
      std::snprintf(buf, sizeof buf, "aviso: VIDIOC_S_PARM %d fps: %s", fps, std::strerror(errno));
      say(err, buf);
    }
  }

  v4l2_requestbuffers req{};
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
    char buf[200];
    std::snprintf(buf, sizeof buf, "VIDIOC_REQBUFS: %s (pidio %u tampones)", std::strerror(errno),
                  req.count);
    say(err, buf);
    err_ = buf;
    return false;
  }
  for (uint32_t i = 0; i < req.count; i++) {
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
      char text[200];
      std::snprintf(text, sizeof text, "VIDIOC_QUERYBUF %u: %s", i, std::strerror(errno));
      freeBuffers();
      say(err, text);
      err_ = text;
      return false;
    }
    void *map = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);
    if (map == MAP_FAILED) {
      freeBuffers();
      say(err, "mmap de los tampones fallo");
      err_ = "mmap de los tampones fallo";
      return false;
    }
    maps_.push_back(map);
    sizes_.push_back(buf.length);
    v4l2_buffer queue{};
    queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    queue.memory = V4L2_MEMORY_MMAP;
    queue.index = i;
    if (ioctl(fd_, VIDIOC_QBUF, &queue) < 0) {
      freeBuffers();
      say(err, "VIDIOC_QBUF fallo");
      err_ = "VIDIOC_QBUF fallo";
      return false;
    }
  }
  w_ = (int)fmt.fmt.pix.width;
  h_ = (int)fmt.fmt.pix.height;
  mjpeg_ = fmt.fmt.pix.pixelformat == fourccCode("MJPG");
  err_.clear();
  if (err) err->clear();
  return true;
}

bool Camera::start(std::string *err) {
  if (fd_ < 0 || maps_.empty()) {
    say(err, "no hay formato ni tampones preparados");
    return false;
  }
  v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
    char buf[200];
    std::snprintf(buf, sizeof buf, "VIDIOC_STREAMON: %s", std::strerror(errno));
    say(err, buf);
    err_ = buf;
    return false;
  }
  running_ = true;
  return true;
}

bool Camera::read(const uint8_t **data, size_t *size, int timeout_ms, std::string *err) {
  if (!running_ || maps_.empty()) {
    say(err, "el flujo no esta activo");
    return false;
  }
  pollfd pfd{fd_, POLLIN, 0};
  int ready = poll(&pfd, 1, timeout_ms);
  if (ready == 0) {
    say(err, "sin fotogramas (fin de espera)");
    return false;
  }
  if (ready < 0) {
    char buf[200];
    std::snprintf(buf, sizeof buf, "poll: %s", std::strerror(errno));
    say(err, buf);
    return false;
  }
  v4l2_buffer buf{};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
    char text[200];
    std::snprintf(text, sizeof text, "VIDIOC_DQBUF: %s", std::strerror(errno));
    say(err, text);
    err_ = text;
    return false;
  }
  held_ = (int)buf.index;
  if (data) *data = (const uint8_t *)maps_[buf.index];
  if (size) *size = buf.bytesused;
  return true;
}

void Camera::release() {
  if (held_ < 0 || fd_ < 0) return;
  v4l2_buffer buf{};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = (uint32_t)held_;
  ioctl(fd_, VIDIOC_QBUF, &buf);
  held_ = -1;
}

std::vector<Camera::Mode> Camera::modes(const std::string &device) {
  std::vector<Mode> out;
  int fd = ::open(device.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) return out;
  for (uint32_t index = 0;; index++) {
    v4l2_fmtdesc fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.index = index;
    if (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) < 0) break;
    const uint32_t code = fmt.pixelformat;
    for (uint32_t size = 0;; size++) {
      v4l2_frmsizeenum fs{};
      fs.index = size;
      fs.pixel_format = code;
      if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fs) < 0) break;
      if (fs.type != V4L2_FRMSIZE_TYPE_DISCRETE) break;
      /* el indice de este ioctl se cuenta sobre la lista plana de (formato, tamano)
         que lleva el controlador, no por formato: se busca a prueba hasta que
         encaje con el tamano pedido (asi lo hacen tambien las utilidades v4l2). */
      int fps = 0;
      for (uint32_t plano = 0; plano < 32; plano++) {
        v4l2_frmivalenum fi{};
        fi.index = plano;
        fi.pixel_format = code;
        fi.width = fs.discrete.width;
        fi.height = fs.discrete.height;
        if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fi) < 0) continue;
        if (fi.type != V4L2_FRMIVAL_TYPE_DISCRETE) break;
        if (fi.discrete.numerator > 0)
          fps = (int)((double)fi.discrete.denominator / (double)fi.discrete.numerator + 0.5);
        break;
      }
      out.push_back({fourccName(code), (int)fs.discrete.width, (int)fs.discrete.height, fps});
    }
    if (out.size() > 64) break;
  }
  ::close(fd);
  return out;
}


/* El nombre que anuncia el conductor de la camara estereo del monitor. */
static bool nombreEsDelMonitor(const std::string &nombre) {
  std::string bajo = nombre;
  for (char &c : bajo) c = (char)std::tolower((unsigned char)c);
  return bajo.find("stereo") != std::string::npos || bajo.find("3d") != std::string::npos;
}

bool Camera::usbIds(const std::string &device, std::string *id) {
  std::string dir;
  if (!usbSysfs(device, &dir)) return false;
  std::string ruta = dir;
  for (int subidas = 0; subidas < 8 && ruta != "/" && !ruta.empty(); subidas++) {
    std::string v = leyerPequeno(ruta + "/idVendor"), p = leyerPequeno(ruta + "/idProduct");
    if (!v.empty() && !p.empty()) {
      if (id) *id = v + ":" + p;
      return true;
    }
    const size_t barra = ruta.find_last_of('/');
    ruta = barra == std::string::npos ? std::string() : ruta.substr(0, barra);
  }
  return false;
}

bool Camera::describe(const std::string &device, Info *out) {
  Info i;
  i.path = device;
  i.nombre = leyerPequeno("/sys/class/video4linux/" + nombreNodo(device) + "/name");
  while (!i.nombre.empty() && isspace((unsigned char)i.nombre.back())) i.nombre.pop_back();
  usbIds(device, &i.usb);
  /* Primero el nombre y la id: no hace falta abrir nada para saber si es la del
     monitor, y abrir un nodo ocupado puede dar EBUSY. */
  i.esMonitor = nombreEsDelMonitor(i.nombre) || i.usb == "04e8:20d7";
  int fd = ::open(device.c_str(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
  if (fd < 0) {
    /* Ocupada (el visor la tiene) o sin permiso. Si el nodo existe y tiene nombre
       lo ensenamos igual: es el caso de "la camara la esta usando el visor". */
    if (errno == EBUSY) i.ocupada = true;
    else return false;
  } else {
    v4l2_capability cap{};
    const bool capturar = ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0 &&
                          ((cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)) ||
                           (cap.device_caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)));
    ::close(fd);
    if (!capturar) return false;   /* nodos de metadata: no dan cuadro */
    i.modos = modes(device);
    for (const Mode &m : i.modos) {
      if (m.fps > i.fpsMejor) i.fpsMejor = m.fps;
      if (m.height > 0 && m.width == 2 * m.height && m.width >= 1024) i.esMonitor = true;
    }
    if (i.modos.empty()) return false;   /* no anuncia ningun modo: no sirve para capturar */
    /* el modo que de verdad usa el rastreo va el primero de la lista: la camara del monitor
       anuncia tambien 640x480 y si no se ordena la ventana ensena ese en vez del 1280x480
       que es el par de vistas (2 x 640x480 => alto 480 = 2 * 240? no: 1280x480 = 2 mitades). */
    std::stable_sort(i.modos.begin(), i.modos.end(), [](const Mode &a, const Mode &b) {
      const bool estereoA = (a.height > 0 && a.width == 2 * a.height && a.width >= 1024);
      const bool estereoB = (b.height > 0 && b.width == 2 * b.height && b.width >= 1024);
      if (estereoA != estereoB) return estereoA;
      if (a.width * a.height != b.width * b.height) return a.width * a.height > b.width * b.height;
      return a.fps > b.fps;
    });
  }
  if (out) *out = i;
  return true;
}

std::vector<Camera::Info> Camera::listar() {
  std::vector<Info> out;
  for (int i = 0; i < 10; i++) {
    const std::string node = "/dev/video" + std::to_string(i);
    Info info;
    if (describe(node, &info)) out.push_back(info);
  }
  return out;
}

bool Camera::esEstereoDelMonitor(const std::string &device) {
  /* 1) el nombre que anuncia el conductor: "3D Stereo WebCam" en la del monitor */
  char texto[160] = {};
  const std::string sys = "/sys/class/video4linux/" + nombreNodo(device) + "/name";
  FILE *f = std::fopen(sys.c_str(), "rb");
  if (f) {
    if (std::fgets(texto, sizeof texto, f)) {
      std::string nombre(texto);
      for (char &car : nombre) car = (char)std::tolower((unsigned char)car);
      if (nombre.find("stereo") != std::string::npos || nombre.find("3d") != std::string::npos) {
        std::fclose(f);
        return true;
      }
    }
    std::fclose(f);
  }
  /* 2) la id USB del dispositivo al que cuelga la interfaz UVC */
  std::string dir;
  if (usbSysfs(device, &dir)) {
    std::string ruta = dir;
    for (int subidas = 0; subidas < 8 && ruta != "/" && !ruta.empty(); subidas++) {
      char vid[32] = {}, pid[32] = {};
      FILE *fv = std::fopen((ruta + "/idVendor").c_str(), "rb");
      FILE *fp = std::fopen((ruta + "/idProduct").c_str(), "rb");
      const bool leido = fv && fp && std::fgets(vid, sizeof vid, fv) && std::fgets(pid, sizeof pid, fp);
      if (fv) std::fclose(fv);
      if (fp) std::fclose(fp);
      if (leido) {
        std::string v(vid), p(pid);
        while (!v.empty() && isspace((unsigned char)v.back())) v.pop_back();
        while (!p.empty() && isspace((unsigned char)p.back())) p.pop_back();
        if (v == "04e8" && p == "20d7") return true;
      }
      const size_t barra = ruta.find_last_of('/');
      ruta = barra == std::string::npos ? std::string() : ruta.substr(0, barra);
    }
  }
  /* 3) senales de siempre: un modo con el doble de ancho que de alto son las dos
     vistas pegadas, que solo tiene la camara del monitor */
  for (const Mode &m : modes(device))
    if (m.height > 0 && m.width == 2 * m.height && m.width >= 1024) return true;
  return false;
}

bool Camera::pickDevice(std::string &dev, std::string *err) {
  if (!dev.empty()) return true;
  std::string alterno;
  for (int i = 0; i < 10; i++) {
    const std::string node = "/dev/video" + std::to_string(i);
    const std::string sys = "/sys/class/video4linux/video" + std::to_string(i) + "/name";
    char texto[160] = {};
    FILE *f = std::fopen(sys.c_str(), "rb");
    bool buena = false;
    if (f) {
      if (std::fgets(texto, sizeof texto, f)) {
        std::string nombre(texto);
        for (char &c : nombre) c = (char)std::tolower((unsigned char)c);
        buena = nombre.find("stereo") != std::string::npos || nombre.find("3d") != std::string::npos;
      }
      std::fclose(f);
    }
    if (buena) {
      dev = node;
      return true;
    }
    if (alterno.empty() && !modes(node).empty()) alterno = node;
  }
  if (!alterno.empty()) {
    dev = alterno;
    return true;
  }
  say(err, "no se encontro ninguna camara de video (/dev/video*)");
  return false;
}
