/* capasajenas.h — lo que dibujan encima del juego otras capas de Vulkan (v1.15): el menu de vkDaedalus, su HUD y
   MangoHud. MAKO, la generacion de cuadros, no dibuja nada.

   Rodolfo: «Voy a usar el frame generation de MAKO en vkdaedalus, checa que no te de problemas y procura que no
   choquen los overlay, que solo uno esté al frente a la vez»; y al preguntarle cuales: el menu de vkDaedalus contra
   la pastilla, un solo contador de FPS, y lo de las otras capas plano en el 3D.

   Lo dibujan sobre el cuadro ya hecho, despues del HUD del juego, asi que la mascara del HUD (la capa de OdysseyVK)
   no los ve y en el 3D tomarian el relieve de la escena. Aqui se mira, desde fuera y sin tocarlas:
     - el menu de vkDaedalus (End): mientras esta abierto, su gancho de entrada (el que precarga vkdaedalus-run, en
       un juego de Wine) escribe el proceso que lo tiene abierto en $XDG_RUNTIME_DIR/vkdaedalus-input-<dev>-<ino>
       (8 bytes: el arranque del proceso << 32 | su pid) para retener la entrada de todo el prefijo, y lo borra al
       cerrarlo. Se da por abierto si ese proceso sigue vivo con ese mismo arranque (lo que comprueba vkDaedalus).
       El menu ocupa la pantalla entera, con un velo oscuro;
     - el HUD de vkDaedalus: su biblioteca cargada en el juego y `hud = true` en ~/.config/vkdaedalus/settings.conf;
       va arriba a la izquierda;
     - MangoHud: su biblioteca cargada en el juego, salvo `no_display` en MANGOHUD_CONFIG; va donde diga su
       `position` (arriba a la izquierda, de fabrica). */
#ifndef ODYSSEY_CAPASAJENAS_H
#define ODYSSEY_CAPASAJENAS_H

#include <sys/types.h>

#include <string>
#include <vector>

namespace ajenas {

/* un trozo de la imagen, de 0 a 1 (x hacia la derecha, y hacia abajo) */
struct Zona {
  float x0, y0, x1, y1;
};

struct Estado {
  bool menuVkd = false;    /* el menu de vkDaedalus, abierto */
  bool hudVkd = false;     /* el HUD de vkDaedalus a la vista */
  bool fpsVkd = false;     /*   y con los FPS en el */
  bool mangohud = false;   /* MangoHud a la vista (lleva los FPS) */
  bool mako = false;       /* (v1.18) la capa de MAKO (la generacion de cuadros) cargada en el juego */
  std::string exe;         /* (v1.18) el ejecutable del juego («Raji-Win64-Shipping.exe»), para su perfil de MAKO */
  std::vector<Zona> planas;   /* lo que va al plano de la pantalla: todo con el menu; la esquina de cada HUD */
  bool hayFpsAjeno() const { return mangohud || fpsVkd; }
  bool operator==(const Estado &o) const {
    return menuVkd == o.menuVkd && hudVkd == o.hudVkd && fpsVkd == o.fpsVkd && mangohud == o.mangohud && mako == o.mako &&
           exe == o.exe;
  }
};

/* lo de ahora para el juego `pid` (0 = no se sabe: solo el menu de vkDaedalus). El menu se mira cada vez (unos
   pocos ficheros de $XDG_RUNTIME_DIR); las bibliotecas del juego y los ajustes, cada 3 s o si cambia el pid */
Estado mirar(pid_t pid);

}  // namespace ajenas

#endif
