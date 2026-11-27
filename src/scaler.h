/* scaler.h — flag 3D del escalador MediaTek del panel, por SCSI vendor (SG_IO).
   El monitor expone un disco SCSI genérico (/dev/sgN) con vendor "GCREADER". */
#ifndef ODYSSEY_SCALER_H
#define ODYSSEY_SCALER_H

#include <string>

class Scaler {
public:
  ~Scaler();
  static std::string findDevice(); /* /dev/sgN con vendor GCREADER, o vacío */
  bool open(const std::string &dev);
  void close();
  /* Enciende (1) o apaga (0) el modo 3D del escalador. */
  bool set3d(bool on, int *status = nullptr, int *host_status = nullptr);
  std::string lastError() const { return last_error_; }
  /* (v1.16) los bytes que el monitor no tomo en la ultima orden (sg_io_hdr.resid): se apuntan en el registro para ver
     si alguna vez no es 0 antes de tomarlo por fallo */
  int ultimoResid() const { return resid_; }

private:
  int fd_ = -1;
  int resid_ = 0;
  std::string last_error_;
};

#endif
