/* session.cpp — hardware del monitor, pantalla y teclas compartidas por los dos visores. */
#include "session.h"

#include "util.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

static const char *rotW(int r) {
  const char *w = Panel::rotationWord(r);
  return w ? w : "desconocida";
}

void Rig::bringUp(const ViewerOpts &o) {
  /* 1) modo del panel: la trama exige píxel a píxel (escala 1, rotación normal, 3840x2160). */
  if (!o.noPanel) {
    if (!Panel::available()) {
      std::fprintf(stderr,
                   "[aviso] kscreen-doctor no esta disponible: deja el panel a 3840x2160, escala 1 y rotacion normal\n");
    } else {
      std::string name = Panel::resolveOutput(o.output);
      PanelState st{};
      std::string err;
      if (name.empty() || !Panel::query(name, st, &err)) {
        std::fprintf(stderr, "[aviso] no se pudo leer el estado del panel (%s): no se cambia el modo\n",
                     err.empty() ? "sin salidas" : err.c_str());
      } else {
        output = name;
        int idx = Panel::pickMode(st, o.hz);
        if (idx < 0) {
          std::fprintf(stderr, "[aviso] %s no tiene modo 3840x2160: no se cambia el modo\n", name.c_str());
        } else {
          /* Aviso si una sesión anterior se quedó sin restaurar (no tocamos nada por nuestra
             cuenta: puede que el usuario haya cambiado la pantalla después). */
          PanelState stale{};
          std::vector<Placement> staleOtras;   /* sitios de origen guardados por esa sesion cortada */
          const bool hayStale = Panel::loadSnapshot(stale, nullptr, nullptr, &staleOtras) &&
                                stale.name == name &&
                                (stale.modeIndex != st.modeIndex || stale.scale != st.scale ||
                                 stale.rotation != st.rotation);
          /* Si además el panel está justo en nuestra configuracion de 3D, lo que vemos no es el
             estado del usuario sino el residuo de una sesion que murio a mitad: el estado de verdad
             es el de la instantanea, y el fichero no se debe sobrescribir con el residuo. */
          const bool residual = hayStale && st.modeIndex == idx && st.scale == 1.0 && st.rotation == RotNone;
          const std::string nombreModo = Panel::modeName(st, idx);  /* el nombre se pierde si sustituimos st */
          if (hayStale) {
            if (residual)
              std::fprintf(stderr,
                           "[aviso] el panel seguia en el modo 3D de una sesion anterior cortada: se devuelve el "
                           "estado guardado (modo %d %s, escala %g, rotacion %s)\n",
                           stale.modeIndex, stale.modeRes.c_str(), stale.scale, rotW(stale.rotation));
            else
              std::fprintf(stderr,
                           "[aviso] hay un estado del panel sin restaurar de otra sesion (modo %d %s, escala %g, "
                           "rotacion %s). Si quieres recuperarlo: odysseyvk ctl panel restore\n",
                           stale.modeIndex, stale.modeRes.c_str(), stale.scale, rotW(stale.rotation));
          }
          /* Sitios de las otras salidas. Al pasar el panel a escala 1 su area logica crece
             (aqui: de 2560x1440 a 3840x2160) y si el otro monitor esta pegado a la derecha, las
             dos areas se montan 1280 px logicos: esa franja se ve en los dos monitores a la vez y
             KWin le cambia el tamano a la ventana del video a media sesion. Movemos el otro monitor
             en la MISMA orden atomica del modo y lo devolvemos al salir.
             Puerta: ODYSSEYVK_LAYOUT=0 o --sin-despejar dejan el escritorio intacto. */
          /* Escala del panel en 3D (v0.76). De siempre se pasaba a escala 1, y eso cambia el tamano
             logico del escritorio. KWin guarda como estaba cada ventana en cada configuracion de
             pantallas y se lo repone al volver a ella: medido el 25-nov, al entrar en 3D sacaba al
             video de pantalla completa (como habia quedado la vez anterior) y al salir se la volvia a
             poner, y el navegador (Zen) no salia bien de ese vaiven. La escala 1 no hace falta: con el
             escritorio a 1,5 la ventana del visor mide 2560x1440 logicos y su lienzo son los
             3840x2160 puntos del panel (medido: «lienzo 3840x2160 fisicos, ventana logica
             2560x1440»), y la ventana del video llega igual a 3840x2160 (medido con `ctl flujo`:
             2014x868 logicos -> 3022x1303). Asi que el panel se queda con la escala del escritorio y
             KWin no tiene nada que reponer. `escala_3d = 1` en serve.conf (ODYSSEYVK_ESCALA_3D=1)
             vuelve a lo de antes. */
          static const bool kEscalaUno = [] {
            const char *e = entornoOdyssey("ODYSSEYVK_ESCALA_3D");
            return e && std::atof(e) == 1.0;
          }();
          const double escalaEscritorio = residual ? stale.scale : st.scale;
          const double escala3d = (kEscalaUno || escalaEscritorio <= 0.0) ? 1.0 : escalaEscritorio;
          std::vector<Placement> movidas, originales;
          const char *noLayout = entornoOdyssey("ODYSSEYVK_LAYOUT");
          const bool dejar = o.noLayout || (noLayout && !std::strcmp(noLayout, "0"));
          if (!dejar) {
            std::vector<PanelState> todos;
            std::string err2;
            int nw = 0, nh = 0;
            if (Panel::allOutputs(todos, &err2) && Panel::modeSize(st, idx, escala3d, nw, nh)) {
              movidas = Panel::despejar(todos, name, nw, nh);
              for (const Placement &q : movidas) {
                for (const PanelState &s : todos)
                  if (s.name == q.name && s.geom) originales.push_back(Placement{s.name, s.x, s.y});
                std::printf("[panel] %s pasa a %d,%d: con el panel a %dx%d logicos las dos zonas se "
                            "montaban y esa franja se veia duplicada (al salir vuelve a su sitio)\n",
                            q.name.c_str(), q.x, q.y, nw, nh);
              }
              if (movidas.empty() && st.geom && st.w > 0 && st.w != nw)
                std::printf("[panel] el panel pasa de %dx%d a %dx%d logicos y el resto del escritorio"
                            " ya esta despejado: no se mueve nada\n", st.w, st.h, nw, nh);
            }
          }
          std::string snapPath;
          if (residual) {
            st = stale;  /* el estado real del usuario es el guardado, no el que se ve ahora */
          } else if (!Panel::saveSnapshot(st, &snapPath, &err, movidas.empty() ? nullptr : &originales)) {
            std::fprintf(stderr, "[aviso] %s\n", err.c_str());
          }
          /* HDR en 3D: se respeta siempre el del escritorio (rama experimental, 26-nov). Hasta la v0.71
             se apagaba al entrar (con HDR el compositor mezcla los canales y en la trama los tres
             subpixeles de un punto pueden ser de ojos distintos), pero Rodolfo lo probo en el panel el
             25-nov y con HDR los colores dejan de verse secos; desde entonces lo pidio sin opcion. */
          /* si el panel ya esta en el modo, la escala y la rotacion que hacen falta, no se manda
             ninguna orden de geometria: ni un parpadeo, ni un cambio de configuracion para KWin */
          const bool yaEsta = st.modeIndex == idx && std::fabs(st.scale - escala3d) < 1e-6 &&
                              st.rotation == RotNone && movidas.empty() && !residual;
          if (Panel::apply(name, idx, escala3d, RotNone, false, &err, false, movidas.empty() ? nullptr : &movidas,
                           yaEsta)) {
            panelSinGeometria = yaEsta;
            saved = st;
            /* En el caso residual los sitios de origen validos son los de la instantanea: lo que se
               ve ahora en el escritorio ya es el escritorio movido de la sesion que se corto. */
            otras = (residual && !staleOtras.empty()) ? staleOtras : originales;
            panelApplied = true;
            std::printf("[panel] %s: modo %d %s, escala %g%s, rotacion normal, HDR %s\n", name.c_str(), idx,
                        nombreModo.c_str(), escala3d,
                        yaEsta ? " (la del escritorio: no se cambia nada)" : "",
                        "como en el escritorio");
            if (!yaEsta) {
              std::printf("[panel] al salir se restaura el modo %d %s con escala %g y rotacion %s\n", saved.modeIndex,
                          saved.modeRes.c_str(), saved.scale, rotW(saved.rotation));
              usleep(400000); /* que KWin asiente antes de crear la ventana */
            }
          } else {
            std::fprintf(stderr, "[aviso] no se pudo cambiar el modo: %s\n", err.c_str());
            Panel::clearSnapshot();
          }
        }
        if (!st.vrr.empty() && st.vrr != "Never") {
          /* v1.11: el mensaje dice lo que queda puesto, no solo lo que habia en el escritorio */
          const char *e = entornoOdyssey("ODYSSEYVK_VRR_3D");
          const bool deja = e && (e[0] == '1' || e[0] == 's');
          if (deja)
            std::printf("[panel] sincronizacion adaptativa: %s, como en el escritorio (`vrr_en_3d = si`)\n", st.vrr.c_str());
          else
            std::printf("[panel] sincronizacion adaptativa: apagada durante el 3D (en el escritorio: %s; al salir vuelve)\n",
                        st.vrr.c_str());
        }
      }
    }
  }

  marcaArranque("panel");
  /* 2) y 3) en un hilo aparte (v1.12): la lente (215 ms en el panel de Rodolfo) y el scaler (180 ms) no
     dependen de nada de lo que viene despues, asi que corren mientras el visor crea la ventana, Vulkan y
     la red. El visor los espera (esperar) antes de pintar el primer cuadro en 3D. */
  const ViewerOpts op = o;
  hilo_ = std::thread([this, op]() {
    const ViewerOpts &o = op;
    /* 2) lente lenticular: autenticación HMAC-SHA3 y conmutación a 3D. */
    if (!o.noLens) {
      std::string port = Lens::findPort();
      if (port.empty()) {
        std::fprintf(stderr, "[aviso] no se encontro la lente (USB 354b:0116): ¿falta el cable USB-B del monitor?\n");
      } else if (!lens.open(port)) {
        std::fprintf(stderr, "[aviso] lente: %s\n", lens.lastError().c_str());
      } else {
        std::printf("[lente] %s\n", port.c_str());
        if (!lens.authenticate()) std::fprintf(stderr, "[aviso] autenticación de lente falló: %s\n", lens.lastError().c_str());
        if (!lens.on()) std::fprintf(stderr, "[aviso] no se pudo encender la lente: %s\n", lens.lastError().c_str());
        lensOk = lens.state();
        std::printf("[lente] estado: %s\n", lensOk ? "3D" : "2D");
      }
    }

    marcaArranque("lente");
    /* 3) scaler MediaTek: modo 3D de la tarjeta del monitor. */
    if (!o.noScaler) {
      std::string dev = Scaler::findDevice();
      if (dev.empty()) {
        std::fprintf(stderr, "[aviso] no se encontro el scaler (dispositivo SCSI con vendor GCREADER)\n");
      } else if (!scaler.open(dev)) {
        std::fprintf(stderr, "[aviso] scaler: %s\n", scaler.lastError().c_str());
      } else if (scaler.set3d(true)) {
        scalerOn = true;
        std::printf("[scaler] %s: modo 3D activado\n", dev.c_str());
      } else {
        std::fprintf(stderr, "[aviso] no se pudo activar el modo 3D: %s\n", scaler.lastError().c_str());
      }
    }
    marcaArranque("scaler");
  });
}

