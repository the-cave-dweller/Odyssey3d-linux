"""mide_rastreo.py — compara dos pasadas del rastreo de cabeza sobre los MISMOS cuadros (v0.74).

Como se usa (la camara solo puede tenerla un proceso: que no haya una sesion 3D abierta):
  mkdir /tmp/toma && odysseyvk ctl track --segundos 20 --grabar /tmp/toma      # graba la camara tal cual
  odysseyvk ctl track --repetir /tmp/toma --csv /tmp/antes.csv --fino no       # el rastreo de antes
  odysseyvk ctl track --repetir /tmp/toma --csv /tmp/ahora.csv                 # el rastreo fino
  python3 tools/mide_rastreo.py /tmp/antes.csv /tmp/ahora.csv [inicio fin]     # [tramo quieto, en segundos]

Saca el ruido de la medida cruda, lo que le llega a la trama (la pose filtrada) con la cabeza quieta
y eso traducido a ciclos de fase: en el centro de la pantalla 5,239/z por cm de x, y en el borde (a
30 cm del centro) 5,239*30/z^2 por cm de z. El ojo que debe ver negro tiene +-0,04 de margen.
La grabacion son fotos de la cara: borrarla al terminar."""
import sys, numpy as np
A = np.genfromtxt(sys.argv[1], delimiter=',', names=True); B = np.genfromtxt(sys.argv[2], delimiter=',', names=True)
quieto = (float(sys.argv[3]), float(sys.argv[4])) if len(sys.argv) > 4 else (16.3, 20.3)
def sin_tend(v, t, ven):
    o = np.full_like(v, np.nan)
    for i in range(len(v)):
        m = (np.abs(t - t[i]) <= ven / 2) & np.isfinite(v)
        if np.isfinite(v[i]) and m.sum() > 3: o[i] = v[i] - v[m].mean()
    return o
def rms(v): v = v[np.isfinite(v)]; return float(np.sqrt(np.mean(v * v))) if len(v) else float('nan')
def fila(nombre, fa, fb, fmt="{:8.3f}"):
    print(f"  {nombre:46s} {fmt.format(fa)}   {fmt.format(fb)}   x{fa / fb if fb else float('nan'):.1f}")
print(f"{'':48s} {'antes':>8s}   {'ahora':>8s}")
res = {}
for nom, D in (('A', A), ('B', B)):
    va = D['valida'] == 1; ok = va & (D['mantenida'] == 0)   # ok = cuadros con medida de verdad (no aguantados)
    t = D['t']; r = {}
    cz = np.where(ok, D['camZ'], np.nan); cx = np.where(ok, D['camX'], np.nan); cy = np.where(ok, D['camY'], np.nan)
    r['crudo z 1s'] = rms(sin_tend(cz, t, 1.0)); r['crudo x 1s'] = rms(sin_tend(cx, t, 1.0)); r['crudo y 1s'] = rms(sin_tend(cy, t, 1.0))
    q = (t >= quieto[0]) & (t <= quieto[1])
    for c, k in (('crudoX', 'x'), ('crudoY', 'y'), ('crudoZ', 'z')):
        w = np.where(ok, D[c], np.nan)[q]; r[f'quieto crudo {k} sd'] = np.nanstd(w); r[f'quieto crudo {k} rec'] = np.nanmax(w) - np.nanmin(w)
    for c, k in (('panelX', 'x'), ('panelY', 'y'), ('panelZ', 'z')):
        w = np.where(va, D[c], np.nan)[q]; r[f'quieto filtrado {k} sd'] = np.nanstd(w); r[f'quieto filtrado {k} rec'] = np.nanmax(w) - np.nanmin(w)
        r[f'filtrado {k} 3s'] = rms(sin_tend(np.where(va, D[c], np.nan), t, 3.0))
    zm = np.nanmedian(np.where(va, D['panelZ'], np.nan))
    px = np.where(va, D['panelX'], np.nan); pz = np.where(va, D['panelZ'], np.nan)
    r['fase centro quieto rec'] = r['quieto filtrado x rec'] * 5.239 / zm
    r['fase borde quieto rec'] = r['quieto filtrado z rec'] * 5.239 * 30 / zm**2
    ipd = D['ipd'][ok & (D['ipd'] > 0)]
    r['ipd med'] = np.median(ipd); r['ipd p5'] = np.percentile(ipd, 5); r['ipd p95'] = np.percentile(ipd, 95)
    r['ipd fuera'] = 100 * np.mean(np.abs(ipd - np.median(ipd)) / np.median(ipd) > 0.08)
    r['zm'] = zm; r['validas'] = int(va.sum())
    res[nom] = r
