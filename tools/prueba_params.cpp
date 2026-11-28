/* prueba_params.cpp — las piezas VARIABLE=valor que el usuario añade en Integraciones (src/paramspropios.h): cuales
   valen, como entran y salen de «Otros parametros» y como se guarda la lista. Sin Steam ni panel.
   g++ -std=c++17 -O1 -Isrc tools/prueba_params.cpp -o /tmp/params && /tmp/params   */
#include "paramspropios.h"

#include <cstdio>

static int fallos = 0;
#define MIRA(c, que)                                           \
  do {                                                         \
    const bool bien_ = (c);                                    \
    if (!bien_) ++fallos;                                      \
    std::printf("%s  %s\n", bien_ ? "bien " : "MAL  ", que);   \
  } while (0)

int main() {
  using namespace paramspropios;
  MIRA(valida("ENABLE_LSFG=1") && valida("DXVK_HUD=fps,frametimes") && valida("PROTON_LOG=1") && valida("A_b9=/opt/x.so:/y"),
       "valen: ENABLE_LSFG=1, DXVK_HUD=fps,frametimes, una ruta");
  MIRA(!valida("ENABLE_LSFG") && !valida("=1") && !valida("FOO=") && !valida("9X=1"), "no valen: sin =, sin variable, sin valor, empezando por cifra");
  MIRA(!valida("FOO=a b") && !valida("FOO=1;rm") && !valida("FOO=$(x)") && !valida("FOO=a|b") && !valida("FOO=\"a\"") && !valida("FO O=1"),
       "no valen: espacios, ; $( ) | comillas (lo que la linea de Steam no sabe leer)");
  MIRA(piezas("DXVK_HUD=fps  PROTON_LOG=1").size() == 2 && piezas("A=\"x y\" B=1").size() == 2 && piezas("").empty(),
       "las piezas se parten por espacios, sin partir lo de entre comillas");
  MIRA(con("DXVK_HUD=fps", "ENABLE_LSFG=1", true) == "DXVK_HUD=fps ENABLE_LSFG=1", "encender: la pieza al final, lo demas en su sitio");
  MIRA(con("DXVK_HUD=fps ENABLE_LSFG=1 PROTON_LOG=1", "ENABLE_LSFG=1", false) == "DXVK_HUD=fps PROTON_LOG=1", "apagar: solo sale esa");
  MIRA(con("FOO=1 BAR=x", "FOO=2", true) == "BAR=x FOO=2", "encender otra de la misma variable quita la de antes");
  MIRA(con("FOO=2 BAR=x", "FOO=1", false) == "FOO=2 BAR=x", "apagar FOO=1 no quita FOO=2");
  MIRA(puesta("A=1 ENABLE_LSFG=1", "ENABLE_LSFG=1") && !puesta("A=1 ENABLE_LSFG=10", "ENABLE_LSFG=1"), "puesta: la pieza entera, no un trozo");
  MIRA(con(con("", "X=1", true), "X=1", false).empty(), "poner y quitar deja «otros» vacio");
  const std::vector<std::string> l = deTexto("# nota\nENABLE_LSFG=1\n\n  OBS_VKCAPTURE=1  \nmalo;=1\nENABLE_LSFG=1\n");
  MIRA(l.size() == 2 && l[0] == "ENABLE_LSFG=1" && l[1] == "OBS_VKCAPTURE=1", "la lista: sin comentarios, sin lo que no vale y sin repetidos");
  MIRA(deTexto(aTexto(l)) == l, "y vuelve igual de su fichero");
  std::printf("%s\n", fallos ? "HAY FALLOS" : "todo bien");
  return fallos ? 1 : 0;
}
