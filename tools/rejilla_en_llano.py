#!/usr/bin/env python3
"""Mide cuanto enrejado de color pinta cada lado en zonas REALMENTE LLANAS.

En una zona llana el ojo izquierdo y el derecho ven lo mismo, asi que una trama
que solo transporta la diferencia entre ojos no deberia pintar nada. Lo que
queda ahi (reencaje verde-rojo, grano cuadro a cuadro, periodos llamados) es
rejilla escrita de mas: se ve en la pantalla y bate con la lente.

Uso: python3 tools/rejilla_en_llano.py FOTO.jpg [FOTO.jpg ...]
"""
import sys
import numpy as np
from PIL import Image

K = 160        # lado de la baldosa
PASI = 40      # paso entre baldosas
LIMITE_STD = 8.0   # solo baldosas de color de verdad parejo

def planas(lum):
    H, W = lum.shape
    out = []
    for y in range(0, H - K, PASI):
        for x in range(0, W - K, PASI):
            rec = lum[y:y + K, x:x + K]
            m = rec.mean()
            if 120 < m < 252 and rec.std() < LIMITE_STD:
                out.append((x, y))
    return out

def escala(im):
    H, W = im.shape[:2]
    lum = 0.2126 * im[..., 0] + 0.7152 * im[..., 1] + 0.0722 * im[..., 2]
    ok = np.where((lum > 130).mean(axis=0) > 0.30)[0]
    return (3840.0 / (ok.max() - ok.min())) if len(ok) > 200 else None, len(ok)

def una(ruta):
    im = np.asarray(Image.open(ruta).convert("RGB"), np.float64)
    lum = 0.2126 * im[..., 0] + 0.7152 * im[..., 1] + 0.0722 * im[..., 2]
    esc, _ = escala(im)
    ts = planas(lum)
    n = min(len(ts), 200)
    print("== %s" % ruta.split("/")[-1])
    if not ts:
        print("   sin baldosas llanas (umbral std < %.0f)" % LIMITE_STD)
        return
    print("   %d baldosas llanas de %d x %d (primeras %d usadas)%s" %
          (len(ts), K, K, n, "   escala 1 px foto = %.3f px panel" % esc if esc else ""))
    gr = np.zeros((K, K)); rb = np.zeros((K, K)); grano = 0.0; brillo = np.zeros((K, K))
    acc = np.zeros((K, K)); h = np.hanning(K)
    for (x, y) in ts[:n]:
        c = im[y:y + K, x:x + K]
        g_r = c[..., 1] - c[..., 0]
        b_g = c[..., 2] - c[..., 1]
        gr += g_r; rb += b_g
        l = 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]
        brillo += l
        grano += np.abs(np.diff(l, axis=1)).mean()
        rec = l - l.mean()
        acc += np.abs(np.fft.fftshift(np.fft.fft2(rec * h[:, None] * h[None, :])))
    gr /= n; rb /= n; brillo /= n; grano /= n; acc /= n
    print("   reencaje de color dentro de lo llano:  verde-rojo %+6.2f niveles (desv %5.2f)   azul-verde %+6.2f (%5.2f)" %
          (gr.mean(), gr.std(), rb.mean(), rb.std()))
    print("   grano cuadro a cuadro (salto horizontal medio): %.2f niveles" % grano)
    e = np.arange(-K // 2, K // 2) / K
    EX, EY = np.meshgrid(e, e)
    R = np.hypot(EX, EY)
    for r0, r1, et in ((0.007, 0.05, "ancho (20-140 px foto)"),
                       (0.05, 0.16, "medio (6-20 px foto)"),
                       (0.16, 0.71, "fino (1,4-6 px foto)")):
        b = (R >= r0) & (R < r1)
        piso = np.median(acc[b])
        tmp = np.where(b, acc, 0.0)
        iy, ix = np.unravel_index(np.argmax(tmp), tmp.shape)
        per = 1.0 / max(np.hypot(e[ix], e[iy]), 1e-9)
        txt = "pico/piso %7.1f  periodo %6.1f px foto" % (acc[iy, ix] / piso, per)
        if esc:
            txt += " = %6.1f px panel = %5.2f mm" % (per * esc, per * esc * 0.1554)
        print("   %-24s %s" % (et, txt))

if __name__ == "__main__":
    for r in sys.argv[1:]:
        una(r)
