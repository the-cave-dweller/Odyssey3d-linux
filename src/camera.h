/* Captura V4L2 para la camara IR estereo del monitor (04e8:20d7) o cualquier
   webcam normal. Solo mueve bytes: la decodificacion JPEG y la deteccion van en
   track.cpp. La camara del Odyssey entrega un solo cuadro con las dos vistas
   pegadas (1280x480 = izquierda | derecha). */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Camera {
public:
  ~Camera() { close(); }

  bool open(const std::string &device, std::string *err = nullptr);
  void close();

  /* fourcc por ejemplo "MJPG" o "YUYV". fps se pasa como VIDIOC_S_PARM para que
     el conductor elija la anchura de banda (el ajuste alternativo del UVC). */
  bool setFormat(int width, int height, const std::string &fourcc, int fps, std::string *err);
  bool start(std::string *err = nullptr);
  void stop();

  /* devuelve el cuadro en el propio tampon del conductor; hay que llamar a
     release() antes de la siguiente lectura. */
  bool read(const uint8_t **data, size_t *size, int timeout_ms, std::string *err = nullptr);
  void release();

  int width() const { return w_; }
  int height() const { return h_; }
  bool mjpeg() const { return mjpeg_; }
  bool streaming() const { return running_; }
  const std::string &path() const { return path_; }
  const std::string &lastError() const { return err_; }

  /* cuatro caracteres -> entero fourcc ("MJPG" -> 0x474A4D4D) */
  static uint32_t fourccCode(const std::string &fourcc);

  /* Elige el dispositivo de captura: prefiere el nombre "3D Stereo WebCam" de la
     camara del monitor y si no, el primer /dev/videoN que anuncie modos. */
  static bool pickDevice(std::string &dev, std::string *err = nullptr);

  /* formatos que el propio dispositivo anuncia (tamanos e intervalos). */
  struct Mode {
    std::string fourcc;
    int width, height, fps;
  };
  static std::vector<Mode> modes(const std::string &device);

private:
  void freeBuffers();

  int fd_ = -1, w_ = 0, h_ = 0, held_ = -1;
  bool mjpeg_ = false, running_ = false;
  std::string err_, path_;
  std::vector<void *> maps_;
  std::vector<size_t> sizes_;
};
