#include "lens.h"
#include "sha3.h"
#include "util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace fs = std::filesystem;

static const uint16_t FPC_VID = 0x354B;
static const uint16_t FPC_PID = 0x0116;

/* Constante fija del protocolo SR+AUTH (la misma para todos los aparatos de esta clase). */
static const uint8_t AUTH_K16[16] = {0xdb, 0xa4, 0x44, 0xfb, 0x61, 0xc9, 0x06, 0xf2,
                                     0xe7, 0x4c, 0x45, 0xa0, 0xf9, 0xb3, 0x6d, 0x8d};

Lens::~Lens() { close(); }

std::string Lens::findPort() {
  std::error_code ec;
  fs::path base("/sys/class/tty");
  if (!fs::is_directory(base, ec)) return {};
  for (const auto &e : fs::directory_iterator(base, ec)) {
    std::string name = e.path().filename().string();
    if (name.rfind("ttyACM", 0) != 0) continue;
    std::error_code ec2;
    fs::path dev = fs::canonical(e.path() / "device", ec2);
    for (int depth = 0; depth < 8 && dev != fs::path("/"); depth++, dev = dev.parent_path()) {
      std::string vid, pid;
      if (!readSmallFile((dev / "idVendor").string(), vid) || !readSmallFile((dev / "idProduct").string(), pid))
        continue;
      if (strLower(strTrim(vid)) == usbIdToHex(FPC_VID) && strLower(strTrim(pid)) == usbIdToHex(FPC_PID))
        return "/dev/" + name;
    }
  }
  return {};
}

bool Lens::open(const std::string &port) {
  port_ = port;
  fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (fd_ < 0) {
    last_error_ = "no se pudo abrir " + port + ": " + std::strerror(errno);
    return false;
  }
  struct termios t;
  if (tcgetattr(fd_, &t) != 0) {
    last_error_ = std::string("tcgetattr: ") + std::strerror(errno);
    close();
    return false;
  }
  cfmakeraw(&t);
  cfsetspeed(&t, B115200);
  t.c_cflag |= (CLOCAL | CREAD);
  t.c_cc[VMIN] = 0;
  t.c_cc[VTIME] = 0;
  tcsetattr(fd_, TCSANOW, &t);
  unsigned bits = TIOCM_DTR | TIOCM_RTS;
  ioctl(fd_, TIOCMBIS, &bits);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  return true;
}

void Lens::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

std::string Lens::cmd(const std::string &c, int wait_ms) {
  if (fd_ < 0) return {};
  tcflush(fd_, TCIFLUSH);
  const std::string line = c + "\r";
  size_t off = 0;
  while (off < line.size()) {
    ssize_t w = ::write(fd_, line.data() + off, line.size() - off);
    if (w <= 0) break;
    off += (size_t)w;
  }
  std::string buf;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now())
                 .count();
    struct pollfd pf{fd_, POLLIN, 0};
    int pr = poll(&pf, 1, ms > 0 ? ms : 0);
    if (pr > 0) {
      char tmp[512];
      ssize_t n = ::read(fd_, tmp, sizeof tmp);
      if (n > 0) buf.append(tmp, (size_t)n);
      if (buf.find("OK\r\n") != std::string::npos || buf.find("ERROR") != std::string::npos) break;
    }
  }
  return buf;
}

std::string Lens::value(const std::string &resp) {
  std::string s = resp;
  for (char &c : s)
    if (c == '\r') c = '\n';
  std::vector<std::string> lines;
  size_t pos = 0;
  while (pos <= s.size()) {
    size_t nl = s.find('\n', pos);
    std::string ln = s.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    if (!strTrim(ln).empty()) lines.push_back(strTrim(ln));
    if (nl == std::string::npos) break;
    pos = nl + 1;
  }
  if (!lines.empty() && lines.back() == "OK") lines.pop_back();
  std::string out;
  for (size_t i = 0; i < lines.size(); i++) {
    if (i) out += "\n";
    out += lines[i];
  }
  return out;
}

std::map<std::string, std::string> Lens::info() {
  std::map<std::string, std::string> out;
  std::string v = value(cmd("SR+INFO"));
  size_t pos = 0;
  while (pos <= v.size()) {
    size_t nl = v.find('\n', pos);
    std::string ln = v.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    size_t colon = ln.find(':');
    if (colon != std::string::npos) out[strTrim(ln.substr(0, colon))] = strTrim(ln.substr(colon + 1));
    if (nl == std::string::npos) break;
    pos = nl + 1;
  }
  return out;
}

bool Lens::authenticated() {
  auto i = info();
  return i.count("Authenticated") && i["Authenticated"] == "1";
}

bool Lens::state() { return value(cmd("SR+LENS")) == "1"; }

bool Lens::on() {
  std::string r = cmd("SR+LENS=1");
  if (r.find("NOT AUTHENTICATED") != std::string::npos) {
    last_error_ = "el tablero rechazó SR+LENS=1: no autenticado (ejecuta authenticate())";
    return false;
  }
  return r.find("OK") != std::string::npos;
}

