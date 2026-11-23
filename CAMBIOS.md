# Cambios de odyssey3d

Las copias de cada versión están en `../Versiones/` (guardadas con `tools/versionar.sh ETIQUETA`).

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
