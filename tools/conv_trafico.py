#!/usr/bin/env python3
"""conv_trafico.py tu.iso carpeta [--solo-tablas | --camiones]: los coches del trafico (CARS.PS2, \\CARS\\obj_othcar_pmt.sz y la
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
  'TAP1' (ver aparicion), y detras:
  'CJM1' + u32 30 + por modelo del jugador 0..29 el mismo registro que TRC1 (clase 5, radio y caja de 520950).
  'SEC1' + 16 u32: efectos del golpe entre coches de 4F65D0 / 4F67A0 ([tipo * 4 + familia], ver tabla_se_golpe).
TAPD.DAT: 'TAPD', u32 n segmentos de STAGE.PS2 (SEGnn.DAT) y por segmento el registro de etapa de 'TAP1' (644 bytes: aparicion,
velocidad, puntos fijos), para los arboles que no son el del modo OutRun (la demo copia los de su arbol al leerlo). Las
listas de puntos fijos de las etapas al reves estan en la BSS: se leen tras ejecutar la rutina que las rellena (ver
aparicion, paso 5; tools/ps2/mips.py).
Con --solo-tablas solo se escriben TRAFC.DAT y TAPD.DAT. Con --rivales=m,m,.. (modelos 0..29) se escriben RCmm.DAT: los
coches de carreras de las misiones de COAST 2 COAST (el rival), que el original abre como coches del trafico (suceso 0x55
de 46E830, 47BC90) y dibuja igual que estos con el registro de 4866C0 de su modelo (obj_rc_<coche>, 5 LOD), en el
formato de TRAFn.DAT; sin las tablas.
Con --camiones solo los camiones con remolque de OUTRUN2SP (camiones(): TRAFK0.DAT el 39 y el 40, que es su cabeza
sola; TRAFK1.DAT el 41; TRAFK2.DAT el 43): la cabeza como los coches y el remolque (el segundo descriptor de cada LOD
de 4866C0, llamadas_remolque) en su marco, que la demo pone con car+C90 (470360); mismas mallas y LOD que el original
y las texturas en VQ como el resto; detras de TAN1 los bloques REM1 (bloque_rem: enganche, ruedas y segunda caja de
choque) y TRK1 (los registros de TRC1 de los modelos del fichero). Sale de tu disco: no se distribuye."""
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
# ruedas del descriptor como las pone 4AD530: {posicion de la pieza, del otro lado (girada pi en y), delantera}; la
# pieza animada k + 1 (campo del nodo << 8) y su fila del bloque TAN1
RUEDAS = [(0x38, False, True), (0x48, True, True), (0x58, False, False), (0x78, False, False), (0x68, True, False),
          (0x88, True, False)]

def llamadas(E, tm, m, lod=0, remolque=False):
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
    if d2 and not remolque: raise SystemExit('modelo %d: remolque (segundo descriptor): con --camiones' % m)
    u = lambda o: E.u32(d + o); g3 = lambda o: np.array(E.f32(d + o, 3), float)
    s = cc.Pila(); res = []
    def dib(ident, nombre):
        if ident != NINGUNO: res.append((ident, s.m[-1].copy(), nombre))
    s.push()                                                    # car+F0
    dib(u(0), 'carroceria')
    dib(u(0xc), 'pilotos')
    dib(u(0x14), 'freno')                                       # 4AD4D0: en lugar de +C con car+38 > 0 (grupo G_FRENO)
    p = u(0x9c) if cerca else 0
    if p and E.u32(p) != NINGUNO:
        e = 0
        while True:
            s.push(); s.translate(np.array(E.f32(p + e + 4, 3), float)); dib(E.u32(p + e), 'lista'); s.pop()
            if E.u32(p + e + 0x10) == NINGUNO: break
            e += 0x10
    s.pop()
    if u(4) != NINGUNO: s.push(); s.translate((0.0, 0.1 if lod == 0 else 0.0, 0.0)); dib(u(4), 'sombra'); s.pop()
    def rueda(k):
        o, otro_lado, _ = RUEDAS[k]
        if u(o) == NINGUNO: return
        s.push(); s.translate(g3(o + 4))
        if otro_lado: s.rot_y(np.pi)
        dib(u(o), 'rueda%d' % (k + 1)); s.pop()               # 4AD530: + el giro en x (car+40 / +44, animado)
    if cerca:
        for k in range(6): rueda(k)
    if d2: res += llamadas_remolque(E, d2, lod)
    return res

