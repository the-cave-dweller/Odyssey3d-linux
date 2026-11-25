/* atajos.h — los atajos del hub (Ctrl+Mayus+1..8 y F1) como atajos GLOBALES del escritorio (v0.75).

   El visor deja el teclado a la ventana activa (el video, el juego), asi que sus teclas solo le
   llegaban mientras el tenia el foco: en cuanto se hacia clic en el video, o con un juego delante,
   Ctrl+Mayus+1 ya no hacia nada. En Wayland una aplicacion no puede quedarse teclas de otra; quien
   puede es el compositor. KDE lo ofrece por D-Bus (org.kde.kglobalaccel, lo sirve KWin): se registra
   cada accion con sus teclas y KWin avisa con una senal cuando se pulsan, tenga el foco quien lo tenga.

   Solo estan activos mientras hay un visor abierto: al cerrar se dejan inactivos y las teclas vuelven
   a las aplicaciones. Quedan apuntados en Ajustes del sistema > Atajos, bajo «OdysseyVK», y ahi se
   pueden cambiar. `atajos_globales = no` en serve.conf (ODYSSEYVK_ATAJOS=0) no los registra.

   Mayus+1 no es «1» para el compositor sino el simbolo que da la distribucion del teclado («!» en la
   espanola, «·» con el 3...), asi que cada accion se registra con las dos formas: se le pregunta a
   xkbcommon que simbolo sale con Mayus en cada tecla de numero. */
#pragma once

#include <string>
#include <vector>

class AtajosGlobales {
public:
  AtajosGlobales() = default;
  ~AtajosGlobales();
  AtajosGlobales(const AtajosGlobales &) = delete;
  AtajosGlobales &operator=(const AtajosGlobales &) = delete;

  /* Registra las acciones y empieza a escuchar. Falso si no hay bus o no es KDE. */
  bool abrir(std::string *err = nullptr);
  bool abierto() const { return bus_ != nullptr && escucha_; }
  /* Cada cuadro: los nombres de las acciones pulsadas desde la ultima vez ("3d", "prof+", ...). */
  std::vector<std::string> pulsados();
  /* Deja los atajos inactivos (las teclas vuelven a las aplicaciones) y suelta el bus. */
  void cerrar();
  /* Lo que quedo registrado, para el registro: «3d = Ctrl+Mayus+1, Ctrl+!» */
  const std::string &resumen() const { return resumen_; }

  /* Para probar sin teclado: le pide al escritorio que dispare la accion como si se pulsara. */
  bool invocar(const std::string &nombre);

  /* Por si un visor murio sin cerrar: deja inactivas todas las acciones. Lo llama el vigilante. */
  static void desactivar();

  /* Los nombres de las acciones, en el orden de las teclas 1..8 y F1. */
  static const std::vector<std::string> &nombres();

  void anotar(const char *nombre);   /* lo usa la retrollamada del bus */

private:
  struct sd_bus *bus_ = nullptr;
  bool escucha_ = false;
  std::string ruta_;   /* la del componente en kglobalaccel */
  std::vector<std::string> cola_;
  std::string resumen_;
};
