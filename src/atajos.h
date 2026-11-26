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

#include <map>
#include <string>
#include <vector>

/* Los atajos personalizables (v1.09). Cada accion tiene su combinacion de fabrica y la que se
   elija en el panel de ajustes (seccion ATAJOS DE TECLADO), guardada en serve.conf como
   `atajo_<clave> = Ctrl+Shift+1`. Las combinaciones se escriben siempre igual: los modificadores en
   el orden Ctrl, Alt, Shift, Meta y la tecla (A..Z, 0..9, F1..F12, Left, Right, Up, Down, PgUp,
   PgDown, Home, End, Insert, Delete, Space, Plus o el signo). */
namespace atajos {

struct Def {
  const char *nombre;    /* el de la accion en kglobalaccel: "3d", "prof+", ... */
  const char *clave;     /* la de serve.conf, sin el `atajo_` */
  const char *texto;     /* lo que se ve en el panel, en español (pasa por idioma::tr) */
  const char *defecto;   /* la combinacion de fabrica */
};
/* las ocho acciones que se pueden cambiar, en el orden del panel */
const std::vector<Def> &lista();
/* la combinacion puesta para cada accion (serve.conf o la de fabrica): nombre -> combinacion */
std::map<std::string, std::string> combos();
/* "ctrl + shift + k" -> "Ctrl+Shift+K"; vacio si no se entiende */
std::string normaliza(const std::string &combo);
/* que no se coma una tecla suelta en todo el escritorio: hace falta Ctrl, Alt o Meta */
bool tieneModificador(const std::string &combo);
/* el renglon de las teclas de la pastilla cuando alguna no es la de fabrica (vacio si todas lo son) */
/* `rendimiento` (rama experimental): el renglon de la pastilla del modo rendimiento, sin Profundidad ni Salida */
std::string renglonPastilla(bool legado, bool rendimiento = false, bool foco = false);

}  // namespace atajos

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
