# odysseyvk — 3D sin gafas en el Samsung Odyssey 3D (G90XF) bajo Wayland

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
inclinación) son las capturadas de **otro** G90XF. Lo propio de cada unidad lo guarda el monitor en
`3dstack.eini` (va dentro de lo que contesta a `SR+CAL3D`, un 7z cifrado, junto a los dos mapas de
corrección), y ese archivo no se puede leer; por eso el paso de esta unidad se afina contra la
lente con `rampa_x` y `rampa_y` (ver «Calibrar el paso contra la lente»).

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

El binario queda en `build/odysseyvk` (o `/usr/local/bin/odysseyvk` si se instala) y busca los
mapas de corrección del panel en `./calib`, junto al ejecutable o en `/usr/share/odysseyvk/calib`.

**Desde v0.57 el proyecto se llama OdysseyVK** (antes `odyssey3d`): el repo es
`~/Documentos/LU agent/odysseyvk-linux`, los binarios `odysseyvk` y `odysseyvk-ajustes`, la unidad
`odysseyvk-serve.service`, la regla udev `70-odysseyvk.rules` y los datos
`~/.config/odysseyvk`, `~/.local/state/odysseyvk` y `~/.local/share/odysseyvk`. `install.sh` además de
compilar e instalar mete el **icono** (`assets/hicolor/…/apps/odysseyvk.png`, ocho tamaños de 16 a 512)
y **dos lanzadores** en `~/.local/share/applications`: `odysseyvk.desktop`, que al pulsarlo abre la
ventana de ajustes (`Exec=odysseyvk-ajustes`), y `odysseyvk-play.desktop` (`NoDisplay=true`, no sale en
el menú), que es el que abre un vídeo con `odysseyvk play %f`. El lanzador del menú ya no puede ser
`odysseyvk play %F`: sin archivo detrás el `%F` se queda vacío y el visor contesta «falta el archivo» y
sale con código 2, así que el icono del menú no abría nada. `install.sh` también **limpia lo del nombre
viejo**: apaga y borra `odyssey3d-serve.service`, quita `odyssey3d.desktop` y la regla
`/etc/udev/rules.d/70-odyssey-3d.rules`, y mueve los binarios viejos a
`/usr/local/bin/viejo-odyssey3d/` en vez de borrarlos.

La migración de datos la hace el propio programa al arrancar: copia (no mueve) lo que haya en las
carpetas viejas a las nuevas, sin pisar lo que ya exista. Por eso, mientras no se borre lo viejo, los
modelos están duplicados: aquí son **237 MB en `~/.local/share/odyssey3d/modelos` y los mismos
237 MB en `~/.local/share/odysseyvk/modelos`**. Comprobado el 2026-11-25, con el 3D y el rastreo
funcionando ya con el nombre nuevo, las tres carpetas viejas (`~/.local/share/odyssey3d`,
`~/.config/odyssey3d` y `~/.local/state/odyssey3d`) se compararon fichero por fichero con las nuevas
—los mismos tamaños— y se movieron a `../papelera-odyssey3d-2026-11-25/`, que se puede borrar cuando se
quiera. Los nombres de entorno antiguos (`ODYSSEY3D_*`) siguen valiendo: la reserva está en
`entornoOdyssey()` (`src/util.cpp`), probada también con la variable nueva.

### Permisos (una sola vez)

La lente es un puerto serie (`/dev/ttyACM0`), el scaler es un dispositivo SCSI vendor
(`/dev/sgN`) y la cámara es un V4L2. La regla `udev/70-odysseyvk.rules` (instalada por
`install.sh`) los abre con `TAG+="uaccess"`, así que no hace falta añadir el usuario a grupos.

> **Importante:** tras instalar la regla **no** se debe hacer
> `udevadm trigger` sobre el dispositivo `GCREADER` ni sobre `ttyACM0`: eso reinicializa el
> hub interno del monitor y **desconecta** lente, scaler y cámara hasta que se vuelva a
> conectar el cable USB o se apague/encienda el monitor. Para aplicar los permisos basta con
> `sudo udevadm control --reload-rules` y reconectar el cable del monitor.

Compruébalo con:

```sh
./build/odysseyvk doctor
```

Debe listar lente (`/dev/ttyACM0`), scaler (`/dev/sgN` con vendor `GCREADER`), cámara
(`04e8:20d7`) y la salida del panel con su modo 4K.

## Uso

```sh
odysseyvk doctor                       # diagnóstico de hardware, permisos y panel
odysseyvk test                         # patrón de calibración a pantalla completa
odysseyvk play pelicula_sbs.mkv        # reproduce un vídeo estéreo
odysseyvk ctl lens auth                # sólo autenticar la lente
odysseyvk ctl scaler off               # sólo apagar el modo 3D del scaler
odysseyvk ctl panel status             # ver modos del panel (kscreen-doctor)
odysseyvk ctl panel snapshot           # guardar el estado actual del panel en disco
odysseyvk ctl panel restore            # devolver el panel al estado guardado
odysseyvk serve                        # vigilar ventanas a pantalla completa (el servicio)
odysseyvk ctl overlay                    # enseña la tarjeta de pregunta en la esquina
odysseyvk ctl windows                    # lista las ventanas y detecta la pantalla completa
odysseyvk ctl salidas                    # cada pantalla con su posición y tamaño lógicos
odysseyvk ctl apilamiento                # el orden arriba/abajo que ve el compositor
odysseyvk ctl ventana sobre --uuid <id>  # encima de todo / debajo / libre / salida DP-4
odysseyvk capture3d --uuid <id>        # trama en 3D la ventana que captura KWin
odysseyvk ctl flujo --uuid <id>        # medir si el compositor manda fotogramas de esa ventana
```

Al arrancar, `test` y `play` hacen todo solos: ponen el panel en `3840x2160@120`, escala 1 y
rotación normal, autentican y encienden la lente y activan el 3D del scaler. Al salir (ESC) lo
deshacen y restauran el modo, la escala y la rotación previos.

