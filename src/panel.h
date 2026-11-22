/* panel.h — pone el panel en el modo geométrico que necesita la trama 3D
   (1:1, sin escala, sin rotación) y lo restaura al salir. Usa kscreen-doctor (KDE). */
#ifndef ODYSSEY_PANEL_H
#define ODYSSEY_PANEL_H

#include <string>
#include <vector>

struct PanelState {
  std::string name;
  int modeIndex = 0;              /* índice del modo activo en la lista de kscreen-doctor */
  std::string modeRes;            /* p.ej. "3840x2160@164.98" */
  std::vector<std::pair<int, std::string>> modes; /* índice -> "3840x2160@60.00" */
  double scale = 1.0;
  int rotation = 0;               /* 0 none, 1 left, 2 inverted, 3 right */
  std::string vrr;                /* "Always" | "Automatic" | "Never" | "" */
  bool hdr = false;
};

class Panel {
public:
  static bool available();
  static std::vector<std::string> outputs(std::string *err = nullptr);
  /* Lee el estado actual de un output (nombre tipo "DP-4"). */
  static bool query(const std::string &out, PanelState &st, std::string *err = nullptr);
  /* Elige el índice de modo cuya tasa se acerque más a preferHz (sin pasar de ella). */
  static int pickMode(const PanelState &st, double preferHz);
  /* Nombre legible de un modo por su índice kscreen ("3840x2160@119.88"). */
  static std::string modeName(const PanelState &st, int modeIndex);
  /* Aplica modo+escala+rotación (y opcionalmente apaga HDR). `dryRun` solo muestra el comando. */
  static bool apply(const std::string &out, int modeIndex, double scale, int rotation, bool disableHdr,
                    std::string *err = nullptr, bool dryRun = false);
  static bool restore(const PanelState &st, std::string *err = nullptr);
  static const char *rotationWord(int r);
  /* Nombre de la salida que tiene un modo 3840x2160 (el panel G90XF). Vacío si no la hay. */
  static std::string resolveOutput(const std::string &want);
};

#endif
