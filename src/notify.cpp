#include "notify.h"

#include <systemd/sd-bus.h>

#include <cerrno>
#include <cstring>

static const char *kDest = "org.freedesktop.portal.Desktop";
static const char *kPath = "/org/freedesktop/portal/desktop";
static const char *kIface = "org.freedesktop.portal.Notification";

/* ---------------- señal de botones ---------------- */

static int onActionInvoked(sd_bus_message *m, void *userdata, sd_bus_error *) {
  Notifier *self = static_cast<Notifier *>(userdata);
  const char *id = nullptr, *action = nullptr;
  int r = sd_bus_message_read(m, "ss", &id, &action);
  if (r < 0) return r;
  sd_bus_message_skip(m, "av");
  if (self && id && action) self->dispatchAction(id, action);
  return 0;
}

/* ---------------- construcción del vardict ---------------- */

namespace {
void addString(sd_bus_message *dict, const char *key, const char *value) {
  sd_bus_message_open_container(dict, 'e', "sv");
  sd_bus_message_append(dict, "s", key);
  sd_bus_message_append(dict, "v", "s", value);
  sd_bus_message_close_container(dict);
}
}  // namespace

bool Notifier::send(const std::string &id, const std::string &title, const std::string &body,
                   const std::vector<std::pair<std::string, std::string>> &buttons, std::string *err) {
  if (!bus_) {
    if (err) *err = "no hay conexión con el portal de escritorio";
    return false;
  }
  sd_bus_message *m = nullptr;
  int r = sd_bus_message_new_method_call(bus_, &m, kDest, kPath, kIface, "AddNotification");
  if (r < 0) {
    if (err) *err = "no se pudo preparar el aviso";
    return false;
  }
  sd_bus_message_append(m, "s", id.c_str());
  sd_bus_message_open_container(m, 'a', "{sv}");
  addString(m, "title", title.c_str());
  addString(m, "body", body.c_str());
  addString(m, "priority", "high");

  if (!buttons.empty()) {
    if (portalVersion_ >= 2) {
      /* versión 2 del portal: botones como array de dicts {label, action} */
      sd_bus_message_open_container(m, 'e', "sv");
      sd_bus_message_append(m, "s", "buttons");
      sd_bus_message_open_container(m, 'v', "aa{sv}");
      sd_bus_message_open_container(m, 'a', "a{sv}");
      for (const auto &b : buttons) {
        sd_bus_message_open_container(m, 'a', "{sv}");
        sd_bus_message_open_container(m, 'e', "sv");
        sd_bus_message_append(m, "s", "label");
        sd_bus_message_append(m, "v", "s", b.second.c_str());
        sd_bus_message_close_container(m);
        sd_bus_message_open_container(m, 'e', "sv");
        sd_bus_message_append(m, "s", "action");
        sd_bus_message_append(m, "v", "s", b.first.c_str());
        sd_bus_message_close_container(m);
        sd_bus_message_close_container(m);
      }
      sd_bus_message_close_container(m);
      sd_bus_message_close_container(m);
      sd_bus_message_close_container(m);
    } else {
      /* versión 1: diccionario acción -> etiqueta */
      sd_bus_message_open_container(m, 'e', "sv");
      sd_bus_message_append(m, "s", "actions");
      sd_bus_message_open_container(m, 'v', "a{ss}");
      sd_bus_message_open_container(m, 'a', "{ss}");
      for (const auto &b : buttons) {
        sd_bus_message_open_container(m, 'e', "ss");
        sd_bus_message_append(m, "s", b.first.c_str());
        sd_bus_message_append(m, "s", b.second.c_str());
        sd_bus_message_close_container(m);
      }
      sd_bus_message_close_container(m);
      sd_bus_message_close_container(m);
      sd_bus_message_close_container(m);
    }
  }
  sd_bus_message_close_container(m);

  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = nullptr;
  r = sd_bus_call(bus_, m, 5000, &error, &reply);
  sd_bus_message_unref(m);
  if (reply) sd_bus_message_unref(reply);
  if (r < 0) {
    if (err) *err = std::string("el portal rechazó el aviso: ") + (error.message ? error.message : std::strerror(-r));
    sd_bus_error_free(&error);
    return false;
  }
  sd_bus_error_free(&error);
  return true;
}

bool Notifier::remove(const std::string &id) {
  if (!bus_) return false;
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = nullptr;
  int r = sd_bus_call_method(bus_, kDest, kPath, kIface, "RemoveNotification", &error, &reply, "s", id.c_str());
  if (reply) sd_bus_message_unref(reply);
  sd_bus_error_free(&error);
  return r >= 0;
}

/* ---------------- conexión ---------------- */

bool Notifier::open(std::string *err) {
  int r = sd_bus_open_user(&bus_);
  if (r < 0) {
    if (err) *err = "no hay sesión de D-Bus de usuario";
    bus_ = nullptr;
    return false;
  }
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = nullptr;
  unsigned int version = 0;
  r = sd_bus_call_method(bus_, kDest, kPath, "org.freedesktop.DBus.Properties", "Get", &error, &reply, "ss", kIface,
                         "version");
  if (r >= 0 && reply) {
    sd_bus_message_read(reply, "v", "u", &version);
    sd_bus_message_unref(reply);
  }
  sd_bus_error_free(&error);
  portalVersion_ = version;

  r = sd_bus_match_signal(bus_, &match_, kDest, kPath, kIface, "ActionInvoked", &onActionInvoked, this);
  if (r < 0) {
    if (err) *err = "no se pudo escuchar ActionInvoked del portal";
    return false;
  }
  return true;
}

Notifier::~Notifier() {
  if (match_) sd_bus_slot_unref(match_);
  if (bus_) {
    sd_bus_flush(bus_);
    sd_bus_close(bus_);
    sd_bus_unref(bus_);
  }
}

void Notifier::setOnAction(ActionFn fn) { onAction_ = std::move(fn); }

void Notifier::dispatchAction(const std::string &id, const std::string &action) {
  if (onAction_) onAction_(id, action);
}

int Notifier::fd() const { return bus_ ? sd_bus_get_fd(bus_) : -1; }

int Notifier::process() {
  if (!bus_) return -1;
  int made = 0;
  for (;;) {
    int r = sd_bus_process(bus_, nullptr);
    if (r < 0) return r;
    if (r == 0) break;
    made++;
  }
  return made;
}

void Notifier::flush() const {
  if (bus_) sd_bus_flush(bus_);
}