Antes de tocar el panel guardan el estado que había en `~/.local/state/odysseyvk/panel.state` y lo
borran al restaurarlo. Si el programa muere a mitad (señal, cuelgue, Kill), el fichero se queda ahí
y la siguiente arrancada avisa; se recupera con `odysseyvk ctl panel restore`. Añade `--dry` a
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
odysseyvk capture3d --uuid '<id>' --conver            # video normal, con la red en la GPU 0
odysseyvk capture3d --uuid '<id>' --conver --red 1    # la red en la GPU 1 (si la 0 esta llena)
```

* `--conver` (o `--convert`) arranca ya con el conversor puesto; sin esa opción se enciende con la
  tecla `C` o con la tesela «Vídeo normal» de la tarjeta.
* `--modelo RUTA` cambia la red. La de fábrica es la abierta, *Depth Anything V2 Small* en ONNX
  (95 MB), en `~/.local/share/odysseyvk/modelos/depth_anything_v2_small.onnx`. Hace falta
  descargarla una vez: la primera vez que el servicio arranca sin ella abre el panel de ajustes,
  que dice de dónde se baja y trae el botón **Descargar**, que la deja en esa carpeta. Desde la
  terminal es lo mismo:

  ```sh
  odysseyvk ctl red descargar
  ```

  Se baja de <https://huggingface.co/XnetShadow/depth_anything_v2_small>. Sin ninguna red el
  resto del 3D sigue funcionando; solo falta el vídeo normal a 3D. La red del Odyssey 3D Hub de
  Windows (`C:\Program Files\Odyssey 3D Hub\Conversion\model\mde_large.fp16.onnx.bin`, que allí va
  ofuscada) se puede elegir en la pestaña Rendimiento del panel con «La de Samsung…», que abre el
  navegador de archivos y espera un `.onnx` legible; da el mismo canto que la abierta (medido) y
  tarda 9–11 ms por mapa en vez de 13–16. Al volver a «Abierta» esa ruta se queda en `samsung =`, para que el
  visor la siga teniendo en la tecla 1.
* `--cada N` calcula la profundidad cada `N` fotogramas (por defecto 3). Con cámara en movimiento
  conviene 2; si la GPU va justa, 4 o 5.
* `--texto-panel si|no` pega o suelta los rótulos del plano de la pantalla (por defecto pegados;
  en marcha es la tecla `R`, y `ODYSSEYVK_TEXTO=0` lo apaga para comparar sin recompilar).
* `--red N` pone la red en la GPU `N` (`0` por defecto). El programa mira cuánta memoria libre tiene
  cada tarjeta: si en la pedida no caben ~1,1 GB la manda a la que más tenga y lo avisa. Si en ninguna
  cabe, la red **se va a la CPU** (tarda 150 ms en vez de 9) y entonces la profundidad se recalcula
  cada 10 cuadros en lugar de cada 3; `--cada N` lo cambia. La estimación corre en su propio hilo, así
  que ni siquiera en CPU se corta el vídeo.

Se ve en el registro: `[conversor] video normal a 3D encendido con NVIDIA`; una estimación tarda
~9 ms en una RTX 4090. La fuerza del relieve la siguen moviendo **3D Salida** (`Ctrl+Shift+5/6`) y
la profundidad base **3D Profundidad** (`Ctrl+Shift+3/4`), igual que con vídeo estéreo.

### Cantos finos: el borde de cada objeto en su sitio (desde v0.89)

El mapa de profundidad mide 518 × 518: un punto cada 7 píxeles, y en el borde de un objeto trae una
rampa de dos o tres puntos. Hasta la v0.88 ese mapa se ensanchaba 3 puntos alrededor de lo cercano
(para que el objeto no dejara atrás su orilla) y se repartía por ojo en la CPU; a cambio, cada
objeto se llevaba pegada una franja de fondo de unos 4 mm, y donde acababa esa franja el fondo se
veía doble: un «fantasma» de 1 a 1,5 mm junto a los planos, más visible al mover la cabeza.

Ahora, antes de la trama, un pase previo recorre la imagen y cambia esa rampa por un escalón puesto
donde lo dice el **color**: cada píxel se va con el fondo o con el objeto según a cuál se parece, y
el corte queda a menos de 2 píxeles del borde de la imagen (medido en cuadros reales: 0,7 de
mediana, antes 26). Cada ojo busca después su vista sobre esa profundidad, y lo que el objeto
destapa al correrse se rellena con el fondo de al lado. No hay nada que ajustar.

* `cantos = viejo` en `serve.conf` vuelve al camino de antes, para comparar en el panel. Quitar la
  línea vuelve a los cantos finos.
* Cuesta 0,4 a 0,7 ms más por cuadro en una RTX 4090 a 3840 × 2160 (1,0–1,4 ms contra 0,66).
* En el registro: `[conversor] cantos: finos …` al arrancar y, cada 20 s, `[cantos] finos: el mapa se
  afinó en N de M cuadros`.
* Para probarlo sin panel: `tools/prueba-aparte.sh ODYSSEYVK_FUENTE_PNG=cuadro.png ODYSSEYVK_PRUEBA=15
  -- --conver --modelo RED.onnx --dump salida` deja en `salida-trama-vulkan.png` la profundidad ya
  afinada (la sonda 15); con `ODYSSEYVK_PRUEBA=1` o `2`, la vista de cada ojo.

### Dos planos por cuadro, como en la Nintendo 3DS (desde v0.48)

El relieve continuo es bonito en los planos buenos y sucio en los malos: un partido de fútbol o una
serie con mucho desenfondo producen un mapa de profundidad manchado, y ahí el relieve se convierte
en una masa sin bordes. La Nintendo 3DS lo resolvía al revés: la pantalla no decidía las capas, las
decidía el **juego**, que dibujaba dos o tres planos y un potenciómetro que solo los separaba más o
menos. Aquí hay ahora lo mismo: el conversor puede cerrar el mapa en **dos láminas planas**, una
detrás y otra delante, y el deslizador de relieve las separa.

```sh
odysseyvk capture3d --uuid '<id>' --conver --planos 0,6    # dos planos desde arrancar
```

* `--planos 0..1` (o `planos = 0,6` en `serve.conf`) es la **dureza**: `0` es el valor de arranque y
  deja el relieve continuo, píxel por píxel, exactamente como estaba; `1` deja dos láminas perfectamente
  planas. En marcha se sube y se baja con `g`/`v`.
* Dónde cae el corte **no lo decide un parámetro fijo ni una red nueva**: lo decide el propio cuadro con
  un Otsu sobre el histograma del mapa ya limpio, y de cada grupo se toma su media. Si el cuadro no tiene
  dos grupos claros (los dos picos separan menos de 0,15 o uno de los dos ocupa menos del 6 % del cuadro)
  se queda en continuo y no se corta nada.
* Los dos cortes **se siguen en el tiempo** con una media móvil lenta (0,08) y banda muerta, y se vuelven
  a calcular de golpe en un cambio de plano o cuando las láminas se pegan. Esto es lo que evita que la
  costura parpadee cuadro a cuadro: medido sobre catorce fotogramas reales de vídeo, con dureza 1 solo el
  **0,20 % de los puntos** cambia de lámina de un cuadro al siguiente.
* En la costura hay una **franja suave** de ancho `min(0,33 · separación, 0,05)` con `smoothstep`, no un
  tajo seco; aun así, en un plano fijo mal segmentado el tajo se va a notar: por eso la dureza empieza en
  0 y se sube a gusto de cada uno.
* **Una muestra intermedia entre las dos láminas (desde v0.50)**: con la franja suave anterior, la porción
  del medio del cuadro sale como una **rampa continua** — cada punto con su desplazamiento — y eso se lee
  como un degradado entre capa y capa. Con `--muestras 1` (o `muestras = 1` en `serve.conf`) el tramo del
  centro recibe **su propio nivel**, clavado a mitad de camino exacta entre las dos láminas, con su meseta:
  se ve sólido. El paso de un nivel al siguiente sigue siendo el `smoothstep` de siempre, solo que
  comprimido en una franja angosta, así que el canto no se corta ni parpadea. `--muestras 2` o `3` añaden
  dos o tres niveles (hasta cinco en total); `0` es la rampa continua de siempre. Medido sobre ocho cuadros
  reales: el parpadeo de la costura **no sube** (0,21 % con y sin muestra) y el cuadro por delante se mueve
  un punto. En marcha: **`n`** agrega una muestra y **`x`** la quita. Como la muestra se aplica antes de la
  mezcla por dureza, con `planos = 0,3` solo se recorre un 30 % del camino a la meseta: para verla sólida
  hay que subir la dureza con `g`.
* Todo esto pasa **en el conversor, sobre el mapa de 518 × 518**, no en el sombreador: no toca los números
  de la trama ni los topes por ojo, que siguen mandando (+12,67 hacia afuera y 9,98 hacia adentro sobre
  3840). Un cuadro con las dos láminas puede verse más brusco en el relieve, nunca más caro.

Se comprueba sin mirar el panel:

```sh
odysseyvk mapa foto.png            # foto real -> mapa PGM, sin panel y sin visor
odysseyvk planos mapa.pgm          # donde estan los dos planos y cuanto parpadean
ODYSSEYVK_MEDIR=1 odysseyvk ...    # imprime [planos] dureza · atras · delante · % del cuadro por delante
```

`odysseyvk planos` acepta varios mapas seguidos (una secuencia) y con `--hist` imprime las 32 casillas
del histograma con barras, que es la forma rápida de ver si un cuadro tiene realmente dos planos o solo
uno; `--dureza 0,6` escribe el mapa ya cerrado en láminas (`--out`) para compararlo byte a byte. Con
`--mapa-fijo MAPA.pgm --apoyo 0,4 --relieve 50` el visor arranca con ese mapa puesto y sin red, así que
la misma trama se puede medir dos veces y salir idéntica: es la única forma de comparar A/B sin que el
panel y la red metan ruido.

## Ajuste fino (teclas)

| tecla | efecto |
|-------|--------|
| `ESC` | salir (restaura panel, apaga lente y scaler) |
| `Tab` | **entregar la pantalla**: apaga el 3D del panel y minimiza esta ventana, así que el ratón y el teclado vuelven a lo de detrás. Devolverle el foco (`Meta+Alt+Tab`, que sí lista las minimizadas, o un clic en la barra de tareas) vuelve a poner lente, scaler y modo 3D |
| `PB` (Pausa/Break) | **la pastilla siempre a la vista** (desde v0.23): si estaba ocultada vuelve, y si el gestor de ventanas nos minimizó la ventana, se devuelve. Si el foco lo tiene el vídeo la tecla no llega al visor: ata `odysseyvk ctl aparece` a un atajo global (abajo, en «El ratón atraviesa el visor») |
| `P` | pausar / reanudar el vídeo del navegador (por MPRIS, sin quitar la capa) |
| `3` | pase 2D (sin tramar) / 3D |
| `F` | invertir profundidad (si se ve pseudoscópico: lo de dentro fuera) |
| `M` | cambiar el formato del vídeo: lado a lado → arriba y abajo → 2D plano → vídeo normal (2D a 3D) |
| `C` | encender / apagar el conversor de **vídeo normal a 3D** (lo mismo que la tesela «Vídeo normal» de la tarjeta) |
| `R` | **pegar / soltar los rótulos y subtítulos** del plano de la pantalla (con el conversor puesto; abajo) |
| `←` `→` | fase de la trama (±0.002 ciclos) |
| `RePag` `SavPg` | paso de lente (±0.0002); se usa si el paso real difiere del nominal |
| `[` `]` | **rampa_x** de la rejilla escrita (±0.0004 ciclos por pixel), encima de `rampa_x` de `serve.conf`. Es la llave fina del batido horizontal: si el paso que escribimos no es el de la lente, salen franjas verticales gruesas por toda la imagen (v0.43). **Con Mayús, paso fino de ±0.00002** (v0.59, solo Vulkan): el grueso corre la fase 1,5 ciclos de borde a borde y se salta el punto bueno |
| `;` `'` | **rampa_y**, lo mismo en vertical: inclina la rejilla escrita y quita las franjas casi horizontales (v0.43). Con Mayús, el mismo paso fino |
| `b` | **patrón de un solo ojo** (v0.59, solo Vulkan): blanco a la vista izquierda y negro a la derecha → al revés → imagen normal. Con un ojo cerrado, si el paso escrito es el de la lente se ve la pantalla entera de un tono; si no, bandas diagonales. Ver «Calibrar el paso contra la lente» |
| `↑` `↓` | crosstalk dinámico (±0.002) |
| `,` `.` | pendiente del filtro de traza (±0.5) |
| `9` `0` | convergencia horizontal (∓0.0005) |
| `j` `k` | **capa fondo** (0,20–2,00, de 0,05 en 0,05): cuánto más se va hacia atrás lo que está **detrás** del plano de apoyo, sin tocar los primeros planos (v0.45) |
| `u` `i` | **capa frente**: lo mismo con lo que sale **delante** de la pantalla. Los dos en 1,00 es exactamente como se comportaba antes |
| `h` `l` | **empuje del fondo** (−12…+12 puntos de panel, de 0,5 en 0,5): desplaza toda la capa de detrás a un plano concreto, sea cual sea su profundidad (v0.46) |
| `y` `o` | **empuje del frente**: lo mismo con la capa de delante; es la forma de traer al frente subtítulos, banderines y ventanas (positivo = sale más de la pantalla) |
| `g` `v` | **dureza de los dos planos** (0…1, de 0,1 en 0,1): 0 deja el relieve continuo de siempre; al subirlo el cuadro se queda en dos láminas planas, una detrás y otra delante, como la Nintendo 3DS (v0.48). Solo tiene efecto con el conversor de vídeo normal puesto |
| `n` `x` | **muestras intermedias** entre las dos láminas (v0.50): `n` agrega un nivel clavado a mitad de camino, con su meseta, para que el centro se vea sólido en vez de degradado (hasta 3); `x` lo quita y a 0 vuelve la rampa continua. Pide primero la dureza subida con `g` |
| `8` | reiniciar el ajuste a los valores nominales (las rampas vuelven a lo de `serve.conf`) |
| `Ctrl+Shift+1` | 3D puesto / apagado (igual que el Odyssey 3D Hub oficial). **Desde la v0.75 los `Ctrl+Shift+1…8` y `Ctrl+Shift+F1` son atajos globales del escritorio** (KDE): valen aunque el teclado lo tenga el vídeo o un juego. Solo están activos mientras hay una sesión 3D; se cambian en Ajustes del sistema → Atajos → «OdysseyVK»; `atajos_globales = no` en `serve.conf` los quita; `odysseyvk ctl atajos probar` los comprueba sin teclado |
| `Ctrl+Shift+F1` | ocultar / mostrar la pastilla de la interfaz (no libera el ratón: para eso es `Tab`). Ocultada, vuelve sola en cuanto el puntero entra en el rincón derecho de arriba de su sitio de siempre. Sigue valiendo `Ctrl+Shift+2`; si el teclado lo tiene otra ventana a pantalla completa, `odysseyvk ctl capa ocultar` hace lo mismo por fichero |
| `Ctrl+Shift+3` `4` | bajan / suben **3D Profundidad** (0–100) |
| `Ctrl+Shift+5` `6` | bajan / suben **3D Salida** (−50…+50) |
| `Ctrl+Shift+7` `8` | achican / agrandan la pastilla (0,5× … 3×, útil al pasar del escritorio al modo 3D) |
| `--ui-escala X` | arrancar la pastilla ya con ese tamaño (`1` pequeña, `2` grande; por defecto 2 en 4K) |
| `espacio`, `-`, `=`, `[`, `]` | pausa, ±5 s, cuadro a cuadro (en `play`) |

Las teclas son **por posición**, con los nombres del teclado de EE. UU. (así las da GLFW): en un
teclado latinoamericano `[` `]` son las dos de la derecha de la `P` y `;` `'` las dos de la derecha de
la `L` (la `ñ` y la de al lado). `odysseyvk play` va por OpenGL y ahí no hay ni paso fino ni `b`: para
calibrar se usa `odysseyvk test` o una sesión del vigilante, que van por Vulkan.

Cada cambio imprime una línea con los valores actuales; puedes arrancar desde ellos con
`--align`, `--pitch` y `--eye-x/--eye-y/--eye-z`, o dejarlos escritos en `serve.conf`
(`fase`, `paso`, `filtro`; abajo, en «Servicio que arranca con la sesión»).

### Calibrar el paso contra la lente (v0.59)

Las franjas gruesas que cruzan la pantalla sin seguir lo que hay en la imagen son la trama batiendo
contra la lente, no la red de profundidad: un error de **paso** las da inclinadas como la lente (unos 20° de
la vertical) y uno de **inclinación** casi horizontales. Un error de la red se ve distinto: cantos de
objetos que se estiran o se despegan, siempre pegados al contenido. Para que no quede ni una banda en la pantalla la fase tiene que derivar menos de 0,1 ciclo
de un borde al otro, o sea un error de paso por debajo de 0,1/3840 ≈ 2,6·10⁻⁵ ciclos por pixel. La foto de
Windows con la que se afinaron `rampa_x`/`rampa_y` da 0,450 ± 0,006: unas 200 veces más gruesa que eso.
La única referencia con esa precisión es la propia lente, y se mide así:

1. `odysseyvk test` (o el botón de la ventana de ajustes, que lo lanza con rastreo). Sentarse centrado
   bajo la cámara y comprobar en la pastilla que diga «cara a la vista»: la fase sigue al ojo.
2. `b`: un ojo recibe todo blanco y el otro todo negro. Cerrar un ojo.
3. Si se ven bandas diagonales, `Mayús+]` o `Mayús+[` hacia donde las bandas **se ensanchan**, hasta que la
   pantalla quede de un solo tono (o con la banda más ancha posible). Después `Mayús+'` / `Mayús+;` para la
   inclinación. Cada pulsación imprime `rampa_x +0.000380 sobre la config (fino)`.
4. Cambiar de ojo: el otro tiene que ver el tono contrario en toda la pantalla. Si un ojo ve el tono
   equivocado **parejo**, eso es fase (`←` `→`) o el orden de ojos (`F`), no paso.
5. Apuntar en `serve.conf` la suma: `rampa_x` de la config + lo de las teclas (en `estado` salen
   `rampa_x` y `rampa_x_teclas`). `b` otra vez vuelve a la imagen; `8` lo apaga todo.

Merece la pena hacerlo también **desde cero**: comentar `rampa_x`/`rampa_y` en `serve.conf` y ver si con la
trama del modelo de fábrica (rampa 0) las bandas son más anchas o más estrechas que con las de ahora.

### Cómo alinear

1. `odysseyvk test` y colócate a la distancia normal de trabajo (la altura de los ojos sobre el
   centro del panel es `--eye-y` ≈ 8.4 cm, distancia `--eye-z` ≈ 65 cm).
2. Mira la fila de nueve cuadros con disparidad del patrón: deben verse **flotando a distinta
   profundidad, sin duplicados ni bordes dobles**.
3. Si se ve una banda que "baila" o se desdobla: `←`/`→` hasta que las columnas encajen.
4. Si el efecto sólo es bueno en el centro de la pantalla y se rompe hacia los lados: ajusta el
   paso con `RePag`/`SavPg`.
5. Si la profundidad está invertida (lo cercano se ve lejos): pulsa `F`.
6. Para comprobar que la lente y el scaler están bien: `3` (2D) debe verse normal y plano, `3`
   otra vez debe dar volumen.

### Si quedan franjas verticales finas por toda la pantalla

Con la trama ya alineada pueden quedar unas líneas verticales claras y oscuras, muy juntas
(uno a tres milímetros). Hay tres orígenes y se distinguen midiendo, no a simple vista:

**1. Las armónicas de la propia trama (lo medido en esta unidad: esto es lo que se ve).**
El cambio de una vista a la otra con la pendiente de filtro de fábrica (10) es casi un escalón,
y un escalón muestreado en la rejilla del panel se pliega: sus armónicas 5ª, 7ª, 9ª y 11ª caen a
**4,34 · 8,17 · 10,73 px de panel**, que con el paso de píxel de este panel (0,1554 mm) son
**0,67 · 1,27 · 1,67 mm**: franjas finísimas por toda la pantalla. Medido sobre el volcado de la
trama (`tools/mide_franjas.py`), los picos salen en **4,3 · 8,2 · 10,9 px** con un realce de
×4,4 · ×4,3 · ×2,9 y 0,4–1,7 niveles de gris: justo lo que se ve y nada que ver con el paso de
la lente mal ajustado. La prueba en marcha: con `RePag`/`SavPg` el periodo **no** se mueve un
0,02 %, se mueve quince veces más (a `paso = 1,005` la raya de 4,34 px pasa a ~5 px). Y con `,`
(bajar la pendiente del filtro) baja mucho, porque las armónicas pasan del 134 % de la fundamental
a ~27 % con pendiente 3.

