#!/usr/bin/env python3
"""conv_param.py tu.iso salida.dat: la tabla de parametros de los coches del ejecutable de PS2 (SLES_*), 19 coches por
fila (0x4c bytes), la misma que la del PC (event+0x2b4). Se localiza por su firma: fila 4 (batalla) entre 2 y 3,1 m,
fila 5 (via) entre 1,2 y 2 m y fila 85 mayor que la batalla (radio de giro). Se guardan 200 filas y despues las
curvas de par motor (162 floats) y de presion de freno (256 floats) que estan justo antes. Sale de tu disco:
no se distribuye."""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2 import iso as isomod

C, FILAS = 19, 200

def busca(d):
    f = np.nan_to_num(np.frombuffer(d[:len(d) // 4 * 4], '<f4').astype(np.float64), nan=-1e9, posinf=-1e9, neginf=-1e9)
    for b in range(0, len(f) - 86 * C - C):
        r4 = f[b + 4 * C:b + 5 * C]
        if not np.all((r4 > 2.0) & (r4 < 3.1)): continue
        r5 = f[b + 5 * C:b + 6 * C]
        if not np.all((r5 > 1.2) & (r5 < 2.0)): continue
        if np.all(f[b + 85 * C:b + 86 * C] > r4) and np.all(f[b:b + C] > 500): return b * 4
    return None

def main():
    iso = isomod.Iso(sys.argv[1])
    nombre = next(k for k in iso.fich if k.startswith('SLES') or k.startswith('SLUS') or k.startswith('SLPM'))
    o, s = iso.fich[nombre]; d = bytes(iso.m[o:o + s])
    b = busca(d)
    if b is None: sys.exit('no encuentro la tabla de parametros en ' + nombre)
    # curvas del cambio automatico justo antes de la tabla: par motor (162 floats: muestras por rad/s, ganancia y
    # 160 muestras) 0x690 bytes antes, y presion de freno (256 floats) 0xa90 bytes antes
    par, fre = d[b - 0x690:b - 0x690 + 162 * 4], d[b - 0xa90:b - 0xa90 + 256 * 4]
    open(sys.argv[2], 'wb').write(d[b:b + 0x4c * FILAS] + par + fre)
    print('tabla de %s en 0x%x: %d filas de %d coches, curvas de par y freno' % (nombre, b, FILAS, C))

if __name__ == '__main__':
    main()
