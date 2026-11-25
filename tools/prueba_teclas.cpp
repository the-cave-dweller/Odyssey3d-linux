/* prueba_teclas.cpp — las teclas de la rampa y del patron de un ojo, sin abrir ninguna ventana (v0.59).
   Llama a tuningKey() como lo hacen los visores y mira los numeros que deja en WeaveTuning:
   el paso grueso de `[` `]` `;` `'` (0,0004), el fino con Mayus (0,00002), que el visor GL (que no
   pasa modificadores) siga en grueso, la vuelta de `b` por 0 -> 1 -> 2 -> 0 y que `8` lo apague todo.

   session.cpp trae tambien el arranque del panel, la lente y el scaler; la prueba no los llama, asi
   que se compila con secciones sueltas y el enlazador tira lo que no se usa:
     F=build/CMakeFiles/odysseyvk.dir/flags.make
     D=$(grep CXX_DEFINES $F | cut -d= -f2-); I=$(grep CXX_INCLUDES $F | cut -d= -f2-)
     for f in session util; do eval g++ -std=c++17 -ffunction-sections -fdata-sections $D $I \
       -c src/$f.cpp -o /tmp/$f.o; done
     g++ -std=c++17 -Isrc tools/prueba_teclas.cpp /tmp/session.o /tmp/util.o -lglfw -Wl,--gc-sections \
       -o /tmp/teclas && /tmp/teclas
   Sale con 0 si todo cuadra y con el numero de fallos si no. */
#include "session.h"
#include "weave.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
static int fallos = 0;
static void ver(bool ok, const char *que) { std::printf("%s %s\n", ok ? "OK  " : "FALLA", que); if (!ok) fallos++; }
int main() {
  WeaveTuning t; const char *tag = nullptr;
  tuningKey(t, GLFW_KEY_RIGHT_BRACKET, &tag, 0);
  ver(std::fabs(t.rampaDX - 0.0004f) < 1e-9f, "] sin Mayus suma 0,0004"); std::printf("     tag: %s\n", tag);
  tuningKey(t, GLFW_KEY_LEFT_BRACKET, &tag, GLFW_MOD_SHIFT);
  ver(std::fabs(t.rampaDX - 0.00038f) < 1e-8f, "Mayus+[ resta 0,00002"); std::printf("     tag: %s\n", tag);
  for (int i = 0; i < 19; i++) tuningKey(t, GLFW_KEY_LEFT_BRACKET, &tag, GLFW_MOD_SHIFT);
  ver(t.rampaDX == 0.0f, "20 pasos finos deshacen uno grueso y quedan en cero exacto"); std::printf("     tag: %s\n", tag);
  tuningKey(t, GLFW_KEY_APOSTROPHE, &tag, GLFW_MOD_SHIFT);
  ver(std::fabs(t.rampaDY - 0.00002f) < 1e-9f && t.rampaDX == 0.0f, "Mayus+' mueve solo y"); std::printf("     tag: %s\n", tag);
  tuningKey(t, GLFW_KEY_SEMICOLON, &tag);   /* sin mods: el visor GL llama asi */
  ver(std::fabs(t.rampaDY + 0.00038f) < 1e-8f, "; sin mods (visor GL) sigue en grueso");
  tuningKey(t, GLFW_KEY_B, &tag); ver(t.unOjo == 1, "b -> 1"); std::printf("     tag: %s\n", tag);
  tuningKey(t, GLFW_KEY_B, &tag); ver(t.unOjo == 2, "b -> 2"); std::printf("     tag: %s\n", tag);
  tuningKey(t, GLFW_KEY_B, &tag); ver(t.unOjo == 0, "b -> 0"); std::printf("     tag: %s\n", tag);
  tuningKey(t, GLFW_KEY_B, &tag); tuningKey(t, GLFW_KEY_8, &tag);
  ver(t.unOjo == 0 && t.rampaDX == 0.0f && t.rampaDY == 0.0f, "8 apaga el patron y las rampas");
  std::printf("%s\n", fallos ? "HAY FALLOS" : "todo bien");
  return fallos;
}
