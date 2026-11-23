# Cambios de odyssey3d

Las copias de cada versión están en `../Versiones/` (guardadas con `tools/versionar.sh ETIQUETA`).

## El navegador vuelve a pintar y se puede pausar sin quitar la capa (v0.9)
- **La preferencia que arregla el congelado de YouTube es `gfx.vsync.compositor.unobserve-count = 0`**, no la
  que decíamos antes (`widget.wayland.vsync.keep-firing-at-idle` no existe en este Zen; estaba mal escrita).
  Medido con el YouTube real del usuario, en bucle y a pantalla completa, comprobando además que la imagen se
  mueve (dos capturas separadas 1 s):
  | panel | sin nuestra capa | con nuestra capa |
  |---|---|---|
  | normal (modo 2, 164,98 Hz, escala 1,5) | 62,7 f/s | — |
  | modo 3D (119,88 Hz, escala 1) | 51,7 f/s | **30,9 f/s** |
  Antes de la preferencia, el mismo montaje con capa daba 0,1 f/s.
- **Descartada `widget.wayland.vsync.enabled = false`**: con esa preferencia el navegador del usuario pintaba
  a 0,4 f/s en el modo 3D *incluso sin nuestra capa*, así que empeoraba. Quito del aviso y del `user.js` del
  perfil (ahí queda fijada en su valor por defecto, `true`, para que no se escape).
- **El navegador también pausa el vídeo solo**, sobre todo al cambiar el modo del monitor: el flujo de KWin
  seguía a 51,7 f/s pero la imagen no avanzaba. Como nuestra ventana a pantalla completa se queda el teclado y
  el ratón, no se puede pulsar el botón de pausa del reproductor. Añadido control por **MPRIS** (`src/mpris.*`,
  con sd-bus, la librería de los avisos):
  1. al arrancar la sesión, si el vídeo estaba parado lo reanuda sola:
     `[video] el navegador habia parado el video (Paused): se reanuda solo` (verificado con YouTube: de
     `Paused` a `Playing`);
  2. **`P` o espacio** en el visor pausan y reanudan el vídeo sin quitar la capa, y la frase de estado bajo la
     pastilla dice `video en pausa`;
  3. comando nuevo `odyssey3d ctl video [status|pause|play|toggle]` para manejarlo desde una terminal.
  Reanuda solo como mucho 6 veces por sesión y nunca si el usuario pausó con `P`.
- Recordatorio en el aviso de flujo cortado: la vía rápida para el ratón sigue siendo `ESC` (cerrar la sesión
  devuelve el panel, la lente y el scaler y vuelve la tarjeta del hub).
- Para ver en 3D sin depender del navegador, la ruta sana sigue siendo `odyssey3d play ARCHIVO` (mpv): con el
  hardware en marcha se midieron 60-63 f/s.

## Señal de cierre y estado del panel (v0.8)
- **Una sesión cortada de golpe dejaba el monitor en modo 3D.** `kill`/`systemctl stop` mandan SIGTERM y
  la sesión no tenía manejador: moría a seco, con la lente encendida, el scaler en 3D y el panel en
  119,88 Hz con escala 1. Ahora `runViewerVk` y `runViewer` capturan SIGTERM/SIGINT y salen por la secuencia
  normal (`[sesion] llego una senal de cierre: se sale devolviendo el panel, la lente y el scaler`), verificado
  matando una sesión de mpv con `kill -TERM`.
- **Ese residuo envenenaba el siguiente arranque.** Como la sesión nueva memoriza el estado que hay al
  empezar, si la anterior murió sin restaurar guardaba el modo 3D como «estado normal» y al salir devolvía
  ese, dejando el escritorio a escala 1 (letra diminuta). `src/session.cpp` ahora lo detecta —panel justo en
  nuestra configuración de 3D e instantánea previa distinta— y usa el estado guardado en lugar del residuo,
  sin sobrescribir el fichero: `[aviso] el panel seguia en el modo 3D de una sesion anterior cortada: se
  devuelve el estado guardado (modo 2 3840x2160@164.98, escala 1.5, rotacion none)`.
- Nota: `odyssey3d play ARCHIVO` (ruta de mpv) sigue dibujando por el camino OpenGL (`runViewer`), mientras
  que las tarjetas del hub y `capture3d` van por Vulkan. La diferencia importa para el ratón/interfaz pero el
  entrelazado es el mismo; pasar mpv a Vulkan queda pendiente.

