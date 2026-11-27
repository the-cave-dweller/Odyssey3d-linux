#!/usr/bin/env python3
"""Las imagenes del README (v1.07), en ingles (docs/img) y en español (docs/img/es).

  python3 tools/capturas-readme.py

Las de la interfaz se pintan con `odysseyvk ctl hud` en un HOME aparte (nada toca la configuracion
real ni abre ventanas); la biblioteca de Steam se lee, sin tocarla, por un enlace. Las de la
profundidad son medidas de cuadros reales (v0.89 y v0.90): aqui solo se les repintan los rotulos
en cada idioma, sobre la imagen guardada en docs/img/fuente."""
import os, shutil, subprocess, sys, tempfile, time
from PIL import Image, ImageDraw, ImageFont

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BIN = os.path.join(RAIZ, "build", "odysseyvk")
FUENTE = "/usr/share/fonts/TTF/DejaVuSans.ttf"
W, H = 3840, 2160   # el panel del Odyssey: todo sale al doble, nitido

def fondo(w, h):
    """un escritorio de mentira: degradado morado y azul"""
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(0, w):
            t = x / max(1, w - 1); u = y / max(1, h - 1)
            px[x, y] = (int(70 + 90 * (1 - t) * (1 - u) + 40 * u), int(48 + 30 * u), int(120 + 80 * t))
    return im

