#!/usr/bin/env python3
"""Hace de la capa de OdysseyVK sin juego (v1.13, fases 2 y 3c): escribe una profundidad guardada en la memoria
compartida con el mismo protocolo que capa/protocolo.h (version 2), unas 60 veces por segundo, para probar el
visor en el compositor aparte. Con --hud, tambien la mascara del HUD.

  ODYSSEYVK_PROFUNDIDAD_SHM=/odysseyvk-profundidad-prueba \\
    python3 tools/simula-capa.py profundidad.r32f 1920 1080 [--segundos 60] [--hud destino.raw]

La profundidad es la del dispositivo en float32 (la que vuelca la capa en la fase 1, o el depth.r32f de un
volcado de Neural Forge). --hud: la imagen de la capa del HUD como la vuelca la sonda (destino-<pid>-<n>.raw,
RGBA8 del tamano de --hud-medida, 3840x2160 por defecto); se reduce a 960x540 como en la tarjeta (la media de
4x4) y se escribe en los huecos del HUD. --otra / --cada turnan dos profundidades. Al terminar borra la memoria
compartida."""
import argparse
import mmap
import os
import struct
import sys
import time

MAGIA, VERSION, HUECOS, CABECERA, DATOS = 0x4F564450, 6, 3, 4096, 64
HUD_W, HUD_H, HUD_CAPAS = 960, 540, 2
D32 = 126   # VK_FORMAT_D32_SFLOAT