def llamadas_remolque(E, d2, lod):
    """el remolque (segundo descriptor del LOD, race_ghost_car.cpp second(): 4ADBC0 / 4ADD80 / 4ADF00 tras la cabeza) en
    SU marco: el original lo dibuja con car+C90 (la matriz de 470360) por encima de la del coche. Segundo descriptor:
    +0 carroceria (con el color), +4 enganche (f32 x 3: la traslacion de car+C90), +10 sombra (subida 0,1 m en el LOD
    0: 40A290), +14 pilotos (solo con car+4 & 0x300: noche o tunel; la demo no los enciende), +18 freno (car+38 > 0:
    G_FRENO), +1C / +20 intermitentes (car+C5C, apagados), +68 lista de piezas sin color {id, x, y, z} (cerca); ruedas
    (4AD8F0, cerca: LOD 0 y 1): +24 en +28, +34 en +38 girada pi en y, +44 en +48 y +54 en +58 girada pi (si hay); las
    gira car+40 (la de las delanteras de la cabeza). Nombres 'rem_*' (pieza 7 de la demo) y 'remrueda1..4' (8..11)"""
    import conv_coche as cc
    u = lambda o: E.u32(d2 + o); g3 = lambda o: np.array(E.f32(d2 + o, 3), float)
    s = cc.Pila(); res = []; cerca = lod <= 1
    def dib(ident, nombre):
        if ident != NINGUNO: res.append((ident, s.m[-1].copy(), nombre))
    dib(u(0), 'rem_carroceria')
    dib(u(0x18), 'rem_freno')
    p = u(0x68) if cerca else 0
    if p and E.u32(p) != NINGUNO:
        e = 0
        while True:
            s.push(); s.translate(np.array(E.f32(p + e + 4, 3), float)); dib(E.u32(p + e), 'rem_lista'); s.pop()
            if E.u32(p + e + 0x10) == NINGUNO: break
            e += 0x10
    if u(0x10) != NINGUNO:
        s.push(); s.translate((0.0, 0.1 if lod == 0 else 0.0, 0.0)); dib(u(0x10), 'rem_sombra'); s.pop()
    if cerca:
        for k, (o, otro) in enumerate(RUEDAS_REM):
            if u(o) == NINGUNO: continue
            s.push(); s.translate(g3(o + 4))
            if otro: s.rot_y(np.pi)
            dib(u(o), 'remrueda%d' % (k + 1)); s.pop()
    return res

# ruedas del remolque como las pone 4AD8F0: {posicion de la pieza, del otro lado (girada pi en y)}; piezas 8..11
RUEDAS_REM = [(0x24, False), (0x34, True), (0x44, False), (0x54, True)]
P_REM = 7                                                       # pieza de la demo: lo que no gira del remolque

def bloque_rem(E, tm, m, tc, d):
    """REM1 (detras de TAN1): 'REM1', f32 enganche x, y, z (+4 del segundo descriptor del LOD 0: 470360 traslada
    car+C90 ahi), u32 ruedas (4), 4 x {f32 x, y, z, del otro lado, 0} (4AD8F0; x = y = z = 0 si no estan) y f32 x 7 la
    segunda caja de choque (la del remolque, en el marco de car+C90: 4F9000 con ia / ib = 1)"""
    d2 = E.u32(E.u32(E.u32(tm + 0x44 * m)) + 4)
    b = [b'REM1', struct.pack('<3fI', *E.f32(d2 + 4, 3), len(RUEDAS_REM))]
    for o, otro in RUEDAS_REM:
        p = E.f32(d2 + o + 4, 3) if E.u32(d2 + o) != NINGUNO else (0.0, 0.0, 0.0)
        b.append(struct.pack('<5f', *p, 1.0 if otro else 0.0, 0.0))
    assert d[tc + m * 0x3c] == 2, 'modelo %d con remolque sin segunda caja' % m
    b.append(d[tc + m * 0x3c + 32:tc + m * 0x3c + 60])
    return b''.join(b)

