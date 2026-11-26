#!/usr/bin/env python3
"""Hace de la capa de OdysseyVK sin juego (v1.13, fase 2): escribe una profundidad guardada en la memoria
compartida con el mismo protocolo que capa/protocolo.h, unas 60 veces por segundo, para probar el visor
en el compositor aparte.

  ODYSSEYVK_PROFUNDIDAD_SHM=/odysseyvk-profundidad-prueba \\
    python3 tools/simula-capa.py profundidad.r32f 1920 1080 [--segundos 60]

La profundidad es la del dispositivo en float32 (la que vuelca la capa en la fase 1, o el depth.r32f de un
volcado de Neural Forge). Al terminar borra la memoria compartida."""
import argparse
import mmap
import os
import struct
import sys
import time

MAGIA, VERSION, HUECOS, CABECERA, DATOS = 0x4F564450, 1, 3, 4096, 64
D32 = 126   # VK_FORMAT_D32_SFLOAT


def main():
    p = argparse.ArgumentParser()
    p.add_argument("profundidad")
    p.add_argument("ancho", type=int)
    p.add_argument("alto", type=int)
    p.add_argument("--segundos", type=float, default=60.0)
    a = p.parse_args()
    nombre = os.environ.get("ODYSSEYVK_PROFUNDIDAD_SHM", f"/odysseyvk-profundidad-{os.getuid()}")
    datos = open(a.profundidad, "rb").read()
    if len(datos) != a.ancho * a.alto * 4:
        sys.exit(f"{a.profundidad} no mide {a.ancho}x{a.alto} en float32")
    redondea = lambda v: (v + 4095) // 4096 * 4096
    hueco = redondea(DATOS + len(datos) + 8)
    total = CABECERA + HUECOS * hueco
    ruta = "/dev/shm" + nombre
    fd = os.open(ruta, os.O_CREAT | os.O_RDWR, 0o600)
    os.ftruncate(fd, total)
    m = mmap.mmap(fd, total)
    m[0:CABECERA] = bytes(CABECERA)
    # magia, version, pid, ancho, alto, formato, bytes por punto, huecos, tamHueco (la magia al final)
    struct.pack_into("<IIiIIIIIQ", m, 0, 0, VERSION, os.getpid(), a.ancho, a.alto, D32, 4, HUECOS, hueco)
    struct.pack_into("<I", m, 0, MAGIA)
    print(f"[simula] {ruta}: {a.ancho}x{a.alto} D32_SFLOAT, {HUECOS} huecos de {hueco} bytes", flush=True)
    seq, fin, lector = 0, time.time() + a.segundos, False
    try:
        while time.time() < fin:
            seq += 1
            h = CABECERA + (seq % HUECOS) * hueco
            struct.pack_into("<Q", m, h, seq)                       # 1. seqAntes
            m[h + DATOS:h + DATOS + len(datos)] = datos             # 2. los datos
            struct.pack_into("<Q", m, h + hueco - 8, seq)           # 3. seqDespues
            struct.pack_into("<Q", m, 40, seq)                      # 4. ultimoSeq
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
