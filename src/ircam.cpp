#include "ircam.h"
#include "util.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>

namespace fs = std::filesystem;

static const uint16_t CAM_VID = 0x04E8;
static const uint16_t CAM_PID = 0x20D7;

IrCam::~IrCam() { close(); }

std::vector<std::string> IrCam::findDevices() {
  std::vector<std::string> out;
  std::error_code ec;
  fs::path base("/sys/class/video4linux");
  if (!fs::is_directory(base, ec)) return out;
  std::vector<std::string> names;
  for (const auto &e : fs::directory_iterator(base, ec)) names.push_back(e.path().filename().string());
  std::sort(names.begin(), names.end());
  for (const auto &n : names) {
    std::error_code ec2;
    fs::path dev = fs::canonical(base / n / "device", ec2);
    for (int depth = 0; depth < 8 && dev != fs::path("/"); depth++, dev = dev.parent_path()) {
      std::string vid, pid;
      if (!readSmallFile((dev / "idVendor").string(), vid) || !readSmallFile((dev / "idProduct").string(), pid))
        continue;
      if (strLower(strTrim(vid)) == usbIdToHex(CAM_VID) && strLower(strTrim(pid)) == usbIdToHex(CAM_PID)) {
        out.push_back("/dev/" + n);
        break;
      }
    }
  }
  return out;
}

bool IrCam::open(const std::string &dev) {
  fd_ = ::open(dev.c_str(), O_RDWR | O_CLOEXEC);
  if (fd_ < 0) {
    last_error_ = "no se pudo abrir " + dev + ": " + std::strerror(errno);
    return false;
  }
  return true;
}

void IrCam::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

static bool do_query(int fd, int unit, int selector, int query, std::vector<uint8_t> &data) {
  uvc_xu_control_query q{};
  q.unit = (uint8_t)unit;
  q.selector = (uint8_t)selector;
  q.query = (uint8_t)query;
  q.size = (uint16_t)data.size();
  q.data = data.empty() ? nullptr : data.data();
  return ioctl(fd, UVCIOC_CTRL_QUERY, &q) == 0;
}

bool IrCam::getCur(int unit, int selector, std::vector<uint8_t> &out) {
  if (fd_ < 0) {
    last_error_ = "cámara no abierta";
    return false;
  }
  if (out.empty()) out.assign(32, 0);
  if (!do_query(fd_, unit, selector, 0x81 /*GET_CUR*/, out)) {
    last_error_ = "GET_CUR unit=" + std::to_string(unit) + " sel=" + std::to_string(selector) + ": " +
                  std::strerror(errno);
    return false;
  }
  return true;
}

bool IrCam::setCur(int unit, int selector, const std::vector<uint8_t> &payload) {
  if (fd_ < 0) {
    last_error_ = "cámara no abierta";
    return false;
  }
  std::vector<uint8_t> data = payload;
  if (!do_query(fd_, unit, selector, 0x01 /*SET_CUR*/, data)) {
    last_error_ = "SET_CUR unit=" + std::to_string(unit) + " sel=" + std::to_string(selector) + ": " +
                  std::strerror(errno) +
                  " (el endpoint de control de esta cámara suele estar gateado hasta autenticar la lente "
                  "y subir el stream de video)";
    return false;
  }
  return true;
}
