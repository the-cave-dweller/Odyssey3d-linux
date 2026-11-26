#!/usr/bin/env python3
"""La profundidad del juego (la que DLSS recibe) contra la de la red, en el mismo cuadro (v1.13).

Rodolfo: «check if any improvements can be made here with it, either with dlss5 method of getting
the depth or with the gpu part» y «Yes, set up the test». Neural Forge (github.com/labj1987/
neural-forge) vive dentro del juego como capa de Vulkan y sabe cual de sus imagenes es la
profundidad que recibe DLSS; su volcado (`neural-forge-cli shmctl dump-cache --holds 2`) la deja en
un archivo junto al color de ese mismo cuadro. Esto la pone frente a la de nuestra red:

  python3 tools/comparar-profundidad.py ~/.local/share/neural-forge/captures/preupscale-cache-<ms>
          [--hold N] [--salida CARPETA] [--relieve 50] [--gpu 1]

1. El color del volcado (HDR lineal, a la resolucion de render) se tonifica a una imagen normal y
   se lleva a 3840x2160 (con bandas si no es 16:9).
2. La profundidad del juego pasa a cercania: con Z invertida (lo lejano a 0) es el propio valor y
   con Z normal 1 - valor. En los dos casos es lineal en 1/distancia, que es lo que pide el relieve
   (la red de profundidad da lo mismo: disparidad relativa), asi que no hacen falta los planos
   cercano y lejano del juego. Se lleva a 0..1 con los percentiles 1 y 99, sin el cielo.
3. El visor aparte (tools/prueba-aparte.sh: sin ventana, sin tocar la sesion 3D) corre la red en
   ese cuadro y deja su mapa, su plano de la pantalla y su vista de profundidad (Ctrl+Mayus+9).
4. El plano de la pantalla del juego se pone en la misma cosa que eligio la red (la mediana de la
   profundidad del juego donde la red cae en su plano), para que las dos vistas se comparen igual.
5. Los dos mapas pasan por el MISMO camino (--mapa-fijo, con la imagen como guia de los cantos):
   la vista de profundidad y la vista del ojo izquierdo (sonda 16) de cada uno.
6. Sale `comparacion.png` (los mapas, las vistas de profundidad y un recorte de los dos ojos
   izquierdos en el canto mas marcado del juego) y `resumen.txt` con los numeros.

Nada toca el HOME de verdad: el visor corre con un HOME dentro de la carpeta de salida, que solo
enlaza (para leer) la carpeta de las redes. Solo numpy y Pillow."""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
APARTE = os.path.join(RAIZ, "tools", "prueba-aparte.sh")
FUENTE = "/usr/share/fonts/TTF/DejaVuSans.ttf"
W4K, H4K = 3840, 2160


