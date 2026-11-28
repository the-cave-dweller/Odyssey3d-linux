#!/bin/sh
# versiones.sh — (v1.18) las versiones anteriores de OdysseyVK, para poder volver.
# Rodolfo: «implementa un sistema de fallback que permita regresar hasta dos versiones si el programa funcionaba previo
# al error».
#
#   versiones.sh guardar PREFIJO VERSION si|no   antes de instalar: lo instalado ahora se copia a
#                                                PREFIJO/lib/odysseyvk/anteriores/FECHA-VERSION (se quedan las dos
#                                                ultimas). «si|no»: esa version llego a funcionar (un 3D de un minuto)
#   versiones.sh lista PREFIJO                   una por renglon, la mas nueva primero: carpeta|version|fecha|funcionaba
#   versiones.sh volver PREFIJO CARPETA          lo guardado en esa carpeta vuelve a su sitio
#
# Lo que se guarda es el programa: bin/odysseyvk, bin/odysseyvk-ajustes, bin/odysseyvk-panel y lib/odysseyvk/*.so (la
# capa de Vulkan). La regla udev, la calibracion, los ajustes y las redes no cambian de una version a otra y no se tocan.
# guardar y volver escriben en PREFIJO: los llama install.sh o `odysseyvk volver` con sudo o pkexec. Este guion se
# instala en PREFIJO/lib/odysseyvk para que la version que quede siempre lo tenga.
# Se prueba sin root con un PREFIJO de mentira: tools/prueba-versiones.sh
set -eu

uso() { sed -n '2,12p' "$0" >&2; exit 2; }
[ $# -ge 2 ] || uso
orden=$1
prefijo=$2
almacen="$prefijo/lib/odysseyvk/anteriores"

# los ficheros del programa que hay ahora en PREFIJO, relativos a el
programa() {
  for f in bin/odysseyvk bin/odysseyvk-ajustes bin/odysseyvk-panel; do
    [ -f "$prefijo/$f" ] && echo "$f"
  done
  for f in "$prefijo"/lib/odysseyvk/*.so; do
    [ -f "$f" ] && echo "lib/odysseyvk/$(basename "$f")"
  done
  return 0
}

# las carpetas guardadas, la mas nueva primero (su nombre empieza por la fecha)
guardadas() {
  [ -d "$almacen" ] || return 0
  for d in "$almacen"/*/; do
    [ -f "${d}info" ] && basename "$d"
  done | sort -r
}

huella() { [ -f "$1" ] && cksum < "$1" || echo nada; }

# con sus permisos, su fecha y sus capacidades (el visor puede llevar las de setcap); si el cp no sabe, al menos lo primero
copia() { cp --preserve=all "$1" "$2" 2>/dev/null || cp -p "$1" "$2"; }

case "$orden" in
  guardar)
    [ $# -eq 4 ] || uso
    version=$3
    funcionaba=$4
    [ -f "$prefijo/bin/odysseyvk" ] || { echo "no hay nada instalado que guardar"; exit 0; }
    # la misma que una ya guardada (se reinstala lo mismo, o se habia vuelto a ella): no gasta un hueco, solo se apunta
    # si ya funcionaba
    ahora=$(huella "$prefijo/bin/odysseyvk")
    for g in $(guardadas); do
      if [ "$(huella "$almacen/$g/bin/odysseyvk")" = "$ahora" ]; then
        if [ "$funcionaba" = si ]; then
          sed -i 's/^funcionaba=.*/funcionaba=si/' "$almacen/$g/info"
        fi
        echo "la version instalada ya estaba guardada ($g)"
        exit 0
      fi
    done
    carpeta="$(date +%Y%m%d-%H%M%S)-$(printf '%s' "$version" | tr -c 'A-Za-z0-9._-' '_')"
    mkdir -p "$almacen/$carpeta"
    programa | while read -r f; do
      mkdir -p "$almacen/$carpeta/$(dirname "$f")"
      copia "$prefijo/$f" "$almacen/$carpeta/$f"
    done
    { echo "version=$version"; echo "fecha=$(date '+%Y-%m-%d %H:%M')"; echo "funcionaba=$funcionaba"; } > "$almacen/$carpeta/info"
    echo "guardada la version $version en $almacen/$carpeta"
    # se quedan las dos ultimas
    guardadas | tail -n +3 | while read -r vieja; do
      case "$vieja" in */*|""|.*) continue ;; esac
      rm -rf "${almacen:?}/$vieja"
      echo "quitada la mas vieja: $vieja"
    done
    ;;
  lista)
    guardadas | while read -r d; do
      v=$(sed -n 's/^version=//p' "$almacen/$d/info")
      f=$(sed -n 's/^fecha=//p' "$almacen/$d/info")
      w=$(sed -n 's/^funcionaba=//p' "$almacen/$d/info")
      echo "$d|$v|$f|$w"
    done
    ;;
  volver)
    [ $# -eq 3 ] || uso
    carpeta=$3
    case "$carpeta" in */*|""|.*) echo "esa no es una version guardada: $carpeta" >&2; exit 1 ;; esac
    origen="$almacen/$carpeta"
    [ -f "$origen/info" ] && [ -f "$origen/bin/odysseyvk" ] || { echo "no esta guardada: $carpeta" >&2; exit 1; }
    # cada fichero se copia al lado y se cambia de golpe: el programa que corre no se queda a medias
    (cd "$origen" && find bin lib -type f 2>/dev/null) | while read -r f; do
      mkdir -p "$prefijo/$(dirname "$f")"
      copia "$origen/$f" "$prefijo/$f.volviendo"
      mv -f "$prefijo/$f.volviendo" "$prefijo/$f"
    done
    echo "de vuelta la version $(sed -n 's/^version=//p' "$origen/info") ($(sed -n 's/^fecha=//p' "$origen/info"))"
    ;;
  *) uso ;;
esac
