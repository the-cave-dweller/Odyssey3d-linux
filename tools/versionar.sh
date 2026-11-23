#!/usr/bin/env bash
# Guarda una copia del código fuente (sin build ni .git) en ../Versiones, con la
# versión de VERSION, una etiqueta y la fecha. Úsalo al terminar cada hito.
#   tools/versionar.sh opengl
#   tools/versionar.sh vulkan
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$here"

ver=$(tr -d '[:space:]' < VERSION)
etiqueta=${1:-$(git rev-parse --short HEAD 2>/dev/null || echo manual)}
fecha=$(date +%Y-%m-%d)
dest_parent="$(cd "$here/.." && pwd)/Versiones"
dest="$dest_parent/v${ver}-${etiqueta}-${fecha}"

mkdir -p "$dest"
tar -cf - --exclude=./build --exclude=.git --exclude='*.o' --exclude='*.pyc' . | tar -xf - -C "$dest"

{
  echo "commit: $(git rev-parse --short HEAD 2>/dev/null || echo sin-git)"
  echo "fecha:  $(date '+%Y-%m-%d %H:%M')"
  echo "cambios sin commitear al guardar:"
  git status --short 2>/dev/null || true
} > "$dest/ESTADO.txt"

echo "versión guardada en $dest"