def leer_volcado(carpeta, hold):
    with open(os.path.join(carpeta, "manifest.json")) as f:
        man = json.load(f)
    if man.get("format") != "neural-forge-cache-dump":
        sys.exit("no es un volcado de Neural Forge (manifest.json sin format neural-forge-cache-dump)")
    w, h = int(man["width"]), int(man["height"])
    cuadros = [c for c in man["frames"] if c.get("depth_valid")]
    if not cuadros:
        sys.exit("el volcado no trae profundidad (¿la caché de ediciones de Neural Forge estaba apagada?)")
    c = cuadros[min(hold, len(cuadros) - 1)] if hold >= 0 else cuadros[len(cuadros) // 2]
    col = np.fromfile(os.path.join(carpeta, c["files"]["colour"]), dtype="<f2")
    dep = np.fromfile(os.path.join(carpeta, c["files"]["depth"]), dtype="<f4")
    if col.size != w * h * 4 or dep.size != w * h:
        sys.exit(f"los archivos no miden {w}x{h}: color {col.size // 4} puntos, profundidad {dep.size}")
    col = col.reshape(h, w, 4)[..., :3].astype(np.float32)
    dep = dep.reshape(h, w).astype(np.float32)
    return man, c, np.nan_to_num(col, nan=0.0, posinf=0.0, neginf=0.0), np.nan_to_num(dep, nan=0.0)


def tonificar(col, exposicion):
    """HDR lineal -> sRGB de 8 bits: la exposicion del volcado si la hay, luego una clave automatica
    (media logaritmica a 0,18) y la curva ACES de Narkowicz"""
    x = col * (exposicion if exposicion and exposicion > 0 else 1.0)
    lum = 0.2126 * x[..., 0] + 0.7152 * x[..., 1] + 0.0722 * x[..., 2]
    clave = float(np.exp(np.mean(np.log(np.maximum(lum, 1e-4)))))
    x = x * min(max(0.18 / max(clave, 1e-4), 0.25), 8.0)   # topada: una escena oscura sigue oscura
    x = np.clip((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0)
    x = np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)
    return (np.clip(x, 0, 1) * 255 + 0.5).astype(np.uint8)


def cercania(dep):
    """profundidad del dispositivo -> cercania 0..1 (1 = cerca) y la mascara de lo lejano (cielo)"""
    n0, n1 = int(np.count_nonzero(dep <= 0.0)), int(np.count_nonzero(dep >= 1.0))
    h = dep.shape[0]
    if max(n0, n1) > 0.002 * dep.size and n0 != n1:
        invertida = n0 > n1                      # el cielo se limpia a lo lejano: 0 con Z invertida
        motivo = f"{n0} puntos a 0 y {n1} a 1"
    else:                                         # sin cielo: el suelo (abajo) suele estar mas cerca
        abajo, arriba = float(dep[3 * h // 4:].mean()), float(dep[:h // 4].mean())
        invertida = abajo > arriba
        motivo = f"media abajo {abajo:.4f} y arriba {arriba:.4f}"
    n = dep if invertida else 1.0 - dep
    lejos = (dep <= 0.0) if invertida else (dep >= 1.0)
    vale = ~lejos
    if np.count_nonzero(vale) < 100:
        sys.exit("la profundidad del volcado esta vacia (todo a lo lejano)")
    lo, hi = np.percentile(n[vale], [1, 99])
    m = np.clip((n - lo) / max(hi - lo, 1e-9), 0.0, 1.0)
    m[lejos] = 0.0
    return m, invertida, motivo, float(np.count_nonzero(lejos)) / dep.size


def a16x9(img, relleno):
    """con bandas a los lados (o arriba y abajo) hasta 16:9; devuelve la imagen y la caja util"""
    h, w = img.shape[:2]
    if abs(w / h - 16 / 9) < 0.01:
        return img, (0, 0, w, h)
    if w / h < 16 / 9:
        W, H = int(round(h * 16 / 9)), h
    else:
        W, H = w, int(round(w * 9 / 16))
    fondo = np.full((H, W) + img.shape[2:], relleno, dtype=img.dtype)
    x0, y0 = (W - w) // 2, (H - h) // 2
    fondo[y0:y0 + h, x0:x0 + w] = img
    return fondo, (x0, y0, w, h)


def escribir_pgm(ruta, m):
    a = (np.clip(m, 0, 1) * 255 + 0.5).astype(np.uint8)
    with open(ruta, "wb") as f:
        f.write(b"P5\n%d %d\n255\n" % (a.shape[1], a.shape[0]))
        f.write(a.tobytes())


def leer_pgm(ruta):
    return np.asarray(Image.open(ruta)).astype(np.float32) / 255.0


def visor(casa, salida, nombre, entorno, args, gpu):
    """una pasada del visor aparte; devuelve su registro"""
    env = [f"HOME={casa}", f"XDG_CONFIG_HOME={casa}/.config", f"ODYSSEYVK_GPU={gpu}",
           "ODYSSEYVK_FUENTE_VIVA=1", "ODYSSEYVK_VOLCADO_CUADRO=150"] + entorno
    orden = [APARTE, "-t", "60", "-e", os.path.join(salida, "estado")] + env + ["--"] + args + \
            ["--sin-interfaz", "--dump", os.path.join(salida, nombre)]
    r = subprocess.run(orden, capture_output=True, text=True)
    reg = r.stdout + r.stderr
    with open(os.path.join(salida, nombre + ".log"), "w") as f:
        f.write(reg)
    if not os.path.exists(os.path.join(salida, nombre + "-trama-vulkan.png")):
        sys.exit(f"el visor aparte no dejo {nombre}-trama-vulkan.png (ver {nombre}.log)")
    return reg


def rotulo(img, texto, tam=26):
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(FUENTE, tam)
    d.rectangle((0, 0, img.width, tam + 14), fill=(24, 24, 28))
    d.text((10, 7), texto, font=f, fill=(225, 225, 230))
    return img


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("volcado")
    p.add_argument("--hold", type=int, default=-1, help="que cuadro del volcado (de fabrica, el del medio)")
    p.add_argument("--salida", default=None)
    p.add_argument("--relieve", type=float, default=50.0)
    p.add_argument("--gpu", default=os.environ.get("ODYSSEYVK_GPU", "1"))
    a = p.parse_args()
    salida = os.path.abspath(a.salida or os.path.join(a.volcado, "odysseyvk-comparacion"))
    os.makedirs(salida, exist_ok=True)

    man, cuadro, col, dep = leer_volcado(a.volcado, a.hold)
    w, h = dep.shape[1], dep.shape[0]
    color = tonificar(col, cuadro.get("exposure"))
    m, invertida, motivo, cielo = cercania(dep)
    color16, caja = a16x9(color, 0)
    Image.fromarray(color16).resize((W4K, H4K), Image.LANCZOS).convert("RGBA").save(os.path.join(salida, "cuadro4k.png"))

    # un HOME aparte: la configuracion de las pruebas y las redes enlazadas para leer
    casa = os.path.join(salida, "home")
    os.makedirs(os.path.join(casa, ".config", "odysseyvk"), exist_ok=True)
    with open(os.path.join(casa, ".config", "odysseyvk", "serve.conf"), "w") as f:
        f.write("profundidad_conv = auto\nsalida_conv = 50\nsaliencia = si\nexp_sin_red = no\n")
    redes = os.path.expanduser("~/.local/share/odysseyvk/modelos")
    enlace = os.path.join(casa, ".local", "share", "odysseyvk", "modelos")
    os.makedirs(os.path.dirname(enlace), exist_ok=True)
    if not os.path.lexists(enlace) and os.path.isdir(redes):
        os.symlink(redes, enlace)
    cuadro4k = os.path.join(salida, "cuadro4k.png")

    print(f"[comparar] {man.get('game', '?')} · render {w}x{h} · {man.get('depth_format')} · "
          f"Z {'invertida' if invertida else 'normal'} ({motivo}) · cielo {100 * cielo:.1f} %")
    # 1) la red en este cuadro: su mapa, su plano y su vista de profundidad
    reg = visor(casa, salida, "red", [f"ODYSSEYVK_FUENTE_PNG={cuadro4k}", "ODYSSEYVK_VER_PROFUNDIDAD=1"],
                ["--conver"], a.gpu)
    planos = re.findall(r"apoyo (\d+)% del rango", reg)
    plano_red = int(planos[-1]) / 100.0 if planos else 0.5
    mapa_red = leer_pgm(os.path.join(salida, "red-profundidad.pgm"))

    # 2) el plano del juego en la misma cosa que eligio la red
    m16, (bx, by, bw, bh) = a16x9(m, np.nan)
    red_a = np.asarray(Image.fromarray((mapa_red * 255).astype(np.uint8)).resize(
        (m16.shape[1], m16.shape[0]), Image.BILINEAR)).astype(np.float32) / 255.0
    util = ~np.isnan(m16)
    zona = util & (np.abs(red_a - plano_red) < 0.03) & (np.nan_to_num(m16) > 0.0)
    plano_juego = float(np.median(m16[zona])) if np.count_nonzero(zona) > 50 else float(np.nanmedian(m16))
    m16 = np.where(util, m16, plano_juego)      # las bandas, en el plano de la pantalla
    escribir_pgm(os.path.join(salida, "juego.pgm"), m16)

    # 3) los dos mapas por el mismo camino
    comun = [f"ODYSSEYVK_FUENTE_PNG={cuadro4k}", f"ODYSSEYVK_GUIA_PNG={cuadro4k}"]
    fijo = lambda mapa, plano: ["--mapa-fijo", mapa, "--apoyo", f"{plano:.4f}", "--relieve", f"{a.relieve:g}"]
    visor(casa, salida, "juego-vista", comun + ["ODYSSEYVK_VER_PROFUNDIDAD=1"],
          fijo(os.path.join(salida, "juego.pgm"), plano_juego), a.gpu)
    visor(casa, salida, "juego-ojo", comun + ["ODYSSEYVK_PRUEBA=16"],
          fijo(os.path.join(salida, "juego.pgm"), plano_juego), a.gpu)
    visor(casa, salida, "red-ojo", comun + ["ODYSSEYVK_PRUEBA=16"],
          fijo(os.path.join(salida, "red-profundidad.pgm"), plano_red), a.gpu)

    # 4) numeros: cuanto se parecen los dos mapas (por rangos, que la escala de cada uno es suya)
    pj = np.asarray(Image.fromarray((np.nan_to_num(m16) * 255).astype(np.uint8)).resize((320, 180), Image.BILINEAR)).astype(np.float64)
    pr = np.asarray(Image.fromarray((mapa_red * 255).astype(np.uint8)).resize((320, 180), Image.BILINEAR)).astype(np.float64)
    rj, rr = pj.ravel().argsort().argsort(), pr.ravel().argsort().argsort()
    spearman = float(np.corrcoef(rj, rr)[0, 1])

    # 5) la hoja: mapas, vistas de profundidad y el canto mas marcado del juego en los dos ojos
    T = (640, 360)
    gris = lambda arr: Image.fromarray((np.clip(np.nan_to_num(arr), 0, 1) * 255).astype(np.uint8)).convert("RGB").resize(T, Image.BILINEAR)
    fila1 = [rotulo(Image.fromarray(color16).convert("RGB").resize(T, Image.LANCZOS), "cuadro (volcado de Neural Forge)", 18),
             rotulo(gris(m16), "profundidad del juego (la de DLSS)", 18),
             rotulo(gris(mapa_red), "profundidad de la red (Depth Anything)", 18)]
    vj = Image.open(os.path.join(salida, "juego-vista-trama-vulkan.png")).convert("RGB")
    vr = Image.open(os.path.join(salida, "red-trama-vulkan.png")).convert("RGB")
    V = (960, 540)
    fila2 = [rotulo(vj.resize(V, Image.LANCZOS), "vista de profundidad · juego (cian = plano de la pantalla)", 20),
             rotulo(vr.resize(V, Image.LANCZOS), "vista de profundidad · red (naranja = saliencia)", 20)]
    # el canto mas marcado DENTRO de la imagen (sin las bandas ni su borde) y lejos de las orillas
    gy, gx = np.gradient(np.nan_to_num(m16[by:by + bh, bx:bx + bw]))
    g = np.zeros_like(m16, dtype=np.float32)
    mx, my = bw // 8, bh // 8
    g[by + my:by + bh - my, bx + mx:bx + bw - mx] = np.hypot(gx, gy)[my:bh - my, mx:bw - mx]
    cy, cx = np.unravel_index(int(np.argmax(g)), g.shape)
    cx4, cy4 = int(cx * W4K / g.shape[1]), int(cy * H4K / g.shape[0])
    rw, rh = 640, 360
    x0, y0 = min(max(cx4 - rw // 2, 0), W4K - rw), min(max(cy4 - rh // 2, 0), H4K - rh)
    oj = Image.open(os.path.join(salida, "juego-ojo-trama-vulkan.png")).convert("RGB").crop((x0, y0, x0 + rw, y0 + rh))
    orr = Image.open(os.path.join(salida, "red-ojo-trama-vulkan.png")).convert("RGB").crop((x0, y0, x0 + rw, y0 + rh))
    fila3 = [rotulo(oj.resize(V, Image.NEAREST), f"ojo izquierdo · juego (recorte x{x0} y{y0}, ampliado 1,5)", 20),
             rotulo(orr.resize(V, Image.NEAREST), "ojo izquierdo · red (el mismo recorte)", 20)]
    hoja = Image.new("RGB", (1920, 360 + 540 + 540), (24, 24, 28))
    for i, im in enumerate(fila1):
        hoja.paste(im, (i * 640, 0))
    for i, im in enumerate(fila2):
        hoja.paste(im, (i * 960, 360))
    for i, im in enumerate(fila3):
        hoja.paste(im, (i * 960, 900))
    hoja.save(os.path.join(salida, "comparacion.png"), optimize=True)

    resumen = (f"juego: {man.get('game', '?')} · cuadro {cuadro.get('index')} del volcado {os.path.basename(os.path.normpath(a.volcado))}\n"
               f"render {w}x{h} · profundidad {man.get('depth_format')} · Z {'invertida' if invertida else 'normal'} ({motivo})\n"
               f"cielo (a lo lejano): {100 * cielo:.1f} % · exposicion {cuadro.get('exposure')} · jitter {cuadro.get('jitter')}\n"
               f"plano de la pantalla: red {plano_red:.2f} · juego {plano_juego:.3f} (la misma cosa)\n"
               f"parecido de los mapas (Spearman por rangos, 320x180): {spearman:.3f}\n"
               f"canto mas marcado del juego: x {cx4}, y {cy4} (de 3840x2160)\n")
    with open(os.path.join(salida, "resumen.txt"), "w") as f:
        f.write(resumen)
    print(resumen + f"[comparar] {os.path.join(salida, 'comparacion.png')}")
    shutil.rmtree(os.path.join(salida, "estado"), ignore_errors=True)


if __name__ == "__main__":
    main()
