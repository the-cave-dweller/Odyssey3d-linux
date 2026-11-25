# El modo de legado (v0.57)

**Qué es.** Un interruptor de la hoja *General* de la ventana de ajustes (y su equivalente en la línea
de mando) que cambia **cómo se arma la imagen**, no la sesión. Con el modo puesto, el visor da por
hecho que **el vídeo ya llega con sus dos vistas** (estéreo nativo del monitor / del reproductor) y
apaga todo lo que sirve para sacar dos vistas de un vídeo normal: la red de profundidad, el conversor
2D→3D y los números finos de la trama, que vuelven a sus valores de arranque. La pastilla del overlay
se queda solo con lo que ahí se usa.

> En la interfaz el modo se llama únicamente **«modo de legado»**. Este fichero y `CAMBIOS.md` sí
> dicen de dónde viene la idea: la imitación del método del cliente oficial de Mac para el mismo
> monitor, `~/Descargas/macdissey-3d-0.10.0` (Swift/C++), que es nuestra referencia local.

---

## 1. Cuatro formas de ponerlo y quitarlo

| dónde | cómo |
|---|---|
| ventana de ajustes | hoja **General** → grupo *Modo de la app* → casilla **«modo de legado»** |
| línea de mando del visor | `odysseyvk play --legado …` |
| orden en caliente | `odysseyvk ctl legado encender \| apagar \| alternar \| estado` |
| para la próxima sesión | `legado = si` / `legado = no` en `~/.config/odysseyvk/serve.conf` |

La casilla de la ventana y `ctl legado` escriben **las dos cosas a la vez**: la orden por el canal
(`~/.local/state/odysseyvk/ajuste`, clave `legado`) para el visor que esté abierto, y `serve.conf`
para el siguiente arranque. Por eso el cambio se nota al instante sin reiniciar nada
(`src/main.cpp`, `cmdLegado`; `src/vkview.cpp:1639-1654`).

## 2. Qué toca por dentro (con el sitio exacto del código)

- **Cambio en caliente, sin tocar la sesión.** `cambioLegado()` (`src/vkview.cpp:251-276`) guarda los
  números que había la trama (`ajusteActual`), aplica los **valores de fábrica** y, al apagar el modo,
  devuelve los que uno estaba usando. El vídeo, la pantalla, la lente y el ON/OFF no se mueven.
- **Valores de fábrica** (`src/ajuste.cpp:309-333`, `valoresFabrica()`): `fase 0`, `paso 1`,
  `filtro 10`, `crosstalk 0`, `conv 0`, rampas `0`, `fondo 1`, `frente 1`, empujes `0`, `planos 0`,
  `muestras 0`, `temblor 0,25`, `canto 0,004`, y encima lo que diga `serve.conf`. Al entrar en el modo
  se copian **todos menos `relieve` y `apoyo`**, que son los únicos dos números que la pastilla deja
  mover (`src/vkview.cpp:263-264`).
- **Puerta a los números finos.** `aplicarAjuste()` rechaza cualquier clave que no sea `relieve` o
  `apoyo` mientras el modo está puesto, y lo dice por consola con el valor actual
  (`src/vkview.cpp:202-210`). Las teclas de sintonía fina (`C`, `1`/`2`, y las de las casillas `g`/`v`)
  están negadas con aviso (`src/vkview.cpp:596-603`); la tecla del conversor (`C`) y el cambio de red
  (`1`/`2`) también (`src/vkview.cpp:545-546`, `582-585`).
- **Sin red ni conversor.** En el arranque del rastreo no se piden las pistas finas de la red
  (`--track-campo` / `--track-cara`) ni la mono forzada: `fo.fuerzaMono = 0` (las dos vistas de la
  cámara del monitor, siempre) y `fo.set.maxReprojectionPx = 40` (`src/vkview.cpp:1374-1380`).
- **La cámara manda sobre la imagen.** Si la cámara deja de ver la cara se suelta la trama y el vídeo
  se ve tal cual, y poco después se apaga la lente; al recuperar la cara vuelven
  (`src/vkview.cpp:2219-2222`, `2488-2510`): corte de trama a los **0,5 s** sin pose fresca, lente a
  los **0,75 s**. Con el modo quitado esto no ocurre (la trama aguanta, como siempre).
- **La pastilla, recortada** (`src/vkui.cpp`, rama `if (leg)`): sin botón de red, sin aviso de `Tab`
  y con los dos deslizadores reetiquetados como **«3D Salida»** y **«Plano de apoyo»**.

