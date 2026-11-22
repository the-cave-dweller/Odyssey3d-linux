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
* Paquetes: `cmake`, `gcc`, `pkgconf`, `glfw` (>= 3.4), `openssl`, `libpng`, `mpv` (para `play`).
  En CachyOS/Arch:

  ```sh
  sudo pacman -S --needed cmake gcc pkgconf glfw openssl libpng mpv
  ```

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

## Ajuste fino (teclas)

| tecla | efecto |
|-------|--------|
| `ESC` | salir (restaura panel, apaga lente y scaler) |
| `3` | pase 2D (sin tramar) / 3D |
| `F` | invertir profundidad (si se ve pseudoscópico: lo de dentro fuera) |
| `←` `→` | fase de la trama (±0.002 ciclos) |
| `RePag` `SavPg` | paso de lente (±0.0002); se usa si el paso real difiere del nominal |
| `↑` `↓` | crosstalk dinámico (±0.002) |
| `,` `.` | pendiente del filtro de traza (±0.5) |
| `9` `0` | convergencia horizontal (∓0.0005) |
| `8` | reiniciar el ajuste a los valores nominales |
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

## Limitaciones y diferencias con macdissey-3d

* **Wayland no permite capturar la ventana de otra aplicación**, así que no existe el modo
  "pantalla completa de cualquier reproductor → trama". El visionado pasa por el reproductor
  interno (`play`, basado en libmpv). Es la única diferencia funcional de bulto frente al
  original de macOS, y no depende de este port: en X11 sí se puede (usando `XComposite`).
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
src/viewer.*   ventana GLFW/Wayland,secuencia de arranque/apagado, teclas
src/main.*     subcomandos y banderas
tools/         verificación numérica de la trama contra el oráculo
calib/         mapas de corrección del panel (3DStackCorrection_A/B.png)
udev/          regla de permisos
```

## Licencia

Del mismo modo que el port X11 del que se tomaron los protocolos y las constantes (MIT), este
código se entrega bajo MIT. El trabajo original de ingeniería inversa del protocolo de la lente
y del oráculo de trama proviene de `Fulviuus/macdissey-3d` y de `m-j-tilley/Linux_3D_SBS_Player`.
