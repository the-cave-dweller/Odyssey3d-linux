/* Avisos por el portal de escritorio (org.freedesktop.portal.Notification).
   Usamos el portal en lugar de hablar con plasmashell directamente para que funcione
   con cualquier escritorio. Los botones que no empiezan por "app." vuelven a nosotros
   con la señal ActionInvoked, así que no hace falta registrarse como aplicación D-Bus. */
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

struct sd_bus;
struct sd_bus_slot;

class Notifier {
public:
  Notifier() = default;
  ~Notifier();
  Notifier(const Notifier &) = delete;
  Notifier &operator=(const Notifier &) = delete;

  bool open(std::string *err = nullptr);
  bool available() const { return bus_ != nullptr; }
  unsigned version() const { return portalVersion_; }

  using ActionFn = std::function<void(const std::string &id, const std::string &action)>;
  void setOnAction(ActionFn fn);

  /* Botones: (identificador interno, etiqueta visible). Como mucho tres se ven bien. */
  bool send(const std::string &id, const std::string &title, const std::string &body,
            const std::vector<std::pair<std::string, std::string>> &buttons, std::string *err = nullptr);
  bool remove(const std::string &id);

  /* La señal ActionInvoked llega desde fuera, la reenvía el manejador de D-Bus. */
  void dispatchAction(const std::string &id, const std::string &action);

  /* Para integrarlo en el bucle poll() del servicio. */
  int fd() const;
  int process();
  void flush() const;

private:
  sd_bus *bus_ = nullptr;
  sd_bus_slot *match_ = nullptr;
  unsigned portalVersion_ = 0;
  ActionFn onAction_;
};