## El vídeo del navegador se congela debajo de la capa (YouTube): qué pasa y qué hace el programa ahora
- Medido con `odyssey3d ctl flujo --uuid ID`: con **mpv** y con vídeos locales en el navegador (1080p y
  4K60) el compositor sigue mandando 25-60 fotogramas por segundo aunque la ventana quede **tapada por
  completo** por nuestra capa a pantalla completa. Con una **página real de YouTube reproduciendo**, el
  flujo cae a ~0 fotogramas/s en unos 2 s y la imagen queda congelada hasta cerrar la capa.
- La causa está del lado del navegador: Gecko pinta al ritmo del aviso de sincronismo (`wl_callback`) que
  le da KWin. Cuando deja de recibirlos —la ventana está tapada y no hay daños— su compositor interno deja
  de producir fotogramas, y si no hay fotogramas nuevos no hay nada que mostrar. Desde fuera no se arregla:
  se probaron ventana sin bordes, dejar 1 px libre, dejar una franja libre de 24 px y que el origen estuviera
  maximizado en vez de a pantalla completa; el flujo se secaba en todos los casos.
- Arreglos nuestros en esta versión:
  1. **El reintento del flujo no volvía a conectar.** Al rehacer la captura se quedaba guardado el marcador
     de «ya enlazado» y el enlace nuevo nunca se creaba (`flujo: unconnected` en el registro), así que los
     reintentos no reanimaban nada. `closeAll()` ahora limpia ese estado (`src/capture.cpp`) y los reintentos
     ya no se rinden a los cuatro: siguen mientras dure la sesión y vuelven a tener presupuesto cuando
     regresan los fotogramas.
  2. **Dejar de gastar GPU en cuadros repetidos.** Si no llega vídeo nuevo y no se movió el ratón, no se
     vuelve a presentar la misma imagen: el techo pasa a 30 presentaciones por segundo (antes 120). Se
     recupera el comportamiento antiguo con `--siempre`. Con un vídeo 4K60 local, el ritmo de presentación
     se acomoda solo al del vídeo.
  3. **Aviso con la solución concreta.** Cuando el corte viene de un navegador, el registro lo dice y
     explica qué tocar:
     `Firefox o Zen: about:config -> widget.wayland.vsync.keep-firing-at-idle = true`
     (alternativa más brusca: `widget.wayland.vsync.enabled = false`), `Chrome o Chromium:
     --disable-gpu-vsync`, y `mpv, VLC y reproductores parecidos no necesitan nada`.
- Pendiente: al reanimar el flujo, a veces el **primer** fotograma no se puede importar en el driver de
  NVIDIA (`vkAllocateMemory del DMA-BUF: fallo de inicialización`) y la imagen tarda un poco más en
  despegar. El registro ya imprime los datos del fotograma (tamaño, paso, fourcc, modificador, descriptor)
  para poder atacarlo; no es un problema del descriptor del búfer, que ahora se duplica con `F_DUPFD_CLOEXEC`
  para que no lo cierre PipeWire antes de tiempo.
- Ventanas: opciones nuevas `--sin-borde` (ventana sin bordes colocada sobre la salida, en vez de pantalla
  completa) y `--pos X,Y`; y `odyssey3d ctl ventana fullscreen|normal|maximizar|cerrar --uuid ID` para
  manejar la ventana del vídeo desde fuera.

## 2D plano deja de tocar el monitor (y la sesión se cierra sola si el vídeo se cierra)
- «2D plano» era un modo más del visor, así que `serve` lo lanzaba como los demás: cambiaba el modo del
  panel a 3840x2160@120, encendía la lente y ponía el scaler en 3D para mostrar una imagen 2D normal.
  Ahora `2D plano` es **no hacer nada**: si había una sesión 3D en curso se termina (lente a 2D, scaler
  apagado, panel devuelto a su modo) y el vídeo sigue como estaba. Con mpv y `serve --dry` el registro
  dice `[serve] 2D plano: sin lente, scaler ni cambio de modo (mpv)` y no se lanza ningún proceso.
- Si el compositor deja de mandar fotogramas de la ventana capturada, el visor ya no se queda callado
  mostrando el último cuadro: a los 2 s avisa `[captura] N s sin fotogramas nuevos del compositor
  (flujo: streaming/paused/…)` y dice en qué estado ve KWin la ventana (active, fullscreen, tamaño),
  porque eso separa dos fallos muy distintos: el navegador dejó de pintar, o se cortó el flujo PipeWire.
  Después reintenta el flujo hasta 4 veces.
