# simula_jbu.py — un ojo del conversor 2D->3D en numpy, para comparar el metodo de antes (cubica +
# recorte de canto) con el agrandado guiado (jbu) de v0.62 sin abrir el visor.
# Entrada: un cuadro 3840x2160 sin tramar y su mapa 518x518 (los que deja `volcado` en serve.conf:
# /tmp/rampa-trama-cruda.png y /tmp/rampa-profundidad.pgm). Escribe jbu-comp.png (original | antes |
# guiado) y jbu-zoom.png en la carpeta que se le pase. Solo numpy y Pillow.
#   python3 tools/simula_jbu.py CARPETA_DE_SALIDA
# La prueba de verdad es el sombreador: ODYSSEYVK_FUENTE_PNG=cuadro.png ODYSSEYVK_PRUEBA=1
# ODYSSEYVK_JBU=0|1 odysseyvk test --windowed --no-panel --no-lens --no-scaler --sin-interfaz
# --mapa-fijo mapa.pgm --apoyo 0.37 --relieve 50 --dump salida
"""Simulacion de un ojo del conversor 2D->3D: metodo actual (cubica + canto) contra agrandado guiado (JBU).
Entrada: cuadro completo 3840x2160 y mapa 518x518 de la sesion. Sale un PNG con recortes lado a lado."""
import sys
import numpy as np
from PIL import Image

S = sys.argv[1]
src = np.asarray(Image.open("/tmp/rampa-trama-cruda.png").convert("RGB")).astype(np.float32) / 255.0
D = np.asarray(Image.open("/tmp/rampa-profundidad.pgm")).astype(np.float32) / 255.0
H, W = src.shape[:2]
n = D.shape[0]
gain = 0.0059                     # uv por unidad de mapa (relieve 50)
gpx = gain * W                    # px de pantalla por unidad de mapa
plano = (D.min() + 0.47 * (D.max() - D.min()))
tope_fuera, tope_dentro = 12.67, 9.98


def hoyos(D, signo):
    """rellenarVistas de hoyos.cpp para un ojo: esparcir con guarda de superficie cercana y rellenar filas."""
    val = np.zeros_like(D); ok = np.zeros(D.shape, bool)
    tope = 0.0035 * n + 1.0
    s = np.clip((D - plano) * gain * n, -tope, tope)
    for y in range(n):
        for x in range(n):
            x0 = int(np.floor(x + signo * s[y, x]))
            d = D[y, x]
            for k in (0, 1):
                xx = x0 + k
                if 0 <= xx < n and (not ok[y, xx] or d > val[y, xx]):
                    val[y, xx] = d; ok[y, xx] = True
        row, o = val[y], ok[y]
        idx = np.nonzero(o)[0]
        if len(idx):
            row[~o] = np.interp(np.nonzero(~o)[0], idx, row[idx])
    return val


def cr(t):
    t2, t3 = t * t, t * t * t
    return 0.5 * np.stack([-t3 + 2 * t2 - t, 3 * t3 - 5 * t2 + 2, -3 * t3 + 4 * t2 + t, t3 - t2])


def crd(t):
    t2 = t * t
    return 0.5 * np.stack([-3 * t2 + 4 * t - 1, 9 * t2 - 10 * t, -9 * t2 + 8 * t + 1, 3 * t2 - 2 * t])


def muestrea(img, xs, ys):
    h, w = img.shape[:2]
    xs = np.clip(xs, 0, w - 1.001); ys = np.clip(ys, 0, h - 1.001)
    x0 = np.floor(xs).astype(int); y0 = np.floor(ys).astype(int)
    fx = (xs - x0)[..., None]; fy = (ys - y0)[..., None]
    a = img[y0, x0]; b = img[y0, x0 + 1]; c = img[y0 + 1, x0]; d = img[y0 + 1, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def ojo(M, x0, x1, y0, y1, signo, metodo):
    ys, xs = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    u = (xs + 0.5) / W * n - 0.5
    v = (ys + 0.5) / H * n - 0.5
    iu, iv = np.floor(u).astype(int), np.floor(v).astype(int)
    fu, fv = u - iu, v - iv
    if metodo == "actual":
        ax, ex, ay = cr(fu), crd(fu), cr(fv)
        val = np.zeros_like(u); der = np.zeros_like(u)
        for r in range(4):
            yy = np.clip(iv + r - 1, 0, n - 1)
            for c in range(4):
                xx = np.clip(iu + c - 1, 0, n - 1)
                d = M[yy, xx]
                val += ay[r] * ax[c] * d
                der += ay[r] * ex[c] * d
        canto = np.clip(0.004 / np.maximum(np.abs(der), 1e-5), 0.10, 1.0)
        s = (val - plano) * gpx * canto
    else:
        # estimacion gruesa (cubica sin canto) para saber que color va a mostrar este punto
        ax, ay = cr(fu), cr(fv)
        val = np.zeros_like(u)
        for r in range(4):
            yy = np.clip(iv + r - 1, 0, n - 1)
            for c in range(4):
                xx = np.clip(iu + c - 1, 0, n - 1)
                val += ay[r] * ax[c] * M[yy, xx]
        s0 = np.clip((val - plano) * gpx, -tope_dentro, tope_fuera)
        guia = muestrea(src, xs - signo * s0, ys)
        ru, rv = np.rint(u).astype(int), np.rint(v).astype(int)
        num = np.zeros_like(u); den = np.zeros_like(u)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                qy = np.clip(rv + dy, 0, n - 1); qx = np.clip(ru + dx, 0, n - 1)
                Dq = M[qy, qx]
                sq = np.clip((Dq - plano) * gpx, -tope_dentro, tope_fuera)
                cx = (qx + 0.5) / n * W - 0.5; cy = (qy + 0.5) / n * H - 0.5
                Cq = muestrea(src, cx - signo * sq, cy)
                esp = np.exp(-((qx - u) ** 2 + (qy - v) ** 2) / (2 * 0.8 ** 2))
                rango = np.exp(-((guia - Cq) ** 2).sum(-1) / (2 * 0.06 ** 2))
                w = esp * (rango + 1e-4)
                num += w * Dq; den += w
        s = (num / den - plano) * gpx
    s = np.clip(s, -tope_dentro, tope_fuera)
    return muestrea(src, xs - signo * s, ys)


ML = hoyos(D, +1.0)
x0, x1, y0, y1 = 1500, 2150, 1300, 2000
a = ojo(ML, x0, x1, y0, y1, +1.0, "actual")
b = ojo(ML, x0, x1, y0, y1, +1.0, "jbu")
orig = src[y0:y1, x0:x1]
fila = np.concatenate([orig, a, b], 1)
Image.fromarray((np.clip(fila, 0, 1) * 255).astype(np.uint8)).save(f"{S}/jbu-comp.png")
# zoom 2x al borde izquierdo del microfono
zx0, zx1, zy0, zy1 = 260, 460, 0, 330
z = np.concatenate([orig[zy0:zy1, zx0:zx1], a[zy0:zy1, zx0:zx1], b[zy0:zy1, zx0:zx1]], 1)
Image.fromarray((np.clip(z, 0, 1) * 255).astype(np.uint8)).resize((z.shape[1] * 2, z.shape[0] * 2), Image.NEAREST).save(f"{S}/jbu-zoom.png")
print("plano", round(float(plano), 3), "listo")
