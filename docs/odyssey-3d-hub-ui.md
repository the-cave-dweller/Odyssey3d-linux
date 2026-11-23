<!-- Copia local del informe de ingeniería inversa de la interfaz oficial (tomado de posts de r/Odyssey3D y documentación de Samsung) -->
# Samsung Odyssey 3D Hub / monitor Odyssey 3D G90XF — análisis de la interfaz y datos técnicos

Fuentes consultadas: web (búsquedas vía lite.duckduckgo.com), documentación oficial Samsung (manuales PDF del Download Center, soporte IE, Community EU), reseñas (iXBT, Stuff, HardwareZone, Tom's Guide, PCMag, cybereality, 3dnews), Reddit r/Odyssey3D (API PullPush) y **capturas de pantalla reales inspeccionadas pixel a pixel** (iXBT, cybereality, Stuff.tv). Lo que no aparece en fuentes se marca explícitamente como *no documentado*.

---

## 1) Dónde aparece la interfaz durante una película o un juego

Hay **tres capas distintas** de interfaz, y no deben confundirse con el OSD del monitor:

### A) La app "Odyssey 3D Hub" (launcher a pantalla completa)
Ventana a pantalla completa con fondo animado. Captura real del home ("RealityHub_Home", cybereality.com):
- Barra superior: campo **"Search"** con lupa (izquierda); a la derecha botón píldora **"Game Scan"**, icono de **engranaje** (ajustes), círculo **"?"** y **"X"** (cerrar).
- Panel central translúcido con esquinas redondeadas, con dos secciones: **"Registered"** (con icono "i") y **"Supported"** (con icono "i"), cada una con una fila de fichas (tiles) con carátulas de juego.
- Esquina superior derecha del panel: **dos iconos de vista** (cuadrícula / lista).
- Algunos tiles llevan una **insignia dorada** (PCMag la llama *"gold ribbon"*) en la esquina superior derecha (p. ej. The First Berserker: Khazan): son los juegos co-desarrollados con Samsung. PCMag: *"The Reality Hub displays compatible games in either a list or a grid for easy access"* y el escaneo funciona sea cual sea la tienda (*"whether you purchased it on Steam, the Epic Games Store, or somewhere else"*).
- El tile seleccionado se resalta con un **borde blanco/claro grueso**. **El cursor de flecha es visible** en la captura.
- Diálogo **"Register game"** (captura real, fondo púrpura desenfocado, panel con borde brillante morado): campos **"Game Installation Path"** + "?", texto *"Select the game file and add the setup path."*, botón **"Change"**; sección **"About Game"** con **Platform: Steam**, **Developer**, **Publisher**, **Genre**, **App Rating**; botones **"Unregister"**, **"Save"**, **"Close"**.
- Manual oficial: "In Odyssey 3D Hub, click a game to open its Steam page… select the installed path of the game from Odyssey 3D Hub".

### B) Popup de conversión — **esquina inferior derecha**
Aparece cuando la aplicación (reproductor, navegador, juego) **pasa a pantalla completa**. Samsung lo describe oficialmente: *"From the bottom right of the screen of the running app, select the desired screen mode (available mode options: 2D mode and side-by-side mode)"*. Dos redacciones documentadas según versión del software:
- Variante captada por cybereality (coincide con el how-to oficial de Samsung IE): título **"Would you like to convert the current video to 3D? [Select] the type of video playing."** con dos teselas numeradas **"2D Video"** y **"Side by Side Video"** (iconos de dos cabezas con etiquetas **L** y **R**), y arriba a la derecha **"?**" y **"✕"**.
- Variante captada por iXBT: título **"What type of video are you watching? Select the right option for 3D conversion."** con teselas **"Standard Video"** / **"Split Video"**, una **"?"**, una **"✕"** y una casilla **"Save video type and don't show again"**.

### C) HUD de ajustes 3D — **esquina SUPERIOR derecha** (durante la película/juego ya en 3D)
Captura real analizada (iXBT, foto de pantalla a 3840×2160). Es una **pastilla flotante, no un panel lateral ni un menú lateral**. Etiquetas exactas leídas en la imagen:

| Bloque | Texto exacto | Mando | Rango / valor visible |
|---|---|---|---|
| Botón 1 | **3D Off** | botón con atajo impreso **Ctrl+Shift+1** | — |
| Botón 2 | **Overlay Off** | botón con atajo impreso **Ctrl+Shift+2** | — |
| Slider 1 | **3D Depth** | "−" [slider] "+" + atajos **Ctrl+Shift+3 / Ctrl+Shift+4** | extremos **0 – 100**, valor actual **50** |
| Slider 2 | **3D Popout** | "−" [slider] "+" + atajos **Ctrl+Shift+5 / Ctrl+Shift+6** | extremos **−50 – +50**, valor actual **0** |

Confirmación textual independiente: PCMag — *"a small 3D overlay will appear at the **top right corner** of the screen with shortcuts to change the intensity of the **3D Pop-Out** and **3D Depth** features"*; iXBT — *"Rяд настроек преобразования… están en la esquina superior derecha"*; Community EU — *"another 3D settings overlay appears top right … you can turn it off"*.

**NO existe** en ese overlay control de **brillo, contraste, "view angle", tamaño/distancia de pantalla ni toggle de eye tracking**: no aparecen en ninguna captura ni en documentación. La distancia (55–95 cm) y el ángulo (±25°) son **recomendaciones físicas**, no ajustes software. HardwareZone afirma que el Hub *"offers options to adjust 3D depth, convergence, and toggles for eye-tracking"*: es la única fuente que menciona "convergencia" y conmutadores de eye tracking; **no lo he podido verificar** en capturas ni en documentación oficial.

Brillo/contraste/tamaño de pantalla están en el **OSD del monitor** (joystick): la captura de Stuff.tv muestra el OSD con pestañas **Game / Picture / PIP / OnScreen Display / System / Support / Exit**, fila de widgets circulares (Black Equalizer, Response Time, Adaptive-Sync, EdgeSync, Virtual Aim Point) y, dentro de Game: **3D PC Selection** (HDMI1), Response Time, Adaptive-Sync, Ultrawide Game View, Black Equalizer, Virtual Aim Point, Edge Lighting, **Screen Size**. Es el único ajuste 3D del OSD (coincide con manual: *"Menu path: Game → 3D PC Selection → Select Input Source"* y con Stuff: solo el OSD dice qué entrada es la 3D).

---

## 2) Orden, agrupación y rangos

Overlay superior derecho, de arriba hacia abajo:
1. Fila de dos acciones: **3D Off** · **Overlay Off** (con su atajo impreso debajo de cada una).
2. **3D Depth**: etiqueta, fila con botón "−", barra deslizante con relleno y **valor numérico en el centro**, botón "+"; debajo, línea con los atajos **Ctrl+Shift+3 / Ctrl+Shift+4**. Rango **0–100**, valor por defecto visible **50**.
3. **3D Popout**: misma estructura, atajos **Ctrl+Shift+5 / Ctrl+Shift+6**. Rango **−50 a +50**, valor visible **0**.

Es decir: **2 botones + 2 sliders** (no hay spinners, ni menús desplegables, ni casillas en el overlay). Los valores van acompañados de la lectura numérica en el centro del slider.

Popup inferior derecho: título (1–2 líneas) → dos teselas grandes lado a lado (con número 1/2 y etiqueta debajo) → casilla "Save video type and don't show again" (en la variante iXBT) → iconos "?" y "✕".

Valores típicos usados por usuarios (Reddit r/Odyssey3D): **Depth ~30–50**, **Popout ~50**.

---

## 3) Estética: color, tipografía, cursor, ocultar el panel

- **HUD de juego/vídeo**: pastilla **gris claro translúcida** (efecto "esmerilado"), esquinas redondeadas; etiquetas en gris/blanco; **acento VERDE** en la barra rellenada del slider y en el número del valor actual. Nota oficial de parche (v1.2.2): *"Overlay UI Visibility Improvement: **Adjusted background transparency and font style**"* — confirma que la transparencia del fondo y la tipografía del overlay se retocaron; también hay un hilo de soporte de Samsung sobre la opción **"Overlay Off"** (bug en v1.2.2).
- **App Hub / popups**: **acento MORADO/violeta** (degradados en las teselas y en el borde del diálogo, fondo 3D animado morado con dunas y una esfera). Samsung (Stuff) advierte que el fondo animado puede distraer. Panel de popup casi **negro translúcido** con texto blanco.
- **OSD del monitor** (para no confundir): contorno y tipografía en **cian/azul neón** sobre negro translúcido.
- **Tipografía**: sans-serif en todos los elementos; **el nombre de la fuente no está publicado** en ninguna fuente.
- **Cursor**: visible (flecha del sistema) en las capturas del Hub y del registro de juego. Si el cursor queda oculto durante el juego en 3D: **no documentado**.
- **Ocultar el panel**: **"Overlay Off" con Ctrl+Shift+2**; apagar el 3D con **Ctrl+Shift+1**. Los atajos **se pueden reasignar dentro del Hub** (PCMag: *"you can change the shortcuts in Reality Hub"*). Testimonio de usuario: si se pulsa **ESC** se apaga el 3D y no vuelve hasta reiniciar (fallo reportado; sin hotkeys personalizables en versiones antiguas).

---

## 4) Cómo se activa el 3D desde el software y qué muestra con una app a pantalla completa

1. Instalar **Samsung Odyssey 3D Hub** (Samsung.com/support o Microsoft Store) y **reiniciar** el PC. En documentos más antiguos y en varias reseñas el mismo software se llama **"Samsung Reality Hub"** (guía rápida oficial distribuida por B&H: *"install it by searching for 'Samsung Reality Hub' in the Microsoft Store"*).
2. Conectar **USB** (cable incluido; USB-B→USB-A): alimenta la **cámara estérea**; sin USB no hay 3D. La app **solo arranca si el monitor Odyssey 3D está conectado** (3dnews). Al abrirla, **se enciende una luz roja en el borde superior** (cámara + LED IR). Comprobar en Device Manager que la "stereo camera" está habilitada.
3. Windows: **HDR OFF**; resolución **3840×2160**; en el OSD: **DisplayPort Ver. 1.4** e **Input Port Ver. 2.0↑**; y **Game → 3D PC Selection** para indicar qué entrada es la del PC.
4. **Juegos**: el Hub escanea ("Game Scan") y se registra el juego (diálogo "Register game"); el juego se lanza desde el Hub (o con una clave especial sin registro, según iXBT). Los juegos **co-desarrollados** con Samsung llevan los ajustes 3D **dentro del menú del propio juego** (Display/Graphics); PCMag lo confirma y usuarios añaden que en esos juegos el overlay **no** funciona. Cyberpunk 2077: requiere 4K, pasa a pantalla completa solo, desactivar Chromatic Aberration/Film Grain, sin Frame Generation, y ajusta la profundidad con el ajuste in-game **"Motion Comfort"**.
5. **Vídeo/cualquier app**: con el Hub abierto, poner la app en **pantalla completa** (F11 en Chrome, Alt+Enter/F11 en players/juegos). **Sí detecta la ventana al pasar a fullscreen**: es el disparador del popup de la esquina inferior derecha que **pregunta el tipo de vídeo** ("2D Video"/"Side by Side Video" o "Standard Video"/"Split Video"). Tras elegir, se aplica la conversión y aparece el **overlay de arriba a la derecha**. Retardo reportado de **5–10 s** al cambiar a fullscreen y latencia en la conversión 2D→3D (reconocido en el manual: *"There may be some latency during conversion from 2D videos to 3D videos"*). La conversión se mantiene al navegar/cambiar calidad **mientras no se salga de fullscreen** (cybereality). Hay opción **"Auto Convert"** desde v1.1.7 (convierte sin preguntar).
6. Límites: conversión AI **solo Nvidia** (RTX 3080/4070 o superior); SBS también con AMD (RX 6900XT+) e Intel; **no DRM**; **no HDR**; **solo 4K**; **un solo espectador**; **stereopar solo horizontal** (no top-bottom, crítico para iXBT).

---

## 5) Datos técnicos públicos

**Panel/modos**
- 26,9", IPS WLED, **3840×2160**, **165 Hz**, 1 ms GtG, 1000:1, 10 bit, HDR10+, ~163 ppi; **pixel pitch 0,1554 mm (cálculo iXBT)**. Brightness medida 350 cd/m² blanco pleno / ~380 cd/m² en ventana 10%.
- Señal de entrada en 3D: sigue siendo **3840×2160@165 Hz**. Ninguna fuente documenta cambio de resolución o refresco de la señal al activar 3D; la pérdida de definición es **interna** (interlacado de puntos de vista).
- En 3D: **sin HDR**; brillo **igual** que en 2D; refresco completo **165 Hz** (megathread r/Odyssey3D). Resolución **por ojo 1920×2160** (megathread; coherente con alternancia 1:1 observada por iXBT).

**Óptica**
- **Microlentes cilíndricas / lente lenticular** delante del panel LCD + **eye tracking** + **view mapping** (Samsung, Display Daily, iXBT, Tom's Guide).
- Diagrama oficial reproducido por iXBT: columnas de subpíxeles alternando **L / R** bajo las lentes lenticulares; iXBT observa que las micro-lentes **"se alternan en tablero 1:1"**, por lo que la resolución estereoscópica es inferior a la nativa; a grandes ángulos se aprecia una **rejilla diagonal fina** incluso en 2D (es decir, la capa lenticular es permanente y visible, no "conmutable" ópticamente). La afirmación de Reddit de "lenticular conmutable/polarizada" **no está respaldada** por las fuentes técnicas.
- **Pitch / paso de la lente lenticular (µm o en píxeles): NO PUBLICADO** en ninguna fuente consultada (ni ficha EU, ni manuales, ni reseñas técnicas). Único dato duro: pixel pitch 0,1554 mm y alternancia 1:1 → inferencia propia (no oficial): cada punto de vista ocuparía ~2 píxeles de ancho, o sea ≈**0,31 mm** de período por pareja de vistas. Tratar como estimación, no como especificación.

**Eye tracking**
- **Cámara estérea** en el bisel superior + **iluminación IR** + **indicador rojo**; blorea/centra en el espectador (un solo usuario). El manual advierte de **parpadeo momentáneo de pantalla** durante el reconocimiento facial y de no exponer la cámara a luz directa ni tapar la cara.

**Requisitos oficiales del software** (manual / guía rápida)
- Instalar: Windows 10 64-bit 22H2+ o Windows 11; 4 GB libres (instalado ~2,9 GB); 2 GB RAM.
- 3D conversion (AI): i7 / Ryzen7 1700X+, **RTX 3080** (serie 30) o **RTX 4070** (serie 40); DDR5-5600 32 GB ×2; PCIe 5.0. AMD/Intel: **no soportado** para conversión AI.
- Side-by-Side: Nvidia RTX 3080/4070+ **o** AMD RX 6900XT / RTX 7900; **Intel también soportado**; mismos requisitos de RAM/PCIe. Mac y gráficas de portátil Intel: no soportado.
- Juegos: "depende del juego" (complejidad), con lista oficial oficial en PDF ("List of 3D-Supported Games", L35, enero-2026: Khazan, Lies of P, Stray, Palworld, Silent Hill 2, Black Myth: Wukong, Hogwarts Legacy, GTA Definitive Edition, Stalker 2, etc.).

**Versiones/bugs relevantes documentados**
- v1.1.7: **Auto Convert** para vídeo. v1.2.2: **Overlay UI Visibility** (transparencia de fondo y estilo de fuente). Notas de "Fix 3D Shortcut", "Fix Game Launch", "SR Hub Service Crash". PCMag: **atajos reasignables** en el Hub.

---

## URLs usadas (principales)
- Samsung IE (how-to oficial 3D): https://www.samsung.com/ie/support/displays/how-to-use-3d-on-the-odyssey-3d-gaming-monitor/
- Manual de usuario oficial (Download Center, S27FG900XW): https://org.downloadcenter.samsung.com/downloadfile/ContentsFile.aspx?CDSite=UNI_IN&OriginYN=N&ModelType=N&ModelName=S27FG900XW&CttFileID=10646167&CDCttType=UM&VPath=UM%2F202507%2F20250711130417435%2FBN81-28065G-01_WUG_G60SD_G61SD_G60SF_G81SF_G90XF_ASIA_MENA_AFRICA_ENG_250605.0.pdf
- Manual UE revisión 2026-08 (misma familia): https://org.downloadcenter.samsung.com/downloadfile/ContentsFile.aspx?CDSite=UNI_DE&OriginYN=N&ModelType=N&ModelName=S27FG900XU&CttFileID=11613829&CDCttType=UM&VPath=UM%2F202608%2F20260805114526001%2FBN81-28255A-02_WUG_Gaming-Series-Y25_EU_ENG_260728.0.pdf
- Guía rápida oficial con el nombre "Reality Hub": https://www.bhphotovideo.com/lit_files/1221746.pdf
- Lista oficial de juegos 3D (PDF): https://org.downloadcenter.samsung.com/downloadfile/ContentsFile.aspx?CDSite=UNI_DE&OriginYN=N&ModelType=N&ModelName=S27FG900XU&CttFileID=10533934&CDCttType=SW&VPath=SW%2F202601%2F20260116092236220%2F03_G90XF_3D_Game_List_WW_L35_260115.0.pdf
- Ficha EU (datasheet): https://cdn.e-misija.si/pdf/S27FG900XU.pdf
- Soporte Samsung (Overlay Off): https://www.samsung.com/us/support/troubleshoot/TSG10010571/
- Samsung Community EU (overlay superior derecho, retardo): https://eu.community.samsung.com/t5/computers-it/samsung-odyssey-3d-g90xf/td-p/12751531
- iXBT (reseña técnica; capturas del HUD y del popup, óptica, mediciones): https://www.ixbt.com/3dv/samsung-odyssey-3d-g90xf-review.html (diagrama lenticular: https://www.ixbt.com/img/r30/00/02/86/67/3dtech01.png)
- PCMag (overlay arriba a la derecha, atajos reasignables, Reality Hub): https://www.pcmag.com/reviews/samsung-odyssey-3d-g90xf
- Stuff.tv (solo 3D PC Selection en OSD, popup 2 opciones): https://www.stuff.tv/review/samsung-odyssey-3d-g90xf-review/ (captura OSD: https://www.stuff.tv/wp-content/uploads/sites/2/2025/03/Samsung-Odyssey-3D-review-OSD.jpg)
- HardwareZone (cámara dual IR, "depth, convergence, eye-tracking"): https://www.hardwarezone.com.sg/pc/accessories/samsung-odyssey-3d-g90xf-review-no-3d-glasses-required-what-cost
- Tom's Guide (Eye Tracking + View Mapping): https://www.tomsguide.com/computing/monitors/samsung-odyssey-3d-g90xf-review
- cybereality (home del Hub, popup "2D Video / Side by Side Video", usos con fullscreen/F11): https://cybereality.com/odyssey-3d-review-samsung-pulled-it-off-what-3d-always-should-have-been/ (img: https://cybereality.com/wp-content/uploads/2025/05/RealityHub_Home-1024x576.png , https://cybereality.com/wp-content/uploads/2025/05/Enable3D_Btn-1024x576.png)
- 3dnews.ru (app solo arranca con el monitor conectado, dos cámaras): https://3dnews.ru/1129794
- Display Daily (lenticular + eye tracking + view mapping): https://displaydaily.com/samsung-joins-the-glasses-free-3d-monitor-game/
- Cyberpunk 2077 soporte Odyssey 3D (Motion Comfort): https://support.cdprojektred.com/en/cyberpunk/pc/gameplay/issue/3018/samsung-odyssey-3d-support-how-to
- Reddit r/Odyssey3D (megahilo: 1920×2160/ojo, 165 Hz, sin HDR; popup en fullscreen; atajos Ctrl+Shift+2; notas de parche): https://www.reddit.com/r/Odyssey3D/comments/1klthtn/samsung_odyssey_3d_g90xf_megathread/ , https://www.reddit.com/r/Odyssey3D/comments/1vir0yn/ , https://www.reddit.com/r/Odyssey3D/comments/1rh1v00/ , https://www.reddit.com/r/Odyssey3D/comments/1txznr3/ , https://www.reddit.com/r/Odyssey3D/comments/1nd45al/ , https://www.reddit.com/r/Odyssey3D/comments/1pn8o6x/
- Microsoft Store (Odyssey 3D Hub): https://apps.microsoft.com/detail/xp8lx6slrzb813 (página JS; sin texto útil extraíble)
- PCMag/Wired/RTINGS: https://www.pcmag.com/reviews/samsung-odyssey-3d-g90xf (403 vía fetch, leído con curl), https://www.wired.com/review/samsung-odyssey-3d-27-inch-g90xf/ (contenido truncado), https://www.rtings.com/monitor/reviews/samsung/odyssey-3d-g90xf-s27fg900x (bloqueada)
- notebookcheck.net: no encontrada reseña propia del G90XF (URLs intentadas → 404)
