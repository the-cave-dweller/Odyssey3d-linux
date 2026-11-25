#!/usr/bin/env bash
# Corre `odysseyvk test` dentro de un compositor APARTE y sin salida (gamescope en modo headless),
# con su propia carpeta de estado. No abre ninguna ventana en el escritorio, no le quita el foco a
# nada y no toca la sesion 3D que haya abierta: sirve para probar el visor (la trama, la pastilla,
# los cambios en marcha con ODYSSEY_GOLPE, el volcado) mientras Rodolfo esta viendo un video.
#
#   tools/prueba-aparte.sh [-t SEGUNDOS] [-e CARPETA_DE_ESTADO] [VAR=valor ...] -- ARGUMENTOS
#
#   tools/prueba-aparte.sh ODYSSEYVK_FUENTE_PNG=/ruta/cuadro.png ODYSSEYVK_FUENTE_VIVA=1 \
#       ODYSSEYVK_VOLCADO_CUADRO=80 -- --sbs --sin-interfaz --dump /ruta/salida
#
# Los ARGUMENTOS son los de `odysseyvk test`; aqui se anaden siempre --windowed --no-panel
# --no-lens --no-scaler (sin panel, sin lente y sin scaler: ahi dentro no hay monitor). El lienzo
# sale de 3840x2160. Para mandarle ordenes al visor de prueba (`ctl ...`) hay que usar la misma
# carpeta de estado:  XDG_STATE_HOME=CARPETA build/odysseyvk ctl ...
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
bin="$here/build/odysseyvk"
tope=60
estado=""
while [ $# -gt 0 ]; do
  case "$1" in
    -t) tope="$2"; shift 2 ;;
    -e) estado="$2"; shift 2 ;;
    *) break ;;
  esac
done
ambiente=()
while [ $# -gt 0 ] && [ "$1" != "--" ]; do ambiente+=("$1"); shift; done
[ "${1:-}" = "--" ] && shift
command -v gamescope >/dev/null || { echo "falta gamescope (es el compositor aparte)" >&2; exit 1; }
[ -x "$bin" ] || { echo "no esta compilado: $bin" >&2; exit 1; }
[ -n "$estado" ] || estado=$(mktemp -d "${TMPDIR:-/tmp}/odysseyvk-aparte.XXXXXX")
mkdir -p "$estado"

# La guarda de dentro: si por lo que sea el visor no fuera a conectarse al compositor aparte (y si
# al escritorio de verdad), no se lanza.
exec timeout -k 3 "$tope" gamescope --backend headless -W 3840 -H 2160 --expose-wayland -- \
  env XDG_STATE_HOME="$estado" "${ambiente[@]}" \
  sh -c 'case "$WAYLAND_DISPLAY" in gamescope-*) ;; *) echo "esto NO es el compositor aparte: no se lanza nada" >&2; exit 9 ;; esac; exec "$0" "$@"' \
  "$bin" test --windowed --no-panel --no-lens --no-scaler "$@"