void Rig::esperar() {
  if (hilo_.joinable()) hilo_.join();
}

void Rig::shutDown() {
  esperar();   /* (v1.12) si la lente o el scaler aun se estan poniendo, primero que acaben */
  if (scalerOn) {
    scaler.set3d(false);
    std::printf("[scaler] modo 3D apagado\n");
    scalerOn = false;
  }
  if (lens.isOpen()) {
    lens.off();
    std::printf("[lente] devuelta a 2D\n");
    lens.close();
  }
  if (panelApplied) {
    std::string err;
    if (Panel::restore(saved, &err, otras.empty() ? nullptr : &otras, panelSinGeometria)) {
      if (panelSinGeometria)
        std::printf("[panel] el modo y la escala no se tocaron: nada que devolver (solo HDR y sincronizacion, si cambiaron)\n");
      else
      std::printf("[panel] restaurado: modo %d %s, escala %g, rotacion %s\n", saved.modeIndex, saved.modeRes.c_str(),
                  saved.scale, rotW(saved.rotation));
      for (const Placement &q : otras)
        std::printf("[panel] %s devuelta a %d,%d\n", q.name.c_str(), q.x, q.y);
      Panel::clearSnapshot();
    } else {
      std::fprintf(stderr, "[aviso] no se pudo restaurar el panel: %s\n", err.c_str());
      std::fprintf(stderr, "[aviso] el estado guardado sigue en ~/.local/state/odysseyvk/panel.state: "
                           "recuperelo con `odysseyvk ctl panel restore`\n");
    }
    panelApplied = false;
  }
}

