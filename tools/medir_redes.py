# medir_redes.py — compara los mapas de varias redes de profundidad (v0.63).
# OJO: los cuadros sueltos van con `mapa --sueltas` y cada rafaga en SU llamada; si no, el suavizado en
# el tiempo mezcla cuadros sin relacion y las medidas salen contaminadas (paso en v0.63, corregido en v0.66).
# Uso: correr `odysseyvk mapa CUADROS... --sueltas --modelo RED.onnx` para cada red dentro de
#   CARPETA/res/<red>/cuadros y CARPETA/res/<red>/rafagas (los -mapa.pgm que deja), con los cuadros
#   originales en CARPETA/cuadros, y despues:  python3 tools/medir_redes.py CARPETA
# Medidas: parecido con mde_large (la red de Windows), bordes del mapa sobre bordes de la imagen,
# dureza de canto (p99 del gradiente), separacion en 3 planos (Otsu de 3 clases, eta 0..1) y
# parpadeo entre cuadros seguidos. Resultado del 25-nov en CAMBIOS v0.63.
"""Compara mapas de profundidad de varias redes (salida de `odysseyvk mapa`).
Medidas por cuadro, promediadas: parecido con mde_large (Pearson y rangos), bordes en su sitio,
dureza de canto (p99 del gradiente), separacion en 3 planos (Otsu de 3 clases, eta) y parpadeo
entre cuadros seguidos (rafagas)."""
import sys, glob, os
import numpy as np
from PIL import Image

S = sys.argv[1]
redes = ["mde_large", "mde_large_fp32", "da2_small", "mde_small"]


def carga(p):
    return np.asarray(Image.open(p)).astype(np.float64) / 255.0


def rangos(a):
    r = np.empty(a.size); r[np.argsort(a.ravel())] = np.arange(a.size); return r


def grad(a):
    gx = np.zeros_like(a); gy = np.zeros_like(a)
    gx[:, 1:-1] = (a[:, 2:] - a[:, :-2]) / 2; gy[1:-1] = (a[2:] - a[:-2]) / 2
    return np.hypot(gx, gy)


def eta3(d):
    """Otsu de 3 clases sobre 64 cubetas: varianza entre clases / varianza total."""
    h, b = np.histogram(d, 64, (0, 1)); p = h / h.sum(); c = (b[:-1] + b[1:]) / 2
    mu = (p * c).sum(); vt = (p * (c - mu) ** 2).sum()
    if vt <= 0: return 0.0
    P = np.cumsum(p); M = np.cumsum(p * c); best = 0.0
    for i in range(1, 63):
        for j in range(i + 1, 64):
            w = [P[i - 1], P[j - 1] - P[i - 1], 1 - P[j - 1]]
            m = [M[i - 1], M[j - 1] - M[i - 1], M[-1] - M[j - 1]]
            vb = sum(wk * (mk / wk - mu) ** 2 for wk, mk in zip(w, m) if wk > 1e-9)
            best = max(best, vb)
    return best / vt


def bordes(d, img):
    """De los puntos con el 5 % de salto de profundidad mas fuerte, cuantos caen sobre el 20 % de
    bordes mas fuertes de la imagen (al azar seria 0,20)."""
    gd = grad(d); gi = grad(img)
    md = gd >= np.quantile(gd, 0.95); mi = gi >= np.quantile(gi, 0.80)
    return (md & mi).sum() / max(md.sum(), 1)


fotos = sorted(glob.glob(f"{S}/cuadros/*.png"))
res = {n: {"pear": [], "spear": [], "bord": [], "p99": [], "eta": []} for n in redes}
for f in fotos:
    base = os.path.basename(f)[:-4]
    img = np.asarray(Image.open(f).convert("L").resize((518, 518), Image.BILINEAR)).astype(np.float64) / 255
    ref = carga(f"{S}/res/mde_large/cuadros/{base}-mapa.pgm")
    for n in redes:
        d = carga(f"{S}/res/{n}/cuadros/{base}-mapa.pgm")
        res[n]["pear"].append(np.corrcoef(d.ravel(), ref.ravel())[0, 1])
        res[n]["spear"].append(np.corrcoef(rangos(d), rangos(ref))[0, 1])
        res[n]["bord"].append(bordes(d, img))
        res[n]["p99"].append(np.quantile(grad(d), 0.99))
        res[n]["eta"].append(eta3(d))
parp = {}
for n in redes:
    vals = []
    for clip in ("clip", "voy", "core"):
        ms = sorted(glob.glob(f"{S}/res/{n}/rafagas/{clip}-*-mapa.pgm"))
        for a, b in zip(ms, ms[1:]):
            vals.append(np.abs(carga(a) - carga(b)).mean())
    parp[n] = (np.mean(vals), np.max(vals))
print(f"{len(fotos)} cuadros sueltos, 3 rafagas de 10\n")
print(f"{'red':16s} {'parecido mde_large':>19s} {'bordes en sitio':>16s} {'canto p99':>10s} {'3 planos':>9s} {'parpadeo':>16s}")
print(f"{'':16s} {'Pearson / rangos':>19s} {'(azar 0,20)':>16s} {'':>10s} {'eta 0..1':>9s} {'media / max':>16s}")
for n in redes:
    r = res[n]
    print(f"{n:16s} {np.mean(r['pear']):9.3f} / {np.mean(r['spear']):.3f} {np.mean(r['bord']):16.3f} "
          f"{np.mean(r['p99']):10.4f} {np.mean(r['eta']):9.3f} {parp[n][0]:9.4f} / {parp[n][1]:.4f}")
# por tipo de cuadro, la separacion en planos y los bordes
print("\npor tipo de cuadro (3 planos · bordes):")
for grupo in ("yt-", "voy-", "core-", "clip-"):
    idx = [i for i, f in enumerate(fotos) if os.path.basename(f).startswith(grupo)]
    print(f"  {grupo:6s}", "  ".join(f"{n} {np.mean([res[n]['eta'][i] for i in idx]):.3f}·{np.mean([res[n]['bord'][i] for i in idx]):.3f}" for n in redes))