def reduce_hud(ruta, w, h):
    """la imagen del HUD a HUD_W x HUD_H, cada punto la media de su bloque (lo que hacen los dos blit)"""
    import numpy as np
    a = np.fromfile(ruta, dtype=np.uint8)
    if a.size != w * h * 4:
        sys.exit(f"{ruta} no mide {w}x{h} en RGBA8")
    a = a.reshape(h, w, 4).astype(np.float32)
    bx, by = w // HUD_W, h // HUD_H
    a = a[:HUD_H * by, :HUD_W * bx].reshape(HUD_H, by, HUD_W, bx, 4).mean((1, 3))
    return np.clip(a + 0.5, 0, 255).astype(np.uint8).tobytes()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("profundidad")
    p.add_argument("ancho", type=int)
    p.add_argument("alto", type=int)
    p.add_argument("--segundos", type=float, default=60.0)
    p.add_argument("--otra", help="otra profundidad del mismo tamano, que se turna con la primera")
    p.add_argument("--cada", type=float, default=3.0, help="segundos de cada una al turnarse (con --otra)")
    p.add_argument("--hud", help="la capa del HUD (RGBA8) volcada por la sonda: su alfa va como mascara")
    p.add_argument("--hud-medida", default="3840x2160")
    p.add_argument("--hud2", help="una segunda capa del mismo cuadro (como el menu sobre el HUD): va su union")
    p.add_argument("--presentes", type=int, default=1,
                   help="cuadros que presenta el juego por cada profundidad (2 = generacion de cuadros de DLSS)")
    a = p.parse_args()
    nombre = os.environ.get("ODYSSEYVK_PROFUNDIDAD_SHM", f"/odysseyvk-profundidad-{os.getuid()}")
    lista = [a.profundidad] + ([a.otra] if a.otra else [])
    todas = [open(f, "rb").read() for f in lista]
    for f, d in zip(lista, todas):
        if len(d) != a.ancho * a.alto * 4:
            sys.exit(f"{f} no mide {a.ancho}x{a.alto} en float32")
    hud = None
    if a.hud:
        hw, hh = (int(v) for v in a.hud_medida.split("x"))
        hud = reduce_hud(a.hud, hw, hh)
        if a.hud2:
            hud += reduce_hud(a.hud2, hw, hh)
    datos = todas[0]
    redondea = lambda v: (v + 4095) // 4096 * 4096
    hueco = redondea(DATOS + len(datos) + 8)
    hud_inicio = redondea(CABECERA + HUECOS * hueco)
    hud_hueco = redondea(DATOS + HUD_CAPAS * HUD_W * HUD_H * 4 + 8)
    total = hud_inicio + HUECOS * hud_hueco
    ruta = "/dev/shm" + nombre
    fd = os.open(ruta, os.O_CREAT | os.O_RDWR, 0o600)
    os.ftruncate(fd, total)
    m = mmap.mmap(fd, total)
    m[0:CABECERA] = bytes(CABECERA)
    # magia, version, pid, ancho, alto, formato, bytes por punto, huecos, tamHueco (la magia al final)
    struct.pack_into("<IIiIIIIIQ", m, 0, 0, VERSION, os.getpid(), a.ancho, a.alto, D32, 4, HUECOS, hueco)
    # version 2: hudAncho, hudAlto, hudBytesPunto, hudHuecos, hudInicio, hudTamHueco (offset 56), la fuente (96)
    struct.pack_into("<IIIIQQ", m, 56, HUD_W, HUD_H, 4, HUECOS, hud_inicio, hud_hueco)
    struct.pack_into("<I", m, 104, HUD_CAPAS)   # version 3: hudCapasMax
    if hud:
        hw, hh = (int(v) for v in a.hud_medida.split("x"))
        struct.pack_into("<II", m, 96, hw, hh)
    struct.pack_into("<I", m, 0, MAGIA)
    print(f"[simula] {ruta}: {a.ancho}x{a.alto} D32_SFLOAT, {HUECOS} huecos de {hueco} bytes"
          f"{', y la mascara del HUD de ' + a.hud if hud else ''}", flush=True)
    t0 = time.time()
    seq, fin, lector, cual, pedido = 0, t0 + a.segundos, False, 0, (0, 0)
    try:
        while time.time() < fin:
            seq += 1
            ahora_c = int((time.time() - t0) / a.cada) % len(todas)
            if ahora_c != cual:
                cual = ahora_c
                print(f"[simula] ahora {lista[cual]} (cuadro {seq})", flush=True)
            datos = todas[cual]
            h = CABECERA + (seq % HUECOS) * hueco
            struct.pack_into("<Q", m, h, seq)                       # 1. seqAntes
            m[h + DATOS:h + DATOS + len(datos)] = datos             # 2. los datos
            struct.pack_into("<Q", m, h + hueco - 8, seq)           # 3. seqDespues
            struct.pack_into("<Q", m, 40, seq)                      # 4. ultimoSeq
            struct.pack_into("<Q", m, 112, seq * a.presentes)       # (version 4) los cuadros presentados
            if hud:                                                 # lo mismo con el HUD (hudUltimoSeq en 88)
                h = hud_inicio + (seq % HUECOS) * hud_hueco
                struct.pack_into("<Q", m, h, seq)
                struct.pack_into("<I", m, h + 8, len(hud) // (HUD_W * HUD_H * 4))   # cuantas capas
                m[h + DATOS:h + DATOS + len(hud)] = hud
                struct.pack_into("<Q", m, h + hud_hueco - 8, seq)
                struct.pack_into("<Q", m, 88, seq)
            quiere, tope = struct.unpack_from("<II", m, 120)   # version 5 y 6: lo que pide el visor
            if (quiere, tope) != pedido:
                pedido = (quiere, tope)
                print(f"[simula] el visor pide: mascara del HUD {'si' if quiere else 'no'}, tope de FPS "
                      f"{tope if tope else 'ninguno'} (cuadro {seq})", flush=True)
            latido = struct.unpack_from("<Q", m, 48)[0]
            ahora = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
            vivo = latido > 0 and 0 <= ahora - latido < 2_000_000_000
            if vivo != lector:
                lector = vivo
                print(f"[simula] {'el visor lee' if vivo else 'el visor dejo de leer'} (cuadro {seq})", flush=True)
            time.sleep(1 / 60)
    finally:
        m.close()
        os.close(fd)
        os.unlink(ruta)
        print(f"[simula] fin: {seq} cuadros; {ruta} borrada", flush=True)


if __name__ == "__main__":
    main()