- Si la ventana del vídeo **desaparece** (la cerraron, mpv se acabó), la sesión se cierra sola y
  devuelve el panel a 2D: `[captura] la ventana del video cerro: se cierra la sesion y se devuelve el
  panel a 2D`. Antes se quedaba la pantalla congelada hasta pulsar ESC.
- Herramienta nueva para medir esto: `odyssey3d ctl flujo --uuid ID [--segundos N]` pide el flujo de una
  ventana y muestra cuántos fotogramas manda el compositor por segundo, sin abrir ninguna ventana
  encima. Con mpv y con un vídeo local en el navegador da 25 fotogramas/s estables incluso con la
  ventana tapada por completo, así que la oclusión no corta el flujo de captura de ventana en KWin 6.8.

## tarjeta superpuesta encima del vídeo a pantalla completa
- Antes, al poner un vídeo a pantalla completa, `serve` mandaba una notificación por el portal D-Bus:
  con «No molestar» activado se archiva y KWin la dibuja **debajo** del vídeo, así que no servía de nada.
- Ahora `src/overlay.*` es un cliente de `zwlr-layer-shell_v1` (KWin lo expone, v5) que pone una capa
  en `layer = overlay`, anclada abajo a la derecha y con `exclusive_zone = -1`: esa capa va **por encima
  de cualquier ventana, incluidas las a pantalla completa**, y no se come el teclado (no pedimos
  `keyboard_interactivity`), así que el vídeo sigue recibiendo sus teclas.
- El dibujo es el mismo `Hud` del visor (`src/vkui.*`) pero rasterizado en CPU sobre un búfer ARGB8888
  de memoria compartida: los mismos rectángulos, la misma fuente FreeType y los mismos iconos, así la
  tarjeta es idéntica a la del hub oficial. `Hud::askBounds`/`pillBounds` dan el rectángulo para el
  lienzo; el ratón entra por `wl_pointer` (coordenadas lógicas, se multiplican por la escala del monitor)
  y se reutiliza `Hud::hit`, que ya trabaja en píxeles reales.
- **Escala**: hay que derivarla del tamaño lógico (`zxdg_output_v1.logical_size` entre el modo físico),
  no del `wl_output.scale`, que en este monitor anuncia 2 con el escritorio al 150 %; con 1.50 correcto
  la tarjeta sale nítida y del tamaño justo (3840x2160 físicos, 2560x1440 lógicos).
- La casilla «guardar el tipo y no volver a preguntar» ya hace algo: al elegir un botón con la casilla
  marcada se añade `auto = app:modo` al final de `~/.config/odyssey3d/serve.conf` (se conservan los
  comentarios) y desde entonces esa aplicación pasa directa sin mostrar la tarjeta. `serve --dry` lo
  avisa: `(dry) mpv ya está ajustada a sbs, no se pregunta`.
- Comprobación: con mpv a pantalla completa y `serve` en seco, la captura de pantalla muestra la tarjeta
  encima del vídeo y el registro dice `[serve] tarjeta puesta por mpv | Voyager_3D_sbs.wmv - mpv`.
- La tarjeta se retira sola si la ventana deja de estar a pantalla completa (o al caducar `espera`), y
  al retirarla se olvida la ventana: si vuelve a pantalla completa se vuelve a ofrecer.
- Orden nueva para verla sin ningún vídeo: `odyssey3d ctl overlay [--segundos 12] [--dump CARDEN.png]
  [--titulo TEXTO] [--cuerpo TEXTO]`.
- El aviso por notificación se queda como respaldo por si el compositor no expone layer-shell.

## rastreo de cabeza (integrado en el visor, sobre v0.5)
- **Calibración de fábrica leída del propio monitor**: `ctl calib` pregunta por el puerto serie
  de la lente (`SR+CALCAM`, `SR+CALET`, `SR+CAL3D`, `SR+CHIP`) y guarda los datos en `calib/`.
  En este G90XF la cámara izquierda sale fx 474.3 fy 474.4 cx 322.2 cy 242.1, la derecha fx 476.2
  fy 476.4 cx 321.6 cy 249.2, con 8 términos de distorsión cada una, base de 69.97 mm y la lente
  separada 3.60 x −17.96 x 0.41 cm con 0.45/0.12/0.28 grados de giro.
