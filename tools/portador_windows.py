#!/usr/bin/env python3
"""portador_windows.py — el estimador de Windows (rueda_blanco.py) corrido aqui, sobre nuestros PNG.

Sirve para comparar manzanas con manzanas: Windows midi6 su tramado con una banda en
(kx, ky) = (0,4501, -0,1607) ciclos/pixel, radio 0,045, piso anular 0,14-0,26, bloques de
512x512 con ventana de Hanning. Este script replica eso exacto y le suma los controles que
piden las reglas de la ronda:

  * control angular  : la misma banda girada +-10 grados
  * control de periodo : la misma banda movida 0,05 ciclos/px en x y en y
  * canal : luminancia, G-R y B-G (asf lo midi6 Windows)
  * pico libre : d6nde cae el pico de verdad, con su incertidumbre (media y desvio de los bloques)

Uso:
    python3 tools/portador_windows.py PNG [PNG...]
"""
import sys

import numpy as np
from PIL import Image

# ---- lo que Windows declar6 (ronda 2/3) ----
KX, KY = 0.4501, -0.1607
L = 512
BLOQUES = [(960, 540), (1900, 540), (960, 1400), (1900, 1400)]
PISO = (0.14, 0.26)
RADIO = 0.045


def bloques_de(img):
    """Corta los mismos cuatro bloques de 512 que corta el script de Windows."""
    for x0, y0 in BLOQUES:
        if x0 + L > img.shape[1] or y0 + L > img.shape[0]:
            continue
        yield img[y0:y0 + L, x0:x0 + L]


def espectro(rec):
    w = np.hanning(L)
    return np.abs(np.fft.fftshift(np.fft.fft2(rec * w[:, None] * w[None, :])))


def ejes():
    e = np.arange(-L // 2, L // 2, dtype=np.float64) / L
    return e[None, :], e[:, None]


def en_banda(v, fx0, fy0, radio=RADIO):
    fx, fy = ejes()
    r = np.hypot(fx - fx0, fy - fy0)
    banda = v[r < radio]
    piso = v[(r > PISO[0]) & (r < PISO[1])]
    if banda.size == 0 or piso.size == 0:
        return None, None
    return float(banda.max()), float(np.median(piso))


def fuerza(v, fx0, fy0, radio=RADIO):
    a, p = en_banda(v, fx0, fy0, radio)
    if a is None or p is None or p <= 0:
      return None, None
    return a / p, a


def pico_libre(v):
    """Pico mas fuerte en la zona de interes (|fx| entre 0,30 y 0,60), con centro de masa."""
    fx, fy = ejes()
    m = (np.abs(fx) > 0.30) & (np.abs(fx) < 0.62) & (np.abs(fy) > 0.05) & (np.abs(fy) < 0.30)
    if not m.any():
        return None
    idx = np.argmax(np.where(m, v, -1.0))
    iy, ix = np.unravel_index(idx, v.shape)
    # centro de masa en una cruz de +-3 celdas, solo para refinar
    def refine(vals, pos):
        a, b = max(0, pos - 3), min(len(vals), pos + 4)
        seg = vals[a:b]
        if seg.sum() <= 0:
            return pos
        return float(np.average(np.arange(a, b), weights=seg))
    e = np.arange(-L // 2, L // 2, dtype=np.float64) / L
    cx = e[0] + refine(v[iy, :], ix) * (e[1] - e[0])
    cy = e[0] + refine(v[:, ix], iy) * (e[1] - e[0])
    return cx, cy


def canales(rgb):
    """Luminancia, G-R y B-G, como los midi6 Windows."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    return {
        "luminancia": 0.2126 * r + 0.7152 * g + 0.0722 * b,
        "G-R": g - r,
        "B-G": b - g,
    }


def angulo_deg(fx, fy):
    return float(np.degrees(np.arctan2(fy, fx)))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    for ruta in sys.argv[1:]:
        im = np.asarray(Image.open(ruta).convert("RGB"), dtype=np.float64)
        print("\n=== %s (%dx%d) ===" % (ruta, im.shape[1], im.shape[0]))
        for nombre, plan in canales(im).items():
            vs = [espectro(rec) for rec in bloques_de(plan)]
            if not vs:
                print("  %s: los bloques de Windows no caben en el PNG" % nombre)
                continue

            def prom(fn):
                val = [fn(v)[0] for v in vs]
                val = [x for x in val if x is not None]
                if not val:
                    return None, None
                return float(np.mean(val)), float(np.std(val))

            f_win, f_std = prom(lambda v: fuerza(v, KX, KY))
            amps = [en_banda(v, KX, KY)[0] for v in vs]
            amps = [a for a in amps if a is not None]
            a_win = float(np.mean(amps)) if amps else None
            f_ang1, _ = prom(lambda v: fuerza(v, KX * np.cos(np.radians(10)) - KY * np.sin(np.radians(10)),
                                              KX * np.sin(np.radians(10)) + KY * np.cos(np.radians(10))))
            f_ang2, _ = prom(lambda v: fuerza(v, KX * np.cos(np.radians(-10)) - KY * np.sin(np.radians(-10)),
                                              KX * np.sin(np.radians(-10)) + KY * np.cos(np.radians(-10))))
            f_dx, _ = prom(lambda v: fuerza(v, KX + 0.05, KY))
            f_dy, _ = prom(lambda v: fuerza(v, KX, KY + 0.05))
            f_esp, _ = prom(lambda v: fuerza(v, KX, KY, 0.010))
            kss, kys = [], []
            for v in vs:
                p = pico_libre(v)
                if p:
                    kss.append(abs(p[0]))
                    kys.append(p[1])
            txt = "  %-11s fuerza(banda Windows) = %6.1f +- %-5.2f amplitud = %6.2f niveles"
            if f_win is None:
                print("  %s: sin banda" % nombre)
                continue
            print(txt % (nombre, f_win, f_std, a_win if a_win is not None else float("nan")))
            print("              controles: angular +-10g = %5.1f / %5.1f · movida x+0,05 = %5.1f · movida y+0,05 = %5.1f"
                  % (f_ang1 or 0.0, f_ang2 or 0.0, f_dx or 0.0, f_dy or 0.0))
            print("              banda angosta (radio 0,010) = %6.1f" % (f_esp or 0.0))
            if kss:
                print("              pico libre kx = %.4f +- %.4f  ky = %+.4f +- %.4f  angulo = %+.1f grad"
                      % (np.mean(kss), np.std(kss), np.mean(kys), np.std(kys),
                         np.mean([angulo_deg(kx, ky) if kx > 0 else angulo_deg(-kx, -ky) for kx, ky in zip(kss, kys)])))
                print("              periodo horizontal = %.4f px de panel = %.5f mm"
                      % (1.0 / np.mean(kss), 0.1554 / np.mean(kss)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