**2. Que no se vayan con `Ctrl+Shift+5` no significa que no sea el relieve.** Con `3D Salida` en
0 la ganancia no es 0: `gananciaRelieve(0) = 0,00104`, o sea **4,0 px de separación entre ojos**
(dos periodos y medio de la lente). Mientras las dos vistas no sean idénticas del todo, la trama
tiene de qué hacer las rayas. Para un A/B limpio usa `relieve = 0` en `serve.conf` (deja la
imagen plana de verdad).

* **3. Las curvas de nivel del mapa de profundidad.** El mapa llega a 518×518 (7,4 px de panel por
  celda), y tras el filtro guiado, el dilate y el corte de plano queda por mesetas: medido, meseta
  mediana de **30 px de panel** con saltos de hasta **255 niveles**, que con el tope de Samsung son
  los 22,65 px de recorrido de un salto a otro. En los cantos el desplazamiento barre la trama de
  golpe y el borde sale mordido (los "cracks" y "rasguños" de los bordes); en las zonas de pendiente
  suave las curvas de nivel se ven como líneas verticales. El programa de Samsung lo tapa con su
  *dither* de profundidad (`depth.dither`, 0,25 px: aquí `temblor`, que ya venía puesto).

En Windows no se notan porque su mezcla entre las dos vistas es más suave y además temblaba la
propia fase del patrón. Aquí se puede probar los dos: `,`/`.` para la pendiente y
`fase_temblor = 0,1` en `serve.conf` para el temblor de la fase (medido sobre la fórmula del
shader: con ±0,1 de ciclo las armónicas de 4,34 y 8,17 px bajan unas cinco veces y la fundamental,
que es la que hace el 3D, pierde solo un 7 %).

Antes de nada, `3` (2D plano): si en 2D también se ven, no es nada de esto; sería escalado
(mira `[aviso] la ventana es WxH … la trama necesita pixel a pixel` en la salida del visor)
o el propio panel.

Para medirlo desde fuera, `volcado = /tmp/chk` en `serve.conf` deja la sesión en marcha y guarda
`/tmp/chk-trama-vulkan.png` (la trama entera tal y como sale al panel),
`/tmp/chk-trama-con-mapa.png` y `/tmp/chk-trama-sin-releve.png` (el **mismo cuadro** con el releve
puesto y apagado, para comparar sin el lío de que el vídeo haya cambiado entre medias),
`/tmp/chk-profundidad.pgm` (el mapa de profundidad) y `/tmp/chk-chico.ppm` (lo que vio la red).
Y `python3 tools/mide_franjas.py /tmp/chk` lo mide y lo traduce: el periodo de cada franja en
píxeles con su amplitud, cuántos golpes de fase por mil píxeles tiene la rayita de la trama y con
qué separación, si esa franja está en la trama con relieve y no en la plana (sale del mapa) o en
las dos (sale de la trama), y de cuántos píxeles son las mesetas del mapa.



## La ventana de ajustes: otro proceso que manda números al visor (desde v0.51)

El visor se queda el teclado de la pantalla del panel, así que hasta v0.50 los números solo se movían
con las teclas de esa ventana o tocando `serve.conf` y reiniciando el servicio. Ahora hay una ventana
aparte, `odysseyvk-ajustes`, que **no comparte nada con el visor**: no enlaza sus librerías, no tiene
socket, no puede tocar la trama. Habla con él por ficheros en `~/.local/state/odysseyvk`:

| fichero | qué es |
| --- | --- |
| `ajuste` | la **orden**: números sueltos y una línea `modelo = /ruta.onnx`. El visor lo mira cada 0,1 s y solo actúa si el fichero cambió |
| `estado` | la **verdad del visor**, reescrito cada 0,5 s: versión, trama y conversor puestos o no, qué red y en qué tarjeta, y los 20 números |
| `preferencias.log` | la **cuenta de lo cambiado**: hora, valor antes → después, red puesta y quién lo pidió |
| `hub` | la **verdad del vigilante** (`odysseyvk serve`), reescrito cada 1 s: si hay sesión, si es de `video` o de `escritorio`, en qué pantalla, con qué modo y qué red. Lo escribe el hub y lo borra al pararse |
| `camara.jpg` | el **último cuadro crudo** de la cámara, volcado por quien la tiene abierta: el visor (una vez cada 2 s, o unas 8 por segundo si la ventana le mandó `vista = 1`) o el hijo `odysseyvk ctl vista` que la propia ventana levanta cuando no hay sesión. La ventana de ajustes lo enseña sin abrir la cámara una segunda vez |

En `estado` hay dos claves nuevas (v0.58) para que la casilla de **Previsualización** y una sesión 3D no
peleen por la cámara: `camara_pedida = si|no` dice si la sesión abierta **quiere** la cámara para el
rastreo, aunque en este momento no la tenga abierta, y `camara_vista = rapida|normal` dice cada cuánto
está volcando `camara.jpg`. Con la primera, el hijo `ctl vista` se aparta solo en cuanto el visor
reclama el dispositivo; con la segunda, la ventana comprueba que el visor le hizo caso.

```sh
odysseyvk-ajustes                       # la ventana (la puedes tener en el otro monitor)
odysseyvk ctl ajuste                    # ver el canal y el estado del visor
odysseyvk ctl ajuste planos 0,6 fase 0,004     # mandar números a mano (también sirve fase=0,004)
odysseyvk ctl ajuste modelo=/ruta/mde_large_fp32.onnx   # cambiar de red en vivo
odysseyvk ctl ajuste limpiar            # quita la orden: vuelven a mandar las teclas
odysseyvk ctl estado                    # lo que el visor está usando ahora
odysseyvk ctl modelos                   # los .onnx que hay en el disco, con la puesta marcada
odysseyvk ctl preferencias --ultimos 20 # lo que se fue cambiando
odysseyvk ctl escritorio poner          # la trama ENCIMA DEL ESCRITORIO, sin vídeo (mismo botón de la ventana)
odysseyvk ctl escritorio quitar         # y abajo: el visor se despide devolviendo el panel
odysseyvk ctl escritorio estado         # qué sesión tiene abierta el vigilante ahora mismo
odysseyvk ctl capa ocultar               # ocultar / mostrar / alternar la pastilla por fichero (estado: `ctl capa`)
odysseyvk ctl 3d apagar                 # lo mismo que el interruptor ON/OFF de la pastilla (estado: `ctl 3d`)
odysseyvk ctl legado encender           # el modo de legado, por fichero (apagar / alternar / estado)
```

Números que admite el canal (con topes, los mismos que ajustan las teclas): `fase`, `paso`, `filtro`,
`crosstalk`, `conv`, `rampa_x`, `rampa_y`, `rampa_x_teclas`, `rampa_y_teclas`, `fondo`, `frente`,
`empuje_fondo`, `empuje_frente`, `planos`, `muestras`, `temblor`, `canto`, `fase_temblor`, `relieve`,
`apoyo`. Las comas se aceptan (`planos 0,6`).

Además de números, el canal admite **una orden**: `overlay = 1` pide que la trama se monte **encima del
escritorio del monitor**, sin esperar a que nadie se ponga a pantalla completa, y `overlay = 0` la quita. No
la atiende el visor sino el hub (es el único que puede lanzar y despachar una sesión), y el botón de la
ventana la pone y la quita con un clic cada uno:

- Un clic lanza `odysseyvk test --siempre`: panel a 3D, escala, lente, pastilla, rastreo y rampas como en
  una sesión normal, pero tejiendo su **patrón de trama** en vez de un vídeo. Sirve para mirar y afinar la
  trama, las franjas y las rampas sin abrir nada, y para dejar el 3D puesto mientras trabajas.
- Otro clic (o `ctl escritorio quitar`, o ESC/Tab sobre el visor) manda SIGTERM al visor, que devuelve el
  panel por la puerta de adelante. Mientras está puesto, el hub **no** ofrece el 3D de vídeo: hay sesión
  abierta.
- No capturamos la salida del monitor para meter el escritorio dentro del 3D: en esa imagen estaría nuestra
  propia ventana y la trama se tejería a sí misma. El patrón de trama es lo que se teje.
- La orden solo actúa cuando el número **cambia** en el fichero; la primera lectura al arrancar el servicio
  se ignora, para que un `overlay = 1` olvidado de cuando el equipo se apagó no monte la trama solo.

Desde v0.57 la ventana tiene **dos hojas**: *General* y **Edición avanzada** (así se llama lo que hasta
ahora era *Trama y redes*, la de arriba tal cual). En *General* está lo breve de la app —el modo, la
sesión abierta y la **cámara**, que se mudó entera desde la hoja que había de *Cámara*— y en *Edición
avanzada* los 20 números y las redes. Con el **modo de legado** puesto, la hoja *Edición avanzada*
**sale atenuada**: ahí no hay nada que mover.

El grupo *Modo de la app* de *General* lleva la casilla **«modo de legado»** con su explicación y una
línea que el temporizador de 0,5 s refresca con la verdad: en qué modo está el visor abierto, y si lo
pedido para la próxima sesión no coincide con lo vivo, avisa (`Ojo: …`). Abajo, el grupo *La sesión
abierta* resume en tres controles lo que antes estaba repartido: `3D: encender la trama` /
`3D: apagar la trama`, `pastilla: mostrar` / `pastilla: ocultar` y la casilla de Facetracking. Los dos
botones se deshabilitan si no hay visor abierto.

El modo de legado se entiende entero en `docs/metodo-legado.md`. En corto: la trama se queda con sus
**números de fábrica** (solo se pueden mover *3D Salida* y *Plano de apoyo*), no se abre red de
profundidad ni conversor 2D→3D, la cara se mide siempre con las dos vistas de la cámara del monitor
(error de reproyección admitido 40 px, como en la referencia), y si la cámara deja de ver la cara se
suelta la trama a los 0,5 s y se apaga la lente a los 0,75 s. La pastilla se recorta a lo justo (ver
*La interfaz*). Se pone también con `odysseyvk play --legado`, con `ctl legado` o con `legado = si` en
`serve.conf`. **En la ruta `--gl` no está implementado** (se acepta la bandera y no hace nada).

La casilla de la ventana y `ctl legado` mandan la orden viva al visor abierto **y** escriben `legado` en
`serve.conf` (lo de antes queda en `serve.conf.auterior`). Desde v0.57 el vigilante vuelve a leer esa
línea justo antes de lanzar un visor, así que el modo nuevo se nota en la siguiente sesión sin
reiniciar el servicio; los demás números de `serve.conf` siguen necesitando reinicio.

Desde v0.54 la ventana enseñaba el rastreo de cara en su propia hoja:

- un sello de color con la verdad del momento: `FACETRACKING ACTIVO · Stereo (3D Studio, camara integrada
  del monitor)`, o `· Externa (webcam aparte, via fallback por el ancho de la cara)`, o
  `FACETRACKING PARADO: …` con el motivo. Sale de las claves nuevas que el visor escribe en `estado`:
  `facetracking`, `camara`, `camara_tipo`, `camara_cara` y `camara_nota`.
- el combo de las `/dev/video*` que dan cuadro: nombre que anuncia el conductor, id USB (`04e8:20d7` es la
  del Odyssey), si es la estereo del monitor (anuncia un modo 2×alto, 1280x480), el modo de captura que
  daría, y si lo tiene abierto otro proceso. El nodo de *metadata* de la cámara del monitor (`/dev/video1`)
  no aparece: no da cuadro. La lista se vuelve a contar cada 4 s, no veinte veces por segundo.
- la **vista**: el JPEG crudo de `camara.jpg` decodificado por Qt (1280x480 con las dos vistas una al lado
  de la otra, medidas en la máquina de la cámara). La ventana no abre la cámara directamente: el monitor no
  admite dos aperturas y su puerto UVC se pone terco si lo toca un segundo proceso.
- la casilla **«Previsualización (la camara aqui, en vivo)»** (v0.58), que es lo que hace que la vista
  **se quede** en pantalla moviéndose mientras esté marcada. Quién manda los cuadros depende de quién tenga
  la cámara:
  * si hay sesión 3D y esa sesión quiere la cámara (en `estado`, `camara_pedida = si`), la ventana le manda
    `vista = 1` por el canal de siempre y el visor vuelca un cuadro **ocho veces por segundo** en vez de uno
    cada dos. Se puede comprobar en `estado`: pone `camara_vista = rapida`;
  * si no hay sesión, o la sesión no usa la cámara, la ventana levanta `odysseyvk ctl vista --fps 8
    --segundos 60 [--dev /dev/videoN]` como hijo suyo: abre la cámara **suelta**, sin rastreo, y le va
    guardando el cuadro. Dura 60 s y la ventana lo vuelve a levantar sola, con metro y medio de calma, así
    que no hay que tocar nada para seguir viendo.
  El hijo mira `estado` cada vuelta y, en cuanto ve que la sesión abierta quiere la cámara, la suelta y se
  sale con **código 3** (sin peleas ni reintentos a lo bruto); el visor, por su parte, reintenta abrir la
  cámara un par de veces cada 2 s si al arrancar la encontró ocupada, así que el rastreo se suma a media
  sesión. Al desmarcar la casilla se cierra la fuente y en el recuadro **queda el último cuadro** que llegó,
  con un aviso al lado de lo viejo que es (`ojo: lo del recuadro es un cuadro de hace 12 s`). Con
  `odysseyvk-ajustes --vista` la ventana arranca ya con la casilla marcada.