a, b = res['A'], res['B']
print("ruido de la medida cruda, cuadro a cuadro (sin la tendencia de 1 s; incluye movimiento real):")
fila("x del punto medio (mm)", a['crudo x 1s'], b['crudo x 1s']); fila("y (mm)", a['crudo y 1s'], b['crudo y 1s']); fila("z, la distancia (mm)", a['crudo z 1s'], b['crudo z 1s'])
print(f"tramo mas quieto ({quieto[0]:.1f}-{quieto[1]:.1f} s), medida cruda en el panel (cm): desviacion")
for k in 'xyz': fila(f"{k}", a[f'quieto crudo {k} sd'], b[f'quieto crudo {k} sd'])
print("  ... y recorrido (maximo menos minimo):")
for k in 'xyz': fila(f"{k}", a[f'quieto crudo {k} rec'], b[f'quieto crudo {k} rec'])
print("tramo mas quieto, pose FILTRADA (la que usa la trama): desviacion y recorrido (cm)")
for k in 'xyz': fila(f"{k} desviacion", a[f'quieto filtrado {k} sd'], b[f'quieto filtrado {k} sd'])
for k in 'xyz': fila(f"{k} recorrido", a[f'quieto filtrado {k} rec'], b[f'quieto filtrado {k} rec'])
print("fase de la trama en el tramo quieto (ciclos, recorrido; el margen es +-0,04 = 0,08 de recorrido):")
fila("centro de la pantalla, por x", a['fase centro quieto rec'], b['fase centro quieto rec'], "{:8.4f}")
fila("borde de la pantalla, por z", a['fase borde quieto rec'], b['fase borde quieto rec'], "{:8.4f}")
print(f"distancia entre ojos medida (mm): antes mediana {a['ipd med']:.1f} (p5 {a['ipd p5']:.1f}, p95 {a['ipd p95']:.1f}, {a['ipd fuera']:.0f} % fuera de +-8 %)")
print(f"                                  ahora mediana {b['ipd med']:.1f} (p5 {b['ipd p5']:.1f}, p95 {b['ipd p95']:.1f}, {b['ipd fuera']:.0f} % fuera de +-8 %)")
print(f"distancia mediana al panel: antes {a['zm']:.2f} cm, ahora {b['zm']:.2f} cm · poses validas {a['validas']} y {b['validas']}")
# la z filtrada por segundo: lo que ve la trama
t = A['t']
print("z filtrada por segundo (mediana, cm):")
print("  antes " + " ".join(f"{np.nanmedian(np.where(A['valida']==1, A['panelZ'], np.nan)[(t>=k)&(t<k+1)]):5.1f}" for k in range(int(t[-1]) + 1)))
print("  ahora " + " ".join(f"{np.nanmedian(np.where(B['valida']==1, B['panelZ'], np.nan)[(t>=k)&(t<k+1)]):5.1f}" for k in range(int(t[-1]) + 1)))
print("x filtrada por segundo (mediana, cm):")
print("  antes " + " ".join(f"{np.nanmedian(np.where(A['valida']==1, A['panelX'], np.nan)[(t>=k)&(t<k+1)]):5.1f}" for k in range(int(t[-1]) + 1)))
print("  ahora " + " ".join(f"{np.nanmedian(np.where(B['valida']==1, B['panelX'], np.nan)[(t>=k)&(t<k+1)]):5.1f}" for k in range(int(t[-1]) + 1)))
