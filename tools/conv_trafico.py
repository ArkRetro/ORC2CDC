#!/usr/bin/env python3
"""conv_trafico.py tu.iso carpeta: los coches del trafico (CARS.PS2, entrada 0: unos 17 registros por coche: carroceria,
luces, sombra, dos ruedas en su origen y, 8 registros despues, la carroceria ligera) a TRAFn.DAT (formato ORD5,
modelo.py): la carroceria ligera con las ruedas en sus cuatro sitios y solo las texturas que usa. La pintura (mallas
sin textura) en gris claro. Sale de tu disco: no se distribuye."""
import os, struct, sys, zlib
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2 import iso as isomod, stage, vif
import modelo

COCHES = [1, 52, 69, 103, 35, 120, 86, 136, 18]        # utilitario, berlinas, autocaravana, autobus...

class Mem:
    def __init__(self, d): self.m = d

def caja(b, r):
    P = [tr[0x68] for pk in b.mallas(r) for tr in vif.tiras(pk)]
    if not P: return None
    P = np.vstack(P).astype(float); P = P[np.isfinite(P).all(1)]
    return (P.min(0), P.max(0)) if len(P) else None

def main():
    iso = isomod.Iso(sys.argv[1]); dest = sys.argv[2]
    h, o, s = iso.paquete('CARS.PS2')[0]; d = zlib.decompress(iso.m[o:o + s])
    w = struct.unpack('<4I', d[:16]); b = stage.Bloque(Mem(d), 0, w)
    for n, r in enumerate(COCHES):
        cuerpo = r + 8
        cb = caja(b, cuerpo); lo, hi = cb
        ruedas = [q for q in range(r, r + 17) if q != cuerpo and (c := caja(b, q)) is not None
                  and np.abs(c[0]).max() < 0.6 and np.abs(c[1]).max() < 0.6 and c[1][1] - c[0][1] > 0.5]
        rad = max(caja(b, q)[1][1] for q in ruedas[:2]) if ruedas else 0.35
        largo = hi[2] - lo[2]; xr = hi[0] - 0.2
        zd = lo[2] + 0.17 * largo + 0.25; zt = hi[2] - 0.17 * largo - 0.25
        copias = {}
        if ruedas:
            copias[ruedas[0]] = [(-xr, rad, zd), (-xr, rad, zt)]
            copias[ruedas[1 % len(ruedas)]] = copias.get(ruedas[1 % len(ruedas)], []) + [(xr, rad, zd), (xr, rad, zt)]
        mod = modelo.Modelo(); mod.ruta = np.zeros((0, 5))
        mod.bloque(b, [cuerpo] + ruedas[:2], sin_textura=0xFFB0B0B0, copias=copias, lista=None)
        for x in mod.mallas:                                   # todo opaco salvo lo que tenga transparencia
            if x[1] < 0: x[0] = 0
        mod.recorta_texturas()
        ini = mod.escribe(os.path.join(dest, 'TRAF%d.DAT' % n))
        print('trafico %d (registro %d): %.1f m, %d tiras, %d triangulos, %d texturas (%d KB), ruedas %s'
              % (n, cuerpo, largo, len(mod.mallas), sum(x[5] - 2 for x in mod.mallas), len(mod.texs),
                 len(mod.blob) >> 10, ruedas[:2]))

if __name__ == '__main__':
    main()
