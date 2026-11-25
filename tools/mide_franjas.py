#!/usr/bin/env python3
"""Mide de dónde salen las franjas verticales del 3D, con los volcados del visor.

Uso:
    odysseyvk capture3d ... --dump /tmp/chk --volcado-seguir     # o `volcado = /tmp/chk` en serve.conf
    python3 tools/mide_franjas.py /tmp/chk

Mira tres cosas y las compara:
  * la trama con el relieve puesto   (`chk-trama-vulkan.png` / `chk-trama-con-mapa.png`)
  * la misma trama con el releve fuera (`chk-trama-sin-releve.png`)
  * el mapa de profundidad            (`chk-profundidad.pgm`)

Una franja que está en la trama con relieve y NO en la trama sin relieve sale del mapa de
profundidad (sus curvas de nivel). Una que está en las dos, o que además se mueve al cambiar
el paso de lente, sale de la trama.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

PX_PANEL = 3840.0      # ancho del panel; sirve para pasar celdas del mapa a píxeles


# ---------------------------------------------------------------- lecturas
def load_rgb(path):
    """PNG/PPM/PGM -> float64 luminancia, y el trío de canales si los hay."""
    im = Image.open(path)
    a = np.asarray(im)
    if a.ndim == 2:
        y = a.astype(np.float64)
        return y, None
    r, g, b = (a[..., i].astype(np.float64) for i in range(3))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b, (r, g, b)


def gauss1d(sigma):
    r = max(1, int(3.0 * sigma))
    x = np.arange(-r, r + 1, dtype=np.float64)
    k = np.exp(-(x * x) / (2.0 * sigma * sigma))
    return k / k.sum()


def difusa(y, s1, s2):
    """Pase de banda horizontal: lo que cambia a escala s1 menos lo que cambia a escala s2."""
    k1, k2 = gauss1d(s1), gauss1d(s2)
    a = np.apply_along_axis(lambda v: np.convolve(v, k1, mode="same"), 1, y)
    b = np.apply_along_axis(lambda v: np.convolve(v, k2, mode="same"), 1, y)
    return a - b


def espectro_x(y, win_lo=3.0, win_hi=900.0):
    """Espectro medio de las filas, en el tramo de periodos que nos interesa.

    Se promedia el módulo por fila en vez de promediar la imagen: si las franjas están
    desfasadas entre filas, promediar la imagen las borra y promediar módulos las deja.
    """
    h, w = y.shape
    det = np.hanning(w)
    f = np.fft.rfftfreq(w, d=1.0)           # ciclos por píxel
    mag = np.zeros(f.size)
    paso = max(1, h // 400)                # con 400 filas basta
    for i in range(0, h, paso):
        fila = y[i] - y[i].mean()
        mag += np.abs(np.fft.rfft(fila * det))
    mag /= (h + paso - 1) // paso
    per = np.where(f > 0, 1.0 / np.maximum(f, 1e-12), np.inf)
    val = (per >= win_lo) & (per <= win_hi)
    return f, mag, per, val


def picos(f, mag, per, val, n=6, umbral=2.2):
    """Los máximos más altos del tramo, con cuánto sobresalen del ruido de alrededor."""
    m = mag.copy()
    m[~val] = -1.0
    med = np.median(mag[val]) if val.any() else 0.0
    out = []
    for i in range(1, m.size - 1):
        if not val[i] or m[i] <= med:
            continue
        if m[i] >= m[i - 1] and m[i] >= m[i + 1]:
            ent = np.median(mag[max(0, i - 12):i - 2]) if i > 14 else med
            if ent <= 0:
                ent = max(med, 1e-9)
            out.append((m[i] / ent, per[i], mag[i], f[i]))
    out.sort(reverse=True)
    # junta picos demasiado juntos (un mismo haz con dos puntas)
    jun, vistos = [], []
    for r, p, a, fr in out:
        if any(abs(p - q) / q < 0.05 for q in vistos):
            continue
        vistos.append(p)
        jun.append((r, p, a, fr))
        if len(jun) >= n:
            break
    return jun


def energia_local(y, s1=6.0, s2=40.0, tile=96):
    """Dónde están las franjas: RMS del pase de banda en cuadros de tile x tile."""
    bp = difusa(y, s1, s2)
    h, w = y.shape
    ty, tx = h // tile, w // tile
    if ty < 1 or tx < 1:
        return bp.std(), np.zeros((1, 1))
    rec = np.zeros((ty, tx))
    for a in range(ty):
        for b in range(tx):
            rec[a, b] = bp[a * tile:(a + 1) * tile, b * tile:(b + 1) * tile].std()
    return bp.std(), rec


def porteadora_local(y, salto_fila=8, venta=9, umbral_rad=1.2):
    """Dónde la raya de la trama cambia de fase de golpe, fila por fila.

    La trama es una rayita de unos 1,8 px de periodo. Si el paso no cuadra con la lente,
    o si el mapa de profundidad mueve el desplazamiento de un ojo, esa rayita cambia de
    fase de golpe y el golpe se ve como una franja vertical (o como un borde roto). Aqui
    se baja la rayita al cero con un porteador complejo y se cuentan los golpes.
    """
    h, w = y.shape
    f, mag, per, val = espectro_x(y, win_lo=1.6, win_hi=2.8)
    if not val.any():
        return None
    i = int(np.argmax(np.where(val, mag, -1)))
    f0 = float(f[i])
    mue = y[::salto_fila]
    bp = mue - difusa(mue, 1.0, 2.6)             # deja solo la rayita fina de la trama
    x = np.arange(w, dtype=np.float64)
    z = bp * np.exp(-2j * np.pi * f0 * x)
    k = np.ones(venta) / venta
    zr = np.stack([np.convolve(fila, k, mode="same") for fila in z])
    amp = np.abs(zr)
    gap = 8
    bruto = np.angle(zr[:, gap:] * np.conj(zr[:, :-gap]))   # vuelta de fase en 8 px
    fuertes = np.abs(bruto) > umbral_rad
    por_mil = 1000.0 * fuertes.mean(axis=1)
    sep = []
    for r in range(0, fuertes.shape[0], max(1, fuertes.shape[0] // 24)):
        pos = np.flatnonzero(fuertes[r])
        if pos.size > 4:
            sep.extend(np.diff(pos).tolist())
    desfave = float(np.median(bruto)) / (2.0 * np.pi * gap)  # ciclos/px de mas o de menos
    return {
        "f0": f0,
        "periodo_px": 1.0 / f0 if f0 > 0 else float("nan"),
        "amp_mediana": float(np.median(amp)),
        "golpes_por_mil_px": float(np.mean(por_mil)),
        "separacion_mediana_px": float(np.median(sep)) if sep else float("nan"),
        "desfase_ciclos_px": desfave,
        "periodo_local_px": 1.0 / (f0 + desfave) if f0 + desfave > 0 else float("nan"),
    }


def mapa_niveles(g):
    """Del mapa de profundidad: cuántos niveles hay y de qué ancho son las mesetas."""
    h, w = g.shape
    celdas = {}
    largos, saltos = [], []
    for row in g[:: max(1, h // 240)]:
        d = np.diff(row)
        cort = np.flatnonzero(d != 0)
        bordes = np.concatenate(([0], cort + 1, [w]))
        ln = np.diff(bordes)
        largos.extend(ln.tolist())
        if cort.size:
            saltos.append(np.abs(row[cort + 1] - row[cort]))
    largos = np.array(largos, dtype=np.float64)
    salt = np.concatenate(saltos) if saltos else np.zeros(1)
    return {
        "niveles": len(np.unique(g)),
        "meseta_media_px": largos.mean(),
        "meseta_mediana_px": float(np.median(largos)),
        "mesetas_gruesas_frac": float((largos >= 8).mean()),
        "salto_medio_niveles": float(salt.mean()),
        "salto_mediano_niveles": float(np.median(salt)),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("base", help="prefijo de los volcados, por ejemplo /tmp/chk")
    ap.add_argument("--ancho", type=float, default=PX_PANEL, help="ancho del panel en píxeles")
    ap.add_argument("--con", help="ruta de la trama CON releve (por defecto -trama-con-mapa.png)")
    ap.add_argument("--sin", help="ruta de la trama SIN releve (por defecto -trama-sin-releve.png)")
    a = ap.parse_args()

    nombres = {
        "con releve": [a.con] if a.con else [a.base + "-trama-con-mapa.png", a.base + "-trama-vulkan.png"],
        "sin releve": [a.sin] if a.sin else [a.base + "-trama-sin-releve.png"],
    }
    datos = {}
    for etiqueta, posib in nombres.items():
        for p in posib:
            if os.path.exists(p):
                y, _ = load_rgb(p)
                datos[etiqueta] = (p, y)
                break
        else:
            print("sin %s (%s)" % (etiqueta, " o ".join(posib)))

    if not datos:
        print("\nno hay nada que medir: falta el --dump del visor (o `volcado = /tmp/chk` en serve.conf)")
        return 1

    print("\n=== franjas por periodo (media de las filas, pase de banda 6-40 px) ===")
    hallazgos = {}
    for etiqueta, (p, y) in datos.items():
        f, mag, per, val = espectro_x(y)
        ps = picos(f, mag, per, val)
        rms, rec = energia_local(y)
        print("\n%-11s %s  %dx%d" % (etiqueta, os.path.basename(p), y.shape[1], y.shape[0]))
        print("  resalte de la franja: RMS %.2f niveles · por zonas min %.2f max %.2f (cuadros de 96 px)"
              % (rms, rec.min(), rec.max()))
        for r, pp, am, fr in ps:
            print("  periodo %7.1f px (%5.2f ciclos/px)  resalte x%-6.1f  amplitud %.2f niveles"
                  % (pp, fr, min(r, 9999.0), am))
        hallazgos[etiqueta] = ps

    # el porteador de la trama: con kx = 0,55394 ciclos/px sale un pico espejo a 0,4461
    for etiqueta, (p, y) in datos.items():
        f, mag, per, val = espectro_x(y, win_lo=1.6, win_hi=2.8)
        if val.any():
            i = int(np.argmax(np.where(val, mag, -1)))
            print("\nporteador de la trama en %s: periodo %.2f px (%.4f ciclos/px), amplitud %.2f niveles"
                  % (etiqueta, per[i], f[i], mag[i]))

    print("\n=== la rayita de la trama, fila por fila ===")
    medidas = {}
    for etiqueta, (p, y) in datos.items():
        m = porteadora_local(y)
        if not m:
            print("  %s: sin porteador claro" % etiqueta)
            continue
        medidas[etiqueta] = m
        print("  %-11s periodo %.2f px · amplitud %.1f niveles · golpes de fase %.0f por mil px"
              % (etiqueta, m["periodo_px"], m["amp_mediana"], m["golpes_por_mil_px"]))
        print("  %-11s separacion entre golpes %.0f px · periodo local %.2f px (%+.4f de desvio)"
              % ("", m["separacion_mediana_px"], m["periodo_local_px"], m["desfase_ciclos_px"]))
    if len(medidas) == 2 and "con releve" in medidas and "sin releve" in medidas:
        c, s = medidas["con releve"], medidas["sin releve"]
        razon = c["golpes_por_mil_px"] / max(s["golpes_por_mil_px"], 0.5)
        print("\n  golpes de fase con releve / sin releve: x%.2f  ->  %s"
              % (razon,
                 "los pone el MAPA (sus curvas de nivel cortan la trama)" if razon > 1.6 else
                 "estan en las dos: los pone la TRAMA (paso o filtro mal ajustados)" if razon < 1.25 else
                 "poco claro: hace falta la trama sin releve del mismo cuadro"))

    pgm = a.base + "-profundidad.pgm"
    if os.path.exists(pgm):
        g = np.asarray(Image.open(pgm))
        m = mapa_niveles(g)
        esc = a.ancho / g.shape[1]
        print("\n=== mapa de profundidad %s (%dx%d, 1 celda = %.2f px de panel) ==="
              % (os.path.basename(pgm), g.shape[1], g.shape[0], esc))
        print("  niveles distintos: %d" % m["niveles"])
        print("  mesetas: media %.1f celdas (%.0f px de panel) · mediana %.1f celdas (%.0f px)"
              % (m["meseta_media_px"], m["meseta_media_px"] * esc,
                 m["meseta_mediana_px"], m["meseta_mediana_px"] * esc))
        print("  fracción de mesetas de 8 celdas o mas: %.1f%%" % (100.0 * m["mesetas_gruesas_frac"]))
        print("  salto entre mesetas: medio %.1f niveles · mediano %.1f"
              % (m["salto_medio_niveles"], m["salto_mediano_niveles"]))
        print("  con el tope de Samsung (22,65 px de recorrido) un nivel son %.3f px de desplazamiento,"
              % (22.65 / 255.0))
        print("  o sea %.4f ciclos de trama (kx = 0,55394): hace falta temblor, si no son escalones."
              % (22.65 / 255.0 * 0.5539365133))

    if "con releve" in hallazgos and "sin releve" in hallazgos:
        print("\n=== veredicto ===")
        con = {round(p, 1): r for r, p, _, _ in hallazgos["con releve"]}
        sin = {round(p, 1): r for r, p, _, _ in hallazgos["sin releve"]}
        for pp, r in sorted(con.items(), key=lambda kv: -kv[1])[:4]:
            cerca = [q for q in sin if abs(q - pp) / max(pp, 1.0) < 0.06]
            mejor = max((sin[q] for q in cerca), default=0.0)
            if r > 1.8 * max(mejor, 0.6):
                causa = "sale del MAPA de profundidad (con releve aparece, sin releve no)"
            elif cerca and r < 2.5 * max(mejor, 0.6):
                causa = "está en las dos tramas: sale de la TRAMA (paso / filtro de la lente)"
            else:
                causa = "dudoso: mira la pantalla y baja 3D Salida con Ctrl+Shift+5"
            print("  periodo %7.1f px  resalte con releve x%-5.1f  sin releve x%-5.1f  ->  %s"
                  % (pp, r, mejor, causa))
    return 0


if __name__ == "__main__":
    sys.exit(main())