/* ---------------- pantalla ---------------- */

GLFWmonitor *pickMonitor(const std::string &output) {
  int n = 0;
  GLFWmonitor **ms = glfwGetMonitors(&n);
  for (int i = 0; i < n; i++)
    if (!output.empty() && output == glfwGetMonitorName(ms[i])) return ms[i];
  for (int i = 0; i < n; i++) { /* si no coincide el nombre, el panel con modo 4K */
    int nm = 0;
    const GLFWvidmode *modes = glfwGetVideoModes(ms[i], &nm);
    for (int j = 0; j < nm; j++)
      if (modes[j].width == 3840 && modes[j].height == 2160) return ms[i];
  }
  return glfwGetPrimaryMonitor();
}

int refreshOf(GLFWmonitor *mon, double wantHz) {
  int n = 0;
  const GLFWvidmode *modes = glfwGetVideoModes(mon, &n);
  int best = 0;
  double bestd = 1e9;
  for (int i = 0; i < n; i++) {
    double d = std::fabs((double)modes[i].refreshRate - wantHz);
    if (d < bestd) {
      bestd = d;
      best = modes[i].refreshRate;
    }
  }
  const GLFWvidmode *cur = glfwGetVideoMode(mon);
  return best > 0 ? best : (cur ? cur->refreshRate : 0);
}

