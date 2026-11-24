# odyssey3d — 3D sin gafas en el Samsung Odyssey 3D (G90XF) bajo Wayland

Port a Linux del visor/reproductor **macdissey-3d** (Swift/Metal para macOS), hecho desde
cero en C++17 para funcionar en **Wayland** (KDE Plasma / CachyOS) con el monitor
**Samsung Odyssey G90XF** (27" con lente lenticular conmutable y seguimiento de mirada).

Qué hace:

1. enciende la **lente lenticular** y pone el panel en el modo que necesita la trama;
2. activa el **modo 3D del scaler** del monitor;
3. **entrelaza por columnas** (weave) las dos vistas de un vídeo estéreo, de forma que el
   ojo izquierdo recibe una columna y el derecho la siguiente;
4. al salir, **apaga la lente, el scaler y devuelve el panel a su configuración anterior**.

El entrelazado usa el modelo analítico del oráculo de Dimenco (`FillAttributes`) capturado y
validado en el repositorio `Linux_3D_SBS_Player`, reimplementado en C++ y verificado contra él
(desviación < 2e-6 en los cuatro atributos de los esquinas, en la posición de ojo de trabajo y
desplazada). Las constantes del panel (`kx`, `ky`, rampa de fase por canal, paso de lente,
inclinación) son las nominales del G90XF, que son idénticas en todas las unidades.

## Requisitos

* Linux con **Wayland** (probado en KDE Plasma + NVIDIA RTX 4090, controlador 620.x).
* Paquetes: `cmake`, `gcc`, `pkgconf`, `glfw` (>= 3.4), `openssl`, `libpng`, **Vulkan**
  (`vulkan-icd-loader` + `vulkan-headers` + `glslang` para compilar los shaders) y `freetype2`
  para la interfaz. Para el servicio de ventanas y la captura: `wayland`,
  `plasma-wayland-protocols`, `libpipewire`, `systemd-libs`. `mpv` es opcional (sólo `play`).
  Para el conversor de vídeo normal a 3D hacen falta además `onnxruntime-cuda` (o `onnxruntime` a
  secas, que va por CPU y mucho más lento) y, en tiempo de ejecución, `cuda` y `cudnn`.
  En CachyOS/Arch:

  ```sh
  sudo pacman -S --needed cmake gcc pkgconf glfw openssl libpng freetype2 \
      vulkan-icd-loader vulkan-headers glslang \
      wayland plasma-wayland-protocols libpipewire systemd-libs mpv       onnxruntime-cuda cuda cudnn
  ```

* Una GPU con **Vulkan 1.3 o más** (probado en RTX 4090, RTX 5080 y AMD con RADV). Todo el
  dibujado —la trama 3D y la interfaz— se hace con Vulkan; la ruta OpenGL antigua sigue viva
  con `--gl` por si hace falta compararla.

* El monitor con su **cable USB-B** conectado (por ese cable van la lente, el scaler y la cámara).

## Compilar e instalar

```sh
./install.sh            # compila y pide sudo para instalar binario + regla udev
```

o a mano:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

El binario queda en `build/odyssey3d` (o `/usr/local/bin/odyssey3d` si se instala) y busca los
mapas de corrección del panel en `./calib`, junto al ejecutable o en `/usr/share/odyssey3d/calib`.

### Permisos (una sola vez)

La lente es un puerto serie (`/dev/ttyACM0`), el scaler es un dispositivo SCSI vendor
(`/dev/sgN`) y la cámara es un V4L2. La regla `udev/70-odyssey-3d.rules` (instalada por
`install.sh`) los abre con `TAG+="uaccess"`, así que no hace falta añadir el usuario a grupos.

> **Importante:** tras instalar la regla **no** se debe hacer
> `udevadm trigger` sobre el dispositivo `GCREADER` ni sobre `ttyACM0`: eso reinicializa el
> hub interno del monitor y **desconecta** lente, scaler y cámara hasta que se vuelva a
> conectar el cable USB o se apague/encienda el monitor. Para aplicar los permisos basta con
> `sudo udevadm control --reload-rules` y reconectar el cable del monitor.

Compruébalo con:

```sh
./build/odyssey3d doctor
```

Debe listar lente (`/dev/ttyACM0`), scaler (`/dev/sgN` con vendor `GCREADER`), cámara
(`04e8:20d7`) y la salida del panel con su modo 4K.

## Uso

```sh
odyssey3d doctor                       # diagnóstico de hardware, permisos y panel
odyssey3d test                         # patrón de calibración a pantalla completa
odyssey3d play pelicula_sbs.mkv        # reproduce un vídeo estéreo
odyssey3d ctl lens auth                # sólo autenticar la lente
odyssey3d ctl scaler off               # sólo apagar el modo 3D del scaler
odyssey3d ctl panel status             # ver modos del panel (kscreen-doctor)
odyssey3d ctl panel snapshot           # guardar el estado actual del panel en disco
odyssey3d ctl panel restore            # devolver el panel al estado guardado
odyssey3d serve                        # vigilar ventanas a pantalla completa (el servicio)
odyssey3d ctl overlay                    # enseña la tarjeta de pregunta en la esquina
odyssey3d ctl windows                    # lista las ventanas y detecta la pantalla completa
odyssey3d capture3d --uuid <id>        # trama en 3D la ventana que captura KWin
odyssey3d ctl flujo --uuid <id>        # medir si el compositor manda fotogramas de esa ventana
```

Al arrancar, `test` y `play` hacen todo solos: ponen el panel en `3840x2160@120`, escala 1 y
rotación normal, autentican y encienden la lente y activan el 3D del scaler. Al salir (ESC) lo
deshacen y restauran el modo, la escala y la rotación previos.

Antes de tocar el panel guardan el estado que había en `~/.local/state/odyssey3d/panel.state` y lo
borran al restaurarlo. Si el programa muere a mitad (señal, cuelgue, Kill), el fichero se queda ahí
y la siguiente arrancada avisa; se recupera con `odyssey3d ctl panel restore`. Añade `--dry` a
`apply` o `restore` para ver el comando `kscreen-doctor` que se ejecutaría sin tocar la pantalla.

### Formatos de vídeo

Se reproduce **cualquier cosa que reproduzca mpv** con la vista izquierda y la derecha en el
mismo fotograma:

* `side-by-side` (por defecto) o `--tb` para arriba/abajo.
* Con decodificación por GPU (`hwdec=auto`, NVDEC en NVIDIA). *Excepción conocida:* VP9 de
  10 bits con NVDEC produce cuelgues en algunos controladores; transcodifica a HEVC:

  ```sh
  ffmpeg -i entrada.webm -c:v hevc_nvenc -preset p5 -rc vbr -cq 21 -pix_fmt p010le -c:a copy salida.mkv
  ```

  Si ves artefactos o congelaciones, prueba `--hwdec no` (decodificación por CPU).

### Vídeo normal (2D) a 3D

Sirve para lo que no viene en side-by-side: un vídeo o una página normales. El visor estima la
profundidad de cada fotograma y desplaza cada ojo unas decenas de píxeles según esa profundidad, así
que lo cercano sale por delante de la pantalla y lo lejano se queda dentro. Es lo que Samsung llama
**«3D Re-Mastering»** en su programa de Windows; aquí se hace en la máquina, con dos diferencias
honestas: la profundidad la deduce una red neuronal a partir de una sola imagen (no hay dos cámaras),
así que se equivoca en reflejos, vallas y pelo suelto, y hay que calcularla, así que el relieve
aparece una fracción de segundo después que el vídeo.

```sh
odyssey3d capture3d --uuid '<id>' --conver            # video normal, con la red en la GPU 0
odyssey3d capture3d --uuid '<id>' --conver --red 1    # la red en la GPU 1 (si la 0 esta llena)
```

* `--conver` (o `--convert`) arranca ya con el conversor puesto; sin esa opción se enciende con la
  tecla `C` o con la cuarta tarjeta («Vídeo normal · 2D a 3D»).
* `--modelo RUTA` cambia la red. La por defecto es *Depth Anything V2 Small* en ONNX,
  `~/.local/share/odyssey3d/modelos/depth_anything_v2_small.onnx` (99 MB). Si falta, el visor lo dice
  y el resto del 3D sigue funcionando sin conversión; el registro trae el `curl` para bajarla:

  ```sh
  mkdir -p ~/.local/share/odyssey3d/modelos && curl -fLo ~/.local/share/odyssey3d/modelos/depth_anything_v2_small.onnx \
      https://huggingface.co/onnx-community/depth-anything-v2-small/resolve/main/model.onnx
  ```
* `--cada N` calcula la profundidad cada `N` fotogramas (por defecto 3). Con cámara en movimiento
  conviene 2; si la GPU va justa, 4 o 5.
* `--red N` pone la red en la GPU `N` (`0` por defecto). El programa mira cuánta memoria libre tiene
  cada tarjeta: si en la pedida no caben ~1,1 GB la manda a la que más tenga y lo avisa. Si en ninguna
  cabe, la red **se va a la CPU** (tarda 150 ms en vez de 9) y entonces la profundidad se recalcula
  cada 10 cuadros en lugar de cada 3; `--cada N` lo cambia. La estimación corre en su propio hilo, así
  que ni siquiera en CPU se corta el vídeo.

Se ve en el registro: `[conversor] video normal a 3D encendido con NVIDIA`; una estimación tarda
~9 ms en una RTX 4090. La fuerza del relieve la siguen moviendo **3D Salida** (`Ctrl+Shift+5/6`) y
la profundidad base **3D Profundidad** (`Ctrl+Shift+3/4`), igual que con vídeo estéreo.

## Ajuste fino (teclas)

| tecla | efecto |
|-------|--------|
| `ESC` | salir (restaura panel, apaga lente y scaler) |
| `Tab` | **entregar la pantalla**: apaga el 3D del panel y minimiza esta ventana, así que el ratón y el teclado vuelven a lo de detrás. Devolverle el foco (`Meta+Alt+Tab`, que sí lista las minimizadas, o un clic en la barra de tareas) vuelve a poner lente, scaler y modo 3D |
| `P` | pausar / reanudar el vídeo del navegador (por MPRIS, sin quitar la capa) |
| `3` | pase 2D (sin tramar) / 3D |
| `F` | invertir profundidad (si se ve pseudoscópico: lo de dentro fuera) |
| `M` | cambiar el formato del vídeo: lado a lado → arriba y abajo → 2D plano → vídeo normal (2D a 3D) |
| `C` | encender / apagar el conversor de **vídeo normal a 3D** (lo mismo que la cuarta tarjeta) |
| `←` `→` | fase de la trama (±0.002 ciclos) |
| `RePag` `SavPg` | paso de lente (±0.0002); se usa si el paso real difiere del nominal |
| `↑` `↓` | crosstalk dinámico (±0.002) |
| `,` `.` | pendiente del filtro de traza (±0.5) |
| `9` `0` | convergencia horizontal (∓0.0005) |
| `8` | reiniciar el ajuste a los valores nominales |
| `Ctrl+Shift+1` | 3D puesto / apagado (igual que el Odyssey 3D Hub oficial) |
| `Ctrl+Shift+2` | ocultar / mostrar la pastilla de la interfaz (no libera el ratón: para eso es `Tab`) |
| `Ctrl+Shift+3` `4` | bajan / suben **3D Profundidad** (0–100) |
| `Ctrl+Shift+5` `6` | bajan / suben **3D Salida** (−50…+50) |
| `Ctrl+Shift+7` `8` | achican / agrandan la pastilla (0,5× … 3×, útil al pasar del escritorio al modo 3D) |
| `--ui-escala X` | arrancar la pastilla ya con ese tamaño (`1` pequeña, `2` grande; por defecto 2 en 4K) |
| `espacio`, `-`, `=`, `[`, `]` | pausa, ±5 s, cuadro a cuadro (en `play`) |

Cada cambio imprime una línea con los valores actuales; puedes arrancar desde ellos con
`--align`, `--pitch` y `--eye-x/--eye-y/--eye-z`.

### Cómo alinear

1. `odyssey3d test` y colócate a la distancia normal de trabajo (la altura de los ojos sobre el
   centro del panel es `--eye-y` ≈ 8.4 cm, distancia `--eye-z` ≈ 65 cm).
2. Mira la fila de nueve cuadros con disparidad del patrón: deben verse **flotando a distinta
   profundidad, sin duplicados ni bordes dobles**.
3. Si se ve una banda que "baila" o se desdobla: `←`/`→` hasta que las columnas encajen.
4. Si el efecto sólo es bueno en el centro de la pantalla y se rompe hacia los lados: ajusta el
   paso con `RePag`/`SavPg`.
5. Si la profundidad está invertida (lo cercano se ve lejos): pulsa `F`.
6. Para comprobar que la lente y el scaler están bien: `3` (2D) debe verse normal y plano, `3`
   otra vez debe dar volumen.

## Cuál es el indicador de la esquina

En modo 3D se dibuja un pequeño cuadrado arriba a la izquierda: **verde** = lente y scaler
activos; **naranja** = sólo uno de los dos; **rojo** = ninguno (no hay 3D).

## La interfaz (pastilla con ratón)

Desde v0.5 el visor dibuja una superposición al estilo del **Odyssey 3D Hub** oficial, con cursor
de flecha visible y controlables con el ratón:

* pastilla gris translúcida arriba a la derecha, con la línea verde de acento:
  * **Apagar 3D / Encender 3D** (`Ctrl+Shift+1`),
  * **Ocultar capa / Mostrar capa** (`Ctrl+Shift+2`),
  * **3D Profundidad** 0…100 (`Ctrl+Shift+3` / `4`) — desplaza la separación de las dos vistas;
  * **3D Salida** −50…+50 (`Ctrl+Shift+5` / `6`) — acerca o aleja la imagen de la pantalla;
  * un punto de estado: verde = lente y scaler activos, naranja = uno de los dos, rojo = ninguno.
* aviso abajo a la derecha, casi negro, con tres teselas (**Lado a lado**, **Arriba y abajo**,
  **2D plano**) y la casilla «no volver a preguntar», que es lo que manda el servicio cuando
  detecta un vídeo a pantalla completa.

La pastilla se calcula en un lienzo virtual de 1080 px de alto y se escala con la altura del panel,
así que conserva las proporciones de la aplicación oficial a 3840x2160. Se apaga con
`--sin-interfaz` (o `Ctrl+Shift+2`). Las teclas finas de alineado (`3`, `F`, flechas, `RePag`…)
siguen funcionando igual.

### El ratón atraviesa el visor (desde v0.22)

La ventana del visor cubre la pantalla entera, pero **no se queda con el ratón**: declara como región de
entrada la pastilla (y el aviso, si está puesto), así que los clics y la rueda que caen fuera de esas cajas
atraviesan el visor y llegan a la ventana de atrás. Se puede dar a pausa en el vídeo, mover la barra de
avance o manejar el navegador sin quitar el 3D. La tarjeta de aviso del servicio hace lo mismo: sólo su
recuadro es pinchable y el resto del lienzo deja pasar el ratón.

Dos detalles:

* mientras se arrastra un deslizador el visor sí se queda el ratón entero, para que el arrastre no se plante
  a media caja;
* el teclado se queda con la ventana activa. Un clic en el vídeo se lleva el foco, y desde ahí las teclas
  finas (`3`, `F`, flechas, `9`/`0`…) mandan sobre el navegador hasta que se pica la pastilla, que devuelve
  el mando al visor. `Tab` y `ESC` siguen siendo la salida limpia de la sesión.

Con `--sin-pasar-raton` la ventana vuelve a quedarse todos los clics, que era el comportamiento hasta v0.21.
Sobre X11 no hay regiones de entrada: el visor se queda los clics y lo avisa
(`[raton] el compositor no dio wl_compositor: el visor seguira tragando los clics`).

## Servicio que arranca con la sesión

`install.sh` activa una unidad de **systemd de usuario** que se levanta con la sesión gráfica y se
queda esperando (sin ventana, sin consumir GPU):

```sh
systemctl --user status odyssey3d-serve      # debe decir "active (running)"
journalctl --user -u odyssey3d-serve -f      # ver lo que hace en vivo
systemctl --user stop odyssey3d-serve        # pausarlo
systemctl --user edit odyssey3d-serve        # cambiarlo si hace falta
```

Qué hace: escucha `org_kde_plasma_window_management` de KWin; cuando una ventana pasa a **pantalla
completa** dibuja en la esquina una **tarjeta propia** (la misma interfaz del visor) con cuatro botones.
Va en una capa `overlay` de `zwlr_layer_shell_v1`, así que **se ve por encima del vídeo a pantalla
completa**, cosa que las notificaciones de KDE no logran (KWin las dibuja detrás, y con «No molestar»
ni siquiera aparecen). Si el compositor no expone ese protocolo se vuelve al aviso de KDE. Al pulsar un
botón lanza
`odyssey3d capture3d --uuid …`, que pone lente + scaler + modo del panel y trama en 3D el
fotograma que captura el propio compositor (**DMA-BUF** de KWin → imagen Vulkan, sin copia por
CPU). El botón **2D plano no lanza nada**: deja el vídeo tal cual (sin lente, sin scaler y sin cambiar
el modo del panel) y, si había una sesión 3D abierta, la cierra devolviendo el panel a su modo normal.
Cuando la ventana del vídeo se cierra, la sesión 3D también se cierra sola.

Se configura en `~/.config/odyssey3d/serve.conf`: `ignore` (aplicaciones que nunca se ofrecen),
`espera` (segundos que queda la tarjeta), `modo` (`ask`, `sbs`, `tb`, `flat`, `conv`), `red` (en qué
GPU corre la red del conversor) y `cada` (cada cuántos fotogramas se calcula la profundidad). Las dos
redes de profundidad se eligen con `modelo` (botón **1**, la de Samsung: cualquier `.onnx` llamada
`mde_*` o `disp_*`) y `nuestra` (botón **2**, la nuestra: `depth_anything_v2_small.onnx`); en el visor
se cambia de una a otra con el botón de la pastilla o con las teclas sueltas `1` y `2` sin cerrar nada,
y la elección queda en `~/.config/odyssey3d/red-elegida` para la próxima vez. Si se
marca la casilla de la tarjeta y se pulsa un botón, se guarda una línea `auto = app:modo` en ese
archivo y desde entonces esa aplicación pasa directa sin preguntar (puedes editar o borrar esa línea a
mano). `conv` es el vídeo normal pasado a 3D: con `auto = zen:conv` el navegador entra directo en
conversión. Si no aparece la
tarjeta:

1. `systemctl --user is-active odyssey3d-serve` → tiene que estar `active`;
2. la ventana tiene que ser **pantalla completa de verdad** (no maximizada) y su `app_id` no debe
   estar en `ignore` (los reproductores bajo XWayland aparecen como `xwayland` y están ignorados
   por defecto: quítalos de la lista si quieres usarlos);
3. en `journalctl` debe aparecer `[serve] tarjeta puesta por <app_id> | <título>` y luego
   `[serve] tarjeta -> <modo>`; para probar la tarjeta sin ningún vídeo:
   `odyssey3d ctl overlay --segundos 8` (acepta `--dump CARDEN.png` para volcarla a un archivo).

Si durante una sesión **la imagen se queda congelada**, el visor lo dice en el registro y eso permite
saber de quién es la culpa:

* `[captura] N s sin fotogramas nuevos del compositor (flujo: streaming)` + la ventana en buen estado
  → el navegador/reproductor dejó de pintar (suele pasar con vídeos en línea: vuelva a darle play);
* `(flujo: paused …)` o `el compositor cerró el flujo` → se cortó la captura; el visor reintenta el
  flujo solo hasta cuatro veces;
* `la ventana del video cerro` → la sesión se cierra y el panel vuelve a 2D.

Para mirar el flujo sin abrir nada encima: `odyssey3d ctl flujo --uuid <id> --segundos 10` imprime
cuántos fotogramas manda el compositor cada segundo (un vídeo normal da 24-25).

> KWin 6.8 sólo expone el portal propio de KDE (`zkde_screencast_unstable_v1`, versión 6). Es lo
> que usa el port; no hay Wlr-Fragment-Capture ni ext-image-copy-capture en este compositor.

## Limitaciones y diferencias con macdissey-3d

* Capturar **otra aplicación** sólo es posible por el portal de captura de KDE
  (`zkde_screencast_unstable_v1` + PipeWire), que es inestable entre versiones de Plasma: si
  KWin cambia el interfaz, `serve` deja de ver los fotogramas (lo avisa
  `journalctl --user -u odyssey3d-serve`). El reproductor interno (`play`, basado en libmpv) sigue
  siendo la ruta más directa; mpv por ahora se dibuja con OpenGL aunque su fotograma se trama en
  Vulkan.
* El **seguimiento de mirada no está activo** todavía. La trama se calcula para la posición de
  ojo nominal (los valores de fábrica del G90XF), igual que el modo "static" del port X11, así
  que el punto dulce es la posición de trabajo normal y se degrada al moverse mucho. Mover la
  cabeza en horizontal da un cambio de fase que puedes seguir con `←`/`→`.
  Está implementado el control de la cámara IR (encendido/apagado del illuminador, `ctl ir`) y
  las intrínsecas/extrínsecas están en `calib/`; el rastreador es la fase siguiente.
* El panel debe quedar a **escala 1 y rotación normal** durante el 3D (la trama necesita pixel a
  pixel); el programa lo cambia y lo devuelve. Si tu escritorio usa escala 1.5, al salir
  volverá a 1.5.
* `kscreen-doctor` de Plasma 6 no expone VRR: si lo tienes activado, desactívalo en
  Ajustes → Pantalla antes del 3D (el entrelazado con tasa variable produce bandas).
* La autenticación de la lente usa la clave fija del firmware y **persiste hasta apagar el
  monitor**, así que `ctl lens auth` basta una vez por sesión de encendido.

## Diagnóstico

```sh
odyssey3d doctor                      # qué ve, qué toca, qué falta
odyssey3d weave [--solid] [--out f.png]  # escribe un PNG con la trama (no toca el panel)
odyssey3d attrs                       # atributos de la trama (para depurar el modelo)
odyssey3d ctl hud --w 1920 --h 1080   # prueba la interfaz sin abrir ventana ( atlas, raton )
odyssey3d ctl vkprobe                 # importa el fotograma de KWin y lo dibuja tal cual en Vulkan
odyssey3d play clip.mp4 --windowed --no-panel --no-lens --no-scaler --dump chk
                                      # guarda chk-trama.png y chk-origen.png y sale
```

`--dump` es útil para ver el entrelazado sin el hardware: `chk-trama.png` es literalmente lo que
se manda al panel; visto de cerca deben apreciarse las franjas diagonales (paso 1.8 px con la
inclinación nominal de 0.29).

### El monitor se queda vertical al cerrar el vídeo

Causa: **kscreen-doctor numera la rotación con la enum de KScreen, no con la transformación de
Wayland**. El puerto leía ese número y al restaurar escribía la palabra equivocada. La tabla real,
comprobada en esta máquina cambiando la rotación y releyendo:

| `Rotation:` en `kscreen-doctor -o` | orientación | palabra para `kscreen-doctor` |
|---|---|---|
| 1 | horizontal (normal) | `rotation.none` |
| 2 | girada a la izquierda | `rotation.left` |
| 4 | boca abajo | `rotation.inverted` |
| 8 | girada a la derecha | `rotation.right` |

En Wayland `1` significa 90°, que es lo que el puerto asumía: guardaba tu estado horizontal como
`1` y al salir mandaba `rotation.left`, dejando el panel en vertical. Corregido en `src/panel.*`
(`RotNone = 1`, `RotLeft = 2`, `RotInverted = 4`, `RotRight = 8`; un valor no reconocido ya no
toca la rotación) y cubierto con `tools/prueba_rotacion.py`.

Si se te ha quedado vertical y no quieres andar por los ajustes de KDE:

```sh
kscreen-doctor output.DP-4.rotation.none     # sustituye DP-4 por el nombre de tu salida
odyssey3d ctl panel status                   # debe decir "rotacion: 1 (none)"
```

### Se congela el vídeo del navegador mientras el visor está encima

Síntoma: con un vídeo de YouTube abierto, al elegir un modo 3D la imagen se queda parada hasta cerrar la
capa. El visor lo avisa:

```
[captura] 2 s sin fotogramas nuevos del compositor (flujo: streaming)
[captura] ventana capturada: zen · pantalla completa · 3840x2160
[captura] el navegador dejo de pintar porque el compositor ya no le manda el sincronismo (la tapamos por completo).
```

Qué significa: KWin sólo manda el aviso de sincronismo a las ventanas que pintan. Cuando el navegador deja
de recibirlo —porque la ventana quedó tapada por nuestra capa a pantalla completa— su compositor interno
deja de producir fotogramas, y sin fotogramas nuevos no hay nada que entrelazar. **No es un problema del
visor**: con mpv, VLC o reproductores parecidos el flujo no se corta nunca, y con vídeos locales en el
navegador (1080p y 4K60 probados) tampoco.

Se comprueba en dos segundos, sin abrir la capa:

```sh
odyssey3d ctl windows                                  # localiza la ventana del vídeo
odyssey3d ctl flujo --uuid '{...}' --segundos 5        # fotogramas por segundo que manda KWin
```

Si marca 0 fotogramas/s con el vídeo sonando, el que dejó de pintar es el navegador. Tiene arreglo del lado
del navegador:

- **Firefox o Zen**: en `about:config`, poner `gfx.vsync.compositor.unobserve-count` en `0` y reiniciar el
  navegador. Medido con un vídeo de YouTube en bucle, el panel en modo 3D (3840x2160 a 119,88 Hz) y nuestra
  capa encima: **30,9 f/s** con la preferencia, frente a 0,1 f/s sin ella. Zen guarda el cambio solo si se
  pone en `about:config`; el proyecto deja además una nota en el perfil (`user.js`) para que no se pierda.
- **No uses `widget.wayland.vsync.enabled = false`**: probado en esta máquina, empeora las cosas (con el
  panel en modo 3D el propio navegador bajó de 0,4 f/s, con capa o sin ella). El valor por defecto, `true`,
  es el que hay que dejar.
- **Chrome o Chromium**: arrancarlos con `--disable-gpu-vsync` (sin probar en esta máquina).
- **mpv, VLC, haruna, toc**: no necesitan tocar nada. Recomendado para ver en 3D:
  `odyssey3d play archivo.mp4` usa mpv y el flujo se mantiene (60-63 f/s con el hardware en marcha).

Lo segundo que hace el navegador es **pausar el vídeo él solo** al cambiar el modo del monitor o al quedar
tapado: el flujo de KWin sigue llegando (se midieron 51 f/s) pero la imagen no avanza. Para no depender de
acertarle al botón de pausa del reproductor con el visor encima, el visor lo hace por MPRIS (Zen, Firefox y
Chrome publican un reproductor en la sesión):

- al arrancar la sesión, si el vídeo estaba parado lo reanuda solo (`[video] el navegador habia parado el
  video (Paused): se reanuda solo`);
- **`P` o espacio** pausan y reanudan el vídeo sin quitar la capa (hace falta que el visor tenga el foco:
  un clic en la pastilla se lo devuelve);
- desde una terminal, en cualquier momento: `odyssey3d ctl video status|pause|play|toggle`.

Si hace falta usar el ratón sobre el reproductor (cambiar la calidad, buscar en la línea de tiempo), desde
v0.22 basta con pinchar **fuera de la pastilla**: el clic atraviesa el visor y lo recibe la ventana de atrás,
que además se queda el teclado. La salida limpia sigue siendo `Tab` o `ESC`: cierra la sesión, devuelve el
panel a 2D y vuelve a aparecer la tarjeta del hub.

El visor hace lo que puede por su parte: si el flujo se corta lo vuelve a pedir una y otra vez mientras
dure la sesión, no vuelve a gastar GPU presentando el mismo cuadro (`--siempre` recupera el ritmo de panel
de 120 presentaciones por segundo), y si cerraste la ventana del vídeo se sale solo y devuelve el panel a 2D.

## Verificación sin hardware

`tools/verifica_weave.py` comprueba pixel a pixel el entrelazado del C++ contra el oráculo
Python de `Linux_3D_SBS_Player` (en la posición de ojo de fábrica, recorte de 360x16 px a
3840x2160):

```
$ python3 tools/verifica_weave.py
recorte 360x16 px   mayor desviación = 1.28 LSB   media = 0.026 LSB        OK
$ python3 tools/verifica_weave.py --corr
recorte 360x16 px   mayor desviación = 23.86 LSB  media = 0.448 LSB        OK
```

Con los mapas de corrección neutros la diferencia es de ~1 LSB (redondeo float32 del filtro).
Con los mapas reales sube a ~24 LSB en pocos píxeles porque el filtrado lineal de texturas de
8 bits lo hace la GPU en punto fijo (~1/256 de ciclo de fase); es el mismo camino que siguen el
Hub de Dimenco y el port X11, así que se deja tal cual.

`tools/prueba_rotacion.py` comprueba la tabla de rotaciones de `kscreen-doctor` (la que dejaba el
monitor en vertical al salir). No cambia nada: escribe una instantánea falsa y mira el comando que
`ctl panel restore --dry` generaría.

`odyssey3d ctl hud --w 1920 --h 1080` comprueba la interfaz sin abrir ventana: construye el atlas
de fuentes, cuenta los vértices y barre la pantalla entera de 2 px para comprobar que el ratón
golpea el botón, el deslizador o la tesela que toca, y luego mueve los deslizadores y los botones.

Trama Vulkan contra trama OpenGL, mismo patrón y mismos parámetros (`--cpu-patron` para que la
fuente sea idéntica en las dos rutas):

```sh
./build/odyssey3d weave --w 1350 --h 900 --cpu-patron --out /tmp/glc      # OpenGL
./build/odyssey3d test --windowed --no-panel --no-lens --no-scaler \
    --w 900 --h 600 --sin-interfaz --dump /tmp/vkn                        # Vulkan (leido de GPU)
python3 -c "d=[[abs(a-b) for a,b in zip(x,y)] for x,y in zip(__import__('PIL.Image',fromlist=['x']).open('/tmp/glc').convert('RGB').load(),__import__('PIL.Image',fromlist=['x']).open('/tmp/vkn-trama-vulkan.png').convert('RGB').load())]"
```

Las dos tramas coinciden (diferencia media ~10/255, concentrada en los mapas de corrección).

También se puede mirar la trama a mano: `odyssey3d weave --solid` produce una imagen 3840x2160
en la que la mitad izquierda del archivo es roja y la derecha verde; midiendo con
autocorrelación sale un paso de **1.806 px** y una inclinación de **0.29 px por fila**, que son
los valores nominales del G90XF.

## Estructura

```
src/lens.*     serie CDC-ACM, protocolo ASCII SR+*, autenticación HMAC-SHA3-256
src/sha3.*     SHA3/HMAC-SHA3 vía OpenSSL libcrypto
src/scaler.*   SCSI vendor por ioctl SG_IO (CDB 0xE9 0x61 …) sobre /dev/sgN
src/ircam.*    UVC extension unit: illuminador IR (unit 3, selector 2)
src/panel.*    modo/escala/rotación del panel vía kscreen-doctor (guardado y restaurado)
src/weave.*    shader de entrelazado + modelo analítico del oráculo Dimenco
src/testsrc.*  patrón SBS de calibración (disparidades, marcas, barras RGB)
src/mpvsrc.*   reproducción con la render API de libmpv
src/session.*  secuencia de arranque/apagado del panel+lente+scaler, teclas comunes, patron SBS
src/viewer.*   ventana GLFW/Wayland y dibujado por **OpenGL** (`--gl`, ruta de reserva)
src/vkview.*   ventana GLFW/Wayland y dibujado por **Vulkan** (la ruta normal)
src/vkdev.*    instancia, dispositivo, DMA-BUF, subidas, barreras, charco de descriptores
src/vkweave.*  cadena de trazado Vulkan: pases weave + interfaz con *dynamic rendering*
src/vkui.*     superposicion de la interfaz (atlas de fuentes FreeType + vertices)
shaders/       GLSL 450 de la trama y de la interfaz (se compilan a SPIR-V al construir)
src/wm.*       vigilancia de ventanas a pantalla completa (plasma-window-management)
src/capture.*  fotogramas de KWin por zkde-screencast + PipeWire (DMA-BUF)
src/notify.*   avisos de KDE por el portal de notificaciones (sd-bus)
src/vkprobe.*  diagnóstico: importar un DMA-BUF y dibujarlo en Vulkan
systemd/       unidad `odyssey3d-serve.service` (arranca con la sesion)
config/        `serve.conf`, plantilla de ~/.config/odyssey3d/serve.conf
src/main.*     subcomandos y banderas
tools/         verificación numérica de la trama contra el oráculo
calib/         mapas de corrección del panel (3DStackCorrection_A/B.png)
udev/          regla de permisos
```

## Licencia

Del mismo modo que el port X11 del que se tomaron los protocolos y las constantes (MIT), este
código se entrega bajo MIT. El trabajo original de ingeniería inversa del protocolo de la lente
y del oráculo de trama proviene de `Fulviuus/macdissey-3d` y de `m-j-tilley/Linux_3D_SBS_Player`.
