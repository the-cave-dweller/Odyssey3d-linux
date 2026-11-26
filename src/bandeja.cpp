/* bandeja.cpp — ver bandeja.h. */
#include "bandeja.h"
#include "bususuario.h"
#include "idioma.h"

#include <systemd/sd-bus.h>
#include <unistd.h>

#include <cstring>

namespace {
const char *kRuta = "/StatusNotifierItem";
const char *kIface = "org.kde.StatusNotifierItem";

int propiedad(sd_bus *, const char *, const char *, const char *prop, sd_bus_message *reply, void *, sd_bus_error *) {
  if (!std::strcmp(prop, "Category")) return sd_bus_message_append(reply, "s", "ApplicationStatus");
  if (!std::strcmp(prop, "Id")) return sd_bus_message_append(reply, "s", "odysseyvk");
  if (!std::strcmp(prop, "Title")) return sd_bus_message_append(reply, "s", "OdysseyVK");
  if (!std::strcmp(prop, "Status")) return sd_bus_message_append(reply, "s", "Active");
  if (!std::strcmp(prop, "IconName")) return sd_bus_message_append(reply, "s", "odysseyvk");
  if (!std::strcmp(prop, "ItemIsMenu")) return sd_bus_message_append(reply, "b", 0);
  if (!std::strcmp(prop, "WindowId")) return sd_bus_message_append(reply, "i", 0);
  if (!std::strcmp(prop, "Menu")) return sd_bus_message_append(reply, "o", "/NO_DBUSMENU");
  if (!std::strcmp(prop, "ToolTip")) {
    /* (sa(iiay)ss): icono, sin imagen, titulo y texto */
    int r = sd_bus_message_open_container(reply, 'r', "sa(iiay)ss");
    if (r < 0) return r;
    sd_bus_message_append(reply, "s", "odysseyvk");
    sd_bus_message_open_container(reply, 'a', "(iiay)");
    sd_bus_message_close_container(reply);
    sd_bus_message_append(reply, "ss", "OdysseyVK", idioma::tr("Clic: ajustes · clic central: juegos"));
    return sd_bus_message_close_container(reply);
  }
  return -EINVAL;
}

int metodo(sd_bus_message *m, void *userdata, sd_bus_error *) {
  Bandeja *b = static_cast<Bandeja *>(userdata);
  const char *miembro = sd_bus_message_get_member(m);
  if (b && miembro) {
    if (!std::strcmp(miembro, "Activate")) b->picado("clic");
    else if (!std::strcmp(miembro, "SecondaryActivate")) b->picado("medio");
    else if (!std::strcmp(miembro, "ContextMenu")) b->picado("clic");
  }
  return sd_bus_reply_method_return(m, "");
}

const sd_bus_vtable kTabla[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Category", "s", propiedad, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Id", "s", propiedad, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Title", "s", propiedad, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Status", "s", propiedad, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("IconName", "s", propiedad, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ItemIsMenu", "b", propiedad, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("WindowId", "i", propiedad, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Menu", "o", propiedad, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ToolTip", "(sa(iiay)ss)", propiedad, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("Activate", "ii", "", metodo, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SecondaryActivate", "ii", "", metodo, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ContextMenu", "ii", "", metodo, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Scroll", "is", "", metodo, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};
}  // namespace

Bandeja::~Bandeja() { quitar(); }

bool Bandeja::poner(std::string *err) {
  if (bus_) return true;
  int r = abrirBusUsuario(&bus_);
  if (r < 0) {
    if (err) *err = std::string("sin bus de sesion: ") + std::strerror(-r);
    bus_ = nullptr;
    return false;
  }
  nombre_ = "org.kde.StatusNotifierItem-" + std::to_string((long)::getpid()) + "-1";
  r = sd_bus_add_object_vtable(bus_, &objeto_, kRuta, kIface, kTabla, this);
  if (r >= 0) r = sd_bus_request_name(bus_, nombre_.c_str(), 0);
  if (r >= 0) {
    sd_bus_error e = SD_BUS_ERROR_NULL;
    r = sd_bus_call_method(bus_, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                           "RegisterStatusNotifierItem", &e, nullptr, "s", nombre_.c_str());
    if (r < 0 && err) *err = std::string("la bandeja no lo acepto: ") + (e.message ? e.message : std::strerror(-r));
    sd_bus_error_free(&e);
  } else if (err) {
    *err = std::string("no se pudo publicar el icono: ") + std::strerror(-r);
  }
  if (r < 0) {
    quitar();
    return false;
  }
  return true;
}

void Bandeja::quitar() {
  if (objeto_) objeto_ = sd_bus_slot_unref(objeto_);
  if (bus_) {
    if (!nombre_.empty()) sd_bus_release_name(bus_, nombre_.c_str());
    bus_ = sd_bus_flush_close_unref(bus_);
  }
}

void Bandeja::atender() {
  if (!bus_) return;
  for (int i = 0; i < 16 && sd_bus_process(bus_, nullptr) > 0; i++) {
  }
}