- **Cámara infrarroja en V4L2** (`src/camera.*`): 1280x480 MJPG a 60 fps, dos mitades de 640x480,
  igual que `StereoCamera.swift`. Avisa de un defecto del enumerador de este dispositivo: los
  intervalos de imagen se indexan en la lista plana de tamaños, no por formato.
- **Rastreo** (`src/track.*`, `ctl track`): YuNet con `cv::FaceDetectorYN` (448x320, 0.7 de la
  vista), triangulación estéreo con la calibración de fábrica (`undistortPoints` +
  `triangulatePoints` + comprobación de error de reproyección) y la transformación de panel
  `DisplayTransform` del hub, con mediana de tres y filtro de salto.
- Pruebas sin tocar la cámara: `ctl track --sintetico` proyecta un punto 3D conocido y lo devuelve
  por toda la cadena (recupera 600.0 mm de 600.0 mm, error 0.000 px) y `ctl track --foto imagen.png`
  mide detección y geometría sobre un par lateral pegado. Con un par sintético a 55 px de desfase
  sale 622.9 mm y −9.35/17.19/62.59 cm en el panel: la dirección del desfase es la que dicta la
  calibración (la vista derecha va hacia la **derecha**).
- **Ya va dentro del visor**: `odyssey3d test --track` (o `play ARCHIVO --track`) arranca un hilo
  (`src/trackfeed.*`) que lee la cámara, mide la cara y deja la última pose; el bucle de dibujado,
  que va a 120 fps, la copia en el ojo de la trama con `wv.setEye` una vez por cuadro. Se usa la
  misma regla que el hub: pose fresca (menos de 0.15 s) y dentro de la zona de visión
  (y entre −50 y 50 cm, z entre 50 y 130 cm); fuera de ahí se queda la pose nominal. Opciones:
  `--track`, `--track-dev`, `--track-model`, `--track-calib-dir`, `--track-offset-x/y/z`,
  `--track-flip-x/-y`, `--track-score`, `--track-suavizado`, `--track-fixed`. Cada segundo saca una
  línea `[rastreo]` con lo medido. Sin OpenCV compilado, avisa y sigue normal.
- **Ojo con la y**: la trama propia mide `y` hacia abajo desde el borde de arriba
  (`lensCorner`: `eyeYPx = eye.y / pxPcm`), mientras `WeaverMath.cpp` la recibe al revés
  (`ey = -eye[1] / px`, hacia arriba desde el centro del panel). Los dos marcos coinciden justo en
  la pose nominal, porque 8.3916 cm es media altura del panel en el modelo, así que la pose fija de
  siempre vale para las dos; al alimentar el rastreo hay que convertir
  `e.y = altoDelPanel/2 − yPanel` (con x y z directas). El modelo de la cámara mira hacia abajo, así
  que una cara a la altura de la cámara sale en y +18 cm, medio centímetro por encima del borde.
- Falta el paso físico: al encender el infrarrojo por el control USB de la cámara el controlador se
  colgó y el monitor desapareció del bus, así que ahora mismo no hay `/dev/video*`. Hay que
  desenchufar y volver a enchufar el cable USB del monitor; probado sin mandar nada de control UVC
  (`ctl ir` ya no toca esos controles por defecto).

## v0.5 — Vulkan, interfaz con cursor y servicio al arrancar (2026-11-23)
- **Todo el dibujado pasa a Vulkan** (dynamic rendering de Vulkan 1.3; los shaders GLSL 450 de
  `shaders/` se compilan a SPIR-V con `glslangValidator` al construir y se incrustan en el
  binario). Ficheros nuevos: `src/vkdev.*` (instancia, dispositivo, DMA-BUF, barreras, charco de
  descriptores), `src/vkweave.*` (pase de trama + pase de interfaz), `src/vkui.*` (superposición
  con atlas de fuentes FreeType), `src/vkview.cpp` (ventana GLFW sin OpenGL), `src/session.*`
  (secuencia de panel/lente/scaler, teclas comunes y patrón de prueba compartidos). La ruta
  OpenGL queda como reserva con `--gl`.
- El fotograma que captura KWin se importa **directo a una imagen Vulkan**
  (`VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT` + `VK_KHR_external_memory_fd`, `tiled` explícito con
  el modificador NVIDIA del compositor): ni copia por CPU ni `mmap` del búfer.