- el botón `Usar esta camara en el rastreo`, que escribe `camara = /dev/videoN` (y `mono = si|no` si se
  eligió cómo leerla) en `serve.conf` —la línea anterior queda en `serve.conf.auterior`— y manda la misma
  orden viva por el canal de siempre. Vale para la **próxima** sesión: la cámara que tiene abierta la sesión
  en curso no se le cierra a nadie.

Las redes vienen en **una sola lista**, sin separadores, y cada una lleva su marca según el nombre del
fichero: las de Samsung se reconocen por cómo se llaman (`mde_*` y `Disp_*`) y salen como
**Samsung replicate**; el resto, **Native**. Los nombres siguen simples (el nombre del fichero, con su
tamaño y lo que sabemos de él —`mde_tiny` avisa de que aquí no abrió, y `face_detection_yunet` de que no es
de profundidad sino la cara del rastreo—). Medido con `--prueba` en esta máquina: 10 redes,
`mde_large.onnx → Samsung replicate`, `depth_anything_v2_small.onnx → Native`.

Tres cosas que conviene saber:

- **La orden es de una sola vez.** Cada escritura reemplaza el fichero entero, así que un número que
  luego moviste con las teclas dentro del visor no vuelve atrás cuando mandes otro distinto. Para ver
  qué tiene el visor de verdad, mira `ctl estado`, no el fichero `ajuste`.
- **Cambiar de red puede tocar cerrar y abrir el visor.** Si un `.onnx` no abre, ONNX Runtime queda
  inservible en ese proceso (es el arreglo de v0.33 para que el visor no reviente sin devolver el panel):
  el encargo se queda escrito en `ajuste` y la red se abre sola en la siguiente sesión. El visor lo dice
  por registro y en la pastilla.

Sin Qt instalado la ventana simplemente no se compila (`cmake -DODYSSEYVK_UI=OFF` o sin `Qt6Widgets`) y
todo lo demás —visor, hub, verbos `ctl`— queda igual.

## Cuál es el indicador de la esquina

**No hay ninguno.** El cuadro de color de arriba a la izquierda del vídeo (verde con lente y scaler,
naranja o rojo si faltaba alguno) se quitó del sombreador en v0.78, y con él el letrero `lente y scaler
activos` que iba escrito encima de la pastilla. Si la lente, el scaler o la cámara **no** responden,
lo dice un aviso colgado debajo de la pastilla (`la lente 3D no respondió`, `el scaler del monitor no
respondió`, `sin cámara: la trama va con la posición de fábrica`); con todo en orden no se pinta nada. El
detalle completo sigue en `odysseyvk ctl estado` o `tail -f ~/.local/state/odysseyvk/estado`.

## El panel de ajustes y los juegos (v0.84)

`odysseyvk ajustes` —el icono de OdysseyVK del menú del sistema, o el engranaje de la pastilla
cuando hay un vídeo en 3D— abre una tarjeta translúcida de esquinas redondas que flota en el centro
de la pantalla, encima de lo que haya. Abrirla otra vez la cierra (también el aspa). Con un vídeo
en 3D debajo se dibuja al doble, porque se ve a través de la lente. Se mueve arrastrando su cabecera
(v0.86). Tres pestañas (la tercera, **Rendimiento**: cuadros por segundo, lo que tarda el conversor y el
uso, la memoria y la temperatura de cada tarjeta gráfica):

* **Ajustes**, solo lo de todos los días:
  * qué hacer al ver un vídeo a pantalla completa: preguntar, vídeo normal o vídeo dividido (`modo`);
  * suavizar los vídeos divididos (`dividido_suavizado = si | auto | no`): siempre, solo si hace falta
    (v0.88: el vídeo se ve con su propio relieve mientras lo más cercano y lo más lejano no pasen de
    8 mm de separación; al pasarse entra el suavizado y sale cuando lleva 4 s por debajo de 5 mm) o
    nunca;
  * fuerza del relieve del conversor (`relieve`) y reducir el fantasma (`crosstalk_base`, se nota al
    momento con un vídeo abierto);
  * seguir la cabeza con la cámara (`rastreo`), mantener el HDR durante el 3D (`hdr_en_3d`), atajos
    de teclado en todo el escritorio (`atajos_globales`);
  * las aplicaciones que pasan sin preguntar (`auto = app:modo`), con un aspa para olvidar cada una.
* **Juegos**: la biblioteca de Steam, leída de sus propios ficheros (`libraryfolders.vdf` y los
  `appmanifest_*.acf` de cada biblioteca; sin Proton ni los Steam Linux Runtime), los jugados hace
  menos primero, en rejilla con su carátula (la de la caché de Steam, `appcache/librarycache`). Se
  pica una carátula y el juego arranca (`steam steam://rungameid/ID`). La pastillita **3D / 2D** de
  cada juego (desde v0.91 un interruptor con la bola a la izquierda en 2D y a la derecha en 3D) dice si entra solo en 3D al ponerse a pantalla completa o se deja como está: se guarda
  como `auto = steam_app_ID:conv` o `:flat`, que es como se llama la ventana de un juego de Steam.

Todo se escribe en `~/.config/odysseyvk/serve.conf` y el servicio lo relee solo cuando cambia (ya
no hace falta reiniciarlo para esas claves). La ventana de siempre, con los veinte números de la
trama, los perfiles por red y la vista de la cámara, sigue existiendo como banco de taller:
`odysseyvk-ajustes`, o clic derecho en el icono → «Ajustes de taller».

## La interfaz (pastilla con ratón)

El visor dibuja una pastilla arriba a la derecha, con cursor de flecha y manejable con el ratón. Desde
v0.78 es **una sola fila** (462 × 76 px del lienzo virtual; 651 con la casilla; antes 824 × 134), y
desde v0.79 lleva el **tono de la tarjeta**: fondo gris casi negro y el acento morado del paisaje
(antes verde):

* **interruptor ON/OFF del 3D** (`Ctrl+Shift+1`): pista verde con la bola a la izquierda y `ON`, pista
  gris con la bola a la derecha y `OFF`. Se pica en cualquier punto de la pista;
* **Profundidad** 0…100 (`Ctrl+Shift+3` / `4`) — en modo de legado se llama **Plano de apoyo**;
* **Salida** −50…+50 (`Ctrl+Shift+5` / `6`) — acerca o aleja la imagen de la pantalla;
* dos **botones de solo icono**, con el dibujo de las teselas de la tarjeta. El que está puesto sale
  encendido y con filo; se pica el otro y el visor cambia de tipo de 3D sin salir del vídeo. Si la
  aplicación estaba guardada, se queda guardada con el tipo nuevo. En modo de legado no salen:
  * una imagen entera = **vídeo normal** (el conversor le pone el relieve);
  * el otro = **vídeo dividido**. De normal va **suavizado** (una imagen con la raya a trazos en
    medio: una sola de sus dos imágenes y el relieve del conversor; ver «Cuando el vídeo dividido se
    ve doble»). El suavizado solo se quita en el panel de ajustes (v0.84); quitado, el botón lleva
    las dos imágenes separadas y teje el vídeo con el relieve que trae;
* el **engranaje** (v0.84), al final: abre el panel de ajustes por encima del vídeo;
* debajo, la casilla **No volver a preguntar**, que solo sale cuando el vídeo lo lanzó el servicio: es la
  misma de la tarjeta de la orilla, pero dentro del vídeo. Marcada, esa aplicación pasa directa al 3D
  la próxima vez; **desmarcada, el servicio vuelve a preguntar**. El cambio se apunta en `serve.conf`
  (`auto = app:modo`) y debajo de la pastilla sale unos segundos la respuesta;
* un renglón con las teclas: `Ctrl+Shift+ 1 el 3D · 3/4 profundidad · 5/6 salida · F1 oculta · Esc sale`.

Salieron de la pastilla (v0.78): el **botón de la red** (se usa siempre la de Samsung, que es la que
mejor mide; las teclas sueltas `1` y `2` la siguen cambiando, y `ctl ajuste modelo=RUTA`), la fila
**Facetracking** (informaba, no mandaba), los avisos de teclas repartidos y el letrero del hardware.
La pastilla sigue con las esquinas cuadradas (v0.56); las únicas curvas son la pista y la bola.

La **tarjeta de la orilla** (abajo a la derecha, la que pone el servicio al ver un vídeo a pantalla
completa) se parece desde v0.78 a la de la aplicación oficial: tarjeta oscura de esquinas redondas con
la pregunta «¿Qué tipo de vídeo estás viendo?», el nombre de la ventana en letra chica, dos teselas
con su dibujo —**Vídeo normal** (una sola imagen: el conversor 2D a 3D) y **Vídeo dividido** (dos
imágenes lado a lado)—, la casilla «Guardar el tipo de vídeo y no volver a preguntar» y, arriba a la
derecha, un `?` que explica las dos opciones al pasar el ratón y el aspa, que cierra sin tocar nada
(lo que antes era la tesela «2D plano»). El estéreo de arriba y abajo sigue solo por línea de orden
(`--tb`).

**La barra del reproductor en un vídeo dividido** (v0.81). Con el vídeo dividido cada ojo recibe
una mitad del cuadro, y lo que el reproductor pinta encima a lo ancho de la ventana (barra de
progreso, botones, título) también se partía en dos. Ahora el visor mira el cuadro una vez cada
cuatro cuadros nuevos: en las filas de vídeo las dos mitades casan (son la misma imagen, corrida
por el relieve); donde el reproductor pintó su barra, no. Esas filas —arriba y abajo de la pantalla—
se tejen en 2D con el cuadro entero: la barra sale de una pieza y se pica donde se ve; el resto sigue
en 3D. Para ponerla hacen falta dos análisis que la vean en medio segundo; para mantenerla basta con
mucho menos, y se quita 0,6 s después de que el reproductor esconde sus mandos (v0.82: antes
parpadeaba). Desde la v0.90, en esa franja **sigue el vídeo** (la imagen izquierda sola, en 2D,
suave y atenuada como detrás de un cristal esmerilado, a la que la imagen en 3D llega fundiéndose) y
encima van solo los **mandos** del reproductor, nítidos y donde están de verdad: se recortan del
cuadro por ser claros, destacar de lo que tienen alrededor y no estar repetidos en la otra mitad.
Antes se veían detrás de la barra las dos mitades del vídeo, una al lado de la otra. Y la franja ya
no se queda puesta con los vídeos de mucho relieve: el detector casa las dos mitades hasta un 12 %
del ancho (antes un 3 %, y un objeto muy cercano se tomaba por barra). `barra_2d = entera` en
`serve.conf` vuelve a la franja de antes (por si a un reproductor le faltara algún mando) y
`barra_2d = no` la apaga del todo. Para probar el detector con
capturas: `odysseyvk barra-detect [--filas] CUADRO.png …`. Con un vídeo dividido abierto,
`odysseyvk ctl barra guardar [N]` le pide al visor que deje N copias chicas del cuadro (518 × 518) en
`~/.local/state/odysseyvk/barra/`, para afinar el detector con el reproductor de verdad; el registro
lleva además un parte cada 20 s (`[barra] 20 s: …`).

**Cuando el vídeo dividido se ve doble** (v0.83). En un vídeo dividido la distancia entre la imagen
y su fantasma es la que el propio vídeo pone entre sus dos imágenes a esa profundidad. El conversor
de este panel nunca separa más de unos 4 mm; un vídeo dividido puede traer mucho más (medido por
Rodolfo: 2–3 mm en lo normal, casi 4 cm en objetos muy cercanos), y eso no lo arregla ninguna
calibración: 4 cm a 67 cm de distancia son 3,4°, que no se funden. Para esos vídeos está el tercer
botón de vídeo dividido, que desde v0.84 va suavizado de fábrica: el visor se queda con la imagen izquierda, estirada al ancho entero, y
el relieve lo pone el conversor de vídeo normal, dentro del presupuesto del panel. Se pierde el
relieve propio del vídeo (queda el que estima la red); con el 3D apagado se ve un 2D de verdad en vez
de las dos mitades. En `serve.conf` se guarda como `auto = app:suave`. El registro dice cada 20 s
cuánto separa el vídeo: `[relieve] el video dividido separa sus dos imagenes: lo normal +2,1 mm · lo
mas cercano +38,0 mm · lo mas lejano -3,2 mm`.

Lo que sí es de calibración es **cuánto se ve** ese fantasma: la parte de la imagen de un ojo que se
cuela en el otro (diafonía). El sombreador resta de fábrica el 1,29 % de la otra imagen en toda la
pantalla; desde v0.83 ese número se mueve en vivo con `odysseyvk ctl ajuste crosstalk_base=0,03`
(0 … 0,25) y se deja fijo con `crosstalk_base = 0,03` en `serve.conf`. (`crosstalk`, la clave de
siempre, solo pesa hacia los bordes de la pantalla: en el centro vale cero.)