def bloque_tan(E, tm, m):
    """TAN1 (detras de los pares de TCX1): 'TAN1', u32 ruedas (6), f32 radio (+08 del registro de 4866C0: el control del
    trafico 47E780 suma a car+40 y +44 +1C4 / radio x 10430,38 cada cuadro), 6 x {f32 x, y, z, del otro lado, delantera} (las del descriptor del
    LOD 0; x = y = z = 0 y sin pieza si no estan). La demo gira cada rueda en x (4AD530) con su matriz"""
    d = E.u32(E.u32(E.u32(tm + 0x44 * m)))
    b = [b'TAN1', struct.pack('<If', len(RUEDAS), E.f32(tm + 0x44 * m + 8))]
    for o, otro, dl in RUEDAS:
        p = E.f32(d + o + 4, 3) if E.u32(d + o) != NINGUNO else (0.0, 0.0, 0.0)
        b.append(struct.pack('<5f', *p, 1.0 if otro else 0.0, 1.0 if dl else 0.0))
    return b''.join(b)

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

def tabla_se_golpe(d, tc):
    """efectos del golpe entre coches de 4F65D0 / 4F67A0 (PC 5E0A78; SLES-53998 0x3de318, la lee 0x194cf8 con
    lui/addiu): 4 tipos x 4 familias de u32 id de SE (con las banderas 0x800 de 424940). Va justo delante de los
    descriptores de 520950 (0x48 bytes antes); se comprueba que son ids de efecto (9 bits bajos 0x40..0x7f)."""
    o = tc - 0x48
    t = struct.unpack('<16I', d[o:o + 64])
    if not all(0x40 <= (x & 0x1ff) < 0x80 and (x & ~0x9ff) == 0 for x in t):
        sys.exit('no encuentro la tabla de efectos del golpe entre coches')
    return t

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
TAPD = None                                         # TAPD.DAT (aparicion): registro por descriptor de tramo

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
    # 5) las listas de puntos fijos de las etapas al reves (descriptores 30..59) estan en la BSS: las rellena al empezar
    #    el trafico (46C950 = PS2 0x1d9e30) una rutina que copia las listas de .data y despues, para cada etapa s de 0 a
    #    0x41, pone a 0 el usado (+0x14) de sus puntos y, si 30 <= s < 60, los da la vuelta: posicion = 1 + la de la
    #    etapa (+0x96 del descriptor, 1772E8) - posicion, carril = 5 - carril (PS2 0x1f2110..0x1f372c). Se localiza como
    #    la funcion que llama a 4EF880 (desde el jr ra de la anterior) y se ejecuta con ps2/mips.py; las listas se leen de esa memoria
    from ps2.mips import Mips
    f4 = E.va(int(i) * 4); mem = None
    for j in np.where(u == (0x0c000000 | (f4 >> 2)))[0]:
        k = int(j)
        while k > j - 4000 and int(u[k]) != 0x03e00008: k -= 1   # el jr ra de la funcion anterior
        k += 2                                                  # (y su hueco)
        while int(u[k]) == 0: k += 1
        try:
            m = Mips(E); m.llama(E.va(k * 4))
            if struct.unpack('<I', m.lee(E.u32(fijos + 4 * 30), 4))[0] < 0x42: mem = m; print('puntos fijos: rutina 0x%x (%d pasos)' % (E.va(k * 4), m.pasos)); break
        except (RuntimeError, ValueError):
            continue
    if mem is None: sys.exit('no encuentro la rutina que rellena los puntos fijos de las etapas al reves')
    base, nprim, _ = conv_union.tablas(A)
    regs = conv_union.arbol(A, 0)
    def registro(ti):
        """el registro de la etapa del descriptor ti (tabla de aparicion, de velocidad y puntos fijos)"""
        de = E.u32(base + 4 * ti); ap = E.lee(E.u32(de + o80), 0x60); ve = E.lee(E.u32(de + o84), 0x15e)
        r = [struct.unpack_from('<H', ap, 12 * k)[0] for k in range(8)]
        assert 0xffff in r and all(r[k] < r[k + 1] for k in range(r.index(0xffff))), 'tabla de aparicion rara'
        e = E.u32(fijos + 4 * ti); fl = []
        while True:
            st, tipo, pos, carril = struct.unpack('<IIHb', mem.lee(e, 11)); mod, = struct.unpack('<I', mem.lee(e + 12, 4))
            col, = struct.unpack('<b', mem.lee(e + 16, 1))
            if st == 0x42 or st > ti: break
            if st == ti: fl.append((pos, carril, mod, col, tipo))
            e += 0x18
        assert len(fl) <= NFIJO
        b = ap + ve + b'\0\0' + struct.pack('<I', len(fl))
        for k in range(NFIJO):
            pos, carril, mod, col, tipo = fl[k] if k < len(fl) else (0, 0, 0, 0, 0)
            b += struct.pack('<HbBbBH', pos, carril, mod, col, tipo, 0)
        txt = ' '.join('%d:%d/%d/%02x' % (r[k], ap[12 * k + 4], ap[12 * k + 5], ap[12 * k + 11])
                       for k in range(r.index(0xffff)))
        return b, txt, len(fl)
    b = bytearray(b'TAP1' + struct.pack('<I', len(regs)) + defecto + struct.pack('<8H', *pesos, 0) +
                  bytes(lista) + b'\0')
    for clave, ti, ui, hi, hd in regs:
        r, txt, nf = registro(ti); b += r
        print('aparicion etapa %2d (descriptor %2d): %s; puntos fijos %d' % (clave, ti, txt, nf))
    # TAPD.DAT: el mismo registro por segmento de STAGE.PS2 (el SEGnn.DAT de la etapa, curso[k].seg de la demo; el
    # descriptor de tramo ti lleva en +0x0c la ruta de su colision, que da su segmento: conv_union.segmento_de) para
    # los demas arboles (OUTRUN2SP, misiones): la demo copia a su tabla por etapa los de su arbol al leerlo
    global TAPD
    vacio = b'\xff' * 0x60 + b'\0' * (0x160 + 4 + NFIJO * 8)
    porseg = {}
    for ti in range(nprim):
        seg = conv_union.segmento_de(A, conv_union.cadena(E, E.u32(base + 4 * ti) + 0x0c))
        try: r, txt, nf = registro(ti)
        except ValueError as ex:                     # tablas fuera del ELF (las rellena el juego al correr): sin
            print('TAPD: descriptor %d (SEG%02d) sin tablas en el ejecutable (%s)' % (ti, seg, ex)); r = vacio
        if seg in porseg and porseg[seg] != r: sys.exit('TAPD: el segmento %d con dos tablas distintas' % seg)
        porseg.setdefault(seg, r)
    nseg = max(porseg) + 1
    TAPD = bytearray(b'TAPD' + struct.pack('<I', nseg))
    for k in range(nseg): TAPD += porseg.get(k, vacio)
    print('aparicion: union por defecto %s, pesos por clase %s, modelos %s, puntos fijos en 0x%x; TAPD %d segmentos'
          % (list(defecto), list(pesos), list(lista), fijos, len(TAPD) // 644))
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
    # coches del jugador (modelos 0..29) con el mismo registro que los de trafico, para la prueba de choque
    b += b'CJM1' + struct.pack('<I', 30)
    for m in range(30):
        r = tm + m * 0x44
        se, k320, clase = struct.unpack('<3i', d[r + 0xc:r + 0x18]); radio, = struct.unpack('<f', d[r + 0x18:r + 0x1c])
        caja = struct.unpack('<7f', d[tc + m * 0x3c + 4:tc + m * 0x3c + 32])
        b += struct.pack('<4i', m, clase, se, k320) + struct.pack('<8f', radio, *caja)
    sec = tabla_se_golpe(d, tc)
    b += b'SEC1' + struct.pack('<16I', *sec)
    open(os.path.join(dest, 'TRAFC.DAT'), 'wb').write(b)
    open(os.path.join(dest, 'TAPD.DAT'), 'wb').write(TAPD)
    print('TRAFC.DAT: modelos en 0x%x, cajas en 0x%x, tabla por clase en 0x%x' % (tm, tc, tk))

def convierte(A, E, tm, m, claves, ruta_fich, remolque=False, extra=b'', riv=False):
    """un modelo de trafico m a ruta_fich (TRAFn.DAT / TRAFKn.DAT); con remolque, el segundo descriptor (llamadas_remolque)
    y detras de TAN1 los bloques extra (REM1, TRK1); riv: un rival (RCmm.DAT, los 8 colores de TCX9). Devuelve la caja
    de la carroceria del LOD 0"""
    import conv_coche as cc
    mod = modelo.Modelo(); mod.ruta = np.zeros((0, 5)); mod.extra = []; vistos = {}
    for lod in range(5):
        piezas = llamadas(E, tm, m, lod, remolque); m0 = len(mod.mallas)
        for ident, M, nom in piezas:
            b = cc._bloque_recurso(A, ident >> 16)
            if nom == 'carroceria' and lod == 0: cuerpo = ident & 0xFFFF; med = caja(b, cuerpo); lo, hi = med
            if id(b) not in vistos: vistos[id(b)] = b
            pz = int(nom[5:]) if nom.startswith('rueda') else 7 + int(nom[8:]) if nom.startswith('remrueda') else \
                P_REM if nom.startswith('rem_') else 0
            cc.pieza(mod, b, ident, R=M[:3, :3], t=M[3, :3], nombre=nom,
                     grupo=cc.G_PILOTOS if nom == 'pilotos' else cc.G_FRENO if nom in ('freno', 'rem_freno') else cc.NADA,
                     pieza_id=pz)
        for x in mod.mallas[m0:]: x[7] = lod               # el LOD en el campo del nodo (la pieza, abajo)
        if lod == 0: piezas0 = piezas
    piezas = piezas0
    largo = hi[2] - lo[2]
    materiales(mod)
    for x, v in zip(mod.mallas, mod.verts):
        q = x[-1]
        if q is None: continue
        if q[0] == 'color': v['c'] = q[1]; x[0] = 2 if q[2] else 0
        else:
            _, uvs, alfa, esp = q; tr = alfa < 250
            x[1] = cc._tex_copia(mod, x[1], tr); x[0] = 2 if tr else 0
            v['u'], v['v'] = uvs[:, 0], uvs[:, 1]
            e = int(min(255, esp.max())); v['c'] = ((alfa if tr else 255) << 24) | (e << 16) | (e << 8) | e
    # ruedas: los vertices a su marco (antes de su giro de reposo: la inversa de su matriz), la demo pone la matriz; lo
    # que no gira del remolque (pieza 7) se queda en el marco del remolque (la demo pone car+C90)
    for x, v in zip(mod.mallas, mod.verts):
        if len(x) > 9 and isinstance(x[9], dict) and x[9].get('pieza'):
            if x[9]['pieza'] != P_REM:
                Mi = np.linalg.inv(x[9]['M'])
                P = np.stack([v['x'], v['y'], v['z']], 1).astype(float) @ Mi[:3, :3] + Mi[3, :3]
                v['x'], v['y'], v['z'] = P[:, 0], P[:, 1], P[:, 2]
                x[2] = P.mean(0); x[3] = float(np.sqrt(((P - x[2]) ** 2).sum(1).max()))
            x[7] = x[7] | x[9]['pieza'] << 8
    # pilotos: el grupo (G_PILOTOS = 1 los normales +C, G_FRENO = 3 los del freno +14) en los bits 24..25 del
    # campo del nodo; la demo dibuja unos u otros (traf_dibuja: car+38 > 0 del trafico, 4AD4D0)
    for x in mod.mallas:
        if len(x) > 9 and isinstance(x[9], dict) and x[9].get('grupo'): x[7] = x[7] | x[9]['grupo'] << 24
    # colores del original: las texturas alternativas, que recorta_texturas conserva con mallas de mentira (la
    # clave y sus dos colores; recorta les pone el indice nuevo)
    pares = []
    for k in claves:
        if not any(x[1] == (k[0] & 0xFFFF) for x in mod.mallas): continue
        pares.append([[9, t & 0xFFFF] + [None] * 6 for t in k[:9 if riv else 3]])
    for f in pares: mod.mallas.extend(f)
    mod.recorta_texturas()
    if mod.vq: mod.comprime(); mod._comprimido = True        # (las alternativas tambien en VQ)
    falsas = {id(q) for f in pares for q in f}
    mod.mallas = [x for x in mod.mallas if id(x) not in falsas]
    # rivales: 'TCX9', los 8 colores de car+0x12 (1..8, +73 del registro de 47CF40) por clave de la lista del modelo
    # (0x36bc18: {clave, color 1..8}); el trafico: 'TCX1' con 3 (la clave y dos)
    tcx = [b'TCX9' if riv else b'TCX1', struct.pack('<2I', 9 if riv else NCOLOR.get(m, 3), len(pares))]
    for f in pares: tcx.append(struct.pack('<%dI' % len(f), *[q[1] for q in f]))
    tcx.append(bloque_tan(E, tm, m))
    tcx.append(extra)
    mod.ruta = np.frombuffer(b''.join(tcx), '<f4').reshape(-1, 1)
    mod.mallas = [x[:8] for x in mod.mallas]
    # las mallas que repite otro LOD (el 1 lleva las del 0 salvo la sombra subida) van una vez, con la mascara de
    # los LOD que las dibujan en el campo del nodo (bit k = LOD k)
    assert len(mod.mallas) == len(mod.verts)
    clave = {}; mallas = []; verts = []; nv = 0
    for x, v in zip(mod.mallas, mod.verts):
        k = (x[0], x[1], x[7] >> 8, v.tobytes()); lod = x[7] & 0xFF   # x[7] = LOD | pieza << 8
        if k in clave: clave[k][7] |= 1 << lod; continue
        y = list(x); y[7] = 1 << lod | (x[7] >> 8) << 8; y[4] = nv; nv += len(v)
        clave[k] = y; mallas.append(y); verts.append(v)
    mod.mallas, mod.verts, mod.nv = mallas, verts, nv
    mod.escribe(ruta_fich)
    print('%s (modelo %d, registro %d): %.1f m, %d tiras, %d texturas (%d KB), piezas %s; vertices por LOD %s'
          % (os.path.basename(ruta_fich), m, cuerpo, largo, len(mod.mallas), len(mod.texs), len(mod.blob) >> 10,
             ' '.join('%s %d:%d' % (q[2], q[0] >> 16, q[0] & 0xFFFF) for q in piezas),
             ' '.join(str(sum(x[5] for x in mod.mallas if x[7] >> k & 1)) for k in range(5))))
    return med

def claves_de(E, T, m, n=3):
    """la lista de colores del modelo m (tabla 0x36bc18): {clave, color 1..} de n palabras (3 el trafico, 9 los rivales)"""
    p = E.u32(T + 4 * m); r = []
    while E.u32(p): r.append(E.u32(E.u32(p), n)); p += 4
    return r

# Camiones (--camiones): {fichero, modelo con remolque, modelos del mismo fichero}: 39 / 40 y 41 / 42 son la misma
# cabeza (el mismo descriptor del LOD 0) con y sin remolque; el 42 no lo pide ninguna tabla (46CE30 da 0x27..0x29 y
# 0x2B; puntos fijos de STAGE: 39, 40, 41 y 43) y no se escribe.
CAMIONES = [('TRAFK0.DAT', 39, (39, 40)), ('TRAFK1.DAT', 41, (41,)), ('TRAFK2.DAT', 43, (43,))]

def camiones(iso, A, dest):
    """TRAFK0..2.DAT: los camiones con remolque (modelos 39, 41 y 43; el 40 es la cabeza del 39 sola): cabeza como los
    coches (llamadas), el remolque en su marco (llamadas_remolque, pieza 7 y ruedas 8..11) y detras de TAN1 los bloques
    REM1 (bloque_rem) y TRK1: 'TRK1', u32 n y por modelo del fichero el registro de TRC1 (48 bytes) y u32 1 si se
    dibuja con el remolque (0: el 40, sin el segundo descriptor)"""
    E = A.elf; T, rec = tabla_colores(A)
    nombre = next(k for k in iso.fich if k.startswith('SLES') or k.startswith('SLUS') or k.startswith('SLPM'))
    o, s = iso.fich[nombre]; d = bytes(iso.m[o:o + s])
    tmo, tc = tabla_modelos(d), tabla_cajas(d); tm = E.va(tmo)
    for fich, m, mods in CAMIONES:
        trk = [b'TRK1', struct.pack('<I', len(mods))]
        for mm in mods:
            r = tmo + mm * 0x44
            se, k320, clase = struct.unpack('<3i', d[r + 0xc:r + 0x18]); radio, = struct.unpack('<f', d[r + 0x18:r + 0x1c])
            cj = struct.unpack('<7f', d[tc + mm * 0x3c + 4:tc + mm * 0x3c + 32])
            con = 1 if E.u32(E.u32(E.u32(tm + 0x44 * mm)) + 4) else 0
            trk.append(struct.pack('<4i', mm, clase, se, k320) + struct.pack('<8f', radio, *cj) + struct.pack('<I', con))
            print('camion %d: clase %d, +320 %d, se %d, radio %.2f, cajas %d, remolque %d' % (mm, clase, k320, se, radio, d[tc + mm * 0x3c], con))
        convierte(A, E, tm, m, claves_de(E, T, m), os.path.join(dest, fich), True, bloque_rem(E, tm, m, tc, d) + b''.join(trk))

def main():
    iso = isomod.Iso(sys.argv[1]); dest = sys.argv[2]
    if '--solo-tablas' in sys.argv[3:]:
        tablas(iso, dest); return
    from ps2.assets import Assets
    A = Assets(sys.argv[1]); E = A.elf
    if '--camiones' in sys.argv[3:]:
        camiones(iso, A, dest); return
    T, rec = tabla_colores(A)
    # --rivales m,m,..: los coches de carreras de las misiones de COAST 2 COAST (el rival: suceso 0x55 de 46E830 con el
    # modelo 0..29, que el trafico dibuja con su registro de 4866C0 como cualquier otro: obj_rc_<coche>, 5 LOD) a
    # RCmm.DAT (mm = el modelo), con el mismo formato que TRAFn.DAT; sin TRAFC.DAT ni TAPD.DAT
    riv = next((a for a in sys.argv[3:] if a.startswith('--rivales=')), None)
    lista = [int(x) for x in riv.split('=', 1)[1].split(',')] if riv else MODELOS
    print('colores del trafico: tabla en 0x%x' % T)
    nombre = next(k for k in iso.fich if k.startswith('SLES') or k.startswith('SLUS') or k.startswith('SLPM'))
    o, s = iso.fich[nombre]; tm = E.va(tabla_modelos(bytes(iso.m[o:o + s])))
    medidas = [convierte(A, E, tm, m, claves_de(E, T, m, 9 if riv else 3),
                         os.path.join(dest, ('RC%02d.DAT' % m) if riv else ('TRAF%d.DAT' % n)), riv=bool(riv))
               for n, m in enumerate(lista)]
    if not riv: tablas(iso, dest, medidas)

if __name__ == '__main__':
    main()
