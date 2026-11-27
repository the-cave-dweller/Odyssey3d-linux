#include "scaler.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <vector>

namespace fs = std::filesystem;

static const unsigned char CDB_SET[16] = {0xE9, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                          0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

Scaler::~Scaler() { close(); }

std::string Scaler::findDevice() {
  std::error_code ec;
  fs::path base("/sys/class/scsi_generic");
  if (!fs::is_directory(base, ec)) return {};
  std::vector<std::string> names;
  for (const auto &e : fs::directory_iterator(base, ec)) names.push_back(e.path().filename().string());
  std::sort(names.begin(), names.end());
  for (const auto &n : names) {
    std::ifstream f((base / n / "device" / "vendor").string());
    if (!f) continue;
    std::string vendor;
    std::getline(f, vendor);
    for (char &c : vendor) c = (char)toupper((unsigned char)c);
    if (vendor.find("GCREADER") != std::string::npos) return "/dev/" + n;
  }
  return {};
}

bool Scaler::open(const std::string &dev) {
  fd_ = ::open(dev.c_str(), O_RDWR | O_CLOEXEC);
  if (fd_ < 0) {
    last_error_ = "no se pudo abrir " + dev + ": " + std::strerror(errno) +
                  "  (hace falta una regla udev para el dispositivo GCREADER)";
    return false;
  }
  return true;
}

void Scaler::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool Scaler::set3d(bool on, int *status, int *host_status) {
  if (fd_ < 0) {
    last_error_ = "scaler no abierto";
    return false;
  }
  unsigned char data[512] = {0};
  data[0] = on ? 1 : 0;
  data[4] = 0xD2;
  unsigned char cdb[16];
  memcpy(cdb, CDB_SET, 16);
  unsigned char sense[32] = {0};

  sg_io_hdr_t h{};
  h.interface_id = 'S';
  h.dxfer_direction = SG_DXFER_TO_DEV;
  h.cmd_len = 16;
  h.mx_sb_len = sizeof sense;
  h.dxfer_len = sizeof data;
  h.dxferp = data;
  h.cmdp = cdb;
  h.sbp = sense;
  h.timeout = 5000;
  int rc = ioctl(fd_, SG_IO, &h);
  if (status) *status = h.status;
  if (host_status) *host_status = h.host_status;
  resid_ = rc < 0 ? 0 : h.resid;
  if (rc < 0) {
    last_error_ = std::string("ioctl SG_IO: ") + std::strerror(errno);
    return false;
  }
  /* (v1.16, de la PR #2 de Noa3) que la llamada vuelva bien no quiere decir que el monitor tomara la orden: un error del
     adaptador (host) o del conductor (driver) tambien es un fallo. Los bytes que no tomo (resid) solo se apuntan: en el
     registro de Rodolfo hay 110 activaciones buenas en 30 dias y ninguna decia cuantos; se mira antes de rechazarlas */
  if (h.status != 0 || h.host_status != 0 || h.driver_status != 0) {
    last_error_ = "el scaler respondió status=" + std::to_string(h.status) +
                  " host=" + std::to_string(h.host_status) + " driver=" + std::to_string(h.driver_status) +
                  " resid=" + std::to_string(h.resid);
    return false;
  }
  return true;
}