**Probar el visor sin abrir ninguna ventana** (v0.82): `tools/prueba-aparte.sh` corre `odysseyvk
test` dentro de un compositor aparte y sin salida (gamescope en modo *headless*), con su propia
carpeta de estado. No aparece nada en el escritorio ni se toca la sesión 3D que haya abierta, así que
se puede medir la trama (volcado, `ODYSSEY_GOLPE`, `ODYSSEYVK_FUENTE_PNG`) mientras se ve un vídeo.

Para mirar la interfaz sin el panel: `ODYSSEYVK_HUD_PNG=/ruta/salida.png odysseyvk ctl hud --w 3840
--h 2160 [--legado]` la deja pintada en un archivo de fondo transparente (el mismo pintado de CPU que
usa la tarjeta), además de medir que cada rótulo quepa en su caja. `ODYSSEYVK_HUD_RATON=x,y` deja el
puntero en un punto, `ODYSSEYVK_HUD_RECORDAR=0|1|2` quita, marca o desmarca la casilla.

La pastilla se calcula en un lienzo virtual de 1080 px de alto y se escala con la altura del panel,
así que conserva las proporciones de la aplicación oficial a 3840x2160. Se apaga arrancando con
`--sin-interfaz`, o en cualquier momento con `Ctrl+Shift+F1` (`odysseyvk ctl capa ocultar` si el
teclado se lo quedó otra ventana). Las teclas finas de alineado (`3`, `F`, flechas, `RePag`…)
siguen funcionando igual.

### Rastreo sin la cámara del monitor: cualquier webcam de respaldo (desde v0.31)

La cámara buena es la **infrarroja estéreo del propio monitor** (`3D Stereo Webcam`, `04e8:20d7`): viene
calibrada de fábrica, da dos vistas de lo mismo y de ahí sale la posición de los ojos con pocos
milímetros de error). Si esa cámara no aparece o está ocupada, el visor **usa la primera webcam que
encuentre** (una HP 320 FHD, por ejemplo) y mide la cabeza por el tamaño de la cara en el cuadro:

* focal estimada `f = (0,5·ancho) / tan(0,5·campo)` con el campo horizontal que anuncie la webcam
  (`--track-campo`, `campo =` en `serve.conf`);
* distancia `z = f · cara / anchodecara`, con `cara = 150 mm` de oreja a oreja (`--track-cara`, `cara =`);
* `x` e `y` salen del centro de la caja de la cara, en centímetros del panel.

Es aproximado: si alguien mide la mitad de la cara, la distancia sale el doble de lejos, y de ahí el
error en `x`. Aun así sigue sirviendo para mover el relieve con la cabeza. El rastreo **no se ofrece a
encender ni apagar**: la fila `Facetracking` de la pastilla dice qué cámara lo alimenta (`3D Studio` la
integrada del monitor, `fallback` una webcam aparte). Se puede forzar el comportamiento
con `--track-mono si|no|auto` (`auto` = mirar qué cámara es: si el nombre dice «stereo»/«3D» o el modo
es 2× alto, se intenta la vía calibrada; si no, la de la cara).

Para comprobarlo sin sentarse delante:

```
odysseyvk ctl track --mono-simulacro              # la cuenta, sin cámara: debe decir "la cuenta cuadra"
odysseyvk ctl track --dev /dev/video0 --segundos 8  # en vivo: cara si · panel x y z en cm
odysseyvk ctl track --foto cuadro.jpg --dump anotado.jpg   # un cuadro guardado, sin tocar la cámara
odysseyvk ctl track --segundos 5 --volcar-crudo /tmp/ab     # vuelca cuadro crudo + anotado + A/B
odysseyvk ctl track --segundos 20 --grabar DIR              # graba la cámara tal cual (DIR ya creada)
odysseyvk ctl track --repetir DIR --csv pose.csv [--fino no]   # la misma grabación por el rastreo, sin cámara
```

Desde la v0.74 la distancia no sale de restar los ojos que YuNet da en cada vista (±4 cm de sesgo
lento, medido), sino de comparar la imagen de la franja de los ojos entre las dos vistas, y el punto
medio de los ojos se sigue con una plantilla. `rastreo_fino = no` en `serve.conf` vuelve al de antes,
y `tools/mide_rastreo.py` compara los dos sobre una grabación.

Y la **cámara suelta, sin rastreo**, que es lo que levanta la casilla de Previsualización de la ventana de
ajustes cuando no hay sesión 3D abierta (v0.58):

```
odysseyvk ctl vista --dev /dev/video0 --fps 8 --segundos 60   # lo que levanta la casilla
```

Abre la cámara con el modo que más le cuadre (MJPG antes que YUYV, y nada por encima de 1920x1200), guarda el
cuadro crudo en `~/.local/state/odysseyvk/camara.jpg` ocho veces por segundo y **cede la cámara**: si en
`estado` aparece una sesión que la quiere (`camara_pedida = si`), la suelta y sale con **código 3**; si le
cierran la entrada estándar —la ventana que lo levantó se cerró— se sale igual, y si la cámara no manda ningún
cuadro en dos segundos lo dice y sale con código 1. Sale solo al vencer `--segundos`. La cámara del monitor
(`04e8:20d7`) no admite dos procesos abiertos, así que esto nunca se usa a la vez que el rastreo del visor: lo
que hace la ventana es apagar el hijo en cuanto el visor reclama la cámara.

El sondeo en vivo cierra con la cuenta de por qué no salió la pose, que son tres cosas distintas:

```
careos       sin cara 512, cara descartada por los ojos 3, geometria rechazada 7 (a las 41 las saco el reintento con CLAHE)
```

La receta de detección es la del cliente de Mac: la vista entra a YuNet con **letterbox** (proporción
conservada, relleno 114) en vez de aplastada, umbral de cara **0,5**, y si la primera pasada no ve a nadie
se **reintenta con CLAHE** (`clipLimit 40`, `tile 8x8`) —se apaga con `--realce no`, solo paga en los cuadros
sin cara—. La triangulación admite hasta **10 px** de error de reproyección (antes 3 px; el Mac admite 40).
Sentado delante, el sondeo de 8 s con la cámara del monitor da **465 poses válidas de 467 fotogramas**, con
0,5-2,1 px de error de reproyección y 3,6-4,6 ms de YuNet por cuadro. Dos cosas a saber: el cuadro llega como
**dos vistas de 640x480 lado a lado**, y hay que recortarlas con `clone()` (una región de un cuadro de 1280
de ancho no es contigua: copiarla a bytes planos sin clonar mezcla las dos vistas y el detector no ve nada);
y el **primer cuadro** después de encender el streaming llega quemado por la exposición automática (57,5 % de
píxeles por encima de 250), que se asienta hacia el cuadro 30, unos medio segundo.

Si en cambio quieres la vía calibrada con la cámara del monitor, `--track-mono no` (exige `calib/`).

### Los subtítulos no se deslizan: rótulos apoyados en la pantalla (desde v0.32)

El síntoma: con el rastreo puesto, los subtítulos y cualquier texto se van de lado a lado al mover la
cabeza, y en Windows en cambio quedan quietos. No es un fallo del rastreo. Una zona **plana** de la
trama no se mueve nunca, porque las dos vistas salen idénticas y la trama no depende del ojo (es la
prueba que está escrita en `shaders/weave.frag`: con disparidad cero la salida no cambia ni con la fase
ni con el crosstalk). Lo que se desliza es lo que el conversor sacó hacia fuera, y una red de
profundidad toma el borde de una letra por relieve de verdad, así que el texto acaba flotando un par de
milímetros delante del panel. Una vez que flota, la ventana de la trama lo lleva de un lado a otro: con
15 cm de cabeza y 2 cm de relieve el recorrido son ~30 puntos del panel, unos 5 mm. El visor de Windows
y el Odyssey 3D Hub ponen el texto en el plano del panel, y por eso ahí no se despega.

Lo que hace el visor ahora: cada vez que la red saca el mapa, `anclarTexto()` (`src/depth.cpp`) mira la
**misma foto** que lo generó, busca las zonas que son rótulo y lleva su valor al plano de apoyo del
propio mapa. Ahí el desplazamiento izquierda/derecha vale cero, las dos vistas salen iguales y el rótulo
queda soldado al cristal; el resto del relieve sigue donde lo puso la red.

Cómo reconoce un rótulo sin leerlo: rejilla de 8×8 puntos (≈65×65), y por celda mide el contraste, la
parte de píxeles con borde, la parte sin borde, la finura del detalle (un trazo de letra deja un
laplaciano fino y antialiasing) y la parte de área de trazo; y sobre todo las **medias de la vecindad**,
que es donde se ve si el detalle está suelto sobre fondo despejado (letras) o si lo llena todo (una tela,
un matorral, una grava): midiendo celda por celda las dos cosas son idénticas. En la franja de abajo
(subtítulos) y en la de arriba (cintillos y relojes) se bajan los umbrales. En las fotos de prueba
(`../pruebas-texto/`): subtítulo blanco sobre fondo liso 10 % de la rejilla, las dos líneas enteras;
cintillo blanco sobre banda roja 2,9 %; matorral de detalle fino, matorral de detalle grueso y fondo
liso, 0 % en los tres, así que la textura conserva su relieve.

Se prueba en marcha con la tecla `R`: el visor imprime
`[rotulos] apoyados en la pantalla: 10,0% del mapa 518 x 518` y al soltarlos
`[rotulos] sueltos: cada rotulo con la profundidad que le dio la red`. Si al ver una película el texto
se ve hundido o el relieve de una zona con mucha letra se corta, suéltalos con `R`.

Para ajustar los umbrales sin sentarse delante del panel (con una foto del `--volcado`, o cualquiera):

```
odysseyvk texto-detect foto.ppm                                  # que cree que es texto, en ASCII
odysseyvk texto-detect foto.ppm --mapa mapa.pgm --plano 0.45     # y como queda el mapa
ODYSSEYVK_TEXTO_CONTRASTE=45 ODYSSEYVK_TEXTO_FINO=0.20 odysseyvk texto-detect foto.ppm
```

Con `--mapa` escribe `mapa-apoyado.pgm` y dice cuántos puntos movió y cuánto; sin él sólo imprime la
rejilla, fila por fila. El apoyo cuesta unos 15 ms en el hilo de la red, así que no corta el vídeo.

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

### La pastilla vuelve siempre (tecla `PB`, desde v0.23)

* **`PB`** (la tecla Pausa/Break): la pastilla a la vista aunque estuviera ocultada, ventana devuelta si el
  gestor de ventanas la minimizó, y el foco pedido para el visor.
* **Un rincón que siempre se pica.** Con la pastilla ocultada la región de entrada quedaba vacía, así que el
  puntero no tenía forma de llamarla de vuelta. Ahora queda la punta derecha de su sitio de siempre
  (170 × 84 px de ventana) y en cuanto el puntero entra ahí la pastilla reaparece sola
  (`[pastilla] el puntero llego al rincon de arriba a la derecha: la pastilla vuelve`).
* **El servicio vigila la ventana.** Si hay una sesión abierta y su ventana aparece minimizada sin que `Tab`
  la haya entregado, el servicio le ordena al compositor que la devuelva
  (`[ventana] el gestor minimizo el visor: lo devolvemos`). El visor anuncia sus minimizaciones voluntarias
  escribiendo `~/.local/state/odysseyvk/entregada` con su número de proceso —mientras ese proceso viva, el
  servicio deja la ventana escondida todo el tiempo que quieras—. Esto es lo que corta el caso de «la imagen se
  esfumó y el monitor se quedó en malla 3D»: la trama vuelve, o se sale con `ESC` y queda todo restaurado.

Si el foco lo tiene el vídeo, `PB` no le llega al visor (el teclado se lo queda la ventana activa). Para que la
tecla funcione desde cualquier ventana, ata un atajo global: **Ajustes del sistema → Atajos de teclado → Añadir
nueva… → acción personalizada**, con nombre «traer la pastilla», comando
`/usr/local/bin/odysseyvk ctl aparece`, y como disparador pulsar `Pausa`. Este Protocolo de KWin no trae orden
de activar ventanas, así que la ventana vuelve a la vista pero sin el foco; un clic en la pastilla devuelve el
mando al visor. Por consola se prueba con `odysseyvk ctl aparece`, y se puede jugar con el estado de cualquier
ventana en `ctl ventana [minimizar|aparecer|sobre|debajo|libre|salida DP-4] --uuid …`.

### El visor vuelve delante del vídeo y sin gráficos rotos (desde v0.24)

Devolver la ventana a la vista no bastaba: KWin la dejaba **detrás del vídeo** y achicada, con la trama llena de
trozos sueltos. Son dos cosas distintas, y cada una tiene su arreglo.

* **Detrás del vídeo.** En el protocolo de ventanas de Plasma no existe la petición «traer al frente», y al
  quitarle la minimizada KWin no activa la ventana: vuelve con el orden que tenía y el vídeo —activo y a
  pantalla completa— queda encima. Desde v0.24 el servicio pide también `KeepAbove` y lo reafirma cada 5 s si el
  compositor lo pierde. Medido con `ctl apilamiento`: la lista del compositor va de abajo hacia arriba, y con
  `Encima` la ventana salta al último puesto.