- **Interfaz con cursor** como el Odyssey 3D Hub oficial: pastilla gris translúcida arriba a la
  derecha (Apagar/Encender 3D, Ocultar/Mostrar capa, deslizador **3D Profundidad** 0–100 y
  deslizador **3D Salida** −50…+50) y aviso abajo a la derecha con teselas *Lado a lado* /
  *Arriba y abajo* / *2D plano* + «no volver a preguntar». Ratón (clic y arrastre) y atajos
  `Ctrl+Shift+1…6`. La pastilla se escala con la altura del panel; `--sin-interfaz` la quita.
- `serve` se instala como **unidad de systemd de usuario** que arranca con la sesión gráfica
  (`systemd/odyssey3d-serve.service`, activada por `install.sh`) y se configura en
  `~/.config/odyssey3d/serve.conf` (`ignore`, `espera`, `modo`).
- `capture3d` acepta por fin `--uuid` (era exactamente lo que lanzaba el servicio al pulsar el
  aviso y moría con «opción desconocida») y el volcado de diagnóstico espera al primer fotograma
  del portal en vez de salir en negro. Probado de punta a punta: mpv a pantalla completa → KWin →
  PipeWire (DMA-BUF 3840x2160 XR24) → imagen Vulkan → trama, con `--no-panel --no-lens
  --no-scaler` para no tocar el monitor.
- Diagnóstico: `ctl hud` prueba la interfaz sin abrir ventana (atlas, vértices, golpes de ratón y
  arrastre de deslizadores) y `ctl vkprobe` importa un DMA-BUF de KWin y lo dibuja en Vulkan.
- Correcciones: el búfer de parámetros de la trama **no seguía `std140`** (`vec3 v4c[4]` con paso
  12 en vez de 16 y 192 bytes en vez de 256), así que no aparecían ni las franjas del entrelazado
  ni el indicador de estado; los parámetros se vuelven a escribir en cada cuadro, antes estaban
  congelados tras el primero y teclas/deslizadores no cambiaban nada. Añadido el volcado con
  `ODYSSEY_UBO=1` para verificar los parámetros que llegan a la GPU.
- Captura más robusta: si el nodo de PipeWire se crea pero no llega ningún fotograma en 5 s, se
  pide otra vez el flujo (hasta 3 veces) y cada 5 s informa cuántos fotogramas llegaron y de qué
  tipo. Con `--segundos` el volcado PNG ya no corta la sesión: se guarda y se sigue viendo el 3D.
- Bandera `--segundos N` en `test`/`capture3d`: sale sola con la secuencia normal de apagado
  (panel, lente y scaler restaurados), útil para probar sin estar pendiente del teclado.
- Prueba real en el panel con Vulkan: `test --segundos 30` puso 3840x2160@119.88 con escala 1 y
  rotación normal, lente a 3D, scaler en 3D, intercambio a 3840x2160 presentando estable a ~119.8
  cuadros/s, y al salir devolvió el panel a 3840x2160@164.98 con escala 1.5.
- Comparación numérica Vulkan ↔ OpenGL con el mismo patrón (`weave --cpu-patron` y `--dump`): la
  diferencia media es ~10/255 y se concentra en los mapas de corrección, donde la ruta OpenGL
  todavía muestrea volteada (pendiente).
- Pendiente: liberar los ~16 objetos Vulkan al salir (aviso `VUID-vkDestroyDevice-device-05137` de
  la capa de validación), mpv sigue dibujando con OpenGL, rastreador de mirada con la cámara IR.

## v0.4 — OpenGL, servicio de ventanas y captura (2026-11-23)
- `serve`: servicio (systemd de usuario) que vigila las ventanas a pantalla completa con
  `org_kde_plasma_window_management` y, al aceptar el aviso de KDE abajo a la derecha,
  lanza una sesión 3D para esa ventana.
- `capture3d`: captura una ventana con `zkde_screencast` + PipeWire y la trama en 3D
  importando el DMA-BUF del compositor directo a una textura OpenGL.
- `ctl windows`, `ctl capture`, `ctl capture-gl`, `ctl notify`: diagnóstico de lo anterior.
- unidad `systemd/odyssey3d-serve.service` e integración en `install.sh`
  (`systemctl --user enable odyssey3d-serve`).
- Render: OpenGL 3.3 (GLFW) + mpv para archivos. Pendiente: pasar a Vulkan y la interfaz
  con cursor al estilo del Odyssey 3D Hub oficial.
