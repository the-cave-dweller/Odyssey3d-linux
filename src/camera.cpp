#include "camera.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

static void say(std::string *err, const std::string &text) {
  if (err) *err = text;
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
    char buf[256];
    std::snprintf(buf, sizeof buf, "VIDIOC_S_FMT %dx%d %s: %s", width, height, fourcc.c_str(),
                  std::strerror(errno));
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