* **Los gráficos rotos.** GLFW no se entera en Wayland de que la ventana fue minimizada, así que el visor seguía
  presentando cuadros contra un intercambio sin superficie; al volver, la ventana estaba achicada y en otra
  salida (en el registro: 3840×2160 → 2560×1440 → 2015×869) y la trama dejaba de ser pixel a pixel. El servicio
  escribe `~/.local/state/odysseyvk/ventana` con `oculta`/`visible` y su reloj; con `oculta` el visor **deja de
  trazar**, y al volver pide la pantalla completa en su salida,
  rehace el intercambio y rearma la captura. El panel solo se devuelve a 2D si el servicio dejó de escribir la
  marca (hub caído); mientras el servicio respire, la ventana vuelve sola y el monitor no parpadea 3D→2D→3D.
* **La salida también se pide.** `ctl aparece` y el servicio mandan la ventana del visor a la salida del panel
  (`send_to_output`, con los nombres y las posiciones lógicas que da `zxdg_output_v1`) y vigilan la ventana del
  vídeo: si pierde la pantalla completa o sale del rectángulo de esa salida, la devuelven
  (`[ventana] el video (…) lo devolvemos a pantalla completa en DP-4`).

* **Y sin quedarse sin vídeo (desde v0.25).** Al rearmar la captura se quedaba encendida la bandera de «flujo de PipeWire ya
  conectado», así que el flujo nuevo nunca se creaba: la pastilla volvía pero con el último cuadro congelado, y los
  reintentos no servían. `stop()` ahora limpia esa bandera y el nombre/serial del nodo anterior. Medido: con el visor
  clavado, `ctl flujo --uuid <video>` seguía midiendo **65,6 fotogramas por segundo** en esa ventana —el compositor sí
  repintaba—, y tras el arreglo el conteo del visor siguió subiendo (797 → 968 → 1217 → 1366) a través de tres
  minimizados seguidos, sin un solo aviso de corte.

Para ver lo que ve el compositor, sin adivinar: `ctl salidas` (nombres y rectángulos lógicos) y
`ctl apilamiento` (el orden de arriba y abajo).


## Servicio que arranca con la sesión

`install.sh` activa una unidad de **systemd de usuario** que se levanta con la sesión gráfica y se
queda esperando (sin ventana, sin consumir GPU):

```sh
systemctl --user status odysseyvk-serve      # debe decir "active (running)"
journalctl --user -u odysseyvk-serve -f      # ver lo que hace en vivo
systemctl --user stop odysseyvk-serve        # pausarlo
systemctl --user edit odysseyvk-serve        # cambiarlo si hace falta
```

Qué hace: escucha `org_kde_plasma_window_management` de KWin; cuando una ventana pasa a **pantalla
completa** dibuja en la esquina una **tarjeta propia** (la misma interfaz del visor) con dos teselas
(vídeo normal y vídeo dividido; ver «La interfaz»).
Va en una capa `overlay` de `zwlr_layer_shell_v1`, así que **se ve por encima del vídeo a pantalla
completa**, cosa que las notificaciones de KDE no logran (KWin las dibuja detrás, y con «No molestar»
ni siquiera aparecen). Si el compositor no expone ese protocolo se vuelve al aviso de KDE. Al pulsar un
botón lanza
`odysseyvk capture3d --uuid …`, que pone lente + scaler + modo del panel y trama en 3D el
fotograma que captura el propio compositor (**DMA-BUF** de KWin → imagen Vulkan, sin copia por
CPU). El botón **2D plano no lanza nada**: deja el vídeo tal cual (sin lente, sin scaler y sin cambiar
el modo del panel) y, si había una sesión 3D abierta, la cierra devolviendo el panel a su modo normal.
Irse del vídeo es la forma normal de apagar la sesión: si la ventana del vídeo se cierra, se minimiza o
deja de estar a pantalla completa, el servicio avisa al visor para que se vaya por la puerta de adelante
—devuelve el panel a su modo normal, apaga la lente y el scaler— y la pastilla deja de quedar detrás de
la ventana activa; tarda unos tres segundos. Al cambiar el modo del monitor KWin deja a la ventana del
vídeo con el tamaño que tenía en 2D —queda chica dentro del lienzo y la trama se ve mal—, así que el
servicio se lo vuelve a dar en cuanto arranca la sesión (en el registro: `[ventana] el cambio de modo
dejó al video sin cubrir el lienzo … lo reacomodamos`). Mientras dura la sesión el servicio además deja
la ventana del vídeo **por debajo** del visor: KWin sube al frente la ventana a pantalla completa que
recibe el foco, así que un clic en la barra de tiempos de YouTube volvía a poner el vídeo encima de la
trama y la pantalla se veía otra vez en 2D (`ctl apilamiento` lo comprueba: el visor arriba, el vídeo
abajo). Al terminar la sesión le quita esa marca y el vídeo vuelve a su sitio. Si el visor se muere de
golpe (o se detiene el servicio), el servicio recupera el panel con la copia que quedó guardada.

Se configura en `~/.config/odysseyvk/serve.conf`: `ignore` (aplicaciones que nunca se ofrecen; nuestra
propia ventana entra siempre en la lista, con el nombre nuevo y con el viejo, para que el vigilante no
se ofrezca 3D a sí mismo),
`espera` (segundos que queda la tarjeta), `modo` (`ask`, `sbs`, `tb`, `flat`, `conv`), `red` (en qué
GPU corre la red del conversor), `cada` (cada cuántos fotogramas se calcula la profundidad),
`rastreo` (`si` para que la cámara IR del monitor siga tus ojos y la trama se recalcule; `no` para la
posición nominal de fábrica) y `suavizado` (`0,35`: cuánto se amortigua ese seguimiento; más pequeño
responde antes y se mueve más; desde v0.60 solo cuenta con el One-Euro apagado, ver abajo). Para la cámara: `camara` (`/dev/video0`, vacío = elegir la mejor),
`mono` (`auto`, `si`, `no`; abajo), `campo` (campo horizontal de la webcam en grados, por defecto 65)
y `cara` (ancho de cara de oreja a oreja en milímetros, por defecto 150); para el conversor también
`texto` (`no` deja los rótulos con la profundidad que les dé la red en vez de apoyarlos en la pantalla). Las dos
redes de profundidad se eligen con `modelo` (botón **1**, la de Samsung: cualquier `.onnx` llamada
`mde_*` o `disp_*`) y `nuestra` (botón **2**, la nuestra: `depth_anything_v2_small.onnx`); en el visor
se cambia de una a otra con las teclas sueltas `1` y `2` sin cerrar nada (el botón de la pastilla salió en v0.78),
y la elección queda en `~/.config/odysseyvk/red-elegida`, aunque **al arrancar se vuelve siempre a la
de Samsung** (desde v0.34): el archivo recuerda tu último cambio, pero el arranque empieza por la 1.

Y para la trama, cuatro claves más (desde v0.40), que son lo mismo que se mueve en marcha con las
teclas pero queda guardado para todas las sesiones: `fase` (ciclos, teclas `←` `→`), `paso`
(multiplicador del paso de lente, teclas `RePag` `SavPg`; `1,004` es un +0,4 %), `filtro`
(pendiente del filtro, teclas `,` `.`; sin esta clave se usa la de fábrica, 10) y `calib` (carpeta
con los `3DStackCorrection_A/B.png` de **este** panel). Además `volcado = /tmp/chk` hace que cada
sesión guarde `/tmp/chk-trama-vulkan.png`, `/tmp/chk-trama-con-mapa.png`,
`/tmp/chk-trama-sin-releve.png`, `/tmp/chk-profundidad.pgm` y `/tmp/chk-chico.ppm` sin cerrarse,
que es como se miden las franjas de arriba.

Y seis para medir sin recompilar (los cuatro primeros desde v0.41), porque ninguna teclas las alcanza: `relieve`
(escala del relieve; `0` deja la imagen **plana del todo**, cosa que `3D Salida = 0` no hace porque
allí quedan 4 px de separación entre ojos), `temblor` (pixeles del temblor del desplazamiento,
0,25 de fábrica; `0` lo apaga para ver las rayitas sin disimulo), `canto` (ancho del puente del
canto, 0,004 de fábrica; subirlo deja los bordes sin mordeduras pero les quita relieve) y
`fase_temblor` (temblor de la propia fase de la trama, en ciclos; `0,1` baja unas cinco veces las
armónicas de 4,3 y 8,2 px y casi no toca el 3D). Estas cuatro se aplican al reiniciar el servicio,
no en caliente.

Las otras dos son `rampa_x` y `rampa_y` (desde v0.42): aprietan o aflojan el **paso de la rejilla que
escribimos**, en ciclos de porteadora por pixel de panel. Sirven para cuadrar nuestro paso con el de
la lente de este ejemplar: medido sobre el cuadro escrito, el nuestro salía un 0,6 % más largo
(2,2358 px frente a los 2,2217 px que escribe Windows), y dos tramas con pasos que difieren un 0,6 %
baten a 353 px de panel, o sea **55 mm**: esas son las franjas verticales gruesas. Con
`rampa_x = -0,00283` queda clavado (2,2219 px = 0,34529 mm) sin tocar la inclinación ni el color.
**No es lo mismo que `paso`**: `paso` escala la rampa de la lente entera, y en esa rampa van metidos
los desfases entre los canales rojo, verde y azul, así que desplaza el color y afloja la inclinación;
por eso `paso` se queda en 1. `tools/paso_rejilla.py` mide el paso escrito y propone el valor de la
rampa, y el servicio imprime al arrancar los números de trama que cargó.

Las dos últimas (v0.45) son `fondo` y `frente`: **cuánto se estira el relieve a cada lado del plano
de apoyo**. `fondo = 1,4` atrasa el fondo un 40 % más sin mover los primeros planos; `frente = 1,3`
acerca lo que ya está fuera de la pantalla. Los dos en `1` (o sin escribir la línea) es el
comportamiento anterior, píxel por píxel. Es la llave para «mandar algo al frente o al fondo» que
antes no existía: el relieve solo tenía una escala para toda la imagen. En el conversor de Windows
esto viene por dentro, en las bandas de su configuración descifrada (`near_diff_scale −0,5`,
`far_diff_scale 1,0`, `far_diff_popup −1,0` en `scene_dspr`, y los signos `dspr_scale(−)` /
`dspr_scale(+)` en `camera`); aquí lo exponemos suelto porque en el Hub de Windows no aparece. El
escalado se aplica **antes** de los topes por ojo de Samsung (+12,67 puntos hacia afuera y 9,98 hacia
adentro sobre 3840), así que por mucho que se suba no nos salimos del presupuesto que su programa
considera sano. En marcha se ajustan con `j`/`k` y `u`/`i`; por escrito se dejan para comparar una
tarde entera sin tocar teclas.

Y en v0.46, el **empuje** de cada capa: `empuje_fondo` y `empuje_frente`, en puntos de panel
(positivo = hacia fuera de la pantalla). Estirar la escala solo mueve lo que ya tiene relieve, así que
un objeto casi plano no se entera; el empuje le da un desplazamiento fijo a la banda entera y permite
llevarla a un plano concreto aunque el plano de apoyo esté en otro sitio — que es exactamente lo que
hace falta para poner subtítulos, banderoles y ventanas por delante del vídeo, o para hundir el fondo
de un plano entero. En la config de Windows eso es `far_diff_popup −1,0` en `scene_dspr` y
`depth_offset 0,0` en la capa `overlay` de `degraph`. Se interpola en una zona suave de 1,5 puntos
alrededor del plano de apoyo, así que un objeto que lo cruza no se parte; y vuelve a pasar por los
topes por ojo, que siguen mandando. Teclas `h`/`l` (fondo) y `y`/`o` (frente), de 0,5 en 0,5, tope
±12 puntos.

Y en v0.48, **`planos`**: la dureza con la que el conversor cierra el mapa en **dos láminas planas**
en vez de dejarlo continuo (`0` … `1`; sin escribir la línea, o `0`, es el comportamiento de siempre,
píxel por píxel). `0,3` es la suave: las dos capas se insinúan sin cortar los bordes, y en deportes,
series con desenfondo y cualquier mapa manchado se ve mucho más limpio que el continuo; `1`
son dos láminas perfectamente planas. El lugar del corte lo saca el propio cuadro y se persigue en el
tiempo, así que no hace falta afinar ningún umbral; subiendo o bajando la dureza (`g`/`v`) basta. Se
escribe igual que los demás números de la config, con coma o con punto (`planos = 0,3`), y en marcha es
lo mismo con `g`/`v`.

Y en v0.51, **`devolver_fullscreen`**: `si` (así) o `no`. Con `si`, cuando la sesión 3D se cerró
**porque el vídeo se fue** —la ventana del vídeo salió de pantalla completa, o el navegador dijo
«Parado» / «sin reproductor» por MPRIS— se le quita la pantalla completa a **esa ventana** del navegador,
para que el escritorio no se quede ocupado por una página a pantalla completa sin nada que ver. Ojo con el
orden: en el momento en que cerramos la sesión la ventana **ya está** en su tamaño normal (por eso la
cerramos) y **un segundo después** la página se la vuelve a poner —medido con Firefox y con mpv—, así que el
vigía se queda mirando **hasta 20 segundos** a ver si se la reponen; si se la pone, se la quita, con pausas
de 1,2 s y **tope de 3 intentos**. Si aun así la página sigue insistiendo, se avisa **una sola vez** en el
registro y se deja al navegador en paz. **Durante una sesión no se toca nada**, y tampoco cuando la sesión se cerró por
elección (`2D plano`, `Tab`, `ESC`): ahí el vídeo sigue sonando y la pantalla completa es tuya. Vale para
cualquier navegador que se ponga a pantalla completa (Firefox, Chrome, Zen, Brave, mpv).

