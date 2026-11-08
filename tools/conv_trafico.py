#!/usr/bin/env python3
"""conv_trafico.py tu.iso carpeta [--solo-tablas]: los coches del trafico (CARS.PS2, \\CARS\\obj_othcar_pmt.sz y la
sombra de \\COMMON\\obj_vshadow_pmt.sz) a TRAFn.DAT (formato de modelo.py) montados como el original los dibuja de
cerca (llamadas: 4AE0F0 / 4ADBC0 del PC con el LOD 0 del registro de 4866C0): carroceria, sombra (subida 0,1 m) y las
cuatro ruedas de su descriptor (las del otro lado giradas pi en y), parado y sin luces; solo las texturas que usa.
LOD del original: el registro de 4866C0 trae 5 descriptores (LOD 0..4); 4ADBC0 / 4ADD80 / 4ADF00 dibujan con el del
LOD del coche (car+0x328): carroceria, pilotos y sombra siempre; la lista +9C y las ruedas solo en los LOD 0 y 1; la
sombra subida 0,1 m solo en el LOD 0. Cada malla lleva en el campo del nodo la mascara de los LOD que la dibujan
(bit k = LOD k: las que se repiten de un LOD a otro van una sola vez); la demo
elige el LOD por la distancia como 46DF50 (PS2 0x1dd728: umbrales de la z de la camara en demo/main.c, traf_lod).
Antes se usaba la carroceria del LOD 2 (que ya trae las ruedas) con otras cuatro ruedas puestas a ojo (inventado:
ruedas dobles).
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

def llamadas(E, tm, m, lod=0):
    """piezas del coche de trafico m y su matriz como las dibuja el original con el LOD lod (4AE0F0 -> model: 4ADBC0
    el LOD 0, 4ADD80 el 1, 4ADF00 los 2..4; race_ghost_car.cpp), parado y sin luces: registro de 4866C0 (PC 650500, 0x44 bytes por modelo): +0 lista
    de 5 LOD, cada uno {descriptor, segundo descriptor (remolque) o 0}. Descriptor: +0 carroceria (con el color),
    +4 sombra (plano bajo el coche; en el LOD 0 subida 0,1 m: 40A290), +8 luz de noche (car+4 & 0x100), +C/+10/+14
    pilotos (normales / car+4 & 0x300 / freno car+38 > 0: los normales, sin luces), +18/+1C intermitentes (car+C5C,
    apagados), +9C lista de piezas con el color {id, x, y, z}; ruedas (4AD530, giros 0): +38 en +3C, +48 en +4C girada
    pi en y (la del otro lado), +58 en +5C, +78 en +7C (si hay), +68 en +6C girada pi, +88 en +8C girada pi (si hay).
    Las ruedas y la lista +9C solo con LOD 0 y 1 (near); la sombra subida solo con LOD 0. Sin descriptor en ese LOD
    (puntero 0) el original no dibuja nada. La matriz local car+F0 es la identidad parado. Devuelve [(id, matriz 4x4,
    nombre)]"""
    import conv_coche as cc
    L = E.u32(E.u32(tm + 0x44 * m) + 4 * lod)
    if not L: return []
    d, d2 = E.u32(L, 2); cerca = lod <= 1
    if d2: raise SystemExit('modelo %d: remolque (segundo descriptor) sin portar' % m)
    u = lambda o: E.u32(d + o); g3 = lambda o: np.array(E.f32(d + o, 3), float)
    s = cc.Pila(); res = []
    def dib(ident, nombre):
        if ident != NINGUNO: res.append((ident, s.m[-1].copy(), nombre))
    s.push()                                                    # car+F0
    dib(u(0), 'carroceria')
    dib(u(0xc), 'pilotos')
    p = u(0x9c) if cerca else 0
    if p and E.u32(p) != NINGUNO:
        e = 0
        while True:
            s.push(); s.translate(np.array(E.f32(p + e + 4, 3), float)); dib(E.u32(p + e), 'lista'); s.pop()
            if E.u32(p + e + 0x10) == NINGUNO: break
            e += 0x10
    s.pop()
    if u(4) != NINGUNO: s.push(); s.translate((0.0, 0.1 if lod == 0 else 0.0, 0.0)); dib(u(4), 'sombra'); s.pop()
    def rueda(o, otro_lado):
        if u(o) == NINGUNO: return
        s.push(); s.translate(g3(o + 4))
        if otro_lado: s.rot_y(np.pi)
        dib(u(o), 'rueda'); s.pop()
    if cerca:
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

# ---------------------------------------------------------------- aparicion del trafico por etapa (bloque 'TAP1')
NFIJO = 24                                          # puntos fijos por etapa como mucho (en el curso hay hasta 18)

def _lui_addiu(u, i, n):
    """direcciones (lui rX, hi + addiu rY, rX, lo en las n instrucciones siguientes) desde la instruccion i"""
    r = []
    for j in range(i, i + n):
        w = int(u[j])
        if w >> 26 != 0x0f: continue
        rx = (w >> 16) & 31
        for k in range(j + 1, j + 7):
            x = int(u[k])
            if x >> 26 == 0x0f and (x >> 16) & 31 == rx: break           # otro lui al mismo registro
            if x >> 26 == 0x09 and (x >> 21) & 31 == rx:
                lo = x & 0xffff; r.append(((w & 0xffff) << 16) + (lo - 0x10000 if lo & 0x8000 else lo)); break
    return r

def _jal(w): return (w & 0x3ffffff) << 2 if w >> 26 == 3 else None

def aparicion(iso):
    """'TAP1': las tablas de aparicion del trafico del ejecutable, localizadas por el codigo que las usa:
     - AREA (estado 0xF; PC race_area.cpp, PS2 0x173bf4): 'jal 4EFB50; lw a0, 0x80(rs); jal 4EFAF0; lw a0, 0x84(rs)':
       el descriptor de tramo lleva en +0x80 la tabla de aparicion (PC +0x68; 4EFB50 la copia a 84BD08: hasta 8
       registros de 12 bytes acabados en 0xFFFF) y en +0x84 la de velocidad (PC +0x6C; 4EFAF0 a 84BF60, 5 registros de
       0x46). Detras, 'lw a0, 0x64(v1); beqz; nop; jal 4EFD20; move a1, zero': 4EFD20 con un registro del arbol (+0x64,
       0 en todo csc_data_2) y, en su rama de la union, la direccion del registro de la union por defecto (6A5DC4 del PC:
       el primer lui/addiu de 4EFD20).
     - 46CC10 (PS2 0x1daa48): 'slti v0, a1, 6' ... 'lw v1, 0x324(v1)': el peso de cada clase en la cuenta por carril
       (5B3DE4 del PC, 7 u16; el lui/addiu de la tabla esta unas instrucciones antes).
     - 46CE30 (PS2 0x1db2a0): 'addiu v0, zero, 7; lui v1; div a0, v0; addiu v1': la lista de 7 modelos (64E198 del PC).
     - 4EF880 (PS2 0x1f20f8): 'lui v0; sll a0, a0, 2; addiu v0, v0; addu; jr ra; lw v0, 0(a0)': la tabla de punteros
       por etapa (6A6CE0 del PC) a la lista de puntos fijos de 478B90 (en PS2 0x18 bytes por punto: u32 etapa, u32 tipo
       de lugar, u16 posicion, i8 carril, u32 modelo en +0xC, i8 color en +0x10, usado en +0x14; acaba en etapa 0x42).
    Salida: 'TAP1', u32 n etapas (orden de clave del arbol csc_data_2, como CURSO.DAT), registro de la union por
    defecto (12 bytes), 7 u16 pesos por clase + u16 0, 7 u8 modelos + u8 0, y por etapa: tabla de aparicion (0x60
    bytes tal cual), tabla de velocidad (0x15E bytes + 2 de relleno), u32 n puntos fijos y NFIJO x {u16 posicion, i8
    carril, u8 modelo, i8 color, u8 tipo de lugar, u16 0}."""
    from ps2.assets import Assets
    import conv_union
    A = Assets(iso.ruta if hasattr(iso, 'ruta') else sys.argv[1]); E = A.elf
    nombre = next(k for k in iso.fich if k.startswith('SLES') or k.startswith('SLUS') or k.startswith('SLPM'))
    o, s = iso.fich[nombre]; d = bytes(iso.m[o:o + s]); u = np.frombuffer(d[:len(d) // 4 * 4], '<u4')
    va = lambda i: E.va(int(i) * 4)
    # 1) AREA: desplazamientos del descriptor y 4EFD20
    o80 = o84 = defecto = None
    for i in np.where((u[1:-6] & 0xfc1fffff) == 0x8c040080)[0] + 1:
        rs = (int(u[i]) >> 21) & 31
        if _jal(int(u[i - 1])) is None or _jal(int(u[i + 1])) is None or int(u[i + 2]) != (0x8c040084 | rs << 21): continue
        for j in range(i + 3, i + 12):
            if (int(u[j]) & 0xfc1fffff) == 0x8c040064 and _jal(int(u[j + 3])) and int(u[j + 4]) == 0x0000282d:
                f = E.off(_jal(int(u[j + 3]))) // 4
                c = _lui_addiu(u, f, 6)
                if c: defecto = E.lee(c[0], 12)
        o80, o84 = 0x80, 0x84; break
    if o80 is None or defecto is None: sys.exit('no encuentro las tablas de aparicion (codigo de AREA / 4EFD20)')
    # 2) pesos por clase de 46CC10
    pesos = None
    for i in np.where(u == 0x8c630324)[0]:
        if not any(int(u[k]) == 0x28a20006 for k in range(i - 4, i)): continue
        for a in _lui_addiu(u, i - 40, 40):
            try: t = struct.unpack('<7H', E.lee(a, 14))
            except ValueError: continue
            if all(1 <= x <= 9 for x in t) and max(t) > 1: pesos = t; break
        if pesos: break
    if pesos is None: sys.exit('no encuentro los pesos por clase de 46CC10')
    # 3) lista de modelos de 46CE30
    lista = None
    for i in np.where(u[:-3] == 0x24020007)[0]:
        if int(u[i + 1]) >> 16 != 0x3c03 or int(u[i + 2]) != 0x0082001a or int(u[i + 3]) >> 16 != 0x2463: continue
        lo = int(u[i + 3]) & 0xffff; a = ((int(u[i + 1]) & 0xffff) << 16) + (lo - 0x10000 if lo & 0x8000 else lo)
        t = E.u32(a, 7)
        if all(30 <= x <= 43 for x in t): lista = t; break
    if lista is None: sys.exit('no encuentro la lista de modelos de 46CE30')
    # 4) puntos fijos (4EF880)
    fijos = None
    for i in np.where((u[:-5] >> 16 == 0x3c02) & (u[1:-4] == 0x00042080) & (u[2:-3] >> 16 == 0x2442) &
                      (u[3:-2] == 0x00822021) & (u[4:-1] == 0x03e00008) & (u[5:] == 0x8c820000))[0]:
        lo = int(u[i + 2]) & 0xffff; fijos = ((int(u[i]) & 0xffff) << 16) + (lo - 0x10000 if lo & 0x8000 else lo)
        if E.off(fijos) is not None and E.off(E.u32(fijos)) is not None and E.u32(E.u32(fijos)) == 0: break
        fijos = None
    if fijos is None: sys.exit('no encuentro la tabla de puntos fijos de 4EF880')
    base, nprim, _ = conv_union.tablas(A)
    regs = conv_union.arbol(A, 0)
    b = bytearray(b'TAP1' + struct.pack('<I', len(regs)) + defecto + struct.pack('<8H', *pesos, 0) +
                  bytes(lista) + b'\0')
    for clave, ti, ui, hi, hd in regs:
        de = E.u32(base + 4 * ti); ap = E.lee(E.u32(de + o80), 0x60); ve = E.lee(E.u32(de + o84), 0x15e)
        r = [struct.unpack_from('<H', ap, 12 * k)[0] for k in range(8)]
        assert 0xffff in r and all(r[k] < r[k + 1] for k in range(r.index(0xffff))), 'tabla de aparicion rara'
        e = E.u32(fijos + 4 * ti); fl = []
        while True:
            st, tipo, pos, carril = struct.unpack('<IIHb', E.lee(e, 11)); mod, = struct.unpack('<I', E.lee(e + 12, 4))
            col, = struct.unpack('<b', E.lee(e + 16, 1))
            if st == 0x42 or st > ti: break
            if st == ti: fl.append((pos, carril, mod, col, tipo))
            e += 0x18
        assert len(fl) <= NFIJO
        b += ap + ve + b'\0\0' + struct.pack('<I', len(fl))
        for k in range(NFIJO):
            pos, carril, mod, col, tipo = fl[k] if k < len(fl) else (0, 0, 0, 0, 0)
            b += struct.pack('<HbBbBH', pos, carril, mod, col, tipo, 0)
        print('aparicion etapa %2d (descriptor %2d): %s; puntos fijos %d' %
              (clave, ti, ' '.join('%d:%d/%d/%02x' % (r[k], ap[12 * k + 4], ap[12 * k + 5], ap[12 * k + 11])
                                   for k in range(r.index(0xffff))), len(fl)))
    print('aparicion: union por defecto %s, pesos por clase %s, modelos %s, puntos fijos en 0x%x'
          % (list(defecto), list(pesos), list(lista), fijos))
    return bytes(b)

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
    b += aparicion(iso)
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
        for lod in range(5):
            piezas = llamadas(E, tm, m, lod); m0 = len(mod.mallas)
            for ident, M, nom in piezas:
                b = cc._bloque_recurso(A, ident >> 16)
                if nom == 'carroceria' and lod == 0: cuerpo = ident & 0xFFFF; medidas.append(caja(b, cuerpo)); lo, hi = medidas[-1]
                if id(b) not in vistos: vistos[id(b)] = b
                cc.pieza(mod, b, ident, R=M[:3, :3], t=M[3, :3], nombre=nom)
            for x in mod.mallas[m0:]: x[7] = lod               # el LOD en el campo del nodo
            if lod == 0: piezas0 = piezas
        piezas = piezas0
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
        # las mallas que repite otro LOD (el 1 lleva las del 0 salvo la sombra subida) van una vez, con la mascara de
        # los LOD que las dibujan en el campo del nodo (bit k = LOD k)
        assert len(mod.mallas) == len(mod.verts)
        clave = {}; mallas = []; verts = []; nv = 0
        for x, v in zip(mod.mallas, mod.verts):
            k = (x[0], x[1], v.tobytes())
            if k in clave: clave[k][7] |= 1 << x[7]; continue
            y = list(x); y[7] = 1 << x[7]; y[4] = nv; nv += len(v)
            clave[k] = y; mallas.append(y); verts.append(v)
        mod.mallas, mod.verts, mod.nv = mallas, verts, nv
        ini = mod.escribe(os.path.join(dest, 'TRAF%d.DAT' % n))
        print('trafico %d (modelo %d, registro %d): %.1f m, %d tiras, %d texturas (%d KB), piezas %s; vertices por LOD %s'
              % (n, m, cuerpo, largo, len(mod.mallas), len(mod.texs), len(mod.blob) >> 10,
                 ' '.join('%s %d:%d' % (q[2], q[0] >> 16, q[0] & 0xFFFF) for q in piezas),
                 ' '.join(str(sum(x[5] for x in mod.mallas if x[7] >> k & 1)) for k in range(5))))
    tablas(iso, dest, medidas)

if __name__ == '__main__':
    main()