/* ---------------- teclas ---------------- */

bool tuningKey(WeaveTuning &t, int key, const char **tag, int mods) {
  *tag = nullptr;
  static char etiqueta[48];   /* para las etiquetas con numero (las rampas) */
  switch (key) {
    case GLFW_KEY_3:
      t.weave = !t.weave;
      *tag = "3D/2D";
      break;
    case GLFW_KEY_F:
      t.swapLR = !t.swapLR;
      *tag = "invertir";
      break;
    case GLFW_KEY_LEFT:
      t.align -= 0.002f;
      *tag = "fase -";
      break;
    case GLFW_KEY_RIGHT:
      t.align += 0.002f;
      *tag = "fase +";
      break;
    case GLFW_KEY_PAGE_UP:
      t.pitch += 0.0002f;
      *tag = "paso +";
      break;
    case GLFW_KEY_PAGE_DOWN:
      t.pitch -= 0.0002f;
      *tag = "paso -";
      break;
    case GLFW_KEY_UP:
      t.xtalkDyn += 0.002f;
      *tag = "crosstalk +";
      break;
    case GLFW_KEY_DOWN:
      t.xtalkDyn -= 0.002f;
      *tag = "crosstalk -";
      break;
    case GLFW_KEY_COMMA:
      t.filterSlope = t.filterSlope < 0.5f ? 0.0f : t.filterSlope - 0.5f;
      *tag = "filtro -";
      break;
    case GLFW_KEY_PERIOD:
      t.filterSlope += 0.5f;
      *tag = "filtro +";
      break;
    case GLFW_KEY_9:
      t.conv -= 0.0005f;
      *tag = "conv -";
      break;
    case GLFW_KEY_0:
      t.conv += 0.0005f;
      *tag = "conv +";
      break;
    /* Paso de la rejilla escrita. Si no coincide con el paso real de la lente del panel, la
       nuestra y la de la lente baten y salen franjas gruesas cruzando toda la imagen; estas dos
       parejas de teclas mueven la rampa de fase limpia (la misma para los tres canales) encima de
      `rampa_x`/`rampa_y` de la configuracion, asi que se puede afinar a ojo mirando el panel.
       Paso fino con Mayus (v0.59). El grueso (0,0004 ciclos por pixel) corre la fase 1,5 ciclos de
       un borde al otro de los 3840 px: entre una pulsacion y la siguiente se salta el punto bueno.
       Para que no quede ninguna banda en la pantalla la fase tiene que derivar menos de 0,1 ciclo
       de borde a borde, o sea un error de paso por debajo de 0,1/3840 = 2,6e-5. El fino es 2e-5:
       0,077 ciclos de deriva por pulsacion, por debajo de esa tolerancia. */
    case GLFW_KEY_LEFT_BRACKET:
    case GLFW_KEY_RIGHT_BRACKET:
    case GLFW_KEY_SEMICOLON:
    case GLFW_KEY_APOSTROPHE: {
      const bool fino = (mods & GLFW_MOD_SHIFT) != 0;
      const float paso = fino ? 0.00002f : 0.0004f;
      const bool enX = key == GLFW_KEY_LEFT_BRACKET || key == GLFW_KEY_RIGHT_BRACKET;
      const float signo = (key == GLFW_KEY_LEFT_BRACKET || key == GLFW_KEY_SEMICOLON) ? -1.0f : 1.0f;
      float &r = enX ? t.rampaDX : t.rampaDY;
      r += signo * paso;
      if (std::fabs(r) < 1e-7f) r = 0.0f;   /* que la vuelta a cero no deje un -0,000000 */
      std::snprintf(etiqueta, sizeof etiqueta, "rampa_%s %+.6f sobre la config%s", enX ? "x" : "y", r,
                    fino ? " (fino)" : "");
      *tag = etiqueta;
      break;
    }
    /* Patron de un solo ojo: ver `unOjo` en weave.h. */
    case GLFW_KEY_B:
      t.unOjo = (t.unOjo + 1) % 3;
      *tag = t.unOjo == 0 ? "patron de un ojo apagado: imagen normal"
           : t.unOjo == 1 ? "un ojo: blanco a la vista izquierda (cierra un ojo)"
                          : "un ojo: blanco a la vista derecha (cierra un ojo)";
      break;
    /* Capas por profundidad. Una sola llave (el relieve) obliga a elegir entre un fondo atrasado
       y unos primeros planos fuertes; aqui se separan los dos lados del plano de apoyo, que es
       como lo trae el conversor de Windows. `j`/`k` mueven lo de DETRAS de la pantalla y `u`/`i`
       lo de DELANTE. Tope 0,2..2,0: fuera de ahi el relieve se sale del presupuesto de la lente y
       lo unico que se consigue es dolor de ojos. El tope oficial por ojo se impone despues, en el
       shader, asi que un fondo en 2,0 no pasa de 9,98 puntos hacia dentro. */
    case GLFW_KEY_J:
    case GLFW_KEY_K:
    case GLFW_KEY_U:
    case GLFW_KEY_I: {
      const auto topeGan = [](float v) { return v < 0.2f ? 0.2f : (v > 2.0f ? 2.0f : v); };
      if (key == GLFW_KEY_J) t.ganFondo = topeGan(t.ganFondo - 0.05f);
      if (key == GLFW_KEY_K) t.ganFondo = topeGan(t.ganFondo + 0.05f);
      if (key == GLFW_KEY_U) t.ganFrente = topeGan(t.ganFrente - 0.05f);
      if (key == GLFW_KEY_I) t.ganFrente = topeGan(t.ganFrente + 0.05f);
      t.capaTocada = true;
      std::snprintf(etiqueta, sizeof etiqueta, "fondo %.2f · frente %.2f", t.ganFondo, t.ganFrente);
      *tag = etiqueta;
      break;
    }
    /* Empuje por capa: desplazamiento FIJO, en puntos de panel, que se suma al relieve que ya
       tiene cada banda. Sirve para llevar una banda entera a un plano concreto (subtitulos y
       ventanas al frente, fondo del plano hundido) sin depender del plano de apoyo; es el
       `far_diff_popup -1,0` de `scene_dspr` y el `depth_offset` de la capa `overlay` de `degraph`
       en la config de Windows. Positivo = hacia fuera de la pantalla. Se topa a 12 puntos porque
       ese es justo el tope oficial hacia fuera (9,98 hacia dentro) y pasar no se ve. */
    case GLFW_KEY_H:
    case GLFW_KEY_L:
    case GLFW_KEY_Y:
    case GLFW_KEY_O: {
      const auto topeEmp = [](float v) { return v < -12.0f ? -12.0f : (v > 12.0f ? 12.0f : v); };
      if (key == GLFW_KEY_H) t.empFondo = topeEmp(t.empFondo - 0.5f);
      if (key == GLFW_KEY_L) t.empFondo = topeEmp(t.empFondo + 0.5f);
      if (key == GLFW_KEY_Y) t.empFrente = topeEmp(t.empFrente - 0.5f);
      if (key == GLFW_KEY_O) t.empFrente = topeEmp(t.empFrente + 0.5f);
      t.capaTocada = true;
      std::snprintf(etiqueta, sizeof etiqueta, "empuje fondo %+.1f · frente %+.1f puntos",
                    t.empFondo, t.empFrente);
      *tag = etiqueta;
      break;
    }
    /* Dos planos. `g` sube y `v` baja la dureza con la que el mapa de profundidad se aplasta hacia
       los DOS unicos valores que hay en el cuadro (un fondo y unos primeros planos): la imagen
       queda en dos laminas con su propio desplazamiento, que es como lo trata el Nintendo 3DS.
       En 0 sigue el relieve continuo de siempre, que es como arranca el programa. Las laminas ya
       colocadas se mueven con las parejas de antes: `j`/`k` y `u`/`i` para estirarlas, `h`/`l` y
       `y`/`o` para llevar una lamina entera a un plano fijo. */
    case GLFW_KEY_G:
    case GLFW_KEY_V: {
      const auto topeDureza = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
      if (key == GLFW_KEY_G) t.planos = topeDureza(t.planos + 0.1f);
      if (key == GLFW_KEY_V) t.planos = topeDureza(t.planos - 0.1f);
      std::snprintf(etiqueta, sizeof etiqueta, "dos planos %.1f%s", t.planos,
                    t.planos <= 0.0f ? " (relieve continuo)" : "");
      *tag = etiqueta;
      break;
    }
    /* Muestras intermedias entre las dos laminas. Con la dureza baja el tramo del centro es una
       rampa y se nota el degradado de una capa a la otra; `n` agrega un nivel intermedio con su
       propia meseta (ahi el centro se ve solido) y `x` lo quita. El paso entre niveles sigue siendo
       un suave angosto, asi que el canto no se corta ni parpadea. Sin dureza no hay laminas. */
    case GLFW_KEY_N:
    case GLFW_KEY_X: {
      if (t.planos <= 0.0f) {
        *tag = "sin dureza no hay laminas: subela con `g`";
        break;
      }
      if (key == GLFW_KEY_N) t.muestras = t.muestras < 3 ? t.muestras + 1 : 3;
      else t.muestras = t.muestras > 0 ? t.muestras - 1 : 0;
      if (t.muestras == 0) {
        *tag = "sin muestras: la rampa continua de siempre";
        break;
      }
      std::snprintf(etiqueta, sizeof etiqueta, "%d muestra%s intermedia%s", t.muestras,
                    t.muestras > 1 ? "s" : "", t.muestras > 1 ? "s" : "");
      *tag = etiqueta;
      break;
    }
    case GLFW_KEY_8:
      t.conv = 0.0f;
      t.align = 0.0f;
      t.pitch = 1.0f;
      t.xtalkDyn = 0.0f;
      t.rampaDX = 0.0f;
      t.rampaDY = 0.0f;
      t.unOjo = 0;
      *tag = "reiniciar";
      break;
    default:
      break;
  }
  return *tag != nullptr;
}

