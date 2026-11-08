#!/usr/bin/env python3
"""conv_coche.py tu.iso entrada salida.dat [--vista imagen.png] | --alto k_alto k_ligero registros: un coche de CARS.PS2 (bloque comprimido con zlib,
mismo formato que las etapas) al formato ORD4 de la Dreamcast (modelo.py), sin ruta. Del modelo ligero (entradas
33 en adelante) se toman la carroceria y los cristales y luces; las dos ruedas (registros en su origen) se repiten
en sus cuatro sitios. La pintura (mallas sin textura) va en rojo. Sale de tu disco: no se distribuye."""
import os, struct, sys, zlib
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2 import iso as isomod, stage, vif
import modelo

class Mem:
    def __init__(self, d): self.m = d

def bloque(iso, k):
    h, o, s = iso.paquete('CARS.PS2')[k]; d = zlib.decompress(iso.m[o:o + s])
    w = struct.unpack('<4I', d[:16]); return stage.Bloque(Mem(d), 0, w)

def caja(b, r):
    P = [tr[0x68] for pk in b.mallas(r) for tr in vif.tiras(pk)]
    if not P: return None
    P = np.vstack(P).astype(float); return P.min(0), P.max(0)

def alto(iso, k_alto, k_ligero, regs, sal):
    """modelo alto: los registros exteriores dados (carroceria, luces, retrovisores) y las ruedas del modelo ligero
    colocadas segun la carroceria alta; sin interior ni bajos"""
    b = bloque(iso, k_alto); bl = bloque(iso, k_ligero)
    lo, hi = caja(b, regs[0])
    cl = {r: caja(bl, r) for r in range(bl.w[0])}
    ruedas = [r for r, c in cl.items() if c is not None and np.abs(c[0]).max() < 0.5 and np.abs(c[1]).max() < 0.5
              and c[1][1] - c[0][1] > 0.4]
    rad = max(cl[r][1][1] for r in ruedas[:2])
    largo = hi[2] - lo[2]; xr = hi[0] - 0.16; zd = lo[2] + 0.2 * largo + 0.25; zt = hi[2] - 0.2 * largo - 0.2
    copias = {ruedas[0]: [(-xr, rad, zd), (-xr, rad, zt)], ruedas[1 % len(ruedas)]: [(xr, rad, zd), (xr, rad, zt)]}
    mod = modelo.Modelo(); mod.ruta = np.zeros((0, 5))
    mod.bloque(b, regs, sin_textura=None)
    mod.bloque(bl, ruedas[:2], sin_textura=None, copias=copias)
    mod.recorta_texturas()
    mod.escribe(sal)
    print('coche alto %d (+ruedas de %d): %d tiras, %d triangulos, %d texturas (%d KB)' % (k_alto, k_ligero,
          len(mod.mallas), sum(x[5] - 2 for x in mod.mallas), len(mod.texs), len(mod.blob) >> 10))

def main():
    a = sys.argv[1:]; vista = None
    if '--alto' in a:                                          # --alto k_alto k_ligero r1,r2,..
        i = a.index('--alto'); alto(isomod.Iso(a[0]), int(a[i + 1]), int(a[i + 2]), [int(x) for x in a[i + 3].split(',')], a[2])
        return
    if '--vista' in a: i = a.index('--vista'); vista = a[i + 1]; del a[i:i + 2]
    iso = isomod.Iso(a[0]); k = int(a[1]); sal = a[2]
    b = bloque(iso, k); cajas = {r: caja(b, r) for r in range(b.w[0])}
    # ruedas: registros pequenos y redondos alrededor del origen; carroceria: el registro mas largo
    ruedas = [r for r, c in cajas.items() if c is not None and np.abs(c[0]).max() < 0.5 and np.abs(c[1]).max() < 0.5
              and c[1][1] - c[0][1] > 0.4]
    cuerpo = max((r for r, c in cajas.items() if c is not None), key=lambda r: cajas[r][1][2] - cajas[r][0][2]
                 if cajas[r][1][2] - cajas[r][0][2] < 6 else 0)
    lo, hi = cajas[cuerpo]
    mitad = b.w[0] // 2 + 1                                      # la segunda mitad repite la primera (variantes)
    resto = [r for r, c in cajas.items() if c is not None and r < mitad and r not in ruedas
             and (c[1] - c[0]).max() < 6]
    rad = max(cajas[r][1][1] for r in ruedas[:2])
    largo = hi[2] - lo[2]; xr = hi[0] - 0.18; zd = lo[2] + 0.2 * largo + 0.25; zt = hi[2] - 0.2 * largo - 0.2
    copias = {ruedas[0]: [(-xr, rad, zd), (-xr, rad, zt)], ruedas[1 % len(ruedas)]: [(xr, rad, zd), (xr, rad, zt)]}
    print('registros', sorted(resto), 'ruedas', ruedas[:2], 'radio %.2f' % rad, 'largo %.2f' % largo)
    mod = modelo.Modelo(); mod.ruta = np.zeros((0, 5))
    mod.bloque(b, sorted(set(resto + ruedas[:2])), sin_textura=None, copias=copias)
    ini = mod.escribe(sal)
    print('coche %d: %d tiras, %d triangulos, %d texturas (%d KB), %d KB' % (k, len(mod.mallas),
          sum(x[5] - 2 for x in mod.mallas), len(mod.texs), len(mod.blob) >> 10, os.path.getsize(sal) >> 10))
    if vista:
        from PIL import Image, ImageDraw
        im = Image.new('RGB', (900, 300), (40, 40, 40)); dr = ImageDraw.Draw(im); S = 180
        V = np.concatenate(mod.verts)
        for li, ti, cc, r, i0, n, _ in mod.mallas:
            p = V[i0:i0 + n]
            for j in range(n - 2):
                t = p[j:j + 3]; c = int(t['c'][0]); col = ((c >> 16) & 255, (c >> 8) & 255, c & 255)
                if ti >= 0: col = (120, 120, 160)
                dr.polygon([(450 + q['z'] * S, 250 - q['y'] * S) for q in t], fill=col)
        im.save(vista)

if __name__ == '__main__':
    main()
