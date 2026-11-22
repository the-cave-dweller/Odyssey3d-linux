#!/usr/bin/env python3
"""prueba_rotacion.py — prueba de regresión de la tabla de rotaciones.

kscreen-doctor numera la rotación con la enum de KScreen (none=1, left=2, inverted=4,
right=8), no con la transformación de Wayland (donde 1 = 90°). El port solía interpretar
el 1 como "left" y al restaurar el panel ponía el monitor en vertical.

Escribe instantáneas sintéticas en ~/.local/state/odyssey3d/panel.state y comprueba que
`ctl panel restore --dry` genera la palabra correcta. No toca la pantalla.
"""
import os
import subprocess
import sys

BIN = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "odyssey3d")
STATE = os.path.expanduser("~/.local/state/odyssey3d/panel.state")

CASOS = [(1, "none"), (2, "left"), (4, "inverted"), (8, "right"), (0, None)]


def run(args):
    r = subprocess.run([BIN] + args, capture_output=True, text=True)
    return r.stdout + r.stderr


def main():
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    fallos = 0
    for num, esperado in CASOS:
        with open(STATE, "w") as f:
            f.write(f"output=DP-4\nmode=2 3840x2160@164.98\nscale=1.5\nrotation={num}\nvrr=Always\nhdr=1\n")
        salida = run(["ctl", "panel", "restore", "--dry"])
        linea = next((l for l in salida.splitlines() if "kscreen-doctor" in l), "")
        token = None
        for tok in linea.split():
            if ".rotation." in tok:
                token = tok.split(".rotation.")[-1]
        if token != esperado:
            fallos += 1
            print(f"  mal: rotation={num} -> {token!r}  (esperado {esperado!r})   [{linea}]")
        else:
            print(f"  ok : rotation={num} -> {token!r}")
    os.remove(STATE)
    print("TODO OK" if not fallos else f"{fallos} CASOS FALLOS")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
