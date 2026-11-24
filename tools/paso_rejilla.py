#!/usr/bin/python3
"""Mide el PASO de la rejilla que lleva escrita un cuadro (volcado PNG 3840x2160 nuestra
o foto del panel de Windows) y cuanto desentonamos del paso de la lente.

Por que una FFT directa y no el estimador de bloques de tools/portador_windows.py: el
estimador encaja una recta por bloque y, cuando la rampa de fase esta ligeramente movida,
salta a otro pico y reporta inclinaciones falsas (dijo -9,7 grados con paso 0,995 y con
rampa -0,00277, cuando la FFT muestra -19,4 y -19,6). La FFT con ventana de Hanning y
refino por centroide sobre un recorte grande no se confunde.

Lo que importa no es el periodo en si, es la DIFFERENCIA con el de la lente: de ahi sale
la distancia a la que nuestra rejilla y la del panel vuelven a coincidir (el batido). Una
desviacion del 0,6% da un batido de unos 350 px = 5,5 cm: esas bandas enormes.

Uso:
  paso_rejilla.py CUADRO.png [CUADRO2.png ...]
  paso_rejilla.py --objetivo 0.4501 CUADRO.png     (para cambiar el blanco)

Blanco por defecto: kx = 0,4501 (periodo 2,2217 px = 0,34530 mm), el valor que mide
tools/portador_windows.py sobre las fotos nativas del conversor de Windows.
"""
import sys
import numpy as np
from PIL import Image

PANEL_MM = 0.1554          # paso de pixel del G90XF a 3840x2160
KH, KV = 0.4501, -0.1607   # porteadora que escribe el conversor de Windows (medida en foto)
RECORTE = 512
BLOQUES = ((1900, 1000), (2600, 1000), (2600, 1500))   # dentro de la mitad derecha (verde)


def espectro(im, canal, recorte=RECORTE, bloques=BLOQUES):
    """Espectro de modulos acumulado de varios recortes, con ventana de Hanning."""
    tot = None
    for x0, y0 in bloques:
        a = im[y0:y0 + recorte, x0:x0 + recorte, canal].astype(np.float64)
        a = a - a.mean()
        w = np.hanning(recorte)
        f = np.fft.fftshift(np.abs(np.fft.fft2(a * w[:, None] * w[None, :])))
        tot = f if tot is None else tot + f
    fr = np.fft.fftshift(np.fft.fftfreq(recorte))
    return tot, fr


def recorte_de(tot):
    return tot.shape[0]


def pico(tot, fr, kh, kv, buscar=0.025, vecindad=3):
    """Pico mas cercano a (kh, kv), refinado por centroide de energia."""
    kx, ky = np.meshgrid(fr, fr)
    m = (np.abs(kx - kh) < buscar) & (np.abs(ky - kv) < buscar)
    f = np.where(m, tot, 0.0)
    i, j = np.unravel_index(np.argmax(f), f.shape)
    s = (slice(max(0, i - vecindad), i + vecindad + 1), slice(max(0, j - vecindad), j + vecindad + 1))
    sub = tot[s]
    p = sub / sub.sum()
    cx, cy = float((kx[s] * p).sum()), float((ky[s] * p).sum())
    banda = (np.hypot(kx, ky) > 0.30) & (np.hypot(kx, ky) < 0.62)
    piso = float(np.median(tot[banda]))
    # energia en un circulo de radio fijo alrededor del centro refinado: asi el numero no
    # depende de si el pico cae justo en un bin de la FFT o entre dos (con la caja fija de
    # bins el paso movido parecia perder un 25% de fuerza, y era solo fugas de la ventana)
    radio = 4.0 / recorte_de(tot)
    d = np.hypot(kx - cx, ky - cy)
    dentro = int((d <= radio).sum())
    fuera = int(((d > 2.0 * radio) & (d <= 4.0 * radio)).sum())
    media_pico = float(tot[d <= radio].sum()) / max(1, dentro)
    media_fondo = float(tot[(d > 2.0 * radio) & (d <= 4.0 * radio)].sum()) / max(1, fuera)
    return cx, cy, media_pico / max(media_fondo, piso), piso


def mide(ruta, kh=KH, kv=KV):
    im = np.asarray(Image.open(ruta).convert("RGB"), np.float64)
    h, w = im.shape[:2]
    fila = {"ruta": ruta, "ancho": w, "alto": h}
    escalas = []
    for nombre, canal in (("rojo", 0), ("verde", 1)):
        tot, fr = espectro(im, canal)
        cx, cy, fuerza, piso = pico(tot, fr, kh, kv)
        if abs(cx) < 0.15:                       # canal plano: no hay porteadora en ese canal
            fila[nombre] = None
            continue
        fila[nombre] = (cx, cy, fuerza)
        escalas.append(abs(cx))
    if escalas:
        fila["escala_px_panel_por_px_imagen"] = None   # la pone el de fotos, ver nota abajo
    return fila


def informa(f, kh, kv, etiqueta=""):
    print(f"== {f['ruta']}  {f['ancho']}x{f['alto']} {etiqueta}")
    for nombre in ("rojo", "verde"):
        r = f.get(nombre)
        if r is None:
            print(f"   {nombre}: sin porteadora (canal plano en este cuadro)")
            continue
        cx, cy, fuerza = r
        per = 1.0 / abs(cx)
        ang = np.degrees(np.arctan2(cy, cx))
        dk = kh - cx
        batido = 1.0 / abs(dk) if abs(dk) > 1e-6 else float("inf")
        print(f"   {nombre}: kx {cx:+.4f}  ky {cy:+.4f}  angulo {ang:+.1f} grad")
        print(f"      periodo horizontal {per:.4f} px = {per * PANEL_MM:.5f} mm   fuerza/piso {fuerza:.0f}")
        print(f"      vs blanco {kh:.4f}: faltan {dk:+.4f} -> batido cada {batido:.0f} px = "
              f"{batido * PANEL_MM:.1f} mm")
        print(f"      rampa_x que lo cuadra: {-dk:+.5f} ciclos por pixel de panel")


def main(argv):
    kh, kv = KH, KV
    rutas = []
    i = 0
    while i < len(argv):
        if argv[i] == "--objetivo":
            kh = float(argv[i + 1]); i += 2
        elif argv[i] == "--inclinacion":
            kv = float(argv[i + 1]); i += 2
        else:
            rutas.append(argv[i]); i += 1
    if not rutas:
        print(__doc__)
        return 2
    for r in rutas:
        informa(mide(r), kh, kv)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
