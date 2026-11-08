#!/usr/bin/env python3
"""conv_trafico.py tu.iso carpeta [--solo-tablas]: los coches del trafico (CARS.PS2, entrada 0: unos 17 registros por
coche: carroceria, luces, sombra, dos ruedas en su origen y, 8 registros despues, la carroceria ligera) a TRAFn.DAT
(formato ORD5, modelo.py): la carroceria ligera con las ruedas en sus cuatro sitios y solo las texturas que usa. La
pintura (mallas sin textura) en gris claro.
Ademas TRAFC.DAT con lo que el juego sabe de cada modelo de trafico, sacado del ejecutable (SLES_*):
  'TRC1' + u32 n + por coche (en el orden de TRAFn): i32 modelo (car+0x11), i32 clase (+0x324: 0..2 pesados, 3..5
         coches), i32 +0x31C (sonido del golpe), i32 +0x320, f32 radio (+0x18 del registro, el de la prueba de
         cercania de 4F9450) y f32 x 7 la primera caja de choque (x0, y0, z0, x1, y1, z1, r; descriptor de 520950).
         Registros de 4866C0 (0x44 bytes por modelo) y descriptores de 520950 (0x3C por modelo), localizados por firma.
  'ACL1' + u32 7 + 7 x 5 f32 la tabla por clase de 472090 / 47E780 (5B3E08 del PC): velocidad de referencia (km/h del
         modelo), aceleracion, frenada, aceleracion fuerte y frenada fuerte (m/cuadro por cuadro).
Con --solo-tablas solo se escribe TRAFC.DAT. Sale de tu disco: no se distribuye."""
import os, struct, sys, zlib
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2 import iso as isomod, stage, vif
import modelo

COCHES = [1, 52, 69, 103, 35, 120, 86, 136, 18]        # utilitario, berlinas, autocaravana, autobus...
# modelo del juego (car+0x11, 30..43 = trafico) de cada uno: emparejados por las medidas de la malla con las cajas de
# choque de cada modelo (descriptor de 520950); 69 y 136 podrian ir al reves (33 / 37), pero los dos son de clase 3
MODELOS = [30, 32, 37, 35, 38, 36, 34, 33, 31]

class Mem:
    def __init__(self, d): self.m = d

def caja(b, r):
    P = [tr[0x68] for pk in b.mallas(r) for tr in vif.tiras(pk)]
    if not P: return None
    P = np.vstack(P).astype(float); P = P[np.isfinite(P).all(1)]
    return (P.min(0), P.max(0)) if len(P) else None

def tabla_modelos(d):
    """registros de 4866C0: 30 coches del jugador (clase 5, radio 2..3) y 14 de trafico (radio 1,5..15, el 31 > 5)"""
    u = np.frombuffer(d[:len(d) // 4 * 4], '<u4'); f = u.view('<f4')
    R = 0x44 // 4
    for i in range(0, len(u) - 44 * R):
        if u[i + 5] != 5 or not 2.0 <= f[i + 6] <= 3.0: continue
        ok = True
        for m in range(44):
            b = i + m * R
            if m < 30: ok = u[b + 5] == 5 and 2.0 <= f[b + 6] <= 3.0
            else: ok = u[b + 5] <= 4 and u[b + 3] <= 3 and 1.5 <= f[b + 6] <= 15.0
            if not ok: break
        if ok and f[i + 31 * R + 6] > 5.0: return i * 4
    return None

def tabla_cajas(d):
    """descriptores de choque de 520950: u8 n + n cajas de 7 floats, 0x3C bytes por modelo, 44 modelos"""
    def bien(o):
        n = d[o]
        if n not in (1, 2) or d[o + 1:o + 4] != b'\0\0\0': return False
        for k in range(n):
            x0, y0, z0, x1, y1, z1, r = struct.unpack('<7f', d[o + 4 + k * 28:o + 32 + k * 28])
            if not (0 < x0 < 4 and -4 < x1 < 0 and 1 < abs(z0 - z1) < 30 and 0.5 < r <= 40): return False
        return True
    for o in range(0, len(d) - 44 * 0x3c, 4):
        if all(bien(o + m * 0x3c) for m in range(44)): return o
    return None

def tabla_clases(d):
    """5B3E08: 7 filas de 5 floats; las clases 0..5 con la misma referencia y aceleracion y frenada creciente"""
    with np.errstate(invalid='ignore', over='ignore'):
        f = np.nan_to_num(np.frombuffer(d[:len(d) // 4 * 4], '<f4').astype(np.float64), nan=-1, posinf=-1, neginf=-1)
    for i in range(len(f) - 35):
        t = f[i:i + 35].reshape(7, 5)
        if not (100 <= t[0, 0] <= 1000 and np.all(t[:6, 0] == t[0, 0]) and t[6, 0] > t[0, 0]): continue
        if not (np.all(t[:6, 1] == t[0, 1]) and np.all(t[:6, 3] == t[0, 3])): continue
        if not (np.all(t[:, 1:] > 0) and np.all(t[:, 1:] < 2) and np.all(np.diff(t[:6, 2]) >= 0)): continue
        return i * 4
    return None

def tablas(iso, dest, cajas_malla=None):
    nombre = next(k for k in iso.fich if k.startswith('SLES') or k.startswith('SLUS') or k.startswith('SLPM'))
    o, s = iso.fich[nombre]; d = bytes(iso.m[o:o + s])
    tm, tc, tk = tabla_modelos(d), tabla_cajas(d), tabla_clases(d)
    if tm is None or tc is None or tk is None: sys.exit('no encuentro las tablas del trafico en ' + nombre)
    b = bytearray(b'TRC1' + struct.pack('<I', len(MODELOS)))
    for n, m in enumerate(MODELOS):
        r = tm + m * 0x44
        se, k320, clase = struct.unpack('<3i', d[r + 0xc:r + 0x18]); radio, = struct.unpack('<f', d[r + 0x18:r + 0x1c])
        caja = struct.unpack('<7f', d[tc + m * 0x3c + 4:tc + m * 0x3c + 32])
        b += struct.pack('<4i', m, clase, se, k320) + struct.pack('<8f', radio, *caja)
        extra = ''
        if cajas_malla and cajas_malla[n] is not None:
            lo, hi = cajas_malla[n]
            extra = ' (malla: ancho %.2f, alto %.2f, z %.2f..%.2f)' % (hi[0], hi[1], lo[2], hi[2])
        print('trafico %d: modelo %d, clase %d, radio %.2f, caja ancho %.2f alto %.2f z %.2f..%.2f%s'
              % (n, m, clase, radio, caja[0], caja[1], caja[5], caja[2], extra))
    b += b'ACL1' + struct.pack('<I', 7) + d[tk:tk + 35 * 4]
    open(os.path.join(dest, 'TRAFC.DAT'), 'wb').write(b)
    print('TRAFC.DAT: modelos en 0x%x, cajas en 0x%x, tabla por clase en 0x%x' % (tm, tc, tk))

def main():
    iso = isomod.Iso(sys.argv[1]); dest = sys.argv[2]
    if '--solo-tablas' in sys.argv[3:]:
        tablas(iso, dest); return
    h, o, s = iso.paquete('CARS.PS2')[0]; d = zlib.decompress(iso.m[o:o + s])
    w = struct.unpack('<4I', d[:16]); b = stage.Bloque(Mem(d), 0, w)
    medidas = []
    for n, r in enumerate(COCHES):
        cuerpo = r + 8
        cb = caja(b, cuerpo); lo, hi = cb; medidas.append(caja(b, r))
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
    tablas(iso, dest, medidas)

if __name__ == '__main__':
    main()
