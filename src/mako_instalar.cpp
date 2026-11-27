/* (v1.18) la instalacion de MAKO FG, el generador de MAKO (ver mako.h): `odysseyvk mako instalar`.
   Rodolfo: «offer an install button» y «quiero que tu controles a mako, no quiero tener que ver con su ui».

   Se baja cada archivo de la revision fijada de Hugging Face con un `curl` aparte (solo https, tambien en las
   redirecciones a su CDN) a una carpeta aparte, `mako-fg.descarga`; se comprueba su tamano y su SHA-256 contra los que se
   midieron el 27-nov; y solo con todo bien se cambia por la carpeta de antes, de una vez. Lo ultimo que se escribe es la
   marca `instalado` (con la version), que es lo que mira hostPropio(): a medias no queda nada que parezca puesto. Como va,
   en ~/.local/state/odysseyvk/mako-fg (lo leen los botones con leerInstalacion). Sin root: todo bajo HOME. */
#include "idioma.h"
#include "integraciones.h"
#include "mako.h"
#include "util.h"

#include <fcntl.h>
#include <openssl/evp.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

namespace mako {
namespace {

std::string sha256De(const std::string &ruta) {
  std::ifstream f(ruta, std::ios::binary);
  if (!f) return "";
  EVP_MD_CTX *c = EVP_MD_CTX_new();
  if (!c) return "";
  std::string r;
  if (EVP_DigestInit_ex(c, EVP_sha256(), nullptr) == 1) {
    std::vector<char> buf(1 << 16);
    bool bien = true;
    while (bien && f) {
      f.read(buf.data(), (std::streamsize)buf.size());
      if (f.gcount() > 0) bien = EVP_DigestUpdate(c, buf.data(), (size_t)f.gcount()) == 1;
    }
    unsigned char d[EVP_MAX_MD_SIZE];
    unsigned int n = 0;
    if (bien && f.eof() && EVP_DigestFinal_ex(c, d, &n) == 1) {
      static const char *hx = "0123456789abcdef";
      for (unsigned int i = 0; i < n; i++) r += hx[d[i] >> 4], r += hx[d[i] & 15];
    }
  }
  EVP_MD_CTX_free(c);
  return r;
}

/* como va, entero cada vez (a un .tmp y rename: quien lo lea nunca ve medio) */
struct Fallo {
  std::string motivo;    /* en el idioma de la interfaz (idioma::tr) */
  std::string archivo;   /* el que fallo, o vacio */
  std::string detalle;   /* lo de la maquina (un codigo, una ruta), tal cual */
  std::string texto() const {
    return (archivo.empty() ? std::string() : archivo + ": ") + motivo + (detalle.empty() ? std::string() : " (" + detalle + ")");
  }
};
std::string unaLinea(std::string t) {
  for (char &ch : t)
    if (ch == '\n' || ch == '\r' || ch == '#') ch = ' ';
  return t;
}
void anotar(const char *estado, long hecho, long total, const Fallo &fallo = Fallo{}) {
  const std::string ruta = rutaEstadoOdyssey("mako-fg");
  const std::string tmp = ruta + ".tmp";
  FILE *f = std::fopen(tmp.c_str(), "we");
  if (!f) return;
  std::fprintf(f, "# la instalacion de MAKO FG (odysseyvk mako instalar)\npid = %ld\nestado = %s\nhecho = %ld\ntotal = %ld\nhora = %ld\n",
               (long)::getpid(), estado, hecho, total, (long)std::time(nullptr));
  if (!fallo.motivo.empty()) std::fprintf(f, "error = %s\n", unaLinea(fallo.motivo).c_str());
  if (!fallo.archivo.empty()) std::fprintf(f, "archivo = %s\n", unaLinea(fallo.archivo).c_str());
  if (!fallo.detalle.empty()) std::fprintf(f, "detalle = %s\n", unaLinea(fallo.detalle).c_str());
  std::fclose(f);
  ::rename(tmp.c_str(), ruta.c_str());
}

/* lo que quiere decir el codigo de salida de curl (su manual, EXIT CODES) */
Fallo porQueCurl(int codigo) {
  switch (codigo) {
    case 127: return {idioma::tr("falta `curl`: instálalo y vuelve a intentarlo"), "", ""};
    case 6:
    case 7: return {idioma::tr("sin conexión: no se llegó a Hugging Face"), "", "curl " + std::to_string(codigo)};
    case 22: return {idioma::tr("el servidor no lo dio (error HTTP)"), "", "curl 22"};
    case 28: return {idioma::tr("tardó demasiado: vuelve a intentarlo"), "", "curl 28"};
    case 23: return {idioma::tr("no se pudo escribir en el disco"), "", "curl 23"};
    default: return {idioma::tr("la descarga falló"), "", "curl " + std::to_string(codigo)};
  }
}

/* un archivo con su `curl`, sin heredar nada nuestro, mirando lo que lleva cada 0,2 s */
bool bajar(const std::string &url, const std::string &destino, long base, long total,
           const std::function<void(long, long)> &avance, Fallo *err) {
  const bool local = url.rfind("file://", 0) == 0;   /* solo las pruebas (ODYSSEYVK_MAKO_FG_ORIGEN) */
  const pid_t p = ::fork();
  if (p < 0) {
    if (err) *err = {idioma::tr("no se pudo arrancar curl"), "", std::strerror(errno)};
    return false;
  }
  if (p == 0) {
    const int nulo = ::open("/dev/null", O_RDWR);
    if (nulo >= 0) ::dup2(nulo, 0), ::dup2(nulo, 1);
    ::execlp("curl", "curl", "-fsSL", "--retry", "2", "--connect-timeout", "20", "--max-time", "900", "--proto",
             local ? "=file" : "=https", "--proto-redir", "=https", "-o", destino.c_str(), url.c_str(), (char *)nullptr);
    ::_exit(127);
  }
  long antes = -1;
  for (;;) {
    int st = 0;
    const pid_t r = ::waitpid(p, &st, WNOHANG);
    struct stat sb {};
    const long lleva = ::stat(destino.c_str(), &sb) == 0 ? (long)sb.st_size : 0;
    if (lleva != antes) {
      antes = lleva;
      anotar("bajando", base + lleva, total);
      if (avance) avance(base + lleva, total);
    }
    if (r == p) {
      if (WIFEXITED(st) && WEXITSTATUS(st) == 0) return true;
      if (err) *err = porQueCurl(WIFEXITED(st) ? WEXITSTATUS(st) : -1);
      return false;
    }
    if (r < 0 && errno != EINTR) {
      if (err) *err = {idioma::tr("la descarga falló"), "", std::strerror(errno)};
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

/* nunca se borra nada que no sea una de nuestras tres carpetas */
bool carpetaNuestra(const std::string &d) {
  const std::string fin = "/.local/share/odysseyvk/mako-fg";
  return d.size() > fin.size() && d.compare(d.size() - fin.size(), fin.size(), fin) == 0;
}

}  // namespace

bool instalarGenerador(std::string *err, const std::function<void(long, long)> &avance) {
  const long total = bytesGenerador();
  auto falla = [&](const Fallo &f) {
    anotar("fallo", 0, total, f);
    if (err) *err = f.texto();
    return false;
  };
  /* una a la vez (dos clics seguidos arrancan dos): el cerrojo vale mientras vive esta; la otra no toca nada */
  struct Cerrojo {
    int fd = -1;
    ~Cerrojo() {
      if (fd >= 0) ::close(fd);
    }
  } cerrojo;
  cerrojo.fd = ::open(rutaEstadoOdyssey("mako-fg.cerrojo").c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
  if (cerrojo.fd < 0 || ::flock(cerrojo.fd, LOCK_EX | LOCK_NB) != 0) {
    if (err) *err = idioma::tr("ya hay una instalación en marcha");
    return false;
  }
  const std::string dir = carpetaGenerador();
  if (!carpetaNuestra(dir)) return falla({idioma::tr("no se pudo crear la carpeta"), "", dir});
  const std::string nueva = dir + ".descarga", vieja = dir + ".vieja";
  std::error_code ec;
  std::filesystem::remove_all(nueva, ec);
  ec.clear();
  std::filesystem::create_directories(nueva + "/model", ec);
  if (ec) return falla({idioma::tr("no se pudo crear la carpeta"), "", nueva + ": " + ec.message()});
  anotar("bajando", 0, total);
  const std::string origen = origenGenerador();
  long hecho = 0;
  for (const ArchivoGenerador &a : archivosGenerador()) {
    const std::string destino = nueva + "/" + a.destino;
    Fallo f;
    bool bien = bajar(origen + "/" + a.origen, destino, hecho, total, avance, &f);
    struct stat sb {};
    if (bien && (::stat(destino.c_str(), &sb) != 0 || (long)sb.st_size != a.bytes))
      bien = false, f = {idioma::tr("bajó incompleto: vuelve a intentarlo"), "",
                         std::to_string((long)sb.st_size) + " / " + std::to_string(a.bytes) + " B"};
    if (bien && sha256De(destino) != a.sha256)
      bien = false, f = {idioma::tr("no es el archivo que se esperaba (su SHA-256 no cuadra): no se instala"), "", ""};
    if (bien && a.ejecutable && ::chmod(destino.c_str(), 0755) != 0)
      bien = false, f = {idioma::tr("no se pudo escribir en el disco"), "", std::strerror(errno)};
    if (!bien) {
      std::filesystem::remove_all(nueva, ec);
      f.archivo = a.destino;
      return falla(f);
    }
    hecho += a.bytes;
    std::printf("[mako-fg] %s · %ld bytes · SHA-256 bien\n", a.destino, a.bytes);
    std::fflush(stdout);
  }
  /* la marca, lo ultimo de la carpeta nueva */
  {
    std::ofstream m(nueva + "/instalado");
    m << "# MAKO FG, instalado por OdysseyVK (odysseyvk mako instalar). Licencia CC BY-NC 4.0: uso no comercial.\n"
      << "version = " << kVersionGenerador << "\norigen = " << origen << "\nhora = " << (long)std::time(nullptr) << "\n";
    if (!m) {
      std::filesystem::remove_all(nueva, ec);
      return falla({idioma::tr("no se pudo escribir en el disco"), "instalado", ""});
    }
  }
  /* el cambio: la de antes aparte, la nueva en su sitio y la de antes fuera */
  std::filesystem::remove_all(vieja, ec);
  ec.clear();
  const bool habia = std::filesystem::exists(dir, ec);
  if (habia) {
    std::filesystem::rename(dir, vieja, ec);
    if (ec) {
      const std::string m = ec.message();
      std::filesystem::remove_all(nueva, ec);
      return falla({idioma::tr("no se pudo escribir en el disco"), "", dir + ": " + m});
    }
  }
  std::filesystem::rename(nueva, dir, ec);
  if (ec) {
    const std::string m = ec.message();
    if (habia) std::filesystem::rename(vieja, dir, ec);
    std::filesystem::remove_all(nueva, ec);
    return falla({idioma::tr("no se pudo escribir en el disco"), "", dir + ": " + m});
  }
  std::filesystem::remove_all(vieja, ec);
  anotar("lista", total, total);
  /* las lineas de Steam de los juegos con MAKO llevan ahora MAKO_FG_HOST: el servicio las escribe con Steam cerrado */
  integ::ponerMarca();
  std::printf("[mako-fg] MAKO FG %s instalado en %s (%ld bytes)\n", kVersionGenerador, dir.c_str(), total);
  std::fflush(stdout);
  return true;
}

bool quitarGenerador(std::string *err) {
  const std::string dir = carpetaGenerador();
  if (!carpetaNuestra(dir)) {
    if (err) *err = "carpeta rara: " + dir;
    return false;
  }
  std::error_code ec;
  for (const std::string &d : {dir + ".descarga", dir + ".vieja", dir}) {
    std::filesystem::remove_all(d, ec);
    if (ec) {
      if (err) *err = d + ": " + ec.message();
      return false;
    }
  }
  ::unlink(rutaEstadoOdyssey("mako-fg").c_str());
  integ::ponerMarca();
  return true;
}

}  // namespace mako
