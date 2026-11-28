#!/bin/bash
# prueba-versiones.sh — tools/versiones.sh con un PREFIJO de mentira (sin root y sin tocar lo instalado): guardar, no
# gastar hueco con lo mismo, quedarse con dos, la lista y volver.
set -u
aqui=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
v="$aqui/versiones.sh"
p=$(mktemp -d "${TMPDIR:-/tmp}/odysseyvk-versiones.XXXXXX")
trap 'rm -rf "$p"' EXIT
fallos=0
mira() { if eval "$1"; then echo "bien   $2"; else echo "MAL    $2"; fallos=$((fallos + 1)); fi; }
instala() {   # una «version» de mentira: su contenido es su nombre
  mkdir -p "$p/bin" "$p/lib/odysseyvk"
  for f in bin/odysseyvk bin/odysseyvk-ajustes bin/odysseyvk-panel lib/odysseyvk/VkLayer_odysseyvk_capa.so; do echo "$1 $f" > "$p/$f"; done
  chmod 755 "$p/bin/odysseyvk"
}
n() { sh "$v" lista "$p" | wc -l; }

sh "$v" guardar "$p" 1.16 si > /dev/null
mira '[ "$(n)" = 0 ]' "sin nada instalado no se guarda nada"
instala 1.16
sh "$v" guardar "$p" 1.16 si > /dev/null; sleep 1
mira '[ "$(n)" = 1 ]' "antes de instalar la 1.17 queda guardada la 1.16"
sh "$v" guardar "$p" 1.16 si > /dev/null
mira '[ "$(n)" = 1 ]' "reinstalar lo mismo no gasta otro hueco"
instala 1.17
sh "$v" guardar "$p" 1.17 no > /dev/null; sleep 1
instala 1.18
sh "$v" guardar "$p" 1.18 si > /dev/null; sleep 1
mira '[ "$(n)" = 2 ]' "se quedan dos"
mira '[ "$(sh "$v" lista "$p" | cut -d"|" -f2 | tr "\n" " ")" = "1.18 1.17 " ]' "las dos ultimas, la mas nueva primero (la 1.16 se fue)"
mira '[ "$(sh "$v" lista "$p" | cut -d"|" -f4 | tr "\n" " ")" = "si no " ]' "cada una con si funcionaba"
instala 1.19
vieja=$(sh "$v" lista "$p" | sed -n 2p | cut -d"|" -f1)
sh "$v" volver "$p" "$vieja" > /dev/null
mira '[ "$(cat "$p/bin/odysseyvk")" = "1.17 bin/odysseyvk" ] && [ "$(cat "$p/lib/odysseyvk/VkLayer_odysseyvk_capa.so")" = "1.17 lib/odysseyvk/VkLayer_odysseyvk_capa.so" ]' \
     "volver a la segunda deja su programa y su capa"
mira '[ -x "$p/bin/odysseyvk" ] && ! ls "$p"/bin/*.volviendo > /dev/null 2>&1' "con sus permisos y sin restos"
mira '[ "$(n)" = 2 ]' "y las guardadas siguen ahi"
mira '! sh "$v" volver "$p" ../../bin > /dev/null 2>&1 && ! sh "$v" volver "$p" no-existe > /dev/null 2>&1' "una carpeta que no es de las guardadas no se acepta"
sh "$v" guardar "$p" 1.17 si > /dev/null   # lo instalado ahora es la 1.17 a la que se volvio
mira '[ "$(sh "$v" lista "$p" | cut -d"|" -f2,4 | tr "\n" " ")" = "1.18|si 1.17|si " ]' \
     "instalar encima de la version a la que se volvio no la guarda otra vez: siguen la 1.18 y la 1.17, y esta ya consta que funcionaba"
[ "$fallos" = 0 ] && echo "todo bien" || echo "HAY FALLOS"
exit "$fallos"
