/* bandeja.h — el icono de OdysseyVK en la bandeja del sistema (v1.03).

   Es un StatusNotifierItem (el protocolo de las bandejas de KDE y de casi todos los escritorios) por
   D-Bus de sesion: lo pone el servicio mientras corre. Un clic abre el panel de ajustes; el clic de
   en medio abre el panel en la pestana Juegos. Sin menu propio (el panel ya lo tiene todo). */
#pragma once
#include <functional>
#include <string>

struct sd_bus;
struct sd_bus_slot;

class Bandeja {
 public:
  Bandeja() = default;
  ~Bandeja();
  Bandeja(const Bandeja &) = delete;
  Bandeja &operator=(const Bandeja &) = delete;
  /* registra el icono; false si no hay bus o no hay bandeja que lo acoja (se puede reintentar) */
  bool poner(std::string *err = nullptr);
  void quitar();
  bool puesta() const { return bus_ != nullptr; }
  /* lo que pasa al picar: `clic` (izquierdo) y `medio` */
  void alPicar(std::function<void(const std::string &)> fn) { alPicar_ = std::move(fn); }
  /* para el bucle del servicio: atender lo que llegue sin bloquear */
  void atender();
  void picado(const std::string &como) { if (alPicar_) alPicar_(como); }

 private:
  sd_bus *bus_ = nullptr;
  sd_bus_slot *objeto_ = nullptr;
  std::string nombre_;
  std::function<void(const std::string &)> alPicar_;
};
