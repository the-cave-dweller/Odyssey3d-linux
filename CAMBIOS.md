# Cambios de odyssey3d

Las copias de cada versión están en `../Versiones/` (guardadas con `tools/versionar.sh ETIQUETA`).

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