def pinta(casa, estado, lengua, nombre, **env):
    salida = os.path.join(casa, nombre + ".png")
    e = dict(os.environ, HOME=casa, XDG_CONFIG_HOME=os.path.join(casa, ".config"), XDG_STATE_HOME=estado,
             ODYSSEYVK_IDIOMA=lengua, ODYSSEYVK_HUD_PNG=salida)
    e.update({k: str(v) for k, v in env.items()})
    subprocess.run([BIN, "ctl", "hud", "--w", str(W), "--h", str(H)], env=e, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    im = Image.open(salida).convert("RGBA")
    return im

def sobre(im, caja, margen=48):
    """recorta `caja` de la capa transparente y la pone sobre el degradado"""
    x0, y0, x1, y1 = caja
    x0, y0 = max(0, x0 - margen), max(0, y0 - margen)
    x1, y1 = min(im.width, x1 + margen), min(im.height, y1 + margen)
    c = im.crop((x0, y0, x1, y1))
    bg = fondo(c.width, c.height).convert("RGBA")
    bg.alpha_composite(c)
    return bg.convert("RGB")

def caja_alfa(im, zona):
    return im.crop(zona).getbbox() and tuple(a + b for a, b in zip(im.crop(zona).getbbox(), (zona[0], zona[1]) * 2))

def guarda(im, ruta, ancho=None):
    if ancho and im.width > ancho:
        im = im.resize((ancho, round(im.height * ancho / im.width)), Image.LANCZOS)
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    im.save(ruta, optimize=True)
    print("  ", os.path.relpath(ruta, RAIZ), im.size)

def interfaz(lengua, destino):
    tmp = tempfile.mkdtemp(prefix="capturas-")
    casa, estado = os.path.join(tmp, "home"), os.path.join(tmp, "state")
    os.makedirs(os.path.join(casa, ".config", "odysseyvk"))
    os.makedirs(os.path.join(estado, "odysseyvk"))
    with open(os.path.join(casa, ".config", "odysseyvk", "serve.conf"), "w") as f:
        f.write("bandeja = si\nbarra_tareas = si\n")
    version = open(os.path.join(RAIZ, "VERSION")).read().strip()
    # (v1.12) GitHub con una version mas: la fila de la version en Ajustes y la tarjeta dicen lo mismo
    siguiente = "%.2f" % (float(version) + 0.01)
    with open(os.path.join(estado, "odysseyvk", "actualizacion"), "w") as f:
        f.write("remota = %s\nhora = %d\n" % (siguiente, int(time.mktime(time.strptime("2026-11-26 09:30", "%Y-%m-%d %H:%M")))))
    steam = os.path.expanduser("~/.local/share/Steam")
    if os.path.isdir(steam):
        os.makedirs(os.path.join(casa, ".local", "share"))
        os.symlink(steam, os.path.join(casa, ".local", "share", "Steam"))
    try:
        mitad = (W // 2, 0, W, H)
        # (v1.12) la pastilla en Auto con su menu abierto; la tarjeta de la pregunta ya no existe
        im = pinta(casa, estado, lengua, "pastilla", ODYSSEYVK_HUD_RECORDAR=0, ODYSSEYVK_HUD_FOCO=1,
                   ODYSSEYVK_HUD_MENU_NORMAL=1)
        guarda(sobre(im, caja_alfa(im, (W // 2, 0, W, 1300))), os.path.join(destino, "pill.png"), 1000)
        # el panel: primera vez (sin red), juegos, ajustes
        centro = (W // 2 - 900, H // 2 - 700, W // 2 + 900, H // 2 + 700)
        im = pinta(casa, estado, lengua, "primera", ODYSSEYVK_HUD_PANEL=0)
        guarda(sobre(im, caja_alfa(im, centro)), os.path.join(destino, "first-run.png"), 1400)
        # (v1.12) de aqui en adelante, como con la red en su sitio: `modelo` apunta a un archivo vacio
        # del HOME aparte y el panel no pone el aviso de que falta (la carpeta real ni se mira)
        red = os.path.join(tmp, "red.onnx")
        open(red, "w").close()
        with open(os.path.join(casa, ".config", "odysseyvk", "serve.conf"), "a") as f:
            f.write("modelo = %s\n" % red)
        im = pinta(casa, estado, lengua, "juegos", ODYSSEYVK_HUD_PANEL=1)
        guarda(sobre(im, caja_alfa(im, centro)), os.path.join(destino, "games.png"), 1400)
        # (v1.15) las integraciones de un juego (Cyberpunk, con lo suyo marcado con un punto), con el panel alto: sus
        # opciones de lanzamiento se leen, sin tocarlas, del localconfig.vdf de Steam por el enlace (nada se escribe)
        im = pinta(casa, estado, lengua, "integraciones", ODYSSEYVK_HUD_PANEL=2, ODYSSEYVK_HUD_INTEG=1091500,
                   ODYSSEYVK_HUD_TAMANO="820x1070")
        guarda(sobre(im, caja_alfa(im, (W // 2 - 1000, 0, W // 2 + 1000, H))), os.path.join(destino, "integrations.png"), 1400)
        # (v1.12) la pestana Ajustes entera, con el panel alto: rendimiento, fluidez, cabeza y programa
        im = pinta(casa, estado, lengua, "programa", ODYSSEYVK_HUD_PANEL=3, ODYSSEYVK_HUD_TAMANO="820x1070")
        programa = sobre(im, caja_alfa(im, (W // 2 - 1000, 0, W // 2 + 1000, H)))
        im = pinta(casa, estado, lengua, "act", ODYSSEYVK_HUD_ACTUALIZACION=siguiente)
        act = sobre(im, caja_alfa(im, (W // 2, H // 2, W, H)))
        hoja = fondo(programa.width + act.width, max(programa.height, act.height))
        hoja.paste(programa, (0, 0))
        hoja.paste(act, (programa.width, hoja.height - act.height))
        guarda(hoja, os.path.join(destino, "settings-program.png"), 1800)
        # (v1.12) sin language.png: el idioma es una fila mas de Ajustes, se ve en settings-program.png
        im = pinta(casa, estado, lengua, "atajos", ODYSSEYVK_HUD_PANEL=0, ODYSSEYVK_HUD_CORRIDA=255,
                   ODYSSEYVK_HUD_CAPTURA="prof+")
        guarda(sobre(im, caja_alfa(im, centro)), os.path.join(destino, "shortcuts.png"), 1400)
        im = pinta(casa, estado, lengua, "aviso", ODYSSEYVK_HUD_AVISO=1)
        guarda(sobre(im, caja_alfa(im, (W // 2, H // 2, W, H))), os.path.join(destino, "card.png"), 1100)
    finally:
        shutil.rmtree(tmp)

ROTULOS = {
    # imagen: [(franja y0, y1, [(x, color, en, es), ...]), ...], tamano de letra
    "player-bar.png": (26, [
        (0, 44, [(14, (240, 110, 110), "BEFORE: behind the bar, both copies of the video", "ANTES: detrás de la barra, las dos copias del vídeo")]),
        (274, 318, [(14, (120, 220, 140), "NOW: the video goes on, soft and dimmed, with the controls on top", "AHORA: el vídeo sigue, suave y atenuado, con los mandos encima")]),
        (548, 592, [(14, (240, 110, 110), "BEFORE: the title at the top, doubled", "ANTES: el título de arriba, doble")]),
        (722, 766, [(14, (120, 220, 140), "NOW: the title at the top, once", "AHORA: el título de arriba, una sola vez")]),
    ]),
    "depth-edges.png": (22, [
        (0, 44, [(10, (240, 110, 110), "BEFORE: where the map said the ship ends", "ANTES: donde el mapa decía que acaba la nave"),
                 (752, (120, 220, 140), "NOW: the cut sits on the edge of the picture", "AHORA: el corte, sobre el borde de la imagen")]),
    ]),
    "depth-map.png": (22, [
        (0, 40, [(10, (225, 225, 230), "video frame", "cuadro del vídeo"),
                 (826, (225, 225, 230), "depth map from the network (white = near)", "mapa de profundidad de la red (blanco = cerca)")]),
    ]),
    # (v1.12) la vista de depuracion de la profundidad: el cuadro de la nave y lo que ensena Ctrl+Mayus+9
    # en el visor aparte (tools/prueba-aparte.sh con ODYSSEYVK_VER_PROFUNDIDAD=1 y --dump)
    "depth-debug.png": (22, [
        (0, 40, [(10, (225, 225, 230), "video frame", "cuadro del vídeo"),
                 (826, (225, 225, 230), "Ctrl+Shift+9: orange = saliency · cyan = screen plane",
                  "Ctrl+Mayús+9: naranja = saliencia · cian = plano de la pantalla")]),
    ]),
    "subject-plane.png": (15, [
        (0, 22, [(6, (225, 225, 230), "before: edges + centre", "antes: bordes + centro"),
                 (486, (225, 225, 230), "now: saliency, like Samsung's Hub", "ahora: saliencia, como el Hub de Samsung")]),
    ]),
}

def rotulos():
    fuente = os.path.join(RAIZ, "docs", "img", "fuente")
    for nombre, (tam, franjas) in ROTULOS.items():
        origen = os.path.join(fuente, nombre)
        if not os.path.exists(origen):   # la primera vez: la de docs/img pasa a ser la fuente
            os.makedirs(fuente, exist_ok=True)
            shutil.copy(os.path.join(RAIZ, "docs", "img", nombre), origen)
        for k, destino in ((2, os.path.join(RAIZ, "docs", "img")), (3, os.path.join(RAIZ, "docs", "img", "es"))):
            im = Image.open(origen).convert("RGB")
            d = ImageDraw.Draw(im)
            f = ImageFont.truetype(FUENTE, tam)
            bg = im.getpixel((im.width - 3, 2))
            for y0, y1, textos in franjas:
                d.rectangle((0, y0, im.width, y1), fill=bg)
                for t in textos:
                    x, color = t[0], t[1]
                    d.text((x, (y0 + y1) // 2), t[k], font=f, fill=color, anchor="lm")
            guarda(im, os.path.join(destino, nombre))

if __name__ == "__main__":
    for lengua, destino in (("en", os.path.join(RAIZ, "docs", "img")), ("es", os.path.join(RAIZ, "docs", "img", "es"))):
        print(lengua)
        interfaz(lengua, destino)
    rotulos()
