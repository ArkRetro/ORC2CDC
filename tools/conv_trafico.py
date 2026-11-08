#!/usr/bin/env python3
"""conv_trafico.py tu.iso carpeta [--solo-tablas]: los coches del trafico (CARS.PS2, entrada 0: unos 17 registros por
coche: carroceria, luces, sombra, dos ruedas en su origen y, 8 registros despues, la carroceria ligera) a TRAFn.DAT
(formato ORD6, modelo.py): la carroceria ligera con las ruedas en sus cuatro sitios y solo las texturas que usa.
Materiales del original (tabla 4 y 5 del registro, como conv_coche.py): las mallas sin textura con su difuso por la luz
(+ emision); los cristales (sin UV, capa 1 = mapa de entorno esferico del bloque) con UV de entorno fijas por la normal
del modelo vista desde detras (como las calcularia la demo con la camara alineada con el coche; el trafico no las
recalcula cada cuadro), base = especular y alfa del difuso; la capa 2 de la carroceria (reflejo) no se dibuja.
Colores: las listas de colores de los modelos 30..43 (PC 683980; en SLES-53998 la tabla de 44 listas de 0x36bc18,
localizada por estructura): {clave, color 1, color 2}, colores 0..2 = la textura original o otra del mismo bloque.
Van en el sitio de la ruta del modelo como 'TCX1', u32 colores del modelo (los que recorre el original: 2 en el 38,
1 en el 39 y el 41, 3 en los demas; PC 478B90/pick 46CE30), u32 pares, pares {u32 textura, color 1, color 2}
(indices de textura del fichero).
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
# modelo del juego (car+0x11, 30..43 = trafico) de cada uno: las texturas de su carroceria son las claves de la lista
# de colores de ese modelo (tabla_colores; antes se emparejaban por medidas y 69 / 136 iban al reves)
MODELOS = [30, 32, 33, 35, 38, 36, 34, 37, 31]

def tabla_colores(A):
    """tabla de 44 listas de colores (modelos 0..43): las de 30..43 con claves del recurso del trafico"""
    import conv_coche
    E = A.elf; R = conv_coche.recursos(A); res = None
    for k in range(400):
        try:
            if E.cadena(E.u32(R + 4 * k)).lower().endswith('obj_othcar_pmt.sz'): res = k; break
        except Exception: break
    def lista(p):
        r = []
        try:
            while E.u32(p) and len(r) < 8: r.append(E.u32(E.u32(p), 3)); p += 4
        except Exception: return None
        return r
    for va, off, fs, ms in E.segmentos:
        w = np.frombuffer(E.d[off:off + fs // 4 * 4], '<u4').astype(np.int64)
        dentro = ((w >= va) & (w < va + fs)).astype(int)
        for i in np.nonzero(np.convolve(dentro, np.ones(44, int), 'valid') == 44)[0]:
            ok = True
            for m in range(30, 44):
                L = lista(int(w[i + m]))
                if not L or any((x[0] >> 16) != res for x in L): ok = False; break
            if ok: return va + 4 * int(i), res
    sys.exit('no encuentro la tabla de colores del trafico')

NCOLOR = {38: 2, 39: 1, 41: 1}                         # colores que recorre el original (el resto 3; PC 478B90)

def materiales(mod):
    """material del original en cada malla (ver el docstring)"""
    import conv_coche as cc
    for x, ex in zip(mod.mallas, mod.extra):
        x.append(None)
        b = x[6]; r, i, k4, fl = ex['origen']; reg = b.registros()[r]
        if not 0 <= k4 < reg[4][0]: continue
        t4 = struct.unpack('<9i', b.m[b.A + reg[4][1] + 36 * k4:b.A + reg[4][1] + 36 * k4 + 36])
        col = np.frombuffer(b.m[b.A + reg[5][1] + 64 * t4[0]:b.A + reg[5][1] + 64 * t4[0] + 64], '<f4') * 2 \
            if 0 <= t4[0] < reg[5][0] else np.full(16, 255.0)
        dif, esp, emi = col[0:4], col[8:11], col[12:15]; alfa = int(min(255, dif[3]))
        N = ex['N'].astype(float); luz = cc._luz(N)
        if x[1] < 0:
            c = np.clip(dif[None, :3] * luz[:, None] + emi[None, :], 0, 255).astype(np.uint32)
            x[-1] = ('color', (np.uint32(alfa) << 24) | (c[:, 0] << 16) | (c[:, 1] << 8) | c[:, 2], alfa < 250)
        elif not ex['uv']:
            Nn = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
            x[-1] = ('entorno', np.stack([0.5 + 0.5 * Nn[:, 0], 0.5 - 0.5 * Nn[:, 1]], 1), alfa, esp)

NINGUNO = 0xFFFFFFFF

def llamadas(E, tm, m):
    """piezas del coche de trafico m y su matriz como las dibuja el original cerca (4AE0F0 -> model con LOD 0:
    4ADBC0, race_ghost_car.cpp), parado y sin luces: registro de 4866C0 (PC 650500, 0x44 bytes por modelo): +0 lista
    de 5 LOD, cada uno {descriptor, segundo descriptor (remolque) o 0}. Descriptor: +0 carroceria (con el color),
    +4 sombra (plano bajo el coche; en el LOD 0 subida 0,1 m: 40A290), +8 luz de noche (car+4 & 0x100), +C/+10/+14
    pilotos (normales / car+4 & 0x300 / freno car+38 > 0: los normales, sin luces), +18/+1C intermitentes (car+C5C,
    apagados), +9C lista de piezas con el color {id, x, y, z}; ruedas (4AD530, giros 0): +38 en +3C, +48 en +4C girada
    pi en y (la del otro lado), +58 en +5C, +78 en +7C (si hay), +68 en +6C girada pi, +88 en +8C girada pi (si hay).
    La matriz local car+F0 es la identidad parado. Devuelve [(id, matriz 4x4, nombre)]"""
    import conv_coche as cc
    L = E.u32(E.u32(tm + 0x44 * m)); d, d2 = E.u32(L, 2)
    if d2: raise SystemExit('modelo %d: remolque (segundo descriptor) sin portar' % m)
    u = lambda o: E.u32(d + o); g3 = lambda o: np.array(E.f32(d + o, 3), float)
    s = cc.Pila(); res = []
    def dib(ident, nombre):
        if ident != NINGUNO: res.append((ident, s.m[-1].copy(), nombre))
    s.push()                                                    # car+F0
    dib(u(0), 'carroceria')
    dib(u(0xc), 'pilotos')
    p = u(0x9c)
    if p and E.u32(p) != NINGUNO:
        e = 0
        while True:
            s.push(); s.translate(np.array(E.f32(p + e + 4, 3), float)); dib(E.u32(p + e), 'lista'); s.pop()
            if E.u32(p + e + 0x10) == NINGUNO: break
            e += 0x10
    s.pop()
    if u(4) != NINGUNO: s.push(); s.translate((0.0, 0.1, 0.0)); dib(u(4), 'sombra'); s.pop()
    def rueda(o, otro_lado):
        if u(o) == NINGUNO: return
        s.push(); s.translate(g3(o + 4))
        if otro_lado: s.rot_y(np.pi)
        dib(u(o), 'rueda'); s.pop()
    rueda(0x38, False); rueda(0x48, True); rueda(0x58, False); rueda(0x78, False); rueda(0x68, True); rueda(0x88, True)
    return res

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
    from ps2.assets import Assets
    A = Assets(sys.argv[1]); E = A.elf; T, rec = tabla_colores(A); claves = {}
    for m in MODELOS:
        p = E.u32(T + 4 * m); claves[m] = []
        while E.u32(p): claves[m].append(E.u32(E.u32(p), 3)); p += 4
    print('colores del trafico: tabla en 0x%x' % T)
    nombre = next(k for k in iso.fich if k.startswith('SLES') or k.startswith('SLUS') or k.startswith('SLPM'))
    o, s = iso.fich[nombre]; tm = E.va(tabla_modelos(bytes(iso.m[o:o + s])))
    import conv_coche as cc
    medidas = []
    for n, m in enumerate(MODELOS):
        mod = modelo.Modelo(); mod.ruta = np.zeros((0, 5)); mod.extra = []; vistos = {}
        piezas = llamadas(E, tm, m)
        for ident, M, nom in piezas:
            b = cc._bloque_recurso(A, ident >> 16)
            if nom == 'carroceria': cuerpo = ident & 0xFFFF; medidas.append(caja(b, cuerpo)); lo, hi = medidas[-1]
            if id(b) not in vistos: vistos[id(b)] = b
            cc.pieza(mod, b, ident, R=M[:3, :3], t=M[3, :3], nombre=nom)
        largo = hi[2] - lo[2]
        materiales(mod)
        import conv_coche as cc
        for x, v in zip(mod.mallas, mod.verts):
            q = x[-1]
            if q is None: continue
            if q[0] == 'color': v['c'] = q[1]; x[0] = 2 if q[2] else 0
            else:
                _, uvs, alfa, esp = q; tr = alfa < 250
                x[1] = cc._tex_copia(mod, x[1], tr); x[0] = 2 if tr else 0
                v['u'], v['v'] = uvs[:, 0], uvs[:, 1]
                e = int(min(255, esp.max())); v['c'] = ((alfa if tr else 255) << 24) | (e << 16) | (e << 8) | e
        # colores del original: las texturas alternativas, que recorta_texturas conserva con mallas de mentira (la
        # clave y sus dos colores; recorta les pone el indice nuevo)
        m = MODELOS[n]; pares = []
        for k in claves.get(m, []):
            if not any(x[1] == (k[0] & 0xFFFF) for x in mod.mallas): continue
            pares.append([[9, t & 0xFFFF] + [None] * 6 for t in k[:3]])
        for f in pares: mod.mallas.extend(f)
        mod.recorta_texturas()
        if mod.vq: mod.comprime(); mod._comprimido = True        # (las alternativas tambien en VQ)
        falsas = {id(q) for f in pares for q in f}
        mod.mallas = [x for x in mod.mallas if id(x) not in falsas]
        tcx = [b'TCX1', struct.pack('<2I', NCOLOR.get(m, 3), len(pares))]
        for f in pares: tcx.append(struct.pack('<3I', *[q[1] for q in f]))
        mod.ruta = np.frombuffer(b''.join(tcx), '<f4').reshape(-1, 1)
        mod.mallas = [x[:8] for x in mod.mallas]
        ini = mod.escribe(os.path.join(dest, 'TRAF%d.DAT' % n))
        print('trafico %d (modelo %d, registro %d): %.1f m, %d tiras, %d vertices, %d triangulos, %d texturas (%d KB), '
              'piezas %s' % (n, m, cuerpo, largo, len(mod.mallas), sum(x[5] for x in mod.mallas),
                             sum(x[5] - 2 for x in mod.mallas), len(mod.texs), len(mod.blob) >> 10,
                             ' '.join('%s %d:%d' % (q[2], q[0] >> 16, q[0] & 0xFFFF) for q in piezas)))
    tablas(iso, dest, medidas)

if __name__ == '__main__':
    main()
