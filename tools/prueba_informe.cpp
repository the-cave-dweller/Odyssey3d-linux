/* prueba_informe.cpp — el informe de un error (src/informe.h), lo que no lee el equipo: que no salgan el usuario, su
   carpeta ni el nombre del equipo; que del registro solo quede lo que cuenta; y que la direccion de GitHub quepa.
   g++ -std=c++17 -O1 -Isrc tools/prueba_informe.cpp src/util.cpp src/monitor.cpp -ldl -o /tmp/informe && /tmp/informe   */
#include "informe.h"

#include <cstdio>

static int fallos = 0;
#define MIRA(c, que)                                           \
  do {                                                         \
    const bool bien_ = (c);                                    \
    if (!bien_) ++fallos;                                      \
    std::printf("%s  %s\n", bien_ ? "bien " : "MAL  ", que);   \
  } while (0)

int main() {
  using namespace informe;
  const std::string sucio = "modelo = /home/ana/.local/share/odysseyvk/models/x.onnx\nequipo torre-de-ana, usuario ana, /home/ana/juegos\n";
  const std::string l = limpiar(sucio, "/home/ana", "ana", "torre-de-ana");
  MIRA(l.find("ana") == std::string::npos && l.find("torre") == std::string::npos, "ni el usuario, ni su carpeta, ni el nombre del equipo");
  MIRA(l.find("modelo = ~/.local/share/odysseyvk/models/x.onnx") != std::string::npos, "la carpeta personal queda en ~");
  MIRA(limpiar("banana y io", "/home/io", "io", "pc") == "banana y io", "un usuario o un equipo de menos de 3 letras no se toca (saldria de otras palabras)");

  std::string reg;
  for (int k = 0; k < 400; k++) reg += "[rastreo] camara (0,0,0) mm · error 0.00 px · siguiendo\n[fps] 165.0 · fuente 164\n";
  reg += "[serve] lanzando 3D (conv) sin preguntar por steam_app_1091500\n";
  reg += "[captura] vkAllocateMemory (DMA-BUF sin modificador): fallo de inicialización\n";
  reg += "[conversor] bandas negras: arriba 0 %\n";
  reg += "[ventana] la ventana del video salio de pantalla completa: cerramos la sesion 3D\n";
  reg += "[serve] " + std::string(400, 'x') + " error\n";
  const std::vector<std::string> f = filtrarRegistro(reg);
  MIRA(f.size() == 4, "del registro quedan los 4 renglones que cuentan algo (ni el rastreo ni los FPS de cada segundo)");
  MIRA(f.size() == 4 && f[3].size() <= 204, "y el largo, cortado a 200 letras");
  std::string muchos;
  for (int k = 0; k < 100; k++) muchos += "[serve] la sesion se cerro apenas empezo (" + std::to_string(k) + ")\n";
  const std::vector<std::string> m = filtrarRegistro(muchos);
  MIRA(m.size() == 30 && m.back().find("(99)") != std::string::npos && m.front().find("(70)") != std::string::npos, "como mucho 30, y son los ultimos");

  const std::vector<std::string> ti = filtrarRegistro("[serve] lanzando 3D por zen | Un video cualquiera - YouTube — Zen Browser: el tipo lo decide\n");
  MIRA(ti.size() == 1 && ti[0] == "[serve] lanzando 3D por zen | (título quitado)", "el titulo de la ventana (lo que se estaba viendo) no sale");
  const std::vector<std::string> rep = filtrarRegistro("[serve] la sesion se cerro apenas empezo\n[serve] la sesion se cerro apenas empezo\n[serve] la sesion se cerro apenas empezo\n[captura] el flujo se corto\n");
  MIRA(rep.size() == 2 && rep[0] == "[serve] la sesion se cerro apenas empezo (x3)", "lo mismo tres veces seguidas: un renglon con (x3)");
  MIRA(filtrarRegistro("[vulkan] cola normal · alta no: sin permiso (hace falta CAP_SYS_NICE)\n[sesion] llego una senal de cierre\n").empty(),
       "lo de cada sesion que no es un fallo (la prioridad, el cierre normal) no entra");

  Datos d;
  d.que = "El juego se cerró al entrar el 3D";
  d.pasos = "Abrir Cyberpunk & esperar";
  const std::string c = cuerpo(d, "OdysseyVK: 1.18\n");
  MIRA(c.find("### Qué pasó\nEl juego se cerró") != std::string::npos && c.find("### Qué esperabas\n_(sin escribir)_") != std::string::npos &&
           c.find("```\nOdysseyVK: 1.18\n```") != std::string::npos,
       "el cuerpo: las tres preguntas (lo vacio lo dice) y lo adjunto en su bloque");
  const std::string u = url("https://github.com/x/y", d, "OdysseyVK: 1.18\n");
  MIRA(u.rfind("https://github.com/x/y/issues/new?title=El%20juego%20se%20cerr%C3%B3", 0) == 0 && u.find(' ') == std::string::npos &&
           u.find("%26%20esperar") != std::string::npos,
       "la direccion: titulo y cuerpo codificados (ni espacios ni & sueltos)");
  std::string largo = "Sistema: x\n";
  for (int k = 0; k < 300; k++) largo += "  [serve] renglon numero " + std::to_string(k) + " con bastante texto para que no quepa\n";
  const std::string ul = url("https://github.com/x/y", d, largo);
  MIRA(ul.size() <= 7000 && ul.find("recortado") != std::string::npos && ul.find("Sistema") != std::string::npos,
       "si no cabe en 7000 letras se recorta el final del registro, se dice, y lo del equipo se queda");
  d.que = std::string(200, 'a');
  MIRA(titulo(d).size() <= 84, "el titulo, de 80 letras como mucho");
  std::printf("%s\n", fallos ? "HAY FALLOS" : "todo bien");
  return fallos ? 1 : 0;
}