## 3. Números heredados de la referencia, y dónde se comprobaron

| cosa | referencia (cliente de Mac) | aquí |
|---|---|---|
| uniformes fijos del tramado «original» | `r = .0128535`, `m = .05`, `s = 10`, `z = 1` — `Sources/OdysseyGL/Renderer.cpp:68-71` | mismos constantes del shader de la trama, sin palancas encima |
| error de reproyección admitido al triangular la cara | `maximumError: 40` — `Sources/OdysseyCore/StereoTracking.swift:82` | `maxReprojectionPx = 40.0` en legado (`src/vkview.cpp:1380`); fuera del legado sigue en `10.0` (`src/track.h:16`) |
| profundidad mínima al triangular | `minimumDepth: 200` — misma línea | `minDepthMillimetres = 200` (`src/track.h:19`, ya igual) |
| dos vistas de la cámara para la triangulación | `eye0`/`eye1` con las 68 marcas — `StereoTracking.swift:71-90` | `fuerzaMono = 0` en legado |

**No verificado aquí:** las curvas exactas con las que aquel cliente convierte el número de la
interfaz en esos uniformes, y sus formatos de pantalla (16 modos frente a nuestros SBS/TAB/2D plano).
Quedan apuntados como pendiente en `CAMBIOS.md`.

## 4. Diferencia deliberada con la referencia

**Aquí sí se sigue mandando el comando del scaler del monitor** (el que pone el panel en modo 3D y
enciende la lente, `src/session.h:25-29`). En el cliente de Mac no hace falta porque usa otro camino
de pantalla; en Linux, sin ese comando el panel no entra en 3D y no se ve nada. Por eso el modo de
legado cambia *qué se tramamos*, pero no *cómo se despierta el monitor*.

## 5. La pastilla medida en los dos modos

Autoprueba sin pantalla: `./odysseyvk ctl hud --w 1440 --h 900 [--legado]`.

| | modo completo | modo de legado |
|---|---|---|
| pastilla dibujada | **824 × 134 px** | **648 × 134 px** |
| vértices de la interfaz | 2784 | **2580** (−204) |
| casillas de ratón del botón de red | 1458 | **0** (no existe) |
| deslizadores (primera casilla de ratón, en x de la ventana) | `3D Profundidad` x = 1116 · `3D Salida` x = 1266 (923 celdas cada uno) | `Plano de apoyo` x = 1110 · `3D Salida` x = 1260 (923 celdas cada uno) |
| aviso de `Tab` | sí | nota corta «Tab entrega la pantalla» |
| texto del hardware | «lente y scaler activos» | «lente activa» / «sin lente» (solo la lente) |

Letras medidas dentro de su hueco (ancho usado / hueco): `ON` 22,5 / 70 · `OFF` 28,8 / 70 ·
`Plano de apoyo` 96,9 / 120 · `3D Salida` 60,0 / 120 · `Ctrl+Shift+3 / 4` 92,2 / 120 ·
`Tab  entrega la pantalla` 140,0 / 272 · `lente activa` 66,0 / 220 · `sin lente` 48,7 / 220.
Caben todas. La nota del Facetracking gasta 306 px y deja 498 px (completo) / 322 px (legado).

## 6. Lo que el modo de legado **no** hace (pendientes declarados)

- **Ruta OpenGL (`--gl`)**: ahí el modo de legado **no está implementado**; la bandera se acepta pero
  no cambia nada. Solo la ruta Vulkan lo aplica.
- **No cierra ni abre la sesión**, no cambia la pantalla, la red ni el formato del vídeo.
- **Formatos estéreo del otro cliente** (los 16 modos de su `Docs/DirectStereo.md`) sin implementar.
- **Corrección que aplica a los dos modos**: `src/lens.cpp:203,209` manda `0x07` fijo donde iría el
  byte 14 del `SEChipID`; sigue pendiente.

## 7. Cómo se comprueba con la mano

```
odysseyvk ctl legado estado          # lo pedido en serve.conf + lo que dice el visor
odysseyvk ctl legado alternar        # cambia en caliente
tail -f ~/.local/state/odysseyvk/estado | grep legado
odysseyvk ctl hud --w 1440 --h 900 --legado | grep pastilla
```

En la ventana de ajustes, con el modo puesto, la pestaña **«Edición avanzada» aparece atenuada**
(deshabilitada) y el estado del grupo *Modo de la app* dice en qué modo está el visor abierto y avisa
si lo pedido y lo vivo no coinciden.
