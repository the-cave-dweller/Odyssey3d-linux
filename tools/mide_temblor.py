# mide_temblor.py — cuanto se mueve el mapa de profundidad de un video segun cuantos mapas por segundo
# se calculen (v0.70). Entrada: dos carpetas por clip con los -mapa.pgm que deja `odysseyvk mapa` sobre
# los cuadros f001.png ... f150.png de un video de 25 cuadros por segundo:
#   CARPETA/sec/<clip>-A  uno de cada cuatro cuadros, con ODYSSEYVK_PASO_FIJO=0.16  (~6 mapas/s)
#   CARPETA/sec/<clip>-B  todos los cuadros,          con ODYSSEYVK_PASO_FIJO=0.04  (25 mapas/s)
# (los cuadros se sacan con ffmpeg y los dos `mapa` van SIN --sueltas: son cuadros seguidos.)
# Salida, en pixeles de pantalla por segundo (27,2 px por unidad de mapa): todo lo que se mueve el mapa,
# lo que sobra respecto al cambio neto, y los vaivenes (cambios que se deshacen): eso es el temblor.
#   python3 tools/mide_temblor.py CARPETA
# Medido el 25-nov con mde_large: nave 0,34 -> 0,86 px/s de vaivenes, globo 0,32 -> 0,76.
import sys, glob, os, re
import numpy as np
from PIL import Image
S=sys.argv[1]; G=27.2   # px por unidad de mapa (relieve 1,2 · salida 50)
def serie(d):
    """mapas colocados en su cuadro de video (25 por segundo), sosteniendo el ultimo"""
    ms={int(re.search(r"f(\d+)-mapa", f).group(1)): np.asarray(Image.open(f)).astype(np.float32)/255 for f in glob.glob(f"{d}/*-mapa.pgm")}
    out=[]; ult=None
    for i in range(1,151):
        if i in ms: ult=ms[i]
        out.append(ult)
    return out, len(ms)
def medidas(seq, desde=38):   # se descarta el arranque (1,5 s)
    seq=seq[desde:]; dur=len(seq)/25.0
    pasos=[np.abs(b-a) for a,b in zip(seq,seq[1:])]
    camino=sum(p.mean() for p in pasos)                      # todo lo que se mueve el mapa
    neto=np.abs(seq[-1]-seq[0]).mean()                       # lo que de verdad cambio de principio a fin
    # vaivenes: cambios que se deshacen (signo contrario al cambio anterior no nulo)
    vaiven=0.0; prev=None
    for a,b in zip(seq,seq[1:]):
        d=b-a
        if prev is not None:
            m=(d*prev)<0; vaiven+=np.abs(d[m]).sum()/d.size
        nz=d!=0
        prev=np.where(nz,d,prev) if prev is not None else d
    saltos=[p.mean() for p in pasos if p.max()>0]
    return camino/dur*G, (camino-neto)/dur*G, vaiven/dur*G, np.mean(saltos)*G if saltos else 0, len(saltos)/dur
print(f"{'':26s} {'mapas/s':>8s} {'movimiento':>11s} {'sobrante':>9s} {'vaivenes':>9s} {'salto medio':>12s}")
print(f"{'':26s} {'':>8s} {'px/s':>11s} {'px/s':>9s} {'px/s':>9s} {'px por mapa':>12s}")
for c,nom in (("voy","nave (escena lenta)"),("core","globo (animacion)")):
    for r,et in (("A","antes: ~6 mapas/s"),("B","ahora: 25 mapas/s")):
        seq,n=serie(f"{S}/sec/{c}-{r}")
        cam,sob,vai,salto,rate=medidas(seq)
        print(f"{nom[:12]:12s} {et:18s} {rate:6.1f} {cam:11.3f} {sob:9.3f} {vai:9.3f} {salto:12.4f}")