/* ---------------- patrón de prueba por CPU ---------------- */

namespace {
inline void px(std::vector<uint8_t> &img, int w, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || y < 0 || x >= w) return;
  size_t o = ((size_t)y * (size_t)w + (size_t)x) * 4u;
  if (o + 3 >= img.size()) return;
  img[o] = r, img[o + 1] = g, img[o + 2] = b, img[o + 3] = 255;
}
inline void rect(std::vector<uint8_t> &img, int w, int h, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g,
                 uint8_t b) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      if (y >= 0 && y < h) px(img, w, x, y, r, g, b);
}
/* color por índice, como el hsv() del patrón de OpenGL */
inline void hue(int i, uint8_t &r, uint8_t &g, uint8_t &b) {
  float hh = (float)i / 9.0f;
  float c[3];
  for (int k = 0; k < 3; k++) {
    float x = std::fabs(std::fmod(hh * 6.0f + (float)k * 2.0f, 6.0f)) - 3.0f;
    x = std::fabs(x) - 1.0f;
    x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
    c[k] = 0.25f + 0.75f * (1.0f - x);
  }
  r = (uint8_t)(c[0] * 255.0f);
  g = (uint8_t)(c[1] * 255.0f);
  b = (uint8_t)(c[2] * 255.0f);
}
}  // namespace

