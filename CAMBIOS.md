# Cambios de OdysseyVK (hasta v0.56: odyssey3d)

Las copias de cada versión están en `../Versiones/` (guardadas con `tools/versionar.sh ETIQUETA`).
Los textos viejos se dejan con el nombre que tenía el proyecto entonces: son historia.

## Versión 1.17: la red que quieras con la vista de depuración en vivo, una red solo para vídeos, AMD y la ventanita de 3D o 2D (v1.17)

- Rodolfo: «haz el anuncio», «brinda mediciones», «usa las medidas fáciles y en tablita». `VERSION` seguía en 1.16
  aunque después de la 1.16 se habían subido 14 cambios: a quien ya la tenía no le salía la tarjeta de actualización.
  Con 1.17 se la ofrece.
- **Lo que trae** (cada cosa en su entrada, abajo):
  - **La vista de depuración en vivo**: una casilla en la pestaña 3D pone la profundidad a la vista y deja el panel en
    una columna a la izquierda con las dos listas de redes y, junto a la casilla, lo que tarda la red cada segundo;
  - **La red que quieras**: las de siempre y las de solo vídeos lado a lado, un desplegable con más de dos, y
    cualquier `.onnx` de la carpeta `models`; el panel, más ancho;
  - **Video Depth Anything pequeña**, solo para vídeos (experimental), con los cambios de plano vistos en la imagen;
  - **AMD con ROCm** (MIGraphX; sin probar en una AMD de verdad), y OdysseyVK compila otra vez sin ONNX Runtime;
  - **La primera vez de cada aplicación**, una ventanita pregunta 3D o 2D; lo de las PR #1 y #2 (los deslizadores
    dicen su valor, la cámara por su enlace, `modo = flat`, la tarjeta CUDA por su UUID, la FP32 en la CPU, el scaler);
  - **CUDA con sus opciones de verdad** (30 MiB menos por red) y sin los 386 MiB en la otra tarjeta; `ctl net
    download` en inglés.
- **Lo que no**: la red por Vulkan (ncnn), que quitaría la dependencia de CUDA y de ROCm, se está midiendo aparte y no
  está en el programa (por ahora unas 10 veces más lenta que por CUDA): no se anuncia.
- **El README**: «Lo nuevo de la 1.17» arriba, con dos tablitas (lo que cuesta cada red en la RTX 5080; la red de
  vídeo en un cambio de plano, antes y ahora); fuera las notas de versiones viejas («desde la 1.13», «hasta la
  1.15»...) y la sección de la pestaña Experimental; una tabla en vez del párrafo largo de la FP16 y la FP32. Las
  capturas, otra vez (`tools/capturas-readme.py`, con lo de en medio de 1160 de ancho), y dos nuevas: `networks.png`
  (las dos listas) y `debug-view.png` (la vista de depuración sobre la de la nave).

## Versión 1.18: el cuadrito de los FPS vigila lo que elijas, el 3D ya no se recarga al abrir o cerrar el panel, y la instalación en silencio (v1.18)

- Rodolfo: «Sí, como 1.18». Lo de después de la 1.17 (cada cosa en su entrada, abajo):
  - **El cuadrito de los FPS**, con una flechita en su tesela: cada tarjeta por separado, su temperatura, memoria y
    consumo, el procesador, la memoria del sistema y el tiempo por cuadro, como MangoHud; se queda en la esquina
    cuando se oculta la pastilla; dice «DLSS» solo con esa profundidad;
  - **El foco**: el visor ya no se minimiza al abrir o cerrar el panel (el auto-iconify de GLFW), así que el 3D no se
    rehace ni el navegador pausa el vídeo; si el vídeo se activa, el servicio le devuelve el foco al visor;
  - **La tarjeta de «el juego se cerró»**, solo en los primeros 20 s;
  - **`install.sh`** en silencio (el detalle en `build/compilacion.log`) y 0 avisos del compilador;
  - **El panel**: sin la flecha de volver a la pastilla; la nota de la carpeta de las redes al pie de cada lista;
  - **Los avisos con la tarjeta propia**: el de las opciones de lanzamiento escritas en Steam, los del actualizador y
    el de «el 3D no llegó a empezar»;
  - **La profundidad de DLSS**: su interruptor vale al momento y el modo Rendimiento no la usa nunca;
  - **La tarjeta de la red, sola** (`red = auto`): lo pesado, en la secundaria; medida si puede ir a más de una; y una
    prueba de lo que corre en cada cambio de capa;
  - **El offloading de Neural Forge en Integraciones**: su modelo en la segunda tarjeta, con su enlace PCIe;
  - **El monitor 3D en otra tarjeta**: el 3D en la del monitor y la tarjeta del juego en Integraciones.
- **El README**: «Lo nuevo de la 1.18» arriba con su tablita, y lo de la 1.17 debajo; una captura nueva,
  `monitor.png` (el menú de la tesela y el cuadrito con lo vigilado).

## El monitor 3D en otra tarjeta: el 3D en la suya y la tarjeta del juego en Integraciones (v1.18)

- Rodolfo: «haz una opcion de offload con el monitor conectado a otra tarjeta tambien, analiza si debe arrancar ahi o
  tener un segundo monitor», y de paso: «remember that if you run neural forge you might be running dlss5».
- **El 3D, en la tarjeta del monitor 3D**: el visor tomaba la primera discreta que pudiera presentar (la 4090); con el
  monitor 3D en otra, cada cuadro del 3D (4K a 165 Hz) habría cruzado el PCIe hasta la del monitor. Ahora toma la que da
  la salida `pantalla` (por su dirección PCI: `VK_EXT_pci_bus_info` y `/sys/class/drm/cardN/device`, en `pciDeConector`).
  MEDIDO en el compositor aparte con `--output DP-7` (la salida de la 5080): «gpu 1: NVIDIA GeForce RTX 5080 · ... · la
  del monitor 3D», y la red a la 4090, la secundaria para ese visor.
- **Lo que cuesta que los cuadros 4K crucen**, MEDIDO con el visor en el compositor aparte, donde sus cuadros pasan por la
  memoria del sistema: en la 4090 (PCIe 4.0 x16) 2,1 a 2,8 ms por cuadro; en la 5080 (4.0 x4, a 2887 MHz y 133 W, P0)
  10,4 ms; con el compositor en la otra tarjeta, el peor «presentar» pasa de 31-37 a 66-68 ms. Son unos 33 MB por cuadro
  a 4K: ~4,7 ms por sentido en x4, la cuarta parte en x16. Es lo que paga cada cuadro del juego cuando el juego dibuja
  en una tarjeta y el monitor está en otra (KWin lo copia). De paso: en reposo el enlace de la 5080 baja a PCIe 1.0
  (225 MHz), por eso el enlace que se enseña es el máximo de la tarjeta y su ranura y no el de ahora.
- **Sin decirle nada, el juego no va a la del monitor**: DXVK y vkd3d-proton toman la primera dedicada de la lista de
  Vulkan, que en NVIDIA va por dirección PCI (aquí la 4090, en `0000:01:00.0`). Con el monitor 3D en la 5080, el juego
  seguiría en la 4090 y cada cuadro cruzaría la x4.
- **La tarjeta del juego** (`juego_tarjeta`, en Integraciones con dos o más dedicadas): manda el juego a una con
  `DXVK_FILTER_DEVICE_NAME` y `VKD3D_FILTER_DEVICE_NAME` (por el nombre que ve el juego: «RTX 5080»; solo si casa con una
  sola tarjeta, y nunca la integrada, cuyo nombre de Vulkan no es el de pci.ids: un filtro que no casa deja a DXVK sin
  tarjeta y el juego no arranca) y `MESA_VK_DEVICE_SELECT` (Vulkan nativo). Sin `_FORCE_DEFAULT_DEVICE`: la capa de Mesa
  va debajo de la de Neural Forge y esconderia la otra tarjeta tambien a su offloading. Vacío = la predeterminada, y
  las variables solo se escriben para otra. Si la elegida no es la del monitor, una nota: «RTX 5080 dibuja el juego y
  RTX 4090 lo enseña: cada cuadro cruza por PCIe 4.0 x4» (el enlace más lento de las dos).
- **Neural Forge con el juego en otra tarjeta**: su segunda tarjeta se cuenta contra la del juego, no contra la del
  monitor; si es la del monitor, la nota lo dice («también da el monitor 3D: cada cuadro del juego ya cruza hasta ella»;
  Neural Forge pide el monitor en la tarjeta del juego). Y la red de profundidad evita también la del juego: el visor la
  lee del entorno del proceso de su ventana (esas variables) o, sin ellas, toma la predeterminada.
- **Lo que pesa de Neural Forge es su DLSS 5** (el modelo de Neural Rendering): en Raji, 25-28 FPS con él y 60-76 sin
  él (MEDIDO esta mañana). Es lo que gana yendo a la segunda tarjeta, y lo que más tráfico manda por el PCIe. La
  profundidad de DLSS que lee la capa no cambia con él (MEDIDO: 19,6 lanzamientos por copia con y sin Neural Forge).
- **El análisis**, para el equipo de Rodolfo (la 4090 en x16 con el monitor 3D, la 5080 en x4 con otro monitor): lo
  mejor es lo de ahora, el juego y el 3D en la del monitor 3D, y la red y el DLSS 5 de Neural Forge en la 5080. «Arrancar
  ahí» (el juego en la tarjeta del monitor) evita que cada cuadro cruce; un segundo monitor no hace falta para nada de
  esto (KWin copia de una tarjeta a otra), y el que ya tiene en la 5080 solo le cuesta lo que KWin dibuje en ella. Con
  el monitor 3D en la 5080, lo menos malo sería el juego también en la 5080 y el DLSS 5 en la 4090, por su x16. Con x8,
  cruzar cuesta la mitad que en x4; con x16, poco.
- **Probado**: `integraciones linea` con el monitor en DP-4 y en DP-7 (la predeterminada, la del monitor, las variables
  leídas y escritas, la segunda de Neural Forge según la del juego, un nombre que no casa a «otros parámetros»); la
  pestaña pintada con `ctl hud` (la fila «Tarjeta del juego» con la RTX 5080 elegida y su nota); el visor en el
  compositor aparte con `--output DP-7` y con gamescope en la 5080. Sin probar: un juego de verdad mandado a otra
  tarjeta, y la lectura del entorno del juego en el visor (en el compositor aparte no hay ventana de juego).

## El offloading de Neural Forge en Integraciones, con el enlace PCIe de la segunda tarjeta (v1.18)

- Rodolfo, tras el análisis de si OdysseyVK puede ir junto al offloading de Neural Forge: «Sí, añádelo»; antes: «remember
  that neural needs a parameter to offload and needs the game added to their list and the parameters on the game» y
  «remember to consider some users use x8».
- **Lo que pide Neural Forge** (su guía, «The model on a second GPU»): `NEURAL_FORGE_OFFLOAD_GPU` con el número de la
  tarjeta o sus números PCI en las opciones de lanzamiento, que no escribe nunca solo (ni su pestaña Setup), el juego
  en su lista (su ejecutable en `NEURAL_FORGE_TARGET_EXE`) y una NVIDIA RTX 40 o más nueva.
- **El interruptor** (`nf_offload`, debajo de `NEURAL_FORGE_ENABLE`, solo con Neural Forge puesto): escribe
  `NEURAL_FORGE_OFFLOAD_GPU=10de:2c02@0000:05:00.0` (fabricante y modelo de pci.ids, y la dirección: el número cambia
  con el orden de la lista y dos tarjetas iguales tendrían los mismos números). Ponerlo pone Neural Forge; quitar Neural
  Forge lo quita. Un valor que ya traía la línea del juego (`=1`, `same`) se respeta (`.nf_gpu` en
  `integraciones.conf`); el nuestro no se guarda, se vuelve a mirar. Sin `NEURAL_FORGE_ENABLE=1`, se queda con las
  demás variables.
- **La segunda tarjeta** (`integ::secundariaNf`): una NVIDIA que no sea la del monitor 3D. La del monitor 3D: la salida
  `pantalla` de serve.conf (o la del hub), si no la que dice «G90XF» en su EDID, y si tampoco la que arrancó el equipo
  (`boot_vga`). Con varias, la de más enlace. MEDIDO: en sysfs el `device` de un conector (`card2-DP-4/device`) es la
  tarjeta de DRM, no la PCI (la primera versión tomaba la 4090 por segunda); la PCI es la de `card2/device`.
- **El enlace** que se enseña es el de verdad: la menor generación y ancho de la tarjeta y de su ranura (el puerto de
  arriba), no lo de ahora, que en reposo baja de velocidad. MEDIDO en el equipo de Rodolfo: la 5080 puede PCIe 5.0 x16 y
  su ranura (`0000:04:00.0`) solo 4.0 x4, así que sale «RTX 5080 · PCIe 4.0 x4» con el aviso «cada cuadro va y vuelve
  por ahí: puede ir más lento que en la tarjeta del juego» (con menos de x16 o menos de PCIe 4.0). La 4090, 4.0 x16.
  Sin segunda NVIDIA, la píldora «Sin segunda tarjeta».
- **Y el juego que no está en la lista de Neural Forge** lo dice en la fila de Neural Forge: «No está en la lista de
  Neural Forge: añádelo en su pestaña Setup».
- **Probado** sin tocar Steam (una casa de mentira con el Steam de verdad enlazado, solo leído): `odysseyvk
  integraciones linea` con la línea de Raji, sin y con `NEURAL_FORGE_OFFLOAD_GPU` (el nuestro, `=1`, y sin
  `NEURAL_FORGE_ENABLE=1`), y la tarjeta encontrada por el EDID (sin serve.conf), por `pantalla = DP-4` (la 5080) y por
  `pantalla = DP-7   # prueba` (la 4090, con el comentario quitado); las reglas del interruptor con un programa de
  prueba; la pestaña pintada con `ctl hud` (la fila con la tarjeta y el aviso) y `integraciones ver`: la línea de Raji
  por escribir con el offloading, sin escribir nada.

## La tarjeta de la red, sola: lo pesado en la secundaria, medida si hay donde elegir, y una prueba en cada cambio de capa (v1.18)

- Rodolfo: «revises el codigo de neural forge y me ayudes a replicar el offloading de gpu, ellos hacen evaluaciones al
  inicio de la partida y ajustan y se quedan con la gpu y capa que mejor rinda», y después: «que se haga con la capa
  que corre, que solo se repita al cambiar de capa», «prioritize dedicated cards», «recuerda que la tarjeta secundaria
  debe procesar lo pesado del modelo, no la principal», «que cada cambio de modelo de dlss ejecute una prueba» («yes,
  the preset»).
- **Lo que hace Neural Forge, leído en su código** (AGPL-3.0, github.com/labj1987/neural-forge; OdysseyVK es MIT, así
  que se toma la idea y no el código): la red en otra tarjeta y en la del juego solo la captura y la composición. La
  otra la elige el usuario (`NEURAL_FORGE_OFFLOAD_GPU`), no se mide al arrancar; si no da abasto (30 de 120 cuadros
  tarde, 5 seguidos sin respuesta o un cuadro de más de 1 s) vuelve a la del juego para el resto de la sesión, y los
  10 primeros cuadros tras arrancar o cambiar de medida no cuentan. Su Placement Auto elige la etapa (antes o después
  de DLSS) por lo que ofrece el juego, no por tiempos.
- **Aquí, `red = auto`** (lo de fábrica cuando `serve.conf` no dice `red`; `red = 0` o `red = 1` la siguen fijando a
  mano, como la `red = 1` de Rodolfo): la principal es la tarjeta del visor (la que dibuja el 3D y el juego), y en ella
  se quedan la captura, la copia chica y la trama. Lo pesado, la red, va por este orden: una dedicada que no sea la
  del visor (con una, ahí sin medir); si no, la del visor y las integradas, que se miden; si tampoco, la CPU. La del
  visor se reconoce por su UUID: MEDIDO, NVIDIA le da a Vulkan el mismo que a CUDA (4973ad92… la 4090, ca3a3844… la
  5080). En el equipo de Rodolfo sale la 5080 sin medir nada, lo mismo que su `red = 1`.
- **La medida, con la capa que corre**: si la red puede ir a más de una tarjeta y esa capa no se midió, se prueba en
  cada una en el orden A B A (la de ahora, las demás, la de ahora otra vez: lo que cambie la escena se reparte), con 8
  mapas de calentamiento (de Neural Forge: los primeros no cuentan) y 3 s de medida. Gana la que menos le quita al
  juego: los cuadros nuevos del juego o del vídeo por segundo (5 % de margen), los refrescos que pierde el visor, lo
  ocupada que queda la tarjeta del visor (8 puntos) y lo que tarda la red (10 %); en un empate, la secundaria. Se
  guarda en `~/.local/state/odysseyvk/tarjeta_red` por capa (la red y las tarjetas del equipo): otra red u otra
  tarjeta se miden otra vez. `DepthNet::candidatas` (las tarjetas del motor, su nombre y si cabe la red) y
  `DepthNet::moverA` (la misma red en otra tarjeta, en marcha; si no abre allí, vuelve a la de antes).
- **Una prueba en cada cambio de capa**: la capa es lo que da la profundidad (una red, la de DLSS con su medida, que
  cambia con el preset de DLSS del juego, o el modo rendimiento). Cuando cambia y aguanta 1,5 s (3 s la primera), se
  mide 3 s lo que corre y va al registro como `[prueba]` y al `estado` como `prueba =`: los FPS del juego (con DLSS,
  los de la capa), las copias de la capa, los mapas por segundo y lo que tarda cada uno, los refrescos perdidos y lo
  ocupada que va cada tarjeta. Si cambia a media prueba, se deja y se prueba la nueva.
- **El mapa de DLSS en la 5080, medido antes de hacerlo, y no compensa** (Rodolfo eligió probarlo: «check if it
  performs better if not the first option works»). Con la partida de Raji abierta el visor hacía 5,8 mapas por segundo
  de 11,9 ms (`mapas_por_segundo` de fábrica, 6). Medido aparte, con 4 hilos en el CCD que no usaba el juego:

  | DLSS | reducirla (copiar, comprobar, por áreas, escala) | el mapa entero |
  |---|---|---|
  | 2228x1256 (Equilibrado) | 5,8 ms | 11,9 ms |
  | 2564x1444 (Calidad) | 8,0 ms | 13,4 ms |

  A 6 mapas por segundo son unos 80 ms de CPU por segundo, menos del 1 % del procesador, y no tocan ninguna tarjeta
  (la copia dentro del juego es la misma). En la 5080 se ahorrarían como mucho los 8 ms de reducirla en cada mapa y no
  subiría ningún FPS (Raji iba limitado por la 4090, no por la CPU), así que va la primera opción: la prueba de lo que
  corre. Si algún día el mapa de DLSS se hace en cada cuadro del juego (a 60 por segundo serían ~80 % de un núcleo),
  eso cambia.
- **Probado** en el compositor aparte (gamescope sin salida, el visor en la 4090 como en casa, una casa y una carpeta
  de estado de mentira), al acabar la partida de Raji:

  | Prueba | Lo que salió |
  |---|---|
  | `red = auto` | la red a la 5080 sin medir («la secundaria; la RTX 4090 dibuja el 3D») y su `[prueba]`: 61 fps, 6 mapas/s, red 5 ms |
  | las dos medidas (`ODYSSEYVK_TARJETA_MEDIR=todas`, A B A) | 4090: red 6,7 ms y 0,3 refrescos perdidos/s; 5080: red 5,0 ms y 0; gana la 5080 («lo que tarda la red»); pasar la red de una a otra, 145 a 325 ms; guardado en `tarjeta_red` |
  | otra vez, con lo guardado | arranca en la 5080 («la que ganó al medir esta capa») y solo la prueba de lo que corre |
  | DLSS de mentira, Calidad y después Equilibrado | una prueba por preset: 2564x1444 (mapa de 9,0 ms en la CPU) y 2228x1256 (8,0 ms); al irse, la de la red |
  | a mano (`red = 1`) | ni regla ni medida de tarjetas; el estado dice `tarjeta = NVIDIA · RTX 5080`, `tarjeta_por = a mano` y la prueba |

  El UUID de Vulkan y el de CUDA de cada tarjeta, mirados con vulkaninfo y nvidia-smi: los mismos.

## La profundidad de DLSS: el interruptor vale al momento, y el modo Rendimiento sin ella (v1.18)

- Rodolfo, jugando a Raji: «porque dlss me esta yendo tan mal en raji», «en la ui apagar profundidad de DLSS no manda
  a los modelos locales» y «rendimiento se deberia desconectar de cualquier modelo incluido dlss».
- **Lo de Raji, MEDIDO** (solo leyendo, con la partida en marcha): el juego se llevaba él solo la 4090 (77-99 % por
  `nvidia-smi pmon`; el visor un 11 %, KWin otro 11 %) a 25-28 FPS con DLSS Equilibrado (2228x1256). A las 06:34:25
  pasó a 60-76 FPS con la misma resolución de DLSS, justo cuando Neural Forge quedó apagado (su ventana, abierta poco
  antes, escribió `set_enabled=0` a las 06:34:34): el coste era la red de Neural Forge dentro del juego, no DLSS. DLSS corre una sola vez por cuadro: 19,6 lanzamientos de núcleos por cada copia de
  la capa, igual en toda la partida, así que nadie lanza un segundo DLSS. El DLSS de Raji es el suyo, el 2.3.1.
- **El interruptor «Usar la profundidad de DLSS»** (Ajustes, Juegos con DLSS) solo se guardaba en serve.conf («se
  aplica desde el siguiente vídeo»): el visor lo leía del entorno al arrancar. Ahora el panel le manda la orden
  `profundidad_dlss` y el visor cambia en el mapa siguiente (`DepthNet::setUsarJuego`). Apagado no se lee la memoria
  compartida, así que el visor deja de latir y a los 2 s la capa deja de copiar dentro del juego. Lo mismo con «El HUD
  al plano de la pantalla» (`hud_plano`, `ponerHudPlano`): el visor y lo que le pide a la capa.
- **El modo Rendimiento no usa la profundidad de DLSS**: antes la del juego mandaba sobre todo (el modo rendimiento
  solo valía sin ella). Ahora va solo con la imagen, sin red ni DLSS, y tampoco lee la memoria compartida.
- **Cambiar de fuente es un cambio de plano**: al pasar de la red a la del juego (o al revés) se reinician la escala y
  el plano de apoyo, y la red de vídeo empieza su ventana. MEDIDO al probar: sin esto, al volver a la de DLSS el mapa
  tardaba 14 mapas en ser el del juego; con esto, el primero ya es idéntico.
- **La capa apuntaba cada cuadro**: Raji turna dos profundidades, una por cuadro, y cada cambio iba al registro (16 000
  líneas, 3 MB en 8 minutos; 10 MB a los 20, en `/tmp`, que es memoria). Ahora cada imagen se apunta una vez.
- **Ningún juego puede escribir una línea por cuadro** (Rodolfo: «asegurate de que ningun juego pueda escribir una
  linea por cuadro»). Repasadas las 52 llamadas al registro de la capa: la mayoría ya tenían tope (los 300 primeros
  números, los 60 primeros lanzamientos, los 40 primeros aplazados), pero otras saldrían en cada cuadro con un juego que
  crease imágenes nuevas en cada cuadro o turnase dos profundidades. El tope va ahora en `anota`, la única salida del
  registro, así que vale también para las líneas de mañana: cada línea del código (su formato) puede salir 60 veces
  seguidas y después una cada 10 s, con cuántas se callaron entretanto («· (y 538 como esta calladas)»); la primera
  vez que se calla una se dice cuál. Y el archivo, 4 MB como mucho («[registro] lleno»). Con la sonda del taller
  (`ODYSSEYVK_CAPA_SONDA=1`) solo cuenta el tope del archivo.
- **Y lo que había detrás de esas líneas**: si un juego turnase dos profundidades de medidas distintas, la capa
  rehacía su memoria compartida en cada cuadro y apartaba la vieja hasta cerrar el juego (decenas de MB de `/dev/shm`
  por cuadro). Ahora una medida nueva tiene que aguantar 0,5 s; mientras, se copian los cuadros de la medida de ahora.
- **Probado** con el código de la capa entero en un programa de prueba (sin Vulkan): dos líneas en cada cuadro a 60 fps
  durante 25 s (1494 cuadros) dejan 62 de cada una más el aviso, y la de cada 10 s (como el resumen) sale entera; sin
  cupo (con la sonda), 6000 líneas de 1 KB paran en 4 194 961 bytes con «lleno»; dos medidas turnándose 10 s a 60 fps:
  0 veces rehecha (antes 600), y un cambio de verdad se rehace a los 30 cuadros (0,50 s).
- **Probado** sin ventana ni tarjeta, con la partida de Raji abierta: la red en la CPU (`ODYSSEYVK_GPU=-1`) y
  `tools/simula-capa.py` con su propia memoria compartida. Apagar: la red en el mapa siguiente (84,9 niveles de
  diferencia), sin máscara del HUD, y simula-capa ve «el visor dejó de leer» a los ~1,9 s; encender: el mapa del juego
  en el primer mapa (diferencia 0,00); Rendimiento con la capa escribiendo: ni la usa ni late. La orden pasa por el
  filtro de órdenes del visor (`ajuste::leer`) con `si` y `no`. Las traducciones del texto de ayuda, en los siete
  idiomas.

## El aviso de las opciones de lanzamiento escritas en Steam, con la tarjeta propia (v1.18)

- Rodolfo: «que la notificación de la app ... también tenga el estilo propio», «la de los parámetros de Steam». Al
  cerrarse Steam y escribir el servicio lo de Integraciones salía una notificación del escritorio (la de KDE). Ahora
  es la tarjeta de OdysseyVK, la de los avisos: «Opciones de lanzamiento escritas en Steam», cuántos juegos y que hay
  una copia de lo de antes; se cierra sola a los 8 s (la cuenta en el botón). Si ya hay otra tarjeta a la vista o no
  hay capa, la del escritorio como antes, para no perderla. `ctl hud`: `ODYSSEYVK_HUD_AVISO=steam`.
- Rodolfo, a si también los demás: «lo que sea». **Los del actualizador** («Actualizando OdysseyVK», «La actualización
  falló» con el registro, «OdysseyVK N instalado») corren aparte y el servicio se reinicia en medio: los deja en
  `~/.local/state/odysseyvk/aviso` (hora, cuenta, título y renglones, escrito de una vez) y el servicio los enseña con
  la tarjeta en cuanto no haya otra; el de «instalado», el servicio nuevo al arrancar. Uno de más de 10 minutos no
  vale. Si el servicio no corre, la notificación del escritorio. Probado: se toma una vez, la segunda no hay nada, y
  uno de hace 700 s se descarta y se borra.
- **«El 3D no llegó a empezar»** (el navegador sacado de pantalla completa al cambiar el monitor), con la tarjeta y ya
  traducido (iba siempre en español); se cierra a los 10 s.
- **El desinstalador** sigue con la del escritorio: el servicio se quita y no queda quién pinte la tarjeta.

## El visor ya no se minimiza al abrir o cerrar el panel (lo que «recargaba la imagen» y pausaba el vídeo), y «DLSS» solo con su profundidad (v1.18)

- Rodolfo: «y sí, arregla el foco». **La causa, MEDIDA** en un KWin aparte (sin GPU) con una ventana GLFW a pantalla
  completa como la del visor, una de Alacritty a pantalla completa como el vídeo, el panel de verdad y una sonda con el
  mismo protocolo de ventanas que el servicio (estados y orden de apilado cada 200 ms): no era KWin sino el
  **auto-iconify de GLFW**, que viene puesto y minimiza la ventana a pantalla completa al perder el foco.
  - Con él (como hasta ahora): al abrirse el panel, que se queda con el teclado, el visor se minimiza solo; al
    cerrarse, el foco va al vídeo, que queda encima. En el visor de verdad, el servicio lo devolvía y se rehacían su
    intercambio y su captura (la imagen «se recargaba») y Firefox pausaba el vídeo.
  - Sin él: el visor no se minimiza; al cerrarse el panel el foco le vuelve y sigue encima. El vídeo nunca se activa.
- **Lo que queda**: si el vídeo se activa (un clic en el navegador: el ratón atraviesa el visor fuera de la pastilla),
  KWin lo sube a su capa de las activas, por encima del «encima fijo». El servicio lo ve (activo, a pantalla completa
  y por encima en el orden de apilado) y le devuelve el foco al visor: MEDIDO, el visor vuelve encima en ~20 ms y sin
  minimizarse. Con un juego no se le quita el foco (le quitaría el teclado y el ratón): si lo tapa 2 s, la tarjeta de
  «El juego tapa el 3D» de siempre.
- **«DLSS» en el cuadrito** solo mientras la profundidad sale de ahí de verdad (Rodolfo: «que solo diga dlss cuando
  esté corriéndose el depth de ahí»): la marca se pone en cada mapa, así que con el conversor parado o el 3D apagado
  se quedaba con lo último. Ahora pide además el conversor y el 3D puestos.
- Sin probar aún en el monitor de verdad con Firefox.

## El cuadrito de los FPS vigila lo que elijas, como MangoHud, y la tarjeta de «se cerró» solo en los primeros 20 s (v1.18)

- Rodolfo: «añade una flechita ahí, donde también dé un dropdown de otras cosas que se puedan monitorear, como uso de
  cada gpu (listada por separado), y algunas como las de mangohud, pero mantén la estética de nuestro proyecto».
- **La tesela «Cuadrito de FPS»** lleva una flechita a la derecha que abre un menú con casillas: cada tarjeta por
  separado (con su nombre: «Tarjeta RTX 4090», «Tarjeta RTX 5080», «Tarjeta Radeon 610M»), su temperatura, su memoria
  y su consumo, el uso y la temperatura del procesador, la memoria del sistema y el tiempo por cuadro del juego. Se
  queda abierto al marcar; cualquier otro clic o Esc lo cierra. Lo elegido va a `serve.conf` (`monitor = ...`) y al
  visor abierto (la orden `monitor`); la tesela dice «Encendido · N más».
- **El cuadrito**: debajo de los FPS, una fila por cosa, con el número en blanco y la unidad chica y gris en columnas,
  del alto que haga falta. Con algo elegido sale también sobre un vídeo (titulado «Monitor»).
- **Cómo se mide** (`src/monitor.cpp`): las NVIDIA por NVML (libnvidia-ml cargada al vuelo: sin contexto de CUDA), las
  AMD por sysfs, el procesador por /proc/stat y hwmon (k10temp, zenpower o coretemp), la memoria por /proc/meminfo;
  nada lanza procesos. En el visor, un hilo lee una vez por segundo y solo mientras haya algo elegido. MEDIDO aquí: las
  tres tarjetas, el procesador y la memoria en 0,86 ms por lectura. El nombre de la Radeon sacado de pci.ids se guarda
  (antes se buscaba en cada lectura).
- **Probado**: `ctl hud` con filas de ejemplo (con la pastilla y en la esquina) y con el menú abierto
  (`ODYSSEYVK_HUD_MONITOR=1`, `ODYSSEYVK_HUD_MENU_TESELA=fps_juego`), y en un KWin aparte con clics en su sitio: la
  flechita lo abre (10 opciones), marcar la 5080 escribe `monitor = gpu:GPU-…` en serve.conf y en el ajuste, y un clic
  fuera lo cierra. Sin probar aún con el visor en un juego.
- **La tarjeta de «el juego se cerró a los N s»** salía hasta los 5 minutos, también al cerrar el juego a mano
  (Rodolfo, con la de 112 s: «que esto solo se abra a los pocos segundos, como para crashes»). La caída que avisa, la
  de la pantalla completa exclusiva al cambiar de modo, es al empezar: ahora solo en los primeros 20 s.

## El cuadrito de los FPS se queda aunque se oculte la pastilla (v1.18)

- Rodolfo: «cuando está prendido el counter de fps y se esconde el hub de 3d, el counter también se esconde, no quiero
  que lo haga, sino que se quede al frente y se mueva a la orilla de la pantalla salvo que lo apague». Se iba con la
  pastilla por dos lados: se pintaba dentro de ella (`Hud::begin` no pintaba nada con la pastilla oculta) y el visor
  no mandaba la interfaz a la pantalla con la pastilla oculta.
- Ahora es su propio trazo (`Hud::dibujaCuadroFps`): con la pastilla a la vista, a su izquierda como antes; oculta, en
  la esquina de arriba a la derecha, pegado a las dos orillas. Solo se va si se apaga (la tesela «Cuadrito de FPS» o
  Ajustes → Juegos con DLSS). Sigue sin tocarse con el ratón. `ctl hud` tiene `ODYSSEYVK_HUD_FPS=1|2` para pintarlo.
- Probado con `ctl hud` en los dos estados (con la pastilla, 684 vértices; sin ella, 198: el cuadrito solo). Sin
  probar aún en un juego de verdad.

## Sin la flecha de volver a la pastilla, la nota de la carpeta en cada lista, y lo que se recargaba (v1.18)

- Rodolfo: «quita la flecha y mira porque presionar el boton luego hace que el pill recargue la imagen». La flecha (a
  la izquierda del aspa, con el panel abierto desde la pastilla) se quita: cerrar ya vuelve al vídeo.
- **Lo que se recargaba, MEDIDO en el registro del servicio** (27-nov, 05:43 y 05:47, Firefox a pantalla completa):
  al cerrarse el panel, KWin le devuelve el foco al navegador (~1 s después); el gestor de ventanas minimiza el visor y
  el servicio lo vuelve a subir, así que el visor rehace su intercambio y su captura (la imagen «se recarga») y
  Firefox pausa el vídeo. A las 05:43 el visor lo reanudó solo; a las 05:47 no, y la fuente se quedó parada ~30 s. No
  es cosa de la flecha: pasa igual al abrirse o cerrarse el panel por otras vías (05:43:38, 05:43:46, 05:43:52) y a
  lo largo del día. Arreglado más arriba (el auto-iconify de GLFW).
- **La nota de la carpeta**, que iba a lo ancho y mal dicha, va ahora al pie de cada lista: «Las redes .onnx que dejes
  en ~/.local/share/odysseyvk/models salen aquí» a la izquierda y «Las de esa carpeta con «video» en el nombre salen
  en esta lista» a la derecha.
- **«Reoptimiza, por si algo se alentó con la vista de depuración»: MEDIDO** en el registro del visor, nada se
  alentó: 165 cuadros por segundo y la vuelta por debajo de ~3 ms (tope 6,1) con la vista puesta y quitada. La
  miniatura del corte por la imagen se calcula ya solo con la red de vídeo (la única que la usa).

## `install.sh` compila en silencio, y sin avisos del compilador (v1.18)

- Rodolfo, con la salida de `./install.sh`: «mis usuarios ven esto?». Sí, quien instala a mano: cientos de avisos del
  compilador (no errores). Quien actualiza desde la tarjeta o la tesela de la Versión no, porque eso corre como un
  servicio de systemd y su salida va al registro.
- **MEDIDO** en una compilación limpia: 561 avisos. 554 eran `-Wmissing-field-initializers` por escribir las estructuras
  de Vulkan y Wayland con su primer campo (`VkImageCreateInfo ii{VK_STRUCTURE_TYPE_...}`), que es a propósito; ese
  aviso se apaga en `CMakeLists.txt`. Dos venían de las cabeceras de OpenCV (ahora `SYSTEM`). Los otros cinco se
  arreglan: un `%zu` con un `int` en `ctl track` (el número de cuadro podía salir mal), dos contadores y una suma sin
  usar, una función sin usar, una copia en un bucle y una comparación con y sin signo. Ahora: 0 avisos, con ONNX
  Runtime y sin él.
- **`install.sh`**: todo el detalle va a `build/compilacion.log`. Se ven las líneas de OdysseyVK que dicen qué trae (el
  conversor 2D a 3D, la ventana de ajustes) y los avisos de CMake; si falla, salen los errores y dónde está el
  registro. `ODYSSEYVK_VERBOSO=1` lo enseña todo, como antes. Probado el bloque de compilar: bien (4 líneas) y con un
  `#error` puesto a propósito (sale el error y el código de salida es 1).

## La red en la CUDA 1 ya no deja un contexto de 386 MiB en la CUDA 0 (v1.17)

- Cada proceso que abría la red en la 5080 (CUDA 1) se quedaba además con **386 MiB en la 4090** (CUDA 0), por eso
  los `capture3d` salían con C+G en la GPU 0. MEDIDO el 27-nov con onnxruntime-cuda 1.29, una sonda paso a paso y un
  rastreo de las llamadas a cudart: al **añadir CUDA** a la sesión (las dos vías, la V2 y la vieja), el complemento
  hace un `cudaMalloc(1)` + `cudaFree` de prueba (para ver si hay que compilar los núcleos al vuelo) en la tarjeta
  actual del hilo, que sin más es la 0 aunque `device_id` diga 1.
- Ahora `DepthNet::init` deja la tarjeta elegida como la actual del hilo justo antes (`tarjetaActualCuda`: su
  contexto primario por libcuda, `cuDevicePrimaryCtxRetain` + `cuCtxSetCurrent`; por libcuda y no por libcudart, para
  que valga sea cual sea el CUDA con que se compiló ONNX Runtime). Si no se puede, todo sigue como antes.
- `odysseyvk mapa depth-map.png ×30 --red 1` con un HOME aparte, dos corridas de cada binario:

  | | CUDA 0 (4090) | CUDA 1 (5080) | por cuadro |
  |---|---|---|---|
  | antes | 386 MiB | 636 MiB | 10,8-11,4 ms |
  | ahora | **0** | 636 MiB | 10,9-11,4 ms |

  El registro sigue diciendo CUDA 1 y los mapas salen iguales (máx. 1/255, lo mismo que entre dos corridas del mismo
  binario). En la sonda, por la vía vieja: 386 → 0 en la 0 y 666 MiB en la 1, igual que antes.

## `tools/prueba-aparte.sh` ya no deja visores de prueba vivos (v1.17)

- Rodolfo: «remove the running cyberpunk tests you found, always make the program end tests after closure». Había dos
  `odysseyvk test` de otra prueba con casi 10 h vivos (606 + 738 MiB de VRAM cada uno): `timeout` mataba gamescope,
  pero su `gamescopereaper` y el visor quedaban huérfanos bajo `systemd --user`, y el visor ya no hacía caso a
  SIGTERM ni a SIGINT (hubo que matarlos con SIGKILL). El cuelgue al cerrar no se repitió en tres pruebas (sin red,
  con `--conver` y con el mismo entorno que aquellas): las tres cerraron bien.
- Ahora todo lo que arranca la prueba lleva la marca `ODYSSEYVK_APARTE` en su entorno y, al acabar por el tope, por
  Ctrl+C o por una señal, se mata todo lo marcado que siga vivo: SIGTERM y, a los 3 s, SIGKILL (probado con un
  proceso que no hace caso a SIGTERM: muerto a los ~5 s).

## La red de vídeo ve los cambios de plano en la imagen, antes de pasar (v1.17)

- Rodolfo: «se pueden usar las separaciones de plano en depth anything video para mejorar o reemplazar las nuestras?».
  Video Depth Anything no tiene ninguna: ni su modo por ventanas ni el de flujo miran cortes ni vacían su memoria
  (leído en su código y su README). Las suyas dependen de las nuestras: el visor le vacía la ventana en cada cambio
  de plano.
- **MEDIDO** (27-nov, `odysseyvk mapa` en la 5080 sin sesión 3D, clips de 40 + 40 cuadros con un corte duro en los dos
  sentidos, comparados con el plano nuevo pasado solo). El corte de siempre mira el MAPA (diferencia media de 0,12 y
  15 mapas de descanso, las cifras de Samsung):
  - con la red de siempre va perfecto: lo ve en el mismo mapa (0,25-0,29) y lo de después es idéntico a empezar de
    cero; el único de más, en el cuadro 24 de la persona hablando, es un corte de verdad del vídeo;
  - con la de vídeo, no: en la persona hablando sus mapas cambian solos (mediana 0,105; 12 de 38 por encima del
    umbral, hasta 0,23, y un corte de verdad dio 0,19), así que salta en falso en cuanto acaba el descanso (cuadros
    4, 57, 73) y cada salto le tira su ventana de 32 cuadros; y el primer mapa del plano nuevo sale con 31 cuadros del
    viejo: 13-15 niveles de 255 de diferencia con el plano solo, hasta 53 después, y en un sentido no se igualaba en
    40 mapas.
- **Ahora**, con la red de vídeo el corte se mira en la IMAGEN, antes de pasar la red: una miniatura de 32 × 32 de la
  luminancia contra la del cuadro anterior, la diferencia media sin las celdas negras en los dos (las bandas de una
  película no la diluyen), corte por encima de 0,14 y 3 estimaciones de descanso. Si hay corte, la ventana empieza con
  ese cuadro antes de pasarla. Las redes de un cuadro siguen con el corte por el mapa.
- **Medido otra vez**: cortes de verdad 0,169-0,199, movimiento rápido dentro de un plano hasta 0,107, un paisaje
  hasta 0,03. La red de vídeo corta solo en los cortes de verdad (24 y 41; 41 y 64) y desde el primer mapa del plano
  nuevo sale idéntica al plano pasado solo (0 niveles de diferencia en los 40); la de siempre da los mismos números
  que antes. Son tres cortes de verdad: el 0,14 queda en medio, pero no está calibrado con muchos (un corte entre dos
  tomas muy parecidas podría quedar por debajo).

## La pestaña 3D: las redes en dos mitades, desplegables y la vista de depuración en vivo (v1.17)

- Rodolfo: «I want a dropdown button for Depth network once the user exceeds 2 models, Add to the right side, like
  half and half one for video only models … also add a debug view on and off button for live testing in the middle
  top between both sides», y luego «make the main hub a bit bigger if needed» y «if there is a way to see the model
  performance measure it on the left panel mentioned beside the debug toggle».
- **Dos mitades**: las redes de siempre a la izquierda y las de solo vídeos a la derecha (`ItemPanel::lado`; las
  seguidas se ponen lado a lado, cada mitad con su altura). Un lado con **más de dos** redes pasa a ser un
  **desplegable** (`ItemPanel::Desplegable`): la puesta en la caja con su etiqueta, y al picarla la lista con la
  etiqueta de cada una (azul si está; gris con «Descargar · N MB» si no, y al elegirla se baja y queda puesta). El de
  vídeo empieza por «Ninguna: los vídeos con la de siempre». Cualquier clic fuera o Esc lo cierran; elegir la que ya
  está no hace nada. El id del desplegable lleva detrás de su cabeza la orden de cada opción (la de su casilla).
- **Vista de depuración**: una casilla en medio, encima de las dos mitades. Manda `ver_profundidad = 1|0` al visor
  (lo de Ctrl+Mayús+9; el visor no hace caso a la que encuentre en su primer vistazo, de una sesión vieja) y el
  panel se queda en una columna a la izquierda, sin velo, sin barra, sin teselas y con el teclado a pedido: las
  redes de siempre en la mitad de arriba y las de vídeo en la de abajo, para cambiarlas con la profundidad a la
  vista. La región de entrada y el desenfoque de KWin siguen a la columna (`panelCaja_`): fuera de ella el ratón va
  al vídeo. Al quitarla vuelve todo. Refleja al visor: abierto con su vista ya puesta, el panel sale así. En gris sin
  un vídeo o un juego en 3D abierto; en el panel flotante de antes no sale (y allí las listas van a lo ancho).
- **Lo que rinde la red**, junto a la casilla y cada segundo: en su pastilla los ms por mapa de la red sola, debajo
  la red y la tarjeta, y tres filas: tiempo por mapa (la red sola y en total), mapas por segundo y cuadros por
  segundo. El visor apunta en su `estado` `red_ms`, `mapas_s` (los mapas hechos entre una escritura y otra, contados
  en no menos de 0,45 s) y `ver_profundidad`.
- **El panel, más grande**: lo de en medio pasa de 980 a 1160 de ancho (en el lienzo mínimo de 1280 quedan 60 de
  margen a cada lado; las teselas grandes, hasta 200) y la tarjeta, de 600 a 700 de alto como mucho. Las notas de las
  redes, más cortas, para que quepan en media fila.
- **Probado**: `ctl hud` (con `ODYSSEYVK_HUD_DEPURACION=1` y `ODYSSEYVK_HUD_DESPLEGADO=lista:red`, nuevos) en
  español e inglés, con 2+1 redes (casillas), 4+3 (desplegables), sin sesión (la casilla en gris) y con el panel
  viejo; y el panel en un KWin aparte con un `HOME` y un estado aparte, por guion (golpes y clics en su sitio):
  abrir y cerrar cada desplegable (con un clic al lado y con Esc), elegir la FP32 (`modelo =` en serve.conf y en el
  ajuste), «Ninguna» (`modelo_video = no`), poner y quitar la vista (`ver_profundidad = 1` y `0`, y la región de
  entrada cambia). Las traducciones: 0 sin traducir.

## Una red de profundidad solo para vídeos: Video Depth Anything pequeña (v1.17, experimental)

- Rodolfo: «can we try this for videos only?» (github.com/DepthAnything). Video Depth Anything mira una ventana de
  los últimos 32 cuadros y da el mapa de todos; se queda el del cuadro de ahora. No hay ONNX oficial: es la
  exportación de la comunidad (`looplj/video-depth-anything-small-onnx` en Hugging Face, Apache-2.0, 117 MB), fijada
  a su revisión y comprobada por su tamaño. Se baja desde la pestaña 3D o con `odysseyvk ctl net download video` y
  queda en `models/video_depth_anything_small_fp32.onnx`; cualquier `.onnx` con «video» en el nombre cuenta igual.
- **Medido en la RTX 5080** (la FP32, `banco` aparte): 518×518, 269 ms por mapa y 5,9 GB; 392×392, 137 ms y 2,8 GB;
  **504×280, 122 ms y 2,8 GB** (la que se usa: la forma del vídeo, múltiplos de 14); 280×280, 50 ms y 1,8 GB. La de
  siempre (DA2 FP16) tarda 4,3 ms. Por el camino entero (`odysseyvk mapa`) salen unos 140-150 ms por mapa, dentro de
  los 6 mapas por segundo del visor.
- **Lo que gana**, medido con dos vídeos por el camino entero (cuántos pares de puntos cambian de orden de cerca a
  lejos de un mapa al siguiente): una persona hablando, del 4,80 % al 4,23 % (los saltos de más del 10 %, de 4 a 2);
  un paisaje, del 0,42 % al 0,21 % (el peor, de 9,0 a 3,9).
- **Cómo va**: `modelo_video =` en serve.conf; el servicio se la pasa al visor (`--modelo-video`) solo con las
  ventanas que no son juegos, y el visor la usa si no está sobre un juego. La ventana empieza de nuevo en los cortes
  de escena y al olvidar. El panel la pone o la quita en vivo (orden `modelo_video = RUTA|no`, que el visor no
  atiende en su primer vistazo ni sobre un juego; con ella cargada, una orden `modelo =` no la cambia). No va por
  la CPU (sería segundos por mapa): ahí se queda la de siempre. En CUDA el techo de memoria sube a 3,5 GB para ella,
  por la vía V2 y por la vieja: con el de 550 MB no arrancaba («Available memory of 40894464 is smaller than
  requested bytes of 399237888»).
- **Probado** con el visor en un gamescope aparte: con un vídeo usa la de vídeo y con un juego la de siempre; el
  cambio en vivo va y la orden vieja no se atiende; 60 cuadros por segundo con el bucle por debajo de 3 ms, y se
  cierra limpio.

## La red por CUDA, con sus opciones de verdad: la vía V2 de ONNX Runtime 1.29 (v1.17)

- Salió midiendo otra red (27-nov): `DepthNet::init` pedía CUDA por nombre (`AppendExecutionProvider("CUDA", …)`) y
  con `onnxruntime-cuda` 1.29.0 eso falla siempre («Unknown provider name 'CUDA'»: por nombre van DML, QNN,
  OpenVINO, XNNPACK, MIGraphX y unos cuantos más, CUDA no). El fallo se callaba y la red abría por la estructura
  vieja `OrtCUDAProviderOptions`: con el techo de 550 MiB y la búsqueda heurística, pero sin lo del arenal.
- **Las claves, MEDIDAS** (`UpdateCUDAProviderOptions` de la 1.29): entran `device_id`, `gpu_mem_limit`,
  `arena_extend_strategy=kSameAsRequested` y `cudnn_conv_algo_search=HEURISTIC`; no entran `enable_mem_arena`
  («Unknown provider option»: el arenal de la tarjeta no se puede apagar), `heuristic` en minúsculas ni
  `arena_extend_strategy` con número. Las opciones de antes tampoco habrían pasado.
- **Cómo quedó**: primero la vía V2 (`Ort::CUDAProviderOptions`) con la tarjeta, el techo de 550 MB, el arenal en
  `kSameAsRequested` (crece lo que se le pide, no al doble) y la búsqueda heurística; si fallara, la estructura vieja
  como antes. El registro dice cuál: «CUDA con sus opciones por la via V2 (…)» o «la via V2 de CUDA no fue (…)».
- **Medido** con la FP16 en la 5080 (CUDA 1), a 1x3x518x518, la memoria del proceso en la tarjeta según nvidia-smi:
  - banco aparte (fuera del repositorio), 2 de calentamiento y 30 pasadas, cada configuración en su proceso, 3
    rondas alternadas: antes 666 MiB (el arenal reserva 255 MiB para un pico de 139 en uso), después 636 MiB
    (reserva 191, pico 129); mediana 4,35 ms en los dos. La V2 con el arenal de fábrica da 666, como antes: lo que
    baja es el arenal, no la vía. Sacar los pesos del arenal (`session.use_device_allocator_for_initializers`) no
    baja nada más;
  - `odysseyvk mapa depth-map.png --red 1` con 30 cuadros y un `HOME` aparte (sus `models`, enlaces a las redes de
    verdad), dos veces cada binario: 666 → 636 MiB, 10-11 ms por cuadro en los dos y el mismo mapa (a lo sumo 1 de
    255 de diferencia, como entre dos corridas del mismo binario).
- **Visto de paso, sin tocar**: cada proceso que abre la red en CUDA 1 ocupa además 386 MiB en la 4090 (CUDA 0)
  desde que se añade el proveedor CUDA a la sesión, por las dos vías igual.

## `odysseyvk ctl net download`, en inglés (v1.17)

- Rodolfo, con las dos líneas del README: «change to english in this command». Ahora se escribe `odysseyvk ctl net
  download` (la FP16) y `odysseyvk ctl net download fp32`; también vale `network`. El de siempre, `ctl red
  descargar`, sigue funcionando (lo pueden tener apuntado scripts o costumbres). Los README, las guías y los avisos
  del programa que lo nombran dicen ya el nuevo.
- Probado: `net download`, `net download fp32`, `network download fp32` y `red descargar` dicen que ya está;
  `net download fp64` da el uso; `net` sola, el estado.

## La red de profundidad también en tarjetas AMD, con ROCm (v1.17)

- Rodolfo: «can you implement rocm apart from cuda?, not sure if i talk nonsense». No lo era: la red va con ONNX
  Runtime, que tiene motores para AMD.
- **Lo MEDIDO antes**: Arch trae `onnxruntime-rocm` 1.29.0 (la misma versión que `onnxruntime-cuda`), y su motor es
  MIGraphX (`libonnxruntime_providers_migraphx.so`, depende de `rocm-hip-sdk` y `migraphx`), no el ROCm de antes. Las
  cabeceras de la 1.29 declaran los dos, así que el mismo código compila con cualquiera de los paquetes.
  `Ort::GetAvailableProviders()` dice qué motores trae la que esté instalada (aquí: CUDA, DNNL y CPU).
- **Cómo elige** (`DepthNet::init`): CUDA si lo trae (como siempre); si no, MIGraphX; si no, ROCm (compilaciones
  viejas); si no, la CPU, diciéndolo. `ODYSSEYVK_PROVEEDOR=cuda|migraphx|rocm|cpu` lo fuerza. Con AMD:
  - la tarjeta: MIGraphX recibe el número de HIP, que no va en el orden de sysfs. Se le pregunta a `libamdhip64` (sin
    soltarla: HIP arranca hilos suyos) la dirección PCI de cada una y se casa con lo de amdgpu: memoria libre y
    nombre. Con eso valen las mismas cuentas de memoria que con NVIDIA;
  - la gráfica integrada del procesador (un Ryzen 7000 o 9000 la trae; en el de Rodolfo, una Radeon 610M de 2 GB)
    puede salir la primera: si la pedida tiene menos de la cuarta parte de memoria que la más grande, se usa la
    grande, y se dice. `HIP_VISIBLE_DEVICES` deja ver solo una;
  - un fallo al abrir la red en la tarjeta corta igual que con CUDA (sin reintento en el mismo proceso), y el
    registro dice el motor, la tarjeta y cuánto tardó en prepararse («lista en N s»).
  - El caché de lo compilado de MIGraphX no se usa: su código ya no está en el repositorio de ONNX Runtime (404 en
    la 1.29 y en main) y sin saber si espera un fichero o una carpeta, ni qué hace si no existe, un error dejaría a
    las AMD siempre en la CPU. Así, la primera vez de cada sesión la red se prepara para la tarjeta y puede tardar.
- **Las tarjetas AMD en Ajustes** (`tarjetasAmd()`, en util): de `/sys/class/drm/cardN/device` con amdgpu, el uso,
  la memoria, la temperatura y el nombre (el `product_name` del conductor o lo de los corchetes de `pci.ids`), junto
  a las de nvidia-smi.
- **Probado** en el equipo de Rodolfo (dos NVIDIA y la integrada AMD, sin ROCm): CUDA sigue igual (`mapa --red 1`,
  la 5080 con su UUID); forzando MIGraphX y ROCm, los dos fallan limpios («Failed to load library», «not enabled in
  this build») y la red va por la CPU con la FP32; en Ajustes sale «Radeon 610M · 0 % · 0,0 / 2 GB · 52 °C». **Sin
  probar**: la red en una tarjeta AMD de verdad (haría falta `onnxruntime-rocm`, que sustituye al de CUDA, y la pila
  de ROCm entera).

## OdysseyVK compila otra vez sin ONNX Runtime (v1.17)

- Rodolfo: «Haz esta tarea aquí: “Make OdysseyVK build without ONNX Runtime”». CMake dice que sin
  `onnxruntime-cuda` el programa se compila sin el conversor de 2D a 3D, pero no compilaba: se había ido rompiendo
  versión a versión (lo notó Noa3 en la PR #2). Con los pasos del README no se notaba, porque instalan
  `onnxruntime-cuda`.
- **Lo que fallaba y cómo quedó**:
  - `depth.cpp`: `util.h` solo se incluía con la red (`entornoOdyssey`, `rutaRedNuestra`); ahora siempre. Sus
    ayudantes de la red, sin usar sin ella, ya no avisan en esa compilación;
  - `vkview.cpp`: `profjuego.h` (la capa de DLSS, que no depende de la red) se incluye siempre; los contadores del
    registro `[fps]`, de los tirones y del cuadrito de FPS, y la Profundidad y el plano de apoyo de la vista, salen
    del bloque de la red (se usaban fuera); la orden `cargar_red` va dentro de él (como en la PR #2); la pastilla de
    rendimiento y su preajuste, solo con la red; y sin ella, un `red` y un `convActivo` de mentira con lo que
    valdría apagado (`sinRed`, `saliencia`, `focusPlane`, `exParalaje`), para lo poco que se le pregunta fuera,
    que sin conversor no se alcanza;
  - de paso, `vkui.cpp`: `antesDe` se quedó sin usar al quitar la flecha de actualizar (avisaba en las dos).
- **Probado**: las dos compilaciones desde cero en carpetas aparte, todos los objetivos (`odysseyvk`,
  `odysseyvk-panel`, `odysseyvk-ajustes`, la capa de Vulkan): sin errores, y con la red los mismos avisos de antes
  (los de siempre, ninguno nuevo); sin la red, ninguno más que con ella. Sin la red, `ctl hud` pinta el panel,
  `mapa` dice que falta la red y el ejecutable no enlaza nada de ONNX. Sin probar: el visor sin red con un vídeo de
  verdad.

## Lo que faltaba de la PR #2: `ctl red descargar` con algo raro, y la FP16 por la CPU (v1.17)

- Rodolfo: «nothing else was missing?». Repasada la PR #2 entera contra lo pasado a mano, quedaban dos cosas chicas:
  - `odysseyvk ctl red descargar fp64` (o con algo de más) bajaba la FP16 sin decir nada; ahora da el uso,
    `[fp16|fp32]`, como en la PR;
  - con la red en la CPU, al llegar la FP16 (la orden `cargar_red`) el visor se cambiaba a ella y `DepthNet::init` la
    volvía a cambiar por la FP32: una recarga de la misma red para nada. Por la CPU ya no se cambia.
- Y una de antes que la PR mencionaba: sin ONNX Runtime el programa no compila (en `depth.cpp` y, sobre todo, en
  `vkview.cpp`, variables que solo existen con la red y se usan fuera). El README instala `onnxruntime-cuda`, así que
  con sus pasos no se nota; queda aparte.
- No hay más PR ni issues en el repositorio: las tres de Noa3, cerradas.

## La primera vez de cada aplicación de vídeo, una ventanita pregunta 3D o 2D (v1.17)

- Rodolfo, sobre la PR #2 de Noa3 (lo de no pasar a 3D lo no apuntado): «he is right, it must be a popup first so that
  the overlay appears only after selecting ... and remembering, please make a consistent option to the pill», y luego
  «recuerda que el layout debe hacer match con el amarillo o azul de las settings».
- **Cuándo**: un vídeo a pantalla completa de una aplicación sin nada apuntado (con `modo = ask`, el de fábrica, y la
  capa del servicio a mano). Antes pasaba siempre al 3D solo. Ahora el servicio pone la ventanita y no toca nada más
  (ni visor, ni lente, ni scaler, ni el modo del panel) hasta que se elige. Lo apuntado va directo como siempre; los
  juegos siguen con su interruptor; `modo = flat` deja todo en 2D sin preguntar.
- **Cómo es**: la tarjeta de la pregunta (antes abajo a la derecha, con las teselas de vídeo normal y dividido, que ya
  mira solo el visor) pasa a ser como la pastilla: arriba a la derecha, pegada a las orillas, del color de la barra y
  con la esquina de abajo a la izquierda redonda. «¿Ver en 3D?» y la ventana; **3D** (como el encendido de la
  pastilla, con su símbolo), **2D** (en el gris de los controles) y el aspa; debajo **Recordar para esta aplicación**
  (puesto de fábrica) y dónde se cambia. Con el azul o el ámbar del modo de los ajustes (`exp_sin_red`), igual que
  las demás tarjetas del servicio (la actualización, los avisos), que antes salían siempre azules.
- **Qué hace**: 3D arranca el visor, que mira solo si el vídeo es normal o dividido; 2D lo deja como está. Con
  «Recordar», se apunta en serve.conf (`auto = app:conv` o `app:flat`): con 3D va directo las siguientes veces, con 2D
  la aplicación se deja en paz (antes el 2D no se apuntaba nunca). Se quita en 3D → Aplicaciones que no preguntan.
  El aspa, o no contestar en 40 s, no apunta nada.
- De paso: `hijoApp` del visor lanzado desde la tarjeta se vaciaba antes de apuntarse.
- **Probado**:
  - pintado con `ctl hud` (ODYSSEYVK_HUD_PASTILLA=0, nuevo: sin la pastilla, como en el servicio), en español e
    inglés y en azul y ámbar;
  - el servicio de verdad en un KWin aparte (`serve --dry`, casa aparte), con Konsole a pantalla completa haciendo
    de aplicación y la tarjeta contestada sola (ODYSSEYVK_TARJETA_PRUEBA, nuevo): con 2D, «apuntado: org.kde.konsole
    se queda en 2D» y la segunda vez «marcada como 2D plano: no se hace nada»; con 3D, `auto = org.kde.konsole:conv`
    y la segunda vez sin preguntar.

## La cámara por su enlace, `modo = flat`, la tarjeta CUDA por su UUID, la FP32 en la CPU y el scaler sin confirmar (v1.17)

- Rodolfo, con la PR #2 de Noa3 («Fix stereo camera detection, 2D opt-in, depth selection and scaler rollback», hecha
  sobre la 1.14). Cinco arreglos; se pasaron a mano a la 1.16, cada uno mirado contra lo de ahora:
- **La cámara estéreo por su enlace estable** (`/dev/v4l/by-id/…`): sysfs la busca por `videoN`, así que con el enlace
  salía como una mono de 640×480. Se resuelve el enlace antes de mirar sysfs (la captura sigue abriendo la ruta
  puesta). Tal cual la PR.
- **`modo = flat` se respeta**: se leía como `ask` y los vídeos a pantalla completa entraban en 3D igual. Ahora lo no
  apuntado se queda en 2D, y el interruptor de Juegos y el lanzarlo dicen lo mismo (`juegoEn3D`: lo apuntado, el
  último como lo lee el servicio, o si no `modo`). De fábrica sigue lo automático. Probado: con `modo = flat` y solo
  Cyberpunk apuntado, Cyberpunk sale en 3D y los demás en 2D.
- **La tarjeta CUDA por su UUID**: la memoria libre se miraba con el número de nvidia-smi (por bus PCI) y se usaba con
  el de CUDA (de la más rápida a la más lenta, y solo las visibles). Ahora se casan por el UUID que da libcuda (sin
  crear contexto) y el registro dice `CUDA N gpu-…`. MEDIDO aquí: en el equipo de Rodolfo coinciden (CUDA 0 = 4090,
  1 = 5080), así que no le pasaba; con otras tarjetas, sí. Probado con `odysseyvk mapa`: `--red 1` cae en la 5080 y
  `--red 0` en la 4090, cada una con su UUID.
- **La FP32 en la CPU**: si la red acaba en el procesador y lo pedido es la FP16 de fábrica, se usa la FP32 si está
  (Noa3 midió 298 ms por mapa contra 1447 en un Ryzen 9 7950X3D); si no está, se dice cómo bajarla. Más simple que en
  la PR: en la 1.16 la red marcada siempre va en `modelo =`, así que el cambio se hace al abrirla y no al elegirla. La
  red de Samsung se rechaza en la CPU también si la CPU sale porque CUDA no arrancó. Probado con `ODYSSEYVK_GPU=-1`:
  abre la FP32. La nota de la FP32 en la lista lo dice.
- **El scaler sin confirmar**: un error del adaptador o del conductor (no solo del monitor) cuenta como fallo; entonces
  el visor no sigue tramando sobre un panel en 2D (se sale por el camino de siempre, sin esperar: se mira en cada
  cuadro) y al salir se manda apagar igual, por si el monitor sí recibió la orden. El servicio, si el visor se va
  enseguida, espera 8 s y luego 60 antes de volver a intentarlo. Los bytes que el monitor no tomó (`resid`), que la PR
  también tomaba por fallo, de momento solo se apuntan («modo 3D activado (resid N)»): en el registro hay 110
  activaciones buenas en 30 días y ninguna lo decía; si siempre es 0 se puede endurecer. `tools/prueba_scaler.cpp`
  (de la PR, ajustada) lo prueba sin monitor: 6 casos, 0 fallos.
- **Sin probar** en una sesión 3D de verdad: el fallo del scaler (no se le puede pedir al monitor que falle) y lo de la
  cámara por su enlace.

## Los deslizadores dicen su valor mientras se arrastran, y Anticipar la cabeza vuelve a 0 con su flechita (v1.17)

- Rodolfo, con la PR #1 de Noa3 («Preview head anticipation while dragging and add a reset button», hecha sobre la
  1.14): «this helps?, help me to implement». Ayuda: no entra tal cual (el panel cambió entero en la 1.16), así que
  se pasó a mano, y lo del valor, a los cinco deslizadores, que tenían el mismo fallo.
- **El valor mientras se arrastra**: el número de la derecha se quedaba en lo guardado hasta soltar (en los cinco:
  fuerza del relieve, reducir el fantasma, suavizar entre las vistas, tope de FPS de la capa y anticipar la cabeza).
  Ahora sigue a la bola, con el mismo redondeo que al soltar (unas funciones compartidas), así que dice lo que se va
  a guardar. Sin arrastrar, el guardado tal cual (el fantasma de fábrica sigue diciendo 1,3 %). La ayuda no sale
  mientras se arrastra.
- **El arrastre aguanta que la lista se rehaga**: en Ajustes la lista se rehace cada segundo (los datos de
  rendimiento) y, con el ratón quieto, la bola volvía a lo guardado. Lo que se arrastra vive ahora en el estado de la
  interfaz (no en la capa) y `llenar` lo vuelve a encontrar por su id (si salen filas encima cambia de sitio), con la
  bola donde estaba. Al soltar se guarda donde se ve la bola.
- **La flechita ↺** a la izquierda de la pista de Anticipar la cabeza: la pone a 0 ms (apagada) sin tocar lo demás;
  al pasar el ratón dice «Volver a 0 ms». Picarla no empieza un arrastre.
- **El guion del panel** gana `aprieta X Y`, `mueve X Y`, `suelta` (el arrastre por el camino del ratón de verdad)
  y `corre N` (la lista corrida, como con la rueda).
- **Probado** en un KWin aparte, como la prueba de la PR: arrastrar de 12 a 45 ms y quedarse quieto 1,5 s (con la
  lista rehecha en medio): se ve 45 ms con la bola en 45 y serve.conf sigue en 12; al soltar, 45; la flechita, 0.

## La tesela de la Versión, como el botón «Actualizar a» de antes (v1.17)

- Rodolfo, con una foto del botón «Actualizar a 1.16» junto al aspa (lo que enseña la 1.15 instalada) y otra de la
  tesela diciendo «1.15, hay la 1.16»: «haz que esto ocurra aqui».
- Con una versión nueva, la tesela dice «Actualizar a 1.17», con la versión de ahora en su título («Versión 1.16»),
  la flecha en ámbar y el filo y el fondo oscuro de aquel botón. Picarla instala, como el botón (ya lo hacía).
- **Probado**: pintado con `ctl hud`, con GitHub de mentira en 1.17, sin y con el ratón encima (sale su rótulo
  «Hay una actualización pendiente… Pica para instalarla»).

## Versión 1.16: la interfaz al estilo de vkDaedalus, las redes con su casilla y la carpeta `models` (v1.16)

- Rodolfo: «program says 1.15 is up to date». Lo estaba diciendo bien: `VERSION` seguía en 1.15 aquí y en GitHub,
  aunque todo lo de la 1.16 ya estaba subido; con 1.16 el actualizador lo ofrece.
- **Lo que trae** (cada cosa en su entrada, abajo):
  - **La interfaz nueva**: el panel a pantalla completa como el menú de vkDaedalus (barra de título, una tesela grande
    por pestaña y seis chicas a los lados, la tarjeta debajo), del mismo tamaño en el escritorio y sobre un juego, con
    la flecha para volver a la pastilla; la paleta de cada modo (azul con la red, ámbar en Rendimiento); la pastilla
    arriba a la derecha del alto de la barra, con los dos botones del tipo de vídeo, que se oculta sola y vuelve al
    llevar el ratón a la orilla de arriba;
  - **Dentro de un juego o con un vídeo**: Integraciones en gris; en Juegos solo el juego de ahora en color (con un
    vídeo, Juegos también en gris); las aplicaciones de captura de pantalla no pasan al 3D;
  - **Las redes de profundidad**: la FP16 (recomendada) y la FP32 de Hugging Face, y cualquier `.onnx` de la carpeta,
    cada una con su casilla; la carpeta pasa de `modelos` a `models` para todos;
  - **ODYSSEYVK=1 para todos** desde el pie de Juegos, con su recordatorio al pasar el ratón y las exclusiones de
    cada juego;
  - **Más rápido**: el panel en su propio ejecutable ligero (`odysseyvk-panel`), que se abre en milisegundos y se
    cierra al momento; cada cuadro del panel en 4K, de ~72 ms a ~13;
  - **MAKO y vkDaedalus** sin chocar con la capa de OdysseyVK; la Versión sin flecha arriba (lo hace su tesela).
- **Las capturas del README**, otra vez (`tools/capturas-readme.py`): ahora se recorta lo de en medio del panel a
  pantalla completa, y la carpeta de las redes lleva una FP16 de mentira (un archivo disperso de su tamaño). La nota
  de la carpeta en la lista dice `~/.local/share/odysseyvk/models` (entera no cabía con una casa de ruta larga).

## La ayuda de la pastilla debajo del aviso, y su rincón para hacerla volver, una franja fina (v1.16)

- Rodolfo, con una foto del renglón de las teclas montado sobre «vídeo normal»: «correct this overlap, also, make
  the space for showing the hud with the mouse much lower».
- **El encime**: el aviso del momento (el que sale al picar un botón del tipo de vídeo) y la ayuda que sale al pasar
  el ratón por la pastilla colgaban los dos justo debajo de ella. Con un aviso a la vista, la ayuda (las teclas, o
  la de Neural | Rendimiento) va debajo de él.
- **El rincón**: con la pastilla oculta, el hueco que la hace volver al pasar el ratón era de 170 de ancho y todo
  el alto de la pastilla (56; 112 píxeles en el 4K), y saltaba al pasar por la esquina. Ahora es una franja de 6 de
  alto (12 píxeles) pegada a la orilla de arriba, del mismo ancho: el puntero se para en la orilla, así que se
  alcanza igual.
- **Probado**: pintado con `ctl hud`, con «vídeo normal» puesto y el ratón sobre la pastilla. La franja, sin
  probar en una sesión 3D de verdad.

## Las redes de profundidad con su casilla (FP16 y FP32), la carpeta `models` y la Versión sin flecha arriba (v1.16)

- Rodolfo: «ofrece descargar la red FP32 tambien, sugiere por defecto la fp16, activalas con un toggle, en gris la no
  descargada y en azul la que si, informa que pueden colocar cualquier red en la carpeta de
  /home/rodolfo/.local/share/odysseyvk/models y la herramienta la empezará a listar», «tambien en esta update
  actualiza el nombre de esa carpeta para todos los usuarios» y «remember that the fp32 was in huggingface».
- **La lista** (pestaña 3D, «Red de profundidad»; arriba del todo si no hay ninguna, si no después de Pantalla; ya no
  se repite en Ajustes):
  - una casilla por red: la FP16 «(recomendada)», la FP32 y cada `.onnx` de la carpeta (por nombre; el detector de
    caras no sale), y la de `modelo =` si está fuera de ella;
  - la puesta va marcada y es una sola: marcar otra la pone (`modelo =` de serve.conf, y el ajuste vivo para el
    visor abierto); desmarcar la puesta no la quita («siempre va una puesta»);
  - la que no está bajada, en gris con «Descargar · 50 MB» / «· 99 MB»: picarla la baja y, al acabar, queda puesta
    (sin ninguna red antes, el visor la carga y el panel se cierra, como antes). Las bajadas, «Descargada» en azul;
    las demás de la carpeta, «En la carpeta»;
  - debajo, la nota: cualquier red .onnx que se ponga en esa carpeta empieza a salir en la lista.
- **La FP32**, del mismo sitio de Hugging Face que la FP16 (`onnx-community/depth-anything-v2-small`,
  `onnx/model.onnx`, 99 060 839 bytes, MEDIDO con una petición de cabeceras), a `depth_anything_v2_small.onnx`.
  Una descarga a la vez. `ctl red descargar fp32` hace lo mismo desde la terminal.
- **La que se marca manda**: el servicio arrancaba siempre con la de Samsung si estaba en la carpeta, y el visor
  también; ahora `modelo =` gana (sin nada marcado, igual que antes).
- **La carpeta**: `~/.local/share/odysseyvk/modelos` pasa a `models` para todos. Al arrancar cualquier pieza de
  OdysseyVK se mueve entera (o lo que falte, si ya estaban las dos) y las rutas que la nombran en serve.conf, en el
  ajuste vivo y en los guardados por red pasan a la nueva. Lo que se copia del nombre viejo (odyssey3d) ya va a
  `models`. El desinstalador quita las dos.
- **La Versión**: Rodolfo, «quita el boton de update de arriba, mismo comportamiento pero recuerda que ya lo
  movimos, ahora esta en la interfaz, pon la funcion a su boton». La flecha junto al aspa ya no sale; la tesela de
  la Versión ya hacía lo mismo (buscar, o instalar la nueva) y ahora lleva también su rótulo al pasar el ratón y la
  flecha en ámbar cuando hay versión nueva.
- **Probado**:
  - en una casa de pruebas con `modelos/` (la FP16 enlazada, una red propia y el detector de caras): pasó a
    `models` y las 3 rutas (serve.conf, ajuste, perfiles) quedaron al día;
  - pintado con `ctl hud`: la lista, sin red y con ella, y el rótulo de la Versión;
  - en un KWin aparte: sin ninguna red, un clic en la FP32 (bajada de un `file://` de prueba, ODYSSEYVK_RED_URL) la
    dejó en `models`, puesta en serve.conf, con `cargar_red` para el visor, y el panel se cerró; con la FP16, marcar
    la propia la puso y desmarcarla no la quitó. Sin bajar nada de Hugging Face.

## ODYSSEYVK=1 para todos desde el pie de Juegos, y el panel del escritorio del tamaño del de encima del juego (v1.16)

- Rodolfo, con una foto del pie de Juegos (donde iba Copiar): «Put a reminder to enable the option either per game
  or globally for DLSS depth, show it only after hovering, place the option there for the global setting but if any
  game has it removed remember the specific exclusions, remember to still respect building the gamescope command as
  told».
- **La casilla**: en el pie de Juegos, «ODYSSEYVK=1 en todos los juegos», con la cuenta de los que la llevan a su
  manera («· 1 sin ella» / «· 2 con ella»). Se pica la casilla o el rótulo. La casilla no se mueve aunque cambie la
  cuenta (centrada, el segundo clic de la prueba caía fuera al encogerse el rótulo).
- **El recordatorio**, solo al pasar el ratón: para la profundidad de DLSS hace falta ODYSSEYVK=1 en las opciones de
  Steam; aquí para todos, uno a uno con el engranaje de la carátula; los que la llevan a su manera siguen así.
- **Las exclusiones**: la casilla cambia solo lo de todos (`prefs::alternarOdysseyTodos`). Un juego con `odysseyvk =
  0` propio la sigue sin llevar, encendida o apagada para todos. Los `odysseyvk = 1` propios (casi siempre lo tomado
  de su línea de Steam la primera vez) siguen a lo de todos: si no, apagarla no la quitaba de ninguno (MEDIDO: los 8
  juegos seguían con ella). La primera versión borraba los propios iguales a lo de todos y con eso, al volver a
  encenderla, se perdía la exclusión de Cyberpunk: ya no.
- **La línea**: la de siempre (`integ::linea`), con gamescope `... -- env ODYSSEYVK=1 MANGOHUD=1 ... %command%`.
- **Dentro de un juego o con un vídeo** la casilla no sale (es de Integraciones, que va en gris).
- Rodolfo, luego: «also, desktop app size does not match the overlay, adjust». MEDIDO en su registro: en el
  escritorio el panel dibujaba a 1,50 (la escala del monitor) y sobre un juego a 2,00: un 25 % más chico. Con el
  marco de vkDaedalus va siempre «al doble», como sobre el juego; y ese doble no pasa de lo que cabe del diseño de
  1280×720 (en una pantalla de 1280×720, a 1,00: el marco se encogía a 0,5 y lo de dentro de la tarjeta no).
- **Probado**:
  - pintado con `ctl hud` (la casilla, y el recordatorio con el ratón encima);
  - en un KWin aparte (1280×720), con una copia de `localconfig.vdf` y de `integraciones.conf` con Cyberpunk
    excluido: apagar quita ODYSSEYVK=1 de los 8 que la tenían; encender la pone en 9 y Cyberpunk sigue sin ella; las
    opciones de lanzamiento del `localconfig.vdf` de verdad, sin tocar;
  - en un KWin aparte de 3840×2160: el panel del escritorio y el de encima de un vídeo, los dos a 2,00 y del mismo
    tamaño.

## La pastilla, otra vez arriba a la derecha, del alto de la barra y al estilo del resto (v1.16)

- Rodolfo, con una foto de la pastilla abajo en el centro: «devuelve el pill a la orilla superior derecha, pero
  integralo a la barra de titulo en cuanto a tamaño, el ancho dejalo asi, quiero que presionar la herramientita
  saque el menu, tambien, adapta el estilo cuadrado al del resto de la aplicacion, que los botones sigan la misma
  forma que el resto».
- **Sitio y tamaño**: pegada a las orillas de arriba y de la derecha, como un trozo de barra de título. Mide 56 de
  alto, lo mismo que la barra del panel (76 solo en el modo de legado, que lleva deslizadores), y el mismo ancho
  de antes. Lo de dentro va centrado en ese alto. El renglón de las teclas pasa a la ayuda que sale al pasar el
  ratón por ella.
- **Estilo**:
  - el color de la barra del panel, con la esquina de abajo a la izquierda redonda (8), y la letra Barlow;
  - la paleta soft: el ámbar de vkDaedalus en el modo rendimiento (con la red, ver abajo: el azul de Neural);
  - los botones con esquinas de 4 como los del resto (el encendido ya no es redondo) y la cápsula Auto/Manual en
    el gris de los controles. El cuadrito de FPS va del mismo alto y color, a su izquierda.
- **Luego**: «pon los dos botones junto a la herramienta» y «el boton de encendido no tiene el azul de neural».
  - Los dos botones del tipo de vídeo (normal y dividido) van en la pastilla, junto al engranaje, con la red y en
    el modo rendimiento, y salen del menú Relieve. La pastilla se ensancha lo que miden.
  - Con la red, el encendido y la cápsula Neural van del mismo azul de la cápsula (66B8FF). En el modo rendimiento,
    del ámbar.
  - Y esos dos botones, del color del modo (Rodolfo: «can the desktop buttons also have the yellow and blue colors?»
    y, con una foto de ellos, «i dont mean the whole ui, i mean this ones»): su dibujo (`paisaje`, con un `tinte`
    nuevo) va en el azul de Neural o en el ámbar del rendimiento. El de la tarjeta de la pregunta sigue morado.
  - Y llenan su cuadro (Rodolfo: «make them fill their square and remove the white square border, make the borders
    as the on and off button one»): el dibujo ocupa el botón entero, sin el filo blanco, con las esquinas de 4 y el
    alto (32) del encendido. El puesto va encendido y el otro apagado, como el encendido con y sin 3D.
- **Sus menús** (Relieve, el del rayo) vuelven a abrirse hacia abajo, pegados a su derecha, en la tarjeta de
  vkDaedalus (121117, esquinas de 8). La tarjeta de aviso vuelve a su esquina de abajo, sin nada encima. El rincón
  que trae la pastilla oculta vuelve a ser su punta derecha.
- **El engranaje** ya abría el panel: el registro de Rodolfo lo dice en cada clic («engranaje: el panel de ajustes»
  y «panel puesto»). Ahora el panel sale de la pastilla: al abrirse baja desde arriba, en vez de subir desde abajo.
- **Probado**:
  - pintado con `ctl hud` (la pastilla con la red, en rendimiento y con el menú Relieve; el panel a media
    aparición);
  - en el compositor aparte: la región de entrada de la pastilla es 2982,0 858×112 (arriba a la derecha, 56 a
    escala 2), y sigue ocultándose sola.

## ODYSSEYVK=1, otra vez una opción (v1.16)

- Rodolfo: «stop putting the odyssey parameter by yourself, return it as an option». Se deshace lo de «ODYSSEYVK=1
  en todos los juegos, solo y sin poder quitarse» (más abajo), salvo que el botón Copiar de Juegos sigue fuera:
  - en Integraciones vuelve a ser un interruptor como las demás variables, de fábrica apagado, para todos los
    juegos o para uno;
  - `efectivo` ya no la fuerza, y `fijar`, `cargar` y `sincronizar` la tratan como a las demás;
  - el servicio ya no escribe las líneas por su cuenta (a los 20 s y cada 5 minutos). Solo escribe lo que quedó
    pendiente de un cambio hecho con Steam abierto, al cerrarse Steam, como antes.
- Lo que ya tenía Rodolfo se queda: su `integraciones.conf` guardó `odysseyvk = 1` para todos los juegos, así que
  sigue puesta hasta que la quite. El README y la guía vuelven a decir que se pone en Integraciones o a mano.
- **Probado** sobre una copia del `localconfig.vdf` de Rodolfo, en un HOME aparte:
  - de fábrica no se le pone a nadie (Cyberpunk, sin nada, sigue sin nada; nada por escribir);
  - puesta para todos, con los juegos ya vistos, a Cyberpunk le queda pendiente `ODYSSEYVK=1 %command%` y los
    demás la llevan.
  - Un `integraciones.conf` hecho a mano, sin haber visto los juegos, toma la línea vacía de Cyberpunk como suya
    (sin ODYSSEYVK). Es lo de siempre de Integraciones (lo que trae la línea la primera vez queda como propio del
    juego), no de este cambio.

## La carátula de Alice, el cierre al momento y cada cuadro del panel en 4K de ~72 ms a ~13 (v1.16)

- Rodolfo: «alice lost its image, also, closing is still slow from a video».
- **Alice: Madness Returns sin carátula**: su `library_600x900.jpg` es un JPEG de cuatro componentes (CMYK).
  OpenCV lo pasaba a RGB, pero libjpeg no sabe hacerlo: daba un error y la carátula se quedaba fuera. Ahora las
  CMYK (y YCCK) se piden en CMYK y se pasan a RGB aquí (R = C·K/255, con la tinta al revés en las de Adobe, como la
  de Alice). Sale con sus colores.
- **Cerrar, lento sobre un vídeo: era la animación.** El panel apunta ahora también lo que tarda en cerrarse
  («[ajustes] cerrado en N ms: animación ... (k cuadros), lo demás ...»). MEDIDO en el KWin aparte en 4K, sobre un
  vídeo (sesión 3D de prueba, dibujo a 2): la animación de 0,10 s duraba 150-165 ms y solo daba 3 cuadros, porque
  cada cuadro de la pantalla entera costaba ~50 ms. Ahora el panel se quita al momento: cerrado en 0 ms.
- **Cada cuadro, mucho más barato** (lo que hacía lenta la animación de abrirse y el resaltado al pasar el ratón,
  sobre todo sobre un vídeo en 4K). Medido con `ODYSSEYVK_DEBUG_UI=1` («[depurar capa] cuadro de ...»):
  - **la conversión al lienzo del compositor, de 50-55 ms a 3-9.** Con el velo, cada punto pasaba por cuatro
    `std::lround` y cuatro productos. Ahora hay una división por punto, un redondeo que da lo mismo para valores
    positivos y hasta 8 hilos por franjas;
  - **el pintado, de 20 ms a 7-12**: hasta 8 hilos por franjas de filas. Cada hilo pone a cero su franja y pinta en
    ella todo, en el mismo orden. Comprobado que la imagen sale idéntica, píxel a píxel, con un hilo y con 8
    (`ODYSSEYVK_PINTA_HILOS=1` obliga a uno, para pruebas). También acelera `ctl hud`.
  - En total, ~70-75 ms por cuadro → ~10-20 ms en 4K a pantalla completa.

## `odysseyvk-panel`: el panel en un ejecutable ligero, que arranca en ~2 ms en vez de ~50 (v1.16)

- Rodolfo, a la pregunta de si hacer un ejecutable aparte para quitar los ~47 ms de arranque: «Yes, do it».
- **El panel, aparte**: `cmdAjustes` (y su señal de cierre) pasa de `main.cpp` a `src/panel_ajustes.cpp`, que
  compilan los dos ejecutables. `odysseyvk ajustes` sigue igual.
- **`odysseyvk-panel`** (`src/panel_main.cpp`): el mismo arranque que `odysseyvk` (los datos del nombre viejo, el
  idioma, Wayland) y luego el panel, con los mismos argumentos.
  - Solo enlaza lo suyo: Wayland, FreeType, libjpeg, libpng, xkbcommon y sd-bus, 18 bibliotecas frente a 285. Lleva
    `overlay`, `vkui`, `preferencias`, `integraciones`, `wm`, `ajuste`, `idioma`, `util` y `atajos`.
  - Para no arrastrar ni OpenCV ni OpenGL, dos cambios que valen para los dos ejecutables:
    - las carátulas de Juegos se leen con libjpeg (descomprimidas ya a la mitad y bajadas a 280 × 420 con la media
      de cada bloque, como el INTER_AREA de OpenCV de antes); OpenCV queda solo de reserva si no hay libjpeg;
    - `glPngSave` pasa a `src/pngsave.cpp`, sin GL.
- **Quien abre el panel lo prefiere** (`execPanelDeAjustes` en `util`): el engranaje de la pastilla, el icono de
  la bandeja y el aviso de que falta la red. Usan `odysseyvk-panel` si está junto a `odysseyvk`, y si no
  `odysseyvk ajustes`. `ODYSSEYVK_PANEL_PESADO=1` obliga a lo segundo, para comparar. El lanzador del menú
  (`odysseyvk.desktop`) abre `odysseyvk-panel` y `odysseyvk-panel --juegos`. `install.sh` lo instala con
  `cmake --install`, en `bin`, junto a `odysseyvk`.
- **Medido**:
  - sin compositor (arranca, no conecta y sale), mediana de 10: `odysseyvk ajustes` 47,9 ms, `odysseyvk-panel`
    1,1 ms;
  - en el KWin aparte, desde que arranca el proceso hasta que empieza el panel: 51 ms frente a 2-7.
  - Con lo de la entrada de abajo, del lanzamiento al panel puesto pasa de ~150 ms a ~30.
- **Probado**:
  - qué abre `execPanelDeAjustes`: el ligero si está, el de siempre si no está o con `ODYSSEYVK_PANEL_PESADO=1`;
  - el ligero en el KWin aparte: una tesela por `clic` (cambia `fps_juego`), Juegos con sus carátulas (libjpeg),
    la flecha de volver a la pastilla y cierre con salida 0;
  - `cmake --install` a una carpeta de prueba deja `bin/odysseyvk-panel`.
- **Sin probar**: abierto desde la pastilla del visor de verdad y desde la bandeja.

## El panel se abre en 14-25 ms en vez de 87-100, y se cierra con su animación (v1.16)

- Rodolfo: «optimize the opening and closing of the layer to your best».
- **Medido primero**: el panel apunta ahora en su registro lo que tarda en abrirse, paso a paso («[ajustes] abierto
  en N ms ... capa · juegos · github · filas · ventanas · puesto»). En el KWin aparte, en 4K al 150 %:
  - capa 8 ms, juegos 12, ventanas 60, puesto 17: 100 ms desde que empieza el panel;
  - y unos 47 ms antes, que tarda en arrancar el programa con todas sus bibliotecas;
  - cerrar, unos 10 ms.
- **Lo que se quitó**:
  - **ventanas, 60 → 0 ms**: `settle(60, 10)` esperaba al menos 60 ms a que el compositor dejara de mandar
    ventanas. Ahora es una sola vuelta con él (`WindowMonitor::sincroniza`), y en ella llegan los datos de cada
    ventana. MEDIDO en el escritorio de Rodolfo (solo leyendo la lista): las mismas 8 ventanas, con su app y la
    activa, en 0,2 ms;
  - **juegos, 12 → 0 ms**: las carátulas se leen la primera vez que se entra en Juegos (o al abrir en Juegos);
    para lo demás basta la lista;
  - **puesto, 17 → 15 ms en 4K (6 → 4 en 1080p)**: el primer cuadro de la aparición es todo transparente y no se
    pinta. Lo que no se ve (opacidad 0) no recorre la pantalla. Comprobado que la imagen sale idéntica, píxel a
    píxel.
- **La animación**: al abrir, 0,16 s en vez de 0,22, saliendo deprisa (1 − (1 − t)³). Al cerrar (aspa, Esc,
  volver o el visor), lo mismo al revés en 0,10 s. Antes desaparecía de golpe. (La de cerrar se quitó luego:
  ver arriba.)
- **Resultado**: abierto en 14 ms en 1080p y en 25 ms en 4K al 150 % (desde que empieza el panel).
- **Queda**: los ~47 ms de arrancar el programa. El panel carga las bibliotecas del visor (onnxruntime, OpenCV,
  mpv y ffmpeg, ICU) aunque no las use. Solo se quitan con un ejecutable aparte para el panel.

## Volver a la pastilla desde el panel (v1.16)

- Rodolfo: «beside the closing button after accessing the overlay from the pill add a back button to just return to
  the pill».
- El visor abre el panel con `--desde-pastilla` (desde su engranaje o su atajo). Así abierto, el panel lleva una
  flecha ← a la izquierda del aspa, y la actualización pasa a la izquierda de la flecha. Al pasar el ratón dice
  «Volver a la pastilla».
- **Al picarla**: el panel se cierra y le manda al visor que ponga la pastilla (lo mismo que
  `odysseyvk ctl capa mostrar`). La pastilla, que se oculta sola a los 5 s, suele estar oculta para entonces. El
  aspa sigue cerrando sin más.
- Su golpe (`HitPanelVolver`) entra en la lista de la capa, para que no le pase lo de las teselas.
  `ODYSSEYVK_DESDE_PASTILLA=1` la pone en `ctl hud` y en el panel, para pruebas.
- Probado en el KWin aparte: con `clic` en la flecha, el panel se cierra («volver a la pastilla», salida 0) y deja
  `capa = mostrar` para el visor. Sin probar: el visor de verdad poniendo la pastilla.

## Toda la interfaz, en azul o ámbar según el modo, con fondos casi negros teñidos (v1.16)

- Rodolfo: «make the rest of the ui follow the yellow and blue tint on the currently purple parts, also adapt the
  background to an almost black but tinted version of each»; y «same for the notice cards».
- `Hud::paleta()` sustituye a la paleta soft (morada) de vkDaedalus. Vale para el panel, las teselas, las tarjetas
  de aviso, la pastilla, sus menús y el cuadrito de FPS:
  - **con la red**: el azul de la cápsula Neural (acento 66B8FF, botón principal 2F6FDE, lo elegido 1C2A42) y
    fondos casi negros azulados (barra 090B0F, tarjeta 0F1218, teselas 18202B, controles 1B2430);
  - **en el modo rendimiento** (en el visor, o con «sin red» en los ajustes para el panel): el ámbar (DEAA7C, B87A3A,
    3A2E22) y fondos casi negros cálidos (0E0B08, 15110D, 231C15, 282018).
- Se calcula una vez por cuadro, al empezar a pintar. El círculo amarillo de la actualización sigue amarillo: es el
  aviso de que hay algo. El panel de antes (`ODYSSEYVK_PANEL_VIEJO=1`) no cambia.
- Probado: pintado con `ctl hud` en los dos modos (el panel, la pastilla y la tarjeta de la actualización).

## El panel, otra vez a pantalla completa en el escritorio (v1.16)

- Rodolfo: «make the desktop version fullscreen again by default». La ventana movible del escritorio queda detrás
  de `ODYSSEYVK_PANEL_VENTANA=1`; sin ella el panel es el marco de vkDaedalus a pantalla completa en todas partes.
  `ODYSSEYVK_PANEL_COMPLETO` ya no hace falta. El arreglo de la escala del ratón (120 avos) sigue valiendo.

## Las aplicaciones de captura de pantalla no entran al 3D (v1.16)

- Rodolfo: «excluye las aplicaciones de captura de pantalla del 3d». Spectacle, por ejemplo, tapa la pantalla
  entera mientras se elige la zona, y el servicio la tomaba por una ventana a pantalla completa.
- `ignoredApp` (la que decide si una ventana a pantalla completa entra al 3D) lleva ahora una lista fija, aparte
  del `ignore` de serve.conf, que no se puede quitar. Se compara con el appId:
  - spectacle, flameshot, ksnip, obsproject, org.gnome.screenshot y gnome-screenshot;
  - kooha, gpu-screen-recorder, simplescreenrecorder, peek, vokoscreen, kazam y shutter;
  - satty, swappy y xdg-desktop-portal (su selector de qué compartir).
  OBS va por su nombre entero (`obs`, `com.obsproject.studio`): «obs» a secas también pillaría Obsidian.
- Probado aparte con appIds de prueba: Spectacle (`org.kde.spectacle`, el de Rodolfo), Flameshot, OBS, Kooha y
  ksnip quedan fuera; Obsidian, un juego de Steam, mpv y Firefox siguen entrando.

## ODYSSEYVK=1 en todos los juegos, solo y sin poder quitarse; fuera el Copiar (v1.16)

- Rodolfo: «no need for copy button anymore, just make sure the game automatically toggles the ODYSSEYVK
  parameter, make it not optional».
- **Siempre puesta** (`integraciones.cpp`):
  - `odysseyvk` vale 1 de fábrica y `efectivo` la fuerza a 1 en cada juego, diga lo que diga un
    `integraciones.conf` tocado a mano;
  - `fijar` no la cambia, `cargar` no la lee y `sincronizar` no la toma como propia de un juego aunque su línea de
    Steam no la lleve: se le pone.
- **Escrita sola**: el servicio, a los 20 s de arrancar y cada 5 minutos (así entran los juegos recién instalados),
  hace lo de «Escribir». Con Steam cerrado escribe lo que falte, con copia del fichero; con Steam abierto deja la
  marca y la escribe su vigilante en cuanto se cierra. Las líneas que no se saben leer (con `;`, tuberías...) se
  siguen dejando como están, sin ella.
- **Integraciones**: la fila de ODYSSEYVK ya no es un interruptor, sino una nota: «ODYSSEYVK=1 va siempre: con ella
  el 3D toma la profundidad de los juegos con DLSS».
- **Juegos**: fuera el parámetro y el botón Copiar del pie. El README y la guía dicen que se escribe solo.
- **Probado** sobre una copia del `localconfig.vdf` de Rodolfo, en un HOME aparte:
  - de 10 juegos, a 2 les faltaba (Cyberpunk, sin nada; Lossless Scaling) y se escribieron, con copia; los 10
    acaban con `ODYSSEYVK=1`;
  - con `odysseyvk = 0` puesto a mano en `[todos]` y en Cyberpunk, sigue saliendo `ODYSSEYVK=1 %command%`.
- **Sin probar**: el servicio de verdad escribiéndolo solo.

## Las teselas no hacían nada al picarlas, ni el Copiar de Juegos (v1.16)

- Rodolfo: «dar click en los ajustes que tienen boton no hace nada»; «los de fps hud dlss y cabeza».
- La capa (`overlay.cpp`, `panelBoton`) solo deja pasar al bucle del panel una lista de golpes, y el de las teselas
  (`HitPanelTesela`) no estaba. El Idioma sí funcionaba, porque su tesela toma el camino del botón del idioma de
  antes. En las pruebas no salió porque el guion metía el golpe directo en el bucle, sin pasar por la capa.
- Con el mismo fallo, desde la 1.14: el botón **Copiar** del pie de Juegos (el parámetro de Steam). Tampoco
  llegaba nunca.
- **Arreglo**: los dos entran en la lista. Copiar apunta en el registro si copió o por qué no.
- **Para que no vuelva a pasar**: el guion del panel tiene `clic X Y`, un clic en el lienzo por el mismo camino que
  el ratón de verdad (`Overlay::clicPrueba`).
- **Probado** en el KWin aparte con `clic`: las cuatro teselas cambian `profundidad_dlss`, `hud_plano`, `rastreo`
  y `fps_juego` (en un HOME aparte); y Copiar llega al bucle con el arreglo y no llega sin él (comprobado quitándolo
  un momento). En el KWin aparte no hay portapapeles al que copiar.

## Dentro de un juego: Integraciones en gris y en Juegos solo el suyo; sin opciones repetidas (v1.16)

- Rodolfo, corrigiendo lo de la entrada de abajo: «si muestra el boton de juegos e integraciones, pero integraciones
  en gris, que juegos deje entrar pero solo se vea de color el juego actual»; y «quita las opciones que ya tengan
  boton de ajustes, no repitas opciones».
- **Dentro de un juego**, o con una sesión 3D, las pestañas se ven todas (antes, con una sesión 3D, Juegos no
  salía):
  - Integraciones, en gris, con «En el escritorio» debajo: no se abre (ni picándola, ni por guion);
  - en Juegos, solo la carátula del juego de ahora va en color, con su 2D | 3D. Las demás van en gris (su
    luminancia, oscurecida), con el nombre tenue, y no se pican: desde dentro de un juego no se arranca otro.
    Ninguna lleva el engranaje que lleva a Integraciones;
  - el juego de ahora es el de la ventana activa (`steam_app_<id>`) o, con una sesión 3D, el de la ventana que se
    ve en 3D: el hub apunta su uuid en `ventana` y el panel lo busca en la lista de ventanas;
  - sin juego a la vista (un vídeo en 3D), Juegos también va en gris, como Integraciones (Rodolfo: «si no se esta
    viendo un juego que la pestaña juegos tambien sea gris, por ejemplo en un video»). En `ctl hud`,
    `ODYSSEYVK_HUD_SESION=1` sin `ODYSSEYVK_HUD_JUEGO`.
- **Sin opciones repetidas**: en el panel nuevo, las filas que ya tienen su tesela salen de la lista de Ajustes
  (Usar la profundidad de DLSS, HUD en el plano, el cuadro de FPS, Seguir la cabeza, Idioma y la de la
  actualización), y con ellas el rótulo de una sección que se quede vacía («Juegos con DLSS»). En el panel de
  antes (`ODYSSEYVK_PANEL_VIEJO=1`), sin teselas, se quedan.
- `ctl hud`: `ODYSSEYVK_HUD_SESION=1` pone Integraciones en gris y `ODYSSEYVK_HUD_JUEGO=<appid>` deja en color ese
  juego.
- Probado: pintado con `ctl hud` (Juegos dentro de Cyberpunk: solo él en color; la lista de Ajustes sin las
  repetidas); en el KWin aparte con una sesión 3D, picar Integraciones no la abre («la pestaña 2 está en gris»).

## El ratón desfasado en el escritorio, el panel en ventana y sin Juegos dentro de un juego (v1.16)

- Rodolfo: «mouse desfasado en el escritorio, en la vista de escritorio hazlo movible, y con boton de cierre»; y
  «also remember, no games once inside a game, same for integrations».
- **El ratón desfasado era la escala**. Error de antes de esta versión, en `overlay.cpp`: el compositor manda la
  escala fraccionaria (`wp_fractional_scale_v1`) en 120 avos y se dividía entre 100. En el monitor de Rodolfo
  (DP-4, 4K al 150 %) llegaba 180 y el panel se ponía a 1,80, sobre una pantalla de 1,5. Lo dicen sus registros:
  «escala del monitor 1.50» y luego «1.80». El dibujo y el ratón iban a 1,8 sobre 1,5: el ratón caía corrido un
  20 %, más cuanto más abajo a la derecha. A pantalla completa se notaba en todas partes; el panel flotante, más
  chico y en el centro, lo disimulaba. Ahora se divide entre 120. No se pudo reproducir en el KWin aparte, que no
  manda esa escala.
- **En el escritorio, una ventana que se mueve**: el mismo marco al estilo de vkDaedalus (la barra, las teselas,
  la tarjeta), en una ventana de 1012 de ancho (el contenido de 980 y sus márgenes), sin velo. Se arrastra por su
  barra, como el panel de antes, y la barra lleva el aspa y la actualización. Sobre un juego o con la sesión 3D
  sigue a pantalla completa. `ODYSSEYVK_PANEL_COMPLETO=1` la pone entera también en el escritorio.
- **Ni Juegos ni Integraciones dentro de un juego**: con un juego como ventana activa al abrir el panel tampoco
  salía la pestaña Juegos. Las dos seguían la misma regla. (Cambiado enseguida: ver la entrada de arriba.)
- Probado: pintado con `ctl hud` (la ventana en 2560 × 1440 y la pantalla completa con la sesión 3D, con solo 3D y
  Ajustes); en el KWin aparte a 150 %, la ventana de verdad (1020 × 888 con su borde). Sin probar: arrastrarla y el
  ratón con la escala arreglada en el escritorio de Rodolfo.

## El panel, al estilo de vkDaedalus (v1.16)

- Rodolfo: «quiero que la interfaz del resto siga un estilo como el de vkdaedalus cuando se esté en un juego, misma
  escala tambien, para que se sientan consistentes»; al preguntarle, «Full menu like vkDaedalus» y en todas partes
  (no solo en los juegos); y «la herramienta desplace el resto de la interfaz hacia la pantalla ya con el estilo de
  daedalus».
- **El panel de ajustes, a pantalla completa como el menú de vkDaedalus**, con sus medidas
  (`overlay/frame_layout.hpp` de vkDaedalus):
  - un lienzo de diseño de 1280 × 720 que se achica solo si la pantalla no llega; en 1080p mide lo mismo que el de
    vkDaedalus;
  - un velo oscuro (el 5,7,10 de vkDaedalus) y la barra de 56 arriba, con el logo y «OdysseyVK» y, a la derecha,
    la actualización (si hay versión nueva, un botón «Actualizar a …» como su Preview) y el aspa;
  - el contenido, de 980 en el centro. Las teselas empiezan en 88: en el centro, una grande por pestaña (3D,
    Juegos, Integraciones, Ajustes) con su icono y una nota debajo («Relieve x1,00», «10 juegos», «Todos igual»,
    la versión). Las que no salen sobre un juego no ocupan sitio;
  - a los lados, seis chicas: Profundidad de DLSS, HUD del juego y Seguir la cabeza a la izquierda; Cuadrito de
    FPS, Versión e Idioma a la derecha. Las cuatro primeras son interruptores: se cambian al picarlas, como su
    fila. Versión busca la actualización e Idioma abre la lista;
  - la tarjeta de la pestaña puesta, desde 304, con su nombre arriba, las filas y el pie.
- **Su paleta, su letra y sus controles**:
  - la paleta «soft» de vkDaedalus, la que tiene puesta Rodolfo (acento A88FD0, tarjeta 121117, letra E9ECEF);
  - su letra, Barlow, en normal y seminegrita, metida en el programa (`assets/fuentes`, licencia OFL;
    `cmake/embed.cmake` como los shaders). El ruso, el chino y el japonés salen de la de siempre, que Barlow no
    los trae. El atlas pasa de 512 a 1024 de alto;
  - las secciones van en frase («Tarjetas gráficas», no «TARJETAS GRÁFICAS»), dejando las siglas;
  - los interruptores son casillas a la izquierda del rótulo, los botones van en el morado fuerte (6E4E8C) con
    letra blanca, y las esquinas son de 4. La opción puesta va en un fondo más claro, no en el acento.
- **Las tarjetas también** (Rodolfo: «Yes, the cards too»): la de la pregunta del tipo de vídeo, la de la
  actualización y los avisos.
  - Van en la tarjeta 121117 con un filo fino (343C46), con esquinas de 8, la letra Barlow y los títulos en
    seminegrita.
  - El botón principal (Actualizar, Entendido) va en el morado fuerte de vkDaedalus, y el otro (Luego) en el gris
    de sus controles, con esquinas de 4. Las teselas del tipo de vídeo van como las del panel, con el filo del
    acento al pasar el ratón, y la casilla de «no volver a preguntar» es la de vkDaedalus.
  - La letra chica sube de 11,5 a 12,5: Barlow a 11,5 queda por debajo de la más chica de vkDaedalus (12).
  - Con `ODYSSEYVK_PANEL_VIEJO=1` vuelven a ser las de antes.
  - Probado: pintadas con `ctl hud`, las de antes al lado.
- **Aparece desde la pastilla**: en 0,22 s la interfaz sube desde abajo, donde está la pastilla, y se hace opaca.
  Las carátulas también. El velo solo se oscurece.
- Sin arrastrar ni cambiar el tamaño (no hay ventana que mover) y sin el pin. Se cierra con el aspa o con Esc,
  como antes. `ODYSSEYVK_PANEL_VIEJO=1` deja el panel flotante de antes; la pastilla no cambia de letra.
- **Medido**: pintar la pantalla entera en la CPU cuesta 5 ms en 1080p y 22 ms en 4K.
- **Probado**:
  - pintado con `ctl hud` (las cuatro pestañas, a media aparición, en ruso);
  - en un KWin aparte, el panel de verdad: lienzo de 1920 × 1080, la aparición y la tesela del cuadrito de FPS
    (`tesela 3` en el guion).
- **Sin probar** sobre un juego de verdad ni a 4K con la sesión 3D. Las capturas del README siguen siendo las de
  antes (se rehacen al sacar la versión).

## La pastilla, abajo en el centro (v1.16)

- Rodolfo: «quiero que el hub siga igual de estilo pero al fondo a la derecha»; y para el resto, «un estilo como el
  de vkdaedalus», con su misma escala, en todas partes (es la primera parte: la pastilla).
- Luego: «que el pill sea igual de practico, pero que vaya al centro inferior y la herramienta desplace el resto de la
  interfaz hacia la pantalla ya con el estilo de daedalus». La pastilla del visor va abajo en el centro, a 24 px del
  borde, con el mismo estilo y los mismos controles. Sus menús (Relieve, el del rayo) se miden antes y se abren
  hacia arriba, centrados encima de ella. La tarjeta de aviso, que ya iba
  abajo a la derecha, sube encima de la pastilla si está a la vista. El cuadrito de FPS sigue a su izquierda, y el
  tramo que la trae de vuelta cuando está oculta es el del medio de donde estaba.
- **Se oculta sola y sin iconos repetidos** (Rodolfo: «que el pill inicie y se auto oculte en unos segundos, y en
  los pill solo manten los iconos de la izquierda, no los repitas en la segunda capsula»):
  - sale al empezar, en un juego o en un vídeo, y se oculta sola a los 5 s sin el ratón cerca (24 px de holgura)
    ni un menú suyo abierto. Vuelve al pasar el ratón por donde estaba y se va otra vez igual. Antes se iba solo una
    vez, al entrar a un juego. Al irse le devuelve el cursor al juego, como al ocultarla a mano.
    `ODYSSEYVK_PASTILLA_FIJA=1` (pruebas) la deja puesta;
  - la segunda cápsula (Auto/Manual, o el preajuste en el modo rendimiento) va sin el chip, la llave ni el rayo,
    que ya salen en la de la izquierda. Es más estrecha.
  - Probado en el compositor aparte: «la pastilla ocultada (se ocultó sola a los 5 s sin usarla)», y su región de
    entrada abajo en el centro.
- **Integraciones, solo en el escritorio** (Rodolfo: «sin menu de integraciones cuando la interfaz esta sobre un
  juego, unicamente fuera en ajustes, en el escritorio»). Con una sesión 3D (vídeo o juego), o con un juego como
  ventana activa al abrir el panel, la pestaña Integraciones no sale. Tampoco el engranaje de las carátulas que la
  abre. Es como la de Juegos, que ya no salía con una sesión 3D. `ODYSSEYVK_HUD_SESION=1` en `ctl hud` quita las
  dos.
- `ODYSSEYVK_HUD_ASK=0` (pruebas): `ctl hud` sin la tarjeta de la pregunta, para ver la pastilla y sus menús solos.
- Probado: pintado con `ctl hud` (la pastilla con el menú Relieve abierto, y con un aviso encima). Sin probar en el
  visor de verdad.

## Versión 1.15: la pestaña Integraciones, el tope de FPS de la capa y las carátulas (v1.15)

- Rodolfo: «Sí, prepárala». Desde la 1.14 publicada (`f980cae`) van tres cambios con el mismo número: con 1.15 el
  actualizador los ofrece.
- **Lo que trae** (cada cosa en su entrada, abajo):
  - **La pestaña Integraciones**: las opciones de lanzamiento de Steam por piezas, para todos los juegos y para cada
    uno (el engranaje que sale en su carátula): `ODYSSEYVK`, `MANGOHUD`, `ENABLE_VKDAEDALUS`, `NEURAL_FORGE_ENABLE` (con
    su `NEURAL_FORGE_TARGET_EXE` sacado de Neural Forge) y `ENABLE_VKBASALT`, otros parámetros, `vkdaedalus-run` y un
    comando propio delante del juego, sus opciones detrás, y gamescope (la resolución de renderizado y la de salida
    por abreviatura, stretch o fit, `-f`, el cursor y el reescalado FSR o NIS, que lo enciende). La línea entera se
    escribe en Steam con copia del fichero: con Steam cerrado al momento; abierto, en cuanto se cierra. Lo que los
    juegos ya tenían, y lo que se cambie luego por fuera, se lee y queda como propio de cada juego.
  - **Con MAKO y vkDaedalus**: la capa de OdysseyVK carga con ellos (no con `mako-launch`); con el menú de vkDaedalus
    abierto la pastilla se aparta y el 3D va plano; un solo contador de FPS; los HUD de las otras capas, planos.
  - **Lo que no está instalado, en gris** con la pastilla «No instalada», y `ENABLE_VKDAEDALUS` y `vkdaedalus-run`,
    uno u otro. La pestaña solo se rehace si algo cambió.
  - **Campos de texto en el panel**: con acentos y AltGr, Ctrl+V, y la tecla pulsada que se repite.
  - **El tope de FPS de la capa** (Ajustes → Fluidez): cuántas veces por segundo copia la capa de DLSS la
    profundidad y el HUD dentro del juego, como media exacta (con 50 de tope salen 50, no 30).
  - **Arreglo**: las carátulas de Juegos ya no tapan la ayuda del panel.
- **Probado** (cada entrada lo cuenta): Integraciones sobre una copia del `localconfig.vdf` de Rodolfo y con el panel
  en un KWin aparte; el tope, en un vkcube de prueba. **Sin probar**: Integraciones con el Steam y el teclado de
  verdad, y el tope dentro de un juego.
- Para el tope hay que reiniciar el juego después de instalar (la capa y el visor hablan el protocolo v6).
- README y README.es: la sección Integraciones, con su captura, y el tope en la de DLSS; las capturas se rehicieron
  (`tools/capturas-readme.py`: la pestaña nueva sale en la fila de pestañas). La guía ya llevaba la sección
  Integraciones y la orden `odysseyvk integraciones`.

## Con MAKO y vkDaedalus: las otras capas no chocan con la de OdysseyVK, y solo una al frente (v1.15)

- Rodolfo: «Voy a usar el frame generation de MAKO en vkdaedalus, checa que no te de problemas y procura que no
  choquen los overlay, que solo uno esté al frente a la vez». Al preguntarle cuáles, las tres: el menú de vkDaedalus
  contra la pastilla, un solo contador de FPS y lo de las otras capas plano en el 3D.
- **MAKO con la capa de OdysseyVK** (mako-render 5.0.0, medido con `VK_LOADER_DEBUG=layer`). Con `ENABLE_MAKO=1`
  el orden, del juego al controlador, es Neural Forge, OdysseyVK, MangoHud, MAKO. La capa de OdysseyVK queda por
  encima de MAKO: ve los cuadros de verdad del juego, la profundidad de DLSS no cambia y el cuadrito de FPS cuenta
  los del juego, no los generados. En un vkcube dentro de un gamescope aparte, con MAKO cargado, la capa crea su
  dispositivo y ve la cadena igual que sin MAKO.
  - **`mako-launch` no vale** con OdysseyVK: pone `VK_IMPLICIT_LAYER_PATH` solo en la carpeta de MAKO y deja fuera
    la capa de OdysseyVK y la de Neural Forge. `vkdaedalus-run` no lo usa (pone `ENABLE_MAKO=1` y sus capas de
    orden en `VK_ADD_IMPLICIT_LAYER_PATH`), así que con él sí carga.
  - **Con vkDaedalus y MAKO LSFG, medido** (vkDaedalus instalado por Rodolfo en `/usr`, Lossless Scaling desde
    Steam). Prueba: vkcube con `vkdaedalus-run` en un gamescope aparte, la capa de OdysseyVK puesta y el menú de
    vkDaedalus manejado con `xdotool` (End y clics donde dice su `VKDAEDALUS_UI_MAP`), con sus ajustes y los de
    MAKO en una copia aparte:
    - en la tarjeta Frame generation se elige MAKO LSFG (lsfg-vk y MAKO FG salen en gris: no están instalados) y
      On. Al volver a arrancar, MAKO lo dice en su fichero de estado: `frame_generation_active: true` a 2x;
    - vkDaedalus cuenta ~30 FPS del juego y 60 enseñados;
    - el orden, con «Effects after frame generation» (lo lleva el perfil default de Rodolfo): juego, MAKO,
      vkDaedalus, OdysseyVK, gamescope WSI, controlador. Aquí la capa de OdysseyVK queda por debajo de MAKO y ve
      también los cuadros generados: su contador de presentaciones (el FPS del juego del cuadrito) cuenta los
      enseñados, como con la generación de cuadros de DLSS. La profundidad de DLSS sigue llegando una vez por
      cuadro de verdad;
    - MAKO crea su propia instancia y su dispositivo, que pasan también por la capa de OdysseyVK (`[instancia]
      creada (mako)` en su registro). Allí no hay DLSS y no hace nada, sin errores.
  - **Sin probar** con MAKO: un juego con DLSS de verdad, el 3D encima y la pastilla con el menú en un juego de
    Wine (vkcube no es de Wine, así que su menú no escribe el fichero de `$XDG_RUNTIME_DIR`).
- **Las otras capas** (`src/capasajenas.cpp`). Dibujan sobre el cuadro ya hecho, después del HUD del juego. La
  máscara del HUD no las ve, así que en el 3D tomaban el relieve de la escena. El visor las mira cuatro veces por
  segundo, sobre un juego, y sin tocarlas:
  - **el menú de vkDaedalus** (End). Mientras está abierto, el gancho de entrada que precarga `vkdaedalus-run` (en
    un juego de Wine) escribe en `$XDG_RUNTIME_DIR/vkdaedalus-input-<dev>-<ino>` el proceso que lo tiene abierto:
    8 bytes, el arranque del proceso por 2^32 más su pid. Lo hace para retener la entrada de todo el prefijo, y lo
    borra al cerrarlo. Se da por abierto si ese proceso vive con ese mismo arranque, que es lo que comprueba
    vkDaedalus. El menú ocupa la pantalla entera con un velo oscuro;
  - **su HUD**: `libvkdaedalus.so` cargada en el juego (su `/proc/<pid>/maps`) y `hud = true` en
    `~/.config/vkdaedalus/settings.conf` (con FPS si `hudItems` lleva `fps`). Va arriba a la izquierda;
  - **MangoHud**: `libMangoHud` cargada en el juego, salvo `no_display` en su `MANGOHUD_CONFIG` o en su fichero
    (`MANGOHUD_CONFIGFILE` o `~/.config/MangoHud/MangoHud.conf`). Va en su `position`.
  - El juego es el proceso de la ventana capturada (KWin lo da). Las bibliotecas y los ajustes se releen cada 3 s.
- **Solo una al frente**:
  - Con el menú de vkDaedalus abierto, la pastilla se aparta y el 3D va entero al plano de la pantalla, para que
    el menú se lea bien. Lo que el usuario pida de la pastilla mientras tanto (Ctrl+Mayús+F1) se guarda y vale al
    cerrarse. El menú de vkDaedalus no se puede cerrar desde fuera, así que gana mientras está abierto.
  - Si MangoHud o el HUD de vkDaedalus enseñan los FPS, el cuadrito de FPS de OdysseyVK no sale.
  - La esquina de cada HUD va al plano de la pantalla como el del juego, con un borde suave de dos puntos del mapa:
    0,22 × 0,30 de la imagen para MangoHud y 0,20 × 0,12 para el de vkDaedalus, medidos a ojo y con holgura.
- **Probado**:
  - La detección, con procesos de prueba: el menú abierto y cerrado, un arranque que no cuadra, MangoHud arriba a
    la derecha y con `no_display`, y el HUD de vkDaedalus con FPS, sin FPS y apagado. Para el HUD, una biblioteca
    cualquiera copiada como `libvkdaedalus.so` y precargada.
  - En el visor, en el compositor aparte, con un menú falso (`ODYSSEYVK_AJENAS_DIR`, para pruebas): la pastilla se
    oculta y el mapa de profundidad sale plano entero (desviación 0; sin el menú, 73).
  - El aplanado de una esquina, aparte.
- **Sin probar** con vkDaedalus, MangoHud y un juego de verdad.

## Integraciones: lo no instalado en gris, vkDaedalus uno u otro y menos trabajo mirando Steam (v1.15)

- Rodolfo: «Que enable_vkdaedalus no permita habilitar vkdaedalus-run y viceversa, es uno y otro, también procura
  que estén en gris todos los que no estén instalados en el equipo, y que la pill diga Proyecto no instalado»; y de
  la pastilla, «Esta herramienta no está instalada, o lo que supongas mejor». Y: «¿no es mucho recurso ver tan
  activamente cuáles están en Steam?, optimiza esa parte lo que se pueda».
- **Lo no instalado** (`integ::instalado`): una capa lo está si algún manifiesto de capa implícita de Vulkan se
  enciende con su variable (`enable_environment`), buscado donde lo busca el cargador (`~/.config`, `XDG_CONFIG_DIRS`,
  `/etc/vulkan`, `~/.local/share`, `XDG_DATA_DIRS`); `vkdaedalus-run` y gamescope (y con él el reescalado), si su
  comando está en el `PATH` (más `~/.local/bin`, `/usr/local/bin` y `/usr/bin`, por si el servicio arranca con un
  PATH corto). Así no hay una lista de rutas por proyecto: vale lo instale quien lo instale. Su fila sale en gris con
  la pastilla «No instalada» (corta, para que quepa al lado del interruptor) y la ayuda «Esta herramienta no está
  instalada en el equipo»; no se enciende, pero si venía puesta en la línea del juego se puede apagar. Se vuelve a
  mirar solo si cambia alguna de esas carpetas. En el equipo de Rodolfo: vkDaedalus, vkBasalt y `vkdaedalus-run`, no
  instalados; ODYSSEYVK, MangoHud, Neural Forge y gamescope, sí.
- **Uno u otro** (`integ::excluyente`): con `ENABLE_VKDAEDALUS` puesto, `vkdaedalus-run` sale en gris con «Es una u
  otra: está puesto ENABLE_VKDAEDALUS» (y al revés). `fijar` apaga el otro al poner uno, por si llega de otra parte.
  Si la línea de un juego traía las dos, ninguna sale en gris, para poder quitar una.
- **Menos trabajo**: con la pestaña a la vista, el panel la rehacía entera cada 2 s (leer y analizar los ~100 KB de
  `localconfig.vdf`, recorrer los ~600 procesos de `/proc` buscando Steam y repintar el panel). Ahora cada 2 s mira
  `integ::firma()` (las fechas de `localconfig.vdf`, `integraciones.conf`, la marca y las carpetas de lo instalado,
  y si Steam está abierto) y solo la rehace si cambió. Steam abierto se mira como lo mira el propio `steam.sh`: el
  proceso de `~/.steam/steam.pid` vivo y llamado `steam` (recordando el último visto); solo sin ese fichero (Steam de
  Flatpak, donde el número es el de su caja) se recorre `/proc`. Antes de escribir en Steam se sigue recorriendo
  `/proc` entero, por si el `steam.pid` mintiera. `rutaLocalconfig` ya no relee `loginusers.vdf` si no cambió. El
  vigilante del servicio sin nada pendiente ya solo miraba si existe la marca (un `stat`); con algo pendiente usa
  ahora la mirada barata.
- **Medido** (`-O2`, este equipo): la mirada de cada 2 s pasa de ~0,6 ms más el repintado a 0,01 ms (Steam abierto,
  por `steam.pid`: 0,002 ms; antes recorrer `/proc`: 0,33 ms; leer `localconfig.vdf`: 0,24 ms).
- **Probado**: la detección en un HOME aparte con un manifiesto de vkDaedalus y un `vkdaedalus-run` falsos; Steam
  abierto y cerrado con un proceso de prueba llamado `steam` y su `steam.pid`; y en el panel, en un KWin aparte, que
  picar vkBasalt (no instalado) y `vkdaedalus-run` (con `ENABLE_VKDAEDALUS` puesto) no cambia nada, y que al quitar
  uno se puede poner el otro. Las capturas del README no se rehicieron.

## La pestaña Integraciones: las opciones de lanzamiento de Steam por piezas, para todos y para cada juego (v1.15)

- Rodolfo: «si suelo ejecutar otras capas de parametros de steam puedes tener una pestaña de Integrations?, quiero
  que tenga parametros de lanzamiento de steam que se puedan activar y desactivar para todo, pero tambien que tengas
  una seccion para gamescope», «quiero que cada juego tenga su herramientita para personalizar esto», y al
  preguntarle cómo llevarlo a Steam: «Write the whole line to Steam». Después, a medio hacer: «Falto el comando a
  correr, seria vkdaedalus-run, tambien a activar o desactivar, y una casilla a escribir de comando personalizado»;
  «en la parte de gamescope no menciones de que es, solo da a elegir la resolucion de salida y la de renderizado,
  solo las abreviaturas de cada una»; «y la casilla de reescalado Gamescope, que al activarla se active lo de
  gamescope, pones igual fsr ahi, y nis».
- **La pestaña** (la 2, la de Rendimiento que ya no se enseñaba; queda entre Juegos y Ajustes):
  - PARÁMETROS: `ODYSSEYVK`, `MANGOHUD`, `ENABLE_VKDAEDALUS`, `NEURAL_FORGE_ENABLE`, `ENABLE_VKBASALT` (con `=1` solo
    puestos: «les quitas el =1 si no lo activan») y un campo de otros parámetros.
  - COMANDO: `vkdaedalus-run`, un comando personalizado (delante de `%command%`) y las opciones del juego (detrás).
  - GAMESCOPE: el interruptor, el reescalado (que lo enciende) con FSR o NIS (`-F`), la resolución de renderizado y la
    de salida por abreviatura (HD, HD+, FHD, QXGA, QHD, 4K; la medida, en la ayuda), `-S stretch|fit`, `-f` y
    `--force-grab-cursor`.
  - LÍNEA DE STEAM: la línea que sale, con Copiar; y si falta escribirla en Steam.
  - JUEGOS CON LO SUYO: los que tienen algo distinto, para ir a ellos.
- **Cada juego**: un engranaje arriba a la derecha de su carátula (al pasar el ratón, en Juegos) abre sus
  integraciones; lo suyo lleva un punto delante, y «Volver a lo de todos los juegos» se lo quita. Cambiar algo al
  valor de todos deja de ser propio.
- **`NEURAL_FORGE_TARGET_EXE`** no se pregunta: con Neural Forge puesto se saca de `neural-forge-cli games` (solo lee),
  o se queda el que traía la línea del juego. Con espacios va entre comillas.
- **La línea entera va a Steam** (`localconfig.vdf` de la última cuenta). Steam solo lo lee al arrancar y lo reescribe
  al salir: con Steam cerrado se escribe al momento; abierto, queda pendiente y el servicio lo escribe en cuanto se
  cierra (dos miradas seguidas sin Steam), con un aviso del escritorio. Antes de cada escritura se guarda una copia en
  `~/.local/state/odysseyvk/steam/` (la primera de todas no se borra nunca; de las demás, las ocho últimas), se
  escribe aparte y se cambia el fichero de golpe, comprobando que se lee de vuelta lo que se quería y que nadie lo
  tocó mientras tanto.
- **Nada se pierde**: la primera vez se leen las líneas que ya hay (Cyberpunk y Harold Halibut con su gamescope, los
  juegos de Neural Forge) y lo que no es como todos pasa a ser propio del juego; lo que se cambie luego por fuera (a
  mano en Steam, o Neural Forge) se toma como propio también, solo lo que cambió. Una línea que no se puede poner por
  piezas (`;`, tuberías, redirecciones, `$(...)`, gamescope sin `--`) se deja como está hasta pedir «Usar la de
  OdysseyVK». Un juego cuya línea ya dice lo mismo (en otro orden, o con `env` de más) no se reescribe.
- **Campos de texto en el panel**: la capa del panel ya da lo que escribe cada tecla (con Mayús, AltGr y las teclas
  muertas del teclado español: ´+a = á, AltGr+4 = ~), Retroceso, Intro, Tab, Supr, flechas, Inicio y Fin, Ctrl+A y
  Ctrl+V (de Klipper o wl-paste), y la tecla que se deja pulsada se repite. Intro o un clic en otra cosa guardan; Esc
  lo deja como estaba (y ya no cierra el panel mientras se escribe).
- Se guarda en `~/.config/odysseyvk/integraciones.conf` (`[todos]` y un `[appid]` por juego, con lo último que se vio
  en Steam). `odysseyvk integraciones ver` enseña cada juego (su línea en Steam, lo suyo, la de OdysseyVK y si falta
  escribirla) sin tocar nada; `escribir` hace lo del panel; `linea "…"` dice cómo se lee una.
- **Probado** sin tocar el Steam de verdad: en una copia de su `localconfig.vdf` (en un HOME aparte) se leyeron las 9
  líneas de Rodolfo, se escribieron cambios (una línea cambiada, `LaunchOptions` añadido a un juego que no lo tenía,
  un bloque de juego nuevo), se tomaron cambios hechos por fuera y se respetó una línea ilegible; el panel, en un
  KWin virtual aparte con su propio bus, con un guion de pasos (`ODYSSEYVK_PANEL_GUION`): la pestaña, MANGOHUD para
  todos, escribir un comando, el engranaje de Cyberpunk, reescalado NIS, FHD, Esc, un clic que guarda lo escrito,
  volver a todos, «Usar la de OdysseyVK» y lo de Steam abierto; el servicio, con `integraciones esperar` (la misma
  función: espera con Steam abierto y escribe al cerrarse); el teclado español, con `ctl teclado es`. **Sin probar**:
  con Steam y el teclado de verdad, y el botón Copiar con Klipper (en el KWin aparte no hay ni Klipper ni wl-copy:
  sale el aviso).

## Tope de FPS de la capa, en Ajustes → Fluidez, y como media exacta (v1.15)

- Rodolfo: «en ajustes en fluidez añade una casilla de Layer FPS cap, y limítala como te digan», y al ver la primera
  medida: «compensate the displayed frames to the loss, for example if you cap 15 and it shows 13 then if someone
  writes 15 make the actual cap 17».
- **El deslizador** «Tope de FPS de la capa» (en inglés «Layer FPS cap»), en Fluidez: 0 = sin tope, y de 10 a 240 de 5
  en 5 (`capa_fps_tope` en serve.conf). Es cuántas veces por segundo copia la capa, dentro del juego, la profundidad y
  la máscara del HUD. Sin tope copia en cada cuadro del juego (~58 por segundo en la partida de Rodolfo) y el visor
  hace 6 mapas por segundo de fábrica (`mapas_por_segundo`): el resto se copiaba para nada.
- **Protocolo versión 6**: `lectorTopeFps`, el visor lo escribe con su latido (del entorno al arrancar,
  `ODYSSEYVK_CAPA_TOPE` puesto por el servicio, y de la orden `capa_fps_tope` de la ventana de ajustes, **al
  momento**) y la capa lo respeta en la copia de la profundidad y en la de la máscara (las capas de un mismo cuadro
  van juntas). El registro de la capa apunta cada cambio.
- **Media exacta** (`tocaPorTope`): cada copia adelanta su turno 1/tope. Con «al menos 1/tope desde la última» los
  cuadros, que llegan a saltos fijos (16,7 ms a 60 fps), dejaban pasar uno de cada dos a partir de 31 de tope: con 45,
  50 o 55 salían 30. Así sale lo que se escribe, en vez de sumarle a mano (el «+2» no vale: lo que se pierde depende
  de los FPS del juego). La «13» que vio Rodolfo no era una pérdida: el lector contaba el arranque.
- **MEDIDO** con vkcube a 60 fps y la capa (`ODYSSEYVK_CAPA_PRUEBA=1`, copia la profundidad en cada pasada): sin tope
  60,2 copias por segundo; con 15, 15,2; con 20, 20,2; con 45, 45,2; con 50, 50,2; con 55, 55,2 (el 0,2 es el borde
  de la ventana de la cuenta). La máscara del HUD con 45 de tope: 45,0 por segundo por el contador de la capa.
- **De paso, un fallo de antes**: `fps_juego` (el interruptor del cuadrito de FPS) no estaba en la lista de órdenes
  que deja pasar el canal vivo (`esOrdenTexto`): la orden de la ventana de ajustes nunca le llegaba al visor y el
  interruptor solo valía desde el siguiente vídeo. Ahora pasan `fps_juego` y `capa_fps_tope`. MEDIDO con el visor
  aparte: arranca con tope 30 (del entorno) y a los 10 s la orden lo pone en 12 y oculta el cuadrito, sin
  reiniciar nada.
- `tools/simula-capa.py` (versión 6) dice lo que pide el visor (la máscara y el tope). Cuatro textos nuevos en los
  siete idiomas; la guía lleva la clave `capa_fps_tope`.

## Las carátulas ya no tapan la ayuda del panel (v1.15)

- Rodolfo, con una captura: la ayuda del botón Copiar (la que sale al pasar el ratón) quedaba **debajo** de las
  carátulas de los juegos; la de Red Dead Redemption 2 tapaba media frase. El panel se pinta en la CPU
  (`Hud::pintarCpu`) y pintaba primero todo lo demás y al final las carátulas, «encima de todo lo anterior»: lo que
  se dibuja después de la lista (la ayuda, la lista de los idiomas) quedaba debajo de ellas.
- Ahora cada carátula apunta cuánto del panel se había dibujado antes que ella y se pinta en su turno: encima de
  lo de antes (su marco al pasar el ratón sigue asomando 3 puntos por fuera) y debajo de lo de después. Debajo de
  la carátula solo van el nombre y el interruptor 3D | 2D, que no la pisan.
- Probado con la biblioteca de Steam de Rodolfo (enlazada, sin tocarla, en un HOME aparte) y el puntero quieto
  sobre Copiar (`ODYSSEYVK_HUD_RATON`): la ayuda sale entera por encima de las carátulas de Yakuza y de Red Dead.

## Versión 1.14: la profundidad de DLSS, el HUD del juego plano y el cuadrito de FPS (v1.14)

- Rodolfo: «Sí, prepárala». El actualizador solo compara el número de `VERSION`, y desde la 1.13 publicada
  (`4c17842`) hay 25 cambios con el mismo número: con 1.14 se ofrecen.
- **Lo que trae** (cada cosa en su entrada, abajo):
  - **La profundidad de DLSS sin Neural Forge**: la capa de Vulkan de OdysseyVK, con `ODYSSEYVK=1` en las
    opciones de lanzamiento del juego, copia la profundidad que el juego le da a DLSS y el visor la usa en vez de
    la red: exacta, en vivo, con su propio plano de la pantalla, 7 ms por mapa, en cualquier modo de DLSS.
  - **El HUD del juego plano en la pantalla**, con su máscara exacta: la imagen donde el juego dibuja el HUD
    (medida en Cyberpunk), reducida en la tarjeta del juego; los menús también.
  - **DLSS a la vista**: Ajustes → Visor 3D dice de dónde viene la profundidad (y los FPS del juego y de la capa,
    y cuánto tapa el HUD); un cuadrito junto a la pastilla con los FPS («DLSS»: juego y capa; «Modelo de serie»:
    el juego); Ajustes → Juegos con DLSS apaga cada cosa; y en medio del pie de la pestaña Juegos, el parámetro de
    Steam (`ODYSSEYVK=1 %command%`) con un botón Copiar.
  - **Arreglos**: el visor ya no se cae con una profundidad del juego a medio escribir; Cyberpunk ya no se cae al
    entrar y salir del menú (lo que la tarjeta del juego lee por la capa ya no se suelta antes de leerlo); /dev/shm
    ya no se llena (7,8 GB de lienzos); el actualizador, probado de punta a punta.
- **Probado en Cyberpunk** (Rodolfo, 17:47, DLSS en Ultra Performance, entrando y saliendo del menú): sin caídas,
  con 32 imágenes de profundidad apartadas que antes eran 32 caídas. **Sin probar**: otros juegos con DLSS (la
  regla del HUD solo se midió en Cyberpunk; si un juego dibuja su HUD de otra forma, no cambia nada) y la capa
  junto a Neural Forge.
- README: la sección de DLSS dice «desde la 1.14», y sus capturas se rehicieron (`tools/capturas-readme.py`: la de
  Ajustes ya lleva la sección Juegos con DLSS; en las demás solo cambia el número de versión del pie). La guía lleva
  las tres claves nuevas de `serve.conf` (`profundidad_dlss`, `hud_plano`, `fps_juego`).

## El parámetro de Steam en el pie de la pestaña Juegos, con un botón Copiar (v1.14)

- Rodolfo: «On the bottom of games tab let user know to add the steam parameter, offer a copy button for it», y al
  ver la primera vuelta (una fila al final de la lista, que con muchos juegos queda abajo del todo): «podrías
  ponerlo aquí, solo que en la pestaña de juegos nada más» y «el parámetro ahí, por el centro», señalando el pie
  del panel. Ahora, **en medio del pie y solo en la pestaña Juegos**: `ODYSSEYVK=1 %command%` y el botón
  **Copiar**; la frase del pie se queda a su izquierda y la versión a la derecha. Al pasar el ratón por el botón,
  la ayuda dice dónde va («Pégalo en Steam: Propiedades del juego → General → Opciones de lanzamiento. Si ya tiene
  opciones, ODYSSEYVK=1 va delante.») y para qué sirve. Al picarlo, el botón se pone verde y dice **«Copiado»**
  2,5 s (en el pie no cabía una frase larga); si no se pudo, lo dice el pie.
- Copiar en Wayland: el portapapeles de una ventana se va con ella (y el panel se cierra al picar fuera), así que
  en KDE se le da a **Klipper**, el portapapeles de Plasma, por el bus de sesión (`org.kde.klipper /klipper
  setClipboardContents`, con el `sd-bus` que ya usa la bandeja): se lo queda él. Sin Klipper, `wl-copy` si está;
  si no hay ninguno, el pie lo dice y el texto está a la vista para escribirlo. Textos nuevos («Copiar»,
  «Copiado», la ayuda, los dos avisos de fallo) en los siete idiomas.
- Probado: el pie pintado con `ctl hud` en español, inglés y alemán, el estado «Copiado» (`ODYSSEYVK_HUD_COPIADO=1`,
  de taller) y que en Ajustes no sale; la captura de Juegos del README ya lo enseña. **Sin probar el botón de
  verdad**: copiar escribe en el portapapeles de Rodolfo y no se tocó sin preguntar; Klipper está en su sesión
  (lo da plasmashell) con ese método.

## Lo que la tarjeta lee por nosotros ya no se suelta antes de leerlo; el ajuste del HUD apaga también la copia en el juego (v1.14)

- **Confirmado en Cyberpunk** (Rodolfo: «arreglado, gracias»; partida de 17:47 a 17:49, Ultra Performance,
  entrando y saliendo del menú): 32 apartadas. El juego destruyó imágenes de profundidad, y soltó su memoria, que
  la tarjeta había leído por nosotros **2 a 6 presentaciones antes** (`[aplazar] el juego destruye … hace 2
  presentaciones`), justo en las vueltas del menú (DLSS pidió 145 números). Antes cada una de esas era una caída;
  ni un Xid desde las 17:35. Las imágenes de profundidad del juego llevan las banderas 0x400400
  (`VK_IMAGE_CREATE_ALIAS_BIT` entre ellas): comparten memoria con otras, y el juego la reutiliza en cuanto termina.

- Rodolfo: «sirvió sin ODYSSEYVK=1, pero creo que es lo de HUD». Con DLSS en Ultra Performance y sin la capa,
  entrando y saliendo del menú, no se cae: la capa está metida. Los tres `gpucrash` de `TXAA_Blurs` señalan la
  lista donde corre DLSS y donde grabamos la copia de la profundidad; el trabajo del HUD va al final del cuadro,
  en la lista del HUD, que estaba «Not started». Y el fallo es siempre el motor de copia leyendo memoria que ya no
  existe.
- La explicación que encaja: la capa graba sus lecturas (la copia de la profundidad, la reducción del HUD, las
  copias de la sonda) en las listas del juego, y la tarjeta las hace después. Si entre medias el juego suelta esa
  imagen porque para él ya no la usa (al entrar o salir del menú en Ultra Performance rehace sus imágenes de
  profundidad y DLSS pide número a varias seguidas: no sabemos cuál lee en cada cuadro), la tarjeta lee memoria
  soltada. Elegir mejor la imagen no basta (17:25).
- **Ahora no puede pasar**: la capa anota cada imagen que hace leer (y la memoria donde está, con ganchos en
  `vkBindImageMemory*`), y si el juego la destruye o suelta esa memoria antes de `kAplazar` = 10 presentaciones,
  no se hace en ese momento: se aparta y se suelta de verdad 10 presentaciones después (o al cerrar el
  dispositivo, esperando a la tarjeta). Cada vez sale en el registro: `[aplazar] el juego destruye … que la
  tarjeta leyó por nosotros hace N presentaciones`, y el resumen cuenta las apartadas. Ese registro dirá si este
  era el mecanismo.
- **Protocolo versión 5**: `lectorQuiereHud`, el visor dice con su latido si quiere la máscara del HUD. Con el
  ajuste «HUD del juego en el plano de la pantalla» apagado, la capa ni toca la imagen del HUD en el juego: el
  interruptor apaga también el trabajo de la tarjeta del juego (antes solo el visor dejaba de usarla).
- Probado sin juego con el visor de anfitrión (`ODYSSEYVK_CAPA_HUD_PRUEBA=1` y un lector en Python que hace de
  visor): queriendo el HUD, 1004 máscaras enteras, y al cerrar el visor destruyó justo la imagen que la tarjeta
  había leído una presentación antes, y su memoria: las dos se apartaron y se soltaron al cerrar el dispositivo,
  sin caída. Sin quererlo, 0 máscaras y «el visor no quiere la máscara del HUD». Con
  `ODYSSEYVK_CAPA_APLAZAR_TODO=1` (de taller: todo pasa por el aplazado) 46 apartadas en 20 s, 15 soltadas en
  marcha y 31 al cerrar, a 60 fps.

## Corrección: las caídas de Cyberpunk desde las 16:50 son todas el mismo fallo de la tarjeta, y la causa no está clara (v1.14)

- Rodolfo: «otra vez se trabó» (17:25, ya con la elección de la profundidad de abajo puesta). MEDIDO en el núcleo:
  **las cuatro** (16:50:42, 17:02:17, 17:07:22 y 17:25:05) son `NVRM: Xid 31 … MMU Fault: ENGINE CE0 … FAULT_P*E
  ACCESS_TYPE_VIRT_READ` (el motor de copia leyendo memoria que no existe), y el juego dejó un `gpucrash` en cada
  una: tres en `TXAA_Blurs` (la lista donde Cyberpunk corre DLSS, la misma donde la capa graba la copia de la
  profundidad) y la de las 16:50 en `PostFX`.
- **Dos cosas de abajo no se sostienen**: lo de las 16:50 no fue (o no solo) el bloqueo del candado, porque el
  núcleo registra este fallo en ese mismo segundo; y elegir la profundidad que el juego usa no lo arregló (volvió
  a las 17:25). Los dos cambios se quedan, porque son correctos de todos modos (esperar con el candado tomado sí
  puede bloquear, y copiar una imagen que el juego no usa sí dio NaN a las 15:54), pero no eran la causa probada.
- Lo que hay: (1) en las sesiones de antes de la máscara del HUD (fases 2, 3 y 3b) ningún fallo, y desde la 3c
  (16:41) todas cayeron en minutos; (2) pero desde las 16:50 Rodolfo tiene DLSS en **Ultra Performance** (1280 ×
  720 por dentro, modelo Transformer2; antes 1920 × 1080), y todas las caídas son con ese modo; (3) y hay
  `gpucrash` del 10 y del 14 de noviembre, antes de que existiera la capa, con el mismo «Device Removed» en
  `TXAA_Blurs`. Hace falta una prueba que separe las dos cosas antes de tocar más.

## Cyberpunk ya no se cae al entrar y salir del menú: solo se copia una profundidad que el juego está usando (v1.14)

- Rodolfo: «cyberpunk now crashed when entering the menu and leaving». MEDIDO: el núcleo dice
  `NVRM: Xid 31 … MMU Fault: ENGINE CE0 HUBCLIENT_CE1 … FAULT_PTE ACCESS_TYPE_VIRT_READ` a las 17:07:22 (el motor
  de copia de la tarjeta leyendo memoria que ya no existía), el juego dejó `gpucrash-…17.07.22.733.log` con
  «Device Removed» (en mitad del posproceso) y el registro de la capa acaba en el mismo segundo, con DLSS
  pidiendo número a varias imágenes de profundidad nuevas (41 a 65) mientras el juego rehacía las suyas.
- La capa copiaba la **última** imagen de profundidad a la que DLSS había pedido número, sin saber si el juego
  la usaba en ese cuadro. Al rehacer las imágenes, esa podía ser una que el juego ya no usaba y que soltó
  mientras nuestra copia esperaba en la tarjeta. Es lo mismo que a las 15:54 dio una profundidad llena de NaN:
  entonces la memoria aún existía.
- Ahora (`elegirProfundidad`) solo se copia una imagen a la que DLSS pidió número **y** que el juego usó como
  destino de profundidad (`vkCmdBeginRendering`) en los últimos 3 cuadros: esa la usa el propio juego en ese
  cuadro y no la suelta hasta acabarlo, después de nuestra copia. Si no hay ninguna, no se copia (el visor
  vuelve a la red en medio segundo). Los ganchos de los destinos de dibujo y de la presentación van ahora
  siempre que se comparte. El registro dice cuál se eligió: `[compartir] la profundidad de DLSS: … (el juego
  la usa como profundidad en este cuadro)`, o `ninguna profundidad de DLSS en uso en este cuadro`.
- Probado sin juego que los ganchos siempre puestos no cambian nada (el visor con la capa: 60 fps, sin
  caídas). La elección en sí solo se puede ver en el juego.

## DLSS a la vista: en Ajustes, en un cuadro junto a la pastilla, y con sus interruptores (v1.14)

- Rodolfo: «when not dlss the title of the fps box should say Stock model». Sin DLSS, con la red sobre un juego,
  el cuadrito dice **«Modelo de serie»** (en inglés «Stock model») con los cuadros nuevos del juego que llegan al
  visor (sus FPS, hasta el refresco) y los mapas de la red por segundo («Red»). Sobre un vídeo no sale. El
  registro del visor lo repite cada 5 s (`[cuadrito] …`). Probado: modelo de serie, juego 60 · red 6 (la «fuente
  60/s» del visor; 6 mapas por segundo, los mismos que en la partida de Cyberpunk); DLSS, juego 116 · capa 58.
- Rodolfo: «en los fps que no son de dlss solo deja los del juego». Con el modelo de serie el cuadrito lleva solo
  «Juego», en un renglón a media altura (el título y el alto, como con DLSS); se fue lo de la red (su cuenta en el
  visor y su texto traducido). El registro: `[cuadrito] modelo de serie · juego N fps`.

- Rodolfo: «puedes hacer que aparezca aqui que se está procesando con DLSS? También junto al HUB quiero que en
  un cuadro pequeño indiques los FPS del layer y los del juego. Que en los settings se puedan desactivar».
- **Protocolo versión 4**: `presentes`, los cuadros que presenta el juego (la CPU de la capa lo escribe en cada
  `vkQueuePresentKHR`, que ahora se engancha siempre que va el HUD). El visor (`ProfundidadJuego::tasas`) saca
  por segundo los **FPS del juego** (de `presentes`) y los **de la capa** (las profundidades que manda, de
  `ultimoSeq`): con la generación de cuadros de DLSS los del juego son el doble.
- **Ajustes → Visor 3D**: con la profundidad del juego, «Red de profundidad: sin usar: la profundidad viene de
  DLSS», y filas nuevas: «Profundidad del juego: DLSS · 1920 × 1080 (la resolución interna)», «FPS del
  juego», «FPS de la capa» y «HUD del juego: en el plano de la pantalla · 9,3 % de la imagen» (o «no se
  encontró su capa»). Las escribe el visor en su `estado` (`prof_juego`, `fps_juego`, `fps_capa`,
  `hud_juego`).
- **El cuadrito**: a la izquierda de la pastilla, de su alto y su negro, «DLSS · Juego 117 fps · Capa 58 fps».
  Solo se mira (queda fuera de la región de entrada) y sale cuando llega la profundidad del juego.
- **Ajustes → Juegos con DLSS**, tres interruptores: «Usar la profundidad de DLSS» (`profundidad_dlss`),
  «HUD del juego en el plano de la pantalla» (`hud_plano`), desde el siguiente vídeo, y «Cuadro de FPS
  junto a la pastilla» (`fps_juego`), al momento (la orden `fps_juego` al visor abierto). El servicio los
  pasa como `ODYSSEYVK_PROFUNDIDAD_JUEGO=0`, `ODYSSEYVK_HUD_PLANO=0` y `ODYSSEYVK_FPS_JUEGO=0`. 17 textos
  nuevos traducidos a los seis idiomas.
- Probado con el visor aparte y el simulador (`--presentes 2`, como la generación de cuadros): el cuadrito
  dice Juego 117 fps y Capa 58 fps; el `estado`, `fps_juego = 116.8` y `fps_capa = 58.4`; la ventana de
  ajustes, pintada con `ctl hud`, enseña las filas nuevas y la sección.

## El juego ya no se congela al cambiar el modo de DLSS; el HUD con los menús encima (v1.14)

- Rodolfo: «i think game froze after changing the dlss resolution». MEDIDO en el registro de la capa: al pasar
  DLSS de 1920 × 1080 a 1280 × 720 la capa rehízo la memoria compartida para la medida nueva y **ahí se paró
  todo** (ni un lanzamiento de DLSS más, ni el resumen de cada 10 s). Para rehacerla esperaba a la tarjeta
  (`vkDeviceWaitIdle`) dentro del lanzamiento de DLSS y con su candado tomado; la tarjeta esperaba trabajo que el
  juego aún no había enviado, y el hilo que lo envía estaba parado en ese candado (lo toman `vkQueueSubmit` y,
  desde la fase 3c, cada `vkCmdBeginRendering`): un bloqueo para siempre. Venía de la fase 2 (el cambio de modo
  solo se había probado con el simulador); los ganchos nuevos lo hicieron mucho más probable.
- Ahora la región vieja **se aparta entera** (su nombre se borra y la nueva es otro objeto, que el visor vuelve a
  abrir) y vive hasta que se cierra el dispositivo: la tarjeta aún puede estar copiando en ella y vkd3d-proton
  puede volver a enviar un buffer de comandos ya grabado. Se espera a la tarjeta solo al cerrar el dispositivo,
  fuera del candado. Y lo mismo en la sonda: sus volcados esperaban a la cola con el candado tomado; ahora fuera.
- **El HUD con un menú encima**: MEDIDO en esa misma partida, en los menús hay **dos** imágenes limpiadas a
  transparente por cuadro (unas 100 líneas por segundo en el registro a ~57 fps, turnándose: 11 000 líneas). El
  visor ya mostraba los menús al 100 % y el juego al 9-13 %. **Protocolo versión 3**: cada hueco del HUD lleva
  hasta dos capas del mismo cuadro (el cuadro se cuenta por las presentaciones) y cuántas lleva; la máscara es
  su unión. Y cada imagen del HUD sale una sola vez en el registro; el resumen dice cuántas máscaras y capas.
- Probado sin juego: la capa en el visor con `ODYSSEYVK_CAPA_HUD_PRUEBA=1` y `ODYSSEYVK_CAPA_RECREAR_CADA=3` (de
  taller: rehace la memoria compartida cada 3 s con otra medida, como un cambio de modo de DLSS): 6 veces en 20
  s, sin tirones (hueco máximo 18 ms, como un cuadro normal) y a 60 fps; el lector la volvió a abrir 7 veces, 1231
  máscaras enteras y ninguna a medio escribir. La unión en el visor con el simulador (`--hud2`): una capa 9,3 % del
  mapa, dos capas 13,6 %. El bloqueo en sí no se puede reproducir sin el juego (depende de sus semáforos).

## Fase 3c: el HUD del juego, plano en la pantalla con su máscara exacta (v1.14)

- Rodolfo: «Sí, conéctala». **La capa** (siempre que comparte la profundidad; `ODYSSEYVK_HUD=0` lo quita)
  sigue los destinos de dibujo (`vkCmdBeginRendering`, sin contar dibujos: eso sigue siendo de la sonda) y
  reconoce la imagen del HUD con la regla medida en la fase 3b: el único destino de color a la resolución de
  la cadena de presentación, RGBA8, limpiado a (0, 0, 0, 0), entero y sin profundidad. Al acabar ese dibujo, en
  el mismo buffer de comandos del juego, la reduce a 960 × 540 con dos `vkCmdBlitImage` a la mitad (cada punto,
  la media de 4 × 4: un trazo fino de una letra no se pierde) y la copia a la memoria compartida, con la misma
  comprobación de cuadro entero que la profundidad. A los destinos RGBA8 grandes se les añade TRANSFER_SRC al
  crearlos (como a la profundidad).
- **Protocolo, versión 2** (`capa/protocolo.h`): tres huecos más con la máscara (2 MB cada uno) y `hudUltimoSeq`.
  Capa y visor van juntos en `install.sh`; un visor viejo con una capa nueva (o al revés) no lee nada y usa la
  red.
- **El visor** (`ProfundidadJuego::leerHud`, `aplicarHud`): el alfa por encima de 6/255 es HUD; se engorda 24 px
  de 4K (MEDIDO en la sonda: el HUD se mece y la máscara puede ser de otro cuadro, hasta ~30 px) con ventanas
  deslizantes, el borde se suaviza (5 × 5) y esa parte del mapa va al plano de apoyo, como un rótulo. Registro:
  `[hud] el HUD del juego al plano de la pantalla: X % del mapa`.
- Probado sin juego, por partes:
  - el visor con `tools/simula-capa.py --hud destino.raw` (el simulador ya habla la versión 2) sobre el cuadro, la
    profundidad y la capa del HUD de un mismo volcado de la sonda: minimapa, misión, botones, iconos y munición en
    el plano de la pantalla (la línea cian de la vista de profundidad), el resto igual; 9,3 % del mapa (el HUD
    es el 4,6 % y el resto es el margen);
  - la capa con el visor de anfitrión y `ODYSSEYVK_CAPA_HUD_PRUEBA=1` (toma su pase de afinado de 4K como HUD)
    y un lector en Python que da el latido: 1096 máscaras en 22 s (una por cuadro a 60 fps), ninguna a medio
    escribir, el contenido es el destino reducido, a 60 fps y sin caídas.
- Falta verlo en Cyberpunk: que la regla salte ahí y cómo se ve.

## Fase 3b: encontrada la capa del HUD de Cyberpunk (v1.14)

- Vuelta de Rodolfo con la sonda de los destinos (16:23 a 16:27): cuatro ventanas (la primera en un menú, 25
  dibujos con destino; las otras tres en juego, 100 a 118), sin caídas y **sin ninguna pasada clásica**: todo va
  por `vkCmdBeginRendering`, como se esperaba de vkd3d-proton.
- **El HUD tiene su propia imagen**: 3840 × 2160 (la resolución de salida) en R8G8B8A8_SRGB, que se limpia a
  (0, 0, 0, 0) y recibe 34 a 36 dibujos. Copiada al acabar ese dibujo trae el HUD entero sobre transparente
  (minimapa, misión, botones, munición, iconos, vida, marcas de distancia) y su alfa es la máscara exacta:
  4,3 a 5,2 % de los puntos, sin un solo falso en neones, luces de freno ni el halo del sol. Después se
  dibuja en ella a 1920, 960, 480 y 240 (sus niveles chicos, para los paneles borrosos) y el nivel grande no
  cambia (4,7 % antes y después).
- **La regla para reconocerla es única** en las tres ventanas: un solo destino de color a la resolución de
  salida, RGBA8, limpiado a (0, 0, 0, 0), con decenas de dibujos; las tres veces la misma imagen. La escena ya
  con posproceso y sin HUD es otro destino (3840 × 2160 RGBA16F, un dibujo) y la mezcla final otro más (RGBA8
  UNORM, igual al cuadro presentado).
- Sobre el cuadro presentado algunos rótulos salen corridos unos píxeles (el HUD se mece al moverse, y la
  copia puede ser del cuadro siguiente): en el 3D la máscara irá un poco engordada.

## Fase 3b, el HUD: la sonda de los destinos de dibujo (v1.14)

- Rodolfo: «Sí, prepárala» (buscar la imagen donde el juego dibuja el HUD: su alfa sería la máscara exacta).
- Con la misma sonda (`ODYSSEYVK_CAPA_SONDA=1`) la capa sigue también **cada dibujo con destino**
  (`vkCmdBeginRendering` / `vkCmdEndRendering`, lo que usa vkd3d-proton) y cuenta sus `vkCmdDraw*`. Al volcar
  se abre una «ventana» desde el lanzamiento de DLSS hasta la presentación siguiente: cada dibujo con destino
  va al registro (`[destinos]`: tamaño, dibujos, si limpia y a qué color, si va tras DLSS en su buffer, si es
  la cadena de presentación) y los de la resolución de salida se copian al acabar ese dibujo
  (`destino-<pid>-<n>.raw` + `.json` con orden, dibujos, limpiado y la imagen; cada imagen hasta 3 veces y 16
  copias por ventana). Lo que va al tamaño de render de DLSS no se copia: es la escena del cuadro siguiente.
  Las pasadas clásicas (`vkCmdBeginRenderPass`) solo se cuentan.
- Los ganchos de dibujo van en miles de llamadas por cuadro: el dispositivo y el contador se guardan por hilo
  (sin candado en cada dibujo), y sin la sonda ni se enganchan.
- Probado con el visor aparte (dibuja con `vkCmdBeginRendering`, como el juego) y
  `ODYSSEYVK_CAPA_DESTINOS_TRAS=300` (de taller, abre la ventana sin DLSS), con `ODYSSEYVK_AFINAR_SIEMPRE=1`
  para que tenga un destino propio en cada cuadro: el pase de afinado (3840 × 2160) copiado al acabar con su
  contenido de verdad, el dibujo siguiente reconocido como la cadena de presentación, el cuadro presentado
  escrito, sin caídas y a 60 fps.

## Fase 3, el HUD: lo que dijo la sonda en Cyberpunk (v1.14)

- Vuelta de Rodolfo con `ODYSSEYVK_CAPA_VOLCAR_CADA=45`: tres volcados (16:07 a 16:09), dos con juego y el
  cuadro presentado del mismo momento (a pie y conduciendo); el primero cayó en una pantalla de carga. De paso,
  el arreglo de arriba en vivo: a las 16:07:29 se tiró un cuadro a medio hacer (159 799 puntos fuera) sin caída.
- **La escena sin HUD es la entrada de color de DLSS**: 1920 × 1080 RGBA16F, descriptor de muestreo (0x2),
  lineal y antes del posproceso. Las 3840 × 2160 con 0xa son su historia interna (codificada, no sirve tal
  cual). La salida 4K (solo almacenamiento, 0x8), copiada al volcar, ya tiene **trozos del HUD**: el juego
  dibuja el HUD en una imagen propia que comparte memoria con esa salida.
- **Máscara por diferencia** (de taller, en el scratchpad): la escena llevada al aspecto final con un ajuste
  global (polinomio en log del color + viñeta, tres vueltas quitando lo que no encaja) y se queda solo lo
  FINO de lo que el ajuste no explica, menos los cantos de la propia escena, en bloques de 8 × 8. Recoge todo
  el HUD (minimapa, misión, iconos, munición, velocímetro, avisos, marcas de distancia); se equivoca en lo que
  brilla (neones, el halo del sol, luces de freno) y en el detalle lejano: ~10 % de los bloques marcados, la
  mitad de más. Sin el ajuste fino (solo el color) eran el 21 %.
- Lo exacto sería la imagen donde el juego dibuja el HUD: su alfa ES la máscara, sin ajustes ni falsos.
  Siguiente paso: buscarla con la sonda (los destinos de dibujo de cada cuadro después de DLSS).

## El visor ya no se cae con una profundidad del juego a medio escribir (v1.14)

- Rodolfo: «no cargo el layer». La capa sí cargó (la sonda dejó dos volcados), lo que se cayó fue **el visor**:
  SIGSEGV en `DepthNet::estimate` a las 15:54:10, justo cuando pasó a «Z normal». MEDIDO en el volcado de ese
  mismo segundo: DLSS acababa de rehacer sus imágenes (números 32 a 39 nuevos) y la profundidad que copió la
  capa era memoria sin escribir: 10 595 NaN, valores de ±3,4e38 y 1,7 millones de puntos fuera de 0..1. Un NaN
  pasa por los recortes (`v < 0 ? 0 : …` lo deja pasar) y además deja la escala del mapa en NaN para siempre.
- `ProfundidadJuego::leer`: en float solo vale 0..1 (una profundidad de verdad no sale de ahí). Un cuadro con
  más de 1 de cada 1000 puntos fuera se tira entero (para ese mapa vuelve la red, con aviso en el registro
  como mucho cada 5 s) y los pocos puntos raros de uno bueno van al fondo. `DepthNet::estimate`, por si acaso:
  la escala solo con valores finitos y el recorte del mapa manda el NaN al fondo.
- Probado con el visor aparte y `tools/simula-capa.py` (nuevo `--otra` / `--cada`: dos profundidades que se
  turnan): con el cuadro malo antes se caía igual (SIGSEGV en `estimate`, reproducido); ahora, turnando el
  malo y uno bueno salpicado de NaN cada 3 s durante 25 s, 9 cambios red ↔ juego, sin caída y a 60 fps.

## Fase 3, el HUD: la sonda de las imágenes de color de DLSS (v1.14)

- Rodolfo: «Yes, start phase 3» (la «capa de DLSS» para el HUD: el HUD no tiene profundidad y toma la de lo
  que tiene detrás; DLSS trabaja con la escena ANTES de que el juego dibuje el HUD, así que comparar sus
  imágenes con el cuadro final dice qué puntos son HUD).
- Primero hay que saber cuál es la escena limpia: en el registro de la fase 1 DLSS pidió números para varias
  imágenes de color (1920 × 1080 y 3840 × 2160 en RGBA16F, unas para leer y otras de almacenamiento) y cómo se
  separa del cuadro final con el etalonaje y los efectos de Cyberpunk. **La sonda** (`ODYSSEYVK_CAPA_SONDA=1`,
  aparte del uso normal): la capa apunta cada imagen de color a la que DLSS pide número y con qué tipo de
  descriptor, las deja copiables al crearlas (las grandes de los formatos que sabe guardar) y, al volcar,
  guarda cada una entera (`color-<pid>-<n>.raw` con su `.json`: medidas, formato, uso y descriptores) junto a
  la profundidad. Con una captura de Steam (F12) del mismo momento, con el HUD a la vista, se diseña la
  máscara del HUD con datos de verdad.
- Probado sin juego: el visor con la capa y la sonda puestas sale igual que sin capa (0,002 de diferencia
  media de 255).
- **Sin captura de pantalla ni orden a mano** (la primera vuelta de Rodolfo no dejó volcados: el `touch` no
  llegó a hacerse, y las capturas F12 no estaban en disco): con la sonda la capa copia también **el cuadro
  que se presenta** (con el HUD), en `vkQueuePresentKHR`, del MISMO cuadro que la profundidad y las imágenes
  de DLSS. La copia va en un buffer de comandos propio en la misma cola, que espera lo que esperaba la
  presentación y avisa con un semáforo propio que es el que espera la presentación de verdad (lo que hacen
  las capas de superposición como MangoHud); la cadena de presentación se crea con TRANSFER_SRC. Y se vuelca
  sola: `ODYSSEYVK_CAPA_VOLCAR_CADA=20` (cada 20 s, como mucho 4 veces, `ODYSSEYVK_CAPA_VOLCAR_MAX`). Con la
  sonda los volcados van a `~/.cache/odysseyvk/sonda` (en disco: cientos de MB cada uno), no a `/tmp`.
  Probado con vkcube (`ODYSSEYVK_CAPA_PRESENTE_TRAS=120`, de taller): el cuadro 120 sale tal cual y vkcube
  sigue.

## Profundidad de DLSS: su propio plano de la pantalla, más rápida, y los modos de DLSS (v1.14)

- Rodolfo, con una captura de la vista de profundidad en Cyberpunk (el piloto en naranja y el plano de la
  pantalla en el asfalto, detrás de él): «performance is better?, also notice that you will need to retrain
  saliencia exclusively for DLSS, maybe separately?»; y «each dlss mode has its own depth, I hope that is not
  an issue».
- **Rendimiento, MEDIDO** (las líneas `[fps]` del visor en su sesión): la mediana del paso de profundidad,
  23,0 ms con la red y 15,0 ms con la del juego; la 5080 ya no corre la red, y leer y subir bajan de 4,3–4,4
  a 3,3 ms. Lo que quedaba era mío: pasar los 2 millones de puntos a cercanía y luego achicarlos. Ahora se
  achica primero (por áreas, en paralelo) y la escala se hace en la rejilla de 518 (es lineal: da lo mismo):
  **6–7 ms** por mapa.
- **El plano, aparte para la del juego** (`DepthNet::estimate`): el histograma pesado por la saliencia daba
  a cada celda un piso de 0,02, y con la profundidad exacta el fondo (el cielo y lo lejano, 0 justo) se
  amontonaba en una casilla y ganaba al sujeto, que se reparte en una docena: el plano caía en el asfalto
  lejano (10 %). Con la del juego: fuera el fondo, peso = saliencia al cuadrado y la mediana pesada en vez
  del pico (17 %: aún detrás, porque la saliencia mide el contraste de profundidad y es alta a los dos lados
  del canto). Y solo cuenta lo que está DELANTE de su entorno (el centro-entorno con signo,
  `centroEntornoSigno`): **64 %, el plano en el piloto** (el contorno cian en su cabeza y sus brazos), la moto
  sale un poco y la calle, el coche y la ciudad quedan detrás. La red sigue con su regla de siempre.
- **Los modos de DLSS**: cada uno renderiza a otro tamaño y su profundidad cambia de medidas; DLSS pide número
  para la imagen nueva, la capa rehace la memoria compartida a su tamaño y el visor ve que el archivo cambió y
  lo vuelve a abrir. PROBADO con el simulador pasando de 1920 × 1080 a 1280 × 720 con el visor en marcha:
  tomó la nueva sola (un segundo con la red en medio). Sin probar: la resolución dinámica, donde el juego
  dibuja en una parte de la imagen y la capa copia la imagen entera.

## La profundidad de DLSS en vivo en el 3D: fase 2, y la capa con `ODYSSEYVK=1` (v1.14)

- Rodolfo: «Change the parameter to ODYSSEYVK=1, and then start phase 2».
- **El interruptor**: la capa se enciende con `ODYSSEYVK=1` en las opciones de lanzamiento del juego (antes
  `ODYSSEYVK_CAPA=1`); `ODYSSEYVK_DISABLE=1` la quita. Nada de OdysseyVK leía una variable con ese nombre.
- **La capa** (`capa/odysseyvk_capa.cpp`, `capa/protocolo.h`): al crear el dispositivo del juego enciende
  VK_EXT_external_memory_host (si la tarjeta la tiene y el juego no la pidió) y, en el primer lanzamiento
  de DLSS, crea `/dev/shm/odysseyvk-profundidad-<uid>`: una cabecera y tres huecos, importados enteros como
  memoria de Vulkan. En cada lanzamiento de DLSS (como mucho uno cada 4 ms) graba en el MISMO buffer de
  comandos: el número del hueco, la copia de la profundidad, el número otra vez y el último número en la
  cabecera, con barreras entre cada paso. La tarjeta del juego escribe directamente en esa memoria; la CPU
  del juego no la toca. Solo copia mientras el visor escriba su latido (menos de 2 s): sin el 3D puesto, el
  juego no paga nada. Al cerrar el juego la borra.
- **El visor** (`src/profjuego.*`, `src/depth.cpp`): cuando la ventana del 3D es un juego (`sobreJuego`)
  y la profundidad llega fresca (de menos de medio segundo, con el número de antes y el de después iguales),
  `DepthNet::estimate` la toma en lugar de la red: Z invertida o normal decidida en cada cuadro, el cielo
  al fondo, los percentiles 1 y 99 a 0..1 y la media por áreas hasta el mapa de 518. A partir de ahí, todo
  como siempre (plano, saliencia, rótulos, bandas, cantos finos), salvo el suavizado en el tiempo, el
  filtro guiado y el relieve local, que estaban para el temblor y lo plano de la red. Si deja de llegar,
  vuelve la red sola. El registro: `[red] la profundidad del juego (la de DLSS, 1920x1080 D32_SFLOAT, Z
  invertida) en lugar de la red`. `ODYSSEYVK_PROFUNDIDAD_JUEGO=0` la apaga.
- **Instalación**: `install.sh` deja la biblioteca en `/usr/local/lib/odysseyvk/` y el manifiesto en
  `~/.local/share/vulkan/implicit_layer.d/` (ahí la ve también el contenedor de Steam); `odysseyvk
  desinstalar` quita los dos.
- **Probado sin juego**: `tools/simula-capa.py` (nuevo) escribe la profundidad volcada de Cyberpunk con el
  mismo protocolo, y el visor aparte, sobre el cuadro de ese mismo momento, la tomó en vez de la red: piloto,
  moto y coche con su silueta exacta y el plano de la pantalla en una línea limpia sobre el asfalto
  (`docs/img/depth-dlss.png`, también en el README). Y el camino de la tarjeta con vkcube y la capa
  (`ODYSSEYVK_CAPA_PRUEBA=1`, solo de taller: comparte la última profundidad creada al acabar cada pasada):
  808 cuadros enteros por la memoria compartida y ninguno a medio escribir; el contenido de vkcube sale
  vacío porque su pasada tira la profundidad al acabar (`storeOp` DONT_CARE), y el de un juego ya se midió
  en la fase 1 (idéntico bit a bit).
- **MEDIDO en Cyberpunk en 3D** (Rodolfo, instalado con `install.sh` y `ODYSSEYVK=1`, 26-nov 15:08–15:12):
  la capa creó `/odysseyvk-profundidad-1000` (1920 × 1080 `D32_SFLOAT`, 3 huecos, 23,7 MB) y mandó 18 533
  cuadros de profundidad al visor, unos 58 por segundo, uno por cuadro del juego; el visor la tomó a los 1,3 s
  de abrir el 3D (`[red] la profundidad del juego (la de DLSS, 1920x1080 D32_SFLOAT, Z invertida)`) y la
  mantuvo de 15:09:32 hasta cerrar el 3D. Volvió a la red tres veces al principio (11, 2 y 4 s), la larga
  con el juego atascado (huecos de 241 ms entre cuadros): una carga o un menú, donde DLSS no corre y no llega
  profundidad nueva, y a medio segundo sin ella se vuelve a la red, que es lo que debe hacer.
- **Lo que quedaba**: Cyberpunk sale sin destruir su dispositivo de Vulkan y la memoria compartida (24 MB)
  se quedaba en `/dev/shm`. Ahora la borra el visor si ve que su juego ya no existe (probado con una de un
  proceso muerto: borrada y no usada) y la capa al salir el proceso del juego.

## /dev/shm se llenaba: 7,8 GB de lienzos de la capa del servicio (v1.14)

- Al probar la fase 2: 1183 archivos `odysseyvk-XXXXXX` en `/dev/shm` (en RAM), 7,8 GB de un día. Los creaba
  `src/overlay.cpp` con `mkstemp` para cada lienzo de la pastilla, el panel o las tarjetas y no borraba el
  nombre. Ahora se borra en cuanto se crea (el compositor solo necesita el descriptor). Los que había se
  borraron (borrar el nombre no molesta a quien aún lo tiene abierto): la memoria usada bajó de 36 a 28 GB.

## El actualizador, probado de punta a punta (v1.14)

- `~/.local/state/odysseyvk/actualizar.log` del 26-nov a las 13:56: con la 1.12 instalada, el botón
  Actualizar bajó la 1.13 de GitHub, la compiló, la instaló pidiendo la clave por pkexec (la ventana de
  polkit, lo que nunca se había probado), reinició el servicio y acabó en «hecho (0)».

## La profundidad de DLSS sin Neural Forge: la capa de Vulkan de OdysseyVK, fase 1 (prueba) (v1.14)

- Rodolfo: «I actually want my users to be able to use this without neural forge, just with the game
  dlss itself».
- **Cómo se encuentra** (`capa/odysseyvk_capa.cpp`): con Proton, DLSS corre como núcleos de CUDA lanzados
  por Vulkan (`vkCmdCuLaunchKernelNVX`, VK_NVX_binary_import) y a cada imagen que esos núcleos leen le
  pide antes un número al controlador (`vkGetImageViewHandleNVX`, VK_NVX_image_view_handle). La capa
  apunta esos números y busca en los parámetros de cada lanzamiento (y en su búfer «extra» de CUDA)
  cuáles aparecen: la imagen de formato de profundidad entre ellas es la del juego, la única de ese
  formato que DLSS recibe. Neural Forge empezó así y luego pasó a un `_nvngx.dll` propio dentro de
  Proton porque necesitaba TODAS las entradas de DLSS con su papel; para la profundidad sola basta el
  formato. El controlador de Rodolfo (620.73) tiene las dos extensiones.
- **Fase 1, esta**: un registro (`/tmp/odysseyvk-capa-<uid>/capa-<pid>.log`: dispositivos, los números
  pedidos con su tamaño y formato, los primeros lanzamientos con las imágenes que leen, un resumen cada
  10 s) y, a petición (`touch /tmp/odysseyvk-capa-<uid>/volcar` u `ODYSSEYVK_CAPA_VOLCAR_TRAS=<s>`), un
  cuadro de esa profundidad en `profundidad-<pid>-<n>.r32f` con su `.json`. Capa implícita que solo se
  carga con `ODYSSEYVK_CAPA=1` (por juego, en sus opciones de lanzamiento, como MangoHud o Neural Forge);
  con la librería de C++ dentro y nada más que libc y libm (el contenedor de Steam). Atajos de prueba,
  anotados para la fase 2: la copia va en diseño GENERAL con barreras anchas (el controlador de NVIDIA no
  mira los diseños), TRANSFER_SRC añadido a las imágenes de profundidad, y el volcado espera a la cola una
  vez.
- **Probado** sin juego: `vulkaninfo` con la capa (instancia y un dispositivo por tarjeta, sin fallos) y
  el visor de OdysseyVK en el compositor aparte con la capa puesta: la misma imagen que sin ella (0,004 de
  diferencia media de 255, como entre dos pasadas sin capa). Falta lo que importa: un juego con DLSS.
- **MEDIDO en Cyberpunk** (Rodolfo, con `ODYSSEYVK_CAPA=1` y Neural Forge a la vez, los dos volcados con
  la misma orden): DLSS pide número para una imagen **1920 × 1080 `D32_SFLOAT`**, el tamaño de render (y
  vuelve a pedirlo al cambiar de escena: 4 veces en un minuto, con 39 números en total). Los parámetros
  de los lanzamientos no vienen como esperaba («0 parámetros, 1 extra», y ese extra vale `0x1`, que es la
  clave `CU_LAUNCH_PARAM_BUFFER_POINTER` de CUDA: la lista de extras sigue después aunque `extraCount`
  diga 1), así que en 90 000 lanzamientos ninguno «nombró» la profundidad y la copia no se grababa. Ahora,
  si los parámetros no la nombran, se copia la última imagen de profundidad a la que DLSS pidió número.
  Resultado: la profundidad de la capa y la del volcado de Neural Forge del mismo cuadro son **idénticas
  bit a bit** (2 073 600 de 2 073 600 puntos, diferencia 0; con el cuadro siguiente, correlación 0,9986,
  lo que se movió). Sin Neural Forge, OdysseyVK tiene exactamente la profundidad que recibe DLSS. (El
  primer volcado salió en la carga, un suelo pelado: la orden llegó antes de la partida.)
- Fases que siguen: (2) compartir la profundidad en vivo con el visor (mismo GPU, sin copias por la CPU) y
  usarla en vez de la red en ese juego, leyendo la lista de extras de CUDA hasta su fin para saber qué
  lanzamiento usa qué imagen (los juegos que alternan dos profundidades); (3) el HUD (no tiene
  profundidad: la salida de DLSS, sin HUD, dejaría separarlo), la instalación y un interruptor por juego
  en la pestaña Juegos.

## Los rótulos ya no aplanan el mundo: renglones en medio, y en los juegos solo frases (v1.13)

- Rodolfo: «Yes, fix it» (el detector de rótulos que el volcado de Cyberpunk dejó a la vista: apoyaba en
  la pantalla el 23,2 % del cuadro y partía al piloto y la moto en franjas planas).
- **Por qué** (`src/depth.cpp`, `anclarTexto`; `texto-detect --secuencia` con la foto quieta 4 mapas,
  como un personaje que no se mueve en la pantalla): no era el relleno de los renglones de abajo (no
  saltó), sino la detección celda por celda y su filtro de forma, que dejaba pasar en todo el cuadro
  bloques de hasta 10 filas: fachadas y letreros, la baranda, el torso del piloto (16,9 % fuera de las
  bandas). La memoria los da por quietos al segundo mapa.
- **Banco** (22 fotos: doce de subtítulos `a`–`f`, `k`/`r`/`ft`, las de `pruebas-texto`, el mástil y
  Cyberpunk), medido antes y después, en las bandas y en medio. Todo lo marcado en medio era falso: el
  estampado de una camiseta, un micrófono, el mástil, la ciudad.
- **En medio**: un bloque (celdas que se tocan, con una de hueco entre palabras) solo si es un renglón:
  3 filas como mucho, 6 columnas o más, el doble de ancho que de alto; y si aun así pasa del 2 % del
  cuadro, nada en medio. Celda por celda no bastaba: las celdas de la orilla fina de una mancha grande
  pasaban.
- **En las bandas**: la regla de siempre por celda y, por bloques, fuera los de más de 6 filas con dos
  celdas de margen, que además pasan a la memoria del veto (con 6 ya por celda, el mástil salía a
  trozos sueltos y el veto de las cuerdas no lo veía).
- **En los juegos** (`steam_app_*` o gamescope, lo que ya sabía el visor; `ODYSSEYVK_JUEGO=1` para
  probar; `texto-detect --juego`): en las bandas solo lo que va en una racha de 16 celdas o más por
  fila (con huecos de 4) y la fila de encima y de debajo. Con cajas no servía: el subtítulo tocaba el
  tambor de encima. La moto de Cyberpunk era un bloque de 13 × 4, la forma de un subtítulo corto.
- **Medido**: Cyberpunk 23,2 % → 7,8 % (vídeo) y 1,3 % (juego); en el visor, el mapa de la red pasa de
  0,888 a 0,908 de parecido con la profundidad de DLSS y, en modo juego, el piloto y la moto salen de
  una pieza. Subtítulos en modo vídeo casi iguales (10,2 → 10,1 %, 7,5 → 7,4 %, `t1` 15,0 y el cintillo
  2,4 iguales); en modo juego se quedan los renglones y se van las pegatinas y el pelo que se marcaban
  (6,7 %, 5,8 %, `t1` 10,8 %). El mástil, 13,0 → 7,3 % (vídeo) y 1,0 % (juego); la guitarra de `k`,
  0,4 → 0,6 % (vídeo) y 0 % (juego). Las texturas y la foto sin texto siguen en 0.

## La profundidad del juego contra la de la red: `tools/comparar-profundidad.py` (v1.13)

- Rodolfo, sobre Neural Forge (github.com/labj1987/neural-forge, su ticket #63): «check if any
  improvements can be made here with it, either with dlss5 method of getting the depth or with the gpu
  part»; y «Yes, set up the test».
- **Lo de la tarjeta, MEDIDO**: la red ya va en la 5080 (`red = 1`) y ahí es más rápida, 10,5 ms por
  mapa contra 15–23 en la 4090 (que además lleva el escritorio), con los mismos ocho cuadros de la nave
  (`odysseyvk mapa --red 0|1`). La 5080 va en x4 detrás del chipset (`lspci -t`), que es lo que frena a
  Neural Forge (unos 3,5 GB/s con cuadros enteros); a OdysseyVK no: mueve ~1 MB por mapa.
- **La profundidad de DLSS**: Neural Forge, dentro del juego, sabe cuál de sus imágenes es la
  profundidad que recibe DLSS y la puede volcar (`neural-forge-cli shmctl dump-cache --holds 2`, con
  el modelo antes de DLSS y la caché de ediciones puesta, que es lo de fábrica). La herramienta nueva
  toma ese volcado y la pone frente a la de la red en el mismo cuadro: tonifica el color HDR, pasa la
  profundidad a cercanía (Z invertida o normal, la decide sola; las dos son lineales en 1/distancia,
  como la disparidad que da la red, así que no hacen falta los planos cercano y lejano del juego),
  corre la red en el visor aparte, pone el plano de la pantalla del juego en la misma cosa que eligió
  la red y pasa los dos mapas por el mismo camino (`--mapa-fijo` con la imagen de guía): vista de
  profundidad y ojo izquierdo (sonda 16) de cada uno, en `comparacion.png`, y los números en
  `resumen.txt` (entre ellos el parecido de los dos mapas por rangos).
- **Probado** con un volcado de mentira (la nave a 1024 × 768 con una profundidad de Z invertida hecha
  de un mapa conocido): Z invertida detectada sola (215 506 puntos a 0), el plano en la misma cosa
  (0,39 la red, 0,389 el juego), parecido 0,967, y el recorte de los ojos en el canto de la nave (al
  principio caía en el borde de la banda: se busca solo dentro de la imagen). Falta el volcado de un
  juego de verdad.
- Neural Forge es AGPL y OdysseyVK MIT: de ahí se toman ideas o se le habla como otro programa, no
  se copia código.
- **MEDIDO con Cyberpunk** (Rodolfo hizo el volcado: 2 cuadros, render 1920 × 1080, `D32_SFLOAT`,
  Z invertida, 11 % de cielo; tercera persona en moto, ciudad detrás). La profundidad de DLSS es exacta:
  el piloto, el manillar, el coche de detrás, las farolas, la palmera y los cantos de los edificios,
  limpios a la resolución de render. La de la red es borrosa (el coche y las farolas casi no están) y
  el parecido por rangos es 0,888: el orden general coincide, el detalle no. Y salió un fallo NUESTRO:
  el detector de rótulos apoyó en la pantalla el **23,2 % del mapa** (`[rotulos] apoyados en la
  pantalla`), y el piloto y la moto quedaron cortados en franjas horizontales planas; con
  `ODYSSEYVK_TEXTO=0` el mismo cuadro sale de una pieza. En un juego en tercera persona, el personaje
  centrado y los letreros de la ciudad pasan por subtítulos. En los ojos, la profundidad exacta deja el
  canto del piloto duro y su hueco más grande (rayas en el pelo con el plano lejos); con la red el
  canto se emborrona y no se nota. (El `-rotulos.pgm` de ese volcado salió cortado: 33 815 de 270 415
  bytes.)

## Versión 1.13: lo que llegó después de la primera 1.12 (v1.13)

- Rodolfo: «Check the update system, it works?» y, con lo que salió, «Yes, bump to 1.13». El
  actualizador solo compara el número de `VERSION` (`versionMasNueva`), y desde la primera subida de la
  1.12 (`fec602d`, 26-nov 10:05) se subieron quince cambios más con el mismo número: a quien tenía esa
  1.12 el panel le decía «estás al día». Con 1.13 se los ofrece.
- **Lo que trae** (cada cosa en su entrada, abajo o dentro de la 1.12): el veto de las cuerdas con
  memoria; salir de pantalla completa sin el minuto de espera y sin la vigilia que no dejaba volver a
  entrar; la línea `[arranque]` y el arranque del 3D en paralelo; la pastilla con el botón de
  encendido y Neural | Rendimiento en marcha (y fuera de la cabecera del panel); los ajustes más
  compactos (Programa en una fila); las imágenes del README rehechas; la vista de depuración de la
  profundidad (Ctrl+Mayús+9); atajos de una tecla sola, la PB (Pausa/Inter), Impr Pant, Bloq Despl,
  Menú y F13–F24; «El juego tapa el 3D» con cuenta atrás de 4 s.
- **El actualizador, revisado**: la mirada a GitHub funciona (la del servicio de Rodolfo, a las 13:51,
  leyó 1.12 de la API; la API y raw contestan en menos de 0,3 s) y una copia limpia de `main` compila
  los dos binarios (10 s). Sin probar todavía de punta a punta: picar Actualizar de verdad, que pide la
  clave por pkexec desde la unidad transitoria, instala y reinicia el servicio.
- **`install.sh`**: la limpieza de lo del nombre viejo (`odyssey3d`) pedía root con `sudo` suelto; con
  `odysseyvk actualizar` no hay terminal donde pedir la clave y la actualización acababa «fallida»
  aunque ya estuviera instalada. Ahora va por `$su` como el resto (pkexec en el actualizador). En este
  equipo no hay nada del nombre viejo, así que aquí no se notaba.
- README y guía: «desde la 1.12» pasa a «desde la 1.13» en lo que llegó después (la tecla sola, la PB).

## Versión estable 1.12: lo de la rama `experimental` pasa a la normal (v1.12)

- Rodolfo: «déjalo en 10, ya quiero subir un estable». Todo lo de abajo hasta la 1.11 entra en la
  versión normal; el detalle y las medidas de cada cosa, en su entrada.
- **Vídeo**: sin tarjeta; el tipo de vídeo (normal, lado a lado, arriba y abajo) se detecta solo en los
  primeros segundos (18 de 18 ventanas de 3 s bien en seis vídeos). La red de fábrica es la fp16 (el
  mismo mapa, 35 % menos de tiempo y 260 MB menos en la tarjeta). Las bandas negras de las películas,
  en el plano de la pantalla. El HDR del escritorio siempre.
- **Relieve**: la pastilla azul con **Auto | Manual**. Auto: la saliencia pone el sujeto en la
  pantalla, el alcance sigue la distancia de los ojos (0,3° de paralaje, nunca menos que los topes de
  fábrica: x1 hasta 75 cm) y la
  Salida se calcula para el mayor relieve sin aplastar (0,9 % del cuadro en los topes en vez del 61 %).
  Cantos con el alcance grande sin dientes ni franjas grises; subtítulos de una pieza en la pantalla
  (letras a 0–1 px entre los ojos). Mezcla entre las vistas de fábrica al 10 % de Samsung, con
  deslizador.
- **Modo rendimiento** (interruptor **Neural | Rendimiento**): profundidad sin la red en el procesador,
  con el menú del rayo (tres preajustes medidos y cuatro deslizadores); el VRR como el escritorio.
- **Juegos**: la pastilla y el panel le quitan el cursor al juego y se lo devuelven; el pin deja el
  panel encima; la pastilla se ve unos segundos al entrar y se oculta.
- **Panel**: cabecera en dos filas (el logo abre la página principal; Juegos, Ajustes y Experimental
  centradas), se redimensiona y recuerda su sitio, color de resaltado de KDE sobre negro translúcido;
  Ajustes trae el rendimiento, la Fluidez (VRR y tope de arrastre) y Desinstalar plegado; secciones
  plegables; Juegos fuera del panel durante un 3D.
- **Sin la pestaña Experimental ni la rama** (Rodolfo: «manda anticipar la cabeza a Ajustes», «luego de
  eso ya deshazte de experimental», «también de la pestaña experimental»): la anticipación de la
  cabeza pasa a *Ajustes → Seguimiento de la cabeza* (con su ayuda); la prioridad del visor en la
  tarjeta, a *Ajustes → Fluidez*, junto al VRR y al tope de arrastre (que ahora solo topa donde la
  imagen se mueve); la red a su proporción (`exp_red`) y el mapa más grande (`exp_mapa`) se quedan solo
  como claves de serve.conf. El alcance del relieve deja de tener deslizador: Manual usa el mismo
  automático que Auto (el `exp_topes_red` guardado ya no se pasa al visor; el suyo
  valía x3,05, apuntado al pasar de Auto a Manual cuando el automático iba a un grado, y al cambiar de
  modo ya no se apunta). `ODYSSEYVK_TOPES_RED` lo fija a mano para el taller. Doce textos sin uso,
  fuera de las traducciones. La rama `experimental` se cierra: todo queda en `main`.
- Arranque del visor sin caché de sombreadores: 4,1 s (8,0 en la 1.11).
- **El alcance, más bajo: 0,4° y después 0,3°.** Con medio grado aún veía los trastes de una guitarra
  partidos y dobles, y rayas junto a los cantos («creo que sigue siendo por el alcance, ¿se puede bajar
  más tu rango?», «¿le bajas más al rango de alcance?»). Primero 0,4° (x1,15 a 65 cm) y luego **0,3°**,
  nunca por debajo de los topes de fábrica: x1 hasta 75 cm (su distancia: 61–76 cm), x1,06 a 80 cm,
  x1,33 a 1 m. MEDIDO en Auto en el visor aparte: x1,00 a 65 cm.
- Las rayas junto a los cantos que veía a simple vista (una bola de mimbre, un cuadro, el marco de una
  puerta) no salen del relieve: la vista de cada ojo antes de tejer está limpia. Son los armónicos de la
  trama (v0.41), que salen donde las dos vistas difieren; crecen con el alcance. El temblor de fase
  (`fase_temblor = 0,1`, medido sobre la fórmula en la v0.41) no se pudo medir en su cuadro (la textura
  tapa los picos) y queda para probarlo en marcha con `odysseyvk ctl ajuste fase_temblor 0,1`.
- **El mástil de una guitarra se tomaba por subtítulo** (`mzHAeJhpdYc`, cámara quieta, el mástil
  cruzando la banda de abajo): el detector de rótulos de siempre ve las cuerdas y los trastes como
  letras, y el apoyo entraba y salía del mástil de un segundo a otro (0–10 % del cuadro en su registro),
  con los trastes cambiando de separación. Ahora un bloque de la banda solo es texto si: es horizontal
  (el centro de la racha de cada fila se corre menos de una celda por fila; el mástil, 2,2–3), y no tiene
  cuerdas (en la imagen chica de 518 cada cuerda es una cresta clara de 1–2 puntos, 10–50 niveles sobre
  la madera, recta; se buscan rectas de −12 a +12° con cresta ≥ 12 niveles que crucen el 60 % del bloque
  sin cortes de más de 3 puntos: con 3 o más, son cuerdas). El que no lo cumple pierde el apoyo; el
  renglón entero solo se rellena si además está centrado (a menos del 15 % del centro). MEDIDO en cinco
  cuadros del vídeo: apoyado 7,5–9,4 % → 0 % en cuatro y 5,4 % en el otro (un trozo con solo dos cuerdas
  a la vista); los tres subtítulos (0 cuerdas en sus renglones) y los cuadros sin texto, igual. (Contar
  bordes en vez de crestas no encontraba las cuerdas: con la inclinación se escapaban de la fila.)
  - **Y con memoria.** En vivo seguía («mejoró pero sigue»): su registro aún saltaba del 1,7 al 10,6 %,
    porque basta un mapa en el que la mano tape cuerdas para que ese trozo de mástil vuelva a pasar por
    texto. Lo que una vez salió cuerdas sigue vetado mientras su marca no baje de 0,25 (se gasta un 5 %
    por mapa: unos 4,5 s a 6 mapas por segundo). MEDIDO con `texto-detect --secuencia` sobre 20 s de su
    vídeo a 6 mapas por segundo: apoyado medio 1,4 % → 0,1 %, máximo 6,3 % → 1,4 %, mapas por encima del
    2 % 22 → 0, saltos de más de 3 puntos entre mapas seguidos 10 → 0. Con 20 s del vídeo subtitulado,
    idéntico con y sin la memoria (los 8 saltos que quedan son los subtítulos que entran y salen).
- **Sin el minuto de espera al salir de pantalla completa.** Rodolfo: «ya arrancó pero súper lento». Su
  registro: dos sesiones cerradas porque él sacó el vídeo de pantalla completa a los 11 y 12 s, y el
  servicio aplicó «la sesión se cerró apenas empezó: esperamos un minuto» (dos sesiones de menos de 20 s
  seguidas); el 3D volvió a los 60 s exactos. Ahora, si la sesión la cierra esa salida (con la sesión ya
  asentada, fuera del cambio de modo del panel), no cuenta como muerte: la espera es la de siempre, 8 s,
  y nunca sube al minuto. Las muertes de verdad siguen con sus 8 s y su minuto.
  - **Primero lo bajé a 1,5 s y fue un error**: al salir, Firefox se vuelve a poner a pantalla completa
    él solo un segundo después y la vigilia del servicio se la quita (hasta tres veces en ~6 s); con
    1,5 s el 3D se relanzaba justo entonces, la vigilia le quitaba la pantalla completa y la sesión se
    cerraba, en bucle cada ~6 s («me pasó como 4 veces»: cuatro vueltas en su registro entre las
    10:18:53 y las 10:19:12, con el panel parpadeando). Los 8 s cubren esa vigilia; con el servicio
    anterior, en su registro de las 10:17, la salida y la vuelta fueron limpias.
  - **Y sin la vigilia tras salir tú.** «Al salir de un vídeo ya no puedo entrar»: cada vez que el vídeo
    salía de pantalla completa, el servicio armaba 6 s de vigilia que le quitaba la pantalla completa al
    navegador si la volvía a poner (hasta tres veces); pensada para la página que se queda a pantalla
    completa cuando el vídeo se va, también se armaba cuando el que salía era el usuario, y el que
    elegía otro vídeo y pedía pantalla completa en esos segundos se quedaba fuera (su registro: «se la
    quitamos (1/3)», «(2/3)», «(3/3)» a las 10:32:03–05 y el 3D a las 10:32:10). Ahora, tras una salida
    de pantalla completa con la sesión asentada, la vigilia no se arma (sigue para el vídeo que se va,
    cuando el navegador dice que no hay vídeo, y para el cambio de monitor) y, como ya nada le quita la
    pantalla completa al navegador, el bucle de arriba no puede darse: la espera baja a 3 s. No probado
    en el panel.
- **La línea `[arranque]`** («¿ayuda al rendimiento cargar el modelo en RAM?»): el archivo de la red ya
  vive en la caché del sistema (leerlo tarda 5 ms) y cada mapa tarda lo mismo (13–14 ms); lo que cuesta es
  arrancar la red en la tarjeta en cada sesión (~0,7–0,8 s de los ~2,3 s que tarda el 3D en entrar).
  Para repartir el resto, el visor apunta cada paso del arranque desde que empezó el proceso (de
  `/proc/self/stat`; el reloj del registro no sirve porque el visor escribe por tandas) y, en el primer
  cuadro en 3D, escribe `[arranque] hasta el visor · panel · lente · scaler · ventana · vulkan · red ·
  camara · pedir la captura · primer cuadro capturado · primer mapa · primer cuadro en 3D · total`. En el
  visor aparte (sin panel, lente, scaler, cámara ni captura): 1210 ms, de ellos red 812 y vulkan 222.
- Lo de «no está el modo Auto» era otra cosa: desinstaló y volvió a instalar, y Desinstalar borra la red
  de profundidad (sin ella no hay Auto, la pastilla cae a la del vídeo dividido); además pasó el
  interruptor a Rendimiento un rato. Con la red bajada de nuevo y Neural puesto, Auto en su registro.
- **Probado en su panel**: la mezcla al 10 % («sí bajó»); el alcance a medio grado sirvió («este último
  sirvió súper bien») pero pidió menos. **No probado en su panel**: los subtítulos de una pieza, el fondo negro y el
  interruptor Neural (medidos sin panel, en el visor aparte y con `ctl hud`).

## «El juego tapa el 3D» se cierra sola a los 4 s (v1.13)

- Rodolfo: «autoclose this game covers the 3d warning, 4 second countdown». La tarjeta salía a la
  segunda vez que un juego a pantalla completa exclusiva mandaba el visor detrás y se quedaba hasta
  darle a Entendido (o 60 s), encima del juego.
- **Ahora** (`src/main.cpp`, `src/vkui.*`): `mostrarAviso` admite una cuenta atrás; esta tarjeta la
  lleva de 4 s. El botón dice cuánto le queda, «Entendido (4)» … «(1)» (el número baja cada segundo:
  el bucle del servicio mira cada 200 ms como mucho y repinta la capa solo cuando cambia), y a cero se
  cierra con `[serve] la tarjeta de aviso se cierra sola (4 s)` en el registro. Entendido y el aspa la
  siguen cerrando antes. La otra tarjeta de aviso (el juego se cerró a los N s de entrar en 3D) sigue
  sin cuenta.
- **Probado**: pintada con `ctl hud` (`ODYSSEYVK_HUD_AVISO=tapa`, `ODYSSEYVK_HUD_AVISO_CUENTA=N`, nuevo)
  en inglés y en español, con 4 y con 1. La cuenta en marcha con un juego de verdad, sin probar: hace
  falta un juego a pantalla completa exclusiva que tape el visor dos veces.

## Atajos de una tecla sola (v1.13)

- Rodolfo: «allow one key being assigned too, on the keyboard shortcuts».
- **Panel** (`src/preferencias.cpp`): al esperar la combinación nueva, una tecla sola (o con Mayús nada
  más) ya no se rechaza con «necesita Ctrl, Alt o Meta»: se guarda, y el pie avisa de que, mientras
  haya 3D, esa tecla ya no llega al vídeo ni al juego (el atajo es global del escritorio mientras hay
  un visor abierto). Los modificadores solos se siguen ignorando hasta que llega la tecla de verdad,
  así que pulsar Ctrl de camino a Ctrl+X no se apunta.
- **Visor** (`src/vkview.cpp`): la tecla con el visor enfocado se busca en los atajos aunque no lleve
  Ctrl, Alt ni Meta, y manda sobre las teclas propias del visor (P, C...), que valen solo si no son de
  ningún atajo.
- **Golpe de prueba** `tecla:<combinación>` (`tecla:d`, `tecla:ctrl+shift+9`): la misma búsqueda que
  una tecla pulsada, sin teclado. Probado en el visor aparte con `atajo_ver_profundidad = D` en un
  HOME aparte: `tecla:x` no es de ningún atajo y `tecla:d` pone la vista de profundidad; el panel
  enseña la D en la lista. Sin probar todavía: el registro de una tecla sola en kglobalaccel con el
  visor de verdad (`ctl atajos` lo registraría en el escritorio de Rodolfo).
- Traducciones: fuera «{}: el atajo necesita Ctrl, Alt o Meta»; nueva la del aviso.
- **La tecla PB** (Rodolfo: «I want to set PB key, i cant»): el panel solo entendía letras, números,
  signos, F1–F12, flechas, las de moverse por la página y Espacio; cualquier otra se ignoraba al
  esperar la combinación, así que Pausa/Inter no había modo de ponerla. Ahora también Pausa/Inter
  (`Pause`), Impr Pant (`Print`), Bloq Despl (`ScrollLock`), Menú (`Menu`) y F13–F24, en los tres
  sitios que nombran teclas: el panel (`src/overlay.cpp`, por xkb), el registro en kglobalaccel
  (`src/atajos.cpp`, con su código de Qt) y la tecla con el visor enfocado (`src/vkview.cpp`, por GLFW).
  Con el visor enfocado el atajo se busca antes que la PB de siempre (sacar la pastilla), así que si
  la Pausa es de un atajo, manda el atajo. Probado en el visor aparte con `atajo_ver_profundidad =
  Pause`: `tecla:pause` pone y quita la vista de profundidad y el panel enseña «Pause»; en KDE la
  Pausa no la tenía ningún otro atajo.

## Vista de depuración de la profundidad: Ctrl+Mayús+9 (v1.13)

- Rodolfo: «is there a chance to create a depth mode, for debugging, that shows both the depth
  elements and marks the saliency ones with other color».
- **Qué se ve**: la misma imagen a los dos ojos (plana, sin relieve). En gris la profundidad que usa la
  trama, ya afinada y con los rótulos apoyados (blanco = cerca); en naranja, con la sombra de la
  profundidad debajo, donde pesa la saliencia, que es la que elige el plano de la pantalla en Auto
  (relativa al punto que más pesa en esa estimación); en cian lo que cae en el plano de la pantalla
  (a ±0,008 de él), lo que se queda quieto y nítido. Bajo la pastilla, la leyenda de los colores
  durante 12 s, o que hace falta el conversor si el vídeo va dividido sin suavizar.
- **Cómo** (`src/depth.*`, `src/vkweave.*`, `src/vkview.cpp`, `shaders/weave.frag`): la saliencia ya
  se calculaba en cada estimación en una rejilla de 65 × 65 (un punto cada 8 del mapa) para pesar el
  histograma del plano; ahora `estimate` la deja guardada (`rejillaSaliencia`), el hilo de la red la
  copia junto al mapa y, con la vista puesta, sube como textura R8 (enlace 6 del sombreador, buffer de
  paso fijo como el del mapa). `tun11.y` enciende la rama, antes de buscar las vistas de cada ojo;
  apagada no cuesta nada.
- **Atajo** `verprof` (Ctrl+Mayús+9, se cambia en el panel como los demás: «Ver la profundidad
  (depuración)»), golpe de prueba `verprof` y `ODYSSEYVK_VER_PROFUNDIDAD=1` para arrancar con ella.
- **Probado** en el visor aparte con un cuadro de una persona hablando: la cara en naranja, el contorno
  del plano de la pantalla en cian alrededor de la cara y los subtítulos apoyados en cian; un toque
  la pone, dos la quitan (la trama vuelve a ser la de siempre). La leyenda sale bajo la pastilla y la
  fila nueva en los atajos del panel. El arranque de Vulkan pasó por 1,0 s las dos primeras veces (la
  caché de sombreadores, por el cambio) y volvió a 0,2 s.
- Traducciones: cuatro textos nuevos.
- **En el README** («share the new mode on the readme with capture»): un párrafo en «Qué hace bueno
  el 3D» y `docs/img/depth-debug.png` (y su versión en español): el cuadro de la nave de las otras
  imágenes y su vista de depuración, sacada del visor aparte con `ODYSSEYVK_VER_PROFUNDIDAD=1` y
  `--dump` (el cuadro, a 3840 × 2160, que es lo que pide `ODYSSEYVK_FUENTE_PNG`). La saliencia cubre la
  nave y el plano de la pantalla (0,92 del rango) le recorre el casco. La imagen sin rótulos está en
  `docs/img/fuente/` y `tools/capturas-readme.py` le pone los de cada idioma.

## Los ajustes, más compactos (v1.13)

- Rodolfo: «en settings reacomoda las opciones que quedan para ser compactas y útiles»; «los juegos
  están bien así» (Juegos no se toca).
- **Página principal** (`src/preferencias.cpp`): *Vídeo* con la fuerza del relieve y suavizar los
  vídeos divididos; *Pantalla* con reducir el fantasma y suavizar entre las vistas; luego los atajos y
  las aplicaciones que no preguntan, plegados. Fuera la fila «El tipo de vídeo se detecta solo» (va en
  la ayuda de suavizar los vídeos divididos); seguir la cabeza con la cámara pasa a Ajustes.
- **Ajustes**: el rendimiento; *Fluidez*; *Seguimiento de la cabeza*: seguir la cabeza con la cámara y
  anticiparla; *Programa*, lo que eran Versión y Otros: la versión instalada, lo de GitHub (en su
  segunda línea) y el botón de buscar o actualizar en una fila (antes tres), avisar, actualizar solo,
  idioma, barra de tareas y bandeja; Desinstalar, plegado como antes.
- **Las explicaciones, a la ayuda**: las segundas líneas de cada opción salen al dejar el ratón encima,
  y cada opción ocupa una fila. Con el panel a 820×400 la página principal corre 102 puntos (258 antes)
  y Ajustes 638 (844 antes), con la fila de la cámara de más.
- La memoria de la tarjeta gráfica, «3.7 / 24 GB» (decía «de» en todos los idiomas).
- Traducciones: «PROGRAMA» y «En GitHub: {}» nuevas; fuera «VERSIÓN», «Instalada», «OTROS», «Buscar
  actualizaciones» y «El tipo de vídeo se detecta solo».
- **Las imágenes del README, rehechas** (eran de la 1.10: la pastilla de Profundidad y Salida, la
  tarjeta de la pregunta y el panel con cinco pestañas). `tools/capturas-readme.py` pinta ahora la
  pastilla en Auto con su menú abierto; la pestaña Ajustes entera (el panel a 820×1070), con GitHub una
  versión por delante para que la fila de la versión y la tarjeta digan lo mismo; después de la primera
  vez, `modelo` apunta a un archivo vacío del HOME aparte para que no salga el aviso de la red; los
  atajos con su cabecera a la vista. Fuera `language.png`, que el README ya no usaba.

## El arranque del 3D, en paralelo (v1.13)

- Rodolfo: «acelera el arranque». MEDIDO en su panel con la línea `[arranque]` (cinco arranques, ms de
  cada paso, uno detrás de otro): hasta el visor 52–57 · panel 80–98 · lente 215 · scaler 167–186 ·
  ventana 3–4 · Vulkan 157–210 · red 530–809 · cámara 71–95 · pedir la captura 5–6 · **esperar el primer
  cuadro de la captura 769–806** · primer mapa ~34 · total **2163–2416**. Casi nada depende de lo
  anterior. (Cargar la red en RAM no ayuda: el archivo ya vive en la caché del sistema, leerlo son 5 ms;
  lo que cuesta es arrancarla en la tarjeta.)
- **La captura se pide al principio** (`src/vkview.cpp`), recién puesto el panel, y un hilo la bombea
  (`WindowCapture::pump`, con su propia conexión con KWin; PipeWire ya tiene su hilo) mientras se crean
  la ventana, Vulkan, la red y la cámara; el hilo se para y se junta antes del bucle, que la bombea desde
  ahí, y se para solo si el visor sale antes por un error.
- **La lente y el scaler en un hilo del rig** (`src/session.*`): `bringUp` pone el panel y deja los dos en
  su hilo; el visor los espera (`Rig::esperar`) antes del primer cuadro en 3D, y `shutDown` y el
  destructor también.
- La línea `[arranque]` pasa a dar el MOMENTO de cada paso (ms desde que arranca el proceso), porque con
  pasos en paralelo las restas ya no dicen nada; las marcas, con cerrojo (la lente y el scaler marcan
  desde su hilo).
- MEDIDO en el visor aparte (sin lente, scaler ni captura, así que ahí solo se ve el resto): el 3D sale a
  los 1018 ms (antes 1210), y los hilos se cierran bien también saliendo enseguida tras un volcado. En su
  panel, la captura (~0,8 s) y la lente con el scaler (~0,4 s) deberían quedar debajo de Vulkan y la red
  (~0,7–1 s): de ~2,2 s a algo más de 1 s. **No probado en el panel** (la captura de KWin no existe en el
  compositor de prueba).

## La pastilla: botón de encendido y Neural | Rendimiento en marcha (v1.13)

- Rodolfo: «en el hub, en vez de on y off, mueve el botón pill de Neural y performance que está en las
  settings, quiero que esté ahí y a la izquierda haya un botón redondo de apagado, quiero que se pueda
  cambiar ahí»; y de la pastilla de Profundidad y Salida: «ya no me sirve, ya forma parte del otro»,
  «el tercero ya no».
- **Pastilla** (`src/vkui.cpp`): a la izquierda un botón redondo de encendido (el símbolo de siempre,
  máscara nueva `icoEncender_`; encendido del color de la pastilla, apagado gris; el mismo golpe Hit3D
  que el interruptor ON/OFF que reemplaza), y a su lado el interruptor **Neural | Rendimiento** del panel
  (azul con el chip, ámbar con el rayo, del mismo ancho en los dos extremos, con su ayuda al pasar el
  ratón; golpe nuevo HitModoRed). La pastilla de Profundidad y Salida se va: fuera del modo de legado la
  pastilla es siempre esta; sin conversor (vídeo dividido tal cual, o sin red) el botón del relieve
  dice Manual y su menú va directo a Profundidad y Salida, sin la fila Auto | Manual.
- **En marcha** (`src/vkview.cpp`, `src/depth.*`): el interruptor pide el cambio y el bucle lo atiende
  con el hilo de la red parado, como las teclas 1 y 2: pone o quita `ODYSSEYVK_SIN_RED` y reabre el
  conversor con la misma ruta (`DepthNet::cambiar(..., forzar)`, que antes no reabría la misma ruta);
  apunta `exp_sin_red` en serve.conf para que el panel y la próxima sesión digan lo mismo, y si la red no
  abre vuelve a como estaba. La frecuencia variable no cambia hasta la próxima sesión.
- MEDIDO en el visor aparte con golpes simulados (`ODYSSEY_GOLPE=…modo…`, ficha nueva): a Rendimiento en
  18 ms y de vuelta a Neural en 204 ms, con el Auto recalculando al momento y `exp_sin_red` apuntado.
  Dibujado con `ctl hud`: la pastilla de la red, la de rendimiento, la sin conversor y los dos menús.
- **Y fuera de la cabecera del panel** («también quita ya esta, la vieja pastilla»): el interruptor
  Neural | Rendimiento solo está en la pastilla; la cabecera queda con el pin, el logo y el nombre,
  actualizar y cerrar.
- No probado en el panel.

## Fondo negro translúcido y el interruptor «Neural» (rama `experimental`)

- Rodolfo: «cambia el fondo gris de todos los hud por un negro ligeramente transparente» y «cambia en
  el pill la palabra AI por Neural para que se mantenga del mismo tamaño».
- **Fondo**: la pastilla, el panel, la tarjeta del borde y la etiqueta bajo la pastilla, negro al 82 %
  (antes gris 0,115–0,185 casi opaco); los menús (el del rayo y el de Auto/Manual) al 88 %, y las
  ayudas y el menú del idioma al 90 %, para que el texto se lea sobre el vídeo. Se quedan los filos
  claros y las cajas de opciones del panel; las casillas vacías, casi negras.
- **Interruptor Neural | Rendimiento** (antes AI | Rendimiento): mide lo mismo en los dos extremos (el
  ancho del nombre más largo) y el nombre puesto va centrado. «Neural» se traduce (Neuronal, 神经网络,
  ニューラル, Нейро).
- Visto con `odysseyvk ctl hud` sobre un cuadro de vídeo: el panel, la pastilla y el menú de Auto dejan
  ver la imagen detrás; el interruptor no cambia de ancho al pasar de un extremo al otro.

## Los cantos con el «alcance del relieve» grande (rama `experimental`)

- Rodolfo, con el alcance automático (x2,88 a 65 cm): «volvió los bordes un desastre lleno de
  curvas», «las líneas rectas de ese fantasma se curvearon», «hay líneas rectas que veo como si
  bailaran», «hay separaciones que si bien son grandes se notan porque de forma agresiva se pasan al
  otro extremo»; fotos del panel con las clavijas de un violín, una cabeza, un atril y una cara, de
  un vídeo suyo de YouTube (Sierra Hull en Paste, 60 s bajados para medir). Tres causas, MEDIDAS con
  la vista de cada ojo antes de tejer (sondas nuevas **16** y **17**) y con mapas de cuadros seguidos:
  1. **El afinado cortaba rampas anchas.** El canto se cambia por un escalón cuando el mapa salta más
     de 2–3 px de desplazamiento entre dos puntos; con x2,88 eso lo pasan también rampas de 8–12
     puntos (el canto desenfocado de un portátil), donde la ventana de 4 × 4 no ve el llano de ningún
     lado y el corte salía a dientes (una fila: 62 62 106 111 73 132 132 89 160). Ahora se mide el
     ancho de la rampa en 8 puntos (todo lo que cambia / el salto mayor) y solo se corta hasta 3
     puntos de ancho (nada desde 4). En el cuadro del portátil, a x2,88: escalones de 8 niveles o
     más 6022 → 2531, exceso de sube-y-baja 62,3 → 13,9 (x1: 6,0 → 6,6, con 1117 → 1105 escalones: los
     cortes de verdad siguen). Paisaje y subtítulos, igual o mejor.
  2. **El relleno de lo destapado se estiraba.** El hueco que un objeto deja al correrse es 2,88
     veces más ancho, y se rellenaba repitiendo por filas el píxel de fondo pegado al corte, que en un
     canto desenfocado aún lleva hasta 3 partes de 10 del objeto: una franja gris rayada de ~20 px y a
     dientes alrededor de cada clavija (con x1, ~8). Ahora, con alcance T > 1, el hueco se parte: su
     última parte (lo que mediría con x1) se rellena como siempre y el resto con fondo limpio, leído
     en el primero de los 6 píxeles siguientes que ya no tiene nada del objeto. `buscaVista` manda el
     ancho del hueco junto con la parte recorrida (parte + 4 × ancho). Y el corte del afinado baja de
     0,3 a 0,3 / T de objeto: el objeto se lleva casi toda su orilla, que si no quedaba al otro lado
     del relleno como una raya fina que repetía el contorno. Con x1 nada de esto cambia.
  3. **El tope de arrastre dejaba pasar los titubeos de la red.** Su fin es que lo que se mueve no
     deje estela; pero también tomaba de golpe los saltos de la red donde la imagen está quieta (la
     rejilla de un micrófono, la orilla de una cabeza). Puntos de imagen quieta (cambio < 0,02 en 5 × 5)
     cuya profundidad salta más de 0,12 de un mapa al siguiente, en 2 × 90 cuadros seguidos: sin tope
     0,181 % / 0,045 %, con el tope 0,326 % / 0,127 %, ahora 0,253 % / 0,081 %. El tope solo actúa donde
     la imagen cambió respecto a la estimación anterior (entre 0,02 y 0,05, poco a poco).
- Coste en la tarjeta (4090, 3840 × 2160, afinando en cada cuadro): 3,1–5,4 ms por cuadro con x1 y con
  x2,88, sin diferencia medible entre los dos.
- **«No inició»** (Rodolfo, tras instalar lo de arriba): el visor se quedaba callado al arrancar y la
  sesión se cerraba a los 11 s. Era el compilador de sombreadores de NVIDIA: `profFuente`, que la
  búsqueda de cada ojo llama unas treinta veces, tenía de reserva una llamada a `afinar` (solo si
  falta el pase de afinado), y el compilador la metía entera en cada sitio; con el ancho de rampa
  nuevo, arrancar sin caché de sombreadores pasó a tardar **18,7 s** (antes de hoy, 8,0). La reserva
  ahora lee el mapa bilineal: **4,1 s**. Con el pase de afinado en uso (lo normal) la imagen no
  cambia: dos volcados con el mismo binario difieren más entre sí (el mapa varía de una ejecución a
  otra) que uno de antes y uno de ahora.
- **El alcance automático, a la mitad.** Rodolfo, después de todo lo de arriba: «alcance de relieve is
  the issue, you might be too aggressive with it». Todos los defectos crecen con el alcance (cada
  error del mapa sale T veces más grande, el fantasma de la lente queda T veces más lejos, los huecos
  son T veces más anchos), y el grado de paralaje entre los ojos es lo que la vista aguanta, no lo que
  esta lente y una red de una sola imagen dan limpio. Ahora el tope de fuera queda en **medio grado**:
  x1,44 a 65 cm (+18,2 / −14,3 px por ojo), x1,1 a 50 cm, x1,77 a 80 cm. MEDIDO en el visor aparte en
  Auto: x1,44 y una Salida equivalente de 66–76 (con el grado llegaba a 225). Los textos de Auto dicen
  «medio grado de paralaje».
- **Suavizado entre las vistas** (Rodolfo: «recuerda que debe haber suavizado entre los fantasmas»;
  eligió «la mezcla de la lente»). La trama reparte cada subpíxel entre los dos ojos con un filtro de
  pendiente 10 (el de Samsung): el paso de una vista a la otra ocupa 1/10 del ciclo, casi un escalón.
  Con el rastreo un poco atrasado al mover la cabeza, la zona donde un ojo ya ve la vista del otro
  tiene un borde nítido que recorre la pantalla en curva, y ahí la imagen «se pasa de golpe al otro
  extremo». Ahora la pendiente de fábrica es **4** (un 25 % del ciclo mezclado). MEDIDO con la sonda
  del filtro: subpíxeles que mezclan las dos vistas 9,7 % → 24,3 %. CALCULADO con un modelo de la
  lente (franja vista ±0,02..0,10 ciclos, subpíxeles rojo y azul a ±0,04): el paso de un 5 a un 50 %
  de fuga se ensancha un 12–38 % y con la cabeza quieta sigue sin fuga; con 2 se ensancharía un
  46–115 % pero ya entra algo del otro ojo con la cabeza quieta si la lente reparte ancho. Deslizador
  **«Suavizar entre las vistas»** en la pestaña principal (PANTALLA), junto a «Reducir el fantasma»: de
  10 % (Samsung) a 100 %, se ve al momento mientras se arrastra; escribe `filtro` en serve.conf.
  - **Y de fábrica vuelve a 10.** Con el 25 % Rodolfo vio las cuerdas de una guitarra dobles
    («duplica cuerdas»): la mezcla deja pasar algo del otro ojo también con la cabeza quieta, que el
    modelo de la lente no daba. Probando en marcha con el deslizador (su registro: 10, 45, 90, 100,
    60 %… y vuelta) se quedó con el **10 %**, y con eso «sí bajó». Se queda el deslizador.
- **Subtítulos de una pieza.** Rodolfo, con una captura: unos subtítulos amarillos con contorno «no
  solo salen con fantasma, los tuerce». Reproducido con un subtítulo así (DejaVu Sans Condensed
  Bold, 112 px, contorno negro de 7) sobre dos cuadros reales, en Auto, midiendo la separación entre
  ojos de las LETRAS (máscara del amarillo en la vista de cada ojo, ventanas de 120 px): sobre el
  cuadro de música, ±2 px; sobre el del portátil, 0 en el centro y **−4, −12 y −16 px** en la punta
  que pasa por delante del portátil. El detector de rótulos decide celda por celda, y con letras
  gruesas y contorno muchas celdas se descartan por «densidad» (letra y contorno llenan la celda de
  bordes, como una textura) o por no tener fondo liso al lado; esas letras quedaban con medio apoyo,
  entre la pantalla y la profundidad del portátil. Ahora, en la banda de abajo y en la de arriba,
  cada renglón largo (una racha de 10 celdas o más, perdonando huecos de 4; los renglones que se
  solapan van juntos aunque haya hasta dos filas sin racha) se apoya entero: el rectángulo de su
  primera a su última celda con una celda de margen. Uno de una sola fila tiene que medir 18 celdas
  o más (en cuadros sin texto salían sueltos de 12). MEDIDO: letras a **0 px** en el portátil y ≤ 1 px
  en música; el cuadro de subtítulos de verdad apoya su renglón entero; en cuadros sin texto el
  apoyo queda como estaba.
- Sin probar en el panel.

## La cabecera del panel en dos filas, con el interruptor AI | Rendimiento (rama `experimental`)

- Rodolfo: «pasa el tope de arrastre y la frecuencia variable ya a ajustes; junto al logo de Odyssey pon
  AI y performance en una pill, que la pill cambie de logo y color con el extremo que tenga el clic»,
  «pasa Juegos, Ajustes y Experimental a una segunda fila y centra las pestañas».
- **Primera fila**: el pin, el logo y el nombre (la página principal), el interruptor **AI |
  Rendimiento**, la flecha de actualizar y el aspa. El interruptor es el «Con la red / Sin red» de antes
  (`exp_sin_red`): azul con la bola y el chip a la izquierda en AI, ámbar con el rayo a la derecha en
  Rendimiento; como el ON/OFF de la pastilla, un clic en cualquier parte lo pasa al otro extremo, y al
  pasar el ratón explica el modo puesto. Mide lo que el nombre puesto.
- **Segunda fila**, centrada: Juegos, Ajustes y Experimental. El cuerpo empieza más abajo (106 en vez
  de 68); se arrastra por cualquier sitio de las dos filas que no sea un botón.
- **Ajustes → Fluidez**: la frecuencia variable durante el 3D (solo con la red: en Rendimiento el 3D deja
  la del escritorio) y el tope de arrastre, que salen de Experimental. Experimental pierde «Modo
  rendimiento» y «Suavizado en el tiempo».

## El tipo de vídeo se detecta solo (rama `experimental`)

- Rodolfo: «¿te puedo entrenar para que identifiques un vídeo normal de uno dividido? que tomes los
  primeros segundos para cambiar el modo y ya luego dejes de inspeccionar», «en base a eso determina
  siempre el modo», «it should override, will always be auto now». Pasó tres vídeos 3D lado a lado
  de YouTube para medir (20 s de cada uno; los tres normales eran los de antes).
- **Cómo**: en un vídeo dividido las dos mitades son casi la misma imagen corrida unos puntos (el
  paralaje). Sobre el cuadro chico que ya se lee para la red, reducido a 259 × 259, se mide el
  parecido de las mitades con la derecha corrida hasta 6 puntos: el de la imagen y el del detalle (la
  imagen menos su versión borrosa, [1 4 6 4 1]/16 dos veces). Dividido si imagen > 0,65 **y**
  detalle > 0,12 (mediana de 3 s con imagen; como mucho 8 s). Arriba y abajo, igual con las mitades de
  arriba y de abajo.
- MEDIDO (ventanas de 3 s, 5 muestras por segundo):

  | Vídeo | Imagen | Detalle | Dividido |
  |---|---|---|---|
  | 3D 1 (IMAX, anémonas; parte con bandas) | 0,94–0,99 | 0,50–0,83 | 18/18 |
  | 3D 2 (submarino, detalle suave) | 0,90–0,95 | 0,27–0,42 | 18/18 |
  | 3D 3 (selva) | 0,77–0,92 | 0,62–0,70 | 18/18 |
  | Hablando | −0,01–0,25 | 0,02–0,03 | 0/18 |
  | Paisaje | 0,00–0,86 | 0,01–0,02 | 0/18 |
  | Película subtitulada | 0,11–0,50 | 0,01–0,21 | 0/18 |

  Con la imagen sola, el paisaje (cielo arriba, suelo abajo en las dos mitades) llegaba a 0,86 como un
  3D; con el detalle solo, la película oscura llegaba a 0,30 y el 3D submarino bajaba a 0,21 (a 518 de
  lado). Las dos juntas los separan.
- **Siempre automático**: el servicio ya no pone la tarjeta ni usa el modo guardado por aplicación;
  lanza el visor como vídeo normal con `--detectar` y el visor, si sale dividido, se cambia como con
  el botón de la pastilla (suavizado o no según los ajustes). Las aplicaciones marcadas como 2D plano y
  los juegos siguen su camino. En el panel, «Al ver un vídeo a pantalla completa» pasa a ser una nota.
- En el visor aparte: un cuadro 3D → «[tipo] video dividido lado a lado en 3.2 s … imagen 0.87 detalle
  0.31» y el cambio a dividido suavizado; el cuadro de paisaje → «video normal en 3.0 s … imagen 0.79
  detalle 0.01». Arriba y abajo no tiene todavía ejemplos medidos.

## El panel se redimensiona, recuerda su sitio y toma el color de KDE (rama `experimental`)

- Rodolfo: «añade redimensionar la ventana de settings, que recuerde la posición, también que el fondo
  sea gris y que tome el color de resaltado de KDE».
- **Redimensionar**: la esquina de abajo a la derecha (seis puntitos en escalera; al pasar por encima,
  el cursor de redimensionar del tema) se arrastra y el panel crece hacia la derecha y hacia abajo
  (la capa no se mueve; mínimo 560 × 400, máximo la pantalla). Se repinta una vez por cuadro del
  compositor, como al moverlo.
- **Recordar**: al soltar después de moverlo o de cambiarle el tamaño, sus márgenes y su tamaño van a
  `~/.local/state/odysseyvk/panel-ventana`; al abrirse vuelve ahí, sin salirse de la pantalla. En un
  KWin aparte, con «300 150 1000 700» guardado, abrió en x 300, y 150, de 1008 × 708 (el borde suave).
- **Colores**: el fondo pasa a gris (antes casi negro y translúcido) y el acento (pestaña puesta,
  opciones, interruptores, deslizadores, botones) es el color de resaltado de KDE, de
  `~/.config/kdeglobals` (`[General] AccentColor` o `[Colors:Selection] BackgroundNormal`; el de
  Rodolfo, el azul de Breeze 61,174,233). Sobre un acento claro, la letra va oscura.
- Para la prueba de la interfaz: `ODYSSEYVK_HUD_ACENTO=r,g,b` y `ODYSSEYVK_HUD_TAMANO=ANCHOxALTO`.

## El logo abre la página principal; Programa pasa a llamarse Ajustes (rama `experimental`)

- Rodolfo: «que visor 3D y tarjetas gráficas vayan encima de versión en Programa, y cámbiale el nombre a
  Ajustes, la primer pestaña que parta del mismo logo del programa».
- **El logo**: el de la aplicación (`assets/hicolor/512x512/apps/odysseyvk.png`), con sus puntos azules
  sacados como máscara del atlas (azul menos rojo, a 96×96, en `src/logo_mascara.inc`) y la baldosa
  blanca pintada aparte, junto a «OdysseyVK»: es la primera pestaña, la página principal (vídeo, pantalla,
  atajos, aplicaciones que no preguntan). Puesta, va en una píldora del acento como las demás.
- **Programa → Ajustes**, con el visor 3D, la red y las tarjetas gráficas arriba, encima de la versión.
- Las pestañas conservan su número (la 0 y la 2 van sin píldora).

## El alcance del relieve, automático en Auto (rama `experimental`)

- Rodolfo: «que el alcance del relieve también lo definas automáticamente».
- **Criterio**: la zona cómoda de la visión estéreo, ±1° de paralaje respecto a la pantalla (la «regla
  del grado»). Con la distancia de los ojos que da el rastreo (sin cámara, la pose nominal de 65,1 cm) y
  el paso de este panel (598 mm / 3840 px = 0,156 mm), el tope de fuera por ojo se pone en medio grado:
  alcance = d · tan 1° / (2 · 12,67 px · 0,156 mm). A 65 cm, x2,88 (+36,5 / −28,7 px por ojo); a 50 cm,
  x2,2; a 80 cm, x3,5 (más cerca, menos). Se suaviza en ~1 s; entre x1 y x4.
- Con él, la Salida automática llena esos topes. MEDIDO con los tres vídeos de YouTube a 65 cm:
  hablando 79,1 px de relieve entre los ojos (1,5 % pegado), paisaje 107,0 px (1,4 %), película 80,9 px
  (1,7 %).
- **El menú de Auto** enseña «Plano · Relieve N px · Alcance xN» en vez de una Salida equivalente: la
  automática puede pedir más de lo que da el deslizador (en la película, el equivalente de Salida 225).
  **Al pasar a Manual** se quedan también el alcance (en `exp_topes_red`) y la Salida hasta su tope de
  50. En el visor aparte: «alcance x2.88 (ojos a 65 cm: +36.5 / -28.7 px por ojo)» y, a Manual,
  `exp_topes_red = 2,88`. El deslizador de Experimental dice «en Auto se calcula solo».

## Rendimiento dentro de Programa, Desinstalar plegado y sin el globo del idioma (rama `experimental`)

- Rodolfo: «mete la info de rendimiento en la casilla de Programa, que desinstalar esté debajo de un
  dropdown contextual con una flechita, también quita el pill del idioma, basta con tenerlo en Otros».
- **Rendimiento** ya no es una pestaña: el visor 3D, la red y las tarjetas gráficas van en Programa,
  entre la versión y «Otros», y se refrescan cada segundo mientras Programa está a la vista. La
  pestaña conserva su número (nombre vacío), así que las demás no cambian.
- **Desinstalar**: sección plegable con su flechita, de fábrica plegada (se recuerda como las otras
  en `panel-secciones`).
- **El globo del idioma** sale de la cabecera; el idioma se elige en Programa → Otros → Idioma. Las
  pestañas ganan ese sitio.

## Las bandas negras de las películas, en el plano de la pantalla (rama `experimental`)

- Rodolfo: «Sí, arréglalas», tras medir con su YouTube que la red les daba 0,64–0,78 de cercanía con
  la imagen en 0,20–0,35: flotaban por delante de la película.
- **Detección** en la guía de luminancia, al tamaño del mapa: filas (o columnas) pegadas al borde con
  un 85 % de puntos por debajo de 0,03. MEDIDO en la película: las bandas son negro puro (mediana y
  percentil 80 = 0,000); los bordes de una escena oscura de interior tienen mediana 0,04–0,13 (con el
  primer criterio, 90 % por debajo de 0,09, salían bandas laterales falsas del 8–15 %). Los subtítulos,
  dentro de la banda de abajo, no la rompen. Un cuadro casi negro (fundido) o una banda que llega al
  40 % no cuentan: se queda lo que había (antes un fundido las ponía a cero 1,5 s). Bajan al momento,
  suben si aguantan 15 mapas, y el primer cuadro las toma de una.
- **Arreglo**: las bandas se quedan con el plano que usa la trama (el visor se lo dice a la red cada
  cuadro: el automático o la Profundidad), o sea a cero paralaje, como un marco; y no cuentan para el
  plano automático ni para los cuantiles de la Salida automática. En la película: «bandas negras:
  arriba 12 % · abajo 12 %», y su profundidad pasa de 0,64–0,78 a la del plano (0,17–0,28). En los
  otros dos vídeos no aparece ninguna banda.
- **La Salida automática baja rápido y sube despacio** (0,2 s y 2 s, en vez de 0,6 s para los dos lados):
  sin las bandas en los cuantiles sigue de verdad a la imagen, que en las escenas oscuras cambia mucho.
  Medida final con los tres vídeos:

  | Vídeo | Alcance | Auto: pegado (peor 5 %) | Auto: relieve | Fija 50 `relieve` x2: pegado / relieve |
  |---|---|---|---|---|
  | Hablando | x1 | 1,5 % (4,2) | 27,5 px | 34,6 % / 39,1 px |
  | Paisaje | x1 | 1,4 % (2,4) | 37,2 px | 49,7 % / 45,3 px |
  | Película | x1 | 1,7 % (6,7) | 28,1 px | 15,0 % / 38,7 px |
  | Hablando | x2,75 | 1,5 % | 75,5 px | 3,5 % / 72,6 px |
  | Paisaje | x2,75 | 1,4 % | 102,2 px | 0,0 % / 73,5 px |
  | Película | x2,75 | 1,7 % | 77,2 px | 0,2 % / 51,6 px |

  En la película, quitar las bandas del tope baja lo pegado con la Salida fija del 41,8 % al 15,0 %.

## El hub del modo normal en azul, con Auto | Manual y la Salida automática (rama `experimental`)

- Rodolfo: «no me gustó el con fondo, quiero solo la parte blanca en un tono más azul claro; quiero que
  cambies ese hub a algo así [el de rendimiento], todo en ese azul, botón de encendido igual, en el de
  a un lado ese logo, y que diga Auto o Manual; Auto que sea saliencia, y Manual la profundidad con lo
  otro». Y: «estudia cómo autocalcular la salida junto a la profundidad para dar el mayor relieve
  posible, mídelos».
- **La Salida automática, MEDIDA** con `odysseyvk mapa` (red fp16) sobre 5 cuadros, cada uno como 8
  copias con ruido de vídeo y como un barrido de 12 recortes. Con el plano de la saliencia en el sujeto,
  la mayor ganancia que deja como mucho un 2 % del mapa en cada tope:
  G = mín(12,67·alcance / (q98 − plano), 9,98·alcance / (plano − q2)).

  | Alcance | Salida | Pegado al tope | Relieve entre los ojos (p5–p95) | Varía (ruido / barrido) |
  |---|---|---|---|---|
  | x1 | automática 2/98 | 0,9 % | 27,8 px | 1,2 % / 2,6 % |
  | x1 | automática 5/95 | 3,9 % | 29,7 px | 1,2 % / 3,0 % |
  | x1 | fija 50, `relieve` x1 | 39,7 % | 28,5 px | — |
  | x1 | fija 50, `relieve` x2 (la de Rodolfo) | 61,3 % | 32,8 px | — |
  | x2,75 | automática 2/98 | 0,9 % | 76,5 px | 1,2 % / 2,6 % |
  | x2,75 | fija 50, `relieve` x2 | 30,7 % | 66,0 px | — |

  Cada escena pide otra ganancia (la nave, ~10 px por unidad; el globo, ~23): una fija aplasta las
  de mucho fondo. Con los topes de fábrica la automática da el mismo relieve sin aplastar; con el
  alcance x2,75 da más relieve y sin aplastar. Se queda la 2/98, suavizada en ~0,6 s y con salto en los
  cortes; tope: lo que daría la Salida 50 con `relieve = 2,00` y el mismo alcance (una escena casi plana
  no infla el ruido de la red). La red calcula los cuantiles 2 y 98 % de cada mapa.
- **El hub del modo normal** (conversor con la red): como el de rendimiento y en azul claro. El
  interruptor, un botón con el chip «AI» (solo el trazo, en azul claro, sin fondo), «Auto» o «Manual» y
  la flechita, y el engranaje. Su menú: Auto | Manual (con su ayuda al pasar el ratón); en Manual, la
  Profundidad y la Salida; en Auto, el plano y la Salida que se calcularon. Debajo, el tipo de vídeo y
  «No volver a preguntar». La pastilla del dividido de siempre también pasa a azul; la de rendimiento
  sigue ámbar y la tarjeta de la orilla, morada.
- **Auto** guarda `profundidad_<modo> = auto` (y enciende la saliencia si estaba apagada);
  Ctrl+Shift+3/4/5/6 no hacen nada y el renglón de teclas se queda en «1 el 3D · F1 oculta · Esc sale».
  **Manual** deja la Profundidad donde estaba el sujeto y la Salida que se calculaba, y las guarda. En el
  visor aparte: «Auto … profundidad_conv = auto», «auto: plano 0.69 · salida 14.5 px por unidad (Salida
  28)», y al volver a Manual `profundidad_conv = 68`, `salida_conv = 29`.
- **Arreglo de paso (viene de main)**: arrastrar la Salida con el ratón daba 50 a mitad del recorrido
  (se tomaba de 0 a 100 en vez de −50 a +50): la bola saltaba al final y lo negativo no se alcanzaba.
- **Con vídeo de verdad** (Rodolfo: «también puedes medir con mi YouTube»): 20 s de tres vídeos
  suyos, 10 cuadros por segundo (el ritmo de mapas del visor con un vídeo de 30), por la red fp16 con la
  saliencia en secuencia, y la Salida automática suavizada como en el visor (cortes por cambio de imagen):

  | Vídeo | Alcance | Auto: pegado | Auto: relieve | Fija 50 `relieve` x2: pegado / relieve | Cambio por cuadro (p95) |
  |---|---|---|---|---|---|
  | Hablando, cortes rápidos (8 cortes) | x1 | 3,5 % | 28,6 px | 34,6 % / 39,1 px | 3,1 % (sin suavizar 7,7) |
  | Paisaje y animales | x1 | 2,2 % | 38,5 px | 49,7 % / 45,3 px | 1,9 % |
  | Película subtitulada (34 % de bandas) | x1 | 2,6 % | 31,0 px | 41,8 % / 42,5 px | 1,5 % |
  | Hablando | x2,75 | 3,5 % | 78,6 px | 3,5 % / 72,6 px | 3,1 % |
  | Paisaje | x2,75 | 2,2 % | 105,9 px | 0,0 % / 73,5 px | 1,9 % |
  | Película | x2,75 | 2,6 % | 85,2 px | 1,6 % / 80,6 px | 1,5 % |

  Con los topes de fábrica, la automática aplasta 10–20 veces menos que la fija de Rodolfo con algo
  menos de recorrido; no bombea (0,4–1 % de cambio medio por cuadro). Con alcance x2,75 llena los topes:
  en el paisaje llega a ~106 px de relieve, con cada punto dentro de sus topes; el alcance es lo que
  pone el límite de comodidad.
- **Las bandas negras de las películas salen hacia delante** (medido, no solo en Auto): la red les da
  0,64–0,78 de cercanía cuando la imagen está en 0,20–0,35, así que flotan por delante de la película
  (con la Salida 50 y `relieve = 2,00`, pegadas al tope de fuera) y se comen el margen de la Salida
  automática. Arreglado en la entrega siguiente.
- Rodolfo, después: «que el manual sea una herramientita». En Manual el botón lleva una llave (el mango
  en diagonal y la cabeza abierta, en el mismo azul claro) en vez del chip; en Auto, el chip.
- Sustituye al chip cuadrado del Foco automático de la entrega anterior.

## Foco automático: el chip «AI» de la pastilla (rama `experimental`)

- Rodolfo: «quiero que hagas un toggle de saliencia, describiendo qué es al poner el mouse encima, y
  que activarlo la ponga en auto y haga lo que tenga que hacer; si amerita que mientras la actives se
  esconda profundidad hazlo, y si hay un mejor nombre tómalo; quiero que el logo del modo normal,
  contrario al de rendimiento, sea algo así, minimalista» (un chip con «AI»).
- **Nombre**: «Foco automático» (en: Auto focus). Es lo que hace: busca el sujeto de cada escena con
  la saliencia y lo deja en el plano de la pantalla; la saliencia es el cómo.
- **El chip**: en la pastilla del modo normal (conversor con la red; vídeo normal y dividido
  suavizado), junto al interruptor. Dibujado como máscara (contorno redondeado, tres patitas por lado y
  «AI», de trazos redondos). Gris quitado, morado puesto; al pasar el ratón, debajo de la pastilla, su
  ayuda dice qué hace y en qué estado está.
- **Puesto**: el plano es el automático con la saliencia (si estaba `saliencia = no`, se pone en
  marcha y se guarda `si`) y se guarda `profundidad_<modo> = auto`. La Profundidad se esconde: tocarla
  fijaba un número y quitaba el automático; Ctrl+Shift+3/4 no hacen nada (lo dice el registro) y el
  renglón de las teclas pierde la profundidad. **Quitado**: la Profundidad vuelve con el valor donde
  estaba el sujeto en ese momento (la imagen no salta) y se guarda ese número.
- La saliencia se puede encender en marcha (`DepthNet::setSaliencia`); antes se leía una vez.
- En el visor aparte (con `profundidad_conv = 100` y `saliencia = no`): picar el chip deja «[foco] Foco
  automatico puesto … profundidad_conv = auto» y `saliencia = si`; picarlo otra vez, «quitado … la
  Profundidad vuelve donde estaba el sujeto · profundidad_conv = 60». `ODYSSEYVK_HUD_FOCO=0|1` lo pinta
  en la prueba de la interfaz.

## El pin del panel: fijarlo encima del juego (rama `experimental`)

- Rodolfo: «pon un pequeño pin a la izquierda del nombre, para always on top, que cuando lo pique el
  juego no quite el overlay».
- **El pin**, a la izquierda de «OdysseyVK» en la cabecera del panel: gris suelto, morado fijado, con
  su rótulo al pasar el ratón. Se recuerda en `~/.local/state/odysseyvk/panel-fijado`.
- **Fijado**: sin velo (los clics fuera llegan al juego) y con el teclado «a pedido» (layer-shell v4:
  la capa lo toma solo cuando se la pica, y un clic en el juego se lo lleva); el panel se queda encima.
  El visor, con el panel fijado, tampoco lo cierra al picar el juego con la pastilla puesta. **Suelto**:
  como antes (velo y teclado exclusivo; un clic fuera lo cierra). Al soltarlo con un juego debajo, el
  velo vuelve y la capa del panel se rehace encima, en el mismo sitio.
- Se enlaza la v4 de layer-shell si el compositor la ofrece: el «a pedido» es el mismo pedido de la
  v1 con otro número. Sin ella, fijado se queda sin teclado (y exclusivo mientras se espera un atajo).
- **Arreglos de paso**: (1) el globo del idioma, su lista y la flecha de actualizar de la cabecera no
  le llegaban nunca al bucle del panel (la capa solo pasaba el aspa, las pestañas y las filas; venía de
  main); (2) con la conexión rota, la capa se quedaba dando vueltas en `prepare_read` y el panel no se
  cerraba ni con SIGTERM; (3) rehacer la capa con el lienzo aún puesto es un error de protocolo («a
  buffer has been attached to a layer surface prior to the first configure»): se suelta antes.
- MEDIDO en un KWin aparte con Konsole de juego y `ODYSSEYVK_PIN_TRAS` (pica el pin solo a los N s):
  suelto→fijado, el velo se va y el panel sigue; fijado→suelto, el velo vuelve debajo del panel; al
  cerrar, el «juego» vuelve a ser la activa en los dos casos.

## Más profundidad con la red: el alcance del relieve (rama `experimental`)

- Rodolfo: «¿podemos intentar meter aún más profundidad sobre el modelo?». Tenía `relieve = 2,00`,
  Salida 50 y `profundidad_conv = 100` (el plano de la pantalla al frente: todo se hunde).
- **Lo que frenaba**: los topes por ojo (+12,67 / −9,98 px, los de Samsung). MEDIDO con los mapas
  de la red (persona, nave, globo), lo que queda pegado al tope y el relieve entre los ojos (p5–p95):

  | Plano | Relieve | Topes | Pegado al tope | Relieve entre los ojos |
  |---|---|---|---|---|
  | 1,0 (Profundidad 100) | x2 | x1 | 85 % | 11,5 px |
  | 1,0 | x2 | x2,75 | 58 % | 46,5 px |
  | 1,0 | x2 | x4 | 32 % | 71,4 px |
  | 1,0 | x3 | x4 | 60 % | 67,2 px |
  | 0,75 | x2 | x2,75 | 35 % | 69,1 px |
  | automático | x2 | x1 | 53 % | 45,3 px |
  | automático | x2 | x2,75 | 2 % | 81,0 px |

  Con el tope en su sitio, más fuerza aplasta más (x3 da menos relieve que x2); lo que da profundidad
  es ensanchar el tope, como el «Alcance» del modo rendimiento.
- **Experimental → RED DE PROFUNDIDAD → «Alcance del relieve»** (`exp_topes_red`, x1..x4, de fábrica
  x1 → `ODYSSEYVK_TOPES_RED`): multiplica los topes por ojo con la red. Se aplica en marcha (orden
  `topes_red`) y la ayuda al pasar el ratón cuenta lo medido. En el visor aparte: «alcance del relieve
  con la red x1.50 (topes por ojo +19.0 / -15.0 px)» al arrancar y «x2.75 (+34.8 / -27.4 px)» tras
  `ctl ajuste topes_red 2,75`.

## La red de fábrica es la fp16 (rama `experimental`)

- Rodolfo, tras la medida de las redes ligeras: «Sí, ponla».
- **Qué red se usa**: `depth_anything_v2_small_fp16.onnx` si está en `~/.local/share/odysseyvk/modelos/`;
  si no, la de siempre (`depth_anything_v2_small.onnx`, fp32), que sigue valiendo.
- **Qué se baja**: la fp16, 49 642 442 bytes, de
  `huggingface.co/onnx-community/depth-anything-v2-small` (`onnx/model_fp16.onnx`). Los textos del
  panel y de `ctl red` dicen 50 MB y esa fuente, en los siete idiomas.
- **Con la de siempre ya puesta**: el panel ofrece «Versión ligera de la red» con su botón de 50 MB
  («La misma red a media precisión: el mismo mapa con un 35 % menos de tiempo y 260 MB menos en la
  tarjeta»), y `ctl red descargar` la baja. Al terminar, el servicio la toma para el siguiente visor sin
  reiniciarse, y el visor abierto cambia a ella en marcha.
- Probado con un HOME aparte que solo tenía la de siempre: el panel ofrecía la ligera; `ctl red
  descargar` la bajó entera (misma suma que la medida) y desde ahí `ctl red` y `odysseyvk mapa`
  usan la fp16.

## Redes de profundidad más ligeras: medidas (rama `experimental`)

- Rodolfo: «¿hay modelos de profundidad más ligeros? quiero que se prueben algunos y su consumo de GPU».
- MEDIDO con `odysseyvk mapa` en la RTX 5080 (libre; la 4090 tenía el escritorio), 61 cuadros
  seguidos, con `nvidia-smi` cada 100 ms. «Red» = la inferencia sola (`ODYSSEYVK_MEDIR`), «mapa» =
  con la limpieza en la CPU; la memoria incluye el contexto de CUDA. La diferencia es contra el mapa de
  la red actual, en tres cuadros (persona, nave, globo; `../pruebas/redes-ligeras-2026-11-26.jpg`):

  | Red | Entrada | Red ms | Mapa ms | Memoria | Uso de la tarjeta | Diferencia /255 | Correlación |
  |---|---|---|---|---|---|---|---|
  | Depth Anything V2 Small fp32 (la actual) | 518 | 7,3 | 13,2 | 1012 MiB | 29 % | 0 | 1,000 |
  | Depth Anything V2 Small fp16 | 518 | 4,7 | 10,5 | 754 MiB | 15 % | 0,5 | 1,000 |
  | Depth Anything V2 Small fp32 | 364 | 3,8 | 10,8 | 754 MiB | 12 % | 14,4 | 0,981 |
  | Depth Anything V2 Small fp16 | 364 | 2,7 | 9,8 | 626 MiB | 8 % | 14,5 | 0,980 |
  | MiDaS v2.1 small | 256 fija | 6,3 | 13,2 | 632 MiB | 22 % | 58,6 | 0,520 |

  La fp16 (`onnx-community/depth-anything-v2-small`, `onnx/model_fp16.onnx`, 49,6 MB) da el mismo
  mapa con un 35 % menos de tiempo de red y 260 MiB menos. Bajar la entrada ahorra más pero el mapa
  cambia (los bordes se ablandan). MiDaS small no compensa: en CUDA es más lenta que la fp16 y su
  mapa se parece poco (el fondo de la nave y el globo salen como manchas).
- Y las de 8 y 4 bits (Rodolfo: «¿y existe fp8?»; no hay fp8 publicada de esta red, y fp8 de verdad
  pediría TensorRT, que no está). Misma prueba, otra tanda (los tiempos salieron algo más altos que
  la primera vez; la comparación vale dentro de la tanda; `../pruebas/redes-8-y-4-bits-2026-11-26.jpg`):

  | Red | Archivo | Red ms | Mapa ms | Memoria | Diferencia /255 | Correlación |
  |---|---|---|---|---|---|---|
  | fp32 | 99 MB | 7,3–8,7 | 15,4 | 1024 MiB | 0 | 1,000 |
  | fp16 | 50 MB | 5,0–5,5 | 12,8 | 766 MiB | 0,6 | 1,000 |
  | int8 (`model_int8.onnx`) | 27 MB | 281–303 | 297 | 766 MiB | 4,9 | 0,998 |
  | q4f16 (`model_q4f16.onnx`) | 19 MB | 5,0 | 10,7 | 688 MiB | 21,6 | 0,887 |

  La int8 va unas 55 veces más lenta: su cuantización se hace en el procesador. La q4f16 tarda lo
  mismo que la fp16 y solo ahorra 78 MiB, pero su mapa cambia (en el globo el fondo sale casi plano).
  Se queda la fp16.
- Para poder probar MiDaS: una red con la entrada de tamaño fijo se usa a ese tamaño («la red pide su
  entrada fija de 256 x 256»); antes se redondeaba a parches de 14 (252) y no abría.

## El HDR del escritorio siempre, secciones plegables y Juegos fuera del panel en 3D (rama `experimental`)

- Rodolfo: «quita la opción de HDR, que se respete siempre el escritorio», «en atajos de teclado pon
  una flechita para contraer y expandir, también en aplicaciones que no preguntan», y «que la sección
  de juegos no aparezca cuando se ejecute en un vídeo o un juego».
- **HDR**: la sesión ya no lo apaga nunca; el 3D deja el que tenga el escritorio (el registro dice
  «HDR como en el escritorio»). Fuera la fila «Mantener el HDR durante el 3D», la clave `hdr_en_3d` y
  `ODYSSEYVK_HDR_3D`. Una línea `hdr_en_3d` que quede en serve.conf ya no hace nada.
- **Secciones plegables**: «ATAJOS DE TECLADO» y «APLICACIONES QUE NO PREGUNTAN» llevan una flechita
  a la derecha de la cabecera (hacia el lado plegada, hacia abajo desplegada) y se pican enteras. De
  fábrica plegadas; lo desplegado se recuerda en `~/.local/state/odysseyvk/panel-secciones`. Esperando
  un atajo nuevo, la de atajos se queda abierta.
- **La pestaña Juegos** no sale con un vídeo o un juego en 3D debajo (las demás conservan su número;
  el clic central de la bandeja abre entonces Ajustes). `ODYSSEYVK_HUD_SESION=1` la quita en la prueba
  de la interfaz.

## Los juegos y el cursor: la pastilla y el panel se lo quitan y se lo devuelven (rama `experimental`)

- Rodolfo: «quiero que el overlay me robe el cursor del juego mientras se muestre», «y claro,
  devolverlo al cerrarse» (la pastilla y el panel, las dos), «que el comportamiento sea mostrarse
  unos segundos para que se vean los shortcuts y ya quitarse», «que dar clic al juego le devuelva el
  control también».
- **Por qué no se podía**: el juego encierra el cursor (Proton/XWayland lo hace con las restricciones
  de puntero) y KWin solo se lo deja encerrado mientras el juego es la ventana activa. La pastilla no
  se alcanzaba sin soltarlo, y el panel sacaba la flecha solo por suerte (nunca ponía cursor: sobre un
  juego que esconde el suyo no se veía).
- **La pastilla en un juego**: al entrar al 3D se ve 6 s (los atajos) y se oculta sola, sin tocar el
  ratón. Puesta a mano (Ctrl+Shift+F1 o `ctl capa`), le quita el cursor al juego: el visor se queda
  con la región de entrada entera y pide ser la ventana activa por el gestor de ventanas de KWin. Al
  ocultarla, con un clic fuera de la pastilla y su menú (eso cierra también el panel), o al cerrarse el
  visor, se le devuelve: la región vuelve a la pastilla y el juego vuelve a ser la activa. Un juego es
  lo que el servicio ya trata como tal (`steam_app_*` o gamescope); con vídeos nada cambia.
- **El panel sobre un juego**: debajo del panel va un velo, una capa transparente de la pantalla
  entera que se queda con el ratón (el juego no ve ni movimiento ni clics) y pone la flecha del tema
  (`wayland-cursor`; el panel también la pone ahora). Un clic en el velo cierra el panel. Al cerrarse,
  el juego vuelve a ser la ventana activa.
- MEDIDO en un KWin aparte, sin pantalla y con su propio bus (`kwin_wayland --virtual` dentro de
  `dbus-run-session`), con Konsole haciendo de juego (`ODYSSEYVK_JUEGO=1`) y un script de KWin que
  lista el apilamiento: con el panel abierto el «juego» deja de ser la activa, el velo (1920×1080)
  queda debajo del panel; al cerrarlo vuelve a ser la activa. Con dos Konsoles y la otra activada a
  mano mientras tanto, al cerrar se activa justo la del juego: es nuestra petición, no la cadena de
  foco de KWin. En el visor aparte (gamescope, sin el gestor de KWin): la pastilla se va sola a los
  6 s; Ctrl+Shift+F1 (golpe «capa») pasa la región a «la ventana entera»; un clic dentro de la
  pastilla no la quita; un clic fuera la oculta y devuelve la región.
- Lo que no se pudo medir aquí: un juego de verdad con el cursor encerrado. El registro lo dice en
  marcha: «[raton] el visor es la ventana activa: el juego soltó el cursor» o «KWin no hizo caso», y
  en el panel «[ajustes] sobre el juego …: deja de ser la ventana activa».
- Para las pruebas: `ODYSSEYVK_JUEGO=1|0` fija si hay un juego debajo, `ODYSSEYVK_VELO=1` pone el velo
  sin juego, y `ODYSSEY_GOLPE` acepta `capa` y lleva su propio contador de cuadros (el de `[fps]`
  vuelve a cero cada segundo y los golpes de después no llegaban).

## Modo rendimiento: el menú del rayo en la pastilla, sin Profundidad ni Salida (rama `experimental`)

- Rodolfo: «¿profundidad y salida hacen falta en ese modo?», y «el menú de modo rendimiento ponlo
  como un menú desplegable junto al rayito en el hub, que no esté en las settings; también quítale
  depth y popout».
- **No hacían falta.** Sin red, la Salida y el `relieve` global solo multiplicaban la misma ganancia
  que la «Fuerza del relieve», y la Profundidad movía el plano de la pantalla lejos de donde se
  midieron los preajustes (el plano automático, el sujeto). En el serve.conf de Rodolfo había
  `profundidad_conv = 0`: el plano al fondo, todo saliendo de la pantalla y buena parte contra el
  tope. Ahora, sin red, la base es fija (`rendimiento::kGananciaBase`: Salida 50 con `relieve = 2,00`,
  con la que se midieron) y el plano es el automático. Ctrl+Shift+3/4/5/6 no hacen nada en este modo
  (lo dice el registro), y los valores guardados para el vídeo con la red no se tocan.
- **La pastilla del modo rendimiento**: interruptor, un botón ámbar con el rayo, el preajuste puesto
  («A mano» si no es ninguno) y una flechita, y el engranaje. El renglón de las teclas pierde
  Profundidad y Salida (también con atajos cambiados).
- **El menú del rayo** cuelga debajo de la pastilla: los tres preajustes y cuatro deslizadores
  (mezcla suelo ↔ imagen, fuerza del relieve, alcance del relieve, paralaje de la cabeza). Se ponen
  en marcha al moverlos y, con el ratón suelto medio segundo, se guardan en serve.conf. Al pasar el
  ratón, a su izquierda sale la ayuda de cada uno (la misma que tenía el panel). La región de entrada
  del visor crece con el menú abierto.
- **El panel**: Experimental → MODO RENDIMIENTO se queda solo con «Profundidad del vídeo normal» (Con
  la red / Sin red, que es lo que enciende el modo y vale desde el siguiente vídeo) y el VRR cuando va
  con la red. Los preajustes y los cuatro deslizadores salieron de allí.
- En el visor aparte, con clics simulados (`ODYSSEY_GOLPE=…;rayo;3392,532;…;3640,314`, que ahora
  cuenta un golpe sobre un deslizador como clic entero): «menu del rayo abierto», «a mano: … relieve
  x1.25», «preajuste intenso: mezcla 50 % · relieve x1.50 · topes x2.75 · paralaje x3.0», y serve.conf
  con las cuatro claves.

## Modo rendimiento: la pastilla con rayo, tres preajustes medidos y ayudas al pasar el ratón (rama `experimental`)

- Rodolfo: «crea un hub distinto con un rayo para modo performance, con 3 botones preset que midas
  buenos; ya en las settings pones esto que me dijiste que cambia cada uno cuando se pase el mouse en
  cada valor», y «que frecuencia variable desaparezca al presionar modo rendimiento, se sigue siempre
  lo que defina el escritorio en ese modo; el toggle vuelve al desactivarlo en el estado en el que lo
  dejó el usuario, por defecto apagado».
- **La pastilla del modo rendimiento** (vídeo normal sin red): acento ámbar en lugar del morado, un
  rayo, y en el sitio de los botones de tipo de vídeo y de la casilla, tres botones **Suave · Medio ·
  Intenso**. Se aplican en marcha (sin esperar al siguiente vídeo) y se guardan en serve.conf. Si los
  cuatro valores no son los de ningún preajuste, ninguno sale marcado.
- **Los tres preajustes** (`src/rendimiento.h`), medidos con `odysseyvk mapa` sobre la persona, la
  nave y el globo con ruido de vídeo:

  | | Mezcla | Fuerza | Alcance | Paralaje | Entre los ojos | Aplastado contra el tope |
  |---|---|---|---|---|---|---|
  | Suave | 25 % | x0,75 | x2 | x1,5 | ~32 px | 3 % |
  | Medio (de fábrica) | 35 % | x1 | x2,75 | x2 | ~43 px | 3 % |
  | Intenso | 50 % | x1,5 | x2,75 | x3 | ~52 px | ~20 % |

  Lo que salió al medir: sin red el parpadeo es 0,16–0,48 de 255 (con la red, 1,43), y la forma se
  parece poco a la de la red (correlación 0,02–0,11). Multiplicar la fuerza y los topes por lo mismo
  (el «relieve x2» de antes) dejaba el 43–57 % del cuadro aplastado contra los topes: se ve como
  recortes planos. Por eso se separan.
- **Fuerza y alcance por separado**: «Fuerza del relieve sin red» (`exp_sin_red_relieve`, x0,5..x2) es
  cuánto se separan los ojos por cada paso de profundidad; «Alcance del relieve sin red»
  (`exp_sin_red_topes`, x1..x4 → `ODYSSEYVK_SIN_RED_TOPES`) multiplica los topes por ojo (+12,67 /
  −9,98 px). Encima, «Preajuste sin red» en el panel escribe las cuatro claves de una vez y se lo dice
  al visor (orden `preajuste = 0|1|2`). En el visor aparte: «[rendimiento] preajuste intenso: mezcla
  50 % · relieve x1.50 · topes x2.75 (+34.8 / -27.4 px) · paralaje x3.0», y serve.conf queda con las
  cuatro claves.
- **Ayudas al pasar el ratón**: cada fila nueva de MODO RENDIMIENTO (y el VRR) lleva una caja con lo
  que cambia; en las filas de opciones, una por opción (cada preajuste dice sus cuatro valores y lo
  que se midió). Se parten en renglones de hasta 460 puntos (en chino y japonés, por letras).
- **El VRR en el modo rendimiento**: con «Sin red» el interruptor de la frecuencia variable desaparece
  del panel y el servicio deja el VRR como lo tenga el escritorio (`ODYSSEYVK_VRR_3D=1`). `vrr_en_3d`
  no se toca: al volver a «Con la red», el interruptor reaparece como lo dejó el usuario (de fábrica,
  apagado).
- **¿El mapa grande mejora el paralaje sin red?** No: con `exp_mapa = 1022` el mapa sin red cambia 2–3
  de 255 respecto al de 518, y cuesta cuatro veces más (8,5 → 34 ms por mapa en la CPU).

## Modo rendimiento: mezcla suelo ↔ imagen y el 3D exagerado sin red (rama `experimental`)

- Rodolfo: «¿no se puede exagerar más el paralaje solo en el modo rendimiento?», «¿y si se pueden
  mezclar ambos modos?», «ajústalo a lo que sería más perceptible de inicio».
- «Profundidad del vídeo normal»: Con la red / Sin red (`exp_sin_red = no|si`; `suelo` e `imagen`
  de antes valen como `si`). «Mezcla sin red: suelo ↔ imagen» (`exp_sin_red_mezcla` 0..1 →
  `ODYSSEYVK_SIN_RED_MEZCLA`): cercanía = (1 − m)·altura + m·pistas, con pistas = 0,50 nitidez +
  0,20 saturación + 0,30 centro. De fábrica m = 0,35: el suelo da un relieve claro y estable y las
  pistas le dan forma a lo cercano (`../pruebas/sin-red-mezclas-2026-11-26.jpg`: 0, 35 y 70 %).
- «Exagerar el relieve sin red» (`exp_sin_red_relieve` x1..x3 → `ODYSSEYVK_SIN_RED_RELIEVE`): la
  ganancia del relieve Y los topes por ojo (+12,67 / −9,98 px) multiplicados; sin agrandar los topes,
  el relieve de más se aplastaba contra ellos. «Exagerar el paralaje de la cabeza sin red»
  (`exp_sin_red_paralaje` x1..x4 → `ODYSSEYVK_SIN_RED_PARALAJE`): la ganancia del paralaje de
  movimiento, con su tope de una vista subiendo hasta dos. Solo valen con el conversor sin red.
- De fábrica, lo más perceptible: relieve x2 (topes +25,3 / −20,0 px), paralaje x2 (con
  `paralaje = 0,5` queda en 1, la geometría real), mezcla 35 %. El registro lo dice: «[conversor]
  sin red: relieve x2.0 (topes por ojo +25.3 / -20.0 px) · paralaje de la cabeza x2».
- En el visor aparte: 9,7 ms por mapa en la CPU, cuadro en la tarjeta 1,6–2,0 ms.

## Modo rendimiento: profundidad sin red y el VRR en la pestaña Experimental (rama `experimental`)

- Rodolfo: «¿hay profundidad sin modelo? quiero probar un modo performance, sin modelo y con
  variable refresh rate». No la había: el vídeo normal solo tenía la red (y `--mapa-fijo`, un mapa
  leído de un archivo para el taller).
- Experimental → MODO RENDIMIENTO:
  - «Profundidad del vídeo normal» (`exp_sin_red = no|suelo|imagen` → `ODYSSEYVK_SIN_RED`): sin red,
    la cercanía sale de la propia imagen en la CPU, al tamaño del mapa, y pasa por todo lo demás como
    la de la red (escala, cortes, suavizado en el tiempo, filtro guiado contra la luminancia, plano de
    apoyo, cantos finos). `suelo` = la altura (abajo cerca, arriba lejos). `imagen` = 0,50 altura +
    0,25 nitidez + 0,10 saturación + 0,15 centro (las tres últimas emborronadas en cajas de 1/24 del
    mapa y llevadas a 0..1 con su percentil 95). Ni modelo, ni CUDA, ni sesión de ONNX.
  - «Frecuencia variable (VRR) durante el 3D» (`vrr_en_3d`, la clave de siempre): el servicio la
    toma ahora sin reiniciarse.
- MEDIDO con `odysseyvk mapa` (persona, nave, globo; `../pruebas/sin-red-suelo-imagen-2026-11-26.jpg`):
  con la red 14–18 ms por mapa; sin red 8–9 ms (suelo) y 9–12 ms (imagen), todo en la CPU. Lo que
  sale: `suelo` es un plano inclinado igual en cada cuadro; `imagen` da algo de forma (la persona,
  la nave) pero también saca hacia delante lo nítido aunque esté lejos (los subtítulos, los bordes
  del planeta). En el visor aparte: «video normal a 3D con CPU (sin red: imagen)», 9,6 ms por mapa,
  y el visor ocupa 257 MB en la tarjeta (con la red, ~1,1 GB más el contexto de CUDA).

## Sin tirones en los juegos: el VRR se apaga durante el 3D (v1.11)

- Rodolfo: «tuve tirones en Cyberpunk». Se buscó en la rama `experimental` (su CAMBIOS.md cuenta cada
  partida) y se pasa aquí lo que quedó probado; Rodolfo, tras la partida de las 04:28: «ya va fluido».
- **La sincronización adaptativa (VRR) se apaga de fábrica mientras dura el 3D** (al salir vuelve
  como estaba). MEDIDO con la línea nueva de tirones, en los tramos con el juego bajo 100 cuadros:
  con VRR, el visor esperaba su imagen libre 19,8 ms de mediana (peor 34,8) y el 25,1 % de sus
  presentaciones salía tarde; sin VRR, 9,8 ms (peor 16,4), 0,1 % tarde, y el visor a 165 fijos. El
  cuadro en la tarjeta tardaba ~1 ms y la valla 0: no era la tarjeta, era el refresco siguiendo al
  juego. `vrr_en_3d = si` lo deja como en el escritorio (antes el valor por defecto era dejarlo).
  El mensaje del registro dice ya lo que queda puesto («apagada durante el 3D (en el escritorio:
  Always…)»); antes decía siempre lo del escritorio.
- **Los comentarios al final de una línea de serve.conf ya no se cuelan en el valor**
  (`vrr_en_3d = no   # prueba` se leía «no   # prueba», que no es «no»): `loadServeCfg` y
  `ajuste::leerServeConf` cortan en un `#` tras un espacio.
- **La medición de tirones**, cada 5 s en el registro: `[fps]` dice los cuadros nuevos del juego
  (`fuente`) y su hueco más largo, el trabajo del visor por vuelta, lo que espera al presentar, leer
  la copia chica y subir el mapa; `[tirones]` desglosa la espera (valla de la tarjeta, adquirir una
  imagen, entregar) y lo que tarda nuestro cuadro en la tarjeta (marcas de tiempo).
- **La red de profundidad**: el registro dice en qué tarjeta abre («con NVIDIA (tarjeta 1)») y el
  servicio toma `red` de serve.conf sin reiniciarse. `red = 1` deja la RTX 4090 para el juego (la
  red tarda lo mismo en la 5080: 12,9 ms contra 13,7).
- **El visor aguanta ir en «modo seguro»** (un binario con `setcap`, como el que hace falta para la
  prioridad alta de la cola, que se queda en la rama experimental): da su propio app_id
  (`GLFW_WAYLAND_APP_ID`), así el servicio lo reconoce y baja el juego; abre el bus de sesión a mano
  si el de siempre falla (atajos globales, pausa del vídeo, avisos, bandeja: `src/bususuario.h`); y
  si tuvo el permiso, lo suelta en cuanto crea la cola. `ODYSSEYVK_PRIORIDAD=alta` pide la cola de
  prioridad alta (sin permiso, se queda en la normal y el registro dice por qué).
## Los comentarios al final de una línea de serve.conf ya no se cuelan en el valor (rama `experimental`)

- La partida de las 04:20 con los arreglos del modo seguro: el juego ya pasa por debajo del visor,
  los atajos globales vuelven, cola alta, red en la tarjeta 1. Pero el VRR siguió puesto: la
  línea `vrr_en_3d = no   # 2026-11-26: prueba…` se leía con el comentario dentro del valor («no   #
  …» no es «no»). `red = 1   # …` sí valía porque es un número. Ahora `loadServeCfg` y
  `ajuste::leerServeConf` cortan el valor en un `#` que vaya tras un espacio.
- Lo que midió esa partida, con el VRR todavía puesto: con el juego a 57–90 cuadros, adquirir
  15–34 ms y presentar >12 ms en 22–269 de cada ~400 presentaciones; con el juego a ~160, adquirir
  ≤ 10 ms y 0–1 tardías. La valla siempre 0 y el cuadro en la tarjeta 0,75–1,37 ms: la tarjeta no
  hace esperar al visor. 32 de 45 tramos de 5 s con el juego por debajo de 100 cuadros.

## El permiso de la prioridad dejaba el visor detrás del juego (rama `experimental`)

- Rodolfo, tras dar `setcap cap_sys_nice+ep` al binario: «lo está mandando tras el juego». MEDIDO en
  la partida de las 04:16: la cola salió con prioridad alta (el permiso sirve), pero
  - el servicio no encontró la ventana del visor en toda la sesión: ni «el video pasa por debajo»
    ni «ventanas más pasaron por debajo». Con un permiso de fichero el proceso queda «no volcable» y
    KWin no puede leer su /proc/<pid>/exe, de donde saca el app_id cuando el programa no lo da; sin
    app_id «odysseyvk» el servicio no reconoce el visor y no baja el juego, que queda encima;
  - el visor: «sin atajos globales (no hay bus del usuario)»: glibc arranca en modo seguro y
    sd_bus_open_user lee la dirección del bus con secure_getenv (vacía). También le pasaba a la
    pausa del vídeo (MPRIS) y a los avisos del escritorio.
- Arreglos: el visor da su app_id (`GLFW_WAYLAND_APP_ID = odysseyvk`); `abrirBusUsuario` (nuevo
  `src/bususuario.h`) abre el bus a mano con DBUS_SESSION_BUS_ADDRESS o XDG_RUNTIME_DIR si el de
  siempre falla (atajos, MPRIS, avisos, bandeja); y en cuanto la cola tiene su prioridad, el visor
  suelta el permiso (capset a cero) y vuelve a ser volcable. Sin probar con el permiso puesto
  (hace falta sudo).
- Lo que dijo la medición nueva: con prioridad alta el cuadro tarda en la tarjeta 0,8–1,3 ms y la
  valla 0: la tarjeta ya no es la que hace esperar. La espera está en ADQUIRIR (imagen libre del
  intercambio): 35–61 ms, con el visor tapado (KWin no presenta una ventana tapada). En la partida
  de las 04:03 (sin desglose) las esperas de 15–23 ms con el juego a ~78 cuadros cuadran con el
  VRR siguiendo al juego. Para la próxima prueba: `vrr_en_3d = no` en el serve.conf de Rodolfo
  (respaldo `serve.conf.respaldo-2026-11-26-antes-vrr`): 165 Hz fijos durante el 3D.

## Prioridad del visor en la tarjeta (rama `experimental`)

- La partida de las 04:03 con la medición nueva: el juego mandaba parejo (74–84 cuadros/s, huecos
  de ~30 ms) y el trabajo del visor cabía (vuelta ≤ 6 ms), pero al presentar esperaba a la tarjeta:
  en las escenas pesadas, 62–145 de cada ~400 presentaciones por 5 s tardaron más de 12 ms (máx.
  15–23 ms). La 4090 al 100 % con Cyberpunk y nuestro cuadro en la fila detrás del juego. Con el
  juego a 120–155 cuadros (más margen) casi no hubo esperas. La red en la 5080 no cambió la caída
  del juego (17–24 ms por mapa contra 20–25). Y seguía en pantalla completa exclusiva (3 veces
  minimizado el visor, tarjeta «El juego tapa el 3D»).
- Experimental → VISOR → «Prioridad del visor en la tarjeta» (`exp_prioridad = normal|alta|tiempo_real`
  → `ODYSSEYVK_PRIORIDAD`): la cola del visor se crea con `VK_KHR_global_priority` HIGH o REALTIME,
  para que su cuadro pase por delante del juego (lo que hacen los compositores de realidad virtual).
  Si el controlador no la da, el dispositivo se crea con la de siempre y el registro dice por qué.
  MEDIDO: el controlador de NVIDIA la niega a un proceso normal (`VK_ERROR_NOT_PERMITTED`: hace falta
  CAP_SYS_NICE en el binario, como gamescope).
- `[tirones]` cada 5 s, debajo de `[fps]`: dentro de presentar, cuánto se esperó la valla (que la
  tarjeta acabe el cuadro anterior de ese hueco), adquirir (una imagen libre: el refresco) y entregar;
  y lo que tarda nuestro cuadro EN LA TARJETA (marcas de tiempo al principio y al final de su lista de
  mandos), media y máximo, con la prioridad de la cola. En el visor aparte, sin juego: 0,94 ms de
  media y 3,3 de máximo por cuadro (con la trama y el conversor).
- El registro dice ya en qué tarjeta abre la red: «con NVIDIA (tarjeta 1)».

## Tirones en Cyberpunk: la red a la 5080 y la medición de tirones (rama `experimental`)

- Rodolfo: «tuve tirones en Cyberpunk, checa si ves un posible motivo». MEDIDO en el registro de
  las dos partidas (26-nov, 02:59–03:07 con la red a 770 y el mapa a 770, 03:07–03:10 de fábrica):
  - Todo en la RTX 4090: el juego, KWin, el visor 4K a 165 Hz y la red de profundidad. La 5080, sin
    uso. Comprobado con `nvidia-smi` mientras corría `odysseyvk mapa`: `--red 0` cae en la 4090 (bus
    01) y `--red 1` en la 5080 (bus 05; deja además un contexto de ~390 MB en la 4090). La misma red
    tarda lo mismo en las dos (13,7 y 12,9 ms por mapa, sin juego).
  - Dentro del juego la red tardó 20–25 ms por mapa de fábrica (14 fuera) y 38–51 con 770: el juego
    y la red se estorban. La red hace como mucho 6 mapas por segundo (`mapas_por_segundo`), o sea
    un 12–15 % del tiempo de la tarjeta de fábrica y un 25–30 % a 770.
  - Cuadros por segundo: de 127 lecturas, 59 bajo 100 (66–86) y 44 a 160 o más, en las dos
    partidas y también con la red de fábrica; en esos tramos llegaban igual de pocos cuadros del
    juego, así que la caída es del juego.
  - 7 veces el escritorio minimizó el visor y salieron 2 tarjetas de «El juego tapa el 3D»
    (pantalla completa exclusiva: tiene que ir sin bordes).
  - 57 «cambios de plano» en unos 10 minutos (al principio uno cada 3 s): cada uno reinicia de
    golpe la escala y el plano; en un juego, un giro rápido de cámara cuenta como cambio de escena.
- `red = 1` en el serve.conf de Rodolfo (respaldo `serve.conf.respaldo-2026-11-26-antes-red1`): la
  red en la 5080 y la 4090 para el juego. El servicio toma ahora `red` de serve.conf sin reiniciarse
  (vale desde el siguiente vídeo).
- La línea `[fps]` (cada 5 s) dice ahora quién tira: `fuente` = cuadros nuevos del juego por
  segundo y su hueco más largo (y cuántos de más de 50 ms); `vuelta` = el trabajo del visor antes de
  presentar (y cuántas veces no cupo en un refresco); `presentar` = lo que espera a la tarjeta y al
  refresco (y cuántas veces más de dos refrescos), con el hueco más largo entre presentaciones;
  `leer`/`subir` = media/máximo de bajar la copia chica para la red y de subir el mapa (las dos
  esperan a la tarjeta); `red` = ms por mapa. Probado en el visor aparte con la red en cada
  tarjeta: 60 cuadros, vuelta máx. 2,6–3,0 ms, leer 1,4–1,6 / subir 1,3–1,6 ms, red 13–14 ms, sin
  turnos perdidos. Falta la partida de verdad.

## Rama `experimental`: la pestaña Experimental (sobre la v1.10)

- Rodolfo, tras leer Any2Odyssey (github.com/jlobue10/Any2Odyssey, 1.0.7, GPL-3: visor en Rust para
  el mismo G90XF; en Linux pasa por Wine con los programas de Leia): «haz la rama, y en las ramas mete
  todo a una pestaña llamada experimental, con opciones por categoría también». Se toman IDEAS, no
  código (su licencia no es la nuestra).
- Pestaña **Experimental** (la quinta), con tres categorías; cada prueba se guarda en serve.conf, se
  pasa al visor por entorno y vale desde el siguiente vídeo:
  - RED DE PROFUNDIDAD · «La red ve el cuadro a su proporción» (`exp_red = cuadrado|518|770` →
    `ODYSSEYVK_RED_ENTRADA=WxH`): de fábrica la imagen 16:9 se aplasta a 518×518; con 518 o 770 la
    red la ve a 518×294 o 770×434 (parches de 14) y la salida se remuestrea al mapa. «Tamaño del mapa
    de profundidad» (`exp_mapa = 518|770|1022` → `ODYSSEYVK_MAPA`): los puntos del mapa que llega a la
    trama; la entrada de la red es cosa aparte (de fábrica 518², siempre).
  - SUAVIZADO EN EL TIEMPO · «Tope de arrastre» (`exp_tope_arrastre = si` → `ODYSSEYVK_TOPE_ARRASTRE=1`):
    la idea de Any2Odyssey 1.0.0; tras mezclar el mapa nuevo con el anterior, ningún punto se queda
    a más de 1/16 de la escala del nuevo.
  - SEGUIMIENTO DE LA CABEZA · «Anticipar la cabeza» (`exp_anticipacion = 0..60` ms →
    `ODYSSEYVK_ANTICIPACION`): la pose de los ojos se adelanta su velocidad (media móvil 0,5 sobre las
    muestras aceptadas) por esos milisegundos, topado a 3 cm. Any2Odyssey da 25 ms a 120 Hz al
    tejedor de Leia. 0 = apagado.
- La red (Depth Anything V2 Small, el mismo archivo que usa Any2Odyssey) tiene los ejes dinámicos:
  acepta cualquier entrada múltiplo de 14 sin reexportar. MEDIDO con una sonda aparte en CPU: 518²
  128 ms, 518×294 96, 770×434 238, 1022×574 502, 1274×714 1120.
- MEDIDO con `odysseyvk mapa --sueltas` en la RTX 4090 (ms por mapa, voy-03 / core-03):
  - de fábrica (518², mapa 518): 13,6 / 15,4 · 518×294: 12,4 / 11,0 · 770×434: 19,5 / 20,2.
  - mapa 770 (red 518²): 25,3 / 25,3 · mapa 1022: 41,7 / 39,2 · 770×434 + mapa 1022 + tope: 43,9 / 41,0.
  - **No abren en CUDA** 770×770 ni 1022×574: `Non-zero status code … Softmax node /blocks.0/attn`
    (más de ~1 700 parches; 770×434 = 1 705 abre, 1022×574 = 2 993 no). Por eso el panel ofrece
    hasta 770 y la entrada de la red no sigue al tamaño del mapa.
  - Lo que cambia el mapa a su proporción (`../pruebas/experimental-red-proporcion-2026-11-26.jpg`):
    la silueta de la nave sale más ceñida; en el globo cambia mucho (a cuadrado el globo queda
    oscuro, lejos; a 16:9 sale redondo con su degradado y el aro delante). El plano de apoyo que
    elige la saliencia cambia con ello: voy-03 0,953 → 0,828 (518×294) → 0,766 (770×434); core-03
    0,391 → 0,641 → 0,297. Cuál es «mejor» hay que verlo en el panel.
  - Tope de arrastre, con tres cuadros iguales y luego la nave corrida 150 px (sin corte de escena):
    contra el mapa del cuadro corrido suelto, diferencia media 13,4 → 8,8 de 255, máxima 112 → 27,
    puntos a más de 1/8 de la escala 10,8 % → 0 % (a más de 1/16, 26,9 % → 22,5 %: el filtro guiado
    va después del tope).
- Sin probar: la anticipación (en el taller no hay cámara) y todo ello en el panel con un vídeo.
- Ideas de Any2Odyssey que NO entran (todavía): TensorRT en media precisión (1,2 ms por mapa en su
  5090, pero 2,8 GB de bibliotecas que Arch no empaqueta), retener los cuadros tempranos de una
  película a 24 (primero medir si nos pasa), su script de KWin (nosotros capturamos la ventana, no la
  pantalla).

## Profundidad 75 y Salida 50 de fábrica, recordadas por modo (v1.10)

- Rodolfo: «on conversion default the depth to 75 and salida to 50 (corrigió el 100), on side by
  side too, but once user modifies remember each value separately»; en lado a lado, solo el
  suavizado.
- El visor, al entrar en cada modo: conversor (`conv`) y dividido suavizado (`suave`) arrancan en
  Profundidad 75 y Salida 50; el dividido con su propio relieve (`sbs`), neutro (50 y 0). La
  Profundidad 75 de fábrica deja el plano FIJO en 0,75, que es lo que se veía al mover el deslizador
  a 75 (moverlo siempre apagaba el plano automático). `profundidad_conv = auto` vuelve al automático
  (la saliencia elige el plano de cada escena).
- Al mover uno de los dos (tecla, pastilla o `ctl ajuste`) y dejarlo quieto un segundo, se guarda
  ese valor para ese modo en serve.conf (`profundidad_conv`, `salida_conv`, `_suave`, `_sbs`), cada
  uno por su lado, y las sesiones siguientes arrancan con él. No se toca con `--apoyo`/`--relieve`
  ni en el modo de legado.
- serve.conf ya no gana un renglón en blanco con cada clave nueva.
- Comprobado en el visor aparte (gamescope sin pantalla, HOME aislado, `ODYSSEYVK_VALORES=1`):
  conversor de fábrica «profundidad 75 (plano fijo) · salida 50»; `ctl ajuste apoyo 60` y
  `relieve 35` → `profundidad_conv = 60` y `salida_conv = 35` guardados por separado; la sesión
  siguiente arranca «60 · 35 (recordados)»; el dividido, «50 · 0 (de fábrica)». Sin probar: los
  cambios de modo con los botones de la pastilla en una sesión de verdad.

## Atajos de teclado personalizables, y el panel se queda con el teclado (v1.09)

- Rodolfo: «Poner una sección de atajos de teclado debajo de PANTALLA, que también deje personalizar
  cada atajo»; y, mientras: «cuando la configuración esté abierta no permitas que el juego tome
  teclas».
- Ajustes → **ATAJOS DE TECLADO**, debajo de PANTALLA: el interruptor de los atajos globales (pasa
  aquí) y las ocho acciones (3D, menos/más profundidad, menos/más salida, pastilla más chica/grande,
  ocultar o mostrar la pastilla), cada una con su combinación en una tecla. Se pica la fila, la
  tecla se enciende («Pulsa las teclas…») y la combinación que se pulse queda puesta. Hace falta
  Ctrl, Alt o Meta (una tecla suelta se comería esa tecla en todo el escritorio); si otra acción ya
  la usa, se dice cuál y se sigue esperando; Esc o un clic en otra cosa cancela; la flecha circular
  ↺ vuelve a la de fábrica.
- Se guardan en serve.conf como `atajo_3d = Ctrl+Alt+3` (`atajo_profundidad_menos`, `_mas`,
  `atajo_salida_menos`, `_mas`, `atajo_pastilla_menos`, `_mas`, `atajo_pastilla`); la de fábrica se
  guarda vacía. Al cambiar uno, el visor abierto recibe la orden `atajos = recargar`: vuelve a
  registrar los globales en KDE con las combinaciones nuevas y su tecla local también las sigue.
  KDE las recibe con NoAutoloading: mandan las del panel, no las que KDE recordara de antes.
  Ctrl+Shift+2 (la vieja de ocultar la pastilla) sigue valiendo salvo que otra acción la tome.
- Con algún atajo cambiado, el renglón de teclas de la pastilla los enseña (los pares con los
  mismos modificadores juntos: «Ctrl+Shift+3/4») y no pasa de la casilla de «no volver a preguntar».
- El panel pide el teclado en exclusiva a la capa (layer-shell, `keyboard_interactivity`) mientras
  está abierto: el juego o el vídeo de debajo no reciben ninguna tecla. Esc cierra el panel (o
  la lista de idiomas, si está abierta). Las teclas se leen con xkbcommon en su nivel de base, así
  Mayús+1 se apunta como Shift+1 y no como «!».
- Comprobado sin panel: el panel pintado en cuatro idiomas con dos atajos cambiados y uno esperando;
  la pastilla con y sin atajos cambiados; la lectura de combinaciones con un programa aparte
  («ctrl + shift + k» → Ctrl+Shift+K, «Strg+Umschalt+F1» → Ctrl+Shift+F1, «K» y «Shift+K» sin
  modificador válido). Sin probar en el escritorio: la captura de teclas en el panel abierto y el
  registro en KDE de una combinación cambiada.

## La tarjeta de aviso cambia de idioma con las demás (v1.08)

- Rodolfo, con la 1.07 en japonés: la tarjeta «El juego se cerró a los 28 s de entrar en 3D» seguía
  en español y solo el botón decía «了解». El servicio guardaba el título y los renglones ya
  traducidos al momento de ponerla (antes de cambiar el idioma); el botón se traduce al pintarse.
- Ahora el aviso se guarda como receta (los textos en español y sus datos, como los segundos) y,
  cuando el servicio nota en serve.conf el idioma nuevo, lo vuelve a escribir y repinta la tarjeta.
  Las otras dos tarjetas (la pregunta del vídeo y la de actualización) ya se traducían al pintarse.

## El idioma: inglés de fábrica y seis más (v1.07)

- Rodolfo: «Crea un botón de idioma, haz el default el inglés, pero mantén Español, Francés, Alemán,
  Chino y Japonés»; y luego «añade el idioma ruso».
- `src/idioma.h/.cpp`: los textos siguen escritos en español en el código y `idioma::tr()` /
  `idioma::trf()` (con `{}` para los datos) los cambian por los del idioma puesto. La tabla
  (177 textos × 6 idiomas) sale de `tools/traducciones.py` (+ `traducciones_ru.py`) a
  `src/idioma_tabla.inc`; `python3 tools/traducciones.py revisar` comprueba que cada texto que el
  código pasa por `tr` está en la tabla (200 usos, 0 sin traducir).
- `idioma = en|es|fr|de|zh|ja|ru` en serve.conf; sin la clave, inglés. `ODYSSEYVK_IDIOMA` lo pisa
  (pruebas).
- Panel: un globo con el código (EN, ES…) a la izquierda de la flecha de actualizar; picado cuelga
  la lista de los siete idiomas, cada uno con su nombre en su idioma. La misma elección está en
  Programa → Otros → Idioma. El cambio es al momento: el panel rehace su atlas de letras, al visor
  abierto le llega la orden `idioma` (la pastilla cambia sin cerrar el vídeo) y el servicio lo ve
  en su siguiente mirada a serve.conf (tarjetas y rótulo de la bandeja). La entrada de la barra de
  tareas cambia de título.
- El atlas de letras carga además los signos de fuera del latino-1 que use el idioma: el ruso y la
  «œ» francesa salen de DejaVu; el chino y el japonés de Noto Sans CJK (la cara SC o JP de la
  colección), con el atlas a 1024×1024 (lleno hasta 544). La búsqueda de esos signos es binaria.
- Lo que el idioma descolocaba, MEDIDO pintando cada pestaña en los siete idiomas: las notas largas
  (francés, alemán, ruso) se achican hasta un 80 % y, si aún no caben, se cortan con «…»; los
  botones de las tarjetas miden lo que su palabra; las pestañas rusas se achican para no meterse
  debajo del globo; en chino y japonés la casilla de la pastilla es corta y los dos botones de tipo
  de vídeo se montaban: el bloque mide al menos 128.
- Los decimales que se enseñan van con punto en inglés, chino y japonés (serve.conf sigue con coma).
- El guion y el guion bajo se perdían en letra chica (salía «the cave dweller»): su caja ya no
  baja de 1,3 puntos.
- La medición del plano de apoyo (v0.65), repetida con la 1.07 (`odysseyvk mapa --sueltas`,
  `ODYSSEYVK_FOCO_IIR=1`, `ODYSSEYVK_SALIENCIA=0` contra 1, misma red abierta): nave (voy-01..04)
  0,30/0,30/0,36/0,33 → **0,98/0,95/0,95/0,95**, igual que en la v0.65; globo (core-01..03)
  0,36/0,27/0,27 → 0,33/0,30/0,39 (en la v0.65 el core-03 daba 0,42). La elección del plano no ha
  cambiado desde entonces; lo que mejoró después son los bordes del mapa (v0.89), que se afinan en
  el sombreador y no salen en este mapa de la red. Los dos vídeos con caras ya no están guardados:
  la imagen nueva lleva solo la nave y el globo.
- README en inglés y español con capturas nuevas de la 1.07 (las de la interfaz en cada idioma, en
  `docs/img` y `docs/img/es`) y los rótulos de las de profundidad en cada idioma; se rehacen con
  `python3 tools/capturas-readme.py`.

## El botón de desinstalar, en rojo suave (v1.06)

- Rodolfo: «cambia el color del botón de desinstalar por un rojo suave». Los botones del panel
  admiten `peligro`: rojo suave (0,82 · 0,33 · 0,36) en vez del morado.

## El icono fijado en la barra de tareas abre los ajustes (v1.05)

- Rodolfo: «una vez fijado al administrador de tareas el icono no abre el overlay». MEDIDO en su
  registro (00:42:09 y 00:42:15): KDE tomó la ventana del panel como «odysseyvk-play», porque
  `odysseyvk-play.desktop` (el de abrir vídeos con el programa) llevaba `StartupWMClass=odysseyvk`;
  el icono fijado era ese lanzador y al picarlo corría `odysseyvk play` sin archivo: «falta el
  archivo», código 2.
- `StartupWMClass=odysseyvk` pasa a `odysseyvk.desktop` (el que abre los ajustes), y `odysseyvk play`
  sin archivo abre los ajustes en vez de fallar: así funciona también el icono que ya está fijado.

## El panel de ajustes en la barra de tareas (v1.04)

- Rodolfo, con la 1.03 y el panel abierto: no está «en el panel de abajo, en el gestor de tareas».
  El panel es una capa (layer-shell) y KDE nunca pone las capas en el gestor de tareas.
- Mientras el panel está abierto, la capa pone además una ventana normal de 1×1 transparente y sin
  entrada de ratón, con título «OdysseyVK — Ajustes» y el app_id `odysseyvk` (el icono y el nombre
  del lanzador): lo que se ve es su botón en el gestor de tareas. Cerrarla desde ahí cierra el
  panel. El interruptor de Otros pasa a «Mostrar OdysseyVK en la barra de tareas» y vale para el
  visor 3D y para el panel.
- Al volver a encender la bandeja en el panel, el icono vuelve al momento (antes esperaba al
  reintento: MEDIDO, 10 s).
- **No probado**: había un panel abierto (abrir otro lo cierra) y no se tocó.

## Otros: icono en la bandeja del sistema en vez de Alt+Tab (v1.03)

- Rodolfo, con la 1.02: «no en Alt+Tab, sino en el panel de notificaciones». El segundo interruptor
  de Otros pasa a ser «Mostrar OdysseyVK en la bandeja del sistema» (`bandeja = si|no`, encendido
  de fábrica); el de Alt+Tab se quita.
- MEDIDO con un programa de prueba: la bandeja de KDE lo registra, contesta el icono y el texto, y
  el clic central llega. El icono es un StatusNotifierItem por D-Bus de sesión (`src/bandeja.cpp`), puesto por el
  servicio mientras corre: clic = panel de ajustes, clic central = la pestaña Juegos, y el texto al
  pasar por encima. Si la bandeja aún no está (el panel de KDE arrancando), se reintenta cada medio
  minuto. Al apagar el interruptor se quita al momento.

## Otros: el visor en la barra de tareas y en Alt+Tab, a elegir (v1.02)

- Rodolfo: «añade en otros una sección que ofrezca apagar y encender el mostrar en el administrador
  de tareas, o el mostrar en el panel».
- Pestaña Programa, sección OTROS: «Mostrar el visor 3D en la barra de tareas» y «Mostrarlo en el
  selector de ventanas (Alt+Tab)», los dos encendidos de fábrica (`barra_tareas`,
  `selector_ventanas` en `serve.conf`). El servicio se lo pone a la ventana del visor con los
  estados `skiptaskbar` y `skipswitcher` del protocolo de ventanas de KWin, y lo repasa en cada
  vuelta: el cambio vale al momento, también con un 3D ya abierto.
- **No probado** en el panel de verdad.

## La versión nueva se ve al momento (v1.01)

- Rodolfo: «la 0.98 no detecta la 1.0 en GitHub». MEDIDO a las 00:31: la rama main ya era la 1.00
  (`git ls-remote`), pero `raw.githubusercontent.com` seguía sirviendo 0.98 desde su caché
  (`cache-control: max-age=300`, `x-cache: HIT`). Y además el servicio había mirado a las 00:17 y no
  volvía a mirar hasta seis horas después.
- Ahora se pide primero a la API de GitHub (`api.github.com/repos/…/contents/VERSION`, sin caché;
  daba 1.00 en el mismo momento) y, si falla, a raw. Y estando al día, la siguiente mirada es como
  mucho a la hora, no a las seis.

## Juegos: el aviso de por qué el 3D queda detrás y el detector de caídas (v1.00)

- Rodolfo, con Cyberpunk (también bajo gamescope): «it can't see the content of the game and it
  sends the overlay to the background no matter gamescope; create a crash detector… suggest
  opening it in borderless fullscreen; verify why it still got sent to the background».
- **Por qué va detrás** (MEDIDO en su registro, 26-nov 00:22, dos sesiones con gamescope): el
  juego pone su ventana a pantalla completa y activa; KWin coloca la ventana activa a pantalla
  completa en su capa de arriba, por encima de las que llevan «encima fijo» como el visor, y el
  visor no puede activarse sin quitarle el teclado y el ratón al juego. El servicio lo devolvía
  («el gestor de ventanas minimizó el visor: la devolvemos») y el juego volvía a taparlo. La
  captura sí recibía los cuadros (3.542 en 25 s): lo que no se veía era el visor. La salida es que
  el juego vaya en ventana sin bordes, que no entra en esa capa.
- **La tarjeta de aviso** (la de la orilla, con «Entendido»): a la segunda vez que un juego manda
  el visor detrás en una sesión, «El juego tapa el 3D» con la explicación y la sugerencia de ventana
  sin bordes. Y el **detector de caídas**: si la ventana de un juego desaparece antes de cinco
  minutos de entrar en 3D, «El juego se cerró a los N s de entrar en 3D» con la misma sugerencia.
  `gamescope` cuenta como juego. Las tarjetas se retiran solas al minuto; la pregunta de un vídeo
  nuevo manda sobre ellas.
- **No probado** con el juego de verdad. HIPÓTESIS: con el juego en ventana sin bordes el visor se
  queda encima y la captura es la misma.

## Mirar GitHub cada tanto, configurable, y al abrir el panel (v0.99)

- Rodolfo: «check for updates from time to time». El servicio ya miraba medio minuto después de
  arrancar y cada seis horas; ahora el intervalo es `actualizaciones_cada = horas` en `serve.conf`
  (0,25 … 168; 6 de fábrica) y al cambiarlo se mira a los cinco segundos. Además el panel de ajustes
  mira al abrirse (si los avisos no están apagados), para que la pestaña Programa y el botón junto
  al aspa salgan al día.

## Los juegos arrancan en paz: el 3D espera a que la ventana se asiente (v0.98)

- Rodolfo: Cyberpunk «tuvo un crash… creo que es por una ventana de error que se abre y ya el
  juego se lanza luego». MEDIDO en su registro (26-nov 00:11): la ventana del juego se pone a
  pantalla completa, el servicio lanza el 3D al segundo, la ventana se minimiza al segundo
  siguiente (el juego arrancando), la sesión se cierra; vuelve a pantalla completa, otra vez lo
  mismo; y al final el servicio **le quitó la pantalla completa al juego** («el vídeo se fue y la
  ventana del navegador sigue a pantalla completa: se la quitamos»), que es lo pensado para los
  navegadores. Por nuestro lado nada se cayó, pero al juego le tocamos la ventana mientras arrancaba.
- Para `steam_app_*`: el 3D se lanza solo cuando la ventana lleva 4 s seguidos a pantalla completa;
  una minimización de menos de 6 s no cierra la sesión (el visor deja de trazar mientras tanto); y
  nunca se le quita la pantalla completa a un juego.
- **No probado**: con el juego de verdad.

## install.sh arranca el servicio (v0.97)

- MEDIDO en el registro de Rodolfo: tras desinstalar (23:54, el servicio queda desactivado) y
  reinstalar la 0.96 (00:03), el servicio siguió parado: `install.sh` lo activaba para el siguiente
  inicio de sesión pero no lo arrancaba. Ahora lo reinicia al terminar (arranca si estaba parado,
  reinicia con la versión nueva si corría). `odysseyvk actualizar` apunta la sesión a reabrir ANTES
  de llamar a install.sh y ya no reinicia por su cuenta.
- De paso, el cierre con Cyberpunk de las 23:43: el visor estuvo 43 s recibiendo sus cuadros con
  normalidad y la sesión se cerró limpia cuando la ventana salió de pantalla completa; sin volcado.
  Se cayó el juego, no se sabe por qué desde aquí.

## README nuevo para GitHub, en inglés y español; el manual pasa a docs/ (v0.96)

- Rodolfo: «reescribe el README para GitHub, no menciones lo que ya no aplica, y da comandos ya
  listos para copiar y pegar para instalar al inicio de todo… imágenes detallando las ventajas del
  depth… la guía métela en otro archivo… en inglés, con una pestaña de Español a un lado».
- `README.md` (inglés) y `README.es.md` (español), enlazados entre sí arriba: qué hace, instalar
  en tres órdenes para copiar (paquetes, clonar + `install.sh`, arrancar el servicio y `doctor`),
  uso, las ventajas con imágenes (`docs/img/`: mapa de profundidad, bordes antes y después, plano
  del sujeto, barra del reproductor, juegos, panel, primer arranque), actualizaciones, las claves
  de `serve.conf` que importan, hardware y créditos.
- El manual largo en español pasa tal cual a `docs/GUIA.md`; `docs/GUIDE.md` es la guía corta en
  inglés (órdenes, teclas, claves, problemas típicos). Sin cambios en el programa.

## Actualizaciones desde GitHub con tarjeta, botón junto al aspa y pestaña Programa con desinstalar (v0.95)

- Rodolfo: un popup cuando el programa detecte una actualización en su GitHub
  (the-cave-dweller/Odyssey3d-linux), que la instale sola y recargue; si había un vídeo o un juego,
  que se reabra y se reinicie el servicio; un botón pequeño de «no notificar»; el botón de actualizar
  junto al de cierre, amarillo si hay una pendiente (con rótulo al pasar por encima) o flecha verde
  si está al día; una pestaña final con desinstalar (red, programa y servicio). Después: la
  notificación desde el overlay, no de KDE («una ventanita encima»); si hace falta root, pedirlo
  también desde el GUI; y un interruptor de actualizar automáticamente, apagado de fábrica.
- **Cómo se detecta**: el servicio lee el `VERSION` de la rama main con `curl` aparte (medio minuto
  tras arrancar y cada seis horas) y deja el resultado en `~/.local/state/odysseyvk/actualizacion`;
  `versionMasNueva` compara número a número. MEDIDO en seco con una dirección de taller
  (`ODYSSEYVK_URL_VERSION=file://…`): «version en GitHub: 0.88 (instalada 0.94)».
- **La tarjeta**: la misma de la orilla (layer-shell), con el círculo amarillo y la flecha, el
  título, las dos versiones, «Actualizar», «Luego» y, chico, «No avisar de actualizaciones»
  (escribe `actualizaciones = no`). Una vez por versión (`actualizacion-avisada`); se retira sola a
  los 3 min; si llega un vídeo a pantalla completa, la pregunta del vídeo manda.
- **El botón junto al aspa** (en el panel): amarillo con flecha = pendiente, pica e instala; flecha
  verde = al día, pica y vuelve a mirar; gris = sin mirar. Con el ratón encima, el rótulo que lo dice.
- **`odysseyvk actualizar`**: clona (o trae) la rama main en `~/.local/share/odysseyvk/fuente`,
  corre el `install.sh` de siempre con `ODYSSEYVK_SUDO=pkexec` (la clave de root la pide polkit en
  una ventana: es lo único que no puede hacer solo), apunta en `reabrir` la ventana y el modo de la
  sesión 3D que haya (el hub ahora publica `sesion_modo`), reinicia el servicio y avisa con
  `notify-send`. Corre en una unidad transitoria (`systemd-run --user`) para que el reinicio no se
  lo lleve. El servicio nuevo, si esa ventana sigue a pantalla completa, la reabre en el mismo modo
  sin preguntar. Todo queda en `~/.local/state/odysseyvk/actualizar.log`. MEDIDO en una carpeta
  aparte con `--solo-bajar`: clona el repositorio de GitHub y compila (el binario bajado dice 0.94).
- **Pestaña Programa**: versión instalada, la de GitHub, Buscar/Actualizar, «Avisar cuando haya una
  versión nueva», «Actualizar automáticamente» (apagado; con él se instala sola en cuanto se
  detecta) y «Desinstalar OdysseyVK», que pide un segundo clic en 12 s. `odysseyvk desinstalar`
  quita el servicio, el lanzador, el icono, la red y la fuente, y por pkexec el binario, la regla
  udev y los datos; `~/.config/odysseyvk` se queda.
- **No probado**: picar Actualizar de verdad (instala y reinicia el servicio), el pkexec, la
  reapertura del vídeo y el desinstalador: todo lo que toca el sistema. HIPÓTESIS: la unidad
  transitoria hereda el entorno gráfico del panel para que pkexec y notify-send encuentren el
  escritorio.

## Nada de Samsung en el panel: la red se detecta sola; sin red, el panel se abre y la descarga carga (v0.94)

- Rodolfo: «quita ya cualquier cosa del modelo de Samsung, rinde igual, ¿no?; solo deja intrínseco
  que si el programa lo detecta en la carpeta lo elija solito»; «si el programa no detecta el modelo,
  que abra el overlay ya en el menú y deje más en claro que se debe descargar para obtener los datos
  de profundidad»; «que una vez descargado se cierre solito settings y cargue el modelo».
- Fuera el selector de red, la fila del archivo, la «i» de Windows y la clave `samsung =` de la
  0.93. El servicio mira la carpeta de redes: si está `mde_large.onnx` arranca con ella (botón 1 del
  visor) y la abierta va al botón 2; si no, la abierta. `modelo =` sigue valiendo para una ruta a
  mano. De la configuración de Rodolfo se quitaron `modelo =` y `samsung =` (respaldo
  `serve.conf.respaldo-2026-11-25-antes-v094`). OJO: en la carpeta solo cuenta un archivo de verdad
  (MEDIDO: un enlace simbólico a la de Samsung no se detectó; una copia sí).
- Sin ninguna red: el servicio abre el panel de ajustes cada vez que arranca así (ya no solo la
  primera vez), y el visor también lo abre solo cuando se le pide el conversor y no hay red que
  cargar. El panel lo dice arriba del todo, con la «i»: «Sin ella no hay datos de profundidad: el
  vídeo normal no se puede pasar a 3D», qué red es, de dónde se baja y dónde queda, y el botón
  «Descargar (95 MB)».
- Al acabar la descarga el panel manda `cargar_red = si` al visor, que carga la red y enciende el
  conversor (el vídeo pasa a 3D sin cerrar nada); el servicio la toma en su siguiente mirada para
  los vídeos que vengan; y el panel se cierra solo a los 2,5 s.
- MEDIDO en el compositor aparte con una carpeta de inicio sin red: el visor arranca plano y abre el
  panel; `ctl red descargar` baja la red; con la orden, el visor dice «la red de profundidad ya
  está: se carga ahora», «vídeo normal a 3D encendido» y saca mapas. En seco, el servicio toma la
  red en cuanto aparece. El panel pintado en los tres casos.
- **No probado**: picar el botón en el panel de verdad y verlo cerrarse solo (el camino desde la
  terminal es el mismo código).

## La red de Samsung no se pierde al elegir la abierta (v0.93)

- MEDIDO en el registro de Rodolfo con la 0.92 recién instalada (23:11:54): al picar «Abierta» en el
  panel, el servicio quedó con «botón 1 = no hay»: la de Samsung desaparecía del visor (teclas 1 y
  2), porque solo se conocía por `modelo =` y ahora ahí va la abierta.
- Clave nueva `samsung =` en `serve.conf`: la escribe el panel al pasar a «Abierta» (con la que
  estaba puesta) y al elegir un `.onnx` con el navegador. El servicio arranca con la red de
  `modelo =` y deja la otra en el otro botón; sin `modelo =`, con la abierta. MEDIDO en seco con las
  cuatro combinaciones (abierta + samsung → arranca con la 2 y la 1 es mde_large; solo mde_large →
  arranca con la 1; nada → abierta; solo abierta → abierta, botón 1 vacío).
- A la configuración de Rodolfo se le añadió esa línea (respaldo
  `serve.conf.respaldo-2026-11-25-antes-samsung`): él ya había picado «Abierta» con la 0.92.
- A 165 Hz el visor da 164,8 cuadros por segundo con el conversor (su registro de las 22:53).

## La red abierta de fábrica, con descarga desde el panel; selector de red (v0.92)

- Rodolfo: «la solapa de 2D/3D está dispareja»; «verifica que sirva el modelo abierto igual que el
  de Samsung, y hazlo el predeterminado; en la primera apertura debe solicitar al usuario la
  descarga del modelo y colocarlo solito en la carpeta que usa el programa»; en Rendimiento, elegir
  la red abierta o la de Samsung con un navegador de archivos, y una «i» que diga dónde está la de
  Samsung en Windows. Y pregunta si la de Samsung la creó Qwen: no, está en su Windows
  (`C:\Program Files\Odyssey 3D Hub\Conversion\model\mde_large.fp16.onnx.bin`, 52.623.651 bytes,
  ofuscada); Qwen la dejó legible (`mde_large.onnx`, SHA-256 `550beef2…`, la misma que hay aquí).
- **El interruptor**: la bola quedaba a 3 px del borde de arriba y a 1 del de abajo, y las dos
  letras a distinta altura. Ahora a 2 px de cada borde y las letras a la misma.
- **La red abierta contra la de Samsung** (MEDIDO sin panel, 10 cuadros, red de verdad): las dos
  corren con los cantos finos y el canto queda igual de puesto (mediana +1,2 px en la nave y +0,6
  en los papeles con cualquiera de las dos; el 89–100 % a menos de 2 px). La abierta tarda 13–16 ms
  por mapa contra 9–11 (a 6 mapas por segundo no se nota) y ve menos cantos duros (p99 del
  gradiente 0,025 contra 0,031). Los mapas se parecen mucho en las escenas con sujeto claro
  (Pearson 0,95–0,97 en la nave, 0,87–0,89 en la Tierra) y poco en una escena ambigua de papeles
  sobre un portátil (0,24–0,48): ahí cada red reparte la profundidad a su manera y ninguna es
  claramente la buena.
- **De fábrica, la abierta**: sin `modelo =` en `serve.conf`, o con la abierta en él, el servicio
  arranca con ella; con la de Samsung puesta, con esa. El archivo del enlace de Rodolfo
  (huggingface.co/XnetShadow/depth_anything_v2_small) es byte a byte el que ya había aquí
  (99.060.839 bytes, SHA-256 `afb6a5c2…`).
- **Descarga**: el panel (arriba de Ajustes cuando no hay ninguna red, y siempre en Rendimiento)
  dice qué red hace falta y de dónde se baja, y el botón «Descargar» la baja con un `curl` aparte,
  enseña el avance y la deja en `~/.local/share/odysseyvk/modelos/`. Lo mismo desde la terminal:
  `odysseyvk ctl red descargar`. La primera vez que el servicio arranca sin ninguna red abre el
  panel de ajustes solo (una vez: se apunta en la carpeta de estado).
- **Rendimiento**: «Red del conversor» con «Abierta» y «La de Samsung…» (abre el navegador de
  archivos del escritorio, kdialog o zenity, y guarda el `.onnx` elegido en `modelo =`); el
  archivo en uso; y la «i» con la ruta de Windows. El servicio relee `modelo =` al cambiar: vale
  desde el siguiente vídeo.
- MEDIDO en una carpeta de inicio aparte: `ctl red descargar` baja los 95 MB (29 → 51 → 79 →
  «lista»), el archivo queda entero y con su nombre; el servicio en seco sin red escribe el aviso y
  la marca, y con `modelo =` la abierta arranca con ella (botón 2). Las cuatro pestañas pintadas.
- **No probado**: el botón «Descargar» y el navegador de archivos desde el panel de verdad (el
  mismo código que `ctl red descargar`, pero picado con el ratón), y que el panel se abra solo en
  un arranque sin red.

## Interruptor 2D | 3D por juego y desenfoque detrás de la interfaz (v0.91)

- Rodolfo: «en la sección de juegos, que el pill de 3D tenga a la izquierda un 2D, tipo encendido y
  apagado, un toggle por juego, también añade algo de blur al hub, y a las settings».
- La pastillita de cada juego es ahora un interruptor: la bola a la izquierda es 2D (apagado) y a la
  derecha 3D (encendido), con la letra de lo puesto en la bola y la otra tenue. Se pica igual que
  antes y guarda lo mismo (`auto = steam_app_ID:conv|flat`).
- Desenfoque: la capa pide a KWin su protocolo `org_kde_kwin_blur` y le manda la misma caja que la
  región de entrada (la pastilla, la tarjeta o el panel). KWin desenfoca lo que hay detrás; la
  interfaz no cambia de transparencia. Si el compositor no ofrece el protocolo, no pasa nada.
- MEDIDO: la pestaña Juegos pintada con el pintado de CPU (9,5 ms a 2560 × 1440) sale con los cinco
  interruptores. **No probado**: el desenfoque (solo lo puede mostrar KWin en el panel) y que la
  región se quite al cerrar la tarjeta.

## Vídeo dividido: la franja de la barra, limpia y sin quedarse puesta (v0.90)

- Rodolfo: «sobre el reproductor lado a lado, implementa estas mejoras y lo que haga falta… la barra
  de navegación y el tope toman cuadros congelados del video que se ven fatal, ¿puedes blurearlos
  con fondo degradado o poner un fondo negro?» y «asegúrate de que al volver al video sí
  desaparezcan, a veces se quedaba el corte».
- **Las mejoras de la 0.89 en el vídeo dividido**: el dividido suavizado usa el mismo conversor, así
  que ya lleva los cantos finos. MEDIDO sin panel: la sesión arranca con `[conversor] cantos: finos`
  y la franja de la barra convive con el pase de afinado. El dividido tal cual no tiene mapa de
  profundidad: ahí no hay cantos que afinar.
- **Por qué se quedaba el corte** (MEDIDO en su registro, 25/11 19:04–19:05): franja puesta el 99 y
  el 100 % del tiempo con el vídeo en marcha, residuo 67–68 arriba, y el vídeo separando lo más
  cercano 29 y 35 mm (58 mm en otra sesión). El detector casaba las dos mitades solo hasta un 3 %
  del ancho (18 mm): un objeto más cercano que eso no casaba, y sus filas se tomaban por barra
  mientras siguiera en pantalla. REPRODUCIDO con un objeto a 30, 45 y 58 mm en la zona de arriba y
  de abajo: el detector instalado pone una franja del 12,8 al 16,5 %; el nuevo, ninguna (residuo de
  1 a 8, umbral 12).
- **Detector** (`src/barra.cpp`): corrimiento hasta un 12 % (72 mm), trozos de un dieciseisavo de la
  mitad en vez de un octavo (el trozo que pilla el borde de un objeto pesa la mitad) y sin dejar
  márgenes sin mirar. Con los 100 cuadros de prueba da lo mismo que antes en todos los grupos (22
  limpios sin franja, 30 con barra con franja, las rayas finas igual); tarda 1,7 ms por análisis
  (antes 1,1), cinco veces por segundo.
- **La franja** (`franjaBarra` en `shaders/weave.frag`): en vez del cuadro entero (las dos mitades
  del vídeo detrás de la barra), sigue el vídeo: la imagen izquierda sola, en 2D, desenfocada y
  atenuada al 55 %, a la que cada ojo llega con un fundido de 24 píxeles desde su vista en 3D. Encima,
  solo los mandos del reproductor, recortados del cuadro: lo que brilla, destaca de su vecindad y
  no está también en la otra mitad (o es una raya larga y fina: la barra de progreso). Con ese mismo
  criterio se limpia el fondo de los mandos que le caen a la imagen izquierda.
- Visto por el camino con los cuadros de prueba: sin la comprobación de la otra mitad se colaban
  como mandos las rayas claras de un código de barras del vídeo; con un fundido de 48 píxeles y la
  barra al 97 % (el detector no la ve y la franja empieza 27 píxeles por encima) el botón rojo salía
  dos veces; lo rojo de la barra se apagaba a la mitad sobre un vídeo claro mientras «fina» se
  medía por brillo y no por color.
- Coste (RTX 4090, 3840 × 2160, sin freno): 0,75 a 0,9 ms más por cuadro mientras la barra está a
  la vista (suavizado 1,55 → 2,31 ms; tal cual 0,53 → 1,41 ms); sin barra, nada. Capa de validación
  de Vulkan sin avisos nuevos (el bloque de parámetros pasa de 368 a 384 bytes).
- `barra_2d = entera` vuelve a la franja de antes (MEDIDO: sale idéntica a la de la 0.89);
  `barra_2d = no` la apaga.
- **No probado**: nada en el panel, y nada con el reproductor de verdad (YouTube en Firefox): la
  barra de las pruebas es una dibujada aquí. HIPÓTESIS: unos mandos grises o medio transparentes
  pueden no pasar por «claros» y no salir; si falta alguno, `barra_2d = entera`. Tampoco subtítulos
  ni menús del reproductor.

## Cantos finos: el borde de cada objeto en su sitio (v0.89)

- Rodolfo, de vuelta al conversor: «percibo un pequeño ghost de 1-1.5mm junto a algunos planos de la
  imagen al mover la cabeza… quiero que casi no se perciba». Y pregunta si la mancha de la izquierda
  de `calib/3DStackCorrection_A.png` afecta a la calidad.
- **La mancha del mapa A**: MEDIDO, son 311 puntos con valor 253–255 en la esquina de abajo a la
  izquierda (2,1 × 5,5 cm de pantalla), rodeados de ceros: la fase, que ahí baja un pelo de cero, dio
  la vuelta al guardarse en 8 bits. Es un defecto del archivo de fábrica, pero el visor ya lo lee
  como 0 desde la v0.59 (`[correccion] mapa A: 311 puntos dados la vuelta`), con un error de 3/255 de
  ciclo como mucho. No afecta.
- **De dónde salía el fantasma** (MEDIDO, tres cosas que se sumaban):
  1. El mapa llegaba **ensanchado** a propósito (`dilatar`, 3 puntos): en 8 cuadros reales, 753
     cantos, el borde del mapa caía a **25,8 px** (4 mm) por fuera del borde de la imagen (mediana;
     el 80 % entre 22,9 y 40,5). Esa franja de fondo se movía pegada al objeto, y donde acababa, el
     fondo se estiraba en un ojo y se encogía en el otro: una banda del ancho del desplazamiento
     (1–1,5 mm) con el fondo doble. Sin ensanchar queda a 1,9 px (de −0,9 a +6,1).
  2. El color de los dos ojos se leía corrido lo que diga «3D Salida» (15,4 px en 50) y el mapa no:
     mapa e imagen iban desencajados 2,4 mm en todos los cantos (sonda 14: canto del mapa en
     x = 1400, canto del color en x = 1385).
  3. El mapa se repartía por ojo en la CPU a su tamaño (un punto cada 7,4 px), distinto en cada ojo.
  Con una escena de canto conocido (objeto 10 px por ojo): el borde de cada ojo a 9–10 px de su
  sitio (20 px con la cabeza a un lado) y una tira de 10–20 px equivocada junto a él.
- **Cantos finos** (`shaders/weave.frag`, de fábrica): el mapa llega sin ensanchar y en la
  profundidad de la FUENTE. Un pase previo (`afinar`, una vez por píxel) cambia la rampa del mapa en
  cada canto por un escalón puesto donde lo dice el COLOR de la imagen: con los 4 × 4 puntos de
  alrededor saca el color del fondo y el del objeto, mira en qué punto de esa mezcla está el píxel y
  corta a lo largo de una línea de nivel (3 partes de 10 de objeto). Luego cada ojo busca su vista
  sobre esa profundidad (`buscaVista`), sin «convergencia» que la desencaje, y lo que el objeto
  destapa se rellena (`rellenoHueco`) con el fondo pegado al corte; si el corte pasa por mitad de un
  canto borroso, con el degradado tendido de un lado al otro.
- Probado y descartado por el camino, con cuadros reales: cortar por votos de parecido de color (en
  un hombro desenfocado el corte caía a media orilla y a saltos de fila en fila: una raya dentada);
  rellenar el hueco con el fondo en espejo (repite cada raya del fondo y, en un canto borroso, deja
  una raya oscura: 171 | 30 30 37 47 | 94); valor de cada lado por mínimo y máximo (un foso en el
  fondo junto al canto).
- El mapa viaja con **el color de la imagen que lo dio** (cuatro bytes por punto): el mapa se renueva
  6 veces por segundo y el corte se decide con el píxel de ahora contra los colores de entonces, así
  que sigue al objeto aunque el mapa vaya atrasado. Si el color no cuadra con nada (el quinto de
  segundo que sigue a un cambio de plano), no se corta: MEDIDO, 2080 escalones con el mapa de su
  propio cuadro y 798 con el de otro.
- MEDIDO sin panel (`tools/prueba-aparte.sh`, red de verdad): el escalón queda a **+0,7 px** de
  mediana del borde de la imagen (439 cantos en 3 cuadros; el 96 % a menos de 2 px; antes 25,8). En
  la escena de canto conocido el borde de cada ojo queda a 1 px o menos, con la cabeza centrada y a
  un lado, y la tira equivocada baja de 19,6–35,5 a 0,3–1,6 niveles.
- Coste (RTX 4090, 3840 × 2160, sin freno): 0,66 ms por cuadro antes; ahora 1,36 ms si hay que
  afinar en cada cuadro y 1,02 ms si no (el pase se salta cuando no cambian ni la imagen ni el mapa:
  30 de 294 cuadros con la imagen quieta). Con `afinar` dentro de la búsqueda eran 21 ms: de ahí el
  pase previo. La capa de validación de Vulkan no da ningún aviso nuevo.
- `cantos = viejo` en `serve.conf` (`ODYSSEYVK_CANTOS=viejo`) vuelve al camino de antes, con su
  ensanchado, para comparar. Taller: `ODYSSEYVK_DILATAR=N`, `ODYSSEYVK_GUIA_PNG`, sondas 14 y 15,
  `ODYSSEYVK_AFINAR_SIEMPRE`. El registro dice `[cantos] finos: el mapa se afinó en N de M cuadros`.
- **No probado**: nada en el panel. Tampoco vídeo en movimiento (aquí solo cuadros quietos), HDR ni
  juegos. HIPÓTESIS: si el fantasma solo aparece MIENTRAS se mueve la cabeza y se va al parar, lo
  que queda es el retraso del rastreo, no los cantos.

## Lanzador con carátulas y 3D por juego; suavizado automático (v0.88)

- Rodolfo, con la 0.87 (el panel se arrastra «sin saltos»): «sigue con el lanzador y el suavizado».
- **Lanzador**: los juegos van en rejilla de cinco con su carátula. `src/preferencias.cpp` la busca en
  `appcache/librarycache/ID` (`library_600x900.jpg`, suelta o en carpeta de huella; si no,
  `library_capsule.jpg`) y la reduce a 280 × 420 con OpenCV. El atlas de la interfaz es de un canal,
  así que las imágenes van aparte y las pinta solo el pintado de CPU, con las esquinas redondeadas
  por cuenta propia (`Hud::imagenes_`). Sin carátula, la ficha de color con la inicial.
- **3D por juego y arranque solo**: MEDIDO en su registro, la ventana de un juego de Steam se llama
  `steam_app_ID` (Stray: `steam_app_1332010`). La pastillita de cada carta apunta `auto =
  steam_app_ID:conv` o `:flat`, que el servicio ya sabía seguir: con `conv` lanza el 3D sin
  preguntar. Al arrancar un juego desde el panel se apunta lo que diga su pastillita.
- **Suavizado automático** (`dividido_suavizado = auto`, `--suavizado-auto`): con el medidor de
  separación cuatro veces por segundo; por encima de 8 mm pasa al suavizado, y vuelve tras 4 s por
  debajo de 5 mm. En el panel, «Suavizar los vídeos divididos» pasa a tres opciones.
- MEDIDO sin panel: en el compositor aparte, un cuadro con un objeto a 4 cm → «el video separa 40 mm:
  se pasa solo al suavizado»; uno con hasta 7 mm → no cambia; arrancando suavizado con las dos
  mitades iguales → «vuelve a caber (0 mm)» y el conversor se apaga. Lógica en una carpeta aparte: la
  pastillita de Stray deja `steam_app_1332010:flat` y luego `:conv`; las tres opciones dejan `auto`,
  `no` y `si`. La pestaña pintada con sus cinco carátulas (10 ms a 2560x1440).
- **No probado**: nada en el panel; lanzar un juego y que entre solo en 3D; el suavizado automático
  con un vídeo de verdad (el cambio de uno a otro es un corte, sin fundido).

## El panel se arrastra sin tirones (v0.87)

- Rodolfo, con la 0.86: la pausa ya se respeta (MEDIDO en su registro: pausa a los 3 s, sin
  reanudar). El panel «se mueve pero de repente da tirones a otra parte de la pantalla».
- HIPÓTESIS (derivada del código): el compositor tarda un cuadro en mover la capa y los movimientos
  del ratón que llegan mientras tanto vienen medidos contra el sitio viejo; sumarlos otra vez corría
  el panel dos veces. `src/overlay.cpp` espera, después de cada corrimiento, a que el compositor
  pinte ese cuadro (o 80 ms) antes de mirar otro movimiento.
- MEDIDO: el panel real abre y cierra. **No probado**: el arrastre con el ratón.

## Pausas respetadas, panel movible y pestaña de rendimiento (v0.86)

- Rodolfo, con la 0.85 (el cursor del panel «ya está bien»): «por unos segundos volvió a darle play
  a mis pausas»; quiere mover el menú arrastrando su título y una tercera pestaña con el rendimiento.
- **Pausas**. MEDIDO en su registro (18:08): en los primeros 8 s de la sesión el visor reanudó el
  vídeo tres veces («el navegador había parado el vídeo: se reanuda solo»), dos de ellas pausas
  suyas. Esa reanudación era para cuando el 3D cambiaba el modo o la escala del monitor; desde la
  v0.76 no se cambia nada, así que ya solo se hace si de verdad se cambió (y al volver de Tab).
- **Mover el panel**: la capa va anclada arriba a la izquierda y colocada con márgenes (empieza en el
  centro); agarrando la cabecera, los márgenes siguen al ratón (`src/overlay.cpp`, `HitPanelTitulo`).
- **Rendimiento**: el visor apunta en su `estado` los cuadros por segundo y lo que tarda cada mapa;
  la pestaña enseña eso, la red y la cámara, y por cada tarjeta lo que dice `nvidia-smi` (uso,
  memoria, temperatura). Se refresca cada segundo.
- MEDIDO: la pestaña pintada fuera del panel con sus dos tarjetas; el panel real se abrió y se cerró
  en su escritorio con el anclaje nuevo, sin errores del compositor.
- **No probado**: arrastrar el panel con el ratón; la pestaña con un vídeo en 3D abierto; que la
  pausa aguante en los primeros segundos.

## El aspa del panel cierra y Esc se lo lleva (v0.85)

- Rodolfo, con la 0.84 y el panel abierto con el engranaje sobre un vídeo: «no sirvió Esc para salir
  de todo el programa, y el botón de cierre no sirve». MEDIDO en su registro: pulsó el engranaje
  siete veces en tres segundos; el interruptor del suavizado sí respondió y el servicio releyó
  serve.conf solo; la «fuerza del relieve» saltó a 2,00 (el tope); el proceso del panel llevaba 14 s
  de procesador en 48 s y 756 MB.
- HIPÓTESIS del aspa (derivada del código, no vista aún corregida en pantalla): el panel al doble
  lleva su tamaño fijo, así que cuando el compositor anuncia su escala de verdad (1,50 → 1,80) el
  recorte no se rehacía; la ventana se medía con una escala y el ratón con otra, y los golpes caían
  un 20 % corridos: el aspa, a la derecha, quedaba fuera de su sitio y el deslizador saltaba al
  tope. `src/overlay.cpp` rehace el recorte también cuando cambia la escala del monitor y lo dice en
  el registro (`[capa] panel: lienzo …`).
- Esc en el visor cierra también el panel de ajustes; y el panel abierto sobre un vídeo se va cuando
  ese vídeo se acaba. (Esc solo le llega al visor si el teclado lo tiene él: el panel no coge teclado.)
- Pintado: una sola muestra para los cuadros lisos (MEDIDO: 50 → 18,7 ms a 3840x2160, imagen igual);
  el panel se pinta una vez por tanda de eventos y no en cada movimiento del ratón; se guarda un
  lienzo libre en vez de pedir uno nuevo cada vez (de ahí los 756 MB).
- `odysseyvk ajustes --grande` para probar el tamaño del 3D sin vídeo.
- **No probado**: nada de esto con el panel real (había un vídeo abierto).
- Su configuración quedó con lo que tocó probando: `dividido_suavizado = no` y `relieve = 2,00`.

## Panel de ajustes flotante, lanzador de juegos y pastilla de dos botones (v0.84)

- Rodolfo, con la 0.83: «Muchísimo mejor». Pidió (1) dejar en la pastilla solo el botón del conversor
  y el del suavizado, y que desactivar el suavizado solo sea posible en la configuración normal del
  programa; (2) rediseñar los ajustes como una interfaz transparente flotante de bordes curvos sobre
  la imagen normal del monitor, que se abra con un engranaje en el hub del modo 3D o con el icono
  del sistema, sin las funciones avanzadas que sobren; (3) empezar a integrar ahí un lanzador de
  juegos que lea la biblioteca de Steam.
- **Pastilla** (`src/vkui.cpp`, `src/vkview.cpp`): dos botones, vídeo normal y vídeo dividido. El
  dividido va suavizado; clave nueva `dividido_suavizado = no` para quitarlo (`suavizado` a secas ya
  era el filtro del rastreo: en su serve.conf vale 0,35). El servicio lanza «dividido» suavizado o
  tal cual según esa clave (también lo que estaba guardado como `sbs` o `suave`) y se lo pasa al
  visor (`--sin-suavizado`); el cambio desde el panel llega al visor abierto por el canal de ajuste.
  Engranaje al final de la pastilla.
- **Panel de ajustes** (`odysseyvk ajustes`): `src/vkui.cpp` lo dibuja (filas genéricas: sección,
  interruptor, opciones, deslizador, fila con aspa, juego; lista que corre con recorte),
  `src/overlay.cpp` lo pone en una capa superpuesta centrada (ratón, arrastre de deslizadores, rueda)
  y `src/preferencias.cpp` dice qué filas hay y qué hace cada una. Un fichero con el número del
  proceso hace de interruptor: abrirlo otra vez lo cierra.
- **Juegos** (`prefs::juegosSteam`, `prefs::lanzarJuego`): las bibliotecas de `libraryfolders.vdf`, los
  `appmanifest_*.acf` instalados, sin herramientas, ordenados por `LastPlayed`.
- `src/main.cpp`: el servicio relee de serve.conf, cuando cambia, las claves que toca el panel;
  `ctl ajuste` acepta órdenes de palabra. `odysseyvk.desktop`: el icono abre el panel; acciones
  «Juegos» y «Ajustes de taller» (la ventana Qt de siempre, que no se toca).
- MEDIDO sin panel:
  - botón de dividido en el compositor aparte: de normal pone el suavizado; con `--sin-suavizado`,
    lado a lado tal cual; `ctl ajuste dividido_suavizado=no` con el suavizado puesto lo quita al
    momento (el registro: «los ajustes quitan el suavizado» y el conversor se apaga);
  - lógica del panel en una carpeta aparte con copia de su serve.conf: los 11 golpes (modo, los
    cuatro interruptores, los dos deslizadores, olvidar zen) dejan cada clave en su sitio; arrastrar
    un deslizador no escribe hasta soltar; `suavizado = 0,35` y el resto, intactos;
  - zonas de ratón (`ODYSSEYVK_HUD_PANEL=0|1 odysseyvk ctl hud`): opciones 4 180 celdas con las tres
    vistas, interruptores ~20 800, deslizadores 6 520 con recorrido 0,00..1,00, cada juego ~19 300,
    aspa 462, pestañas 1 155 / 1 092; la rueda corre la lista 90 puntos;
  - biblioteca: 5 juegos (Harold Halibut, Stray, Cyberpunk 2077, Spyro Reignited Trilogy, Ninja
    Gaiden 2 Black); Proton, el Steam Linux Runtime y los redistribuibles, fuera.
- MEDIDO en su escritorio, sin sesión 3D abierta (un segundo en pantalla): `odysseyvk ajustes --dump`
  puso el panel en DP-4, el volcado del lienzo real mide 1490 × 1094 y se ve bien; un segundo
  `odysseyvk ajustes` lo cerró y quitó el fichero del proceso.
- **No probado**: los clics, el arrastre y la rueda con el ratón de verdad; el panel sobre un vídeo en
  3D (al doble) y el engranaje; lanzar un juego; que el servicio relea al cambiar el fichero (se
  compila y es corto, pero no se corrió); la acción «Juegos» del icono.
- Dos errores míos en la prueba: una orden encadenada iba a correr el programa de prueba de la lógica
  contra su serve.conf real (la frenó el sistema; no se ejecutó), y el compositor aparte se quedó
  colgado cuando el cliente salió enseguida (`tools/prueba-aparte.sh` lleva ahora `timeout -k 3`; su
  proceso se llama `gamescope-wl`, no `gamescope`).
- Pendiente del lanzador: carátulas, elegir el 3D por juego y arrancar el 3D solo.

## Vídeo dividido suavizado, medidor de separación y diafonía pareja (v0.83)

- Rodolfo, con la 0.82: la franja de la barra «es mucho mejor, ya se puede controlar, está quieta».
  Y pidió revisar los vídeos divididos: «en objetos muy cercanos el fantasma que queda de imagen es
  de varios centímetros, casi 4», por si es de las calibraciones o hace falta un suavizado también
  para esos vídeos. MEDIDO por él en el panel, a media tarea: en objetos normales 2–3 mm; las
  personas se ven bien; puertas y fondo, unos 3 mm de fantasma; cosas cercanas, milímetros.
- DERIVADO: la distancia entre imagen y fantasma es la separación que el vídeo trae entre sus dos
  imágenes a esa profundidad; no depende de la calibración. 4 cm en este panel (598 mm de ancho,
  3840 puntos) son 257 puntos, el 6,7 % del ancho, y a 67 cm (lo que mide el rastreo) 3,4°. El
  conversor de vídeo normal, con los topes por ojo de Samsung (+12,67 / −9,98 puntos), no pasa de
  ~25 puntos entre ojos: 3,9 mm. Lo normal de sus vídeos (2–3 mm) cabe; lo muy cercano se sale diez
  veces. Lo que sí depende de la lente y de su compensación es cuánto se VE el fantasma (la diafonía).
- **Vídeo dividido suavizado** (`--suave`, tercer botón de la pastilla, `auto = app:suave`): el
  conversor trabaja solo con la imagen izquierda de la ventana, estirada al ancho entero.
  `shaders/weave.frag`: `fuente()` y `tun10.x`; la franja de la barra del reproductor pasa por delante
  del conversor. `src/vkweave.*`: `setMediaFuente`, `dropDepthMap`, `readSmall(…, entera)` (el
  detector de la barra sigue viendo el cuadro entero). `src/vkview.cpp`: al cambiar de imagen se
  espera al hilo de la red, se tira su resultado a medias, se olvida su memoria y se tira el mapa.
- **Medidor de separación** (`medirSeparacion`, `src/barra.cpp`): bloques de 16 × 16 de la imagen
  izquierda buscados en la derecha hasta el 10 % del ancho, con afinado por parábola; una medida por
  segundo y un renglón `[relieve]` cada 20 s, en milímetros de este panel.
- **`crosstalk_base`** (ajuste en vivo y clave de serve.conf): la compensación pareja de la diafonía,
  que no tenía mando (`crosstalk` mueve solo el término que pesa hacia los bordes: MEDIDO con
  `odysseyvk attrs`, su factor vale 0 en el centro y 0,11..0,41 en las esquinas).
- MEDIDO sin panel y sin abrir ventanas (`tools/prueba-aparte.sh`):
  - suavizado sobre un cuadro dividido contra el conversor sobre su imagen izquierda ya estirada: lo
    que ve la red difiere 0,09 niveles, el mapa de profundidad correlaciona 0,9999 y la trama difiere
    0,48 niveles de media (el estirado no es idéntico punto a punto);
  - cambiar a suavizado en marcha da la misma trama que arrancar en suavizado (0,00 %); volver a
    dividido da la de lado a lado de siempre (0,00 %);
  - con la barra del reproductor: las dos franjas, idénticas al cuadro entero (0,00), en 3D y con el
    3D apagado; el centro es la imagen izquierda (0,05 niveles en 2D);
  - medidor: dos mitades iguales 0,0 mm; un objeto puesto a 1, 2 y 4 cm → +11,2, +20,9 y +39,8 mm;
    28 escenas con separación sintética de hasta 7,2 mm → entre −6,9 y +7,2; 1,0..1,6 ms por medida;
  - `crosstalk_base`: 0,03 por entorno al arrancar y 0,05 por `ctl ajuste` en marcha, los dos
    reflejados en `estado` y en `preferencias.log`.
- **No probado**: nada en el panel. Si el relieve del conversor sobre media imagen (la mitad de
  puntos a lo ancho) convence. Si subir `crosstalk_base` baja el fantasma de 3 mm o lo vuelve
  negativo: eso solo se ve delante del monitor.
- Pendiente, si el suavizado convence: que sea automático (el relieve propio del vídeo mientras
  quepa, y el suavizado solo mientras algo se salga), usando el medidor.

## La franja de la barra del reproductor, sin parpadeo (v0.82)

- MEDIDO en el registro de Rodolfo con la 0.81 y YouTube (25/11, 15:30–15:32):
  - vídeo dividido de verdad (centro 0,6..1,7), 30 s: la franja se puso cuatro veces (5,3 % y 7,2 % de
    abajo; una vez con 4,7 % de arriba) y dos de los tres apagones duraron 0,3 s y 0,8 s: parpadeo.
    Al ponerse solo pasaban el umbral 4 a 6 filas (residuo 46..96): justo el mínimo de 4;
  - al entrar con el ratón sobre la barra: 15,9 % de abajo (16 filas);
  - vídeo normal visto como dividido: «no parece dividido» (centro 57 y 138), ninguna franja;
  - 119,8 cuadros por segundo con el detector puesto.
- `src/barra.cpp`:
  - el residuo va en **color** (los tres canales) y no solo en luminancia. MEDIDO en los cuadros de
    prueba: la barra al 31 % pasa de 29..32 a 73..74 y al 55 % de 26 a 84; las 28 escenas limpias
    siguen en 11 como mucho;
  - **poner la franja pide más que mantenerla**: para ponerla, dos análisis que la vean en 0,5 s (ya
    no tienen que ser seguidos), cada uno con 4 filas sobre el umbral o 2 sobre el doble; para
    mantenerla, 2 filas sobre el umbral dentro de la franja o 1 sobre el doble; se quita a los 0,6 s;
  - suavizado con suma acumulada (cada análisis: 1,1..1,4 ms con los tres canales).
- `src/vkview.cpp`: análisis cada 6 cuadros nuevos **o cada 0,2 s** aunque no llegue ninguno (con el
  vídeo en pausa la 0.81 podía no llegar a analizar); un fallo suelto al bajar la copia ya no apaga
  el detector (hacen falta cinco seguidos); parte cada 20 s en el registro; `ctl barra guardar [N]`
  guarda copias chicas del cuadro (la orden lleva su hora y caduca a los cinco minutos).
- `tools/prueba-aparte.sh` (nuevo): el visor de prueba dentro de gamescope sin salida. Es la manera
  de probar por Vulkan sin abrir ventanas en el escritorio de Rodolfo (lo que hice mal con la 0.81).
- MEDIDO sin panel y sin abrir ninguna ventana:
  - los 64 cuadros de antes: 28 limpios sin franja, 30 con barra detectados, 6 «no dividido»;
  - secuencias (0,1 s por cuadro): vista / no vista / vista → se pone; puesta y luego solo la línea
    de progreso fina (1 fila fuerte) o solo botones de trazo fino (2 filas) → se mantiene; después
    cuadros limpios → se quita a los 0,6 s; la línea fina sola desde el principio → no se pone;
  - por Vulkan, en el compositor aparte: la trama con barra es idéntica (0,00 %) a la de la 0.81; con
    un cuadro limpio no sale franja y la trama es idéntica a la de sin detector; con la fuente quieta
    (ningún cuadro nuevo) la franja sale igual (97 análisis en 20 s, 98 % del tiempo con franja);
    convertido → dividido → convertido: la franja se pone y se quita con el cambio; `ctl barra
    guardar 5` dejó 5 archivos de 805 KB en la carpeta de estado de la prueba.
- **No probado**: nada en el panel. Variantes que NO se detectan con la franja apagada: la línea de
  progreso de 5 px sola, la barra al 6 % o menos con línea fina, y botones de trazo fino con la
  barra al 1 % (al empezar un vídeo la franja puede tardar en salir). Son barras imitadas: para
  afinar hacen falta cuadros del reproductor de verdad, y para eso está `ctl barra guardar`.

## En el vídeo dividido, la barra del reproductor de una pieza (v0.81)

- Rodolfo, con la 0.80: «¿hay forma de que el sbs tenga una barra de progreso consistente? La función
  se parte como el vídeo en dos».
- `src/barra.h`, `src/barra.cpp` (nuevos): el detector. Sobre la copia chica del cuadro (518 × 518, la
  del conversor), por fila, la diferencia de luminancia entre la mitad izquierda y la derecha después
  de casarlas por trozos (un octavo de la mitad cada uno, corrimiento de hasta el 3 %) y suavizadas con
  una caja de 11 puntos. Umbral: max(12, 3 × la mediana de las filas del centro + 3). Zonas: el 26 %
  de abajo y el 16 % de arriba. Hacen falta 4 filas, dos análisis seguidos para poner la banda y
  0,45 s sin verla para quitarla; mientras dura se queda con la mayor extensión. Margen de 3 % sobre
  la primera fila de abajo (la barra de progreso va justo encima de los botones).
- `shaders/weave.frag`, `src/vkweave.*`: en lado a lado, las filas de arriba y de abajo que diga el
  visor (`tun9.w`, `tun3.x`) se pintan con el cuadro entero para los dos ojos, sin partir ni correr.
- `src/vkview.cpp`: cada cuatro cuadros nuevos, con el vídeo dividido puesto, baja la copia chica y
  pasa el detector; lo dice en el registro (`[barra] …`). `src/main.cpp`: `barra_2d = no` en
  serve.conf lo apaga; orden nueva `odysseyvk barra-detect`.
- Dos criterios se descartaron MIDIENDO, con 22 escenas reales pasadas a lado a lado con su mapa de
  profundidad: (1) un solo corrimiento por fila y umbral 9 daba una falsa alarma (un primer plano con
  subtítulos: 18 filas a 19 niveles); (2) casar por trozos con caja de 5 seguía dándola (14 niveles):
  la copia chica toma un punto de cada siete sin promediar y el detalle fino sale distinto en cada
  mitad. Con la caja de 11: 0 falsas alarmas.
- MEDIDO sin panel, 64 cuadros de 3840x2160 (barra de reproductor dibujada por mí, con botones más
  chicos que los de YouTube):
  - 22 escenas limpias + 6 con franjas negras: ninguna banda (residuo máximo abajo 11, arriba 5);
  - 6 cuadros que no son vídeo dividido: «no dividido» (centro 93..115), ninguna banda;
  - 30 con barra (progreso a 0, 31, 55 y 97 %; con y sin título arriba; con franjas): las 30
    detectadas, residuo 26..32 abajo y 58..84 arriba; banda de 6,7..8,4 % abajo y 5,7 % arriba. La
    barra de progreso queda a 5,5 % del borde: dentro en todos los casos;
  - secuencia a 0,1 s por cuadro: la banda sale al segundo cuadro con barra y se quita 0,5 s después
    del último; cada análisis tarda 0,43 ms.
- MEDIDO por Vulkan fuera del panel (`ODYSSEYVK_FUENTE_PNG` con un cuadro con barra, `--sbs`,
  volcado en el cuadro 80, con y sin `ODYSSEYVK_BARRA_2D=0`): cambian las filas 0..121 (5,6 %) y
  1978..2159 (8,4 %); el centro, 0,0000 % de píxeles distintos; la banda de abajo queda idéntica al
  cuadro original (0,0 niveles de diferencia; sin el detector, 45,2). Con un cuadro limpio no sale
  ninguna banda.
- **No probado**: nada en el panel ni con el reproductor de YouTube de verdad (su barra la imité; no
  tengo un cuadro real suyo en lado a lado). El coste de bajar la copia chica dentro de una sesión.
- Error mío durante la prueba: lancé tres ventanas de prueba con una sesión 3D de Rodolfo abierta
  (comprobé que había sesión y encadené la orden igual). El registro del servicio muestra el vídeo
  perdiendo el foco tres veces y el visor devuelto una vez.

## El botón de vídeo dividido pasa de verdad a lado a lado (v0.80)

- Rodolfo, con la 0.79: «los botones están perfectos, pero el derecho no cambia al modo doble».
- Eran dos fallos, los dos míos o destapados por la 0.79:
  1. **La trama no se enteraba de que el conversor se apagaba.** `Weave::setConverter` solo se llamaba
     con el conversor encendido; al apagarlo (el botón nuevo, o la tecla `C` de siempre) la trama se
     quedaba con el último mapa de profundidad puesto. El registro decía «apagado» y en pantalla
     seguía el vídeo normal con relieve. En la 0.79 solo miré el registro, no la imagen.
  2. **El vídeo dividido arrancaba convertido** (regresión de la 0.79). El servicio empezó a pasar
     `--modelo` / `--alterno` también con el vídeo dividido, y esas dos banderas encienden el
     conversor por sí solas. Elegir «Vídeo dividido» en la tarjeta daba vídeo normal.
- `src/vkview.cpp`: cada cuadro, si el conversor está apagado y la trama lo tiene puesto, se le quita.
- `src/main.cpp`: bandera nueva `--conversor-en-espera` (la red viaja, el conversor arranca apagado);
  el servicio la pasa con `sbs` y `tb`.
- Taller: `ODYSSEYVK_VOLCADO_CUADRO=N` hace el volcado en el cuadro N (antes siempre el 4), y
  `ODYSSEY_GOLPE` acepta `normal` y `dividido`, para sacar la trama después de un cambio en marcha.
- MEDIDO fuera del panel (`odysseyvk test --windowed --no-panel --no-lens --no-scaler --sin-interfaz`,
  trama de 3840x2160 leída de Vulkan, comparada píxel a píxel con la de un arranque `--sbs`):
  - antes del arreglo, conversor ya con mapa y clic en dividido: 17,98 % de píxeles distintos de la
    de lado a lado (el conversor sin tocar: 18,52 %). Si el clic llegaba antes del primer mapa salía
    0,00 %: por eso la prueba de la 0.79 no lo vio;
  - después: 0,00 % (idéntica a lado a lado);
  - dividido y vuelta a normal: 0,00 % contra un arranque con conversor;
  - arranque como lo hace el servicio con el vídeo dividido (`--sbs --conversor-en-espera --cada 3
    --red 0 --modelo … --alterno …`): el conversor no arranca, 0,00 % contra lado a lado;
  - ese mismo arranque y clic en normal: abre `mde_large` (la de serve.conf) y queda a 0,39 % de
    píxeles (0,026 de media sobre 255) de un arranque con conversor.
- **No probado**: nada en el panel ni con un vídeo de verdad; el servicio entero lanzando un vídeo
  dividido (se probó la línea de banderas que arma, escrita a mano).

## Pastilla morada y botones de tipo de 3D (v0.79)

- Rodolfo, con la 0.78 en el panel: «me gusta», y pidió que la pastilla tenga la tonalidad de la
  tarjeta («ese morado») y, sobre el «no volver a preguntar», un botón sin texto para los dos 3D,
  solo los iconos.
- `src/vkui.cpp`: el acento de la pastilla pasa del verde al morado del paisaje (0,46 0,37 0,92) y el
  fondo al gris de la tarjeta. Bloque de la derecha: arriba dos botones de icono (el paisaje entero =
  vídeo normal, las dos mitades = vídeo dividido; el puesto encendido y con filo, el otro apagado),
  debajo la casilla. La pastilla sigue en 651 × 76 con la casilla y mide 616 × 76 sin ella.
- `src/vkview.cpp`: los botones cambian de tipo en marcha por el camino de la tecla `C` (encender el
  conversor, o apagarlo y dejar lado a lado con los deslizadores en 50 / 0). La casilla guarda el tipo
  que esté puesto, y si la aplicación ya estaba guardada se reapunta con el nuevo.
- `src/main.cpp`: el servicio pasa la red (`--modelo`, `--alterno`, `--cada`, `--red`) también cuando
  lanza un vídeo dividido; si no, al pasar a vídeo normal desde la pastilla se abría la otra red.
  DERIVADO del código: la red solo se carga al encender el conversor (`if (convActivo)`).
- MEDIDO sin panel: `ctl hud` a 3840x2160, ningún rótulo se sale; botón normal 2 418 celdas de ratón,
  dividido 2 418, casilla 4 752. Imágenes revisadas (normal puesto y casilla marcada; dividido puesto
  con el ratón encima del otro; sin casilla).
- MEDIDO por Vulkan fuera del panel (`ODYSSEY_GOLPE="dividido;normal;dividido;dividido;normal"
  odysseyvk test --windowed --no-panel --no-lens --no-scaler --conver …`): conversor apagado,
  encendido, apagado, nada (ya estaba), encendido; sin errores.
- **No probado**: nada en el panel; los botones con el ratón sobre un vídeo de verdad; el reapunte de
  la aplicación guardada al cambiar de tipo (no se mandó para no escribir en el `serve.conf` real).

## Tarjeta como la oficial y pastilla de una fila (v0.78)

- Rodolfo, con la 0.77 («vuelve perfecto y el 3d bien»), pidió mejoras de interfaz con la foto de la
  tarjeta oficial de Samsung: que la de la orilla se parezca, que el «no volver a preguntar» se pueda
  desactivar dentro del vídeo, quitar de la pastilla lo que sobra (el modelo, si uno es mejor), dejar
  lo demás compacto, y fuera el texto de encima de la pastilla y el cuadro verde de la esquina.
- **Tarjeta de la orilla** (`src/vkui.cpp`): oscura, de esquinas redondas, con la pregunta «¿Qué tipo
  de vídeo estás viendo?», dos teselas ilustradas (Vídeo normal = conversor, Vídeo dividido = lado a
  lado), la casilla de guardar, un `?` que explica al pasar el ratón y el aspa. Los dibujos (paisaje,
  aspa, palomita, anillo) son máscaras propias horneadas en el atlas, no imágenes de Samsung. La tesela
  «2D plano» salió: cerrar hace lo mismo.
- **Pastilla** (`src/vkui.cpp`): una fila — interruptor, Profundidad, Salida, la casilla y un renglón de
  teclas. De 824 × 134 a 462 × 76 (651 con la casilla). Salieron el botón de la red, la fila
  Facetracking, los avisos de teclas sueltos y el letrero del hardware. La red: siempre la de Samsung
  (`mde_large`), que ya era la de arranque; `1` / `2` la siguen cambiando.
- **«No volver a preguntar» dentro del vídeo**: el servicio pasa `--recordar app:modo` (y `--recordado`
  si ya estaba apuntada); la casilla de la pastilla deja un recado en `~/.local/state/odysseyvk/recordar`
  y el servicio lo aplica a su memoria y a `serve.conf` (`ajuste::fijarAutoEnServeConf`, que quita solo
  el par de esa aplicación y deja copia en `serve.conf.auterior`).
- **Cuadro verde**: quitado de `shaders/weave.frag`. **Texto de encima de la pastilla**: quitado. Los
  fallos (lente, scaler, cámara) y la respuesta a la casilla salen en un aviso bajo la pastilla.
- De paso, tres defectos del dibujo que se vieron al sacar la interfaz a un archivo:
  - el aviso de estado se colocaba con el alto REAL de la ventana en un lienzo virtual: a escala 2
    (el panel) caía fuera de la pantalla. DERIVADO del código: y = (2160 − 34) × 2;
  - los rellenos lisos se muestreaban a caballo con el borde del atlas y salían degradados (75 % → 100 %);
  - el pintado de CPU de la tarjeta pintaba también los píxeles que el cuadro solo rozaba y traía la
    máscara vecina (una raya junto al interruptor, un marco tenue en cada letra). Ahora sigue la regla
    de la GPU. Y el espacio mide lo que dice la fuente (8 px de 26) y no medio cuadratín (13).
- MEDIDO sin panel (`ODYSSEYVK_HUD_PNG=… odysseyvk ctl hud`, a 1080, 1440 y 2160): los 15 rótulos caben
  en su caja; golpes de ratón: tesela normal y dividida 15 080 celdas cada una, casilla de la tarjeta
  5 500, aspa 441, `?` 420, casilla de la pastilla 2 806, botón de red 0. Imágenes revisadas a ojo
  (tarjeta en reposo, con una tesela bajo el ratón, con el `?`, casilla marcada; pastilla con y sin
  casilla, modo de legado).
- MEDIDO en una carpeta de prueba (HOME aparte): con `auto = mpv:sbs, zen:conv` y `auto = firefox:conv`,
  quitar zen deja `auto = mpv:sbs`; cambiar firefox a sbs mueve su línea al final; quitar mpv borra la
  línea vacía; quitar una aplicación que no estaba no toca el fichero; los comentarios se conservan.
- MEDIDO por Vulkan, fuera del panel (`odysseyvk test --windowed --no-panel --no-lens --no-scaler
  --recordar zen:conv --recordado --dump …`, cuadro de 3840x2160 leído de la GPU): la pastilla sale
  igual que en el pintado de CPU, con la casilla marcada; el aviso de debajo dice «la lente 3D no
  respondió · el scaler del monitor no respondió» (cierto en esa prueba); la esquina de arriba a la
  izquierda vale (13, 13, 15) en 10,10, 30,30 y 60,60: ya no hay cuadro.
- **No probado**: nada en el panel. El servicio con la casilla no se corrió entero (había un
  vigilante vivo y no se levantó otro al lado): se probaron sus dos mitades, el recado y la
  reescritura de `serve.conf`, por separado. La tarjeta nueva no se puso sobre el escritorio.

## ESC saca siempre del vídeo y del 3D a la vez (v0.77)

- Rodolfo, con la 0.76: Esc sacaba del vídeo solo si antes había hecho clic en él; si no, el teclado lo
  tenía el visor y Esc cerraba el 3D dejando el vídeo a pantalla completa. Pidió que saque de los dos.
- `src/vkview.cpp`: Esc en el visor le quita antes la pantalla completa a la ventana capturada.
- `src/main.cpp`: sin cambio de escala, perder la pantalla completa en los primeros 5 s es el usuario
  saliendo; ya no sale el aviso de «el 3D no llegó a empezar» (medido a las 14:34:31, salió sin venir a
  cuento).
- MEDIDO en el registro de la 0.76: las sesiones arrancan con «escala 1.5 (la del escritorio: no se
  cambia nada)». Rodolfo no dijo cómo se ve la imagen 3D así.
- Comprobado: compila. No probado: en el panel.

## El 3D ya no cambia la escala del monitor (v0.76)

- Rodolfo, con la 0.75: entrar, la pausa y los atajos bien; salir del vídeo con Esc, no («el punto 4 no
  fue así»).
- MEDIDO en el registro de la 0.75 (13:54–13:55): la pausa se respetó (dos pausas, ninguna «se reanuda
  solo») y los atajos globales llegaron con el teclado en el vídeo (3D/2D, pastilla, relieve −4 y +4).
  La primera sesión cayó en «el escritorio sacó al vídeo de pantalla completa» y se cerró sola; la
  segunda, ocho segundos después, ya NO arrancó sacando al vídeo: lo de dejar la ventana a pantalla
  completa antes de devolver el monitor funciona, y confirma que es KWin quien repone el estado.
- MEDIDO: al salir (13:55:46) la ventana pasó por «maximizada → pantalla completa (puesta por nosotros)
  → maximizada (quitada por nosotros)» en menos de un segundo, y para KWin quedó bien. HIPÓTESIS de lo
  que vio Rodolfo: Zen no sale limpio de una pantalla completa que le ponen y le quitan desde fuera tan
  seguido. En la 0.74 pasaba lo mismo con la que reponía KWin.
- La causa de fondo es que el 3D cambia la configuración de pantallas (escala 1,5 → 1 y vuelta). MEDIDO
  que no hace falta: con el escritorio a 1,5 el visor a pantalla completa da «lienzo 3840x2160 físicos,
  ventana lógica 2560x1440», y `ctl flujo` sobre una ventana de 2014x868 lógicos entrega 3022x1303, o
  sea que la del vídeo a pantalla completa llega a 3840x2160.

### Lo que cambia

- `src/session.cpp`: el panel se queda con la escala del escritorio. Si además ya está en el modo y la
  rotación que hacen falta, no se manda ninguna orden de geometría (ni al entrar ni al salir); solo HDR
  y sincronización adaptativa, que no cambian el tamaño del escritorio. El otro monitor ya no se mueve.
- `escala_3d = 1` en serve.conf (ODYSSEYVK_ESCALA_3D=1) vuelve a pasar el panel a escala 1, con todo lo
  de la v0.75 para el vaivén de KWin, que queda como estaba para ese caso.
- El aviso «la trama necesita pixel a pixel» ya solo sale si el lienzo no mide lo que el modo.

### Comprobado / no probado

- Comprobado fuera de una sesión (visor a pantalla completa en DP-4, sin lente ni scaler, 4 s): las
  salidas quedan igual antes, durante y después (DP-4 2560x1440 a escala 1,5; DP-7 en 2560,624); las
  ventanas de Zen y Firefox no cambian de sitio, tamaño ni estado; 116,5 fps; al salir, «nada que
  devolver».
- No probado: que KWin presente el lienzo punto a punto con la escala 1,5 (una captura de pantalla no
  sirve: sale reescalada a 2). Si no fuera así, la imagen 3D se vería con bandas o sin relieve, y para
  eso está `escala_3d = 1`. Tampoco probado: la sesión real (entrar, salir con Esc) ni un juego, que con
  el escritorio a 1,5 puede quedarse a la resolución que tuviera en vez de pasar a 4K.

## Atajos del hub globales, la pausa se respeta y la salida del vídeo no deja la ventana atrapada (v0.75)

- Rodolfo, tras ver vídeos en el navegador (Zen) y jugar a Stray: (1) «al salir del vídeo otra vez se
  quedaba la ventana activa en el modo 3D»; (2) «cada que ponía pausa al vídeo se despausaba»; (3) «los
  atajos de teclado del hub no funcionaban, ni en vídeo ni en el juego».

### La pausa

- MEDIDO en el registro: 19 veces entre las 10:47 y las 11:29 sale «el navegador había parado el vídeo
  (Paused): se reanuda solo», entre 0 y 3 s después de cada «dice Paused». El visor reanudaba el
  reproductor cada vez que lo veía parado, hasta seis veces por sesión; estaba pensado para el arranque,
  cuando el cambio de modo deja el vídeo en pausa, pero valía toda la sesión.
- MEDIDO: en la sesión del juego (Stray, 10:52:58) el visor cogió el reproductor del navegador, que
  estaba en pausa detrás, y lo reanudó.
- Cambia (`src/vkview.cpp`): la reanudación sola vale los primeros 8 s de la sesión y 8 s al volver de
  `Tab`; y el reproductor solo se toca si la ventana capturada es de un navegador o de un reproductor
  (si no, «no se toca ningún reproductor»), con la aplicación de la ventana como pista.

### Los atajos

- Causa (leída en el código): `Ctrl+Mayús+1…8` y `F1` solo se atendían en la retrollamada de teclas del
  visor, o sea con el visor enfocado. En cuanto se hace clic en el vídeo, o con un juego delante, el
  teclado es de esa ventana y al visor no le llega nada.
- Cambia: `src/atajos.cpp` los registra como atajos globales de KDE (`org.kde.kglobalaccel`, que sirve
  KWin) al abrir el visor y los deja inactivos al cerrar; el vigilante los desactiva también al morir el
  visor. Cada uno va con tres formas, porque para KWin `Mayús+1` es el símbolo de la distribución («!» en
  la española, «·» con el 3…): se le pregunta a xkbcommon. `atajos_globales = no` los quita.
- Comprobado sin teclado (`ctl atajos probar`, que pide al escritorio que dispare cada acción): 9 de 9
  llegan; con el visor abierto fuera del panel, `relieve+`, `prof-`, `3d`, `capa` y `pastilla+` hacen lo
  mismo que la tecla; al cerrar, el componente queda inactivo (`isActive = false`).
- No probado: con el teclado de verdad, en el panel, ni con un juego.

### La salida del vídeo

- MEDIDO (11:25:32): en el mismo segundo en que el visor devuelve el monitor a su escala, la ventana del
  vídeo pasa sola de «maximizada» a «pantalla completa». Nadie nuestro lo pide: KWin guarda cómo estaba
  cada ventana en cada configuración de pantallas y se lo repone al volver a ella. Y por lo mismo las tres
  sesiones siguientes EMPEZARON con «el cambio de modo dejó al vídeo sin pantalla completa» (11:14:49,
  11:17:28, 11:29:32): la última vez que el monitor estuvo en la configuración de 3D, la ventana ya no
  estaba a pantalla completa, porque el usuario acababa de salir. Antes de las 10:48 no pasaba (cinco
  sesiones sin reposición); no sé qué lo dispara.
- DERIVADO (del código de Firefox, no medido): cuando el gestor de ventanas le quita la pantalla completa,
  el navegador suelta el vídeo; y si se la volvemos a poner nosotros, queda en SU pantalla completa (la de
  F11), con la trama tejida sobre la página. Hay que volver a pulsar el botón del vídeo, y luego ESC solo
  saca del vídeo, no de F11: la ventana sigue a pantalla completa y la sesión 3D no se entera.
- Cambia (`src/main.cpp`, el vigilante):
  - al cerrar porque el vídeo salió de pantalla completa, antes de devolver el monitor la ventana vuelve
    a quedar a pantalla completa un instante, para que KWin la recuerde así en la configuración de 3D; se
    le quita en cuanto el monitor vuelve a su tamaño lógico (sin esperar a que muera el visor). Solo si
    el 3D cambia de verdad ese tamaño (con el escritorio a escala 1 no hay nada que recordar);
  - si aun así KWin saca al vídeo de pantalla completa al empezar y la ventana es de un navegador, no se
    le repone: se cierra la sesión, sale un aviso («vuelve a ponerlo a pantalla completa») y el segundo
    intento ya entra. Si en tres minutos se repite con la misma ventana, se repone como antes.
- No probado: nada de esto en el panel. La primera sesión tras instalar puede caer en el segundo caso,
  porque KWin todavía recuerda la ventana como la dejó la última sesión.
- Contrapartida conocida: al salir del vídeo el navegador se ve a pantalla completa una fracción de
  segundo antes de volver a su ventana. La solución de fondo es no cambiar la escala del monitor al
  entrar en 3D (el visor a la escala del escritorio), que quita todo esto de raíz; no está hecha.

## Nota del 25-nov, sin versión nueva: el paso retocado con el rastreo fino (`serve.conf`)

- Rodolfo, con la 0.74 y el patrón de los nueve cuadros: los de la izquierda del naranja se mueven bien
  de un ojo al otro, menos los dos primeros con el ojo derecho; los tres últimos dejan un doble más
  marcado en los dos ojos. El centro bien, lo peor a la derecha.
- DERIVADO con el modelo (`odysseyvk attrs`, ojo en x −4,6 cm): un centímetro más de distancia mueve la
  fase −0,024 ciclos en el borde izquierdo y +0,035 en el derecho (+1,59·10⁻⁵ ciclos por píxel, con el
  cero en el píxel 1475). El rastreo fino mide ~1,2 cm más lejos que la media del de antes: −0,028 y
  +0,042, o sea lo peor a la derecha, que es lo que se ve. El paso de la v0.59 se había afinado con el
  rastreo de antes.
- Probado en vivo con `ctl ajuste rampa_x … fase …`, de 0,00002 en 0,00002 y girando en el píxel 1475
  (y en el tercer paso, en el píxel del ojo): −0,00122 con fase 0,0295 «mejoró»; −0,00124 con 0,059
  «mejor»; −0,00126 con 0,0938 «peor». Queda `rampa_x = -0,00124` y `fase = 0,059` en `serve.conf`
  (respaldo `serve.conf.respaldo-2026-11-25-antes-paso-v074`).
- Comprobado después de guardar: con el vigilante reiniciado y el patrón arrancado ya de `serve.conf`
  (`--align 0.059000` y `rampa_x = -0.001240` en el registro), Rodolfo lo ve «igual de bien» que el
  mejor de los tres pasos.
- No probado: la fase sola, y el patrón de un ojo (la tecla `b` no llegó al visor). La corrección
  exacta sería un desfase de z del rastreo, que gira con la cabeza; con rampa y fase fijas el error al
  mover la cabeza ±5 cm es de 0,006 ciclos.

## Nota del 25-nov, sin versión nueva: qué trae la calibración de fábrica de este panel y qué usa el visor

- Rodolfo preguntó si las pruebas «en blanco» que hizo QWEN en Windows habían servido: en las dos salía
  la imagen blanca y nunca se vio la línea negra.
- MEDIDO: las 22 capturas de `AnalisisOdyssey/medidas-ronda-2/blanco/` (`c00-ON-*` y `c00-OFF-*`) son
  blanco liso, todos los puntos a 255 y sin portadora. Con las dos vistas en blanco la trama no deja
  ninguna línea, así que de ahí no sale el paso del programa oficial. No se usaron para calibrar: el
  paso se midió contra la lente con el patrón de un ojo (v0.59).
- MEDIDO: `calib/3DStackCorrection_A.png` y `_B.png` son, byte a byte, los de
  `C:\ProgramData\Simulated Reality\Devices\VN022514ET0194\` (SHA-256 `5e14a614…` y `7b5074e4…`).
  Abierto tal cual, el A se ve negro (0 a 55) y el B gris liso (117 a 136): son datos, no fotos.
- MEDIDO sin panel (`ODYSSEYVK_PRUEBA=9` con `--dump`): el sombreador lee el A con media 22,90 y el B con
  127,17, sin voltear, a 0,07 y 0,08 niveles del PNG. El patrón de un ojo (`b`) deja 45,7 % de
  subpíxeles negros y 45,7 % blancos, y sus dos posiciones son complementarias (suman 255 ± 3,2).
- MEDIDO: `calib/SR+CAL3D.bin`, lo que contesta el monitor a `SR+CAL3D`, son 4 bytes de longitud y un 7z
  de 37 295 bytes (LZMA2 + AES) con tres archivos del 2-abr-2025: `temp/3dstack.eini` (252 bytes),
  `temp/3DStackCorrection_A.png` y `temp/3DStackCorrection_B.png`. El contenido va cifrado, los
  nombres no.
- Lo que NO usa el visor: `3dstack.eini`, donde tiene que ir lo propio de la lente de esta unidad. Sigue
  cifrado (QWEN probó 59 823 claves) y no se intenta abrir. Por eso `kx`, `ky` y la inclinación son los
  capturados de otro G90XF y lo de esta unidad se corrige a ojo con `rampa_x` y `rampa_y`. El README
  decía que esas constantes eran idénticas en todas las unidades: corregido.
- HIPÓTESIS de por qué en la prueba de un ojo de la v0.59 nunca se vio negro: el rastreo medía la
  distancia con ±4 cm de error (medido en la v0.74), ±0,15 ciclos de fase en los bordes, y al ojo que
  debía ver negro le llegaba el subpíxel rojo o el azul. No probado: repetir esa prueba con la 0.74,
  que además dejaría afinar `rampa_x` y `rampa_y` con el rastreo ya quieto.

## El rastreo mide la distancia comparando la imagen de los ojos, y los rótulos tienen memoria (v0.74)

- Rodolfo, con la 0.73: «ahora volvió otra vez el lado a lado». El visor de la 0.73 es el de la 0.72 (solo
  cambió el vigilante), así que la causa no es un cambio nuestro: es algo que varía de una sesión a otra.
- MEDIDO en el registro de las 20 sesiones del día: la distancia entre los ojos que saca el rastreo, que es
  fija, tiene de 7 a 25 mm de desviación dentro de cada sesión (del 18 al 86 % de las muestras a más de
  ±8 % de 64,8 mm), y la z YA FILTRADA cambia de 0,85 a 2,2 cm de un segundo al siguiente (mediana de
  cada sesión desde las 08:34).
- MEDIDO con la cámara (dos tomas grabadas con `ctl track --grabar`, 1186 y 1486 cuadros): YuNet da cada
  ojo por separado en cada vista con ±2,7 a ±3,4 px de temblor y, peor, con un sesgo lento que ningún
  filtro quita. Con la cabeza quieta, la disparidad de YuNet (mediana de cada segundo) va de 49,3 a
  56,4 px mientras la correlación de imagen entre las dos vistas se queda entre 52,4 y 53,1: son ±4 cm
  de distancia.
  La z filtrada por segundo, sobre los mismos cuadros: **de 62,6 a 72,3 cm** antes, de 66,4 a 68,4 ahora.
- DERIVADO: 4 cm de z son 5,239·30/65² · 4 = **0,15 ciclos de fase en el borde de la pantalla**, contra
  un margen de ±0,04. La trama iba con el paso mal medido la mayor parte del tiempo, y cambiando.
- MEDIDO en el registro, aparte: con el video de Nueva York el apoyo de los rótulos marcaba del 10 al 36 %
  del cuadro como texto (no tiene subtítulos) y el porcentaje cambiaba más de 3 puntos entre 0,6 y 1,4
  veces por segundo; con los videos de antes, del 1 al 5 %.
- HIPÓTESIS (las dos medidas son firmes; que sean el vaivén que ve Rodolfo no está probado en el panel):
  el vaivén es la suma de la z del rastreo, que mueve la fase en los bordes, y de zonas que el apoyo de
  rótulos pega a la pantalla y suelta varias veces por segundo.

### Lo que cambia

- **Rastreo fino** (`src/track.cpp`, `afinar`). YuNet solo dice dónde está la cara:
  - el punto medio de los ojos se sigue de cuadro a cuadro con una plantilla de la franja de los ojos
    (correlación normalizada, con decimales de píxel), anclada despacio (10 s) a lo que dice YuNet;
  - la distancia sale de buscar esa franja (solo lo que hay entre los dos ojos) en la otra vista;
  - una pareja con más de 2 px de error de reproyección se tira (las buenas dan 0,8 a 1,2; las falsas,
    de 2,5 para arriba), y un cuadro suelto sin medida aguanta la pose un cuarto de segundo antes de
    caer a la vía de antes.
  MEDIDO sobre los mismos 1186 cuadros (antes → ahora): ruido por cuadro con la cabeza quieta, x 0,081 →
  0,008 cm, y 0,221 → 0,019, z 1,124 → 0,095; z cruda contra su mediana de 9 cuadros, p95 62,8 → 2,4 mm;
  recorrido de la z filtrada en 4 s quieto, 2,16 → 0,68 cm (fase en el borde 0,079 → 0,024 ciclos);
  distancia entre ojos, de 61–137 mm a 56–71. En la segunda toma (cara de lado y en el borde del cuadro)
  la vía de antes daba z de 71 a 125 cm con tirones de 50; ahora, de 77,0 a 80,8. Cuesta 0,1 ms más por
  cuadro.
- **El filtro de la pose abre a 2 Hz** cuando la medida viene del rastreo fino (antes 0,6 Hz; los cuadros
  que caen a la vía de antes siguen a 0,6). MEDIDO: quieto, la x filtrada recorre 0,38 cm en vez de 0,29
  (lo demás es la cabeza, que no está clavada); en marcha, el retraso baja de 0,72 a 0,35 cm rms.
- **Apoyo de los rótulos con memoria** (`src/depth.cpp`, `MemoriaTexto`). Una celda solo cuenta como
  rótulo si sus trazos están donde estaban en el mapa anterior (un subtítulo no se mueve; el texto y la
  textura de la escena sí, en cuanto se mueve la cámara), entra cuando lleva dos mapas así y sale cuando
  lleva dos sin verse; en la banda de abajo entra al primero.
  MEDIDO a 6 mapas por segundo: papel impreso grabado a pulso, 2,40 → 0,85 % de la rejilla cambiando por
  mapa (fuera de la banda de abajo, 1,95 → 0,27); el mismo papel corriéndose 1 punto del mapa por mapa,
  de 3,0 % apoyado y 1,65 % cambiando a **0 y 0**; un subtítulo de prueba sobre video en movimiento se
  apoya un mapa después que antes (0,17 s) y se suelta un mapa después.
- Para medir sin sentarse delante: `ctl track --grabar DIR`, `--repetir DIR`, `--csv`, `--fino no`,
  `--euro MIN;BETA`; `texto-detect --secuencia A.ppm B.ppm …`; `tools/mide_rastreo.py`.
- Para apagarlo sin reinstalar: `rastreo_fino = no` en serve.conf; `ODYSSEYVK_TEXTO_MEMORIA=0`.
- El registro del visor dice cuántos cuadros salieron del rastreo fino y con qué parecido.

### Comprobado / no probado

- Comprobado: compila; las dos tomas grabadas repetidas por la vía de antes y por la fina; el rastreo en
  vivo con `ctl track` (4 a 5 ms por cuadro); el visor fuera del panel con el conversor (119,5 fps, ningún
  cuadro lento); las secuencias del apoyo de rótulos.
- **Comprobado en el panel** (Rodolfo, 25-nov, sesiones de las 10:01 y 10:02 con dos videos): el vaivén
  de lado a lado «sí, se fue». MEDIDO en esas dos sesiones, ya de frente: la z filtrada cambia 0,15 y
  0,24 cm de un segundo al siguiente (1,41 en la sesión de las 09:54 con la 0.73), la distancia entre
  ojos tiene 1,2 y 4,7 mm de desviación (19,4), el rastreo fino llevó cerca del 98 % de los cuadros y el
  visor dio 120,0 y 119,3 fps.
- No probado: cada cambio por separado. No se sabe si el vaivén lo quitó el rastreo, la memoria de los
  rótulos o los dos. No tengo cuadros del video de Nueva York: lo de los rótulos está medido con otros
  videos. En las dos tomas grabadas Rodolfo miraba al otro monitor, con la cabeza girada.
- Queda a la vista: con «El PEOR Error en la Historia de Hollywood» el apoyo de rótulos siguió cambiando
  mucho (44 cambios de más de 3 puntos en 26 s, 11,9 % apoyado de mediana); con el segundo video, 26 en
  105 s (6,6 %). Rodolfo: «en el primer video sí, hay subtítulos». O sea que ahí el apoyo estaba
  haciendo su trabajo y los cambios son los de los propios subtítulos; no hay falsos positivos que
  perseguir con ese video.
- A tener en cuenta: la z nueva sale en promedio ~1 cm más lejos que la de antes (que además iba y venía
  ±4 cm). Si quedara imagen doble fija en los bordes, se retoca `rampa_x` con el patrón de un ojo.
- Las dos tomas de la cámara eran fotos de la cara de Rodolfo: estaban en la carpeta temporal de la
  sesión y se borraron al terminar.

## Al salir del video, el navegador ya no se queda a pantalla completa (v0.73)

- Rodolfo, con la 0.72: el vaivén de lado a lado «sí bajó mucho» (la zona muerta del paralaje, probada
  en el panel). Y: «en mi última ejecución, al salir del video se quedó el navegador en fullscreen».
- MEDIDO en el registro de esa sesión (09:04:08): a los 11 s la ventana de Firefox pasó de pantalla
  completa a 1709x1048 (la salida del video) y el vigilante contestó «el cambio de modo dejó al video sin
  pantalla completa: se la devolvemos (1/3)». Esa orden va a la VENTANA, no a la página: Firefox quedó a
  pantalla completa sin video, con la trama encima, hasta que Rodolfo salió otra vez a los 18 s.
- MEDIDO en las 20 sesiones del día: lo que nuestro cambio de modo le hace a la ventana del video termina
  a los 2 s de lanzar la sesión y **ninguna vez** le quitó la pantalla completa; la única reposición del
  día fue esa. La ventana de «sesión joven» era de 12 s.
- MEDIDO de paso: la vigilia que le quita la pantalla completa al navegador tras el cierre (v0.5x) solo
  caducaba al ver la ventana a pantalla completa, así que quedaba armada sin fin: seis veces saltó
  minutos después («sigue a pantalla completa después de 0 intento(s)») cuando Rodolfo volvió a poner un
  video. Con menos de 20 s le habría quitado la pantalla completa recién pedida.

### Lo que cambia (`src/main.cpp`, el vigilante)

- La pantalla completa del video solo se repone en los **primeros 5 s** de la sesión (antes 12). Después,
  perderla es salir del video: la sesión se cierra a los 0,4 s, como ya hacía con la sesión asentada.
- La vigilia de después del cierre dura **6 s** (antes 20) y caduca aunque la ventana esté normal; si el
  usuario pone pantalla completa pasado ese tiempo, es suya y no hay aviso.

### Comprobado / no probado

- Comprobado: compila. La causa está leída en el registro, no deducida.
- No probado: en el panel. Queda un hueco conocido: salir del video en los primeros 5 s todavía repone
  la pantalla completa (hasta 3 veces).

## El paralaje deja de mecerse con la cabeza quieta, y la sincronización adaptativa se queda como esté (v0.72)

- Rodolfo, con la 0.71: «el framerate y el HDR sirvieron, pero el movimiento de lado a lado también
  [volvió]; ¿será por el cambio de velocidad? No lo veía cuando estaba a 120». Y quiere dejar encendida la
  sincronización adaptativa: ¿tiene que adaptarse ese lado a lado a las subidas y bajadas de cuadros?
- MEDIDO en el registro: esa sesión (08:53:38) **no corrió a 165**: `# hz = 165` sigue comentado, el panel
  fue al modo 20 (3840x2160 a 119,88) y el visor dio 119,8. Lo único distinto de la sesión sin vaivén
  (08:44, v0.70) es `hdr_en_3d` (HDR encendido) … y que la cabeza medida se movió más: mediana de 0,31 cm
  entre muestras de un segundo contra 0,11. El rastreo no fue peor (error 1,39 px contra 1,13–2,25).
- MEDIDO: la sincronización adaptativa **nunca se apagó**. `Panel::apply` mandaba `vrr.never` y
  `vrr.disable`, que kscreen-doctor no conoce (la palabra es `vrrpolicy`, vista con `strings`); todas las
  sesiones 3D, también las buenas, corrieron con `Vrr: Always`. No es la causa de nada.
- HIPÓTESIS (por descarte, sin probar en el panel): lo único que convierte algo en movimiento lateral es
  el **paralaje** (v0.67), y respondía a la deriva de milímetros de la cabeza quieta. DERIVADO (la misma
  cuenta del visor, con una deriva de 0,25 cm a ~0,7 Hz más 0,05 cm de ruido): lo más cercano se mecía
  ±0,42 px y recorría **3,2 px por segundo**, diez veces más que los vaivenes del mapa a 6 por segundo
  (0,34 px/s).

### Lo que cambia

- **Zona muerta suave en el paralaje** (`src/vkview.cpp`): el desplazamiento d de la cabeza se multiplica
  por d² / (d² + 0,6² cm) (0,3 cm → 20 %, 3 cm → 96 %), y el paso bajo sube de 60 a 120 ms.
  DERIVADO: quieto, ±0,42 → **±0,12 px** y 3,2 → **0,58 px por segundo**; moviendo 3,25 cm en 0,3 s el
  pico pasa de 6,0 a 5,6 px y llega a los 0,42 s en vez de 0,33.
- **Sincronización adaptativa**: el visor ya no intenta apagarla (era lo que pasaba de hecho); el aviso
  «apágala» se cambia por una línea que dice cómo está. `vrr_en_3d = no` en `serve.conf`
  (`ODYSSEYVK_VRR_3D=0`) la apaga durante la sesión con la orden que sí existe (`vrrpolicy.never`), y el
  restaurado usa `vrrpolicy.always` / `.automatic`.
- Respuesta a la duda: nada del visor cuenta cuadros para moverse. El paralaje, el filtro del rastreo, el
  plano de apoyo y la escala van por tiempo real, y los mapas tienen su tope por segundo: con la
  frecuencia subiendo y bajando se comportan igual.

### No probado

- En el panel: si el vaivén se va. Si sigue, descartar por este orden en `serve.conf`, sin reinstalar:
  `paralaje = 0`, luego `hdr_en_3d = no`.
- `vrr_en_3d = no` (nadie lo ha pedido; la orden nueva no se ha ejecutado nunca).
- `hz = 165` sigue sin probar. Ojo: 165 no es múltiplo de 24, 25, 30 ni 60, así que un video se repartiría
  en refrescos desiguales (2-3-3…) y eso sí daría un vaivén de verdad en los travelings; 120 reparte parejo
  el de 24, 30 y 60.

## HDR en 3D como opción, frecuencia configurable y cuánto margen tiene el dibujo (v0.71)

- Rodolfo, con la 0.70: «mucho mejor, y HDR sí ayudó» (probó `kscreen-doctor output.DP-4.hdr.enable` en
  mitad de una sesión). Y dos dudas: si se puede pasar de 120 cuadros, y si conviene dibujar la imagen
  doble a otra resolución con un reescalado tipo FSR, o portar Python a Rust u OpenGL a Vulkan.

### Lo que se añade

- **`hdr_en_3d = si`** en `serve.conf` (`ODYSSEYVK_HDR_3D=1`): el visor ya no apaga el HDR al entrar en
  3D (`src/session.cpp`). Por defecto se sigue apagando; en el `serve.conf` de Rodolfo queda en `si`.
  Riesgo conocido, sin medir: con HDR el compositor mezcla los canales con una matriz de color, y en la
  trama los subpíxeles de un punto pueden ser de ojos distintos (más imagen fantasma en los cantos).
- **`hz`** en `serve.conf`: la frecuencia del panel durante el 3D (el vigilante pasa `--hz`). Sin la
  línea, 120. El panel anuncia 3840x2160 a 164,98; en el `serve.conf` de Rodolfo queda comentado
  (`# hz = 165`) para probarlo: no se sabe si la lente y el scaler aceptan 165 en modo 3D.
- Taller: `ODYSSEYVK_SIN_VSYNC=1` dibuja sin esperar al refresco, solo para medir.

### Cuánto margen tiene el dibujo (MEDIDO, RTX 4090, 3840x2160, conversor con `mde_large` a 6 mapas/s)

| | cuadros por segundo |
|---|---|
| con el freno del panel (lo normal) | 119,8 |
| sin freno, con todo (guiado, compresión, paralaje, realce) | **2.086 – 2.374** |
| sin freno, sin realce | 2.178 – 2.345 |
| sin freno, sin guiado ni realce | 5.458 – 5.516 |

Con todo puesto la tarjeta va al 95 % y 390 W a 2.100 cuadros: el coste de un cuadro es ~0,45 ms de los
8,3 que hay. **El límite es la frecuencia del panel, no el programa.**

### Las respuestas (para no repetir el análisis)

- **Pasar de 120**: solo subiendo la frecuencia del panel en 3D (`hz = 165`). Con video de 24-60 cuadros
  no hay más cuadros de contenido; lo que mejora es cada cuánto se recoloca la trama a los ojos
  (8,3 → 6,1 ms) y la suavidad del paralaje.
- **Otra resolución + FSR**: no aplica como en un juego. No se dibujan dos imágenes: el sombreador lee
  la fuente directamente en la coordenada desplazada de cada ojo, a la resolución del panel, y la trama
  tiene que escribirse subpíxel a subpíxel. Lo equivalente (calcular el desplazamiento guiado a media
  resolución en un pase aparte) ahorraría parte de esos 0,45 ms: no hace falta. DERIVADO: cada ojo ve
  la mitad de los subpíxeles, o sea ~2715 × 1527 efectivos; un video de 1440p ya llena lo que cada ojo
  puede resolver.
- **Python → Rust**: el visor no ejecuta Python (hay 1.095 líneas, todas en `tools/`, de medición). El
  tiempo se va en la tarjeta y en ONNX Runtime, que ya son C++/CUDA: reescribir en Rust no lo acorta.
- **OpenGL → Vulkan**: el camino principal (vigilante, captura, `test`) ya es Vulkan. OpenGL queda en
  `--gl` y en `odysseyvk play` (libmpv), que además **no tiene conversor** ni nada de v0.62–v0.70. Ese es
  el único porte con sentido: llevar `play` al visor Vulkan.

### No probado

- `hdr_en_3d` desde el arranque de una sesión (lo probado fue encender el HDR a mano a mitad de sesión).
- `hz = 165` en 3D.

## El temblor de la v0.69: tope de mapas por segundo (v0.70)

- Rodolfo, con la 0.69 instalada: «sí tiembla más, una sacudida leve de lado a lado, no pareja, pero se
  percibe». En esa sesión el visor iba ya a **119,8 cuadros por segundo** (antes 81-84): la fluidez se
  quedó, el temblor era lo que la v0.69 dejó «No probado».
- Causa (MEDIDO con video real, `tools/mide_temblor.py`, 150 cuadros de dos clips a 25 por segundo,
  `mde_large`, relieve 1,2 y salida 50): cada mapa trae algo de ruido de la red, y con más mapas por
  segundo ese ruido deja de ser una deriva lenta y se vuelve temblor.

  | clip | mapas por segundo | movimiento del mapa (px/s) | sobrante sobre el cambio neto | **vaivenes** (px/s) |
  |---|---|---|---|---|
  | nave (escena lenta) | ~6 (antes) | 0,84 | 0,65 | **0,34** |
  | nave | 25 | 2,49 | 2,28 | **0,86** |
  | globo (animación) | ~6 (antes) | 1,49 | 1,00 | **0,32** |
  | globo | 25 | 3,15 | 2,65 | **0,76** |

- Arreglo: tope de **6 mapas por segundo** (el ritmo que se aprobó en el panel hasta la v0.68, ahora
  elegido a propósito), en `src/vkview.cpp`. `mapas_por_segundo` en `serve.conf` /
  `ODYSSEYVK_MAPAS_SEG` (2..60) lo cambia. El dibujo no depende de esto y sigue a 120.
- Comprobado sin panel: con el tope por defecto, 30 estimaciones en 5 s (6,0 por segundo), 119,8 fps y
  0 de 600 cuadros lentos; con `ODYSSEYVK_MAPAS_SEG=20`, 100 en 5 s. A 6 por segundo los pesos «por
  tiempo» de la v0.69 valen lo mismo que los de antes, así que el mapa es el de la v0.68.
- No probado: en el panel. Deuda que queda: para tener a la vez mapas rápidos y sin temblor hay que
  suavizar el mapa crudo en el tiempo **antes** del procesado (ver la v0.69) o promediar varias pasadas
  de la red por mapa.

### El color «seco / gris» (pregunta de Rodolfo: ¿se dibuja una red virtual encima?)

- No hay red virtual: la trama solo decide, subpíxel a subpíxel, de qué ojo es cada uno. MEDIDO con el
  cuadro real y el visor de verdad (trama completa contra la fuente): media RGB 51,56 / 37,31 / 32,18
  contra 51,62 / 37,37 / 32,24; contraste 54,8 contra 55,3; saturación 22,2 contra 20,5; negro (p1) 1,0 en
  las dos. El programa manda al panel los mismos colores que entrega el navegador.
- Lo que sí cambia al entrar en 3D (MEDIDO en `kscreen-doctor -o` y en `src/panel.cpp`): el escritorio va
  con **HDR y gama ancha encendidos** (blanco SDR a 310 nits) y el visor **apaga el HDR** al entrar
  (`Panel::apply(..., disableHdr = true)`, restaurado al salir); la gama ancha no se toca. HIPÓTESIS: el
  tono distinto viene de ahí (y de la lente física encendida), no de la trama. Sin probar: dejar el HDR
  puesto, o quitar también la gama ancha, durante la sesión.

## El visor deja de pararse 75 ms en cada estimación: 120 cuadros por segundo con el conversor (v0.69)

- Pedido de Rodolfo: investigar por qué el visor va a ~83 cuadros por segundo y no a los 120 del panel.
- MEDIDO en el registro del vigilante: no es de hoy. Sesiones de conversor: 03:51 (v0.57) mediana 80,9;
  07:04 (v0.60) 75,9; 07:20 a 08:18 (v0.62 a v0.68) entre 78 y 85.
- Reproducido sin panel, sin captura y sin navegador: `ODYSSEYVK_FUENTE_VIVA=1` (taller nuevo: da la
  fuente por nueva en cada cuadro) con `test --convert` cae a **78-82 fps**, y `ODYSSEYVK_MEDIR=1` (ahora
  también en el visor) dice dónde: **`leer` 75 ms** por estimación (bajar la copia de 518 × 518 para la
  red) y `subir` 11 ms (mandar el mapa). Las dos en el hilo que dibuja.

### Las tres causas, por orden de peso

1. **El búfer de lectura estaba en memoria sin caché.** Se pedía «visible y coherente» y en NVIDIA el
   primer tipo que cumple eso (`memoryTypes[2]`, 0x6) es de escritura combinada; el de lectura es el
   siguiente (`[3]`, 0xe, con `HOST_CACHED`). Leer 1 MB punto a punto de ahí costaba 75 ms: nueve
   refrescos parados por estimación, y por eso además solo salían ~6-8 mapas por segundo.
   `crearBufferDeLectura` (`src/vkweave.cpp`) pide `HOST_CACHED` y cae al de antes si no hay.
   **`leer`: 75 → 1,5 ms.** Vale también para `grab` (los volcados).
2. **El relleno de huecos corría en el hilo que dibuja** (dentro de `setDepthMap`) y tardaba de 0,5 a
   17 ms según peleara por la CPU con la red. Ahora lo hace el hilo de la red y el visor solo sube el
   resultado (`setDepthMapRG`). **`subir`: 11 → 1,1 ms.**
3. **El hilo de la red se creaba de nuevo en cada estimación** (y con él su cuadrilla de OpenMP), y los
   «3 cuadros» de `cada` se contaban después de terminar, no desde el lanzamiento. Ahora es un hilo
   permanente y se cuenta desde el lanzamiento. La red pasa de 10-17 ms a ~4,3 ms por estimación.

### Para que se vea igual que lo aprobado

- Los tres seguimientos sueltos (escala del mapa 0,25, plano de apoyo, láminas 0,08) pasan a ser **por
  tiempo** (`porTiempo`, referencia 1/6 s, el ritmo real de las sesiones aprobadas): con más
  estimaciones por segundo el plano y la escala no corren más deprisa.
- El suavizado del mapa (0,6) se queda **por estimación a propósito**: mezcla la salida ya procesada
  (dilatado, relieve local, dos planos) y lo que se ve es el equilibrio de ese bucle. Probé pasarlo a por
  tiempo y el mapa cambiaba 10 niveles de media a 18 estimaciones por segundo: descartado. Queda apuntado
  como deuda: ese bucle debería suavizar antes de procesar, no después.

### Comprobado (sin panel)

| | antes | v0.69 |
|---|---|---|
| cuadros por segundo con el conversor a ritmo de video | 78-82 | **119,4-119,9** |
| cuadros que pierden un refresco | 15 % | **0,0 %** (0 de 600, tres tandas) |
| `leer` / `subir` por estimación | 75 ms / 11 ms | 1,5 ms / 1,1 ms |
| mapas por segundo (fuente a 120) | ~8 | **40** |

- `odysseyvk mapa` con una ráfaga de 10 cuadros y la misma red: **idéntico** a la v0.66 (diferencia 0).
- Imagen quieta tras 8 estimaciones a 6/s contra 20/s: mismo equilibrio (media 1,04 niveles, máx 4); el
  plano de apoyo se mueve −0,017/s en los dos casos.
- El dibujo con mapa fijo es idéntico bit a bit al de la v0.68 (diferencia 0). Sin errores en el registro.

### No probado

- En el panel con un video real (falta instalar). Ahí el navegador decodifica 4K a la vez, y con video de
  60 cuadros saldrán ~20 mapas por segundo en vez de ~6: el relieve seguirá mejor lo que se mueve, y es
  lo único que puede verse distinto.

## Realce de detalle en el sujeto, como el Hub (v0.68)

- Tercera de las tres ideas pedidas por Rodolfo. Un sujeto más nítido que el fondo se despega de él
  (enfoque selectivo: una señal de profundidad de las de foto, que se suma a la de los dos ojos).
- Leído en el ensamblador NV del Hub (`inpaint.unsharp_mask.bin`, MEDIDO): detalle = imagen − imagen en
  el nivel 2 de mip; puerta de ruido smoothstep(t/2, t, |detalle|) con t = `de_threshold` 0,05 ·
  (1 + luminancia/2); se suma con `de_strength` 0,4 (tope 3); y con `de_focus` se apaga lejos del foco con
  smoothstep(`fg_thresh` 0,2, `bg_thresh` 0,3, distancia). En el Hub viene **encendido** de fábrica.
- Portado en `realceSujeto` (`shaders/weave.frag`), por ojo y en la coordenada ya desplazada de ese ojo.
  Sin mips de la fuente, el desenfoque son cuatro lecturas bilineales a ±1,5 puntos (un cuadro de 5 × 5
  sin la cruz del centro); fuera del sujeto no se hacen. La distancia se mide en el mapa contra la
  profundidad del **sujeto** (`DepthNet::sujeto()`, por `tun9.y`), no contra el plano de la pantalla, que
  con `pivote` ya no coincide. Fuerza por `tun9.z`.
- `detalle` en `serve.conf` / `ODYSSEYVK_DETALLE`: **0,4 por defecto** (la de fábrica del Hub); 0 lo apaga.

### Comprobado (sin panel)

- El sombreador real con el cuadro real y el mapa real (apoyo 0,6 = la cara), ojo izquierdo: con 0,4 la
  nitidez del sujeto (media del laplaciano) sube **×1,26** y la de lo lejano ×1,000; píxeles cambiados
  (> 2 niveles): 4,7 % del sujeto y **0,00 % de lo lejano**; cambio máximo 57 niveles. Con 1,0: ×1,55.
  Sin la llave = 0,4 (diferencia 0). 119-120 fps con y sin realce. Sin avisos de NaN.
- A ojo en el recorte (`../pruebas/realce-sujeto-0-04-10-v0.68.png`): pelo, cejas y piel más definidos,
  sin halos visibles con 0,4.

### No probado

- En el panel. Con video muy comprimido el realce puede subir el ruido de compresión de la cara (la
  puerta de 0,05 lo frena); si pasa, `detalle = 0,2` o `0`.

## Paralaje de movimiento: lo cercano se desliza al mover la cabeza (v0.67)

- Segunda de las tres ideas pedidas por Rodolfo. Hasta ahora el rastreo solo apuntaba cada vista a su
  ojo: el contenido no cambiaba al moverse. El Hub tiene la pieza (`warp.auto_viewpos`,
  `iir_viewpos_weight 0,1`) y la trae apagada.
- Cómo: cada ojo es una cámara en la posición p (izquierdo −1, derecho +1, en medias distancias entre
  ojos) y un punto con desplazamiento s se lee en q + p·s. Con la cabeza movida h, p = h ∓ 1
  (`vistaIzq`/`vistaDer` en `shaders/weave.frag`, también en la guía del agrandado guiado). La separación
  entre ojos sigue siendo 2 s: no gasta presupuesto de la lente. Nuevo `vec4 tun9`: `kUboBytes` 336 → 352,
  `kOffTun9`, `static_assert` y nombre en la vigía de NaN.
- En el visor (`src/vkview.cpp`): h = ganancia · (x − x_reposo) / 3,25 cm, con el reposo siguiendo a la
  cabeza con 6 s de constante (da igual dónde te sientes), tope ±1 y un paso bajo de 60 ms encima del
  One-Euro; sin cara, vuelve a 0.
- `paralaje` en `serve.conf` / `ODYSSEYVK_PARALAJE` (0 = apagado, por defecto en el código; **0,5 en el
  `serve.conf` de Rodolfo**; negativo invierte el sentido). Taller: `ODYSSEYVK_PARALAJE_FIJO` fuerza h.

### Comprobado (sin panel)

- El sombreador real con el cuadro real y el mapa real, forzando h, contra la original corrida los 15,36 px
  de la convergencia: **h = +1, ojo izquierdo = la original** (media 0,074, 0,00 % de píxeles distintos);
  **h = −1, ojo derecho = la original**; h = +0,5 medio desplazamiento (4,30 % distintos); h = 0 entero
  (8,70 %); h = +1 ojo derecho, doble (14,41 %). Con h = 0, idéntico bit a bit a la v0.64 (diferencia 0).
- DERIVADO (la misma cuenta del visor, simulada): mover la cabeza 3,25 cm en 0,3 s con ganancia 0,5 da un
  pico de h = 0,47 a los 0,33 s, 0,19 a los 6 s y 0,07 a los 12 s; un temblor de rastreo de 0,17 cm deja
  h temblando 0,011, o sea **0,13 px** en lo más cercano.

### No probado / visto de paso

- **Con el rastreo en vivo**: el sentido (si gira al revés, `paralaje = -0,5`), si «nada» con la cabeza
  quieta y cómo se ve el retraso al moverse deprisa. Los mapas por ojo del relleno de huecos están hechos
  para p = ∓1; con h ≠ 0 quedan corridos h·s (como mucho un par de puntos del mapa).
- Visto de paso: con «3D Salida» en 50 toda la imagen se corre 15,36 px igual para los dos ojos
  (`t.conv = ... popout/50 · 0,004`). En el conversor eso no cambia el relieve: solo desplaza la imagen
  respecto al video de debajo (y al sitio donde caen los clics). Sin tocar.

## Más profundidad: pivote, relieve local, saliencia por defecto, y las mediciones corregidas (v0.66)

- Pedido de Rodolfo: «¿alguna otra idea que pueda resaltar profundidades?» → relieve local primero, y
  después paralaje de movimiento y realce del sujeto (v0.67 y v0.68). Dijo que con `saliencia = si` mejora
  mucho.

### Un fallo de medición mío, y su arreglo

- Las tablas de v0.63 (redes) y v0.64 (aplastamiento) se midieron pasando 22 cuadros sin relación en una
  sola llamada a `odysseyvk mapa`. `DepthNet` suaviza en el tiempo (0,6) y sigue el plano con un IIR, así
  que cada mapa salía mezclado con el anterior (se veía el fantasma de la nave detrás de una cara) y el
  apoyo no era el del cuadro. Las dos entradas llevan ya su corrección.
- Arreglo: `DepthNet::olvidar()` y **`odysseyvk mapa --sueltas`** (cada foto por separado). Comprobado: una
  foto medida detrás de otra da el mismo mapa que sola (diferencia máxima 0). Y la primera estimación de
  una sesión **salta** al plano del sujeto en vez de salir de 0,5 (tardaba 1-2 s en llegar).

### Lo que se añade

- **Saliencia puesta por defecto** (elegida en el panel). `saliencia = no` la quita.
- **Pivote** (`pivote` en `serve.conf`, `ODYSSEYVK_PIVOTE`; 0 por defecto en el código, **0,3 en el
  `serve.conf` de Rodolfo**): con el plano de la pantalla en el sujeto casi todo queda detrás y solo se
  usa el presupuesto hacia dentro (9,98 px por ojo); el de hacia fuera (12,67) queda vacío. Con pivote p,
  `focusPlane()` = sujeto · (1 − p): el sujeto sale y el fondo se hunde. MEDIDO (22 cuadros sueltos,
  saliencia, relieve 1,2, salida 50, con la compresión suave):

  | pivote | recorrido usado p1..p99 (px por ojo, de 22,65) | el sujeto sale | imagen por delante |
  |---|---|---|---|
  | 0 | 16,7 | 0 px | 25 % |
  | 0,2 | 18,6 | 3,5 px | 39 % |
  | **0,3** | **19,5** | **5,2 px** | 44 % |
  | 0,5 | 19,8 | 7,8 px | 52 % |

  Los rótulos siguen yendo al plano de la pantalla (usan el mismo `focusPlane()`).
- **Relieve local** (`relieve_local`, alfa 1,0 por defecto, radio 48): D' = D + alfa (D − base), con la
  base del filtro guiado del mapa consigo mismo. MEDIDO: **rinde poco**: +11 % de pendiente dentro de los
  objetos, +6 % de salto en los cantos, 2,5 % de puntos pegados a 0 o 1; con alfa 2: +19 % / +12 % / 4,5 %.
  Las redes dan mapas lisos por dentro. Sin coste medible (~16 ms por estimación con y sin él).
- Leído y descartado: el `dspr.shift` del Hub (`shift = 0,5`) corrige la violación de ventana (objetos que
  salen y los corta el borde) y viene apagado (`window_violation.b = False`).

### No probado

- Todo esto en el panel (falta instalar): si `pivote = 0,3` gusta o cansa; con el sujeto saliendo, los
  hombros de una persona cortados por el borde de abajo son justo la «violación de ventana».

## La saliencia del Hub para el plano de apoyo, y la ganancia de Windows (v0.65)

- Pedido de Rodolfo tras la v0.64: «se ve mejor pero aún le falta profundidad comparado con Windows;
  porta lo que decías» (la saliencia y el foco del Hub).
- Leído en el ensamblador NV de la caché del Hub (MEDIDO):
  - `depth.salc_dspr`: contraste centro-entorno = media de |F_s − F_c| en los pares de niveles de mip
    (3,6) (4,6) (4,7) (5,7) (5,8) (6,8) (`salc.level.center 3`, `surround 6`).
  - `depth.salc_merge`: saliencia = (0,2·D + 0,6·contraste(D) + 0,1·bordes + 0,1·contraste(Lab)) ×
    smootherstep(1 − r/rmax) (6t⁵ − 15t⁴ + 10t³, r = distancia al centro, rmax = media diagonal). El
    emparejado textura ↔ peso es el natural por nombre; los registros salen en otro orden.
  - `dspr.get_cam_param`: con `b_auto_control` calcula foco y `verg` para que el tramo del sujeto llene
    `real.depth_range` (−3..+4 cm); sin él copia `dflt.*`. La config de fábrica trae
    `camera.virt.auto = False` (DEDUCIDO: es ese interruptor), así que **no está probado que el Hub mueva
    el plano cero con la saliencia**; su `focus` (IIR 0,02 + cambio de escena) sí alimenta el centro de
    `dspr.compress` y el relleno de bordes.
  - Ganancia: con `real.*` (ipd 6,5, distancia 65, pantalla 50, recorrido −3..+4 cm) la separación cruda
    del mapa entero sale de ~27 px por ojo, contra 22,7 de Linux con «3D Salida» 50 (DERIVADO): ~1,2 veces,
    metida en el tope por la compresión suave.
- Lo que se añade:
  - **Saliencia** para el histograma del plano de apoyo (`src/depth.cpp`: `hacerPiramide`,
    `centroEntorno`, en una rejilla de 64 × 64 sobre el mapa de 518); el color oponente (R−G, B−Y) se
    guarda antes de normalizar la entrada de la red. **Apagada por defecto** (`ODYSSEYVK_SALIENCIA=1` o
    `saliencia = si` en `serve.conf`): con ella el sujeto se queda en la pantalla y todo lo demás va hacia
    dentro, que puede sentirse con menos «salida».
  - `foco_iir` en `serve.conf` / `ODYSSEYVK_FOCO_IIR` (0,06 por defecto; el Hub usa 0,02).
  - `serve.conf` del usuario: **`relieve = 1,2`** (la ganancia de Windows) y `saliencia = no` para
    comparar. Respaldo: `serve.conf.respaldo-2026-11-25-antes-v065`.

### Comprobado (sin panel)

- Plano de apoyo elegido (con `ODYSSEYVK_FOCO_IIR=1` para ver el destino de cada cuadro), antes → con
  saliencia: nave 0,36 → **0,95** (la nave en la pantalla), video de Riemann 0,61 → 0,80 (la cara en la
  pantalla), video del micrófono 0,23 → 0,27, globo 0,27 → 0,42. Imagen:
  `../pruebas/saliencia-antes-despues-2026-11-25.png`.
- Coste: con la rejilla de cada 4 puntos la saliencia costaba ~6 ms por estimación; con la de cada 8,
  16–19 ms por estimación con y sin saliencia (diez cuadros en caliente: dentro del ruido).
- Sin ajuste, el binario elige lo mismo que antes (nave 0,359).

### No probado

- En el panel: si `relieve = 1,2` da la profundidad que falta, y si `saliencia = si` se ve mejor o peor.

## Los planos dejan de aplastarse contra el tope: la compresión suave de Windows (v0.64)

- Pedido de Rodolfo: en Windows se distinguen planos (fondo, centro, frente, «como parallax») y aquí no.
- Leído el pipeline de profundidad de Windows **en su ensamblador NV** (caché del controlador del Hub:
  `Users/Dual/AppData/Local/odyssey3dplayer/local/c.programfiles.odyssey3dhub.conversion/glfx/binary/`,
  ficheros `!!NVfp5.0`/`!!NVcp5.0` en texto con los nombres de los parámetros; el análisis de Windows
  buscaba GLSL y por eso los dio por ilegibles). MEDIDO en el código:
  - `normalize_dspr`: D = (red − mín) / (máx − mín). `dspr_to_depth`: Z = 0,1 / (D + 0,1).
    `depth_to_dspr`: disparidad = ipd · verg · (Z − F) / (Z · F) · `depth_scale` + `depth_shift`, que
    con ese Z es **10 · (D_foco − D)**: una recta en la salida de la red, centrada en el foco, como aquí.
  - `build_remap_func` + `remap_depth`: tabla de 1024 entradas que deja igual el tramo del sujeto
    (`staend`), comprime lo de detrás (`comp`) y estira lo de delante (`expnd`), con los huecos entre
    grupos aparte (`comp0`/`expnd0`). Con la config de fábrica (`depth_remap` todo en 1,0) **es la
    identidad**: en Windows no hace nada.
  - La saliencia (`salc_*`) pesa el histograma con el que `find_main_focus` elige el sujeto, y el relleno
    de bordes del warp (`hole_edge`, `b_apply_salc`); **no** escala el relieve.
  - `dspr.compress`: **s' = c + (s − c) / (1 + |s − c| · k)** a cada lado, con k de una textura que se
    calcula del máximo del cuadro. Compresión suave: nada se aplasta.
- La diferencia que sí había: Linux recortaba con `clamp` en los topes por ojo (+12,67 / −9,98 px). Con
  22 mapas reales de `mde_large` y el apoyo que calcula Linux para cada uno:

> **CORREGIDO en v0.66.** La tabla de abajo usó un plano de apoyo mal medido (mismo fallo: varios cuadros
> en una llamada, y el plano salía de 0,5 con IIR). Con el apoyo de cada cuadro (`mapa --sueltas`), 22
> cuadros, clamp → suave: salida 30: 0,4 % → 0,0 %; **salida 50: 11,6 % → 0,3 %** (peor cuadro 46 %);
> salida 100: 34,8 % → 3,4 %. Con saliencia y `relieve = 1,2` (lo que usa Rodolfo), salida 50:
> **38,5 % → 7,3 %** (peor cuadro 82 %). La dirección se mantiene; las cifras de abajo no valen.

| relieve | puntos aplastados contra el tope (a < 0,2 px), clamp → suave | niveles de profundidad distinguibles (> 0,05 px), clamp → suave |
|---|---|---|
| 30 | 7,6 % → 1,1 % | 226,6 → 209,0 |
| **50** (el de Rodolfo) | **24,4 % → 2,0 %** (peor cuadro: 56 % con clamp) | 180,1 → 168,9 |
| 70 | 31,7 % → 2,8 % | 148,7 → 153,7 |
| 100 | 40,3 % → 3,7 % | 121,0 → 141,3 |

- Arreglo: `compSuave` en `shaders/weave.frag`, centrado en el plano de apoyo (pendiente 1 ahí: el sujeto
  no se deforma) y con k por lado para que el punto más lejano posible caiga justo en el tope; el clamp
  queda solo de red de seguridad (empuje fijo, temblor). También en la guía del agrandado guiado.
  `tun8.w` (hueco libre del bloque). `ODYSSEYVK_COMPRESION=0` vuelve al clamp.

### Comprobado (sin panel)

- La tabla de arriba.
- El sombreador real con el cuadro real, el mapa real, relieve 50, sonda 1: sin avisos de NaN, cambia el
  0,17 % de los píxeles en ese cuadro (casi todo queda detrás del plano), 119-120 fps con y sin compresión.

### No probado / lo que queda de Windows

- En el panel: si se nota la separación en planos.
- Sin portar: la saliencia de Windows para elegir el sujeto (contraste de profundidad centro-entorno 0,6 +
  disparidad 0,2 + Lab 0,1 + bordes 0,1; Linux pesa por bordes y centro) y su IIR del foco 0,02 (aquí 0,06).

## El cierre tarda 0,8 s menos, y comparación de las redes (v0.63)

### Abrir y cerrar

- MEDIDO en el registro del vigilante (sesión de las 07:20, video de YouTube con el conversor): de elegir
  «conv» al primer mapa, **8,8 s**. Seis de esos segundos eran el **volcado de diagnóstico**
  (`volcado = /tmp/rampa` en `serve.conf`): cuatro PNG 4K escritos al arrancar, con el visor a 11,6 fps y la
  red esperando. Se comentó en `serve.conf` (respaldo `serve.conf.respaldo-2026-11-25-con-volcado`). En la
  sesión siguiente, de elegir «conv» al primer mapa: **2,6 s**.
- Cierre, misma sesión: Firefox salió de pantalla completa a las 07,09 s, el vigilante lo dio por hecho a
  las 08,29 y el panel estaba en 2D a las 08,77. **1,2 s eran la `paciencia`** antes de cerrar. Esa rama
  solo corre con la sesión asentada (las salidas falsas eran las del arranque), así que pasa a **0,4 s**
  (dos vueltas del bucle de 200 ms). Lo que queda: 0,46 s de devolver panel, lente y scaler.

### Comparación de las redes (`tools/medir_redes.py`)

22 cuadros (2 de YouTube con persona hablando, 8 de una nave, 6 de un globo animado, 6 de un clip con un
papel) y 3 ráfagas de 10 cuadros seguidos, por `odysseyvk mapa` (el binario de verdad, con su limpieza).

> **CORREGIDO en v0.66.** Esta tabla se midió pasando todos los cuadros en UNA llamada a `odysseyvk mapa`:
> el suavizado en el tiempo mezcló cada mapa con el del cuadro anterior (sin relación) y las tres ráfagas
> entre sí. Repetida con `mapa --sueltas` y cada ráfaga aparte: ms 13,3 / 19,1 / 14,3 / 9,2 (large / fp32 /
> HF / small); parecido a mde_large 1 · 1,000 · 0,854 · 0,947; bordes 0,251 · 0,250 · 0,292 · 0,245;
> 3 planos 0,929 · 0,929 · 0,897 · 0,923; parpadeo media/máx 0,0057/0,0132 · 0,0057/0,0132 ·
> **0,0088/0,0275** · 0,0040/0,0125. La conclusión no cambia, pero un dato sí: la de HuggingFace es la que
> **más** parpadea, no la que menos.

| red | ms/cuadro (mediana) | parecido a mde_large (Pearson / rangos) | bordes sobre bordes (azar 0,20) | canto p99 | 3 planos (eta) | parpadeo media / máx |
|---|---|---|---|---|---|---|
| **mde_large** (fp16, la de Windows) | 11,9 | 1 / 1 | 0,249 | 0,0236 | **0,920** | 0,0152 / 0,100 |
| mde_large_fp32 | 15,5 | 1,000 / 1,000 | 0,249 | 0,0236 | 0,920 | 0,0152 / 0,100 |
| depth_anything_v2_small (HF) | 12,4 | 0,851 / 0,876 | **0,281** | 0,0195 | 0,892 | 0,0141 / **0,055** |
| **mde_small** (copiado del árbol de Windows, SHA256 verificado) | **8,2** | 0,949 / 0,943 | 0,241 | 0,0231 | 0,912 | **0,0132** / 0,107 |
| mde_tiny | — | no abre: pide 256 × 256 y `DepthNet` siempre le da 518 | | | | |

- Conclusión: **se queda `mde_large` por defecto** (la de Windows; la que mejor separa en planos).
  `mde_large_fp32` es idéntica y más lenta: no aporta. `mde_small` es casi igual y un 31 % más rápida:
  queda instalada en `~/.local/share/odysseyvk/modelos/` como alternativa. La de HuggingFace pone mejor
  los bordes en el clip del papel y parpadea menos en el peor caso, pero despega menos la cara del cuerpo.
- Lo que el usuario siente como «planos» de Windows (fondo / centro / frente) **no sale de otra red**: las
  cuatro separan igual de bien (eta 0,89–0,92). Sale del post-proceso de Windows (`focus` con IIR 0,02 y
  cambio de escena, `salc`, `apply_salc`, `depth_remap`, `vcs.compress`): ver
  `detalle-profundidad-windows.md` §1 en el árbol de Windows.

### No probado

- El cierre con 0,4 s en el panel (falta instalar).

## Los cantos dejan de salir en peine: agrandado guiado del mapa (v0.62)

- Lo que quedaba de la captura de Rodolfo tras la v0.61: el canto del micrófono (y de los hombros)
  en sierra fina.
- Causa: el mapa sale de la red a 518 × 518 y el sombreador lo estiraba a 3840 × 2160 con la cúbica,
  **sin mirar la imagen** (7,4 × 4,2 px por punto). El canto de un objeto quedaba en escalera de ~4 filas
  de pantalla y, en la franja de transición, el desplazamiento iba cambiando poco a poco y estiraba el
  color en horizontal. El recorte de canto (`CantoSuave`) solo lo disimulaba quitando relieve.
- Arreglo (`mapaGuiado` en `shaders/weave.frag`, el `jbu` del conversor de Windows): cada píxel mira los
  3 × 3 puntos del mapa de alrededor y los pesa por distancia (σ 0,8 puntos) y por cuánto se parece el
  color que ese punto va a mostrar (la imagen en su sitio menos su desplazamiento) al color que va a
  mostrar el píxel (σ 0,06 en RGB 0..1). Con eso el canto del mapa cae en el canto de la imagen y el
  desplazamiento va entero (sin recorte de canto; el temblor sigue solo en lo llano). Nuevo `vec4 tun8`
  en el bloque: **`kUboBytes` 320 → 336**, `kOffTun8 = 320`, `static_assert` al día, y su nombre en la
  vigía de NaN (sin él, un NaN en `tun8` leía fuera de la lista).
- Llaves: `ODYSSEYVK_JBU=0` vuelve a la cúbica con recorte; `ODYSSEYVK_JBU_COLOR` cambia el 0,06.
  Taller nuevo: `ODYSSEYVK_FUENTE_PNG=cuadro.png` hace que `odysseyvk test` use ese PNG (del tamaño de la
  pantalla) en vez del patrón, para pasar un cuadro real por el sombreador con `--mapa-fijo` y `--dump`.

### Comprobado (sin panel)

- Simulación en numpy de un ojo con el cuadro real 3840 × 2160 de la sesión y su mapa
  (`tools/simula_jbu.py`): el borde del micrófono sale escalonado con la cúbica y liso con el guiado.
- **El sombreador de verdad**, con ese cuadro (`ODYSSEYVK_FUENTE_PNG`), el mapa real, apoyo 0,37,
  relieve 50 y las sondas 1 y 2 (cada ojo por separado): con la cúbica, muescas en el canto izquierdo del
  micrófono en el ojo izquierdo y en el derecho en el ojo derecho; con el guiado, el contorno del
  micrófono como en el original. Cambian el 0,49 % de los píxeles.
- Sin ruido nuevo en zonas con textura: detalle horizontal (media de |∂x|) cabeza 2,14 original · 2,08
  cúbica · 2,06 guiado; libros 0,56 · 0,55 · 0,55.
- Cuadros por segundo con el cuadro real y el mapa, 14 s cada uno, dos veces: 118,9-119,8 sin guiado y
  118,7-119,8 con guiado (topado a 120 Hz por la sincronía: sin coste medible).

### No probado

- En el panel con video en movimiento. La red va cada 3 cuadros: entre mapa y mapa la imagen cambia y el
  guiado compara con colores de un cuadro que ya no es el del mapa; con movimiento rápido puede temblar el
  canto. Si pasa, `ODYSSEYVK_JBU=0` lo apaga sin recompilar.

## El detector de rótulos deja de aplastar la cara y el micrófono (v0.61)

- Pedido de Rodolfo, con captura: video de YouTube con el conversor, «bordes asquerosos», sobre todo en el
  micrófono; en Windows no le pasa.
- MEDIDO con los volcados de esa sesión (`/tmp/rampa-chico.ppm` y `-profundidad.pgm`) y `odysseyvk
  texto-detect`: el mapa tenía **cuadros de 8×8 puntos** (33 px de pantalla). Salían de `anclarTexto`
  (`src/depth.cpp`), que marcaba como «texto» **la cara** (columna de 3-4 celdas × 18 de alto: lentes,
  nariz, boca) y **el micrófono** (bloque de ~9 × 16), y los aplastaba contra el plano de la pantalla:
  se perdía el relieve de la cara y quedaban escalones en sus bordes. Además el peso se aplicaba por
  celda entera, sin interpolar.
- Arreglo:
  - **Forma**: una celda solo es rótulo si su racha horizontal mide ≥ 4 celdas y, además, es el doble de
    ancha que alta o no pasa de 10 celdas de alto. `ODYSSEYVK_TEXTO_RACHA` cambia el 4 (0 = sin filtro).
  - **Peso interpolado** entre centros de celda en vez de leído por celda.

| | video real | t1 subtítulo | t2 fina | t3 gruesa | t4 sin texto | t5 cintillo |
|---|---|---|---|---|---|---|
| antes | 11,1 % (cara, micro, subtítulo) | 10,0 % | 0,0 % | 0,2 % | 0,0 % | 2,9 % |
| racha 6 | 6,3 % | 6,7 % | 0 | 0 | 0 | 2,4 % |
| racha 5 | 6,6 % | 7,0 % | 0 | 0 | 0 | 2,4 % |
| **racha 4 (puesto)** | **6,6 % (solo subtítulo)** | **8,5 %** | **0** | **0** | **0** | **2,4 %** |
| racha 3 | 6,8 % | 9,0 % | 0 | 0 | 0 | 2,7 % |

- Escalones, con un mapa plano y el cuadro real: salto máximo entre vecinos **39 → 5** niveles; saltos de
  más de 8 niveles **8080 → 0**; mapa tocado 17,6 % → 8,3 %.

### Comprobado / no probado

- Compila; las tablas de arriba (sin panel).
- No probado en el panel. Queda **otra** fuente de bordes feos que esto no toca: el mapa de 518 puntos se
  estira a 3840×2160 sin mirar la imagen (cúbico), y en el canto del micrófono queda en escalera de ~4
  filas de pantalla; ahí el shader estira el color en horizontal y sale el peine fino. Lo que lo arregla es
  un agrandado guiado por la imagen (`jbu` de Windows). Siguiente paso.

## El rastreo deja de sacudir la trama: filtro One-Euro (v0.60)

- Pedido de Rodolfo, tras calibrar con el patrón de un ojo: la mezcla roja/azul del ojo que debe ver negro
  «se mueve mucho».
- Por qué (DERIVADO de los números del modelo): a 60 cm un centímetro de `x` mueve la fase de la trama
  5,239/60 = **0,087 ciclos**, y uno de `z` la mueve 0,044 en el borde de la pantalla. El ojo que debe ver
  negro solo tiene **±0,04** de margen (zona negra ±0,225, subpíxeles rojo y azul a ±0,185). El filtro de
  antes (mediana de 3 + exponencial α 0,35 por cuadro, ~2 cuadros de constante de tiempo) dejaba pasar
  casi todo el temblor de la detección (MEDIDO: `z` de la cámara ±30 mm cuadro a cuadro con la cara quieta).
- Lo que se añade: `FiltroEuro` en `src/track.h` (Casiez, Roussel y Vogel, CHI 2012), un paso bajo cuyo
  corte sube con la velocidad. `HeadTracker::publish` lo usa en lugar del exponencial, después de la
  mediana de 3 y de la guarda de saltos; el primer enganche y el re-enganche lo reinician.
  `ODYSSEYVK_EURO=0` vuelve al exponencial (`suavizado`); `ODYSSEYVK_EURO="0,6;0,01"` cambia corte y β.
- Elección de los números con `tools/prueba_euro.cpp` (ruido simulado x ±0,8 cm, z ±3 cm, 3 % de tirones):

  | filtro | fase que tiembla, centro / borde | retraso a 20 cm/s |
  |---|---|---|
  | exponencial α 0,35 (antes) | 0,034 / 0,066 ciclos | 1,7 cm |
  | One-Euro 0,6 Hz, β 0,25 | 0,031 / 0,074 | 1,5 cm |
  | **One-Euro 0,6 Hz, β 0,01 (puesto)** | **0,015 / 0,033** | 2,7 cm |
  | One-Euro 0,3 Hz, β 0,01 | 0,011 / 0,028 | 3,0 cm |

  Con β alto el ruido de la detección se lee como velocidad y abre el corte: no suaviza nada.

### Comprobado

- Compila; `tools/prueba_euro.cpp` da la tabla de arriba.
- En el panel (`build/odysseyvk test --track`, rampas −0,0012 / +0,0006): Rodolfo, con el patrón de un ojo:
  «se sacude menos».
- `serve.conf` del usuario: `rampa_x = −0,0012`, `rampa_y = 0,0006` (medidas contra la lente a 60 cm con el
  rastreo ya arreglado); las de las rondas B/C quedan comentadas debajo. Respaldo:
  `serve.conf.respaldo-2026-11-25-antes-v059`.

### El vigilante cerraba la sesión a los 4 s: miraba el reproductor de otra app

- MEDIDO (25-nov 07:01, recién instalada la 0.60): video de YouTube en Firefox a pantalla completa con
  el conversor; la sesión 3D se cerraba sola a los ~8 s, dos veces seguidas, y le quitaba la pantalla
  completa a Firefox. Rodolfo lo vio como «un crash en el overlay». No había volcado: lo cerraba el
  vigilante (`el navegador dice Stopped: el video se fue`).
- Causa (`src/mpris.cpp`): se quedaba con el primer reproductor cuyo nombre tuviera «chrom», «firefox»…
  y no lo soltaba. La app de escritorio de Claude (Electron) publica
  `org.mpris.MediaPlayer2.chromium.instanceN`, Identity «Claude», siempre `Stopped`; salía antes en la
  lista que `firefox.instance_1_…` (`Playing`).
- Arreglo: cada reproductor recibe puntos: +8 si coincide con la app de la ventana del video (el
  vigilante le pasa el `app_id` con `Mpris::pista`), +4 si su **Identity** es de un navegador, +2 si está
  `Playing`, +1 si `Paused`; a igualdad se queda el que ya tenía.
- Comprobado: `ctl video status` en el mismo instante, con los dos reproductores vivos: el binario nuevo
  elige `firefox.instance_1_6139 · Playing`; el instalado, `chromium.instance1036898 · Stopped`.

### No probado / visto de paso

- Al **cerrar un ojo** para la prueba, la detección pierde ese ojo y la distancia entre ojos salta a
  100-131 mm: la propia prueba ensucia el rastreo. Con los dos ojos abiertos debería ir mejor; sin medir.
- No hay predicción: con 2,7 cm de retraso a 20 cm/s, al mover la cabeza deprisa la trama va detrás
  (~0,23 ciclos un instante). El oficial adelanta 15,6 ms.
- El visor oscila entre 29 y 120 fps mientras `strata` ocupa casi toda la memoria de las dos tarjetas.
- Nada instalado: `/usr/local/bin` sigue en 0.58, así que el vigilante y los vídeos todavía usan el rastreo
  viejo (con el bloqueo) aunque ya lean las rampas nuevas de `serve.conf`.

## Paso fino de la rampa y patrón de un solo ojo, para calibrar contra la lente y no contra Windows (v0.59)

- Pedido de Rodolfo: evaluar si el port está bien o si hay que volver a Windows a medir, porque «la
  profundidad no está siendo eficiente» y se ven **diagonales** en la imagen.
- Diagnóstico (DERIVADO, no medido en el panel): las bandas que cruzan la pantalla sin seguir el contenido
  son el batido de la trama contra la lente, no la red. Con las rampas de ahora (`rampa_x −0,0040`,
  `rampa_y +0,0016`) el batido que queda contra lo que escribe Windows es de **833 mm** (ronda C), más que
  los 597 mm de ancho del panel: ~0,7 ciclos de deriva de fase de borde a borde, o sea una banda donde los
  ojos se cruzan. Para que no haya ninguna hace falta un error de paso por debajo de 0,1/3840 ≈ **2,6·10⁻⁵**
  ciclos por pixel.
- Por qué no sirve volver a Windows (DERIVADO): el objetivo con el que se afinaron las rampas es 0,450 ±
  **0,006** (foto de Windows, ronda 2), unas 200 veces más grueso que esa tolerancia, y la trama sin rampa
  (0,4473) ya cae dentro de ese margen. Además la trama depende de dónde está el ojo, y en la sesión de la
  ronda C el rastreo dio `sin cara` en los 4261 cuadros: se compararon posturas distintas.
- HIPÓTESIS sin comprobar: las rampas de ahora podrían estar persiguiendo el ruido de esa foto. Se sale de
  dudas mirando la lente, que es la única referencia con la precisión que hace falta.

### Lo que se añade

- **Paso fino con Mayús** en `[` `]` (x) y `;` `'` (y): **0,00002** ciclos por pixel por pulsación, 0,077
  ciclos de deriva de borde a borde (el grueso, 0,0004, son 1,5 ciclos y se salta el punto bueno). La
  etiqueta pasa a seis decimales y dice `(fino)`; `printTuning` también saca seis. El visor GL no pasa
  modificadores (`tuningKey(..., mods = 0)`) y se queda en grueso, como estaba.
- **Patrón de un solo ojo**, tecla **`b`** (0 → 1 → 2 → 0; `8` lo apaga): `WeaveTuning::unOjo` viaja por el
  hueco de la sonda (`tun6.w`, códigos 12 y 13; la sonda de `ODYSSEYVK_PRUEBA` manda sobre él). En
  `weave.frag` pisa las dos vistas después de elegirlas y antes del contraste, el crosstalk y el filtro:
  un ojo recibe todo blanco y el otro todo negro, con la trama de siempre. `F` también le da la vuelta.
- La ventana de ajustes mueve `rampa_x`/`rampa_y` con paso 0,00001 (antes 0,0001).
- README: las teclas nuevas, que las teclas van por posición (en el teclado latinoamericano `[` `]` son las
  dos de la derecha de la `P`), y la sección «Calibrar el paso contra la lente» con el procedimiento.
- `tools/prueba_teclas.cpp`: las teclas sin ventana.

### Comprobado

- Compila limpio en `build/` (solo los avisos de inicializadores de Vulkan que ya había).
- Trama Vulkan volcada sin tocar panel, lente, scaler ni cámara (`ODYSSEYVK_PRUEBA=N odysseyvk test
  --windowed --no-panel --no-lens --no-scaler --sin-interfaz --dump`, 3840x2160 recortado 40 px por el
  indicador): **12 + 13 = 255 en cada punto** (error máximo 1 nivel, redondeo); el 12 sigue al peso del
  filtro (sonda 4) con desvío medio **0,16** y máximo **4** niveles, que es la precompensación de crosstalk
  (0,0128 × 255 ≈ 3,3); el 13 es su negativo; el cuadro normal no se parece a ninguno (desvío medio 126).
- Ojo con el volcado: `odysseyvk play` y `odysseyvk weave` van por **OpenGL** y ahí la sonda no hace nada;
  la primera tanda de pruebas salió igual con 0, 4, 12 y 13 por eso. El volcado bueno es el de `test`.
- `tools/prueba_teclas.cpp`: **9 de 9** (grueso, fino, veinte finos deshacen un grueso y quedan en cero
  exacto, `y` sin tocar `x`, grueso sin modificadores, la vuelta de `b` y `8`).

### No probado

- **Nada en el panel**: ni el patrón visto con un ojo, ni que Mayús llegue como modificador por Wayland,
  ni el procedimiento de calibración. Tampoco los binarios instalados: siguen siendo la 0.58.

### Arreglo en el panel: las teclas no llegaban a la trama Vulkan

- Primera prueba en el panel (Rodolfo, `build/odysseyvk test --track`): **`b` no hacía nada**, aunque las
  teclas sí llegaban (`rampa_x_teclas = 0,002` en `estado` tras cinco `]`).
- Causa: el bucle de `vkview.cpp` copia los ajustes a la trama **campo por campo** y no copiaba ni `unOjo`
  ni **`rampaDX`/`rampaDY`**. Lo segundo viene de la v0.43: desde entonces `[` `]` `;` `'` solo cambiaban el
  número de `estado` y la trama Vulkan seguía con la rampa de `serve.conf` sola. Las rampas de `serve.conf`
  sí funcionaban (entran por el entorno), y el volcado con `ODYSSEYVK_PRUEBA` tampoco lo vio por lo mismo.
- Se copian las tres. Lo que viste en esa prueba (halos degradados a un lado u otro de cada bloque, que
  cambian con la profundidad y no con la cabeza) es fantasma en los cantos con disparidad, no el batido.
- Comprobado después en el panel: `b` alterna el patrón y las rampas mueven la trama (Rodolfo ve bandas
  grandes que se ensanchan con Mayús + `]`; el punto de más ancho quedó cerca de `rampa_x = +0,0094` sobre
  cero, todavía sin confirmar si es el bueno o un falso por armónicos).

### El rastreo se quedaba clavado en una pose mala para siempre

- MEDIDO en el panel: con la cara al centro, la pose que usa la trama (`panel (...)` en el registro) se
  quedó **fija en (−22,60, 0,83, 62,49) cm** durante minutos mientras la de la cámara cambiaba cuadro a cuadro.
  La trama creía al espectador 22,6 cm a la izquierda: de ahí buena parte del rojo/azul que veía el ojo
  que debía ver negro.
- Causa (`HeadTracker::publish`, `src/track.cpp`): la guarda de saltos compara con la ÚLTIMA pose aceptada.
  Una vez colada una mala, todas las buenas quedan «demasiado lejos» (> 12 cm) y se rechazan; `hold`
  reenvía la vieja como aceptada, y como cada cuadro con cara pone `misses_` a 0, nunca se llega a los 40
  de `holdFrames`. Bloqueo permanente. Además el primer enganche tomaba UNA muestra suelta.
- Arreglo: **re-enganche** tras 15 cuadros seguidos rechazados por salto (~0,25 s): se adopta la
  observación nueva (ya mediana de tres). Y el primer enganche espera a tener tres muestras.
- MEDIDO después, con cinta a 60 cm de los ojos a la pantalla: `panel z` **58,7–60,9 cm, media ~59,8**.
  La escala de distancia del rastreo está bien (error < 1 %); en el marco de la cámara la media sale ~575 mm
  con ±30 mm de dispersión y la distancia entre ojos salta entre 48 y 109 mm cuadro a cuadro (ruido de la
  detección, sin tocar).
- Visto de paso: el visor bajó de 120 a 29 fps mientras otro programa (`strata`) ocupaba 18,5 GB de la
  4090 y 10,6 GB de la 5080, con la RAM a 55/60 GB; y salen `Corrupt JPEG data` de la cámara.

### La mancha de la izquierda: el mapa A dado la vuelta

- Con el patrón de un ojo se ve una mancha fija en el borde izquierdo, abajo. Es el mapa
  `3DStackCorrection_A.png` de esta unidad: **311 puntos en 254-255 pegados a 0-1** (x 0..135, y 1552..1903
  del panel), mientras el resto del mapa no pasa de **55**. El mapa A es una fase (255 = una vuelta), así que
  son valores apenas negativos que se dieron la vuelta. El sampler interpola y en el borde entre 0 y 255 saca
  ~0,5 vueltas: media fase al revés.
- `loadCorrections` (`vkview.cpp`) lee como 0 todo lo que esté en ≥ 192 del mapa A y lo dice al arrancar
  (`[correccion] mapa A: 311 puntos dados la vuelta (>= 192) leidos como 0`; luego `mapa A: 0..55`). El
  fichero de calibración no se toca. Error que queda: 2/255 de vuelta como mucho. Solo en el visor Vulkan.
- Visto de paso en el registro del rastreo, sin arreglar: la distancia medida salta entre 53 y 68 cm y la
  distancia entre ojos entre 58 y 105 mm de un segundo a otro sin moverse; eso cambia el paso escrito y
  puede hacer que las bandas respiren.

## La casilla «Previsualización»: la cámara en vivo y **quedándose** en la ventana de ajustes (v0.58)

- Pedido de Rodolfo: «hay forma de que el visor de la camara si muestre el preview y se quede mostrandola si
  presiono o doy click en una casilla de previsualizacion?». Antes no: `camara.jpg` era un cuadro cada dos
  segundos y solo existía mientras una sesión 3D tenía la cámara abierta, así que el recuadro de la hoja de
  cámara estaba vacío (o congelado) sin sesión y se movía a pasos de dos segundos con ella.

### La casilla

- Al fondo del grupo de cámara de la hoja *General*, la casilla **«Previsualización (la camara aqui, en
  vivo)»**. Marcada, la cámara se ve en el recuadro y **se queda** viéndose ocho veces por segundo mientras
  esté marcada: no es un botón que haya que estar pulsando. Desmarcada, se cierra la fuente y **en el recuadro
  queda el último cuadro que llegó** (la ventana guarda el último `QPixmap` bueno), con el aviso al lado:
  `ojo: lo del recuadro es un cuadro de hace 12 s`. Encima de la imagen no se pinta nada.
- Debajo del recuadro, la línea de siempre dice ahora **quién está mandando los cuadros** (`los cuadros los
  manda el visor, ocho veces por segundo` / `la camara la abrio esta ventana con odysseyvk ctl vista`) y, si
  hace falta, lo viejo del cuadro.
- `odysseyvk-ajustes --vista` abre la ventana con la casilla ya marcada (útil por ssh y para probar).

### Quién tiene la cámara, y cómo se la cede al visor

La regla que manda: la `04e8:20d7` del monitor **no admite dos procesos abiertos** y su puerto UVC se pone
terco si lo toca un segundo proceso, así que la ventana nunca la abre ella: levanta un hijo y se la cede al
visor en cuanto la quiere.

- `odysseyvk ctl vista [--dev /dev/videoN] [--fps 8] [--segundos 25]` — la cámara **suelta**, sin rastreo:
  elige el modo bueno (MJPG antes que YUYV, nada por encima de 1920x1200; si la cámara no sabe de JPEG escribe
  un PPM), guarda el cuadro crudo en `camara.jpg` a la tasa pedida y se cierra solo al vencer `--segundos`.
  Medido aquí con la del Odyssey: **107–227 cuadros leídos, 8–30 guardados** en 2–4 s, ~48–55 fps de captura y
  8 por segundo de vuelco.
- **Cede la cámara** (código de salida **3**) en dos casos: si en `estado` aparece una sesión que la quiere
  (`camara_pedida = si`, mirado cada 0,4 s) y si le cierran la entrada estándar —la ventana que lo levantó
  murió—. Para eso `cmdVistaCamara` hace `poll` del fd 0 con timeout 0, y solo se fía del cierre de la cañería
  si al arrancar era una `S_ISFIFO` o una terminal; si no, avisa una vez (`la entrada no es una tuberia ni una
  terminal: no hay boton de apagar por ahi`) y no vuelve a mirar.
- La ventana (**`reconciliarVista`**, al principio del refresco de 0,5 s): con la casilla marcada y el visor
  vivo pidiendo cámara, suelta al hijo (un `\n` por su entrada, y `terminate`/`kill` si hace falta) y manda
  `vista = 1`; si el visor no estaba vivo o su sesión no usa la cámara, levanta el hijo con
  `ctl vista --fps 8 --segundos 60`. Como el hijo dura 60 s y la ventana lo vuelve a levantar **sola** —con
  metro y medio de calma, y cada 10 s si encadena tres fallos—, la vista se mantiene sin tocar nada. Antes de
  levantar nada mira el listado: si la cámara la tiene otro proceso, directamente no lo intenta.
- **Nada de bloqueos ida y vuelta**: el visor escribe `camara_pedida = si` en `estado` desde el arranque de la
  sesión si la quiere (`--track`), **aunque la apertura falle**, y reintenta abrir la cámara **6 veces cada 2 s**
  si la encontró ocupada. Probado de verdad: con la casilla marcada y el hijo comiéndose la cámara, la sesión
  nueva dice `[rastreo] no arranco: ... Device or resource busy` y dos segundos después
  `[rastreo] la camara estaba ocupada (la vista previa de la ventana de ajustes?): ya abrio, /dev/video0 en
  MJPG 1280x480 @ 60` y pasa a `siguiendo` a ~55 fps; el hijo se había salido con código 3 sin que nadie lo
  matara.
- Claves nuevas en `estado`: `camara_pedida = si|no` y `camara_vista = rapida|normal` (esta última confirma que
  el visor aceptó `vista = 1`; la ventana la re-manda si la sesión arrancó sin saberla). El visor vuelca
  `camara.jpg` cada 2 s, o cada 0,12 s con la casilla marcada. Orden nueva en el canal: `vista = 1|0`.
- La ventana cierra el hijo al cerrarse ella (`aboutToQuit`) y al cambiar de cámara en el combo; no queda ningún
  `ctl vista` huérfano tras matar la ventana (comprobado).

## Todo se llama OdysseyVK y aparece el «modo de legado» (v0.57)

- Pedido de Rodolfo: «renombra todo el proyecto a OdysseyVK, quita el texto sobre el overlay que habla
  sobre los clicks al video de atrás, haz el botón de on y off un poco menos ancho, quita el pequeño
  cuadrado de la esquina superior izquierda», «crea una sección general de la app, mete todo lo de
  webcam ahí, pero en el inicio mete opciones generales breves, pon un toggle llamado modo legacy o de
  Legado (clona o imita el método de Macdissey y ponlo ahí), que activar ese toggle desactive por
  completo la pestaña de edición avanzada (el nombre de la antigua pantalla de desarrollo» y «que el
  overlay en modo legacy también pase a ser más básico, solo con lo que ocuparía de acuerdo a sus
  funciones». **En la interfaz el modo se llama solo «modo de legado»** y su texto no menciona al
  cliente de Mac; la explicación de dónde viene está en `docs/metodo-legado.md` y en esta entrada.

### Renombrado, con migración de datos

- Nombre en todos lados: `NOMBRE_APP = "OdysseyVK"` (`src/util.h`), binarios `odysseyvk` y
  `odysseyvk-ajustes`, carpeta del repo `~/Documentos/LU agent/odysseyvk-linux` (antes
  `odyssey3d-linux`), unidad `odysseyvk-serve.service`, regla udev `70-odysseyvk.rules`, lanzador
  `odysseyvk.desktop` (`Name=OdysseyVK`, `Icon=odysseyvk`, `StartupWMClass=odysseyvk`). Los ficheros
  moved con `git mv`, así que el historial sigue su rastro.
- **Datos**: `~/.config/odysseyvk`, `~/.local/state/odysseyvk` y `~/.local/share/odysseyvk`. La primera
  vez que arranca un binario nuevo, `migrarDatosDeViejoNombre()` (`src/util.cpp`) **copia** lo de las
  carpetas viejas con `copy_options::skip_existing` (memoizado por proceso, una vez). Medido aquí: la
  copia de los modelos son **237 MB duplicados** (`~/.local/share/odyssey3d/modelos` y la nueva igual);
  se puede borrar la vieja cuando todo funcione.
- **Entorno**: `entornoOdyssey("ODYSSEYVK_...")` prueba la variable nueva y si no, la vieja
  `ODYSSEY3D_...`. Reescritos los **26 sitios** que leían variables; `ODYSSEY_CONV`, `ODYSSEY_UBO`,
  `ODYSSEY_GOLPE` y `ODYSSEY_HWDEC` se quedaron como estaban porque nunca llevaron el nombre del
  proyecto.
- `install.sh` (69 → ~110 líneas): instala el icono y el lanzador, y **limpia el nombre viejo** (apaga y
  borra `odyssey3d-serve.service`, quita `odyssey3d.desktop` y `/etc/udev/rules.d/70-odyssey-3d.rules`,
  y mueve los binarios viejos a `/usr/local/bin/viejo-odyssey3d/` en vez de borrarlos). Los
  `gtk-update-icon-cache` y `update-desktop-database` van protegidos con `command -v` porque el script
  corre con `set -euo pipefail`.
- **Icono**: `assets/icono-odysseyvk.png`, recorte cuadrado 608x608 sacado del PNG que pasó Rodolfo
  (1408x768 con fondo transparente; la caja de alfa medía `(400,88)-(1008,688)`), más ocho tallas en
  `assets/hicolor/{16,24,32,48,64,128,256,512}x…/apps/odysseyvk.png` (de 706 B a 294.888 B). La ventana
  de ajustes lo pilla del tema (`QIcon::fromTheme("odysseyvk")`) con reserva a la ruta del repo, y el
  visor lo pone como título de ventana (`NOMBRE_APP`, en `src/viewer.cpp:138` y `src/vkview.cpp:909`).
  El SVG que llegó primero venía truncado y se descartó.

### La pastilla, más limpia

- **Interruptor más corto**: `sw3D_` pasa de 130 a **112 px** de ancho (`src/vkui.cpp:315`). Medido en la
  autoprueba: `ON` gasta 22,5 px y `OFF` 28,8 px sobre 70 px de hueco; cabe la letra y sobra aire.
- **Fuera el cuadrito de la esquina** (el de color de lente/scaler, `solid(x0+8, y0+8, 6, 6, …)`) y
  **fuera el aviso escrito de «clic fuera de la pastilla manda sobre el vídeo de atrás»**. El estado del
  hardware se sigue diciendo solo con texto arriba de la pastilla: `lente y scaler activos` /
  `hardware parcial` / `sin lente ni scaler`, y en legado `lente activa` / `sin lente` (los bits son
  `lensOk = 1`, `scalerOn = 2` en `src/session.h:29`).

### La hoja General de la ventana de ajustes

- Pestañas: **General** y **Edición avanzada** (antes *Trama y redes*; la hoja *Cámara* desaparece como
  tal y **todo lo de la webcam se muda a General**, intacto: combos de `/dev/video*`, sello verde
  `FACETRACKING ACTIVO · Stereo (3D Studio, camara integrada del monitor)`, «Cómo la lee el rastreo»,
  «Usar esta camara en el rastreo» y su sello de `serve.conf`).
- General lleva, en orden: título y estado del visor; grupo **Modo de la app** con la casilla
  **«modo de legado»**, la nota que explica qué hace y una línea viva que el temporizador de 0,5 s
  refresca («El visor que está abierto está en modo completo…» / «…en modo de legado: la hoja Edición
  avanzada está apagada y la pastilla lleva solo el interruptor, 3D Salida y Plano de apoyo») y avisa
  si lo pedido y lo vivo no coinciden; grupo **La sesión abierta** con `3D: encender la trama` /
  `3D: apagar la trama`, `pastilla: mostrar` / `pastilla: ocultar` y la casilla de Facetracking.
- La casilla escribe **las dos cosas**: la orden viva por el canal (`legado = encender|apagar`) y
  `legado = si|no` en `serve.conf` (`ajuste::fijarEnServeConf`). Con el modo puesto,
  `pestanas->setTabEnabled(1, false)` **atiene la pestaña Edición avanzada**. Comprobado con captura
  real de la ventana (spectacle, recorte de la zona 1338..3782 px): con `legado = si` la casilla sale
  marcada en negrita y la pestaña atenuada; con `legado = no`, desmarcada y la pestaña viva.
  Ojo: aquí no hay inyector de teclas ni de clics en Wayland, así que el clic **no se probó de
  verdad**; se probó escribiendo la config y mirando la ventana.
- Dos detalles corregidos por lo que se vio en la captura: `QCheckBox` **no pinta HTML** (la casilla
  decía literalmente `<b>modo de legado</b>`), así que el texto va plano y la negrita con
  `QFont::setBold(true)`.

### El modo de legado (por dentro)

- Estado `legado` en `Hud::State` (`src/vkui.h`), y cuatro formas de moverlo: la casilla,
  `odysseyvk play --legado`, `odysseyvk ctl legado encender|apagar|alternar|estado` (nuevo, en
  `src/main.cpp`) y `legado = si` en `serve.conf` (`ServeCfg::legado`, y el hub pasa `--legado` al
  lanzar el visor). El visor lo publica en `estado` con la línea `legado = si|no`.
- **No toca la sesión**: `cambioLegado()` (`src/vkview.cpp:251-276`) guarda los números de la trama,
  aplica los de `ajuste::valoresFabrica()` **excepto `relieve` y `apoyo`** (`src/ajuste.cpp:309-333`) y
  al apagar el modo los devuelve. Se puede cambiar en caliente sin cerrar nada.
- **Puerta a los números finos**: `aplicarAjuste()` ignora cualquier clave que no sea `relieve` o
  `apoyo` (`src/vkview.cpp:202-210`); están negadas con aviso por consola las teclas de sintonía fina
  (`src/vkview.cpp:596-603`), la del conversor (`C`, `:545-546`) y el cambio de red (`1`/`2`,
  `:582-585`). El hub tampoco pasa `--track-campo` / `--track-cara` cuando el modo viene puesto.
- **Rastreo heredado**: `fo.fuerzaMono = 0` (las dos vistas de la cámara del monitor, siempre) y
  `fo.set.maxReprojectionPx = 40.0` (`src/vkview.cpp:1374-1380`), que son los mismos números de la
  referencia: `minimumDepth: 200` / `maximumError: 40` en `StereoTracking.swift:82` y `minDepth` 200 ya
  igual en `src/track.h:19`. Fuera del legado el error admitido sigue en 10 px (`src/track.h:16`).
- **La cámara manda**: si deja de ver la cara se suelta la trama a los **0,5 s** y se apaga la lente a
  los **0,75 s**, y vuelven al recuperar la cara (`src/vkview.cpp:2488-2510` y la puerta
  `caraPerdida` en `:2219-2222`). El vídeo se ve tal cual mientras tanto.
- **Uniformes fijos comprobados en la referencia**: `r = .0128535`, `m = .05`, `s = 10`, `z = 1` en
  `Sources/OdysseyGL/Renderer.cpp:68-71`.
- **Diferencia deliberada**: aquí el modo de legado **sí** sigue mandando el comando del scaler del
  monitor. En el cliente de Mac no hace falta; en Linux, sin ese comando el panel no entra en 3D.
- Pendiente declarado: la ruta **`--gl` no implementa el modo de legado** (acepta la bandera y no hace
  nada), y quedan sin copiar los 16 formatos estéreo de aquel cliente y sus curvas de la interfaz a
  uniformes. También sigue pendiente el `0x07` fijo de `src/lens.cpp:203,209` (va el byte 14 del
  `SEChipID`), que aplica a los dos modos.

### La pastilla en modo de legado, medida

`./odysseyvk ctl hud --w 1440 --h 900 [--legado]` (autoprueba sin pantalla, en la máquina):

| | modo completo | modo de legado |
|---|---|---|
| pastilla dibujada | **824 x 134 px** | **648 x 134 px** |
| vértices de la interfaz | 2784 | **2580** (−204) |
| casillas de ratón del botón de red | 1458 | **0** (no existe) |
| deslizadores (primera casilla, x de la ventana) | 1116 y 1266 | 1110 y 1260 |
| casillas de cada deslizador | 923 y 923 | 923 y 923 |

En legado los deslizadores se reetiquetan **«Plano de apoyo»** (es `apoyo`, la `3D Depth` del panel) y
**«3D Salida»** (es `relieve`); el botón de red y su bloque de zonas de ratón se van (`btnRed_ =
{-10,-10,0,0}`) y el aviso largo de `Tab` pasa a la nota corta `Tab  entrega la pantalla`. Letras
medidas dentro de su hueco (usado / hueco): `Plano de apoyo` 96,9 / 120 · `3D Salida` 60,0 / 120 ·
`Ctrl+Shift+3 / 4` 92,2 / 120 · `Tab  entrega la pantalla` 140,0 / 272 · `lente activa` 66,0 / 220 ·
`sin lente` 48,7 / 220. La fila del Facetracking gasta 306 px y deja 498 px (completo) / 322 px
(legado), así que la nota de 224 px cabe en los dos modos.

### Lo que se tocó fuera del repo

- `Master/maestro.sh`: `REPO` apunta a `../odysseyvk-linux` (con `ODYSSEYVK_REPO` por delante de la
  variable vieja), `haySesion` mira las dos unidades, y `guardar` mete en el bulto `install.sh`,
  `odysseyvk.desktop`, `systemd/`, `udev/`, `assets/` y `config/`, leyendo la config y el estado de la
  carpeta nueva **y** de la vieja.
- Limpieza de las copias del nombre viejo (2026-11-25 04:10, después de ver el 3D y el rastreo con el
  nombre nuevo): antes de moverlas, `find … -printf '%s %p\n' | sort` + `diff` dio que
  `~/.local/share/odysseyvk` tiene exactamente los mismos ficheros y tamaños que
  `~/.local/share/odyssey3d` (237 MB: las cuatro redes y el detector de cara). Las tres carpetas viejas
  (`share`, `config` y `state` de `odyssey3d`) se movieron a
  `../papelera-odyssey3d-2026-11-25/` en vez de borrarse, por si hace falta volver. No queda nada del
  nombre viejo en `~/.config`, `~/.local/share` ni `~/.local/state`.

### Instalando la v0.57: dos cosas que el renombrado dejó cojas

- `install.sh` corrida el 25-11 a las 03:34: `/usr/local/bin/odysseyvk` y `odysseyvk-ajustes` a 0.57,
  unidad `odysseyvk-serve` activa y activada para la sesión, regla `/etc/udev/rules.d/70-odysseyvk.rules`
  (la vieja `70-odyssey-3d.rules` ya no está), lanzador `~/.local/share/applications/odysseyvk.desktop`
  y el icono en las ocho tallas de `~/.local/share/icons/hicolor`. Los binarios del nombre viejo se
  guardaron en `/usr/local/bin/viejo-odyssey3d/` y `odyssey3d-serve` quedó apagado y sin fichero de
  unidad. `odysseyvk doctor` con la regla nueva: lente en `/dev/ttyACM0` (accesible, `auth: si`,
  firmware 3.16.68), scaler en `/dev/sg1` y `/dev/video0` y `/dev/video1` accesibles — sin tocar el USB.
- El `serve.conf` que copió la migración seguía diciendo `ignore = odyssey3d, …` y
  `modelo = ~/.local/share/odyssey3d/modelos/mde_large.onnx`. Con el `app_id` nuevo la primera línea ya
  no tapaba nuestra propia ventana, y el filtro por título buscaba `odyssey 3d`, que tampoco coincide
  con el título `OdysseyVK`: el vigilante podía ofrecer 3D a su propio visor si este se arranca a mano
  (`odysseyvk play …`) mientras el servicio mira las ventanas.
- Arreglo en el código: `loadServeCfg` añade siempre `odysseyvk` y `odyssey3d` a la lista de ignoradas
  (`src/main.cpp:2828-2836`) y `ignoredApp` acepta los dos títulos (`src/main.cpp:2849-2852`). En la
  config viva se escribió `ignore = odysseyvk, odyssey3d, …` y la red de Samsung apunta ahora a
  `~/.local/share/odysseyvk/modelos/mde_large.onnx`; los dos `mde_large.onnx` miden 49.477.899 bytes, así
  que cambiar la ruta es inocuo y deja de usarse la copia vieja. Tras `systemctl --user restart
  odysseyvk-serve` el log dice `boton 1 = /home/rodolfo/.local/share/odysseyvk/modelos/mde_large.onnx`.
- Probado en la máquina con el binario de `build/`: `ctl legado encender` y `ctl legado apagar` dejan
  `legado = si` y `legado = no` en la línea 141 de un `serve.conf` de 146 líneas (9.680 bytes), y
  `ctl legado estado` responde `modo de legado pedido en serve.conf: si/no` más
  `no hay visor leyendo el canal (o es de otra versión)` cuando no hay sesión.
- Dos cosas más que salieron al probar: el vigilante leía `serve.conf` **una sola vez al arrancar**, así
  que el modo cambiado desde la ventana no llegaba al visor que se lanzara después; ahora `spawnViewer`
  vuelve a mirar `legado` justo antes del `fork` (`src/main.cpp:2864-2875`, solo ese valor: los demás
  números siguen necesitando reinicio). Y el respaldo se escribía en `serve.conf.anterior` mientras el
  README y la ventana prometen `serve.conf.auterior`, así que en la máquina quedaron los dos ficheros
  (9.680 y 9.681 bytes, `src/ajuste.cpp:371-379`); se escribe el nombre de siempre y el sobrante se borró.
- Sin probar: que los dos filtros (lista y título) se notan con una ventana a pantalla completa delante,
  y que el modo refrescado llegue a una sesión real. Las dos cosas piden poner una ventana a pantalla
  completa, y eso agarra el panel; además aquí no hay inyector de clics.
- Instalado y comprobado (03:50 y 03:57): `/usr/local/bin/odysseyvk` y `odysseyvk-ajustes` coinciden byte
  a byte con los de `build/` (`md5` iguales), la unidad arrancada a las 03:57 corre ese binario, y Rodolfo
  hizo una sesión real de 95 s con Firefox a pantalla completa: panel a 3D, scaler activado, y al salir
  `modo 3D apagado`, `lente devuelta a 2D` y `panel restaurado: modo 20 3840x2160@119.88, escala 1.5`,
  con DP-7 devuelta a su sitio.

### El rastreo no arrancaba en las sesiones lanzadas por el servicio (v0.57)

- Esa sesión de 95 s tuvo 3D pero **sin seguimiento de cabeza**: las líneas periódicas decían
  `0/0 cuadros (0.0 fps) · hace -1,00 s · calibracion: sin SR+CALCAM.bin en .` y el estado
  `camara_cara = no`, aunque la cámara se había abierto bien (`[rastreo] /dev/video0 en MJPG 1280x480 @
  60 (dos vistas, calibrada), buscando la cara`).
- La causa no es el renombrado: `calibBases()` (`src/calib.cpp:78-91`) buscaba `SR+CALCAM.bin` y
  `SR+CALET.bin` junto al binario y en rutas **relativas** (`.`, `./calib`, `../calib`), pero nunca en
  `~/.config/odysseyvk/calib`, que es donde se guardan los de ESA unidad (los mapa de corrección sí se
  buscaban ahí, por `findDataFile`, `src/main.cpp:1365-1387`). Como el servicio lanza el visor con el
  directorio actual en el home, las rutas relativas no llevaban a ninguna parte. Contado en el registro
  de la unidad vieja: 3.768 líneas de rastreo con cuadros (las sesiones arrancadas desde el repo, que
  tenían `./calib` a mano) frente a 1.044 con la nota de calibración faltante y **0 cuadros**; las dos
  cosas están perfectamente correlacionadas. Con `odyssey3d` instalado y sesiones desde el repo, por eso
  se veían hasta `4340/4340 cuadros (59.8 fps)`.
- Arreglo: `calibBases()` pone primero `~/.config/odysseyvk/calib` y después el del nombre viejo
  (`~/.config/odyssey3d/calib`) y luego lo de siempre; y el aviso ya no dice solo el primer directorio
  probado, lista todos (`src/calib.cpp:93-128`) porque el texto sale en `camara_nota`.
- Medido con el binario instalado, desde `/tmp` y sin abrir sesión: antes
  `ctl track --segundos 3` → `sin SR+CALCAM.bin en .` y ninguna foto; después → `cara si`,
  `167 fotogramas leidos, 165 poses validas` en 3 s, cara a ~55-60 cm, error 0,3-2,4 px; la segunda
  pasada en 2 s dio `107 fotogramas leidos, 105 poses validas`. Los ficheros que se usan son los
  migrados: `SR+CALCAM.bin` y `SR+CALET.bin`, 256 bytes cada uno, con cabecera 248/1 y 56/1 y
  `fx = 474,32` en el de la cámara.
- **Comprobado dentro de una sesión lanzada por el servicio** (04:08-04:09, con `ctl escritorio poner`,
  que es el botón de la ventana: el vigilante lanza `odysseyvk test --siempre … --track` y no toca ningún
  vídeo): a los 6 s de abrir la cámara `camara_cara = si` y
  `camara_nota = MJPG 1280x480 @ 60 (dos vistas, calibrada) · cara a la vista`; las líneas periódicas
  pasaron de `1/0 cuadros (0.0 fps)` a `328/328 cuadros (57.7 fps) · error 1,70 px · siguiendo` y
  aguantaron `1409/1409 cuadros (59.4 fps)` con error 0,1-2,6 px y la cara a ~67-76 cm del panel. Al
  quitarlo, salida limpia: `modo 3D apagado`, `lente devuelta a 2D`, `panel restaurado: modo 20
  3840x2160@119.88, escala 1.5`, `DP-7 devuelta a 2560,624` y ningún visor quedando vivo.

### El icono de la aplicación no abría nada (v0.57)

- Al pulsar el icono del menú no pasaba nada. No es el icono: el lanzador hacía `Exec=odysseyvk play %F`
  y al abrirlo desde el menú no hay archivo detrás, así que el `%F` se queda vacío. Medido:
  `odysseyvk play` a secas responde `falta el archivo: odysseyvk play ARCHIVO` y **sale con código 2**. El
  lanzador venía así desde la época de `odyssey3d`, solo que entonces casi nunca se usaba el icono.
- Ahora son dos entradas: `odysseyvk.desktop` abre **la ventana de ajustes** (`Exec=odysseyvk-ajustes`, con
  una acción «Reproducir un archivo en 3D» en el menú contextual) y `odysseyvk-play.desktop`
  (`NoDisplay=true`) guarda la asociación con vídeos (`Exec=odysseyvk play %f`, `MimeType=…`). El lanzador
  visible lleva `TryExec=odysseyvk-ajustes` para que el menú lo apague si el binario no está.
- `install.sh` copia las dos y refresca el caché de iconos y la base de escritorios. Las dos pasan
  `desktop-file-validate` sin quejas.
- Comprobado: `gtk-launch odysseyvk` (lo que hace el menú al pulsar) levantó la ventana — proceso
  `odysseyvk-ajustes` vivo y visible en la captura de pantalla, con el título «Ajustes del visor 3D (proceso
  aparte)» y el icono nuevo en la barra de título. Lo que **no** se ha probado: abrir un vídeo de verdad
  desde el explorador de archivos ni la acción contextual.
- En la máquina el lanzador instalado no era idéntico al del repo (tenía `MimeType` y `Name[es]` distintos,
  tocados por el editor de menús de KDE); se sobrescribió con el del repo. El viejo quedó en
  `/tmp/odysseyvk.desktop.viejo` y en el historial de git.

## La pastilla deja de ser redonda y el 3D se apaga con un interruptor (v0.56)
- Pedido de Rodolfo: «quita el botón de apagar 3D y el de ocultar capa, cambia el de apagar por un botón
  más o menos así» (foto: pista oscura con bola blanca y la letra `ON` / `OFF`), «el overlay se oculta y
  muestra con Control+Shift+F1» y «el overlay no sea redondo, se ve con curvas, debe ser cuadrado».
- **Interruptor en vez de botón.** `Hit3D` pica la pista entera (`sw3D_` de 130x34), no solo la bola:
  pista verde (`accent`) con la bola de 26 px a la izquierda y `ON` a su derecha cuando la trama está
  puesta; pista gris (0,22 / 0,30) con la bola a la derecha y `OFF` a su izquierda cuando está apagada.
  Entre la bola y el borde quedan 4 px de aire. La letra va a 15 px, centrada con `textWidth`.
- **Esquinas cuadradas de verdad.** El fallo no estaba en los botones sino en `Hud::mask9`: recortaba la
  esquina con el radio del rectángulo (17 px) sobre una máscara de 96 px, así que a las teselas y al panel
  les salía un arco raro —y al interruptor una lente puntiaguda. Ahora `mask9` guarda el radio propio de
  cada máscara (`maskRad_[2] = {48, 8}`: píldora y tarjeta) y recorta con `min(maskRad_/msz, 0,5)`. Lo
  rectangular se dibuja con `Hud::solid()`, sin máscara: panel, botón rojo, chips de Facetracking, punto
  verde, punto de hardware, panel del aviso, teselas, casilla, mandos de los deslizadores (9 px) y puntos
  de estado. Las únicas curvas que quedan son la pista y la bola del interruptor.
- **Adiós al botón «Ocultar capa»; es tecla.** Se fue `HitOverlay`: la tabla de zonas de la autoprueba pasó de
  12 nombres a **11** y barre **10** zonas de golpe de ratón; el interruptor ocupa **1188 celdas** y el
  botón capa ya no existe. La tecla es
  **`Ctrl+Shift+F1`** y `Ctrl+Shift+2` sigue valiendo. Se comprobado que KWin no la usa: su
  «Switch to Desktop 1» está en `Ctrl+F1` / `Meta+F1`, así que `Ctrl+Shift+F1` está libre.
- Y como el teclado se lo puede quedar una ventana a pantalla completa, la misma orden viaja por fichero:
  **`odyssey3d ctl capa [ocultar|mostrar|alternar|estado]`** (y para el interruptor,
  **`odyssey3d ctl 3d [encender|apagar|alternar|estado]`**). Son órdenes de palabra en el mismo fichero
  `~/.local/state/odyssey3d/ajuste`, leídas cada décima de segundo; `capa` y `3d` las atiende el visor, no
  el hub, por eso están en `esOrdenTexto` y no en `esOrden`. El fichero `estado` ahora dice también
  `capa = puesta|oculta`, y cada cambio se anota en `preferencias.log`.
- Medido con la autoprueba en cuatro tamaños (1920x1080, 1440x900, 1024x600, 800x600): todas las letras
  caben —`oculta o muestra la pastilla` mide **157,4 px** en 166 de hueco, `OFF` **28,8 px** en 88—, la
  fila de Facetracking ocupa **306 px** de 824 y el rincón pinchable sigue valiendo (`1726,24 170x134`).
- Probado de verdad, con la trama encima del escritorio (`ctl escritorio poner`) y una captura de la
  ventana (`ctl capture-gl`): al ocultar, la región de entrada del visor pasa de **2144,48 1648x268** a
  **3452,48 340x268** (solo el rincón) y la pastilla desaparece de la imagen —la zona pasa de 90.756 a
  **4.109 bytes**— mientras la trama sigue `activa`. Con `ctl 3d apagar` la pista sale gris, con la bola a
  la derecha y la letra `OFF`, como en la foto.
- **Lo que no se pudo probar aquí**: la pulsación física de `Ctrl+Shift+F1`. En esta máquina no hay
  inyector de teclas para Wayland (no están `ydotool` ni `wtype`, `xdotool` no alcanza las superficies
  nativas y Rodolfo no pertenece al grupo `input`), así que la tecla está escrita en el manejador y
  verificada por su equivalente de fichero, que hace exactamente el mismo llamado.
- Detalle honesto sobre el canal: si la orden se manda en los primeros ~3 segundos de una sesión, el visor
  todavía está arrancando Vulkan y la cámara y puede no darle a leer el fichero a tiempo; la orden que
  queda escrita se aplica cuando arranca, pero las intermedias se pierden. Esperar un poco y mandar la
  orden funciona (y por eso la comprobación se hizo con `sleep 8`).

## El rastreo en vivo por fin ve la cara: las dos vistas se recortaban mal (v0.55)
- Pedido de Rodolfo: «me siento ahora, sondea 8 s» y, después del informe, «sí: score 0,5, letterbox, CLAHE y
  reproyección ~10 px». El sondeo daba **0 poses con una cara delante**, así que se fue a medir en vez de adivinar.
- **La causa, encontrada midiendo**: `HeadTracker::decodeFrame` recortaba el cuadro de 1280x480 en dos vistas con
  una región de OpenCV **sin `clone()`**, y la copiaba a bytes planos con `assign(left.data, left.data + total)`.
  La región de un cuadro de 1280 de ancho no es contigua (sus filas están separadas 1280 bytes), así que la copia
  lineal se llevaba las 240 primeras filas de **las dos vistas mezcladas** y datos de la vista vecina. El detector
  recibía un cuadro remezclado: por eso nunca salía una cara en vivo, mientras que la vía de archivo
  (`describeStill`, que sí usa `.clone()`) las encontraba en los mismos cuadros.
- Cómo se vio, sin tocar la cámara: `ctl track --volcar-crudo RUTA` imprime el brillo del cuadro ya decodificado y
  salva los bytes crudos. Los bytes eran **idénticos** (firma FNV igual antes de decodificar, después y en el
  archivo), y al decodificarlos otra vez salía otra imagen: vivo 137,7 de brillo frente a 114,4 de los mismos
  bytes. Con eso quedó descartado el decodificador, el `imdecode` en memoria y la exposición, y señalado el recorte.
- Arreglo: las dos regiones se **clonan** antes de copiarlas (`track.cpp`, en `decodeFrame`). Es justo lo que hace el
  cliente de Mac, que arma las vistas fila por fila respetando el paso (`StereoTracking.swift:150-156`).
- Medido después del arreglo, sentado delante, sin overlay montado:
  `ctl track --segundos 8` → **467 fotogramas leidos, 465 poses validas**, y la cuenta cierra con
  `careos  sin cara 1, cara descartada por los ojos 0, geometria rechazada 0`. Pose típica:
  panel (-3,5; 10,1; 54,0) cm, ipd 66-74 mm, **error de reproyección 0,48-2,11 px**, YuNet 3,6-4,6 ms por cuadro.
  Con `--realce no` sale idéntico (465 de 467), así que el CLAHE no hace falta en sala iluminada: se queda como
  red de seguridad para escena oscura, y solo paga cuando la primera pasada no ve nadie.
- Comprobado a ojo también: los círculos de los ojos caen **sobre las pupilas** en las dos vistas, con una
  disparidad horizontal de ~60 px, que es la que toca (focal ~554 px × 60 mm de separación / 0,55 m ≈ 60 px).
- Los cambios de detección que pidió Rodolfo siguen puestos y son los del Mac —**letterbox** con relleno 114 en vez
  de aplastar a 448x320, `score` de **0,5**, reintento **CLAHE** (`clipLimit 40`, `tile 8x8`), triangulación con
  hasta **10 px** de error en vez de 3—, pero hay que decirlo claro: la tabla A/B que escribí primero (4 de 5
  cuadros válidos frente a 1 de 5) se midió sobre cuadros ya remezclados, así que **ese número no vale**. El que
  vale es el de arriba: 465 de 467.
- Añadidos de diagnóstico que se quedan: la línea `careos ...` del sondeo (tres motivos que antes se escondían
  detrás de «sin cara»), `--volcar-crudo RUTA` (cuadro crudo en JPEG + anotado en PNG + el mismo cuadro repasado
  por la vía de archivo) y `--realce no`. El reintento CLAHE vive en `opencv_photo`, ya enlazado.
- Sigue siendo cierto lo de la exposición: el **primer cuadro** después de `STREAMON` llega quemado (brillo medio
  212 y **57,5 %** de píxeles por encima de 250) y al cuadro 30 (≈0,5 s) ya está bien (112, 2,9 %). Aun así la cara
  se detecta en el cuadro 1; la exposición del monitor tarda medio segundo en asentarse.

## La pastilla dice Facetracking con 3D Studio y fallback, y la ventana tiene hoja de Cámara (v0.54)
- Pedido de Rodolfo: «el comportamiento del software debe ser simple, ya no ofrecer activar o desactivar el
  rastreo, sino en el overlay decir Facetracking y junto 3D Studio o fallback, que esté marcado el activo y
  el texto del otro grayed out»; y «crea una pestaña con selección de webcam, debe venir indicador de un
  use Stereo, o External, si puedes muestra una preview de lo que se observa». Con *Studio* se refiere a la
  **cámara integrada del monitor**.
- Qué se quitó: el botón `Rastreo (T)` de la pastilla, su área tocable (`trackBounds`) y la tecla `T`. El
  rastreo ya no se ofrece a encender ni apagar: si `serve.conf` dice `rastreo = si` y la cámara abre, manda
  la cámara; si la cámara no abre, la trama va con la pose de fábrica y la fila lo dice con letras.
- Qué se puso en la pastilla: la palabra `Facetracking` y dos pastillitas, `3D Studio` y `fallback`. La viva
  va tintada con el acento (92 %) y con un punto verde; la otra en gris (0,20 / 0,21 / 0,23 y el texto al
  40 % de tinta). Con la integrada del monitor sale `3D Studio` marcada; con una webcam aparte sale
  `fallback`, que es la vía que mide la distancia por el ancho de la cara (`mono = si`). La fila **informa,
  no manda**: no tiene área tocable. La nota de la derecha se recorta contra el ancho de la pastilla
  (824 px), medida en `test hud`.
- Medido con la v0.54 instalada, sin montar el overlay: `ctl track --segundos 5` →
  `energia control=on estado=active`, `camara /dev/video0 1280x480 MJPG @ 60`, **287 fotogramas en 5 s**
  (≈57 fps) con YuNet a **4,4–6,3 ms** por cuadro, y **0 poses válidas**: los 287 dicen `sin cara` porque la
  silla estaba vacía, como se ve en la propia vista. O sea que el rastreo está puesto y la cámara responde;
  para ver la pastilla marcada hace falta una cara delante. Queda esa prueba tuya.
- Hoja nueva `Cámara` en `odyssey3d-ajustes` (la ventana pasa a dos hojas: *Trama y redes* y *Cámara*): combo
  con las `/dev/video*` que dan cuadro —nombre del conductor, id USB, si es la estereo del monitor, el modo
  que daría y si lo tiene otro proceso—, sello de color con `FACETRACKING ACTIVO · Stereo (3D Studio…)` /
  `· Externa…` / `FACETRACKING PARADO: motivo`, la vista, la comparación entre la cámara que usa la sesión
  en curso (la dice `estado`) y la pedida en `serve.conf`, y el botón `Usar esta camara en el rastreo`, que
  escribe `camara` (y `mono`) en `serve.conf` dejando copia en `serve.conf.auterior` **y** manda la orden
  viva. La ventana re-cuenta las cámaras cada 4 s, no veinte veces por segundo, porque `Camera::describe`
  abre cada nodo para leerle el nombre y los modos.
- La preview no la saca la ventana: la cámara del monitor no admite dos aperturas y su puerto de control UVC
  se pone terco si lo toca un segundo proceso (lo sufrimos con `VIDIOC_S_FMT ... Input/output error`). El que
  la abre —el visor en sesión, o el sondeo `ctl track`— vuelca cada 2 s el último cuadro crudo a
  `~/.local/state/odyssey3d/camara.jpg`, y la ventana lo decodifica y lo enseña. Medido: 150 KB escritos, y
  `--prueba` lo decodifica con Qt a **1280x480**, las dos vistas una al lado de la otra. `Camera::describe`
  descubre el `/dev/video1` de *Metadata Capture only* y lo deja fuera de la lista.
- Claves nuevas en el fichero `estado`, escritas por el visor: `facetracking` (`abierto` / `sin abrir`),
  `camara`, `camara_tipo` (`3D Studio` / `externa`), `camara_cara` (`si`/`no`) y `camara_nota`.
- Lista de redes, pedido aparte: «con otros no me refiero a otros dispositivos, quita la separación, solo
  marca como Native los que no son de samsung y Samsung replicate el que sí es, mantén los nombres simples».
  Una sola lista plana y la marca sale del **nombre del fichero**: `mde_*` y `Disp_*` → `Samsung replicate`,
  lo demás → `Native`. Medido con `--prueba`: 10 redes, `mde_large.onnx → Samsung replicate`,
  `depth_anything_v2_small.onnx → Native`, `face_detection_yunet_2023mar.onnx → Native`.

## Revertir por red, dos listas con flechas y globos en cada deslizador (v0.53)
- `Revertir` deja los números de fábrica de la red puesta y borra su perfil guardado; los guardados se llenan
  solos con las redes que se salieron de fábrica y se pueden exportar (un `.conf` comentado con lo suelto) o
  borrar. Las redes pasan de una lista desplegable a dos listas con flechas (disponibles y seleccionadas) más
  una tercera de guardados, y cada deslizador enseña un globo transparente con el nombre y el valor mientras
  se arrastra.

## El botón que pone la trama encima del escritorio, y las redes separadas en nativas y otras (v0.52)
- Pedido de Rodolfo: «añade un botón para cargar el overlay en el escritorio del monitor, el click lo
  fuerza y un segundo click lo quita; también, referente a los modelos, separa los nativos como modelos
  nativos y los otros márcalos como otros».
- Cómo viaja la orden: la ventana de ajustes escribe `overlay = 1` (o `0`) en el fichero de siempre
  (`~/.local/state/odyssey3d/ajuste`), y manda lo mismo `odyssey3d ctl escritorio poner|quitar|estado`. El
  visor pasa de esas líneas en silencio (`ajuste::esOrden`), porque no son un número suyo: quien la atiende
  es el **hub**, que es el único que puede lanzar y despachar una sesión. Para que el botón sepa si ya está
  puesto, el hub cuenta lo suyo en un fichero nuevo, `~/.local/state/odyssey3d/hub`: `hora`, `servicio`,
  `sesion = ninguna|video|escritorio`, `ventana`, `pantalla`, `modo`, `red`, `modelo`. Lo escribe una vez
  por segundo y lo **borra al pararse**; si viene con más de 5 segundos de retraso la ventana dice «el
  servicio se fue» en vez de quedarse creyendo el recuerdo.
- Qué monta: `test --siempre`, o sea el patrón de trama de siempre, por el mismo camino del visor (panel a
  3D, escala, lente, pastilla, rastreo y rampas incluidos), sin `--uuid` y sin conversor. **No** capturamos
  la salida del monitor: ahí estaría nuestra propia ventana tejiéndose a sí misma, así que el escritorio
  forzado enseña la trama y su relieve, no el escritorio de fondo. El protocolo de captura de KWin sí
  ofrece `stream_output`; lo descartamos por eso.
- Es un flanco, no un nivel: solo hacemos algo cuando el número **cambia** en el fichero y cuando lo que
  pide no está ya hecho. Un clic de más no monta nada dos veces, y el mismo botón lo quita (SIGTERM al
  visor, que devuelve el panel por la puerta de adelante). La primera lectura del fichero al arrancar el
  servicio **no** cuenta como orden: si el equipo se apagó con `overlay = 1` escrito, al encender no aparece
  la trama sola en el escritorio. Mientras el overlay forzado está puesto el hub no ofrece el 3D de vídeo
  (hay sesión abierta); se quita con el botón, con `ctl escritorio quitar` o con ESC/Tab sobre el visor.
- Comprobado hoy sin tocar el panel: `serve --dry` con la orden escrita a los 2 s →
  `[serve] (dry) habría puesto el overlay del escritorio (patrón de trama sin video)`, y al pararse el
  fichero `hub` desaparece (`ls: no se puede acceder a «/home/rodolfo/.local/state/odyssey3d/hub»: No existe
  el fichero o el directorio`). La sesión de patrón lanzada a mano en ventana
  (`test --siempre --windowed --no-panel … --dump /tmp/ofd --volcado-seguir`) deja `/tmp/ofd-trama-vulkan.png`
  (3840x2160 leído de Vulkan), `/tmp/ofd-trama-cruda.png` y `/tmp/ofd-origen.png`, y al SIGTERM sale con
  `[sesión] llegó una señal de cierre: se sale devolviendo el panel, la lente y el scaler`. Con
  `volcado = /tmp/rampa` en `serve.conf`, el overlay del escritorio también deja sus volcados: sirven para
  medir las franjas con `tools/mide_franjas.py` sin abrir ningún vídeo.
  **Queda por tu parte: la prueba en el panel de verdad**, que es la que cambia la escala y la frecuencia.
- Las redes de la lista salen en dos bloques, y los títulos no se pueden elegir: **NATIVAS** = las
  instaladas en la máquina (`~/.local/share/odyssey3d/modelos` y el `modelos/` del programa), **OTROS** = la
  memoria USB, carpetas sueltas y lo que diga `ODYSSEY3D_MODELOS`. Cada fichero lleva su aviso cuando
  sabemos algo: `mde_tiny` → «aquí no abrió: pide otras dimensiones de entrada» (medido: ONNX Runtime la
  rechaza con `Got invalid dimensions for input: input`) y `face_detection_yunet_2023mar` → «no es de
  profundidad: es la cara para el rastreo». `Otro archivo…` ya no añade duplicados: si la ruta estaba en la
  lista la selecciona, y si no la añade marcada como `otros (a mano)`.
- Lo que cuenta `odyssey3d-ajustes --prueba` hoy: 10 redes, **5 nativas** (`depth_anything_v2_small`,
  `face_detection_yunet`, `mde_large`, `mde_large_fp32`, y otra copia de la cara en el repo) y **5 otras**
  en la USB (`Disp_Disp_1008_ver1`, `mde_large`, `mde_large_fp32`, `mde_small`, `mde_tiny`).

## Medido: la meseta del medio fabrica cantos nuevos (`muestras` vuelve a 0)
- Pedido de Rodolfo tras probar v0.50: «volvió el raspado».
- Medido sin pantalla, con numpy sobre los dos mapas reales que escribe `odyssey3d planos --out`
  (`/tmp/bancos/cuadro003-mapa.pgm` y `cuadro007-mapa.pgm`, dureza 0,6), contando los saltos bruscos
  entre píxeles vecinos:
  - **sin muestra intermedia**: saltos de más de 8 niveles = **0,00 %**, más de 20 = **0,00 %**, salto
    máximo entre vecinos = **9** niveles.
  - **con 1 muestra**: saltos de más de 8 niveles = **0,48 %**, más de 20 = **0,22 %**, salto máximo =
    **37** niveles (unos 3 px de disparidad de golpe).
- Conclusión: la meseta crea un canto nuevo donde antes había rampa, y ese canto es lo que él veía
  raspar. Tenía razón, y se revierte el valor: `muestras = 0` en `serve.conf` con la medición escrita
  en el comentario. El código de la muestra sigue ahí (`n` / `x`, `--muestras`, `ctl planos`) porque
  sirve para comparar, pero **no es el valor por defecto en la máquina**.
- Verificado byte a byte: con `muestras = 0` la salida del conversor es idéntica a la de v0.49 (`cmp`).

## La ventana de ajustes es un proceso aparte, y manda el modelo al visor vivo (v0.51)
- Pedido de Rodolfo: «quiero que la ventana de Qt sea **independiente por completo**, pero que los
  cambios de **modelo** en ella se apliquen al overlay, que esto permita que analices la última
  preferencia y ya contigo hablar sobre ella y corregir».
- Canal nuevo (`src/ajuste.h` / `src/ajuste.cpp`): tres ficheros en `~/.local/state/odyssey3d`, sin
  socket ni librería compartida, así que la ventana no puede romper la trama ni colgar el 3D:
  - `ajuste` es la **orden**: números sueltos con las mismas palabras que `serve.conf` y una línea
    `modelo = /ruta.onnx`. El visor lo mira cada 0,1 s y **solo lo aplica si el fichero cambió**
    (compara mtime y tamaño). Es una orden de una sola vez: cada escritura reemplaza el fichero
    entero, para que un número que luego se movió con las teclas no vuelva atrás al mandar otro.
  - `estado` es la **verdad del visor**: lo escribe él cada 0,5 s (versión, si la trama y el conversor
    están puestos, qué `.onnx` y en qué tarjeta, y los 20 números con su nombre). Si el visor no está,
    el fichero deja de actualizarse y la ventana lo dice.
  - `preferencias.log` es la **cuenta de lo que se cambió**, con hora, valor antes → valor después, red
    puesta y quién lo pidió (`la tecla o la pastilla` / `la ventana de ajustes`). Rota a
    `preferencias.log.anterior` al pasar 256 KB. Esto es lo que se puede repasar después en voz alta.
- Ventana (`src/ui_ajustes.cpp`, target opcional `odyssey3d-ajustes`): solo Qt Widgets, sin enlazar
  nada del visor. Lista los `.onnx` que hay en el disco (incluidos los de la copia de Windows),
  deslizador + caja por número con sus topes, recetas para guardar/cargar y el historial a la vista.
- El cambio de **modelo** en vivo ya existía para las teclas `1`/`2` (`DepthNet::cambiar`); ahora la
  ventana encarga la ruta por el fichero y el visor la atiende con el hilo de la red parado, como con
  las teclas. Si el conversor está apagado el encargo **queda pendiente** y se aplica al encenderlo con
  `C`. Si la red no abre, ONNX Runtime queda inservible en ese proceso (`quemada_`, v0.33) y no se
  insiste: el encargo se queda escrito y abre en la siguiente sesión.
- Números que hasta hoy se leían **una sola vez** del entorno al arrancar ya son miembros del visor y
  se pueden mover en vivo: `temblor` (0,25 px), `canto` (0,004), `fase_temblor` (0), `rampa_x`/`rampa_y`
  de la configuración. Se rellenan en cada cuadro en `VkWeave::fillUbo`, así que surten al instante.
- Prueba sin tocar el panel (`test --windowed --no-panel --no-lens --no-scaler --conver`): números
  aplicados al cuadro siguiente, y dos cambios de red seguidos en la misma sesión
  (`depth_anything_v2_small` → `mde_large_fp32` → `depth_anything_v2_small`, los dos con
  `[conversor] red cambiada … mapa 518 x 518`) sin que el visor se cayera. Una red que no abre
  (`mde_tiny.onnx`, dimensiones de entrada distintas) avisa y deja el relieve apagado, como siempre.
- Nuevos verbos: `ctl ajuste [clave valor … | clave=valor | modelo=RUTA | limpiar]`, `ctl estado`,
  `ctl modelos`, `ctl preferencias [--ultimos N]`.

## El navegador ya no se queda a pantalla completa sin vídeo (v0.51)
- Pedido de Rodolfo: «quiero que **cualquier vídeo en cualquier navegador** haga esto; el navegador no
  tiene por qué quedarse en fullscreen sin vídeo».
- Medido el 24/11 a las 20:53 con el registro del servicio: 20:53:16 «la ventana del video salió de
  pantalla completa … cerramos la sesión 3D» y **un segundo después** Firefox se la volvió a poner él
  solo. O sea: la pantalla completa la sostiene la página y el navegador se la reponía.
- Qué se hace ahora (en el hub, `src/main.cpp`): solo cuando la sesión se cerró **porque el vídeo se
  fue** (la ventana salió de pantalla completa, o el navegador dijo «Parado» o «sin reproductor» por
  MPRIS) se le quita la pantalla completa a ESA ventana, con pausas de 1,2 s y **tope de 3 intentos**
  dentro de los **20 segundos** siguientes.
- Fallo que salió a la primera y se corrigió (medido a las 22:04): la primera versión se bajaba del
  vigía en cuanto la ventana aparecía «ya en su tamaño normal»… que es justo como está en el momento en
  que cerramos la sesión, porque por eso la cerramos. Así que nunca llegaba a ver la reposición de la
  página, que llega un segundo DESPUÉS. Ahora la vigía se queda mirando hasta 20 s a ver si se la vuelven
  a poner.
- Medido a las 22:06 con mpv mandado por su propio socket IPC (sale de pantalla completa a los 0 s, la
  página se la pone a los +5 s y vuelve a insistir a los +9 s). Registro de esa corrida:
  - `la ventana del video salio de pantalla completa (…): cerramos la sesion 3D y devolvemos el panel a 2D`
  - `sesion 3D terminada (…)`
  - `el video se fue y la ventana del navegador sigue a pantalla completa: se la quitamos (1/3)`
  - `… se la quitamos (2/3)`, y después `ctl windows` deja la ventana en `activa 0,0 1280x748`, **sin** la
    marca de pantalla completa. O sea: el escritorio recupera su tamaño y el navegador no se queda
    tapando todo con una página que ya no tiene vídeo.
  - Al insistir una tercera vez, ya pasado el plazo: `… sigue a pantalla completa despues de 2 intento(s) y
    no insistimos mas: la pagina se la sigue poniendo ella.` Aviso **una sola vez** y la ventana se queda
    como ella quiere: no peleamos con el navegador para siempre.
- Regla dura que se respetó: **nunca** se toca la pantalla completa durante una sesión (probado antes:
  el va-y-ven saca al vídeo de su propio fullscreen y corta el 3D) ni cuando la sesión se cerró por
  elección — `2D plano`, `Tab` o `ESC`—, porque ahí el vídeo sigue sonando y la pantalla completa es
  suya. Se apaga con `devolver_fullscreen = no` en `serve.conf`.
- Queda por tu parte: comprobarlo con YouTube de verdad (con el navegador, no con mpv), que es donde la
  página sostiene la pantalla completa y donde MPRIS dice «Parado» al acabar el vídeo. La mecánica ya está
  medida con mpv y no depende del navegador.

## La muestra intermedia entre las dos láminas (v0.50)
- Pedido de Rodolfo: «el modelo nativo ya no tiene bordes rotos como el de samsung, se ve mucho más fluido
  el tramo entre ambas capas, ahora quiero que, si es posible haya **una muestra intermedia** para que se vea
  aún más fluido el borde, **no como degradado** sino que ya se vea **más sólida la porción del medio** de
  ambas capas».
- Dónde estaba el problema: con `aplicarPlanos` el mapa se llevaba a las dos láminas con un `smoothstep`
  continuo entre `a` y `b`. Como la banda `[a,b]` abarca casi todo el intervalo, la porción del medio salía
  como **una rampa**: cada punto con su propio desplazamiento, que es el degradado que él notaba.
- Como se arregló (en el conversor, sobre el mapa de 518 × 518; el sombreador y sus topes por ojo no se
  tocaron): `aplicarPlanos(…, muestras)`. Con `muestras >= 1` el suave continuo se cuantiza en escalones —
  `nv = muestras + 2` niveles (máximo 5) — y cada nivel tiene su **meseta**: el cruce entre niveles se hace
  con el mismo `smoothstep` de siempre pero comprimido en una franja angosta (`ancho = 0,18` de cada salto),
  así que por debajo de `f = 0,32` y por encima de `f = 0,68` el valor ya está clavado en la meseta. Con
  `muestras = 1` salen tres niveles exactos: fondo, **medio a mitad de camino exacta**, y frente. Con `0`
  queda la rampa de antes, byte por byte.
- Medido con el mapa rampa sintético (`/tmp/rampa-profundidad.pgm`, dureza 1): sin muestras el mapa de
  salida tiene **51 valores distintos** (la rampa); con **1 muestra** tiene **9**, y los tres niveles son
  84 · **125** · 166 — el del medio clavado justo a mitad de camino, y el 6,2 % del cuadro cayendo en él.
- Medido en ocho cuadros reales del banco, dureza 0,3: el **parpadeo de la costura no sube** (0,21 % con y
  sin muestras), y el porcentaje del cuadro por delante se mueve 1 punto (31,0 % → 30,0 %), así que la
  geometría del relieve no se desplaza.
- Regresión verificada: `--muestras 0` produce un mapa de salida **idéntico byte a byte** al del binario
  instalado de v0.49 (`cmp` limpio).
- Mandos nuevos: teclas **`n`** (agrega una muestra intermedia, hasta 3) y **`x`** (la quita, y a 0 vuelve a
  la rampa continua); `--muestras 1` en la línea de mando y **`muestras = 1`** en `serve.conf`. El propio
  `odyssey3d planos` acepta `--muestras N` y con `--hist` imprime el histograma **después** de aplastar, que
  es donde se ven las mesetas.
- Ojo con la dureza: la muestra intermedia se aplica **antes** de la mezcla por dureza, así que con `planos =
  0,3` solo se nota un 30 % del camino hacia la meseta. Para verla sólida hay que subir la dureza con `g`.

## Tres arreglo del navegador: la franja duplicada, el sitio de las salidas y la trama que no se quita (v0.49)
- Pedido de Rodolfo, tres cosas: (1) al pulsar **convertir** el vídeo «pasa a formar parte de un navegador
  en pantalla completa» y hay que dar click en sus controles para traerlo; (2) **al salir del vídeo no se
  quita la trama** porque el navegador sigue a pantalla completa; (3) **una parte del borde se pasa a la
  segunda pantalla** cuando el overlay está activo, igual que con los cuadrados de prueba. Aclarado por él:
  «overlay» es **la trama tejida sobre la imagen**, y el vídeo es **siempre Firefox (YouTube)** con la
  ventana del navegador a pantalla completa.
- Causa medida de (1) y (3), la misma: **el área lógica del panel cambiaba de tamaño al entrar en sesión**.
  `kscreen-doctor -o` da el panel DP‑4 en `0,0 2560x1440` con **escala 1,5** (el lienzo físico es 3840 × 2160)
  y DP‑7 en `2560,624 2048x1536` con escala 1. Al poner escala 1 el área lógica del panel pasa a
  0…3840 × 0…2160, y **se monta 1280 px lógicos sobre DP‑7** (que empieza en 2560). De ahí la franja
  duplicada y de ahí que KWin le cambie el tamaño a la ventana del navegador a media sesión.
- Arreglado en dos capas, como eligió él («preparar la escala 1 **y** reponer posiciones por si acaso»):
  - **Escritorio**: DP‑4 quedado **permanentemente a escala 1** y DP‑7 movido a **3840,624**, en una sola
    orden atómica. Medido después: DP‑4 `0,0 3840x2160`, DP‑7 `3840,624 2048x1536`, **solape 0**; la ventana
    de Firefox pasó a `0,0 3840x2160` y la de Strata siguió a su monitor. El texto del escritorio se ve más
    chico (es el precio de la escala 1); se compensa subiendo el tamaño de fuente en Ajustustes, no la escala.
  - **Código**: `Panel` ahora **lee la geometría** (`parseAll()` ignoraba la línea `Geometry:`, así que antes
    no se guardaba ni se devolvía ningún sitio), y hay `allOutputs()`, `modeSize()` y `despejar()`. La regla de
    `despejar()`: si la zona de otra salida se monta sobre la del panel con el tamaño nuevo, se mueve a la
    derecha (o a la izquierda, según de qué lado caiga su centro) **en la misma invocación de kscreen-doctor**,
    que es como lo pide la ayuda del propio kscreen-doctor. `saveSnapshot`/`loadSnapshot` guardan `geometry=`
    y una línea `otra=NOMBRE x,y` por salida movida, y `shutDown()` / `devolverPanelHuerfano` la devuelven.
    `ctl panel status` imprime el sitio, y `ctl panel apply --dry` imprime el comando completo con las
    posiciones. Verificado en seco: `output.DP-4.mode.20 output.DP-4.scale.1 output.DP-4.rotation.none
    output.DP-7.position.3840,624`, y con el escritorio ya preparado **no mueve nada**.
  - Puertas por si acaso: `--sin-despejar` en el visor y `ODYSSEY3D_LAYOUT=0` dejan el escritorio intacto.
    Si una sesión se cortó a mitad, los sitios de origen se toman de la instantánea, no del escritorio ya movido.
- (2) ya no depende de la ventana: **el navegador lo dice por MPRIS**. Firefox publica
  `org.mpris.MediaPlayer2.firefox.instance_1_42` con `PlaybackStatus` (medido: «Paused» con el título de la
  pestaña). Añadido un vigía en el hub: con sesión abierta sobre una ventana de navegador, si el navegador
  dice **«Stopped»** durante `video_parado` segundos, o si el reproductor desaparece del bus (la página se
  quedó sin vídeo), mandamos SIGTERM al visor y la trama se quita sola. **«Paused» no cierra nada** — una
  pausa es una pausa y ahí la trama se queda viendo — y solo se usa el reproductor **del propio navegador**,
  para que el «Stopped» de un reproductor de música abierto no cierre la sesión. `video_parado = 4` en
  `serve.conf`; con `0` se apaga y se vuelve al comportamiento anterior.
- Nota del registro real (20:02 de hoy) que explica por qué antes se cerraba y volvía: la ventana del
  navegador **recupera su pantalla completa sola** un segundo después (`[ventana] video: … -> … pantalla
  completa`), así que por la ventana la señal de salida no es fiable en un navegador. Confirmado en código:
  ni el visor ni la sesión escriben `Fullscreen` — la pantalla completa la sostiene la página, no nosotros.
- Nuevos avisos en el registro: `[panel] DP-7 pasa a …`, `[panel] DP-7 devuelta a …`, `[video] <reproductor>
  dice Playing/Paused/Stopped`, `[ventana] el navegador dice Stopped: el video se fue…`.


## Dos planos por cuadro, como en la Nintendo 3DS (v0.48)
- Pedido de Rodolfo: «revises lo de crear un modelo que separe la imagen en 2 planos parallax, porque
  parece que todo lo maneja de forma similar, algo parecido al nintendo 3d». Elegido por él: **empezar
  sin red nueva**, usando el mapa que ya tenemos, y **arrancar apagado** con un mando para subirlo (el
  relieve continuo de hoy sigue siendo el valor de arranque).
- La lección de la 3DS que manda el diseño: allí la pantalla **no decidía las capas**. El juego dibujaba
  sus planos y el potenciómetro (`3D_SLIDERSTATE`, leído por `osGet3DSliderState()`) solo los separaba
  más o menos; la barrera física era fija. Aquí igual: **el contenido pone los dos planos** y el deslizador
  de relieve los escala, con el presupuesto duro por ojo ya impuesto (+12,67 hacia afuera, 9,98 hacia
  adentro sobre 3840). Por eso la separación va **en el conversor**, sobre el mapa de 518 × 518, y no en
  el sombreador: el bloque de parámetros sigue en 320 bytes y los topes por ojo no se tocaron.
- Añadido en `depth.cpp/h`: `buscarPlanos()` y `aplicarPlanos()`. El corte sale de un **Otsu sobre el
  histograma de 32 casillas del propio mapa ya limpio** (filtro guiado + dilate + corte de plano), y el
  valor de cada lámina es la **media** de su grupo, no el pico del histograma; con el pico las láminas
  salían corridas hacia los bordes. Rechaza el cuadro si los dos grupos separan menos de 0,15 o si uno
  ocupa menos del 6 % o más del 94 % del cuadro: sin dos picos claros no hay dos planos que cortar.
- Costura sin tajo: franja suave de ancho `min(0,33 · separación, 0,05)` con `smoothstep`, y la **dureza**
  interpola entre el mapa continuo y las láminas (`mapa += (lamina − mapa) · dureza`). Con dureza 0 el
  bloque ni se ejecuta: el mapa sale **byte a byte idéntico** al de v0.47 (comprobado con `cmp`).
- Lo que de verdad evita el parpadeo es el **seguimiento temporal**: los dos cortes persiguen lo que ve
  el cuadro con media móvil de 0,08 y banda muerta de 0,004, saltan de golpe en un cambio de plano y se
  recalculan si las láminas se pegan (separación < 0,10). El mapa ya cuantizado alimenta `prev_`, así que
  la realimentación es estable en vez de pelearse cuadro a cuadro.
- Mandos: teclas `g`/`v` (dureza ±0,1, tope 0…1), `--planos 0..1` en la línea de mando y `planos = 0,3`
  en `serve.conf` (esa rama de la config ya convierte las comas en punto, como `rampa_x` y compañía).
  No es un número
  de la trama: no entra en el UBO, y por eso el vigía anti-pisotón de v0.47 solo lo mira como un campo más
  de los que mueven las teclas.
- Herramientas nuevas para medir esto **sin tocar el panel**:
  - `odyssey3d mapa foto.png` hace mapas PGM de fotos reales con la red (acepta PNG y PPM, `--modelo`,
      `--red N`) y dice en cada cuadro apoyo, dos planos y cuánto cae por delante.
  - `odyssey3d planos mapa1.pgm mapa2.pgm …` sigue los dos cortes por la secuencia, mide el **parpadeo de
      la costura** (cuántos puntos cambian de lámina de un cuadro al siguiente) y cierra con la media.
      `--dureza` escribe el mapa cerrado (`--out`) y `--hist` imprime las 32 casillas con barras.
  - `--mapa-fijo MAPA.pgm --apoyo 0..1 --relieve 0..100` arranca el visor con un mapa ya calculado y sin
      red, para A/B deterministas. Sin estas tres banderas el A/B no valía nada: en modo `test` la red solo
      hace una pasada y el mapa nunca llegaba a subir.
  - `ODYSSEY3D_MEDIR=1` añade `[planos] dureza … · atras … · delante … · % del cuadro por delante`.
- **Medido** (todo sin panel, sobre archivos):
  - Mapa real de un cuadro (`/tmp/rampa-profundidad.pgm`): `atras 0,184 · delante 0,830 · separación 0,646
      · 30,4 % del cuadro por delante`.
  - Secuencia real de 14 fotogramas de vídeo (red `mde_large`, 6,7–8,3 ms por foto): los planos salen muy
      estables, `atras 0,102…0,120`, `delante 0,775…0,817`, `30,6…31,4 % por delante`. Con el seguimiento
      en marcha el **parpadeo medio de la costura es 0,20 % de los puntos por cuadro** a dureza 1.
  - Escalera de dureza sobre el mismo mapa: 0 = idéntico byte a byte; 0,3 = 214 niveles y desvío medio 5,5;
      0,6 = 173 niveles y 11,0; 1,0 = 118 niveles con dos picos dominantes (47 con 60,8 % y 212 con 22,4 %),
      o sea dos láminas ya planas.
  - Coste: con dureza 0,6 en el visor, `[medir] plano de apoyo` pasa de 0,5 a 1,0 ms por pasada de red
      (~0,5 ms extra sobre un presupuesto de ~8 ms). `[planos] dureza 0.60 · atras 0.17 · delante 0.57 ·
      39.4 % del cuadro por delante`.
  - A/B de extremo a extremo con mapa fijo (misma orden cuatro veces, mismo cuadro trameado): control con
      el mismo mapa **desvío 0,00 y 0,0 % de píxeles distintos**; mapa en dos láminas duras **6,6 % de
      píxeles distintos (3,5 % con más de 8 niveles)**; dureza 0,6 **5,1 % / 2,4 %**. Repetido, los números
      salen idénticos, así que la diferencia es el mapa y no el azar.
- Errores que aparecieron por el camino y quedaron arreglados:
  - `--planos 0,6` y `--dureza 0,6` se leían como **0** (`atof` con coma). Añadido `leerNumero`, que acepta
      punto y coma, para `--planos`, `--apoyo`, `--relieve` y `--dureza`.
  - Con `--mapa-fijo` y el conversor apagado las cuatro tramas salían **idénticas**: sin relieve el mapa no
      mueve nada, y el `setConverter` solo corría dentro del bloque de la red. Añadido `--relieve` y una
      llamada paralela para el mapa fijo.
  - En ventana el lienzo a veces salía a 3840 × 2160 y a veces a 5760 × 3240: con el panel a escala de
      contenido 1,5 pedíamos una ventana del tamaño del **modo** y KWin la hacía en puntos lógicos, o sea
      1,5 veces más grande que la pantalla. Se veía solo una esquina, y de la escalera de disparidad del
      patrón desaparecían los cuadros de la derecha (Rodolfo: «se ven solo los últimos, del amarillo al
      azul»). Arreglado en modo ventana: se pide el tamaño del modo dividido por la escala de contenido y
      el lienzo vuelve a salir píxel a píxel. Medido después del arreglo: los **nueve** cuadros de la
      escalera están en el volcado y dos pasadas seguidas dan un volcado **idéntico** (desvío 0,00), así
      que los A/B por fin son comparables. La sesión de verdad no se toca por aquí: al tomar el panel la
      escala queda en 1 y no se entra en ese parche.
  - Las dos funciones nuevas cayeron dentro del primer `namespace {` anónimo de `depth.cpp` y chocaban con
      las declaraciones de `depth.h` (llamada ambigua). Movidas fuera del anónimo.
- Detalles a tener en cuenta: `anclarTexto` sigue apoyando los rótulos en `focus_` (el plano de la
  pantalla), no en las dos láminas, así que con dureza alta el rótulo queda en medio del tajo con
  desplazamiento ≈ 0, que es lo deseado; pero su histograma entra en `prev_` y puede mover un poco el corte
  de Otsu en cuadros con mucho texto.
- Riesgos dichos de frente: la segmentación puede parpadear en planos raros (mitigado, no eliminado), en la
  costura se nota el borde si el mapa venía sucio, y en un plano fijo mal segmentado el tajo se ve si se
  sube mucho la dureza. Por eso arranca en 0.
- Siguiente paso acordado: **Paso 2**, portar `degraph` (cajas de gráficos, subtítulos y ventanas dentro del
  vídeo → plano delantero con `depth_offset`, `occlusion`, `blur_edge`, umbrales y dilate/erode) apoyándose
  en lo que ya hace `anclarTexto`, y el modo «3 bandas» con la semántica exacta de `scene_dspr`
  (`near_diff_scale −0,5`, `far_diff_scale 1,0`, `far_diff_popup −1,0`) para poder comparar si él lo enciende
  en Windows. Paso 3 opcional: una red figura/fondo chica (`u2netp`/`salnet`, ~4 MB, 320 × 320) o
  `Disp_Disp_1008_ver1.onnx` como segunda opinión, con `iir_weight 0,1` y rampa lenta.

## El visor gris: el bloque de la trama se salía de su caja y se pisaba sus propios números (v0.47)
- Síntoma que dio Rodolfo: «veo el overlay con un fondo color gris, nada de imagen». Medido sobre el
  cuadro del propio visor: **RGB(127,127,127) exacto, varianza 0** en toda la banda central. Con la
  misma textura pero sin tramar sí salía imagen (media 18,7), y la red y la captura estaban sanas
  (mapa de 518x518 con 47.461 colores, `[fps] 83/76`). O sea: el gris lo fabricaba el sombreador.
- **Causa**: `uboData_`, el almacén en CPU del bloque de parámetros de `weave.frag`, seguía declarado
  `uint8_t uboData_[288]` (desde v0.12) mientras el bloque pasó a **320 bytes en v0.45** al añadir el
  octavo `vec4` de las capas. Cada cuadro `memset` + escrituras llegaban hasta el byte 319, es decir
  **32 bytes fuera de la caja**, y en la pila esos 32 bytes eran exactamente los números de la trama
  del visor. Comprobado con puntos de vigilancia sobre la variable: la escritura la hacía la propia
  `VkWeave::fillUbo` sobre `ui.tune` (distancia `dst` → `&ui.tune` = 295 bytes).
- **Por qué eso es gris y no otra cosa**: el contraste caía a 0 y el sombreador hace
  `L = (L − 0,5) * contraste + 0,5`; con contraste 0 eso es **0,5 para todos los píxeles** → 127,5 →
  gris parejo. De paso se iban a 0 la pendiente del filtro (trama sin separar) y la escala de la
  corrección A, y `paso`/`crosstalk` recibían basura de la pila. El fallo era **mudo**: nada avisaba.
- Arreglado: `kUboBytes` se declaró en `vkweave.h` y el almacén usa ese mismo número
  (`uint8_t uboData_[kUboBytes]`), y en `vkweave.cpp` hay un `static_assert` de que el último desfase
  (`kOffTun7 + 16`) no se sale del bloque. Así no puede volver a pasar: si alguien añade un `vec4` y
  sube los desfases, salta la compilación.
- Añadida **red contra números locos** en `fillUbo`: si algún parámetro del bloque no es un número
  finito (NaN/infinito), se sustituye por el neutro y se avisa diciendo el primer nombre y su offset
  (el sombreador con un NaN pinta gris y no deja rastro).
- Añadido **vigía de los números de la trama** en el bucle por cuadro: compara de cuadro a cuadro los
  únicos campos que ninguna tecla toca (contraste, escala de la corrección A, crosstalk dinámico) y
  avisa una sola vez si cambian solos, más un chequeo de que ninguno se vuelva loco. Probado
  volviendo a poner el almacén corto a propósito: salta
  `[aviso] algo pise los números de la trama entre cuadros: el contraste valía 1,000000 y ahora vale −0,000000`
  y ya no vuelve a aparecer el gris silencioso.
- Añadida **sonda del tramado** en el visor (solo con `--dump`): mide el cuadro trameado y avisa
  `[aviso] el tramado pintó una imagen plana: 98% de los puntos están en 125,5`, y guarda al lado
  `-trama-cruda.png` (la misma textura sin tramar). Si un día sale gris, con mirar esos dos archivos
  se sabe si el problema es la captura o los números, sin tocar el panel.
- Sondas de taller en el sombreador (puerta `ODYSSEY3D_PRUEBA`, 0 = normal, no cuesta nada mientras no
  se usen): 1 = muestra del ojo izquierdo, 2 = ojo derecho, 3 = textura con coordenada 2D, 4 = peso del
  filtro, 5 = crosstalk empaquetado, 6 = fase base, 7 = coordenadas de la cuadrícula, 8 = `v4` de las
  esquinas, 9 = correcciones y paso, 10/11 = lo que sale de la mezcla. Con ellas se vio que la mezcla ya
  nacía gris (`Lp`/`Rp` = 127) y que el crosstalk llegaba bien (0,012854).
- Seguro extra en el sombreador para el crosstalk: `xt` se topa a ±0,90 antes de la división
  `xt/(1−xt)` y a ±12 después; sin eso, un parámetro fuera de rango podía invertir o disparar la mezcla.
- **Falta revalidar con él**: con el filtro y el contraste vuelven a valer 10 y 1, así que el aspecto
  cambia respecto a las últimas sesiones (que estaban en gris). Hay que volver a mirar `rampa_x = −0,0040`
  y `rampa_y = 0,0016` y el aspecto de las franjas: es probable que parte de los defectos que venía
  reportando fueran este pisotón y no el paso de la lente.

## Empuje por capa: llevar una banda entera a un plano (v0.46)
- Con solo estirar la escala (`fondo`/`frente` de v0.45) un elemento **casi plano** no se entera: si
  su profundidad casi no se separa del plano de apoyo, su desplazamiento es casi cero y no hay forma
  de traerlo al frente. Falta la segunda mitad de `scene_dspr`: el **desplazamiento fijo** de cada
  banda, que en la config de Windows es `far_diff_popup −1,0` y en la capa `overlay` de `degraph`
  el `depth_offset`.
- Añadido: `empuje_fondo` / `empuje_frente`, en **puntos de panel** (positivo = hacia fuera), teclas
  `h`/`l` y `y`/`o` de 0,5 en 0,5, topados a ±12 (el tope oficial hacia fuera es 12,67). Van por los
  huecos que ya tenía `tun7` (`z` y `w`), así que **el bloque de parámetros sigue en 320 bytes** y el
  diseño de memoria no se tocó; comprobado otra vez sobre el SPIR-V compilado (desplazamientos
  0…304, último miembro en 304).
- Detalle que importa: el lado no se conmuta de golpe en el plano de apoyo, se **interpola en una zona
  suave de 1,5 puntos** (`smoothstep` sobre el desplazamiento ya calculado). Con un salto duro, un
  objeto que cruza el plano se parte en dos y la costura se ve; así solo cambia el plano al que va
  cada capa. El empuje se suma **antes** de los topes por ojo, que siguen mandando.
- Para no caer en la trampa de que una tecla bajando a cero «reviva» el valor de `serve.conf`, el
  ajuste lleva un `capaTocada`: en cuanto se pulsa una tecla de capas manda lo del visor.
- Sin tocar lo conseguido: con los dos empujes en 0 (o sin escribir la línea) el cuadro sale idéntico
  al de v0.45; la rama del conversor, la red, la trama y las rampas no se mexieron.

## Qué hace el deslizador de profundidad del Nintendo 3DS (investigación, fuentes duras)
- **El deslizador no toca la óptica.** Ruta real: potenciómetro → MCU por I2C (`mcu::HID`, comando
  `0x00070000 GetRaw3DSliderPosition`, un byte) → el módulo HID lo normaliza a float y lo publica en
  la memoria compartida en `0x1FF81080` (0,0 = 3D apagado · 1,0 = tope) → el juego lo lee
  (`osGet3DSliderState()`) y **escala su propia disparidad**. No hay recorte duro por hardware: si el
  juego se pasa, se pasa. (3dbrew: MCU_Services, Configuration Memory; libctru `include/3ds/os.h`)
- En los emuladores pasa lo mismo: en Citra y Azahar el «factor 3D» **solo alimenta** ese valor de
  memoria compartida (`Settings::values.factor_3d` → `SharedPage::Set3DSlider(v/100)`); ningún shader
  de presentación tiene un uniform de profundidad. Azahar permite 0–255 (hasta 2,55) con aviso de que
  eso en una consola real no existe: poder pasarse es consecuencia de que el valor sea puro software.
- **La barrera sí se mueve, pero por otro camino** y no la controla el deslizador: máscara de 12
  unidades con bit de polaridad, rendija abierta 6/12 (mitad), posición en pasos de un iod/12 con
  iod = 62 mm (unos 5,17 mm por paso, 12 posiciones), escrita por un expansor I2C→paralelo TI
  TCA6416A y reescrita **unas 100 veces por segundo alternando el patrón y su complemento** (así se
  evita polarizar en continuo esa capa). El «Super Stable 3D» de la New 3DS mueve la **posición de la
  barrera** siguiendo los ojos (cámara interna 320x240 a 30 fps + giroscopio para predecir), no toca
  la imagen que renderiza el juego. (3dbrew: LCD_Registers `0x10202000`/`0x10202004`, QTM_Services;
  libctru `qtm.h`)
- Apunte honrado: no apareció ninguna fuente primaria del porqué la pantalla se atenúa al subir el
  deslizador; lo único confirmado es el coste fijo de tener 3D puesto (rendija al 50 % ⇒ alrededor de la mitad
  de la retroiluminación). Tampoco existen `SecureGet3Dn2D` ni `GET3DFEATURE` en Config Services (el
  comando 9 de `cfg:u` es el código de país), ni hay un «modo mini» anti-mareo documentado.
- **Qué nos lleva a nosotros**: (1) el parecido con lo que acabamos de hacer es exacto —el Hub de
  Windows y el 3DS exponen un multiplicador de disparidad por software, y los dos se apoyan en un
  presupuesto duro; nuestro tope por ojo (+12,67 / −9,98 puntos) es lo que al 3DS le falta; (2) la
  idea de mover la barrera según la cabeza es literalmente lo que hacemos con la fase de la trama y el
  rastreo IR; (3) alternar dos fases cuadro a cuadro es lo que hace el 3DS con patrón + complemento, y es
  el argumento a favor del `fase_temblor` que ya tenemos; (4) el 3DS guarda un retardo explícito de
  conmutación 2D↔3D (`cfg 0x00050006`) en vez de cambiar de golpe: imitable con una rampa global 0..1.

## Dos mandos separados: hundir el fondo sin tocar los primeros planos (v0.45)
- Falta que él señaló: para «mandar unos elementos al frente y otros al fondo» el conversor solo
  tenía **una** escala de relieve para toda la imagen. Ahora el desplazamiento se estira **por
  separado a cada lado del plano de apoyo**: `fondo` (lo que queda detrás de la pantalla) y
  `frente` (lo que ya sale delante). En `weave.frag` es el hueco nuevo `tun7.x`/`tun7.y`, aplicado
  **antes** de los topes oficiales por ojo, así que por mucho que se suba no nos salimos del
  presupuesto de Samsung (+12,67 puntos afuera · 9,98 dentro sobre 3840).
- De dónde sale la idea: la configuración descifrada del conversor de Windows
  (`/run/media/rodolfo/.../AnalisisOdyssey/conv-config.json`) ya lo hace, y lo hace por **bandas de
  profundidad**, no por clases de objeto: `scene_dspr` trae `near_diff_scale −0,5` ·
  `mid_diff_scale 0,0` · `far_diff_scale 1,0` · `far_diff_popup −1,0`, y el bloque `camera` los dos
  signos `dspr_scale(−) = −1` / `dspr_scale(+) = 1`. En el Hub son dos mandos sueltos
  (`3D Depth` 0–100 y `3D Popout` −50…+50; en su equipo 1,7 y 1,0, según
  `configs/hub-usuario/OverlaySettings.json`) y ahí `Depth` mueve el plano de parallaje cero, que
  es lo que ya hace nuestra «3D Profundidad».
- Números de la fórmula (aritmética del shader, no hizo falta montar sesión): con «3D Salida» = 50
  la escala es **22,66 puntos por unidad de mapa**, así que un mapa que barre todo el 0–1 ya da
  −11,3 / +11,3 puntos en sus extremos y **toca los topes** (9,98 dentro, 12,67 fuera) con las
  llaves en 1. Consecuencia práctica escrita en `serve.conf`: lo útil de `fondo`/`frente` está entre
  **0,5 y 1,2**; pasar de ahí solo pega más puntos al tope y deja escalones en el fondo. Y para
  sacar algo hacia afuera primero se baja «3D Profundidad» (mover el plano de apoyo) y después se
  sube `frente`.
- Teclas `j` `k` (fondo) y `u` `i` (frente), de 0,05 en 0,05, con los dos números impresos en cada
  golpe; por escrito `fondo = ` y `frente = ` en `serve.conf` (dejados comentados, con la explicación).
- Bloque de parámetros de la trama: de 304 a **320 bytes**. Comprobado sobre el SPIR-V ya compilado,
  no de memoria: el último miembro del bloque está en el desplazamiento **304**, o sea 320 de tamaño,
  que es lo que asigna y sube `vkweave.cpp`.
- Sin tocar lo conseguido: con las dos llaves en 1 (o sin escribir la línea) el cuadro es idéntico
  píxel a píxel; red, trama, rampas y relleno de huecos no se mexieron. Instalado 0.45 y servicio
  reiniciado con `rampa_x −0,0040` / `rampa_y +0,0016` intactos y la red de Samsung por delante.

## RONDA C: el paso Y la inclinación escritos ya coinciden con los de Windows (medido sobre el cuadro)
- Sesión de medición disparada sin tocar el teclado (`auto = mpv:conv` en `serve.conf` + un clip
  propio de 3840x2160 y 60 fps, barrido lento sobre un fondo liso), volcado del cuadro escrito en
  `/tmp/rondaC-trama.png` y `tools/paso_rejilla.py` encima:
  - canal rojo: kx **+0,4503** · ky **−0,1606** · periodo horizontal 2,2208 px = **0,34511 mm**
  - canal verde: kx **+0,4502** · ky **−0,1606** · 2,2210 px = **0,34514 mm**
  - objetivo (foto de Windows): 0,4501 · −0,1607 · 0,34530 mm
- Con `rampa_x = −0,0040` y `rampa_y = +0,0016` el batido horizontal queda a **833 mm** (rojo) y
  **1048 mm** (verde) y el vertical ya ni aparece: las franjas diagonales de la ronda B eran el
  desajuste de inclinación (faltaban 0,0020 ciclos por píxel → bandas cada 78 mm).
- Secuencia de los tres pasos escritos, medidos sobre el cuadro real: **0,34836 mm** (ronda A, rampa 0)
  → **0,34537 mm** (ronda B, `rampa_x −0,0040`) → **0,34511 mm** (ronda C, igual + `rampa_y +0,0016`).
- La sesión dejó el panel como estaba (`[panel] restaurado: modo 2 3840x2160@164.98, escala 1.5`) y el
  rastreo ya leyó **4261 fotogramas a 48,8 fps** dentro de la sesión (antes: 0).
- Lo que falló en esa sesión fue la **cara**: `sin cara` en los 4261 cuadros. La foto de lo que ven las
  dos vistas (`/tmp/ir-rondaC.png`, sacada con ffmpeg sobre `/dev/video0`) explica por qué: la cabeza
  entra **recortada por el borde izquierdo** de las dos vistas; la cara está fuera del campo de las
  cámaras IR, que miran rectas al frente. No es un fallo de software ni de umbral (YuNet a 448x320,
  umbral 0,6, sin recorte de búsqueda): hace falta sentarse centrado bajo la cámara del monitor.
  Se comprueba sin montar ninguna sesión con `odyssey3d ctl track`.

## La cámara IR: por qué no llegaba un solo cuadro, y qué hacemos ahora si se cuelga (v0.44)
- Síntoma de las últimas sesiones: `[rastreo] … 0/0 cuadros (0.0 fps) … calibracion: sin SR+CALCAM.bin en .`
  o sea la cámara se abría pero **nadie leía los fotogramas** y el rastreo usaba la pose de fábrica.
- Causa: a esta unidad le faltaba la **calibración de fábrica de la cámara** en disco. El hilo del
  rastreo la necesita para triangular los dos ojos y, si no está, se salía. La leímos del monitor
  (`odyssey3d ctl calib --out …`, puerto `/dev/ttyACM0`) y la dejamos en
  `/usr/local/share/odyssey3d/calib/` y `~/.config/odyssey3d/calib/`: `SR+CALCAM.bin`,
  `SR+CALET.bin`, `SR+CAL3D.bin`. Datos de esta unidad: `fx 474,32 / 476,22`, `cx 322,24 / 321,58`,
  `cy 242,11 / 249,17`, separación entre las dos vistas **69,97 mm**, cámara→panel
  `X 3,60 · Y −17,96 · Z 0,41 cm` con giros de media décima de grado.
- Comprobado en el código: **nunca mandamos un reset de USB** (no hay `USBDEVFS_RESET` ni equivalente).
  Eso es lo único que tumba del todo la cámara del monitor, y solo se levanta apagando y encendiendo
  el monitor; no lo hacemos ni lo haremos.
- Dos cosas que sí arreglamos, para que la cámara no se quede «agarrada»:
  - Si el rastreo no puede empezar (falta calibración o falla el rastreador), **soltamos el flujo**
    (`stop` + `close`) en vez de dejarlo montado sin lector. Un flujo montado y no leído es justo lo
    que revienta el `S_FMT` de la `04e8:20d7` en la sesión siguiente.
  - Si en medio de la sesión dejan de llegar fotogramas (**3 segundos** sin ninguno), cerramos y
    **reabrimos el dispositivo con el mismo modo** que funcionaba (MJPG 1280x480 @ 60 en el monitor),
    con 2 segundos de espera entre intentos. En el log se ve como `[rastreo] camara: flujo reabierto`.
    Si aun así no vuelve, la nota dice por qué.
- Fuera de sesión la cámara responde bien: `odyssey3d ctl camera --size 1280x480 --format MJPG` saca
  5 de 5 fotogramas (~224 KB cada uno). Con la calibración ya en disco, la próxima sesión debería
  imprimir `N/N cuadros` en vez de `0/0` y la trama se moverá con tus ojos.

## El paso escrito en una sesión de verdad: 0,3484 mm → 0,3454 mm (medido sobre el cuadro, v0.43)
- Medido sobre el **cuadro que escribió el visor en una sesión real** (volcado Vulkan del panel,
  `tools/paso_rejilla.py`), no sobre el cuadro de laboratorio:
  - **Ronda A** (`rampa_x = 0`): kx **+0,4461** · ky −0,1591 · 2,2417 px = **0,34836 mm** ·
    fuerza/piso 383. Contra el blanco de Windows (0,4501) el batido cae a **249 px = 38,8 mm**.
  - **Ronda B** (`rampa_x = −0,0040`): kx **+0,4500** · ky −0,1587 · 2,2225 px = **0,34537 mm**.
    El batido horizontal se va a más de un metro, o sea: desaparece.
- El cuadro de laboratorio con `rampa_x = 0` sale kx 0,4473 y el de sesión real 0,4461. La diferencia
  (**−0,0012**) **no viene del relieve**: con el relieve apagado el cuadro sale idéntico. Viene de los
  **mapas de corrección** de esta unidad, que curvan la fase y estiran el paso. Por eso la llave se
  ajustó con dos fotos de la pantalla y no con la cuenta del laboratorio: en esta unidad el valor que
  clava el paso es **−0,0040**, no −0,00283. Respuesta otra vez 1:1 (−0,0040 mueve +0,0039).
- **Queda el desajuste de inclinación**: nuestro ky escrito en sesión es **−0,1587** y el de la foto de
  Windows es **−0,1607**. Ese Δ de 0,0020 da un batido de **500 px de panel = 78 mm**, franjas casi
  horizontales (2,9 grados de vuelta) repartidas por todo el panel, que es lo que el usuario describe
  como «muchas franjas ligeramente inclinadas a lo largo del panel». Regla medida en el laboratorio con
  el cuadro sólido: **ky nuevo = ky viejo − rampa_y** (con +0,0020 → −0,1618, con −0,0020 → −0,1578),
  respuesta 1:1 y sin tocar kx. Para pasar de −0,1587 a −0,1607 hace falta **`rampa_y = +0,0016`**.
  Caveat: el ky de Windows sale de una foto con escala 1,159 (±1 % → ±0,0016), así que el valor bueno
  lo cierra la pantalla, no la cuenta.
- **Teclas nuevas para afinar el paso a ojo, sin reiniciar nada**: `[` y `]` mueven `rampa_x` y `;` y `'`
  mueven `rampa_y`, **0,0004 ciclos por pixel de panel** cada pulsación, **sumando** a lo que diga
  `serve.conf`. `8` las pone a cero (vuelven a la config). Cada pulsación imprime en el registro la
  línea `[rampa_x +0,0004 ...]` con el valor actual, y el `ODYSSEY_UBO` imprime la rampa final, así que
  el cuadro volcado queda etiquetado solo. Están en `WeaveTuning::rampaDX/rampaDY` y se suman a la
  rampa del entorno antes del tope ±0,05, en las dos copias de la trama (Vulkan y GL).

## La red de Samsung no reconoce objetos ni personas (medido sobre el fichero, v0.43)
- `mde_large.onnx` (49,5 MB; también está `mde_large_fp32.onnx`, 98,9 MB) es un **transformador de visión
  con cabezal DPT**: `patch_embed.proj.Conv` + `pos_embed` + `encoder.blocks.N` con `LayerNormalization` y
  atenciones fusionadas (`com.microsoft Attention`, `attn_fused_N_qkv_w/_b/_ctx`), y detrás
  `model/model/decoder/` con `refinenet4.resConfUnit2.conv2`, `Resize` y `Reshape`/`Slice`. Eso es
  un estimador **denso de profundidad por parches** (DPT: fusiona cuatro niveles del transformador),
  **una sola salida**, igual de familia que nuestra `depth_anything_v2_small.onnx`.
- Buscado en el fichero (`strings`) cualquier rama de `seg|cls|class|person|semantic|mask|label`:
  **no hay nada**. No hay máscaras, ni clases, ni etiquetas, ni una capa «frontal» aparte del fondo.
  La idea de «identifica personas y las pone en el plano de delante» no está en el modelo: lo que hace
  es escupir un mapa de profundidad completo cada cuadro, y el tramado decide el desplazamiento.

## La rejilla que escribiamos estaba un 0,6 % desviada del paso real de la lente (medido, v0.42)
- Herramienta nueva `tools/paso_rejilla.py`: FFT con ventana de Hann de 512 sobre cuatro recortes
  del **cuadro escrito** (no de una foto), pico afinado por centroide. Da el paso en pixeles de
  panel y en milimetros, la inclinacion, la fuerza frente al piso, y el batido contra el blanco.
- Blanco (lo que escribe el conversor de Windows, medido sobre su foto con escala 1,159):
  **kx 0,4501 · periodo 2,2217 px = 0,34530 mm · inclinacion -19,0 grados**.
- Lo nuestro hasta ahora: **kx 0,4473 · 2,2358 px = 0,34744 mm**. 0,62 % mas largo, o sea dos tramas
  que baten a `1/0,0028 = 353 px` de panel = **54,9 mm**: franjas verticales de cinco centimetros y
  medio, que es exactamente la magnitud de las franjas que el usuario ve y no se van con la fase.
- Llave nueva **`rampa_x`** (variable `ODYSSEY3D_RAMPA_X`, ciclos de porteadora por pixel de panel,
  tope ±0,05): rampa limpia de fase **igual para los tres canales**, metida dentro del `fract()` de
  la fase en `shaders/weave.frag`. Con `rampa_x = -0,00283` el cuadro escrito queda
  **kx 0,4501 · 2,2219 px = 0,34529 mm** clavado al blanco, y la inclinacion sin tocar (-0,1598).
  La fuerza de la porteadora no baja (437 → 429 sobre el piso). Respuesta medida 1:1: cada 0,001 de
  rampa mueve 0,001 de kx.
- El signo va al reves del ajuste que se busca: la porteadora que sale por la pantalla es el espejo
  de la escrita (`kx medido = 1 - kx escrito`), asi que para subir el kx medido hay que restar rampa.
- **`paso` (la escala "--pitch") no sirve para esto**: cuadra el paso igual (0,995 -> kx 0,4500)
  pero escala la rampa de la lente entera, y en esa rampa van metidos los desfases entre los canales
  rojo/verde/azul; afloja la inclinacion (ky -0,1589 en vez de -0,1598) y corre los canales. Se queda
  en 1 y se usa `rampa_x`.
- Rectificacion de la ronda anterior, que escribimos aqui mal: con `paso = 0,995` la inclinacion NO
  se caia a -9,7 grados. Eso lo decia el estimador por bloques (`tools/portador_windows.py`), que con
  el pico ya movido salta de un maximo al de al lado; la FFT directa dice -19,5 grados. Por eso nacio
  `paso_rejilla.py`, que es el que hay que usar con cuadros escritos. Con foto sigue valiendo
  `portador_windows.py`, que trae la correccion de escala.
- `rampa_y` es la misma llave en vertical. Con -0,0012 baja la inclinacion de -0,1598 a -0,1585, o
  sea que para subir a -0,1607 (el valor de la foto de Windows) tendria que ser unos +0,0009. La
  diferencia nuestra con ese valor da un batido de 17 cm, demasiado gordo para verlo: se deja en 0 y
  se mira primero el efecto de `rampa_x`.
- El servicio imprime al arrancar que numeros de trama cargo (`[serve] trama cruda ...`), para no
  adivinar si la linea de `serve.conf` entro o se la comio un tope de rango.
- Falta lo unico que falta: **una foto por valor en el panel** (rampa 0 y rampa -0,00283, mismo
  encuadre que «conversor2») para confirmar que el batido de 55 mm desaparece de la pantalla.

## En zonas llanas nuestra pantalla lleva cuatro veces mas grano que la de Windows (medido, v0.41)
- Herramienta nueva `tools/rejilla_en_llano.py`: toma baldosas de 160x160 con brillo medio y
  **desviacion tipica menor de 8 niveles** (color de verdad parejo) y mide dentro de ellas cuanto
  reencaje de color, cuanto grano cuadro a cuadro y que periodicidades quedan.
- Comparacion honesta, con las dos fotos a la misma escala (1,166 px de panel por px de foto el
  nuestro, 1,159 el de Windows):
  - grano (salto horizontal medio dentro de lo llano): **nosotros 4,41 niveles · Windows 0,95**
  - banda fina (1,4-6 px de foto): pico/piso **40,7 a 6,1 px de panel** nosotros · **19,0 a 6,2 px** ellos
  - banda media (6-20 px de foto): **44,2 a 9,1 px de panel (1,41 mm)** nosotros · **4,7** ellos
- Ojo con la trampa: la foto de Windows es un videojuego (zonas llanas sinteticas, limpias) y las
  nuestras son un video comprimido con su propio grano, asi que parte del 4,4 puede ser del video.
  Lo que si vale es que el pico fino sale a 6,1-6,2 px de panel en las dos (eso es panel + lente,
  comun) y que en nosotros la banda de 1,41 mm esta unas diez veces mas fuerte.
- En el CUADRO ESCRITO (volcado `/tmp/faseA-trama-vulkan.png`, antes de la lente y de la camara) las
  zonas mas llanas tienen reencaje verde-rojo de **solo 1,32 niveles de desviacion** y la porteadora
  en brillo apenas 2,7 veces el piso: el visor NO estampa una rejilla fija donde los dos ojos ven lo
  mismo (era la sospecha del conversor 2D a 3D). La rejilla solo aparece donde hay diferencia entre
  ojos, que es como debe ser.
- Correccion a la nota de abajo: la diferencia de paso contra Windows (0,6 %) no es despreciable, es
  un batido de **352 px = 54,8 mm** a traves de la pantalla, que es del orden de las franjas de color
  que se ven en las fotos nuevas. Hay un boton para probarlo en vivo: `paso =` en serve.conf
  (`--pitch`, escala de la rampa de fase) y las teclas RePag/SavPg, que lo mueven de a 0,0002.
  0,6 % son 30 pulsaciones. La rampa escrita (0,55394) esta por encima de Nyquist, asi que lo que
  sale por la pantalla es su espejo: kx emitido = 1 - rampa + pendiente de los mapas = 1 - 0,55394 +
  0,00124 = 0,4473, que es justo lo medido. Para que salga 0,4501 hace falta rampa 0,55114, o sea
  **paso = 0,995** (por debajo de 1, no por encima). La barrida con RePag/SavPg confirma el sentido.

## La nuestra y la de Windows escriben la MISMA rejilla (medido, v0.41)
- Llega la ronda 3 de Windows con la sospecha de que Linux pinta una "rejilla virtual" propia y que
  por eso los bordes diagonales se rompen en ciertas secciones. Se mide con el estimador de ellos,
  replicado en `tools/portador_windows.py` (bloques 512, Hanning, banda radio 0,045 alrededor de
  (0,4501, -0,1607), piso anular 0,14-0,26, mas controles angular y de periodo).
- Cuadro: `odyssey3d weave --solid --out /tmp/trama_solida.png` (la trama real de este visor, con los
  mapas de correccion de la unidad cargados; no toca el panel).
- Porteadora nuestra medida = **kx 0,4473 +- 0,0005 · ky -0,1598 +- 0,0005 · angulo -19,7 grad ·
  periodo horizontal 2,2358 px = 0,34744 mm · perpendicular 2,1049 px = 0,32710 mm**.
- Porteadora de Windows = kx 0,450 +- 0,006 · ky -0,1607 · angulo -19,6 grad · horizontal 2,222 px =
  0,34530 mm · perpendicular 2,0923 px = 0,32515 mm. Diferencia: 0,6 % en el paso y 0,1 grad.
- Consecuencia: el "batido de 9,63 px = 1,497 mm" de la ronda 3 compara el valor **escrito** en el
  shader (0,55394, por encima de Nyquist) con el que **sale** por la pantalla (0,4461, su espejo).
  El batido entre las dos maquinas es 1/0,0027 = **370 px = 57,5 mm**, no 1,5 mm. (Ahi escribimos
  "no hay error de paso que corregir": hay que leerlo al reves, ese 0,6 % es un batido de 5,5 cm que
  cruza la pantalla de arriba abajo y si se puede probar, ver la nota de arriba.)
- Rigidez de la nuestra, en la misma rejilla de bloques: kx local **0,4473 +- 0,0000** en los 28
  bloques, o sea el paso es constante en toda la pantalla; la fase se desvia de una onda plana
  **21 grad de mediana y 85 grad peor** (los mapas A/B la doblan). Ellos reportan 141 grad de
  dispersion para la suya, medida sobre una fuerza de 0,98 niveles; la nuestra sale con fuerza
  259,2 +- 2,7 niveles en el cuadro rojo/verde. No son comparables tal cual.
- Control sin trama (fondo plano, sin disparidad): fuerza **< 3** y amplitud **0,00 niveles**. O sea
  este visor **no pinta rejilla donde no hay disparidad**; Windows si ve 0,98 niveles sobre blanco.
  Eso apunta a que los mapas de correccion de ellos mueven la **ganancia** por subpixel, no solo la
  fase, y oscurecen un 5 % de la imagen.
- Pendiente y no hecho a proposito: una fuerza comparable pide un cuadro plano con relieve suave (no
  el salto rojo/verde), y el A/B de `fase_temblor` necesita una sesion que tome el panel.

## Camara IR 04e8:20d7: se levanta, se cuelga, y un reset USB no la revive (hoy)
- `lsusb -d 04e8:20d7 -v` la ve (UVC 1.00, High Speed) y `v4l2-ctl --list-formats-ext` en
  `/dev/video0` lista **MJPG 1280x480 @60 · MJPG 640x480 @60 · YUYV 640x480 @30 · YUYV 1280x480 @15**.
  Confirma lo de Windows: a 1280x480 en crudo (YUYV) solo hay 15 fps, y `/dev/video1` es el nodo de
  metadatos (no lista ningun formato).
- Pero `VIDIOC_S_FMT` contesta **EIO** ("Input/output error"): la interfaz de flujo esta colgada.
  Se pidio un reset USB (`USBDEVFS_RESET` sobre `/dev/bus/usb/005/015`) y el resultado fue peor:
  el kernel encadena `device descriptor read/64, error -110` y la camara **desaparece del bus**; ya
  no hay `/dev/video*`. Hace falta apagar y encender el monitor. Leccion para los dos lados: con la
  interfaz UVC colgada, el reset de USB no la levanta; hay que cortarle la alimentacion.

## Las franjas son armonicas aliasadas de la propia trama (v0.41)
- Reportado despues de la v0.40: las franjas **no desaparecen** bajando `3D Salida` a 0, y los bordes
  de los objetos se ven rotos, con grietas y raspon.
- Medido con `volcado = /tmp/chk` y `tools/mide_franjas.py` sobre el cuadro tramado de verdad. La
  porteadora de la trama leida del PNG sale en 0,4461 ciclos/px (2,24 px, el espejo del `kx` de
  fabrica 0,55394), y encima hay tres picos que antes no se habian mirado: **4,3 px con realce x4,4,
  8,2 px con x4,3 y 10,9 px con x2,9**, de 0,4 a 1,7 niveles de gris. Con el paso de pixel del panel
  (0,1554 mm) son **0,67 · 1,27 · 1,67 mm**: las franjas que se ven.
- De donde salen: se reprodujeron con la formula sola del shader (`fase = fract(...)`, peso
  triangular con pendiente 10) muestreada a pixel de panel, y los picos caen en **4,34 · 8,17 · 10,73
  · 4,97 · 3,24 px**, o sea la 5a, 7a, 9a y 11a armonica de `kx` plegadas por el muestreo. No es el
  paso de la lente torcido ni el mapa: es que el paso de una vista a la otra es casi un escalon. Con
  pendiente 10 esas armonicas suman el 134 % de la fundamental; con pendiente 3, el 26 %.
- Por eso no se van con `3D Salida = 0`: `gananciaRelieve(0)` no es 0, es 0,00104, que son **4,0 px
  de separacion entre ojos** (dos periodos y medio de lente). Mientras las dos vistas no sean
  identicas la trama tiene de que hacer las rayas.
- Los bordes rotos salen del mapa: 230 niveles, meseta mediana de 30 px de panel, y **salto mediano
  entre mesetas de 255 niveles** (el corte de plano + dilate lo dejan casi binario). Con el tope de
  Samsung de 22,65 px de recorrido, un nivel del mapa son 0,089 px = 0,049 ciclos de trama, asi que
  el borde salta de golpe de una meseta a otra; y `cantoI` recorta hasta el 90 % del relieve en los
  cantos filosos.
- Añadido para poder probar esto sin recompilar, las cuatro van en `serve.conf` y se pasan por
  entorno al visor: `relieve` (0 = plano del todo, para el A/B), `temblor` (apaga el temblor del
  desplazamiento), `canto` (mas ancho = bordes sin mordeduras) y `fase_temblor` (temblor de la fase
  de la trama en ciclos; el temblor de fabrica mueve el desplazamiento del contenido, no la fase, por
  eso no rompía las franjas).
- Añadido `tun6` al bloque de la trama (`shaders/weave.frag`, UBO de 288 a 304 bytes,
  `ODYSSEY3D_FASE_TEMBLOR`): la fase de cada punto tiembla un numero fijo de ciclos con el mismo
  ruido para los tres canales, para no ensuciar el color. Medido sobre la formula: con 0,1 ciclo la
  armonica de 4,34 px baja de 0,087 a 0,012 y la de 8,17 de 0,069 a 0,020, y la fundamental pierde
  solo un 7 %. Es lo que hace el filtro de Samsung, por eso en Windows no se notan.
- `tools/mide_franjas.py`: nuevas `--con` / `--sin` para comparar los dos PNG del mismo cuadro, y
  funcion nueva de porteadora local (bajar la rayita al cero, amplitud mediana, golpes de fase por
  mil pixeles y separacion).
- Arreglado: `-trama-sin-releve.png` no salia. El bloque estaba en el vuelco del cuadro 4, que en
  ese momento va por la rama de lado a lado y `converterOn()` era falso. Ahora la pareja
  `-trama-con-mapa.png` / `-trama-sin-releve.png` se guarda en el cuadro 8, forzado el conversor
  fuera y dentro con `setConverter(gan, ref, false/true)`, asi que son el MISMO cuadro.

## Franjas verticales y los mapas de correccion de ESTA unidad (v0.40)
- Reportado: con el 3D puesto quedan unas franjas verticales claras y oscuras por toda la pantalla,
  separadas entre si un milimetro o tres; con el mismo video en Windows no se perciben.
- Preguntado en la pantalla, para no adivinar: siguen al video (se mueven con los objetos), se
  deslizan al mover la cabeza y desaparecen con `3` (2D plano). Esas tres respuestas descartan la
  rejilla del panel y el escalado, y apuntan al mapa de profundidad antes que a la trama: una raya
  cada T pixeles por desajuste del paso sale con `T = 1/(0,554 * delta)`, o sea habria que llevar el
  paso un 1,8 % torcido para rayas cada 100 px, y esas rayas quedan FIJAS en el panel, no se deslizan
  con la cabeza. Las curvas de nivel de un mapa de profundidad mal temblado hacen lo contrario: se
  van con el video y con la cabeza.
- Medido aparte, y era un fallo real: los mapas de correccion que llevamos instalados NO son los de
  este panel. Comparados con los volcados de `C:\ProgramData\Simulated Reality\Devices\` de la unidad
  `VN022514ET0194` (480x270, misma pinta, tamano identico):
  * A instalado 1..57 media 31,2 frente al de la unidad 0..255 media 23,5 · correlacion 0,39 ·
    diferencia media 10,2 niveles y maxima 237.
  * B instalado 120..142 frente al de la unidad 117..136 media 127,2 · correlacion 0,83 ·
    diferencia media 1,3 niveles.
  * traducido a fase de trama: error medio 0,040 ciclos (p95 0,086, maximo 0,929), o 0,072 px de
    desplazamiento de la trama (p95 0,156, maximo 1,68 px sobre un periodo de 1,805 px: casi un
    periodo entero). El error gordo esta localizado en filas 194..213 y columnas 0..16 del mapa, que
    es la esquina inferior izquierda del panel. Defecto que hay que corregir, pero localizado: no
    explica franjas en toda la pantalla.
- Como quedo:
  * el visor busca `~/.config/odyssey3d/calib` ANTES que los mapas instalados (los de cada unidad
    valen solo para su panel). Ahi quedaron copiados los de esta unidad, y en `calib/` del repo
    tambien; los de la otra unidad siguen ahi como `3DStackCorrection_*.de-otra-unidad.png`.
  * la salida del visor dice de donde salieron los mapas y como son, porque cargar los de otro panel
    es invisible a simple vista:
    `[correccion] de /home/rodolfo/.config/odyssey3d/calib/3DStackCorrection_A.png y ...B.png`,
    `[correccion] mapa A: 0..255 media 23,5`, `[correccion] mapa B: 117..136 media 127,2`.
  * `serve.conf` admite `fase`, `paso`, `filtro` y `calib`, y se pasan al visor (`--align`, `--pitch`,
    `--filtro`, `--calib-dir`). Antes el filtro solo se movia en marcha con `,` `.`, asi que habia que
    recompilar o teclar cada vez para dejar la pendiente puesta.
  * `volcado = /tmp/chk` en serve.conf deja la sesion en marcha y guarda `chk-trama-vulkan.png`,
    `chk-profundidad.pgm`, `chk-chico.ppm` y `chk-trama-con-mapa.png`. Para eso hay opcion nueva
    `--volcado-seguir`: sin ella el volcado cerraba el visor a los cuatro fotogramas.
  * el volcado guarda ademas `chk-trama-sin-releve.png`, la misma trama con el conversor apagado,
    sacada en el mismo instante (metodos nuevos `converterOn/Gain/Ref` en `VkWeave`). Sin esa
    segunda imagen no hay forma de separar una raya que pone la trama de una que pone el mapa: en
    el panel las dos se ven igual.
  * `tools/mide_franjas.py /tmp/chk` lo traduce a numeros: el periodo de cada franja en pixeles y
    su amplitud, si aparece solo con releve (sale del mapa) o en las dos tramas (sale de la trama),
    el porteador de la trama (2,24 px leidos, que es el espejo de 0,55394 ciclos por pixel) y de
    cuantas celdas son las mesetas del mapa de profundidad. Probado con unos volcados falsos
    (banda de 120 px solo en la trama con releve, mesetas de 16 celdas): el veredicto sale bien.
  * README: seccion nueva «Si quedan franjas verticales finas por toda la pantalla», con la prueba
    que separa los dos origenes en un minuto (bajar `3D Salida` a 0 con `Ctrl+Shift+5` si la raya
    sigue al video; mover `SavPg`/`RePag` si esta fija al panel).

## El boton 1 no devolvia a la red de Samsung (v0.39)
- Reportado: dentro del visor el cambio de red salia de la de Samsung a la otra, pero no de vuelta, y
  la otra red estaba nombrada de un modo confuso.
- Medido con tools/prueba_idayvuelta.cpp (ida y vuelta de las dos redes, sin tomar el panel): la red de
  Samsung ocupa 678 MB de memoria de grafica (la grafica 0 paso de 1667 a 989 MB libres) y la otra unos
  260 a 400. Al soltar una sesion no devuelve toda: se quedan unos 300 MB del contexto de CUDA y de
  cuDNN que el proceso ya pago. El umbral viejo era el mismo para las dos y pedía 1550 MB libres, de
  modo que al pedir la Samsung con 1171 MB libres (03:55:45) o con 1522 MB (03:56:59) la respuesta era
  no abrirla: se iba a la CPU, y la red de Samsung por CPU ni se intenta (se cuelga mas de 4 minutos y
  aborta). El boton volvia a la red anterior, y esa tambien se reabria por CPU: 179 ms por mapa en lugar
  de 16. Es el "179,6 ms con CPU" que se lee en el registro de las 03:56.
- Como quedo: el piso de memoria depende de la red (Samsung 700 MB mas 300 de colchon, nativa 480 mas
  300) y el colchon se mueve con ODYSSEY3D_COLCHON en MB, sin recompilar. Comprobado con la misma
  prueba: Samsung 11,3 ms, nativa 15,6 ms, Samsung otra vez 11,3 ms, las tres veces por la tarjeta.
- La red que no es la de Samsung se llama NATIVA en todos lados: el boton dice "Red 2 · nativa", el
  aviso del visor dice "red 2 (nativa) · NVIDIA" y el arranque del servicio tambien lo dice. El archivo
  sigue siendo depth_anything_v2_small.onnx; renombrarlo a nativa.onnx no cambiaria nada del manejo.

## El click en la barra del video y la cara de 140 mm (v0.38)
- El otro fallo de sesion reportado: durante el 3D no habia forma de picar la barra de tiempos de abajo.
  La causa esta en nuestro propio truco de apilamiento. Para que un click en el video no suba el
  navegador por encima de la trama le ponemos «debajo de todo», pero ese debajo manda al video al fondo
  del apilamiento COMPLETO: con otra ventana normal abierta (medido: la ventana maximizada del lanzador
  de juegos, posicion 9 de 10) el click que el visor deja pasar se lo queda ESA ventana y el video no se
  entera de nada. Se veia el video perfecto, porque la imagen la sacamos de la ventana y no de la
  pantalla, pero el click caia en otro lado.
- Como quedo: mientras dura la sesion bajamos tambien a las demas ventanas, asi el video queda arriba
  dentro del grupo de abajo (es la ventana activa, o sea la ultima de su grupo) y el click llega a su
  barra de tiempos. Lo revisamos cada cinco segundos por si abren algo a mitad de pelicula. Al terminar,
  cada ventana vuelve a su lugar: si alguna tenia «siempre encima», se lo devolvemos. Medido en la sesion
  real de las 03:55 con `ctl apilamiento`: cinco ventanas por debajo del navegador y el navegador en la
  posicion 6 de 11, o sea el primero que recibe el click; el aviso nuevo dice «6 ventana(s) mas pasaron
  por debajo del visor». Si el servicio se muere de una muerte brusca y alguna ventana se queda abajo,
  `odyssey3d ctl ventana libre --uuid ...` la suelta.
- Un arreglo de la v0.35 que no llegaba a ninguna sesion. Pusimos la cara de 140 mm (el numero de
  Samsung) en el visor y en el rastreador, pero el hub lanza al visor con `--track-cara` y ese valor suyo
  seguia en 150: todas las sesiones reales seguian con 150 mm, o sea el +7 % de sesgo en la distancia que
  sacamos del ancho de la cara seguia ahi. Ahora el hub arranca en 140, y la linea de ayuda de
  `--track-cara` dice lo mismo.
- Con los dos arreglos de ventana puestos, la sesion de prueba no cerro la sesion: el video siguio con el
  estado de pantalla completa todo el tiempo, y el registro nuevo lo apunta («video: ... pantalla completa
  0,0 2560x1440 -> ... 3840x2160 (sesion de 3 s)»). O sea KWin agranda la ventana solo, despues de
  nuestro cambio de modo, y no hace falta pelearse con ella.
## El video volvia a su reproductor dentro de la pagina (v0.37)
- Lo que se veia: al aceptar la tarjeta encima de un video del navegador, el conversor arrancaba con el
  video **ya fuera de pantalla completa** (volvia al reproductor chico dentro de la pestana) y la trama
  corrida sobre la pagina entera. Y despues no se podia picar donde el video parecia estar.
- La causa era un arreglo nuestro. En `main.cpp` habia un reacomodo: si la ventana del video media menos
  que la salida (nuestro propio cambio de modo, 165 a 120 Hz y escala 1,5 a 1, la deja chica un instante)
  le **quitabamos** la pantalla completa y la volvia a poner para que KWin la estirara de nuevo. En un
  navegador ese va-y-ven hace que el navegador salga del video a pantalla completa, y como el estado de
  KWin queda puesto otra vez el vigilante no se entera: la sesion sigue y sigue sobre la pagina. El aviso
  viejo del journal lo describia: «(2560x1440 en una salida de 3840x2160, pantalla completa si): lo
  reacomodamos (1/8)», y salia justo antes de que se viera el video dentro de la pestana.
- Medido: probar con una consola a pantalla completa que ponerle «debajo» (lo que hacemos con el video
  para que un clic en la barra de tiempos no tape la trama) **no** le quita la pantalla completa, asi que
  esa parte estaba limpia. Y en el journal de la sesion real el flujo del navegador ya estaba en
  3840x2160 siete segundos despues del cambio de modo: KWin manda el tamano nuevo solo, hace falta
  pelearse con la ventana.
- Como quedo: al video solo se le devuelve la pantalla completa **si perdio el estado** (un pedido cada
  0,6 s, tres intentos en vez de ocho) y **nunca** se la quitamos. Si esta a pantalla completa pero
  todavia chico, se avisa una sola vez y no se toca nada: mientras la ventana vuelve a llenar la salida
  la trama se estira, que se ve algo menos nitido pero no roto y no rompe el video. Cada cambio de la
  ventana del video queda apuntado en el journal con su reloj («video: activa, pantalla completa 0,0
  2560x1440 -> ... (sesion de 4 s)»), para saber quien lo provoco la proxima vez.
- Un aviso que mentia: en la sesion del conversor el visor decia `[formato] lado a lado` siempre, porque
  se imprimia sin mirar. Ahora el conversor dice que la red decide cuanto desplaza cada ojo, y el aviso
  de «se vera doble y movido» solo aparece con 3D lado a lado o arriba y abajo de verdad. La pastilla
  tampoco lleva escrito «lado a lado» en una sesion de conversion.
## Relleno de huecos en el canto del objeto (v0.36)
- Frente elegido: que el canto de los objetos no se rompa ni babee. En el programa de Samsung eso lo
  hacen `warp.least_hole`, `warp.hole_edge` e `inpaint.push/pull/padtopot`; aqui son `src/hoyos.cpp`
  y `src/hoyos.h`.
- Como se hizo, y por que sale barato. Al subir el mapa se guarda **una version por ojo** en la misma
  textura (pasa de R8 a R8G8: un canal por ojo), asi que el shader sigue haciendo las mismas
  dieciseis lecturas y solo cambia `.r` por `.rg`. Lo guardado no es el mapa en crudo sino el mapa
  **puesto ya en el sitio de destino de ese ojo**: cada punto se esparce hacia donde cae, **gana la
  superficie mas cercana** (ahi esta la guarda que evita que el fondo se suba por encima del canto),
  y el hueco que queda detras se rellena interpolando entre los validos vecinos. Como el
  desplazamiento de la lente es solo horizontal, los huecos son horizontales: se rellena fila a fila
  y no hace falta la piramide push-pull de dos dimensiones ni dos texturas mas.
- Coste medido: **0,93 ms por mapa** con cuatro hebras (1,08 antes de quedarse los tampones entre
  cuadros). Ademas la red va en su propio hilo y el mapa se sube cada tres cuadros, asi que en el
  presupuesto de 8,3 ms por cuadro de imagen quedan unos **0,3 ms**. Sin tocar los 7,8 ms ganados en
  la v0.35.
- Cifras fuera del panel (`tools/prueba_hoyos.cpp`, escena sintetica de cantos duros, 518x518,
  ganancia 3,06 puntos de mapa = relieve de 50, plano de apoyo 0,50; la referencia es el esparcido
  directo con oclusion, que es lo que deberia verse):
  - huecos rellenados 4066 de 536648 puntos (**0,76 %**);
  - ojo izquierdo: error medio **1,714 -> 0,824** y errores gordos **4078 -> 2039**;
  - ojo derecho: **1,713 -> 0,852** y **4170 -> 2131**;
  - ancho del canto (distancia 10-90 de la rampa) ojo izquierdo **2,12 -> 1,26** puntos de mapa,
    que multiplicado por 7,4 son puntos de pantalla. Ojo derecho 0,48 -> 0,46.
- Lo que se descarto por medir menos de lo que cuesta: guardar en lugar del valor el **desplazamiento
  exacto** del punto ganador (ancho del canto 1,22 frente a 1,26, practicamente igual y mas riesgoso).
  Y la metrica de "cuantas lecturas caen al otro lado de un salto" resulto inutil (4170 -> 4182):
  castiga el fondo revelado correctamente, que esta bien que aparezca.
- Material de verdad, sin panel: con una captura del propio panel a 3840x2160 recortada y con la
  escena de la pelota, al relieve de fabrica el relleno toca **0,02-0,06 %** de los puntos y mueve el
  color de la vista izquierda en el **0,02 %** de los pixeles (mas de 24 de salto). O sea: quita el
  halo de un lado en los cantos mas duros, pero no es un cambio enorme con tan poco recorrido total
  (3 puntos de mapa de ida y vuelta).
- Y salio un numero que cambia el plan siguiente: midiendo el mapa que suelta la red de Samsung, el
  salto **maximo** del mapa es de ~10 puntos de 255 en 24 puntos de mapa (perfil de una fila
  cruzando el borde de un objeto). No hay saltos duros en el mapa, así que el canto se abre en unos
  180 puntos de pantalla. El babbeo que queda no es falta de relleno sino de **filo en el propio
  mapa**: ahi van la curva angular `vcs`, `inpaint.detail` y quizas un filo extra en el filtro guiado.
- Sin recompilar: `ODYSSEY3D_HOYOS=0` apaga el relleno (los dos canales llevan el mapa en crudo, que
  es la imagen de la v0.35). `ODYSSEY3D_MEDIR=1` anade una linea con los huecos rellenados y su coste.
- Y una perilla nueva para mirarlo en el panel: **`ODYSSEY3D_CANTO`** (0,004 de fabrica) es el
  `CantoSuave` del shader, o sea cuanto cambio de mapa por punto hace falta para no podar nada en un
  canto. Es la pareja del relleno: mapa con mas filo => la poda corta mas relieve en el borde. Bajo
  (0,001) conserva mas relieve en el canto; alto (0,010) lo deja mas quieto. Viaja en `tun5.w`, que
  estaba libre, asi que el tamano del bloque de parametros no cambia (288 bytes).
- Herramientas: `tools/prueba_hoyos.cpp` nuevo
  (`g++ -O2 -fopenmp -Isrc tools/prueba_hoyos.cpp src/hoyos.cpp -o /tmp/hoyos && /tmp/hoyos 518 3.06 0.50`);
  `tools/prueba_dibr.cpp` ahora arma las dos vistas con el mapa de cada ojo, acepta
  `MODELO=/ruta.onnx` para probar con la red de Samsung y `MAPA=/ruta.pgm` para volcar el mapa y
  medirlo aparte.
- Pendiente: mirarlo en el panel. Al pasar el mapa a dos canales el shader lee `.rg`, asi que si algo
  fuera raro en la textura la senal de aviso es que un ojo deja de desplazarse.

## Acabado: números oficiales en la cara y el temblor, y el cuadro mas barato (v0.35)
- Lo pedido fue mirar el acabado (suavizado en el tiempo, temblor por contorno, cara de 140 mm,
  lienzo de 384x160). Antes de tocar nada se comprob6 cada punto del informe de Windows contra
  nuestro c6digo, y dos de las cuatro cosas que decia que faltaban **ya estaban**:
  - el suavizado temporal del mapa existe (`smooth_` en `src/depth.cpp`, 0,35 de fabrica, con la
    guarda de cambio de plano), y el plano de apoyo tambien va filtrado en el tiempo (`focus_`,
    0,06 frente al 0,02 de Samsung, que responde antes a proposito);
  - el temblor del desplazamiento ya estaba **puenteado por contorno** (`canto` en
    `shaders/weave.frag:144-155`): solo se suma donde el mapa esta llano, que es donde se ven las
    bandas, justo al reves de lo que decia el informe.
- **Lo unico que se aline6 del temblor es la medida**: de 0,35 px a **0,25 px**, que es lo que sale
  de `filter.dither_strength = 0,01` sobre los 25 px de recorrido oficial (`src/vkweave.cpp:712`).
  `ODYSSEY3D_DITHER` sigue mandando sin recompilar.
- **Cara de 140 mm, no 150** (`src/track.h`, `src/viewer.h`): el rastreador de Samsung usa
  `FaceWidth_mm = 140` y nosotros 150, o sea un +7 % de sesgo en la distancia sacada del ancho de
  cara con la webcam normal. Se puede volver atras sin recompilar con `--track-cara 150`.
- **La medida que cambio el plan.** Con `ODYSSEY3D_MEDIR=1` el conversor ensena el reparto del
  cuadro sin sentarse delante del panel. Salia: red 6,6 ms · suavizado 0,7 · **limpiar y dilatar
  4,0** · plano de apoyo 0,5. O sea: los filtros del mapa costaban casi lo que la red, y bajar el
  lienzo a 384x160 (lo que hace Samsung) no hacia falta y ademas choca con lo de los rotulos, que
  necesita el mapa fino de 518. Se ataco el coste por otro lado.
- **El filtro guiado y la dilatacion van en cuatro hebras** (`caja` y `dilatar` en `src/depth.cpp`,
  OpenMP; `CMakeLists.txt` lo enlaza). Cada fila y cada columna es independiente, asi que el
  resultado es el mismo; con las treinta y dos hebras de la maquina el reparto se comia la ganancia,
  por eso el tope son cuatro y se puede cambiar con `OMP_NUM_THREADS`.
- **Comprobado fuera del panel, mapa por medio**: `tools/prueba_red.cpp` acepta ahora un numero de
  repeticiones y se compar6 el mapa escrito antes y despues del cambio. Con la red por CPU (que si
  es repetible) sale **identico byte a byte**, e identico con una, cuatro y ocho hebras. Por el
  camino salio un fallo propio: al separar los productos `guia x guia` y `guia x prof` la fila de
  paso de una caja quedó en la misma bufeta que su entrada, y la suma deslizante escribia por
  delante de donde leia (10.866 puntos movidos, hasta 4 de 255); corregido, y con un comentario para
  que no vuelva.
- **Cifras de cuadro ya sin panel** (NVIDIA, 518x518): con la red de Samsung 10,4 -> **7,8 ms**
  (red 3,0 + filtros 1,9 + apoyo 0,5 + subida 0,1), por debajo de los 8,3 ms que deja un panel a
  120 Hz; con la nuestra 13,4 -> **11,2 ms**. Ojo: la red de Samsung por GPU es la que abre la
  sesion mas rapida pero no da el mapa igual de dos pasadas seguidas, asi que para comparar calidad
  hay que mirar mapas sueltos, no restarlos.
- No se dispar6 ninguna prueba de relieve: todo se midio con herramientas de linea de comandos.

## Fuera el deslizador de ángulo a mano, y al arrancar siempre la red de Samsung (v0.34)
- Lo pedido: «quita el slider manual, manten el viejo comportamiento pero no toques lo de las camaras,
  eso está bien, inicia por defecto todo siempre en el modelo de samsung».
- **Desaparece el deslizador de ángulo** (−15°..+15°) de la pastilla, las teclas `[` `]` `\` y el giro
  que se sumaba a la pose del ojo (`src/vkui.h`, `src/vkui.cpp`, `src/vkview.cpp`): sin rastreo la trama
  vuelve a la pose nominal de fábrica, que es el comportamiento de antes de la v0.31. En el sitio del
  deslizador el botón escribe qué está mandando (la cámara o la pose de fábrica).
- **No se toca nada de las cámaras**: sigue el botón `T` de rastreo puesto/apartado, la elección de
  cámara, la vía mono por tamaño de cara y los deslizadores de profundidad y salida.
- **Arranque siempre con la red de Samsung** (botón 1) si el archivo está: `src/main.cpp` ya no pregunta
  la preferencia guardada y `src/vkview.cpp` elige `rutaRed[1]` por delante de lo que viniera por
  bandera. `~/.config/odyssey3d/red-elegida` se sigue escribiendo al cambiar con 1 / 2, pero al arrancar
  no manda. Con la red de Samsung y la gráfica llena entra la guarda de la v0.33 y se prueba la nuestra.
- Comprobado con `ctl hud` (prueba de la interfaz sin ventana): el mapa de golpes ya no tiene deslizador
  de ángulo, el botón del rastreo sigue en su caja de 180x30 y las etiquetas de la pastilla caben.

## Al cambiar de red de profundidad el visor se colgaba y moría (v0.33)
- Lo pedido: «nuevo tema, ahora al presionar el cambio de modelo el programa se crashea».
- **No era una regresión de la v0.32**: `coredumpctl list` marca el mismo `SIGABRT` (señal 6, sin
  volcado por el `LimitCORE=0`) con la 0.31 a las 01:54 y con la 0.32 a las 02:18, en los dos casos
  ~40 s después de `[conversor] no hay 1100 MB libres en ninguna grafica ... la red se hara con la CPU`.
- **Causa medida.** El botón **1** (la red de Samsung, `mde_large.onnx`) pedía abrir la sesión con la
  gráfica llena, así que `init()` se iba a la CPU. Ahí ONNX Runtime **no termina de crear la sesión**:
  reproducido fuera del visor con `ODYSSEY3D_GPU=-1`, `mde_large.onnx` lleva 4 minutos de reloj
  quemando CPU (proceso en estado R, 11 hilos, 1,4 GB) sin imprimir la sesión; en el visor eso acaba
  en abort y se lleva el cuadro y el panel por delante. La nuestra (`depth_anything_v2_small.onnx`) por
  CPU sí funciona: 1,0 s en abrir y 189,6 ms por mapa.
- **Lo que hace ahora (`src/depth.cpp`).** `esRedDeSamsung()` reconoce `mde_*`/`disp_*`. En `init()`, si
  el destino es la CPU y la red es la de Samsung, **no se intenta**: avisa con el número de MB que hacen
  falta (1550, con el colchón de 450) y cómo soltarlos. `cambiar()` avisa antes de soltar la sesión en
  curso si la gráfica está justa, y si el intento falla **vuelve la red anterior** (que estaba vivo y
  sigue vivo, medido con `tools/prueba_cambio.cpp`: salida con `viva=1` en menos de un segundo, antes
  se colgaba). En `vkview.cpp`, al arrancar, si la red pedida por el servicio no abre se prueba la
  nuestra: mejor un conversor más lento que ninguno.
- **También:** la tecla `R` ya no limpia la red mientras el hilo de estimación está dentro de ella
  (espera a que termine el mapa en el aire), y `ODYSSEY3D_GPU=-1` / `=N` fuerza CPU o una tarjeta para
  poder probar estos caminos sin recompilar.

## Rótulos y subtítulos apoyados en el plano de la pantalla (v0.32)
- Lo pedido: «no me hace sentido que veo los subtitulos y cualquier otro texto moviendose de lado a
  lado a pesar de tener el rastreo, el rastreo es como el de windows?, en windows se queda quieto el
  texto, cualquier texto».
- **Diagnóstico.** No era el rastreo. Contenido plano = las dos vistas iguales = la trama no depende
  del ojo, así que una zona plana no se mueve nunca (es la prueba algebraica de `shaders/weave.frag`
  ~183–198: con `L == R` la salida no cambia ni con la fase ni con el crosstalk, que son los unicos
  terminos dependientes del ojo). Si el texto se desliza es porque tiene disparidad: la red de
  profundidad toma el borde de las letras por relieve y lo saca un par de milimetros del panel. Con
  relieve de 2 cm y 15 cm de cabeza el barrido esperado son ~30 puntos del panel (5 mm), que es justo
  lo que se ve. En Windows el texto esta en el plano del cristal, por eso ahi no se despega.
- **Lo que hace ahora (`anclarTexto()` en `src/depth.cpp`).** Despues de la red y sobre la MISMA foto
  que genero el mapa, busca las zonas de rotulo y lleva el valor del mapa al plano de apoyo
  (`red->focusPlane()`), donde el desplazamiento izquierda/derecha vale 0 y las dos vistas salen
  iguales: el rotulo queda soldado a la pantalla y el resto del relieve no se toca. Rejilla de 8x8
  puntos; por celda mide contraste, parte de bordes, parte sin bordes, finura del detalle y area de
  trazo, y decide con las **medias de la vecindad** (3x3 celdas): las letras estan rodeadas de fondo
  despejado y una tela o un matorral no dejan ni un hueco. En la franja de abajo y la de arriba se
  bajan los umbrales. Despues dilata una celda y difumina, para que el apoyo entre despacio.
- **Medido fuera del panel** (fotos sinteticas de `../pruebas-texto/`, sin tocar el relieve real):
  subtitulo blanco sobre fondo liso 10,0 % de la rejilla (las dos lineas enteras, filas 79–92 % del
  alto); cintillo blanco sobre banda roja 2,9 %; detalle fino, detalle grueso y fondo liso 0,0 %, 0,2 %
  y 0,0 % (la textura conserva su relieve). Con `--mapa` se comprueba la cuenta: la banda pasa de 230 a
  115 = el plano 0,45, y el resto queda intacto.
- **Mando:** tecla `R` en el visor (alterna y reinicia la red para que el siguiente mapa salga distinto),
  `--texto-panel si|no`, `texto = si|no|auto` en `serve.conf` y `ODYSSEY3D_TEXTO=0` para comparar sin
  recompilar. Avisa el visor: `[rotulos] apoyados en la pantalla: 10,0% del mapa 518 x 518`.
- **Herramienta de ajuste:** `odyssey3d texto-detect foto.ppm [--plano 0,45] [--celda 8] [--mapa m.pgm]`
  imprime la rejilla de lo que considera rotulo, fila por fila, y con `--mapa` escribe
  `mapa-apoyado.pgm` con las estadisticas del cambio. Umbrales a la venta por variable:
  `ODYSSEY3D_TEXTO_CONTRASTE`, `ODYSSEY3D_TEXTO_FINO`. Con `--volcado` cae tambien `-rotulos.pgm`.
- **Coste:** unos 15 ms en el hilo de la red (que ya iba a ~7 ms con la red en la GPU); no anade nada
  por fotograma en la GPU.

## Rastreo con cualquier webcam y deslizador de ángulo en la pastilla (v0.31)
- Lo pedido: «la camara esta en 3d stereo webcam /dev/video0 segun discord, no puedes usarla? … quiero
  que añadas un fallback que use cualquier otra camara conectada para tracking, y ultima cosa, en el
  overlay quiero un boton para activar o desactivar el tracking junto con un slider largo sobre el
  overlay que permita ajustar manualmente el angulo del modelo, para poder hacer buen enfoque si no hay
  camara y que no tenga que estarse moviendo de lado a lado sin verse bien».
- **Estado de la camara del monitor:** sigue sin enumerarse. `ls /dev/video*` no la lista, `lsusb` no ve
  el `04e8:20d7`, y `ctl camera` no la encuentra. El LED rojo del borde de arriba encendido no prueba
  nada: sin nodo `video4linux` no hay de dónde leer. Lo único que la hace aparecer es apagar y encender
  el monitor (o reenchufar el cable USB-B); `udevadm trigger` sobre el monitor no sirve de nada.
- **De verdad hay una webcam conectada:** `HP 320 FHD Webcam` (`03f0:654a`) en `/dev/video0` y
  `/dev/video1`. Es una cámara normal, una sola vista, sin calibrar.
- **Como funciona el respaldo (`src/track.cpp`, modo `mono`).** Sin la calibración de fábrica no hay
  dos vistas que triangular, así que la posición sale del tamaño de la cara: `f = (0,5·ancho) /
  tan(0,5·campo)` y `z = f·cara/px`, con el centro de la caja de la cara dando `x` e `y` en centímetros
  del panel. No hace falta `calib/`, y `init()` deja de exigir la calibración cuando `set.mono`.
  `Camera::esEstereoDelMonitor()` decide: si el nombre del nodo dice «stereo»/«3D», si el USB es
  `04e8:20d7`, o si algún modo anunciado mide el doble de ancho que de alto, es la del monitor; si no,
  webcam normal. `ctl track --mono-simulacro` comprueba la cuenta sin cámara (devuelve la distancia
  pedida con error 0,000 en 640 y 1280 px).
- **En vivo, con la HP:** `ctl track --dev /dev/video0 --segundos 8` → `camara /dev/video0: una sola
  vista`, formato `1280x720 MJPG @ 30`, `123 fotogramas leidos, 123 poses validas`, cara de 170 a 250 px
  y distancia de 60 a 90 cm, unos 3 ms por cuadro. Con `--cara 175 --campo 70` la distancia sube a 83 cm,
  o sea que las dos llaves hacen lo que dicen.
- **Opciones nuevas:** `--track-mono si|no|auto`, `--track-campo GRADOS`, `--track-cara MM`; en
  `serve.conf` `camara`, `mono`, `campo`, `cara` (se pasan al visor al abrir la ventana). `ctl track`
  acepta `--mono`, `--campo`, `--cara` y `--mono-simulacro`, y avisa si se le pide la vía de dos vistas
  a una cámara que no es la del monitor.
- **Botón y deslizador en la pastilla (`src/vkui.cpp`).** La pastilla con rastreo pasa de 84 a 134 px de
  alto y gana una fila: `Rastreo puesto (T)` / `Rastreo a mano (T)` / `Sin camara (T)` (180x30) y un
  deslizador de 592 px de ancho, de −15° a +15° en décimas de grado. El punto verde del botón solo brilla
  si hay rastreo puesto **y** la cámara está dando poses. La prueba de la interfaz (`ctl hud`) ya cubre
  los dos sitios nuevos y comprueba el recorrido: extrema derecha +14,9°, extrema izquierda −14,9°.
- **Teclas:** `[` `]` giran 1,0° (con `Shift`, 0,2°), `\` centra, `T` aparta el rastreo. En la ventana 3D;
  en `play` `[` `]` siguen siendo cuadro a cuadro.
- **Como se aplica el ángulo (`src/vkview.cpp`).** El ojo que usa la trama sale de una sola fuente
  (`ui.ojoBase`): si el rastreo está puesto y la cámara da una pose fresca, esa pose es la base; si está
  apartado, la base vuelve a la posición nominal de fábrica. Sobre la base se suma siempre el desplazamiento
  del deslizador, `x += tan(angulo)·z`, y sólo se llama a `setEye` cuando el resultado cambia (así no se
  recalcula la trama cada cuadro sin necesidad). Con el rastreo apartado y el deslizador puesto, el
  relieve queda enfocado en un sitio fijo: ya no hace falta moverse de lado a lado.

## «Se inicio la pura rejilla 3D» y al salir se veia horrible: el conversor abortaba y el panel se quedaba en 3D (v0.30)
- Lo pedido: «en las ultimas ejecuciones se inicio la pura rejilla 3d sin mostrar el video en 3d, checa el
  motivo, tambien note que en unas pocas el video se mostraba bien en 3d pero al salir el overlay se veia
  horrible por unos segundos antes de cerrarse».
- **El motivo de la rejilla, del journal (`journalctl --user -u odyssey3d-serve`):** en la corrida de las
  00:58:36, despues de `[vulkan] intercambio 3840x2160 · 3 imagenes` salen
  `[conversor] no hay 1100 MB libres en ninguna grafica: la red se hara con la CPU, mas despacio` y
  `[conversor] la grafica no dio para abrir el modelo (.../providers/cuda/cuda_call.cc:154 ... CudaCall ...):
  se intentara con la CPU`, y a continuacion **SIGABRT** con `COREFILE present ... 298.7M`. La pila del hilo
  que revienta esta entera dentro de `libonnxruntime.so.1`, llamada desde el reintento (`_ZZN8DepthNet4initE...
  ...cold`): o sea, no muere en el intento de la grafica, muere en el **reintento**.
- **Dos bugs que se suman.** (1) El aviso decia "se hara con la CPU" pero no era cierto: con `sinGpu` se lanzaba
  una excepcion que caia en el `catch` y ahi se abria CUDA por la puerta vieja
  (`AppendExecutionProvider_CUDA`), asi que el intento de CPU nunca existio. (2) En el manejo del fallo estaba
  `dev_ = "nada"; if (dev_ == "CPU") return false;`, prueba que ya no puede ser cierta jamas: siempre se caia en
  el reintento, que es el que aborta dentro de la libreria.
- **Consecuencia visible:** el visor murio antes de dibujar el primer cuadro con el panel ya en modo 3D
  (`[panel] DP-4: modo 20 3840x2160@119.88`, `[lente] estado: 3D`, `[scaler] /dev/sg1: modo 3D activado`), y por
  eso se vio solo la rejilla. El hub no lo note hasta las **00:59:00**, 22 s despues, porque systemd-coredump
  estaba recogiendo el volcado de 298 MB antes de que `waitpid` avisara de la muerte del hijo; recien ahi
  `devolverPanelHuerfano` devolvio el panel.
- **Hecho en `src/depth.cpp`:** con `sinGpu` no se toca CUDA (se va directo a CPU); el "si cabe" de la grafica
  lleva un colchon de 450 MB (1,1 GB libres anunciados no alcanzaban: la sesion CUDA pide su contexto antes que
  la red); antes de abrir por CPU se mira `MemAvailable` de `/proc/meminfo` (con menos de 1,5 GB se queda apagado
  y lo dice); y **despues de un intento CUDA fallido no se abre otra sesion en el mismo proceso** (`quemada_`):
  el visor sigue, el video se ve sin relieve y el panel se devuelve bien.
- **Hecho en `systemd/odyssey3d-serve.service`:** `LimitCORE=0`. El visor es hijo del servicio, asi que el
  volcado de 300 MB retrasaba el `waitpid` 22 s y dejaba el monitor en 3D sin nadie dibujando. La pila del fallo
  la imprime igual el manejador del visor, no se pierde diagnostico.
- **Lo del "horrible al salir":** al salir el video de pantalla completa el hub aguanta 2,5 s antes de cerrar (para
  no cerrar por un parpadeo) y en ese hueco la trama seguia trazada sobre el escritorio. Ahora, en cuanto el video
  deja de cubrir el lienzo, el hub escribe `oculta` en `~/.local/state/odyssey3d/ventana` y el visor deja de trazar
  al instante (si el video vuelve, se escribe `visible` y sigue); la espera de cierre baja de 2,5 s a 1,2 s porque
  el panel en modo 3D sobre un escritorio 2D ya no tiene nada encima que lo tapen.
- **Ademas:** el hub ya puede pasar el rastreo al visor, que era lo pendiente de v0.29: `rastreo = si` y
  `suavizado = 0,35` en `~/.config/odyssey3d/serve.conf` (argumentos `--track` y `--track-suavizado`), y al arrancar
  avisa si no aparece la camara `04e8:20d7`.

## La camara IR no entregaba fotogramas: autosuspension USB y un puerto colgado (v0.29)
- Lo pedido: «el modelo se adapta con las camaras del monitor a mis ojos, pero percibo que hay un delay en el
  ajuste, en windows se ajusta mas rapido, es porque tal vez el mapeo ocular sea menos frecuente?». Lo primero
  era saber si aqui se esta mapeando algo, y **no se esta mapeando nada**: el hub nunca pasa `--track` al visor
  (la linea de comandos del hijo arma `capture3d --uuid … --output … --sbs` y los argumentos del conversor, y
  ahi no aparece `--track`) y en 48 h de `journalctl --user -u odyssey3d-serve` no hay una sola linea `[rastreo]`.
  El retardo que se nota no es el de un filtro: es que la trama se calcula para la posicion de ojo nominal.
- **Los numeros de Windows, leidos del USB** (`AnalisisOdyssey/configs/leiasr/tracker-ft_user.ini`): el rastreador
  de Samsung (`EyeTracker=BLINKEYE`, `cameraName=MEDIAFOUNDATION`) publica poses con `maxPosFrequency=240`,
  declara `cameraLatency_s = 0,0207` y **adelanta el entrechado** con `predictionTime_s = 0,0156`; las camaras IR
  van a 60-90 fps. Cadencia alta + prediccion: por eso alla el ajuste llega antes.
- **El bloqueo real: la camara no entrega nada.** `ctl track` → `lectura: sin fotogramas (fin de espera)`;
  `ctl camera` → `ajuste de formato: VIDIOC_S_FMT 1280x480 MJPG: Input/output error`. En `dmesg`, ya antes de
  tocar nada y repetido cada vez que se intenta el flujo:
  `uvcvideo 5-1.4.2.3:1.1: Failed to set UVC probe control : -110 (exp. 26).` — o sea, el endpoint de control de
  la **interfaz de flujo** deja de contestar (los de la interfaz de video, como `power_line_frequency`, si
  contestaban) y eso es el `EIO` que ve el usuario.
- **La causa, medida:** la camara estaba suspendida — `power/runtime_status = suspended` con
  `power/autosuspend_delay_ms = 2000` — y cuelga de cuatro hubs Genesys (`05e3:0608`/`0610`) en un unico puerto de
  480 M compartido con impresora, audio, tablero de la lente y el lector del scaler. Subir `uvcvideo.timeout` de
  100 a 1500 ms no cambia nada: no es que tarde, es que el resume no llega y la interfaz se queda colgada.
- **Hecho:** `udev/70-odyssey-3d.rules` escribe `power/control="on"` en la camara (`04e8:20d7`) y en el tablero de
  la lente (`354b:0116`) al conectar; con el dispositivo siempre despierto los hubs de en medio tampoco pueden
  suspenderse. `Camera::open()` lo intenta tambien desde el programa (`Camera::keepAwake`, sin root deja la nota),
  y `doctor`, `ctl camera` y `ctl track` imprimen `energia control=… estado=…` para no vuelva a ser invisible.
  Si `VIDIOC_S_FMT` falla con `EIO`, el mensaje ya dice que la interfaz de flujo no contesta.
- **Lo que no se arregla por software:** despues de `USBDEVFS_RESET` sobre `/dev/bus/usb/005/020` la camara dejo de
  enumerar (`device descriptor read/64, error -110` en bucle), asi que hay que **apagar y encender el monitor**.
  Comprobado despues del reset: la lente sigue en `/dev/ttyACM0` con `auth: si` y el scaler en `/dev/sg1`, o sea
  el reset no barre la cadena; al encender el monitor se pierde la autenticacion de la lente y el programa la
  vuelve a hacer solo.
- **Pendiente, con la camara despierta:** pasar `--track` a las sesiones del hub, medir la cadencia y el retardo
  reales (`cameraFps`, `latencyMs`) y bajar el filtrado (`--track-suavizado`, hoy mediana de 3 + media movil con
  0,35) para que el ajuste se note inmediato como en Windows.

## Un click en el vídeo tapaba la trama: el hub lo deja por debajo del visor (v0.28)
- Lo pedido: «al dar click en otro punto del video (en el timeline) el overlay pierde foco y se va al
  fondo, se empieza a ver mal cuando eso pasa».
- **La causa, medida con `ctl apilamiento`** (el orden que manda el compositor, de abajo hacia arriba).
  En una sesión sana: vídeo en 9 y visor en 11. Al activar el vídeo —que es lo que hace un click en su
  barra de tiempos— el navegador sube a 11 y el visor baja a 10: **KWin pone al frente la ventana a
  pantalla completa que tiene el foco**, y ahí el `KeepAbove` del visor no manda nada. Probado y anotado:
  reenviar `KeepAbove`, quitarlo y volverlo a poner, o quitar y volver a poner la pantalla completa del
  visor **no** lo devuelven arriba. Y no hay forma de traerlo: `plasma-window-management` (rev 21) no trae
  pedido de `raise` ni de `request_activate` (está en el XML del sistema), y por D-Bus `/KWin` tampoco.
- **Lo que sí funciona, medido: poner el vídeo con `KeepBelow`.** Con eso el vídeo queda en la posición 1
  del apilamiento y el visor en la 11, y activar el vídeo dos o tres veces seguidas ya no lo sube por
  encima. El hub lo pone al arrancar la sesión (`[ventana] el video pasa por debajo del visor: un click en
  el video (la barra de tiempos) ya no tapa la trama`), lo vuelve a poner si el compositor se lo quita al
  cambiar el modo, y tiene una red: cada 5 s (o cuando el vídeo recupera el foco) pide el orden de
  apilamiento y, si el vídeo quedó encima, se lo vuelve a meter debajo con el truco de quitarla y ponerla.
  Al cerrar la sesión (o al detener el servicio) se le quita la marca para que el usuario no se quede con
  el vídeo escondido detrás de las ventanas.
- **Comprobado con la copia instalada:** sesión con el vídeo en `1` y el visor en `11`, tres activaciones
  simuladas del vídeo (`qdbus org.kde.KWin /WindowsRunner Run`) y el apilamiento sin cambiar; el flujo de
  captura siguió llegando `3840x2160` (3408 fotogramas) aunque el vídeo estuviera abajo, porque la trama
  captura la ventana, no lo que se ve en pantalla. Al cerrar, el vídeo volvió a `activa, maximizada` sin
  el `debajo`.
- **Ruido quitado:** mientras la sesión se está cerrando ya no se pide el orden de apilamiento, porque al
  restaurar el monitor el compositor contesta tarde y salía `[ventana] el compositor no termino de mandar
  el orden de apilamiento`.

## El vídeo queda chico en el lienzo al cambiar el modo: se lo devuelve el hub (v0.27)
- Lo pedido: «servía mejor en la build anterior, en esa el problema era el posicionamiento; en esta también
  me está sacando de pantalla completa el video y la pantalla se ve mal». O sea: 0.26 cerraba sesiones que
  el usuario quería vivas y además la imagen se veía mal.
- **Lo que estaba mal no era la pantalla completa, era el tamaño.** Mirando los registros de las sesiones
  reales: `[captura] el fotograma del compositor es 2560x1440 y el panel esta en 3840x2160: la trama
  necesita pixel a pixel`. Al pasar el panel al modo 3D cambia la escala de la salida, y KWin deja la ventana
  del vídeo con el tamaño viejo: **conserva el estado de pantalla completa pero sigue en 2560x1440 sobre un
  lienzo de 3840x2160**. El vídeo ocupa una parte del monitor y la trama se ve mal. La guardia de 0.26, además,
  leía la pérdida momentánea del estado como «el usuario se fue» y cerraba la sesión.
- **El hub reacomoda la ventana del vídeo.** Mientras la sesión es joven (12 s, los segundos del cambio de
  modo) si al video le falta el estado **o no cubre la salida del panel**, se lo devolvemos: cuando conserva
  el estado pero está chica hay que **quitarle y volverle a poner** `Fullscreen`, porque ponerla otra vez no
  hace nada. Hasta ocho intentos, separados 0,6 s para no machacar al compositor. Asentada la sesión, la
  pantalla completa que falta sí es el usuario: se cierra con 2,5 s de paciencia (antes 8 s).
- **Medido con la copia instalada (23-nov, YouTube en DP-4, `sbs`).** Antes del arreglo: flujo `#12 2560x1440`
  y el aviso de `pixel a pixel` en cada sesión, cierre a los segundos. Después: **un solo** `[ventana] el
  cambio de modo dejó al video sin cubrir el lienzo (2560x1440 en una salida de 3840x2160, pantalla completa
  si): lo reacomodamos (1/8)`, flujo `#12 3840x2160` desde el primer fotograma, cero avisos de `pixel a pixel`,
  y la sesión siguió viva 62 s con el conteo subiendo (1 → 4405 fotogramas). Al irse del vídeo
  (`ctl ventana normal`) la sesión se cerró en unos tres segundos con `[scaler] modo 3D apagado`,
  `[lente] devuelta a 2D` y `[panel] restaurado: modo 19 3840x2160@164.98, escala 1.5`.
- **Arreglo de compilación:** CMake no volvía a configurar cuando cambiaba `VERSION`, así que el binario
  instalado seguía diciendo la versión vieja (con `VERSION` en 0.26 respondía `odyssey3d 0.25`). Se agregó
  `set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)`.

## Al irse del vídeo se apaga la sesión, no se queda el monitor en 3D (v0.26)
- Lo pedido: «now that works but letting the video leaves the 3d screen active and the overlay behind the
  active window, I need to close it manually». Al salir del vídeo (ESC, cambiar de pestaña, minimizar o
  cerrar la ventana) el monitor se quedaba en modo 3D y la pastilla caía detrás de la ventana activa: había
  que cerrarla a mano.
- **Estábamos peleando por el sitio equivocado.** El vigía del hub, si la ventana del vídeo perdía la
  pantalla completa, **se la volvía a poner**. Eso peleaba contra la intención del usuario: irse del vídeo es
  la forma normal de terminar una sesión. Ahora la salida del vídeo es el botón de apagar: el hub manda
  `SIGTERM` al visor, que se va por la puerta de adelante (`rig.shutDown()`) y devuelve panel, lente y scaler.
- **Medido antes de tocar nada.** Muestreo de la ventana del vídeo con `ctl windows` una vez por segundo
  durante el arranque de una sesión: a `t=1` está `activa, maximizada, pantalla completa` en `0,0`; de `t=2`
  a `t=6` aparece **sin** `pantalla completa` y corrida a `640,360`; de `t=7` en adelante vuelve a `0,0` con
  el estado puesto. O sea que **cuando el panel cambia de modo KWin le quita la pantalla completa al vídeo
  unos cinco segundos**. La primera versión cerraba la sesión al notar eso y entraba en un ciclo de lanzar y
  cerrar (`[ventana] la ventana del video salio de pantalla completa` + `[serve] lanzando 3D` cada ~6 s, con
  el panel parpadeando entre el modo 19 y el 20). Lo que separa el transitorio de una salida real es cuánto
  dura: gracia de 5 s desde que arranca la sesión y paciencia de 8 s para ese motivo; si la ventana se cerró
  o se minimizó, 1,5 s.
- **Tres redes por si el visor se muere.** `devolverPanelHuerfano()` lee la copia del panel que quedó
  guardada y la restaura: al arrancar el hub (si no hay visor vivo), al recolectar el hijo muerto y al
  detener el servicio. Además, si una sesión muere apenas empieza el hub espera 8 s antes de volver a
  ofrecer 3D, y tras dos muertes seguidas tempranas espera un minuto (`[serve] la sesion se cerro apenas
  empezo: esperamos un minuto antes de volver a ofrecer 3D`).
- **El visor también se va solo si no hay nada que esperar.** Si el flujo de captura no llegó a crearse y la
  ventana del vídeo ya no existe, el visor se sale él: `[captura] la ventana del video cerro sin dejar flujo:
  se cierra la sesion y se devuelve el panel a 2D`. Esa comprobación pregunta al compositor sólo en la rama
  de fallo, nunca en el bucle sano, porque abrir la conexión y acomodarse cuesta.
- **Medido con la copia instalada (23-nov, YouTube en DP-4).** Con el vídeo a pantalla completa la sesión
  aguantó 45 s seguidos, sin avisos de salida y con el visor presente en todas las muestras. Al irse del
  vídeo (`ctl ventana normal`) salieron en orden `[ventana] la ventana del video salio de pantalla completa`,
  `[sesion] llego una senal de cierre`, `[scaler] modo 3D apagado`, `[lente] devuelta a 2D`,
  `[panel] restaurado: modo 19 3840x2160@164.98, escala 1.5, rotacion none` — unos ocho segundos, con el
  panel en 19 y la lente `off` al final. Matando al visor con `SIGKILL` (sin puerta de adelante) funcionó la
  red: `[serve] la sesion 3D se termino: quedaba el panel guardado en modo 3D (modo 19 …)` + `[panel] restaurado`.

## El flujo de captura sobrevive al rearme (v0.25)
- Lo pedido: «en un momento noté que volvia el hub pero ya sin el video». La pastilla volvía por encima del vídeo,
  pero la trama seguía mostrando el último cuadro de antes del minimizado.
- **El conteo clavado no era KWin dejando de repintar.** Con el visor clavado —`3361 fotogramas` repetido y
  `[captura] el flujo se corto, pidiendo el flujo otra vez (intento 1)` en bucle— medimos la MISMA ventana del vídeo
  con una conexión nueva: `ctl flujo --uuid <video> --segundos 8` → **528 fotogramas en 8,0 s = 65,6 fps**. El
  compositor sí repintaba y mandaba fotogramos de una ventana a pantalla completa tapada por otra; lo que no existía
  era nuestro flujo.
- **La bandera que no se limpiaba.** `stop()` destruía el flujo de PipeWire pero dejaba `pwStarted` en true, así que
  `readyToConnect()` respondía «ya estoy conectado» y `connectToNode()` no volvía a crear el nodo: los doce reintentos
  del vigía gastaban `stop()` + `start()` sin crear nunca un flujo nuevo, y el contador se quedaba helado. `closeAll()`
  ahora limpia también `pwStarted`, el nombre y el serial del nodo anterior —ese nodo ya estaba muerto, y usarlo de
  objetivo garantizaba el corte.
- **El aviso ya no manda a tocar el navegador.** El consejo de `gfx.vsync.compositor.unobserve-count = 0` sólo se
  imprime si nuestro flujo sigue en `streaming`; con `sin estado` o `paused` el roto es nuestro. La propia línea ahora
  dice cómo comprobarlo: `odyssey3d ctl flujo --uuid <id> --segundos 5`.
- **Medido con la copia instalada (23-nov, sesión real sobre YouTube en DP-4, tres minimizados seguidos)**: el conteo
  del visor siguió subiendo a través de los ciclos — 3046 → 3270 → 3526 → 3894 → 4188 → 4458 → 4712 fotogramas —, con
  `[ventana] de vuelta: lienzo 3840x2160 en DP-4, intercambio y captura rehechos` y **cero** avisos de corte, cero
  importaciones fallidas y el panel sin parpadear.

## El visor vuelve delante del vídeo, en su pantalla y sin gráficos rotos (v0.24)
- Lo pedido: «ok, el overlay vuelve pero se queda detras del video en otra ventana llena de glitch graficos».
- **Dos fallos, el uno encima del otro.**
  1. *Detrás del vídeo.* Quitar la minimizada no basta: en `org_kde_plasma_window_management` **no hay petición
     de «activar/traer al frente»**, así que KWin devuelve la ventana con el orden que ya tenía y el vídeo —activo
     y a pantalla completa— queda por encima. Medido con el nuevo `ctl apilamiento`: la lista que manda el
     compositor va de **abajo hacia arriba** (el fondo siempre el primero) y con `KeepAbove` la ventana salta al
     último puesto, o sea arriba. El hub la pide al devolverla y la **reafirma cada 5 s** si el compositor la perdió.
  2. *Los gráficos rotos.* GLFW no se entera en Wayland de que lo minimizaron, y el bucle del visor seguía
     presentando cuadros contra un intercambio sin superficie; al volver, la ventana estaba achicada y en otra
     salida (los registros la muestran 3840x2160 → 2560x1440 → 2015x869) y la trama dejaba de ser pixel a pixel.
     A eso se sumaba que, si `vkAllocateMemory` fallaba al importar el DMA-BUF del compositor, el visor marcaba el
     cuadro como nuevo igual y tramaba una fuente que no se había importado.
- **El hub habla con el compositor por la salida** (`sendToOutput`, `outputNames()` y `outputs()` en `src/wm.cpp`,
  enlazando `wl_output` + `zxdg_output_manager_v1` para tener nombres y rectángulos lógicos): al devolver la
  ventana del visor pide también `Fullscreen` y `send_to_output(DP-4)`. Y vigila la **ventana del vídeo**: si pierde
  la pantalla completa o su centro cae fuera del rectángulo de la salida del panel, la devuelve y avisa
  (`[ventana] el video (…) esta en x,y WxH …: lo devolvemos a pantalla completa en DP-4`), como mucho cada 5 s.
- **El visor deja de trazar mientras esté oculto** (`src/vkview.cpp`): el hub escribe
  `~/.local/state/odyssey3d/ventana` con `oculta`/`visible` y su reloj monotónico; con `oculta` el bucle no dibuja,
  espera con `glfwWaitEventsTimeout`. El panel solo se devuelve a 2D (`rig.shutDown()`) si la marca ya es vieja
  —hub muerto— y seguimos ocultos 2 s: con el hub vivo la ventana vuelve sola y apagar el panel hacía parpadear
  el monitor 3D→2D→3D en cada minimizado (así salió en la prueba del 23-nov). Al volver:
  `glfwRestoreWindow` + `glfwSetWindowMonitor` en nuestra salida, `wv.resize()` y rearme de la captura
  (`cap.stop()` + `cap.start()`), que en esta 0.24 todavía no servía: el flujo nuevo no llegaba a crearse, y eso se
  arregla abajo, en la 0.25. Si el compositor nos deja con un lienzo que no mide lo del modo, se vuelve a pedir la
  pantalla completa (aviso único, cada 3 s).
- **Fuente que no se importa, no se trama**: `newSource` sólo se marca si `sourceFromDma` tuvo éxito, y tras 12
  importaciones falladas seguidas se avisa de que falta memoria de video (en la máquina había 712 MB libres).
- **Mandos para no adivinar**: `ctl salidas` (nombre, posición y tamaño lógicos: `DP-4 0,0 2560x1440`),
  `ctl apilamiento` (el orden de arriba y abajo) y `ctl ventana sobre|debajo|libre|salida NOMBRE`. `ctl aparece`
  ahora, además de quitar la minimizada, pone `KeepAbove`, `Fullscreen` y manda la ventana a la salida del panel.
- **Medido en la máquina (23-nov, hub 0.24 lanzando una sesión real sobre YouTube con `modo = sbs`)**: con el
  visor en pantalla completa, `ctl apilamiento` lo dio en el puesto 9 de 9 —por encima del vídeo, puesto 8— y
  `ctl windows` en su geometría `0,0 3840x2160` sobre DP-4. Minimizado el visor a la fuerza, el hub escribió
  `oculta 11180.6`, lo devolvió solo y la marca pasó a `visible 11184.6` (4 s), con `[ventana] de vuelta: lienzo
  3840x2160 en DP-4, intercambio y captura rehechos` y de nuevo arriba del vídeo. Quitándole la pantalla completa
  al vídeo a propósito, el guardia lo devolvió: `[ventana] el video (…) esta en 640,360 2560x1440 con activa,
  maximizada: lo devolvemos a pantalla completa en DP-4`.
- **El panel no debe parpadear al devolver la ventana**: el visor sólo devuelve el panel a 2D si el servicio **dejó de
  escribir** la marca de ventana (marcador viejo y la ventana minimizada). Con el umbral corto (1,5 s) el panel hacía
  3D → 2D → 3D en cada minimizado, porque el hub tarda unos 2 s en devolver la ventana.

## La pastilla vuelve siempre: tecla PB y ventana minimizada devuelta (v0.23)
- Lo pedido: «el overlay al esfumarse ya no vuelve con los keys», «la tele se quedó en el modo de malla 3D»
  y «¿hay forma de que se pueda mostrar de vuelta siempre el overlay con la tecla PB?».
- **La causa** (vista con `ctl windows`): la ventana del visor salía como `minimizada, pantalla completa` y en
  el registro del servicio no había ni un `[pantalla] entregada`, así que no la habíamos minimizado nosotros:
  lo hizo el gestor de ventanas (Meta+F9, un clic en la barra de tareas, un gesto). Con la ventana minimizada la
  trama desaparece —«se esfumó»—, no llega ninguna tecla porque el foco lo tiene otra ventana, y el proceso
  sigue vivo con el panel en modo 3D: de ahí «la tele se quedó en malla 3D». El visor no se entera de la
  minimización (GLFW no la ve en Wayland), así que la vigilancia tiene que hacerla el hub.
- **Tecla `PB` (Pausa/Break)** en el visor (`src/vkview.cpp`): muestra la pastilla aunque estuviera ocultada,
  se devuelve si estaba minimizada y pide el foco. `glfwKey` ya conocía la tecla (`GLFW_KEY_PAUSE = 284`).
- **El rincón que siempre se pica** (`Hud::huecoDeLaPastilla` en `src/vkui.cpp` y `aplicarRegionEntrada`): con
  la pastilla ocultada la región de entrada quedaba **vacía**, y sin región no llega ni un movimiento del
  puntero, o sea que nada podía traer la pastilla de vuelta. Ahora, si no hay ninguna caja, se deja marcada la
  punta derecha de su sitio de siempre (170 × 84 px de ventana) y el bucle del visor, en cuanto el puntero cae
  ahí, muestra la pastilla (`[pastilla] el puntero llego al rincon de arriba a la derecha: la pastilla vuelve`).
- **El hub devuelve la ventana** (`cmdServe` en `src/main.cpp`): si hay sesión y su ventana aparece minimizada
  sin que fuera cosa nuestra, se le ordena al compositor `set_state(Minimized, 0)` y se avisa una sola vez
  (`[ventana] el gestor minimizo el visor: lo devolvemos. Para estar aparte usa Tab`), con 2 s de enfriamiento.
  Para distinguir el `Tab` voluntario de una minimización ajena, el visor escribe
  `~/.local/state/odyssey3d/entregada` con su pid al entregar y lo borra al recuperar y al salir; el hub sólo lo
  cree si ese pid sigue vivo (`kill(pid, 0)`). La nueva ruta sale de `rutaEstadoOdyssey` en `src/util.cpp`.
- **`ctl aparece`** y `ctl ventana [minimizar|aparecer]`: el primero busca la ventana del visor por su
  `app_id`/título, sin saber el uuid, y la devuelve a la vista. Es el comando que se puede atar a un atajo
  global del escritorio, que es la única forma de responder a `PB` cuando el foco lo tiene el vídeo (no hay
  acceso al teclado crudo: el usuario no está en el grupo `input`).
- **Comprobado**: `ctl hud` añade `rincon para traer la pastilla de vuelta: 1726,24 170x84  vale` y
  `la pastilla vuelve con la tecla PB: si`; sobre una sesión real, `ctl ventana minimizar` dejó la ventana en
  `minimizada` y `ctl aparece` la devolvió —KWin sí obedece `set_state(Minimized, 0)`—, con lo que el guarda del
  hub tiene por dónde. La salida por señal del visor cerrado dejó el panel como estaba
  (`[panel] restaurado: modo 19 3840x2160@164.98, escala 1.5`, lente a 2D, scaler apagado).
- **Límite**: este Protocolo de KWin no trae `request_activate`, así que la ventana devuelta no queda activa;
  hay que picar la pastilla para devolverle el teclado al visor.

## El raton atraviesa el visor: se puede pausar el video sin quitar el 3D (v0.22)
- Lo pedido: «no puedo controlar lo que está en la pantalla, el overlay no me debería quitar control del
  resto del contenido mostrado». Con el visor a pantalla completa no se podía dar a pausa, ni mover la barra
  de avance, ni pinchar en el navegador de abajo.
- **La causa**: en todo el proyecto no había ninguna región de entrada (`wl_surface_set_input_region`). La
  ventana del visor cubre la pantalla entera y, si no se le dice nada, el compositor le manda todos los clics
  y toda la rueda. Lo mismo pasaba con la capa del hub: su lienzo llega hasta el borde de la salida y lo que
  no es tarjeta es transparente, pero se picaba igual.
- **El visor** (`src/vkview.cpp`): la ventana la pone GLFW, así que sacamos su superficie con
  `glfwGetWaylandWindow` sobre la misma conexión de GLFW y le marcamos una región con la pastilla y, si está
  puesto, con el aviso. Fuera de esas cajas el ratón atraviesa el visor y lo que se pica es la ventana de
  atrás. La región sólo se manda cuando cambia (se compara la firma de las cajas, no cada cuadro). Al
  arrastrar un deslizador la ventana vuelve a quedarse el ratón entero: si no, el puntero se sale de la caja a
  medio arrastre y el deslizador se planta. `--sin-pasar-raton` devuelve el comportamiento de antes.
- **La capa del hub** (`src/overlay.cpp`): lo mismo con la tarjeta de aviso, sólo la caja de la tarjeta es
  pinchable y el resto del lienzo deja pasar el ratón. Viaja en el mismo envío del lienzo, sin commit extra.
- **Se avisa**: encima de la pastilla aparece `clic fuera de la pastilla  manda sobre el video de atras`, en
  el arranque del visor salen dos líneas explicándolo y cada cambio de región se anota
  (`[raton] el raton atraviesa el visor fuera de la pastilla · region de entrada: 814,22 763x78 …`, y en el
  registro del servicio `[capa] region de entrada: solo la tarjeta`).
- **Lo que hay que saber**: el teclado se queda con la ventana activa. Un clic en el vídeo se lleva el foco y
  las teclas finas del visor (`3`, `F`, flechas…) dejan de ir al visor hasta que se pica la pastilla; la rueda
  y los clics sobre la pastilla siguen en el visor, y `Tab` / `ESC` siguen siendo la salida limpia. Sobre X11
  no hay regiones: el visor se queda los clics y lo dice (`el compositor no dio wl_compositor`).
- Comprobado: con la pastilla puesta la región sale con las dos cajas y con la capa oculta sale
  `region de entrada: ninguna`; con un ratón falso de `uinput` el puntero entraba en la ventana sobre la
  pastilla y salía al irse de ella, y con `--sin-pasar-raton` el mismo barrido de ocho puntos daba eventos en
  varias partes de la ventana. El posicionamiento del ratón falso no era fino, así que la prueba de clic sobre
  el vídeo queda para el escritorio real. En el camino salió un bug: con la pastilla oculta la región vacía no
  se mandaba nunca porque la firma inicial también estaba vacía; ahora la firma empieza con un valor imposible.

## Boton 1 / 2 en la pastilla: cambiar de red de profundidad sin cerrar el visor (v0.21)
- Lo pedido: un boton que alterne entre el modelo de Samsung (1) y el nuestro (2), y que se recuerde
  la ultima que usamos.
- **La pastilla** lleva un boton nuevo entre los dos botones y los dos deslizadores: `Red 1 · Samsung`
  o `Red 2 · nuestra`, con un punto de color cuando el conversor esta abierto y la leyenda
  `1 / 2  cambia de red` debajo. Si el visor esta sin conversor el boton se apaga y dice
  `Red sin conversor`. Las teclas sueltas `1` y `2` hacen lo mismo; `Ctrl+Shift+1..8` no cambian.
- **Cambio en caliente** (`DepthNet::cambiar`): primero se suelta la sesion vieja, que es lo de verdad
  devuelve la memoria de la grafica, y despues se abre la nueva; se conservan la tarjeta usada, el
  suavizado y la limpieza del mapa, y se olvidan el mapa anterior y el seguimiento del apoyo para no
  mezclar las escalas de dos redes distintas. Si la red nueva no abre, se reintenta la vieja y el
  conversor sigue como estaba, avisando. Medido suelto: el cambio cuesta unos 0,3 s, y un cuadro de
  518x518 sale en 10,6 ms con la red de Samsung y 14,9 ms con la nuestra (NVIDIA).
- **Se recuerda la eleccion** en `~/.config/odyssey3d/red-elegida` en cuanto se pulsa el boton o la tecla. Al
  arrancar, el hub arranca con esa red; si el fichero no esta, usa la de Samsung si existe y si no la
  nuestra.
- **Cual de las dos es cual**: la misma regla en el hub y en el visor, por el nombre del fichero
  (`slotRed`) — lo que lleve `mde_` o `disp_` es la red de Samsung (boton 1); cualquier otra .onnx es
  la nuestra (boton 2). El hub avisa de las dos rutas: `[serve] redes de profundidad: boton 1 = ... ·
  boton 2 = ...` y `arrancamos con la N (...)`. Clave nueva en `serve.conf`: `nuestra=RUTA`
  (y `serve --nuestra RUTA`); el visor acepta `--alterno RUTA` para la otra red del boton. Si la red de
  un boton no esta en ninguna parte, el visor lo dice en vez de quedarse mudo:
  `no sabemos donde esta la red N (ruta): pasele --alterno RUTA`.
- Comprobado: prueba suelta que cambia Samsung → nuestra → Samsung y luego intenta una ruta que no
  existe (avisa y sigue con la anterior); el hub con preferencia 1, con 2 y sin fichero; y el visor con
  una ventana real y el clic del boton simulado (`ODYSSEY_GOLPE=red`), que dio `golpe 5` →
  `cambiada a la red 2 (la nuestra)` → `cambiada a la red 1 (la de Samsung)` y dejo el fichero en 1.
- La auto-prueba de la interfaz (`ctl hud`) ahora, aparte de recorrer los golpes del raton, mide cada
  rotulo con la fuente real y dice si cabe en su caja: con el boton nuevo la etiqueta
  `Red sin conversor` media 130,8 px dentro de una caja de 130, y `Tab  entrega la pantalla` se montaba
  sobre el boton; el boton paso de 150 a 160 px de ancho y la leyenda del Tab se bajo al hueco de debajo
  de los deslizadores. Las 13 medidas salen ahora con `cabe`.

## La tarjeta del aviso vuelve a salir sola, y la letra deja de salirse de su caja (v0.20)
- Sintoma: al pasar un video a pantalla completa ya no aparecia la tarjeta de aviso, y en la ultima vez
  que salio los textos y las cajas se salian del recuadro.
- **Por que dejo de salir**: el compositor cortaba la comunicacion de la capa porque le mandabamos el
  lienzo dibujado antes de su primera confirmacion de tamano ("a buffer has been attached to a layer
  surface prior to the first layer_surface.configure"). Al intentar arreglar la colocacion se seguia
  usando esa conexion muerta, así que la tarjeta no volvia nunca. Ahora no se manda nada hasta la
  confirmacion, la capa nueva espera la suya, y si el compositor corta la conexion se nota
  (`el compositor cerro la conexion de la capa superpuesta`), se suelta y se abre una capa nueva sola
  (`capa superpuesta reabierta en DP-4`).
- **Por que la letra se salia**: dos cosas.
  - El compositor anuncia su propia escala para esta capa: 1,80, no la 1,50 del escritorio. El recorte
    del lienzo se calculaba con la escala vieja y la tarjeta se dibujaba un 20 % mas grande que su
    propio lienzo. Ahora, si la escala cambia a mitad de camino, se recalculan lienzo y posicion
    (`la escala del compositor paso de 1,50 a 1,80: la tarjeta se reajusta`).
  - La plantilla de las esquinas redondeadas estaba mal calculada: a la distancia del rectangulo no se
    le restaba el radio, así que la parte opaca quedaba encogida un radio por cada lado. Toda caja,
    pastilla o tarjeta dibujada con esa plantilla salia unos 10 px logicos mas pequena que su recuadro
    y sin esquinas redondeadas. Medido píxel a píxel sobre el lienzo volcado: la caja pedia
    2651..3004 x 1883..2023 y el relleno solo ocupaba 176..287 de alto. Después del arreglo el relleno
    coincide exacto con la caja y las esquinas se ven redondeadas. Lo mismo sirve para la pastilla del
    visor, que usa la misma plantilla.
  - La separacion desde el borde de la pantalla ya no se pide con el margen del compositor (lo
    multiplica por su escala y se comia parte de la tarjeta): el lienzo llega hasta el borde y la
    separacion va dentro, en zona transparente.
- Comprobacion de la colocacion mas tolerante: que el compositor ofrezca un area MAS PEQUENA que la
  pedida es normal (800x267 logico = 668x224 en su escala 1,8); solo una oferta claramente MAS GRANDE
  delata otra pantalla. Antes saltaba el aviso en cada pasada.
- Menos ruido en la tarjeta: fuera la opcion "estereo vertical" (quedan Lado a lado, 2D plano y Video
  normal) y el texto ya saca el euro, la raya y las comillas tipograficas en vez de signos de
  interrogacion: el lector de texto convertia cualquier caracter fuera del rango latino en "?".
- Probado con una ventana real a pantalla completa (Zen): `tarjeta puesta por zen | ... pantalla DP-4`,
  sin error de protocolo y la tarjeta se queda en el rincon inferior derecho del Odyssey.

## El visor dejaba de irse al monitor normal: causa encontrada (v0.19)
- Síntoma que daba la prueba de v0.18: la imagen se veía como una imagen 2D normal con los bordes
  difuminados o punteados. No era la red ni el relieve: con dos monitores conectados el visor elegía
  **el monitor viejo (DP-7, 1600x1200)**. En un panel sin la lámina lenticular la trama de columnas se
  ve directamente como una rejilla de puntos, y ahí no hay 3D que valga.
- El fallo estaba en `Panel::pickMode`: si un monitor no tenía modo 3840x2160 devolvía **el modo en
  curso** en vez de "no lo encuentro", así que `resolveOutput` se creía que el primer monitor de la
  lista era el Odyssey. Ahora devuelve -1 cuando no hay modo nativo, y la elección de la salida solo
  admite monitores con 3840x2160 (con preferencia por el que se anuncia como Odyssey / SyncMaster / G90).
- Si ningún monitor tiene ese modo, lo dice en vez de quedarse callado con uno equivocado.
- El vigilante pasa su `pantalla =` al visor (`--output DP-4`): panel, ventana y tarjeta van a la misma
  pantalla sin adivinar.
- Comprobado con el flujo real: panel DP-4 en modo 20 3840x2160@119,88 con escala 1 y rotación normal,
  lienzo 3840x2160 a píxel a píxel (sin el aviso de escala) y el mapa hecho en 12 ms con la red oficial.

## La tarjeta ya no se estira ni se va a otra pantalla, y se puede usar la red de Samsung (v0.18)
- La tarjeta del aviso seguía cayendo al otro monitor (un Sun Electronics X7149A). Además de lo de v0.17
  habia dos fallos mas:
  - **La elección con `pantalla=DP-4` no tenía respaldo**: si la pista no coincida con ninguna pantalla
    (la pantalla se acaba de anunciar y aún no trae nombre), el programa se quedaba con la primera de
    la lista, que era la del monitor equivocado. Ahora la elección es en cadena: la pista, si no el panel
    que se reconoce como Odyssey, y si no el de mas pixeles.
  - **Se elegía el monitor con datos incompletos**: ahora, si la pantalla elegida aún no tiene nombre
    ni modos, se esperan hasta 240 ms a que el compositor los mande y se vuelve a elegir.
  - **El compositor manda en el configure el área útil de la pantalla donde puso la capa, y nosotros la
    usábamos como tamaño de la tarjeta**: la capa acababa ocupando media pantalla (por eso se podía
    "mover" con el raton y tapaba de mas). El tamaño pedido se respeta; el configure solo recorta.
  - **Comprobación de la colocación**: lo que ofreció el compositor delata en qué pantalla cayó la capa.
    Si la tarjeta no cabe ahi, se destruye y se rehace sobre la pantalla pedida (dos intentos) y queda
    en el registro.
- Comprobado con dos monitores conectados: la tarjeta aparece en el rincon inferior derecho del Odyssey
  y no hay nada en el otro monitor (capturas de pantalla comparadas).
- El hub puede usar la red de profundidad oficial de Samsung: `modelo = RUTA` en
  ~/.config/odyssey3d/serve.conf o `--modelo RUTA` en `serve`. Ya esta puesta la linea con
  ~/.local/share/odyssey3d/modelos/mde_large.onnx; se quita comentando la línea para volver a la nuestra.
  La primera pasada: 31 ms por mapa en la gráfica NVIDIA, al mismo ritmo que con la nuestra.

## La tarjeta del aviso siempre va al panel del Odyssey (v0.17)
- Pasaba esto: con dos monitores conectados (el Odyssey y un Sun Electronics X7149A de 1600x1200) la tarjeta
  aparecio en la pantalla equivocada. Tres cosas lo provocaban, y las tres estan corregidas:
  - **El monitor se elegia una sola vez**, al arrancar el servicio. Ahora se vuelve a elegir cada vez
    que se pone la tarjeta, así que un monitor conectado despues, una suspension o un cambio de modo
    de la lente ya no dejan la tarjeta en un sitio viejo.
  - **El compositor puede retirar y volver a anunciar una pantalla** (pasa al cambiar el modo 3D de la
    lente). No haciamos caso al aviso de "global removed", así que la capa se creaba con un monitor ya
    muerto y el compositor la colocaba donde queria. Ahora esa pantalla se marca como muerta, se suelta
    su objeto y la capa se rehace sobre la que sigue en pie.
  - **Se guardaba el ultimo modo anunciado, no el mas grande**, así que "la pantalla de mas pixeles" se
    podia calcular con un modo pequeno. Ahora se guarda el modo mas grande de cada pantalla.
- Y por las dudas, la eleccion ya no es solo "la de mas pixeles": se prefieren los monitores cuyo nombre
  o modelo contiene odyssey / syncmaster / g90. Se puede fijar a mano con `pantalla = DP-4` en
  ~/.config/odyssey3d/serve.conf (ya puesto en tu equipo) o con la variable ODYSSEY3D_PANTALLA.
- `ctl overlay` ahora lista every screen the compositor announces with its exact name, so finding the
  right `pantalla=` is easy, and the log of `serve` says on which screen the card was actually placed.
- Arreglados de paso tres mensajes de error que mandaban una cadena de C++ donde iba un %s (podian
  imprimir basura o cortar el programa al equivocar una opcion).
- Probado: con el navegador a pantalla completa, registro "tarjeta puesta por zen · pantalla DP-4
  (Samsung Electric Company Odyssey G90XF) 3840x2160", aguantada sin moverse de sitio.

## La tarjeta para elegir la conversion ya no se retira sola (v0.16)
- Estaba pasando que la tarjeta aparecia y a los 4 o 6 segundos se iba sola, y volvia: una oscilacion
  que no dejaba tiempo de pulsar nada. La causa: al comprobar si la tarjeta seguia teniendo sentido se
  volvía a preguntar por "la mejor ventana a pantalla completa" en vez de mirar la ventana concreta. En
  cuanto la tarjeta aparecia, el navegador perdía el foco un instante y la "mejor ventana" pasaba a ser
  otra, así que el vigilante consideraba que la original ya no estaba a pantalla completa, retiraba la
  tarjeta y la volvia a poner en el siguiente recorrido. En el registro se veian seis ciclos en 20 segundos.
- Ahora se comprueba esa ventana por su identificador (`find` + `usable()`), sin importar que haya otra
  ventana por delante o sin foco. La tarjeta se aguanta los 40 segundos de rigor y solo se va si ESA
  ventana sale de pantalla completa, y el registro dice con que estado se fue ("activa, maximizada"),
  que es la prueba de que el retiro es legitimo.
- Probado poniendo y quitando la pantalla completa de la ventana del navegador desde fuera: tarjeta
  puesta a los 2 s, aguantada 15 s sin un solo retiro, retirada al devolver la ventana a su tamano.

## El mapa ya no llega quebrado al panel: muestreo cubico, temblor y topes por ojo (v0.15)
- **El mapa de profundidad se interpola cubico.** El mapa llega de 518 x 518 y la pantalla es de 3840,
  o sea un punto del mapa cubre casi 8 pixeles. Con el filtro lineal el desplazamiento era continuo por
  pedazos: la pendiente (que se media a un punto de distancia) saltaba de golpe al cruzar de una celda
  del mapa a la siguiente, y ahi el recorte del canto se comportaba como un escalon. Ahora el cuadro
  usa la base cubica de Catmull-Rom, que pasa por los puntos, y la pendiente sale de la derivada de esa
  misma base: las 16 lecturas del mapa se hacen una sola vez. Medido sobre el mapa real que vuelca el
  visor (`../analisis-samsung/comparar-muestreo.py`): el desplazamiento medio entregido no cambia
  (7,73 → 7,68 px, así que el relieve no se sube ni se baja), el salto maximo de un pixel al siguiente
  baja de 12,26 a 10,59 px y la diferencia entre lo viejo y lo nuevo es de 0,31 px rms. Coste: nada
  medible (108,0 cuadros/s en ventana con conversor, igual que antes).
- **Temblor del desplazamiento (lo que ellos llaman `dither`).** Se le suma un patron fijo de 0,35 px al
  desplazamiento, pero solo donde el mapa esta llano (`canto = 1`), que es donde se ven las bandas de
  profundidad y donde el temblor no se nota porque el color de fondo es parejo. El programa de Samsung
  usa 0,25 px (`dither_strength 0,01`); aqui se puede cambiar con `ODYSSEY3D_DITHER=0` para apagarlo o
  subirlo.
- **El relieve pasa al presupuesto del programa de Samsung (decidido viendo el panel).** El config
  oficial no deja pasar de **+12,67 px por ojo hacia afuera y −9,98 px hacia adentro**
  (`virt.max_disparity 0,0033` / `min_disparity −0,0026`, en fraccion del ancho y por ojo), o sea
  **22,6 px de separacion binocular**, y en juegos medidos con esa capa la disparidad tipica era de
  6,8 a 12,3 px. El nuestro entregaba 65 px de separacion (32,5 por ojo), 2,9 veces mas. Ahora el
  deslizador a 50 vale 22,6 px y el cuadro ademas recorta cada ojo a los dos numeros de ellos, así que
  ningun objeto se sale del volumen comodo del panel. Ajuste a mano: `ODYSSEY3D_TOPE=0` quita el recorte
  y `ODYSSEY3D_RELIEVE=2.9` devuelve el recorrido viejo de 65 px; el arranque dice en el registro que
  modo esta puesto (`arranque: relieve 50 (23 px de separacion entre ojos a 3840 px de ancho, tope por
  ojo +12,67 / -9,98 px)`). Coste: ninguno (108,3 cuadros/s en ventana con conversor).
- **Correccion a lo que escribi en la entrada anterior:** ahi dije que los cuatro `.onnx.bin` del
  programa traian los pesos sustituidos por relleno. **Es falso.** El bloque `[DUMMY_START]…[DUMMY_END]`
  esta al FINAL del archivo y es solo relleno (1/3/3 MiB segun el modelo); detras vienen inicializadores
  fp16 con pesos de verdad: `mde_large` 49.477.899 bytes con 224 inicializadores y 49,4 MB de pesos,
  entrada `input` 1x3x518x518 y salida `depth` 1x518x518. El agente de Windows los descifro y los jo;
  aqui los baje con la misma clave y los corro con nuestro ONNX Runtime: cargan y valen, y cuestan lo
  mismo que la nuestra (17,3 ms la primera estimacion en la grafica, 108 cuadros/s en ventana con
  conversor cada 3 cuadros). En un cuadro real de nuestra muestra los dos mapas correlacionan 0,90–0,94;
  en la foto que ellos usaron solo 0,36, y la red de ellos tiene el rango de salida mucho mas ancho
  (0–13 frente a 0,7–4,4). Siguen siendo pesos propietarios de Samsung: no van en el repositorio, y se
  pueden probar con `--modelo RUTA`.
- **Hueco libre en el bloque uniforme:** `weave.frag` recibe ahora un `tun5` (temblor y los dos topes)
  en los 16 bytes que sobraban; el tamano del bloque sigue en 288 bytes.

- **El plano de apoyo lo elige la imagen, no un deslizador fijo.** Se hace un histograma del mapa de
  profundidad donde cada celda vale más si tiene cantos al lado y si cae en el centro del cuadro; el
  pico de ese histograma es el plano donde la separación entre ojos vale cero, o sea lo que queda
  "pegado al cristal". Tiene un suelo en la mediana para que un sujeto pequeño no arrastre el apoyo al
  fondo, y se mueve de forma suave (peso 0,06) para que el relieve no respire. Con las cifras nuevas el
  apoyo automático queda en 44–48 % del rango, justo donde antes había que ponerlo a mano en 55.
  `Ctrl+Shift+3`/`4` ahora **anulan** el apoyo automático (el visor se da cuenta de que moviste la
  profundidad y deja de perseguir la imagen); al reencender el conversor con `C` vuelve lo automático.
- **El mapa llega limpio al tramado.** Antes el mapa de la red se usaba tal cual. Ahora pasa por un
  filtro guiado (radio 8, épsilon 0,04) que usa la propia foto como guía para no comerse los cantos,
  un dilatar de 3 puntos para que el sujeto tape su propio borde, y el suavizado en el tiempo de
  siempre. Los números están copiados del programa oficial del Odyssey (sección `filter.depth`). Coste
  medido: de 9,6–12,2 ms a 13,0–15,5 ms por estimación, y como la red va en su propio hilo el cuadro no
  se entera (sigue a 106,9–112,6 cuadros/s en ventana y 37,8–38,4 en el modo 3D real del panel).
- **Los cambios de plano cortan, no arrastran.** Se compara la profundidad media con la anterior
  (muestra cada 41 celdas); si la diferencia pasa de 0,12 y hace más de 15 estimaciones del último
  corte, se tiran la escala acumulada y el apoyo y se fijan de golpe al cuadro nuevo. Sin eso, al pasar
  de un plano a otro el relieve se queda torcido medio segundo. Sale en el registro:
  `[conversor] cambio de plano (n): se reinicia la escala de profundidad y el apoyo`, y de vez en vez
  `[conversor] apoyo 46% del rango de profundidad (cortes 1, mapa 518 x 518)`.
- **Curva del relieve recalibrada y arranque dentro del rango.** La ganancia ahora es
  `0,003 + 3D Salida/100 × 0,028`, o sea **65 px de separación entre ojos** a 3840 px de ancho con el
  deslizador a 50 (32,5 px por ojo). El arranque del conversor ya no pone 55: los deslizadores de la
  pastilla solo dejan mover el relieve entre −50 y +50, así que 55 quedaba fuera y cualquier tecla lo
  recortaba sin que se notara por qué. Al encenderlo el registro dice los píxeles exactos.
- **Medido, no a ojo.** Con la limpieza apagada (`ODYSSEY3D_LIMPIEZA=0`) sobre ocho fotogramas reales:
  aspereza del mapa 0,68 con limpieza frente a 0,70 sin ella, y temblor entre cuadros 11,42 frente a
  11,51 niveles sobre 255. La limpieza apenas cambia cifras porque el mapa de la red ya venía suave; lo
  que de verdad se nota es el apoyo en el sujeto y el corte en los cambios de plano. El canto del sujeto
  se conserva igual (6 celdas con y sin filtro), que era el riesgo.

## Que hace el programa de Samsung por dentro (analisis, sin tocar codigo nuestro)
- **La cadena de etapas esta identificada**, en `Conversion\`: `depth.dll` + `mde.dll` (red de
  profundidad) → `disparity.dll` (una red pequena de disparidad) → `warp.dll` (desplazamiento por ojo)
  → `inpaint.dll` (relleno de huecos en cantos) → `degraph.dll` (compensacion de fantasma). El envoltorio
  `ConversionWrapper.dll` solo exporta `GetFPS`, `Start` y `Stop`.
- **Las siete DLL de etapa exportan unicamente `get_proc_address`** y estan empaquetadas (secciones con
  nombres basura), así que desarmarlas sin un volcado de memoria en marcha no da nada util. Los seis
  volcados que ya habia en la carpeta tampoco sirven: cero apariciones del modelo.
- **El "cifrado" de la configuracion y de los modelos es un XOR con una clave fija de 229 bytes**,
  alineada en el offset 0, la misma para todo. Con la clave en la mano el config
  (`conversion-config.json.bin`, 4521 bytes) se lee y se reescribe. Los cuatro modelos `.onnx.bin` del
  programa **traen los pesos sustituidos por relleno** (`[DUMMY_START]` + 1/3/3 MiB de datos aleatorios
  + `[DUMMY_END]`) y el config lleva `model_update: true`: los pesos reales se descargan. El de
  disparidad (`Disp_Disp_1008_ver1.onnx.bin`, 465 KB) si trae pesos reales.
- **Cifras oficiales que si copiamos**: filtro guiado radio 8 / epsilon 0,04 / dilatar, umbral de cambio
  de escena 0,12 con descanso de 15 cuadros, apoyo por saliencia con peso real 0,02, presupuesto angular
  `virt.min_disparity = -0,0026` y `max_disparity = +0,0033` (unos -10/+13 px, que entendemos por ojo:
  unas tres veces menos que lo nuestro), y las etapas que vienen apagadas de fabrica (`vcsr`, `degraph`,
  `jbu`, `depth_blur`, `gauss_blur`). No se ha guardado el config ni la clave en el repositorio: son
  propiedad de Samsung; las copias estan en `../analisis-samsung/`.
- **Lo que falta saber se pidio a un agente de Windows**: el prompt listo para ese trabajo esta en
  `../prompt-qwen-windows.md`.

## El relieve baja de 98 a ~66 px (valor de fabrica)
- Con 98 px de separacion entre ojos molestaba a los ojos, y con 50 casi no se notaba. Se fijo el
  arranque en el termino medio (~66 px, luego 65 px con la curva nueva).

## El relieve se sube: curva mas fuerte, arranque plano y medicion en pantalla
- **Curva del deslizador "3D Salida" casi duplicada.** Antes, a mitad de deslizador, la separacion
  maxima entre la vista del ojo izquierdo y la del derecho era de ~50 px en una pantalla de 3840 px de
  ancho; ahora es de ~98 px (2,6 % del ancho), que es donde este panel aun se fusiona comodo. El
  arranque del conversor pone el deslizador en 60 en vez de 50 y el plano de referencia en 60, o sea
  mas contenido por delante del cristal: con 50/50 se veia casi plano.
- **Al encender el conversor ya no se parte la pantalla.** Sin mapa de profundidad (el primer medio
  segundo, o si la red falla) el tramado caia en la rama de «lado a lado» y se veian dos medias
  pantallas una junto a la otra. Ahora, con el conversor puesto y sin mapa, los dos ojos reciben la
  imagen entera y plana.
- **Se ve de cuanto hablamos.** Al encender el conversor el registro dice la separacion en pixeles
  entre ojos y el plano de referencia, y en pantalla aparece un aviso de 9 segundos con las teclas
  `Ctrl+Shift+6` (subir) y `Ctrl+Shift+5` (bajar), para ajustar el relieve mirando el panel.
- **Volcado util.** Con `--dump` ahora se guarda también `-trama-con-mapa.png`: la trama leida de
  Vulkan con el mapa ya puesto. El volcado de siempre se hacía antes de la primera estimacion, así que
  nunca dejaba ver el relieve aplicado.

## El conversor aguantado: cantos limpios, red en su propio hilo y tarjetas llenas
- **Los cantos ya no se babeen.** En el tramado, donde el mapa de profundidad cambia rápido en
  horizontal (un borde) se recorta el desplazamiento de cada ojo. Sin eso, el relleno del mapa —que
  llega de 518×518 y se estira a 3840— estira el color en peine justo en los bordes de los objetos. La
  constante `CantoSuave` de `weave.frag` (0,004) manda: subirla apaga más el desplazamiento en los
  cantos, bajarla devuelve relieve.
- **La red ya no congela el cuadro.** La estimación corre en un hilo aparte: el visor baja la imagen
  chica, se la pasa y sigue dibujando; el mapa se sube cuando está listo. Antes, con la red por CPU
  (150 ms), cada estimación paraba el cuadro entero.
- **Elección de tarjeta con criterio.** Antes de abrir el modelo se mira la memoria libre de cada GPU;
  si en la pedida no caben ~1,1 GB se prueba en la que más tenga y, si en ninguna cabe, se usa la CPU
  con avisos claros (`cada` sube solo a 10). En esta máquina, con 246 MB libres en la 4090 y 1,8 GB en
  la 5080, el conversor ahora funciona por CPU en vez de quedarse mudo (antes: `CUDA error ... out of
  memory` en el nodo `/Resize` y nada de relieve).
- **Mapas de corrección encontrados desde cualquier sitio.** Faltaban dos rutas de búsqueda
  (`<carpeta del ejecutable>/../calib` y `/usr/local/share/odyssey3d/calib`): lanzando el binario del
  `build` desde otra carpeta se trabajaba con corrección neutra sin decir por qué.

## Los vídeos normales también salen en relieve: conversor 2D a 3D en tiempo real (v0.12)
- **Qué hace.** Ya no hace falta que el vídeo traiga dos imágenes. Con el conversor puesto, el visor
  mira el fotograma normal, deduce qué está cerca y qué está lejos, y desplaza cada ojo unas decenas de
  píxeles según esa profundidad: lo cercano se sale por delante del cristal y lo lejano se mete dentro.
  Es el equivalente local del «3D Re-Mastering» del programa de Windows. Se ve en el mapa guardado con
  `--dump`: de una página de texto con una foto, la red separa la cara y las manos de la pared del fondo.
- **Cómo se mide la profundidad.** *Depth Anything V2 Small* en ONNX (99 MB, en
  `~/.local/share/odyssey3d/modelos/`), con el complemento de CUDA: **9,2 ms** por cuadro de 518×518.
  La imagen de la ventana se reduce a ese tamaño en la propia GPU (un pipeline pequeño del visor), se
  copia a un mapa de 8 bits y ése es el que usa la trama. Como el visor va a ~25 fotogramas por segundo
  y la red no da para uno por cuadro, la profundidad se recalcula **cada `--cada N` fotogramas**
  (por defecto 3) y se suaviza entre estimaciones; el desplazamiento se aplica en el shader de la trama,
  que es gratis.
- **Dos fallos que hubo que cazar.** (1) El pase pequeño muestreaba con la coordenada del shader de
  vértices, que ya lleva dentro el campo afín de la lente: salían franjas de colores en vez de la
  imagen. Ahora ese pase usa la coordenada normal («copia pura»). (2) El signo del desplazamiento estaba
  al revés: lo cercano se metía dentro. Corregido y medido sobre un cuadro sintético: el objeto que
  sale da +4,9 px de disparidad y el fondo −5,3 px. También se bajó la ganancia, que era 20 veces
  excesiva: ahora es `0,003 + 3D Salida/100 × 0,020`, o sea ±28 px a 3840 con el relieve a medias.
- **Dónde se activa.** Cuarta tarjeta del hub («Vídeo normal · 2D a 3D») junto a las otras tres, tecla
  `C` para encenderla y apagarla, y `M` para recorrer los cuatro formatos. La casilla de «no volver a
  preguntar» guarda `auto = app:conv`; en `serve.conf` valen `modo = conv`, `red = N` y `cada = N`.
  Comprobado con clic simulado sobre la tarjeta: los cuatro botones caen donde se ven.
- **Con la GPU ocupada la red se mueve sola.** Con el 4090 lleno (20,7 GB de otra aplicación) los
  repartos de la red fallaban con «sin memoria de dispositivo» y el conversor se quedaba mudo. Ahora, si
  el reparto en la gráfica 0 falla, se reintenta en la 1 y el registro lo dice; `--red 1` (o `red = 1` en
  `serve.conf`) es la forma de fijarlo de antemano. En la 5080 la estimación tarda lo mismo (9,3 ms). El
  error de ONNX Runtime, que venía con rutas de compilación y plantillas de C++ por delante, se resume en
  una línea: `[conversor] el modelo no abrio: CUDA failure 101: invalid device ordinal`.
- **Ratón descolocado en la ventana del visor.** La interfaz se dibuja en píxeles físicos del panel
  (decisión de v0.11) pero GLFW entrega la posición del ratón en puntos lógicos de la ventana: con el
  panel en modo normal (escala 1,5) los clics caían a dos tercios de donde estaba el botón, así que las
  tarjetas y la pastilla eran imposibles de pulsar a escala fraccionaria. Ahora la posición se
  reescala al lienzo antes de mirar qué se pulsó.
- **Herramientas de prueba.** `tools/prueba_red.cpp` (la red suelta sobre una foto),
  `tools/prueba_profundidad.cpp` (mapa de profundidad a PGM) y `tools/prueba_dibr.cpp` (señal de
  disparidad sobre un cuadro sintético). `--dump RUTA` vuelca también la copia de 518 que ve la red
  (`RUTA-chico.ppm`) y el mapa que produce (`RUTA-profundidad.pgm`), que es como se verificó esto.
- **Rendimiento medido.** Sin conversor: 62,7 f/s en modo normal y ~30 f/s en modo 3 real. Con
  conversor: 25–26 f/s en ventana y 25,7–27,8 f/s en modo 3. El presupuesto por cuadro en modo 3 es
  8,3 ms, y de ahí que la red vaya cada varios cuadros y no en cada uno.

## La pastilla se veía pequeña en el modo 3D: regla corregida y tamaño ajustable en marcha (v0.11)
- **Por qué se veía pequeña.** En v0.10 la interfaz del visor se medía con el tamaño *lógico* del
  escritorio. El problema es que el modo 3D del panel **se lleva la escala del escritorio**: al pasar al
  modo 3 el panel queda a `3840x2160 con escala 1`, así que ventana lógica y lienzo son lo mismo y la
  escala de la interfaz salía **1,00**. La pastilla está dibujada con letras de 11 a 15 unidades, que a
  escala 1 son 11–15 píxeles reales en un panel de 4K: ilegible. Se veía en el propio registro de la
  sesión: `[ventana] lienzo 3840x2160, ventana logica 3840x2160: la interfaz se dibuja a escala 1.00`.
- **Regla nueva, distinta para cada capa.** El visor se mide en **píxeles físicos** del panel (aquí ×2),
  así que la pastilla mide siempre lo mismo con el panel en modo 2 o en modo 3; la tarjeta del hub sigue
  midiendo en puntos lógicos (×1,5 aquí) porque ésa vive en el escritorio y nunca aparece sobre el panel
  en 3D. El registro lo dice: `lienzo 3840x2160 fisicos, ventana logica 2560x1440: la interfaz se dibuja
  a 2.00 veces`.
- **Tamaño a mano, sin recompilar.** `Ctrl+Shift+7` achica y `Ctrl+Shift+8` agranda la pastilla en marcha
  (de 0,5× a 3×, de 0,25 en 0,25), y `--ui-escala X` arranca ya con ese tamaño. Probado con teclas
  sintéticas (`uinput`) sobre una sesión real; el registro recogió la secuencia
  `2.00 → 1.75 → 1.50 → 1.75 → 2.00 → 2.25` y las capturas muestran el crecimiento sin que la pastilla
  se salga del panel.
- **La frase bajo la pastilla ahora avisa de lo que de verdad arruina la imagen:**
  `… · si el video es normal (una sola imagen) se vera doble: tecla 3 o 2D plano`. El registro del hub
  muestra que las sesiones de prueba eran `sbs` sobre vídeos normales de YouTube (con lente en 3D, scaler
  activado y el panel en modo 3, todo correcto): tramar un vídeo que no lleva dos imágenes produce justo
  la imagen doble y movida de las capturas. Para comprobar el 3D sin vídeos existe el patrón real de
  profundidad: `odyssey3d test` (ver abajo `test`).

## La interfaz deja de verse gigante y `Tab` devuelve el ratón al navegador (v0.10)
- **La interfaz se medía con la regla equivocada.** La escala de la capa salía del alto *físico* del panel
  partido por 1080, o sea 2, y con el escritorio al 150 % salía todo un 50 % más grande de lo que tocaba.
  Medido sobre la captura del usuario (3840×2160): la tarjeta del hub medía **1040×432 px**, exactamente los
  520×216 puntos que dibuja el código multiplicados por 2. Ahora `Hud::begin()` recibe también el alto *lógico*
  y la escala sale de la relación físico/lógico (recortada a 1..2). Con el DP-4 a 2160 físicos y el escritorio al
  150 % (1440 lógicos) la escala pasa a **1,5** y la tarjeta a **780×324 px**, verificado midiéndolo en una
  captura propia. El hub saca el alto lógico de su propia escala fraccional Wayland (`wp_fractional_scale`) y el
  visor del tamaño de la ventana de GLFW frente al lienzo, y lo dice al arrancar:
  `[ventana] lienzo 3840x2160, ventana logica 2560x1440: la interfaz se dibuja a escala 1.50`.
- **`Tab` entrega la pantalla.** El visor es una ventana a pantalla completa (es la única forma de quedar por
  encima del vídeo a pantalla completa del navegador: KWin 6.8 no sabe excluir ventanas de la captura y GLFW no
  expone la superficie Wayland para recortar la zona de entrada), así que se queda el ratón y el teclado. Con
  `Tab` apagamos el 3D del panel y nos minimizamos, y el ratón y el teclado vuelven a lo de detrás; al
  devolverle el foco se vuelve a poner todo. Verificado con el hardware real, midiendo el panel entre medias:
  | momento | panel (DP-4) | trazas |
  |---|---|---|
  | sesión en marcha | modo 3 (`3840x2160@119.88`) | `[scaler] /dev/sg1: modo 3D activado` |
  | después de `Tab` | **modo 2 (`3840x2160@164.98`, escala 1,5)** | `[scaler] modo 3D apagado`, `[lente] devuelta a 2D`, `[pantalla] entregada…` |
  | al devolver el foco | modo 3 otra vez | `[pantalla] recuperada: lente, scaler y modo 3D otra vez puestos` |
  Volver a la ventana es cosa del escritorio: **`Meta+Alt+Tab`** (el cambio de ventanas que sí lista las
  minimizadas; el `Alt+Tab` de salto se salta las minimizadas y por eso no funciona) o un clic en la barra de
  tareas. `ESC` sigue siendo salir de la sesión. El aviso está también en la pastilla (`Tab entrega la
  pantalla`) y en la ayuda de consola, y mientras estamos minimizados no se traza ningún cuadro.
- Quitada una pista falsa: **`Ctrl+Shift+2` sólo esconde la pastilla**, no libera el ratón (eso ahora lo hace
  `Tab`). El texto que lo decía está corregido en la ayuda y en el aviso del navegador.
- **La tarjeta de «¿qué formato tiene el vídeo?» ya no se repite.** La elección hecha en el hub se pasa al visor
  con `--sbs` / `--tb` / `--2d`, y con el formato ya decidido el visor no vuelve a pintarla encima del vídeo
  (antes sí: en la captura del usuario se veían a la vez la tarjeta y la pastilla durante la sesión).

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
