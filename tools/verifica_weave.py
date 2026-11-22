#!/usr/bin/env python3
"""verifica_weave.py — comprueba el entrelazado del C++ contra el oráculo Python.

Renderiza `odyssey3d weave --solid` (fuente plana: mitad izquierda roja, derecha verde)
y reproduce en numpy el mismo cálculo (atributos de esquina del oráculo paramétrico,
interpolación bilineal como la GPU, mapas de corrección muestreados como GL lineal,
filtro triangular y crosstalk). Deben coincidir a pocos LSB.
"""
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "..", "Linux_3D_SBS_Player", "hub"))
from linux_oracle import LinuxOracle, OPTPOS  # noqa: E402

W, H = 3840, 2160
PNG = os.path.join(ROOT, "samples", "trama_solid.png")

FS, XTF, XTD, CONTRAST, CAS, CONV = 10.0, 0.0128535, 0.0, 1.0, 1.0, 0.0
ALIGN, PITCH = 0.0, 1.0


def png_to_array(path):
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", path, "-f", "rawvideo", "-pix_fmt", "rgba", "-"],
        capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.uint8).reshape(H, W, 4).astype(np.float64) / 255.0


def sample_gray(path, u, v):
    """Muestrea un PNG en escala de grises como GL_TEXTURE_2D lineal + CLAMP_TO_EDGE."""
    import PIL.Image as I
    im = np.asarray(I.open(path).convert("L"), np.float64) / 255.0
    th, tw = im.shape
    x = u * tw - 0.5
    y = v * th - 0.5
    x = np.clip(x, 0.0, tw - 1.0)
    y = np.clip(y, 0.0, th - 1.0)
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    x1 = np.minimum(x0 + 1, tw - 1)
    y1 = np.minimum(y0 + 1, th - 1)
    fx, fy = x - x0, y - y0
    return (im[y0, x0] * (1 - fx) * (1 - fy) + im[y0, x1] * fx * (1 - fy)
            + im[y1, x0] * (1 - fx) * fy + im[y1, x1] * fx * fy)


def main():
    use_corr = "--corr" in sys.argv
    args = [os.path.join(ROOT, "build", "odyssey3d"), "weave", "--solid"]
    if not use_corr:
        args.append("--no-corr")
    subprocess.run(args + ["--out", PNG], check=True, cwd=ROOT)
    img = png_to_array(PNG)

    o = LinuxOracle(W, H, parametric=True)
    o.set_eye(*OPTPOS)
    fa = [o.fill(nx, ny) for (nx, ny) in [(-1, -1), (1, -1), (-1, 1), (1, 1)]]
    mn = min(min(f[2]) for f in fa)
    v4min = int(np.floor(mn))

    # recorte a comprobar
    y0s, y1s, x0s, x1s = 1000, 1016, 1200, 1560
    ys = np.arange(y0s, y1s)
    xs = np.arange(x0s, x1s)
    X, Y = np.meshgrid(xs, ys)
    u = (X + 0.5) / W
    v = (Y + 0.5) / H                     # v = 0 arriba (igual que el VBO del C++)

    def bil(get):
        a, b, c, d = (get(f) for f in fa)
        return a * (1 - u) * (1 - v) + b * u * (1 - v) + c * (1 - u) * v + d * u * v

    fx = bil(lambda f: f[0][0]).astype(np.float32).astype(np.float64)
    fy = bil(lambda f: f[0][1]).astype(np.float32).astype(np.float64)
    v3x = bil(lambda f: f[1][0]).astype(np.float32).astype(np.float64)
    v3y = bil(lambda f: f[1][1]).astype(np.float32).astype(np.float64)
    cch = [bil(lambda f, ch=ch: f[2][ch] - v4min).astype(np.float32).astype(np.float64)
           for ch in range(3)]

    suv_x = (X + 0.5) / W
    suv_y = 1.0 - (Y + 0.5) / H
    calib = os.path.join(ROOT, "calib")
    if use_corr:
        corrA = sample_gray(os.path.join(calib, "3DStackCorrection_A.png"), suv_x, suv_y) * CAS
        corrB = sample_gray(os.path.join(calib, "3DStackCorrection_B.png"), suv_x, suv_y)
    else:
        corrA = np.zeros_like(u)
        corrB = np.full_like(u, 128.0 / 255.0)

    rsq = 1.0 / np.sqrt(1.0 + v3x * v3x + v3y * v3y)
    base = fx * rsq - 2.0 * fy * (corrB - 0.5) + corrA

    # fuente plana: con la convención validada, la vista IZQUIERDA sale de la mitad DERECHA
    # del archivo (verde) y la DERECHA de la mitad IZQUIERDA (rojo).
    L = np.array([0.0, 1.0, 0.0])
    R = np.array([1.0, 0.0, 0.0])
    L = (L - 0.5) * CONTRAST + 0.5
    R = (R - 0.5) * CONTRAST + 0.5
    dRL = R - L
    xt = ((1.0 + v3x * v3x + v3y * v3y) * fy * fy) * XTD + XTF
    xt = xt / (1.0 - xt)
    Lp = np.stack([L[c] - xt * dRL[c] for c in range(3)], axis=-1)
    Rp = np.stack([R[c] + xt * dRL[c] for c in range(3)], axis=-1)

    pred = np.empty((*xt.shape, 3))
    for ch in range(3):
        phase = (base + cch[ch] * PITCH + ALIGN - 0.25) % 1.0
        t = 2.0 * phase - 1.0
        w = np.clip(-np.abs(t) * FS + FS * 0.5 + 0.5, 0.0, 1.0)
        pred[..., ch] = Rp[..., ch] * (1 - w) + Lp[..., ch] * w

    got = img[y0s:y1s, x0s:x1s, :3]
    d = np.abs(np.clip(pred, 0.0, 1.0) - got)
    print(f"recorte {got.shape[1]}x{got.shape[0]} px   mayor desviación = {d.max()*255:.2f} LSB   "
          f"media = {d.mean()*255:.3f} LSB")
    per = [float(d[..., c].max() * 255) for c in range(3)]
    print(f"por canal (LSB): R {per[0]:.2f}  G {per[1]:.2f}  B {per[2]:.2f}")
    tol = 30.0 if use_corr else 2.0
    print(("OK" if d.max() * 255 < tol else "DIFERENCIA")
          + f"   (tolerancia {tol:.0f} LSB"
          + (": el filtrado lineal de texturas de 8 bits se hace en punto fijo)" if use_corr else
             ": con correcciones neutras solo queda el redondeo float32)"))


if __name__ == "__main__":
    main()