bool Lens::off() { return cmd("SR+LENS=0").find("OK") != std::string::npos; }

bool Lens::authenticate() {
  if (authenticated()) return true;
  auto infoMap = info();
  auto get = [&](const char *k) { return infoMap.count(k) ? infoMap[k] : std::string(); };
  std::vector<uint8_t> chipid = unhex(get("ChipID"));
  std::vector<uint8_t> sechip = unhex(get("SEChipID"));
  if (chipid.size() != 16 || sechip.size() < 14) {
    last_error_ = "SR+INFO no reporta ChipID/SEChipID usables (ChipID=" + get("ChipID") +
                  " SEChipID=" + get("SEChipID") + ")";
    return false;
  }

  /* nonce del host: 64 bytes aleatorios */
  std::array<uint8_t, 64> hn{};
  FILE *ur = fopen("/dev/urandom", "rb");
  if (ur) {
    fread(hn.data(), 1, hn.size(), ur);
    fclose(ur);
  } else {
    for (size_t i = 0; i < hn.size(); i++) hn[(size_t)i] = (uint8_t)(i * 37 + 11);
  }
  cmd("SR+NONCE=" + toHexUpper(hn.data(), hn.size()));
  cmd("SR+AUTH"); /* reto del dispositivo (lectura) */
  std::string dnHex = value(cmd("SR+NONCE"));
  size_t sp = dnHex.find_first_of(" \n\r\t");
  if (sp != std::string::npos) dnHex = dnHex.substr(0, sp);
  if (dnHex.size() >= 4 && strLower(dnHex.substr(dnHex.size() - 4)) == "aa01")
    dnHex = dnHex.substr(0, dnHex.size() - 4);
  std::vector<uint8_t> dn = unhex(dnHex);
  if (dn.size() < 64) dn.resize(64, 0);

  std::vector<uint8_t> hdr;
  hdr.insert(hdr.end(), sechip.begin() + 6, sechip.begin() + 14);
  hdr.insert(hdr.end(), 16, 0xff);
  hdr.insert(hdr.end(), 16, 0x00);

  uint8_t s0[32], s1[32];
  {
    std::vector<uint8_t> m = hdr;
    m.insert(m.end(), dn.begin(), dn.begin() + 32);
    m.insert(m.end(), {0x80, 0x07, 0x00});
    hmac_sha3_256(AUTH_K16, sizeof AUTH_K16, m.data(), m.size(), s0);
  }
  {
    std::vector<uint8_t> m = hdr;
    m.insert(m.end(), dn.begin() + 32, dn.begin() + 64);
    m.insert(m.end(), {0x00, 0x07, 0x00});
    hmac_sha3_256(s0, 32, m.data(), m.size(), s1);
  }
  uint8_t resp[32];
  for (int i = 0; i < 32; i++) resp[i] = (uint8_t)(s1[i] ^ chipid[(size_t)i % 16]);
  cmd("SR+AUTH=" + toHexUpper(resp, 32));
  return authenticated();
}

bool Lens::readFixed(const std::string &command, size_t payloadSize, int wait_ms,
                     std::vector<uint8_t> &out) {
  out.clear();
  if (fd_ < 0) {
    last_error_ = "puerto cerrado";
    return false;
  }
  tcflush(fd_, TCIFLUSH);
  std::string line = command + "\r";
  if (ssize_t w = ::write(fd_, line.data(), line.size()); w != (ssize_t)line.size()) {
    last_error_ = "fallo escribiendo " + command;
    return false;
  }
  /* El tablero responde "\r\n" + payload + "\r\nOK\r\n". El payload binario puede
     contener CRLF o incluso "OK" dentro, asi que solo cuenta el tamano exacto. */
  const size_t total = payloadSize + 8;
  std::vector<uint8_t> buf;
  buf.reserve(total);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
  while (buf.size() < total) {
    int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                        std::chrono::steady_clock::now())
                 .count();
    if (ms <= 0) break;
    struct pollfd pf{fd_, POLLIN, 0};
    int pr = poll(&pf, 1, ms);
    if (pr <= 0) break;
    uint8_t tmp[4096];
    ssize_t n = ::read(fd_, tmp, sizeof tmp);
    if (n > 0)
      buf.insert(buf.end(), tmp, tmp + (size_t)n);
    else if (n < 0 && errno != EAGAIN && errno != EINTR)
      break;
  }
  if (buf.size() < total) {
    last_error_ = command + ": respuesta corta (" + std::to_string(buf.size()) + " de " +
                  std::to_string(total) + " bytes)";
    return false;
  }
  if (buf[0] != '\r' || buf[1] != '\n' || memcmp(buf.data() + total - 6, "\r\nOK\r\n", 6) != 0) {
    last_error_ = command + ": respuesta no reconocida";
    return false;
  }
  out.assign(buf.begin() + 2, buf.begin() + 2 + payloadSize);
  return true;
}

