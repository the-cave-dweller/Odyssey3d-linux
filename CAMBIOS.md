# Cambios de OdysseyVK (hasta v0.56: odyssey3d)

Las copias de cada versión están en `../Versiones/` (guardadas con `tools/versionar.sh ETIQUETA`).
Los textos viejos se dejan con el nombre que tenía el proyecto entonces: son historia.

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

## El arranque del 3D, en paralelo (v1.12)

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

## La pastilla: botón de encendido y Neural | Rendimiento en marcha (v1.12)

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