Y en v0.50, **`muestras`**: cuántas **muestras intermedias** se ponen entre las dos láminas (`0` … `3`; sin
escribir la línea, o `0`, es la rampa continua de v0.48). Con `muestras = 1` el centro del cuadro tiene su
propio nivel, clavado a mitad de camino entre el fondo y los primeros planos, con meseta: la porción del medio
se ve **sólida** en vez de degradada, y el paso entre niveles sigue siendo suave para que el canto no se corte.
Con `2` o `3` hay dos o tres niveles intermedios. En marcha se prueba al momento con `n` (agregar) y `x`
(quitar), y se combina con la dureza: con `planos = 0,3` la meseta solo se insinúa, con `planos = 0,6` o más ya
se planta. Se escribe con coma o con punto, como los demás números (`muestras = 1`).

Y en v0.49, **`video_parado`**: cuántos segundos tiene que decir el navegador «Parado» para que se quite
la trama (segundos; `4` sin escribir la línea, `0` lo apaga). Es la salida de un vídeo de YouTube: la
pantalla completa la sostiene **la página**, no el vídeo, así que al irse del vídeo la ventana de Firefox
sigue a pantalla completa y por la ventana no hay manera de enterarse de que ya no hay nada que tramar.
Firefox (y Chrome, Zen, Brave) publican el estado del vídeo por MPRIS, y con eso basta: «Parado» unos
segundos, o el reproductor que desaparece del bus, cierran la sesión por la puerta de adelante. **«Pausado»
no cierra nada**, porque una pausa es una pausa y ahí la trama se queda viendo. El vigía solo escucha al
reproductor del propio navegador, así que un reproductor de música detenido no apaga la sesión.

**El sitio de las salidas también se cuida** (v0.49). `Panel` lee la `Geometry:` que reporta kscreen y, si
al poner el panel a escala 1 su área lógica crece encima del monitor de al lado, ese monitor **se mueve en la
misma orden atómica** del cambio de modo y se devuelve al salir (`[panel] DP-7 pasa a 3840,624` / `[panel]
DP-7 devuelta a 2560,624`). Sin esto la zona derecha del panel se veía duplicada en las dos pantallas y KWin
le cambiaba el tamaño a la ventana del vídeo a media sesión. `ctl panel status` imprime el sitio de cada
salida y `ctl panel apply --dry` imprime el comando completo, con las posiciones incluidas. Si no quieres que
se mueva nada: `--sin-despejar` en el visor o `ODYSSEYVK_LAYOUT=0`. En este equipo el panel ya quedó **a
escala 1 de forma permanente** y el otro monitor a la derecha, así que en sesión normal no cambia nada.

**Desde la v0.76 el 3D no cambia la escala del panel.** Antes lo pasaba a escala 1 y lo devolvía al
salir; KWin guarda cómo estaba cada ventana en cada configuración de pantallas y se lo repone al volver
a ella, así que al entrar sacaba al vídeo de pantalla completa y al salir se la volvía a poner. No hacía
falta: con el escritorio a 1,5 la ventana del visor mide 2560×1440 lógicos y su lienzo son los 3840×2160
puntos del panel, y la ventana del vídeo llega igual a 3840×2160. `escala_3d = 1` en `serve.conf` vuelve
a lo de antes, por si la trama no saliera punto a punto con la escala del escritorio.

**Los mapas de corrección son de fábrica por unidad.** Cada G90XF tiene los suyos, escritos en su
línea de montaje, y desde fuera son idénticos (480×270, un PNG en escala de grises). Los que traía
el instalador eran de otra unidad: midiendo, la media del mapa A coincidía más o menos (31 frente
a 23 niveles) pero la correlación era 0,39 y había zonas con 237 niveles de diferencia —0,93
ciclos de fase, casi un periodo entero de trama en una esquina—. Ahora el visor busca
`~/.config/odysseyvk/calib` **antes** que los instalados, y en la salida dice de dónde salieron y
cómo son:

```
[correccion] de /home/rodolfo/.config/odysseyvk/calib/3DStackCorrection_A.png y …B.png
[correccion] mapas 480x270 y 480x270
[correccion] mapa A: 0..255 media 23,5
[correccion] mapa B: 117..136 media 127,2
```

Si esos números no coinciden con los de tu unidad, tienes los mapas equivocados: saque los tuyos
en Windows de `C:\ProgramData\Simulated Reality\Devices\` y ponlos en `~/.config/odysseyvk/calib/`.

**El botón 1 con la gráfica justa** (arreglado en v0.33): la red de Samsung **solo** arranca en la
gráfica. Pedirla por CPU no llega a crear la sesión —medido el 24-nov con `mde_large.onnx`: más de
**4 minutos** quemando CPU sin acabar, y en el visor acababa en `SIGABRT`—, así que pulsar `1` con la
VRAM llena mataba el visor y dejaba el panel en 3D sin vídeo. Ahora, si en ninguna tarjeta hay
1550 MB libres (1100 MB que pide la red + 450 de colchón), la red de Samsung se rechaza con un aviso
claro y **sigue funcionando la red anterior**; al arrancar, si la red pedida no abre se prueba la
nuestra, que sí va por CPU (1 s en abrir y 190 ms por mapa). Para experimentar: `ODYSSEYVK_GPU=-1`
fuerza la CPU y `ODYSSEYVK_GPU=1` obliga a la segunda tarjeta.

Si se
marca la casilla de la tarjeta y se pulsa un botón, se guarda una línea `auto = app:modo` en ese
archivo y desde entonces esa aplicación pasa directa sin preguntar. Se deshace **desde dentro del
vídeo**, desmarcando «No volver a preguntar» en la pastilla (v0.78), o borrando esa línea a mano. `conv` es el vídeo normal pasado a 3D: con `auto = zen:conv` el navegador entra directo en
conversión. Si no aparece la
tarjeta:

1. `systemctl --user is-active odysseyvk-serve` → tiene que estar `active`;
2. la ventana tiene que ser **pantalla completa de verdad** (no maximizada) y su `app_id` no debe
   estar en `ignore` (los reproductores bajo XWayland aparecen como `xwayland` y están ignorados
   por defecto: quítalos de la lista si quieres usarlos);
3. en `journalctl` debe aparecer `[serve] tarjeta puesta por <app_id> | <título>` y luego
   `[serve] tarjeta -> <modo>`; para probar la tarjeta sin ningún vídeo:
   `odysseyvk ctl overlay --segundos 8` (acepta `--dump CARDEN.png` para volcarla a un archivo).

Si durante una sesión **la imagen se queda congelada**, el visor lo dice en el registro y eso permite
saber de quién es la culpa:

* `[captura] N s sin fotogramas nuevos del compositor (flujo: streaming)` + la ventana en buen estado
  → el navegador/reproductor dejó de pintar (suele pasar con vídeos en línea: vuelva a darle play);
* `(flujo: paused …)` o `el compositor cerró el flujo` → se cortó la captura; el visor reintenta el
  flujo solo hasta cuatro veces;
* `la ventana del video cerro` → la sesión se cierra y el panel vuelve a 2D.

Para mirar el flujo sin abrir nada encima: `odysseyvk ctl flujo --uuid <id> --segundos 10` imprime
cuántos fotogramas manda el compositor cada segundo (un vídeo normal da 24-25).

> KWin 6.8 sólo expone el portal propio de KDE (`zkde_screencast_unstable_v1`, versión 6). Es lo
> que usa el port; no hay Wlr-Fragment-Capture ni ext-image-copy-capture en este compositor.

## Limitaciones y diferencias con macdissey-3d

* Capturar **otra aplicación** sólo es posible por el portal de captura de KDE
  (`zkde_screencast_unstable_v1` + PipeWire), que es inestable entre versiones de Plasma: si
  KWin cambia el interfaz, `serve` deja de ver los fotogramas (lo avisa
  `journalctl --user -u odysseyvk-serve`). El reproductor interno (`play`, basado en libmpv) sigue
  siendo la ruta más directa; mpv por ahora se dibuja con OpenGL aunque su fotograma se trama en
  Vulkan.
* El **seguimiento de mirada está activo** si en `serve.conf` pone `rastreo = si` (el hub lo avisa al
  arrancar: `rastreo activo (la camara IR sigue los ojos)`).
  La pastilla del overlay lo dice desde v0.54 sin ofrecer nada a pulsar: la palabra `Facetracking` y al lado
  las dos pastillitas `3D Studio` y `fallback`; la que está alimentando el rastreo va tintada y con un punto,
  la otra en gris. `3D Studio` es la cámara integrada del monitor (USB `04e8:20d7`, "3D Stereo WebCam": dos
  vistas en un cuadro de 1280x480 MJPG @ 60, la distancia sale de las dos vistas); `fallback` es la webcam
  aparte, que mide la distancia por el ancho de la cara. Debajo sale la nota con el modo y si hay cara. **No
  hay botón ni tecla para apagarlo** (se quitó el botón `Rastreo` y la tecla `T`): si `serve.conf` dice
  `rastreo = si` y la cámara abre, manda la cámara; si no abre, la fila lo dice y la trama va con la pose de
  fábrica. Sin él, la trama se calcula para la
  posición de ojo nominal (los valores de fábrica del G90XF), igual que el modo "static" del port
  X11, y por eso el punto dulce es la posición de trabajo normal. Mover la cabeza en horizontal da un
  cambio de fase que puedes seguir con `←`/`→`.
  El rastreo está implementado (`--track`, `ctl track`: cámara IR + YuNet + triangulación con la
  calibración de fábrica que lee `SR+CALCAM`), pero necesita dos cosas de la cámara:
  * **la calibración en disco**. Si falta, el log dice `calibracion: sin SR+CALCAM.bin (buscado en …)`
    y no llega ningún fotograma (`0/0 cuadros`), aunque la cámara esté bien y se haya abierto en el
    formato correcto. Se saca del propio monitor con `odysseyvk ctl calib` (habla con `/dev/ttyACM0`, no
    toca la pantalla) y deja `SR+CALCAM.bin`, `SR+CALET.bin` y `SR+CAL3D.bin`. Son **de fábrica por
    unidad**, así que el sitio recomendado es `~/.config/odysseyvk/calib` —el mismo de los mapas de
    corrección—; el de sistema (`/usr/local/share/odysseyvk/calib`) solo sirve para enseñar la forma.
    Desde v0.57 esa carpeta se busca también para la calibración de la cámara: antes se buscaba junto al
    binario y en rutas relativas al directorio actual, así que las sesiones lanzadas por el servicio se
    quedaban sin rastreo (medido: `0/0 cuadros` en una sesión de 95 s lanzada por `odysseyvk-serve`,
    mientras lanzado desde el repo llegaba a `59,8 fps`). El orden de búsqueda es
    `~/.config/odysseyvk/calib`, `~/.config/odyssey3d/calib`, el directorio actual, junto al binario y
    `/usr/local/share/odysseyvk/calib`.
  * que le contesten los controles UVC: si `ctl camera` falla con `VIDIOC_S_FMT ... Input/output
    error`, en `dmesg` verás `Failed to set UVC probe control : -110`: la interfaz de flujo de la
    cámara dejó de responder a los controles UVC. Suele empezar por una suspensión USB (el kernel la
    suspenda a los 2 s y el resume no llega detras de los hubs del cable del monitor), por eso la regla
    `udev/70-odysseyvk.rules` pone `power/control=on` en la cámara y en la lente. `odysseyvk doctor`
    y `ctl camera` dicen el estado (`control=... estado=...`).
    Desde v0.44 el hilo de rastreo **reabre el dispositivo solo** si dejan de llegar fotogramas durante
    3 s, y suelta el flujo si no puede arrancar (un flujo montado y sin lector es lo que deja el
    `S_FMT` colgado para la sesión siguiente). Nunca mandamos un reset del bus USB, porque eso deja la
    cámara muerta de verdad:
  * una vez la cámara ya no enumera (`device descriptor read/64, error -110`) no hay reset por
    software que la levante: hay que **apagar y encender el monitor** (o desenchufar su USB-B).
    Al encender se pierde la autenticación de la lente, que el programa vuelve a hacer solo.
* En Windows el lazo lo cierra el software de Samsung con otra cadencia: las cámaras IR a 60-90 fps
  y su rastreador publicando poses a hasta 240 Hz, con el entrechado **adelantado 15,6 ms** para
  compensar los 20,7 ms de retardo de la cámara (`configs/leiasr/tracker-ft_user.ini` en el análisis
  de esa instalación). Aquí el rastreo va a unas decenas de Hz con mediana de 3 + filtro **One-Euro**
  (v0.60: corte mínimo 0,6 Hz, β 0,01; `ODYSSEYVK_EURO=0` vuelve a la media móvil de `--track-suavizado`,
  `ODYSSEYVK_EURO="0,6;0,01"` cambia los dos) y sin adelantar nada, así que el ajuste se nota más tarde.
  El One-Euro se eligió con `tools/prueba_euro.cpp`: a 60 cm la fase de la trama tiembla 0,015 ciclos en
  el centro y 0,033 en el borde (antes 0,034 y 0,066), contra un margen de ±0,04 antes de que se cuele
  el subpíxel rojo o el azul en el ojo que no toca.
* Conversor 2D→3D, llaves de `serve.conf` para la profundidad (v0.64–v0.68; todas tienen su variable
  `ODYSSEYVK_…` y su entrada en `CAMBIOS.md`): `relieve` (escala de la ganancia; 1,2 ≈ el Hub),
  `saliencia = si|no` (elegir el sujeto como el Hub; `si` por defecto), `pivote` (0–0,9: cuánto sale el
  sujeto de la pantalla; 0 = en la pantalla), `relieve_local` (realce del relieve dentro de los objetos;
  rinde poco), `paralaje` (ganancia del paralaje al mover la cabeza; 0 = apagado, negativo = invertido),
  `detalle` (realce de nitidez del sujeto; 0,4 de fábrica), `foco_iir` (lo que tarda el plano en seguir
  al sujeto). Para medir fotos sueltas: `odysseyvk mapa FOTOS… --sueltas`.
* Conversor 2D→3D: desde v0.89 van los **cantos finos** (ver arriba); lo que sigue vale para `cantos = viejo`.
  Desde v0.62 el mapa se agranda **guiado por la imagen** (el `jbu` de Windows) en vez de
  con la cúbica, para que los cantos no salgan en peine. `ODYSSEYVK_JBU=0` vuelve a lo de antes;
  `ODYSSEYVK_JBU_COLOR` (0,06) cambia cuánto tiene que parecerse el color. Para probarlo sin video:
  `ODYSSEYVK_FUENTE_PNG=cuadro.png odysseyvk test ... --mapa-fijo mapa.pgm --dump salida` (ver CAMBIOS v0.62).
  Cuando la cámara vuelva a funcionar se mide la cadencia real (`cameraFps`, `latencyMs`) y se ajusta.
* El panel debe quedar a **escala 1 y rotación normal** durante el 3D (la trama necesita pixel a
  pixel); el programa lo cambia y lo devuelve. Si tu escritorio usa escala 1.5, al salir
  volverá a 1.5.
* `kscreen-doctor` de Plasma 6 no expone VRR: si lo tienes activado, desactívalo en
  Ajustes → Pantalla antes del 3D (el entrelazado con tasa variable produce bandas).
* **El modo de legado (v0.57) solo existe en la ruta Vulkan.** Con `--gl` la bandera `--legado` se
  acepta y no hace nada; la hoja *Edición avanzada* de la ventana tampoco se apaga ahí, porque el
  visor OpenGL no escribe `legado` en `estado`. Queda pendiente, igual que los **16 formatos estéreo**
  que usa el cliente de referencia (aquí seguimos con lado a lado, arriba y abajo y 2D plano) y las
  curvas exactas con las que aquel cliente convierte el número de su interfaz en uniformes. Lo que sí
  está copiado y medido: los uniformes fijos `r = 0,0128535`, `m = 0,05`, `s = 10`
  (`Sources/OdysseyGL/Renderer.cpp:68-71`) y el par `minimumDepth: 200` / `maximumError: 40` al
  triangular la cara (`Sources/OdysseyCore/StereoTracking.swift:82`). Y una diferencia **deliberada**:
  aquí el modo de legado **sí** sigue mandando el comando del scaler del monitor, porque en Linux sin
  ese comando el panel no entra en 3D.
* La autenticación de la lente usa la clave fija del firmware y **persiste hasta apagar el
  monitor**, así que `ctl lens auth` basta una vez por sesión de encendido.

## Diagnóstico

```sh
odysseyvk doctor                      # qué ve, qué toca, qué falta
odysseyvk weave [--solid] [--out f.png]  # escribe un PNG con la trama (no toca el panel)
odysseyvk attrs                       # atributos de la trama (para depurar el modelo)
odysseyvk ctl hud --w 1920 --h 1080   # prueba la interfaz sin abrir ventana ( atlas, raton )
odysseyvk ctl estado                  # qué red, qué tarjeta y qué números está usando el visor vivo
odysseyvk ctl ajuste fase 0,004       # mandarle un número al visor sin tocar el panel
odysseyvk ctl preferencias            # la cuenta de lo que se fue cambiando (preferencias.log)
odysseyvk ctl escritorio estado       # qué sesión tiene el vigilante: ninguna, de vídeo o del escritorio
odysseyvk ctl capa estado             # pastilla puesta u oculta; `ctl 3d estado`, la trama
odysseyvk ctl capture-gl --out p.png  # captura real de la ventana, para ver la interfaz sin mirar la pantalla
odysseyvk-ajustes --prueba            # la ventana sin pantalla: redes con su marca, estado, vigilante, camaras y su vista
odysseyvk ctl vkprobe                 # importa el fotograma de KWin y lo dibuja tal cual en Vulkan
odysseyvk play clip.mp4 --windowed --no-panel --no-lens --no-scaler --dump chk
                                      # guarda chk-trama.png y chk-origen.png y sale
