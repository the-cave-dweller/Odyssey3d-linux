#!/usr/bin/env bash
# Compila e instala odyssey3d (binario, regla udev y datos de calibración).
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$here"

# --- dependencias -----------------------------------------------------------
missing=()
for pc in glfw3 libcrypto; do
  pkg-config --exists "$pc" || missing+=("$pc")
done
opt=()
pkg-config --exists mpv    || opt+=("mpv (hace falta para `play`)")
pkg-config --exists libpng || opt+=("libpng (mapas de corrección del panel)")
if ((${#missing[@]})); then
  echo "faltan dependencias: ${missing[*]}" >&2
  echo "  sudo pacman -S --needed cmake gcc pkgconf glfw openssl libpng mpv" >&2
  exit 1
fi
if ((${#opt[@]})); then
  echo "aviso: sin estas el binario pierde funciones:" >&2
  printf '  - %s\n' "${opt[@]}" >&2
fi

# --- compilar ---------------------------------------------------------------
cmake -S "$here" -B "$here/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$here/build" -j"$(nproc)"

# --- instalar ---------------------------------------------------------------
echo
echo "se va a pedir sudo para instalar el binario, la regla udev y los datos de calibración"
sudo cmake --install "$here/build"

# --- permisos ---------------------------------------------------------------
sudo udevadm control --reload-rules

# --- servicio en segundo plano (por usuario, sin sudo) ----------------------
prefix=$(sed -n 's/^CMAKE_INSTALL_PREFIX:INTERNAL=//p' "$here/build/CMakeCache.txt")
prefix=${prefix:-/usr/local}
unit_dir=${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user
conf_dir=${XDG_CONFIG_HOME:-$HOME/.config}/odyssey3d

mkdir -p "$unit_dir" "$conf_dir"
sed "s|@BINDIR@|$prefix/bin|g" "$here/systemd/odyssey3d-serve.service" > "$unit_dir/odyssey3d-serve.service"
[ -f "$conf_dir/serve.conf" ] || cp "$here/config/serve.conf" "$conf_dir/serve.conf"
systemctl --user daemon-reload
systemctl --user enable odyssey3d-serve.service
echo "servicio instalado y activado para la próxima sesión: $unit_dir/odyssey3d-serve.service"

cat <<'EOF'

Instalado. Para que los permisos surtan efecto:

  * NO ejecutes `udevadm trigger` sobre el monitor: reinicializa su hub interno y
    desconecta la lente, el scaler y la cámara hasta que vuelvas a conectar el USB.
  * En su lugar, desenchufa y vuelve a enchufar el cable USB-B del monitor
    (o apágalo y enciéndelo).

Comprueba con:  odyssey3d doctor
Reproduce con:  odyssey3d play tu_video_sbs.mkv

Servicio de ventanas (avisa cuando algo se pone a pantalla completa y lo pone en 3D):
  systemctl --user start odyssey3d-serve      # arrancarlo ahora
  systemctl --user stop odyssey3d-serve       # pararlo
  systemctl --user disable odyssey3d-serve    # que no arranque con la sesión
  journalctl --user -u odyssey3d-serve -f     # ver lo que hace
  ~/.config/odyssey3d/serve.conf              # ignorar apps, tiempo del aviso, modo
EOF
