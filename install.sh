#!/usr/bin/env bash
# Compila e instala odysseyvk (binario, regla udev y datos de calibración).
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$here"

# --- dependencias -----------------------------------------------------------
missing=()
for pc in glfw3 libcrypto; do
  pkg-config --exists "$pc" || missing+=("$pc")
done
# (v1.18) el compilador de los efectos de ReShade (third_party/reshadefx) saca SPIR-V con las cabeceras de Khronos
[ -f /usr/include/spirv/unified1/spirv.hpp ] || missing+=("spirv-headers")
opt=()
pkg-config --exists mpv    || opt+=("mpv (hace falta para `play`)")
pkg-config --exists libpng || opt+=("libpng (mapas de corrección del panel)")
if ((${#missing[@]})); then
  echo "faltan dependencias: ${missing[*]}" >&2
  echo "  sudo pacman -S --needed cmake gcc pkgconf glfw openssl libpng mpv spirv-headers" >&2
  exit 1
fi
if ((${#opt[@]})); then
  echo "aviso: sin estas el binario pierde funciones:" >&2
  printf '  - %s\n' "${opt[@]}" >&2
fi

# --- compilar ---------------------------------------------------------------
# (v1.17) En silencio: todo el detalle va a build/compilacion.log y de la configuracion solo salen las lineas de
# OdysseyVK que dicen que trae (el conversor 2D a 3D, la ventana de ajustes) y los avisos; si algo falla, salen los
# errores. Antes salian cientos de avisos del compilador, que asustaban sin decir nada al que instala.
# ODYSSEYVK_VERBOSO=1 lo deja todo a la vista, como antes.
if [ -n "${ODYSSEYVK_VERBOSO:-}" ]; then
  cmake -S "$here" -B "$here/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$here/build" -j"$(nproc)"
else
  mkdir -p "$here/build"
  log="$here/build/compilacion.log"
  fallo() {
    echo "la compilación falló; el registro entero está en $log. Lo último:" >&2
    { grep -E -i -B2 -A3 'error|no se encontr|not found' "$log" || cat "$log"; } | tail -n 40 >&2
    exit 1
  }
  cmake -S "$here" -B "$here/build" -DCMAKE_BUILD_TYPE=Release > "$log" 2>&1 || fallo
  { grep -E '^-- (ONNX Runtime|Sin ONNX|Ventana de ajustes|sin xdg-shell|sin fractional)' "$log" | sed 's/^-- /  /'
    grep -E -A3 '^CMake Warning' "$log"; } || true
  echo "compilando OdysseyVK (unos minutos; el detalle queda en build/compilacion.log)…"
  cmake --build "$here/build" -j"$(nproc)" >> "$log" 2>&1 || fallo
  echo "compilado"
fi

# --- instalar ---------------------------------------------------------------
echo
# ODYSSEYVK_SUDO=pkexec lo usa `odysseyvk actualizar` (v0.95): pide la clave en una ventana del
# escritorio en vez de en la terminal. Una sola peticion para el binario, la regla udev y los datos.
su=${ODYSSEYVK_SUDO:-sudo}
echo "se va a pedir la clave ($su) para instalar el binario, la regla udev y los datos de calibración"
prefix=$(sed -n 's/^CMAKE_INSTALL_PREFIX:INTERNAL=//p' "$here/build/CMakeCache.txt")
prefix=${prefix:-/usr/local}
# (v1.18) antes de instalar, lo que hay ahora se guarda para poder volver (tools/versiones.sh: se quedan las dos
# últimas en $prefix/lib/odysseyvk/anteriores; `odysseyvk volver` las devuelve). Con la misma clave que la instalación.
# «funcionaba»: el servicio apunta en ~/.local/state/odysseyvk/funcionaba el binario con el que hubo un 3D de un minuto
# (su suma, no su número de versión: de una misma versión salen muchos binarios)
ver_ahora=""
[ -x "$prefix/bin/odysseyvk" ] && ver_ahora=$("$prefix/bin/odysseyvk" --version 2>/dev/null | awk 'NR==1{print $2}') || true
funcionaba=no
[ -n "$ver_ahora" ] && grep -qxF "$(cksum < "$prefix/bin/odysseyvk")" "${XDG_STATE_HOME:-$HOME/.local/state}/odysseyvk/funcionaba" 2>/dev/null && funcionaba=si
guardar=":"
[ -n "$ver_ahora" ] && guardar="sh '$here/tools/versiones.sh' guardar '$prefix' '$ver_ahora' $funcionaba || echo 'aviso: no se pudo guardar la versión anterior'"
$su sh -c "$guardar; cmake --install '$here/build' && udevadm control --reload-rules"

# --- servicio en segundo plano (por usuario, sin sudo) ----------------------
prefix=$(sed -n 's/^CMAKE_INSTALL_PREFIX:INTERNAL=//p' "$here/build/CMakeCache.txt")
prefix=${prefix:-/usr/local}
unit_dir=${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user
conf_dir=${XDG_CONFIG_HOME:-$HOME/.config}/odysseyvk

mkdir -p "$unit_dir" "$conf_dir"
sed "s|@BINDIR@|$prefix/bin|g" "$here/systemd/odysseyvk-serve.service" > "$unit_dir/odysseyvk-serve.service"
[ -f "$conf_dir/serve.conf" ] || cp "$here/config/serve.conf" "$conf_dir/serve.conf"
systemctl --user daemon-reload
systemctl --user enable odysseyvk-serve.service
# y en marcha ya, con la version recien instalada (v0.97: antes quedaba parado hasta el siguiente
# inicio de sesion, o hasta que alguien lo arrancara a mano; si ya corria, se reinicia con la nueva)
if systemctl --user restart odysseyvk-serve.service 2>/dev/null; then
  echo "servicio instalado, activado y en marcha: $unit_dir/odysseyvk-serve.service"
else
  echo "servicio instalado y activado para la próxima sesión (no se pudo arrancar ahora: systemctl --user start odysseyvk-serve)"
fi

# --- la capa de Vulkan de la profundidad de DLSS (v1.13): su manifiesto, por usuario -------------
# Se carga solo en los juegos que lleven `ODYSSEYVK=1` en sus opciones de lanzamiento (Steam).
capas=${XDG_DATA_HOME:-$HOME/.local/share}/vulkan/implicit_layer.d
mkdir -p "$capas"
cp -f "$here/build/capa/instalada/VkLayer_odysseyvk_capa.json" "$capas/VkLayer_odysseyvk_capa.json"
# (v1.18) la generacion de cuadros de MAKO, contada: el contador (explicito: solo lo carga la meta-capa) y la meta-capa que
# pone [la capa, MAKO, el contador] con ODYSSEYVK_CONTAR_MAKO=1 (lo escribe Integraciones con ODYSSEYVK y MAKO puestos)
explicitas=${XDG_DATA_HOME:-$HOME/.local/share}/vulkan/explicit_layer.d
mkdir -p "$explicitas"
cp -f "$here/build/capa/instalada/VkLayer_odysseyvk_contador.json" "$explicitas/VkLayer_odysseyvk_contador.json"
cp -f "$here/build/capa/instalada/VkLayer_odysseyvk_mako_contado.json" "$capas/VkLayer_odysseyvk_mako_contado.json"
echo "capa de la profundidad de DLSS puesta: añade ODYSSEYVK=1 a las opciones de lanzamiento del juego en Steam"

# --- el icono y el lanzador del menú (por usuario, sin sudo) ------------------
datos=${XDG_DATA_HOME:-$HOME/.local/share}
for carpeta in "$here"/assets/hicolor/*x*/apps; do
  if [ -d "$carpeta" ]; then
    tam=$(basename "$(dirname "$carpeta")")
    mkdir -p "$datos/icons/hicolor/$tam/apps"
    cp -f "$carpeta/odysseyvk.png" "$datos/icons/hicolor/$tam/apps/odysseyvk.png"
  fi
done
mkdir -p "$datos/applications"
# El lanzador del menú abre la ventana de ajustes. Antes hacía `odysseyvk play %F`, y al pulsarlo sin
# ningún archivo detrás el %F se queda vacío: el visor contesta "falta el archivo" y sale con código 2,
# así que el icono del menú no abría nada. La asociación con vídeos vive aparte, en
# odysseyvk-play.desktop (NoDisplay, no aparece en el menú).
cp -f "$here/odysseyvk.desktop" "$datos/applications/odysseyvk.desktop"
cp -f "$here/odysseyvk-play.desktop" "$datos/applications/odysseyvk-play.desktop"
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -q -t -f "$datos/icons/hicolor" || true
fi
update-desktop-database "$datos/applications" 2>/dev/null || true
echo "icono y dos lanzadores del menú puestos en $datos (Icon=odysseyvk)"

# --- lo del nombre viejo (odyssey3d): apagar y quitar lo que estorba ----------
# Desde v0.57 todo se llama odysseyvk. El servicio viejo hay que apagarlo: dos vigilantes
# mirando las mismas ventanas pelean por la lente y el panel. Lo demas se guarda aparte.
if [ -f "$unit_dir/odyssey3d-serve.service" ]; then
  systemctl --user disable --now odyssey3d-serve.service >/dev/null 2>&1 || true
  rm -f "$unit_dir/odyssey3d-serve.service"
  echo "servicio viejo apagado y quitado: odyssey3d-serve.service"
fi
rm -f "$datos/applications/odyssey3d.desktop"
if [ -e "$prefix/bin/odyssey3d" ] || [ -e "$prefix/bin/odyssey3d-ajustes" ]; then
  # (v1.13) por $su como el resto: con `odysseyvk actualizar` no hay terminal y `sudo` no puede pedir la clave
  $su mkdir -p "$prefix/bin/viejo-odyssey3d"
  for b in odyssey3d odyssey3d-ajustes; do
    if [ -e "$prefix/bin/$b" ]; then
      $su mv "$prefix/bin/$b" "$prefix/bin/viejo-odyssey3d/$b"
    fi
  done
  echo "los binarios del nombre viejo se guardaron en $prefix/bin/viejo-odyssey3d"
fi
if [ -f /etc/udev/rules.d/70-odyssey-3d.rules ]; then
  $su rm -f /etc/udev/rules.d/70-odyssey-3d.rules
  echo "regla udev del nombre viejo quitada (la nueva es 70-odysseyvk.rules)"
fi

cat <<'EOF'

Instalado. Los datos del nombre viejo (config/odyssey3d, .local/state/odyssey3d y
.local/share/odyssey3d, con la calibración, las redes y los perfiles) se copian solos la
primera vez que arranca odysseyvk: a partir de ahí viven en las carpetas .../odysseyvk.
Lo del nombre viejo no se borra, por si quieres volver atrás.

Para que los permisos surtan efecto:

  * NO ejecutes `udevadm trigger` sobre el monitor: reinicializa su hub interno y
    desconecta la lente, el scaler y la cámara hasta que vuelvas a conectar el USB.
  * En su lugar, desenchufa y vuelve a enchufar el cable USB-B del monitor
    (o apágalo y enciéndelo).

Comprueba con:  odysseyvk doctor
Reproduce con:  odysseyvk play tu_video_sbs.mkv

Servicio de ventanas (avisa cuando algo se pone a pantalla completa y lo pone en 3D):
  systemctl --user start odysseyvk-serve      # arrancarlo ahora
  systemctl --user stop odysseyvk-serve       # pararlo
  systemctl --user disable odysseyvk-serve    # que no arranque con la sesión
  journalctl --user -u odysseyvk-serve -f     # ver lo que hace
  ~/.config/odysseyvk/serve.conf              # ignorar apps, tiempo del aviso, modo
EOF
