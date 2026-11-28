/* prueba_pantallas.cpp — las pantallas y su clase (src/pantallas.h), con EDID hechos a mano y un /sys/class/drm de mentira.
   g++ -std=c++17 -O1 -Isrc tools/prueba_pantallas.cpp -o /tmp/pantallas && /tmp/pantallas [CARPETA_DE_PRUEBA] */
#include "pantallas.h"

#include <cstdio>

static int fallos = 0;
#define MIRA(c, que)                                         \
  do {                                                       \
    const bool bien_ = (c);                                  \
    if (!bien_) ++fallos;                                    \
    std::printf("%s  %s\n", bien_ ? "bien " : "MAL  ", que); \
  } while (0)

/* un EDID de 128 bytes con ese fabricante, ese nombre y esa resolucion */
static std::string edid(const char *fab, const char *nombre, int ancho, int alto) {
  std::string e(128, '\0');
  const unsigned m = (unsigned)((fab[0] - 64) << 10 | (fab[1] - 64) << 5 | (fab[2] - 64));
  e[8] = (char)(m >> 8), e[9] = (char)(m & 255);
  unsigned char *d = (unsigned char *)&e[54];   /* el detalle de tiempos */
  d[0] = 1, d[1] = 1;
  d[2] = (unsigned char)(ancho & 255), d[4] = (unsigned char)((ancho >> 8) << 4);
  d[5] = (unsigned char)(alto & 255), d[7] = (unsigned char)((alto >> 8) << 4);
  unsigned char *n = (unsigned char *)&e[72];   /* el nombre */
  n[3] = 0xFC;
  std::string t = nombre;
  t += '\n';
  while (t.size() < 13) t += ' ';
  for (int i = 0; i < 13; i++) n[5 + i] = (unsigned char)t[(size_t)i];
  return e;
}

int main(int argc, char **argv) {
  using namespace pantallas;
  {
    const Pantalla p = deEdid("DP-4", edid("SAM", "Odyssey G90XF", 3840, 2160));
    MIRA(p.fabricante == "SAM" && p.nombre == "Odyssey G90XF" && p.ancho == 3840 && p.alto == 2160, "el EDID del Odyssey: fabricante, nombre y resolucion");
    MIRA(p.clase == Con3D && p.rotulo() == "DP-4 · Odyssey G90XF", "y es la de 3D");
  }
  MIRA(deEdid("DP-7", edid("SUN", "X7149A", 1600, 1200)).clase == Normal, "el otro monitor de Rodolfo (SUN X7149A), normal");
  MIRA(clasifica("SAM", "Odyssey G7") == Normal, "un Odyssey sin lente (G7) no es de 3D");
  MIRA(clasifica("SAM", "Odyssey 3D") == Con3D, "un Samsung con «3D» en el nombre, si");
  MIRA(clasifica("ACR", "SpatialLabs View") == Candidata && clasifica("ACR", "ASV27-2P") == Candidata, "Acer SpatialLabs: candidata");
  MIRA(clasifica("SNY", "ELF-SR2") == Candidata, "Sony ELF-SR2: candidata");
  MIRA(deEdid("HDMI-A-1", "corto").nombre.empty() && deEdid("HDMI-A-1", "corto").clase == Normal, "sin EDID entero: normal y sin nombre");
  {
    const std::vector<Pantalla> v = deTexto("HDMI-A-1|GSM|LG ULTRAGEAR|2560x1440;DP-4|SAM|Odyssey G90XF|3840x2160");
    MIRA(v.size() == 2 && !hay3D({v[0]}) && hay3D(v) && v[0].ancho == 2560, "las de la prueba, de su texto");
    MIRA(deTexto("").empty() && !hay3D({}), "ninguna pantalla: sin 3D");
  }
  if (argc > 1) {   /* un /sys/class/drm de mentira: dos conectadas y una suelta */
    namespace fs = std::filesystem;
    const std::string r = argv[1];
    auto pon = [&](const char *c, const char *estado, const std::string &e) {
      fs::create_directories(r + "/" + c);
      std::ofstream(r + "/" + c + "/status") << estado << "\n";
      std::ofstream(r + "/" + c + "/edid", std::ios::binary) << e;
    };
    pon("card0-DP-7", "connected", edid("SUN", "X7149A", 1600, 1200));
    pon("card2-DP-4", "connected", edid("SAM", "Odyssey G90XF", 3840, 2160));
    pon("card2-HDMI-A-1", "disconnected", "");
    fs::create_directories(r + "/card2");
    const std::vector<Pantalla> v = lista(r);
    MIRA(v.size() == 2 && v[0].conector == "DP-4" && v[0].clase == Con3D && v[1].conector == "DP-7", "del nucleo: las dos conectadas, la de 3D primero");
  }
  std::printf("%s\n", fallos ? "HAY FALLOS" : "todo bien");
  return fallos ? 1 : 0;
}