void makeTestSbs(std::vector<uint8_t> &img, int w, int h, double t, double parallax) {
  img.assign((size_t)w * (size_t)h * 4u, 0);
  const int hw = w / 2;
  for (int eye = 0; eye < 2; eye++) {
    const int ox = eye * hw;
    const double s = eye == 0 ? 1.0 : -1.0;  /* el otro ojo, desplazado en sentido contrario */
    for (int y = 0; y < h; y++) {
      float fy = (float)(h - 1 - y); /* como gl_FragCoord: 0 abajo, para que el dibujo coincida */
      for (int x = 0; x < hw; x++) {
        float pxx = (float)x, pyy = fy;
        uint8_t r = 13, g = 13, b = 15;
        /* rejilla de referencia en el plano de la pantalla */
        float gx = std::fmod(pxx + (float)(s * 0.0), 96.0f), gy = std::fmod(pyy, 96.0f);
        if (gx < 2.0f || gy < 2.0f) r = g = b = 51;
        /* escalera de profundidad: 9 cuadros, cada uno con otra disparidad (uno oscila) */
        for (int i = 0; i < 9; i++) {
          double disp = ((double)i - 4.0) * 14.0 + (i == 4 ? 12.0 * std::sin(t * 0.9) : 0.0);
          double cx = (double)hw * ((double)i + 0.5) / 9.0 + s * disp;
          double cy = (double)h * 0.55;
          if (std::fabs(pxx - cx) < 66.0 && std::fabs(pyy - cy) < 66.0) hue(i, r, g, b);
        }
        /* crucero en el plano de la pantalla */
        double ccx = hw * 0.5, ccy = h * 0.86;
        if ((std::fabs(pxx - ccx) < 3.0 && std::fabs(pyy - ccy) < 40.0) ||
            (std::fabs(pyy - ccy) < 3.0 && std::fabs(pxx - ccx) < 40.0))
          r = g = b = 255;
        /* dos marcas verticales con disparidad fuerte: deben fusionarse por delante */
        if (std::fabs(pxx - (hw * 0.5 - s * parallax)) < 3.0 && std::fabs(pyy - h * 0.16) < 50.0) r = g = b = 255;
        /* barras R/G/B sin disparidad: sirven para ver el alineado por canal de la trama */
        if (pyy > 70.0 && pyy < 114.0) {
          float tt = (pxx - hw * 0.30f) / (hw * 0.40f);
          if (tt > 0.0f && tt < 1.0f) {
            if (tt < 0.333f)
              r = 255, g = b = 0;
            else if (tt < 0.666f)
              g = 255, r = b = 0;
            else
              b = 255, r = g = 0;
          }
        }
        px(img, w, ox + x, y, r, g, b);
      }
    }
  }
  (void)rect;
}
