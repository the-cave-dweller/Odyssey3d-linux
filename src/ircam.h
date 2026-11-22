/* ircam.h — control de la cámara estéreo IR "3D Stereo WebCam" (04e8:20d7).
   El iluminador IR no es un control V4L2 estándar: es una Extension Unit UVC
   (unit 3, selector 2, 1 byte). Se usa para el seguimiento de ojos/cabeza. */
#ifndef ODYSSEY_IRCAM_H
#define ODYSSEY_IRCAM_H

#include <cstdint>
#include <string>
#include <vector>

class IrCam {
public:
  ~IrCam();

  /* /dev/videoN de la cámara 04e8:20d7 (puede haber dos nodos). */
  static std::vector<std::string> findDevices();

  bool open(const std::string &dev);
  void close();

  bool getCur(int unit, int selector, std::vector<uint8_t> &out);
  bool setCur(int unit, int selector, const std::vector<uint8_t> &payload);

  bool irOn() { return setCur(3, 2, {0x01}); }
  bool irOff() { return setCur(3, 2, {0x00}); }

  std::string lastError() const { return last_error_; }

private:
  int fd_ = -1;
  std::string last_error_;
};

#endif