```

`--dump` es útil para ver el entrelazado sin el hardware: `chk-trama.png` es literalmente lo que
se manda al panel; visto de cerca deben apreciarse las franjas diagonales (paso 1.8 px con la
inclinación nominal de 0.29).

### El visor sale gris parejo, sin imagen

Con `--dump` el visor comprueba solo su cuadro trameado: si casi todo sale del mismo valor avisa
`[aviso] el tramado pintó una imagen plana: 98% de los puntos estan en 125,5` y guarda al lado
`chk-trama-cruda.png`, que es la MISMA textura sin tramar. Con la cruda bien y la trameada gris, el
problema son los números de la trama (contraste, pendiente del filtro o un número loco), no la
captura ni la red. Para verlo sin tocar el panel:

```sh
odysseyvk test --windowed --no-panel --no-lens --no-scaler --dump /tmp/prueba
```

Si aparece `[aviso] algo pise los numeros de la trama entre cuadros` hay una escritura fuera de su
sitio: di qué campo dice el aviso y se busca ahí. Para seguir el paso del cálculo dentro del
sombreador, `ODYSSEYVK_PRUEBA=1..11` saca en claro de pantalla una pieza concreta del cálculo (1 ojo
izquierdo, 2 ojo derecho, 3 coordenada 2D, 4 peso del filtro, 5 crosstalk, 6 fase base, 7 cuadrícula,
8 esquinas, 9 correcciones y paso, 10 y 11 la mezcla); `0` o sin la variable es lo normal. Para ver
los números que salen hacia la tarjeta, `ODYSSEY_UBO=1` imprime una línea `[ubo …]` por cuadro.

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
odysseyvk ctl panel status                   # debe decir "rotacion: 1 (none)"
```

### Se ve sólo la rejilla del panel, sin el vídeo

Síntoma: entras en 3D y no aparece el vídeo, sólo la trama del panel; o al salir se ven unos
segundos horribles antes de que se cierre.

Causa, medida el 23-nov en el journal de `odysseyvk-serve`:

    [vulkan] intercambio 3840x2160 · 3 imágenes
    [conversor] no hay 1100 MB libres en ninguna grafica: la red se hara con la CPU, mas despacio
    [conversor] la grafica no dio para abrir el modelo (.../cuda_call.cc:154 ...): se intentara con la CPU

y justo después `SIGABRT` con un volcado de 298 MB. El aviso decía «se hará con la CPU» pero CUDA se
abría igual por la ruta antigua; fallada la sesión, el *reintento* abortaba dentro de
`libonnxruntime.so` (pila entera dentro de la librería). El visor moría antes de dibujar el primer
cuadro, con el panel ya en modo 3D (`[lente] estado: 3D`, `[scaler] /dev/sg1: modo 3D activado`), así
que en la pantalla sólo quedaba la rejilla. Y el hub no se enteraba hasta 22 s después: systemd-coredump
estaba recogiendo el volcado de 300 MB y por eso tardaba tanto en volver el `waitpid` que dispara la
devolución del panel.

Qué se cambió en v0.30:

* con poca VRAM libre ya no se toca CUDA (antes el aviso de «va por CPU» era mentira: se abría CUDA
  por la puerta antigua y ahí reventaba);
* el umbral de «sí cabe» lleva un colchón de 450 MB: 1,1 GB anunciados como libres no alcanzaban;
* tras un intento CUDA fallido **no se abre otra sesión en el mismo proceso** (es lo que abortaba):
  el conversor queda apagado, el vídeo se ve sin relieve y el panel se devuelve bien. La tecla `C` no
  lo vuelve a levantar en esa misma sesión; con la tarjeta más libre, la siguiente sesión ya convierte;
* `LimitCORE=0` en la unidad del servicio: sin volcado de memoria el hub se entera de la muerte del
  visor al instante y devuelve el panel, en vez de dejar la rejilla 22 segundos;
* cuando el vídeo deja de cubrir el lienzo, el hub escribe enseguida la marca de «oculto» y el visor
  deja de trazar (antes la trama se quedaba 2,5 s rayada sobre el escritorio) y la espera de cierre
  baja de 2,5 s a 1,2 s.

Para comprobarlo: `journalctl --user -u odysseyvk-serve | grep conversor` y `nvidia-smi`. Con las dos
tarjetas por debajo de ~1,6 GB libres el conversor se queda apagado a propósito.

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

Antes de tocar nada del navegador, comprobar quién está roto: `odysseyvk ctl flujo --uuid <id> --segundos 5`
mide el flujo con una conexión nueva. En KWin 6.8 esa medición dio **65,6 fotogramas por segundo** en una
ventana tapada por completo por el visor, así que si el aviso aparece con `flujo: sin estado` o
`flujo: paused` el cortado es el nuestro, no el del navegador (así estaba el caso arreglado en v0.24).

Se comprueba en dos segundos, sin abrir la capa:

```sh
odysseyvk ctl windows                                  # localiza la ventana del vídeo
odysseyvk ctl flujo --uuid '{...}' --segundos 5        # fotogramas por segundo que manda KWin
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
  `odysseyvk play archivo.mp4` usa mpv y el flujo se mantiene (60-63 f/s con el hardware en marcha).

Lo segundo que hace el navegador es **pausar el vídeo él solo** al cambiar el modo del monitor o al quedar
tapado: el flujo de KWin sigue llegando (se midieron 51 f/s) pero la imagen no avanza. Para no depender de
acertarle al botón de pausa del reproductor con el visor encima, el visor lo hace por MPRIS (Zen, Firefox y
Chrome publican un reproductor en la sesión):

- al arrancar la sesión, si el vídeo estaba parado lo reanuda solo (`[video] el navegador habia parado el
  video (Paused): se reanuda solo`);
- **`P` o espacio** pausan y reanudan el vídeo sin quitar la capa (hace falta que el visor tenga el foco:
  un clic en la pastilla se lo devuelve);
- desde una terminal, en cualquier momento: `odysseyvk ctl video status|pause|play|toggle`.

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

`odysseyvk ctl hud --w 1920 --h 1080` comprueba la interfaz sin abrir ventana: construye el atlas
de fuentes, cuenta los vértices y barre la pantalla entera de 2 px para comprobar que el ratón
golpea el botón, el deslizador o la tesela que toca, y luego mueve los deslizadores y los botones.

Trama Vulkan contra trama OpenGL, mismo patrón y mismos parámetros (`--cpu-patron` para que la
fuente sea idéntica en las dos rutas):

```sh
./build/odysseyvk weave --w 1350 --h 900 --cpu-patron --out /tmp/glc      # OpenGL
./build/odysseyvk test --windowed --no-panel --no-lens --no-scaler \
    --w 900 --h 600 --sin-interfaz --dump /tmp/vkn                        # Vulkan (leido de GPU)
python3 -c "d=[[abs(a-b) for a,b in zip(x,y)] for x,y in zip(__import__('PIL.Image',fromlist=['x']).open('/tmp/glc').convert('RGB').load(),__import__('PIL.Image',fromlist=['x']).open('/tmp/vkn-trama-vulkan.png').convert('RGB').load())]"
```

Las dos tramas coinciden (diferencia media ~10/255, concentrada en los mapas de corrección).

También se puede mirar la trama a mano: `odysseyvk weave --solid` produce una imagen 3840x2160
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
systemd/       unidad `odysseyvk-serve.service` (arranca con la sesion)
assets/        icono de la app: `icono-odysseyvk.png` (608x608) y las ocho tallas de `hicolor/`
docs/          `odyssey-3d-hub-ui.md` (la interfaz oficial analizada) y `metodo-legado.md` (el modo de legado)
config/        `serve.conf`, plantilla de ~/.config/odysseyvk/serve.conf
src/main.*     subcomandos y banderas
tools/         verificación numérica de la trama contra el oráculo
calib/         mapas de corrección del panel (3DStackCorrection_A/B.png)
udev/          regla de permisos
```

## Licencia

Del mismo modo que el port X11 del que se tomaron los protocolos y las constantes (MIT), este
código se entrega bajo MIT. El trabajo original de ingeniería inversa del protocolo de la lente
y del oráculo de trama proviene de `Fulviuus/macdissey-3d` y de `m-j-tilley/Linux_3D_SBS_Player`.
