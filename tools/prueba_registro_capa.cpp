/* prueba_registro_capa.cpp — (v1.18) el registro de la capa con un juego que apunta en cada cuadro. El codigo de la capa
   entero en este programa (sin Vulkan: no se llama a nada de la tarjeta). El registro sale donde el de los juegos,
   /tmp/odysseyvk-capa-<uid>/capa-<pid de la prueba>.log.
     g++ -O1 -std=gnu++17 -Icapa -Ibuild/capa tools/prueba_registro_capa.cpp -lpthread -o /tmp/registro
     /tmp/registro cupo 25                    dos lineas en cada cuadro a 60 fps y una cada 10 s que no se calla
     ODYSSEYVK_CAPA_SONDA=1 /tmp/registro tope  sin cupo (como con la sonda): el tope de 4 MB del archivo
     /tmp/registro medida                     dos medidas de profundidad turnandose, y un cambio de verdad */
#include "../capa/odysseyvk_capa.cpp"

#include <thread>

int main(int argc, char **argv) {
  const std::string modo = argc > 1 ? argv[1] : "cupo";
  if (modo == "cupo") {
    const double segundos = argc > 2 ? std::atof(argv[2]) : 25.0;
    const double t0 = ahora();
    long cuadro = 0;
    double ultimaLenta = t0;
    while (ahora() - t0 < segundos) {
      cuadro++;
      anota("[prueba] la misma linea en cada cuadro: %ld", cuadro);
      anota("[prueba] otra, tambien en cada cuadro: %ld", cuadro);
      if (ahora() - ultimaLenta >= 10.0) {
        ultimaLenta = ahora();
        anota("[prueba] una cada 10 s, como el resumen: cuadro %ld", cuadro);
      }
      std::this_thread::sleep_for(std::chrono::microseconds(16667));
    }
    std::printf("cuadros %ld en %.1f s\n", cuadro, ahora() - t0);
  } else if (modo == "tope") {
    std::string kb(1000, 'x');
    for (int i = 0; i < 6000; i++) anota("[prueba] %d %s", i, kb.c_str());
    std::printf("6000 lineas de 1 KB pedidas\n");
  } else if (modo == "medida") {
    Dispositivo d;
    static uint8_t region[16];
    d.shm = region;   /* como si ya hubiera una de 2228x1256 */
    d.shmW = 2228, d.shmH = 1256, d.shmFmt = VK_FORMAT_D32_SFLOAT_S8_UINT;
    Imagen a, b;
    a.w = 2228, a.h = 1256, a.formato = VK_FORMAT_D32_SFLOAT_S8_UINT;
    b.w = 1280, b.h = 720, b.formato = VK_FORMAT_D32_SFLOAT_S8_UINT;
    /* lo mismo que compartir() antes de crearCompartida, con el reloj de mentira */
    auto rehace = [&](const Imagen &im, double t) {
      if (d.shm && d.shmW == im.w && d.shmH == im.h && d.shmFmt == im.formato) {
        d.nuevaDesde = -1.0;
        return false;
      }
      if (!medidaAguanta(&d, im, t)) return false;
      d.nuevaDesde = -1.0;
      d.shmW = im.w, d.shmH = im.h;   /* «crearCompartida» */
      return true;
    };
    int rehechas = 0;
    double t = 100.0;
    for (int k = 0; k < 600; k++, t += 1.0 / 60.0) rehechas += rehace(k % 2 ? b : a, t);   /* 10 s turnandose */
    std::printf("turnandose dos medidas 10 s a 60 fps: %d veces rehecha (antes: 600)\n", rehechas);
    int cuando = -1;
    for (int k = 0; k < 120; k++, t += 1.0 / 60.0)
      if (rehace(b, t) && cuando < 0) cuando = k;
    std::printf("cambio de verdad a 1280x720: rehecha en el cuadro %d (%.2f s)\n", cuando, cuando / 60.0);
  }
  return 0;
}
