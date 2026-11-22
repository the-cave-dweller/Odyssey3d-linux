/* lens.h — control del "SR Lens Switch Board" del Samsung Odyssey 3D.
   Es un dispositivo serie USB CDC-ACM (VID 354B / PID 0116) que habla un protocolo
   ASCII "SR+" terminado en CR. La lente lenticular está gatingada por un reto
   SR+AUTH (HMAC-SHA3-256); el estado autenticado persiste hasta apagar el monitor. */
#ifndef ODYSSEY_LENS_H
#define ODYSSEY_LENS_H

#include <map>
#include <string>

class Lens {
public:
  Lens() = default;
  ~Lens();

  /* Devuelve la ruta del puerto (/dev/ttyACM0) buscándolo por VID/PID en sysfs. */
  static std::string findPort();

  bool open(const std::string &port);
  void close();
  bool isOpen() const { return fd_ >= 0; }

  std::map<std::string, std::string> info();
  bool authenticated();
  bool state();
  bool on();
  bool off();
  bool authenticate();

  std::string lastError() const { return last_error_; }

private:
  std::string cmd(const std::string &c, int wait_ms = 1000);
  static std::string value(const std::string &resp);

  int fd_ = -1;
  std::string port_;
  std::string last_error_;
};

#endif
