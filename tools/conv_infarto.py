#!/usr/bin/env python3
"""conv_infarto.py tu.iso salida/: datos del modo INFARTO (HEART ATTACK, variante 2 de [780258] / PS2 0x443508) de
OutRun 2006 Coast 2 Coast (PS2, SLES-53998) -> INFARTO.DAT, INFARTOP.DAT y VOCESH.DAT. Sale de tu disco: no se distribuye.

Todo se lee del ELF de PS2 (SLES_539.98) en las direcciones de esta version; cada tabla se comprueba por el codigo que
la usa (el par lui/addiu de la instruccion indicada) para no leer basura de otra version. Logica y nombres del port de
PC (outrun2-decomp, platform/race_traffic_heart*.inc: 45AFB0..4650E3); direcciones de PS2:
  request manager 464350 = PS2 0x1ae440, control 464D20 = 0x1a7f28 (tabla de casos por tipo 0x3ef9c0), puesta en
  marcha 0x1ae768 (tabla 0x3f00b0), evaluacion 0x1aea30 (0x3f0120), acciones previas 463950 = 0x1af060 (0x3f0230),
  cola de voces 45BD30 = 0x1ac3d8, corazones perdidos 45AFB0 = 0x1a7ec0, nota 45C990 = 0x1b0780, corazones del
  resultado 45DCE0 = 0x1b3708, voz del resultado 45DDC0 = 0x1b3880, fin de la peticion 462F10 = 0x1b3a78.

INFARTO.DAT (little endian):
  'HRT1', u32 n_misiones (41), u32 desplazamientos (desde el principio) de: misiones, tablas de puntos, voces de
  anuncio, voces de resultado, tablas de corazones, grupos de voz por clave, coches, modelos; u32 tamano total.
  misiones: 41 x 0x140 bytes (PC 5A94D8, PS2 0x3f0b98: [78024C] = 0, el preset de OutRun 2 SP): 32 registros de 10 bytes
    {s16 seccion, s16 fin (< 0: accion previa -1..-19), s16 tipo / argumento, s16 p6, s16 p8}.
  puntos (45E2F0, tablas por tipo de peticion): 4 x 6 u32 {choque, adelantado, muro (por cuadro), golpe, rueda fuera
    del asfalto (por cuadro), trompo} de los tipos 0 (PS2 0x3efb20, PC 5B0158), 2 (0x3efd60, 5B0190), 22 (0x3efd78,
    5B01A8) y 6 (0x3efda8, 5B01C0).
  anuncio (463950, accion -6, PS2 0x1af244 con la tabla 0x3f0280): 26 x {u16 voz, u16 voz si p6 == 1} por tipo.
  resultado (45DDC0 = 0x1b3880, tabla 0x3f0a00): 26 u8 tabla por tipo (0..4; 0xff = la de defecto), 2 de relleno,
    5 tablas x 8 u32 voces por nota 0..7 (PS2 0x3f0960, 0x3f0980, 0x3f09a0, 0x3f09c0, 0x3f09e0) y la de defecto
    (u32 indice).
  corazones (45DCE0): 4 u32 por el byte de nivel del registro de la etapa (PS2 0x3f0908, PC 5B0088) y 15 u8 suma
    por clave de etapa (k - 2 por la tabla 0x3f0920 de la funcion, PC B[13] / V[5]: 10, 20, 30, 50 o 0), 1 de relleno,
    y 8 u32 tope de corazones por nota (0x1b07f0, tabla 0x3f07f0, PC 5B0038; evaluacion del tipo 6).
  grupos: 15 u8 grupo de voz de la clave de etapa k (0..3: los bancos \\SPANISH\\HAM\\<n><letra>.PAK distintos;
    el banco de la clave k es el k-esimo de la tabla de bancos 0x352840 a partir de la entrada 0x43 = HAM\\1A, en el
    orden del arbol: 1A, 2A, 2B, 3A..3C, 4A..4D, 5A..5E; PROBABLE: que el juego elija el banco por la clave),
    1 de relleno; y 15 u8 mision base de la clave (44DC50: indice del descriptor de tramo de la clave en el arbol
    csc_data_2.bin; > 14 resta 15, 45C160), 1 de relleno.
  coches (45D710 / 45D470, PS2 0x1b2e58 / 0x1b2b20): 64 tablas x 100 registros {u32 clase, s16 seccion, s8 carril,
    0} del preset != 1 (PS2 0x35c958, PC 63EE68; el 0 es OutRun 2 SP).
  modelos: 7 u32 modelo de la clase 3 / 4 por id del suceso % 7 (0x3f0850, PC 5E0C34) y 7 del tramo 0x1a (0x3f0870).
  Las misiones y los coches de INFARTO.DAT son los del preset 0 (csc_data_cvt, OutRun 2 SP: 0x1760c0(0)); hasta el
  2026-11-13 aqui ponia "del curso de OutRun 2". Los del preset 1 (OutRun 2) van en INFARTOP.DAT.
INFARTOP.DAT: 'HRP1', u32 desplazamientos de: misiones del preset 1 (41 x 0x140, PS2 0x3f3ed8: 0x1a7bb8 con
  [0x44350c] != 0), misiones base (15 u8 + 1 del arbol 0 csc_data_2 y 15 u8 + 1 del arbol 1 csc_data_cvt: 45C160 =
  0x1aec08 con el descriptor de la clave en el arbol del preset), coches del preset 1 (64 x 0x320, PS2 0x362718:
  0x1b2e58 con [0x44350c] == 1); u32 tamano total.
HAHUD.DAT (formato de HUD.DAT, tools/conv_hud.py; se abre aparte solo en el modo, con solo las tablas en la RAM): los
  corazones del medidor de 45FB80 (PS2 0x1ac580) del banco SPRANI 0x2B (GAME_CVT): HA_CORAZON (escena 0x15, cuadros
  0..0x69: aparece en 0x3D..0x69 y late en 1..0x3B), HA_ROTO (0x0E, 0..59) y HA_C0..HA_C5 (0x0F..0x14, 0..0x69: el centro y los efectos de 45B950), en un
  atlas recortado al alfa y comprimido en VQ (error RMS <= 9).
VOCESH.DAT: el formato VOZ1 de VOCES.DAT (tools/conv_sonido.py, sin tablas de cuando habla) con las muestras de
  \\SPANISH\\HC.PAK y PERH.pak (HEC.PAK no lo pide la carrera del modo) (id tal cual) y las de los bancos HAM (id + 0x200 x (grupo + 1)): pitch y
  volumen de la tabla SE del IRX (las voces a 12 kHz y los efectos con el suyo)."""
import hashlib, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.iso import Iso
import conv_sonido as cs

def extra(elf, V, u32, par):
    """INFARTOX.DAT: lo que piden los tipos de peticion de la Rev 1.1 (docs/PARIDAD.md, INFARTO: los tipos de peticion
    que faltaban). 'HRX1', u32 n, n x {char[4] etiqueta, u32 desplazamiento, u32 tamano}, datos:
      PTS2: 26 x 6 u32, la tabla de puntos de 45E2F0 (PS2 0x1a8988) de cada tipo de peticion, leida del par lui/addiu
            del a0 de la llamada de su control (0 en los tipos que no la llaman: 7 / 24, 17, 18, 21)"""
    sec = []
    # tablas de puntos por tipo: (tipos, lui, addiu, direccion esperada); 0, 2, 22 y 6 como en INFARTO.DAT
    pts = [(0,), 0x1a8d6c, 0x1a8d78, 0x3efb20], [(2,), 0x1aaaf0, 0x1aab00, 0x3efd60], [(22,), 0x1aaaf4, 0x1aaafc, 0x3efd78], \
          [(6,), 0x1aaf3c, 0x1aafa8, 0x3efda8], [(8, 23), 0x1a9a84, 0x1a9a90, 0x3efcb8], [(9,), 0x1a9f4c, 0x1a9f58, 0x3efcd0], \
          [(11,), 0x1aa384, 0x1aa390, 0x3efce8], [(14,), 0x1ab6e0, 0x1ab6ec, 0x3efdc0], [(15,), 0x1b0264, 0x1b026c, 0x3f07c0], \
          [(16, 25), 0x1b0678, 0x1b06a8, 0x3f07d8], [(19,), 0x1b11dc, 0x1b11e4, 0x3f0810], [(20,), 0x1b1f70, 0x1b1f7c, 0x3f0828]
    t = [bytes(24)] * 26
    for tipos, a, b, esp in pts:
        d = par(a, b); assert d == esp, 'tabla de puntos 0x%x != 0x%x' % (d, esp)
        for k in tipos: t[k] = elf[V(d):V(d) + 24]
    sec.append((b'PTS2', b''.join(t)))
    # HOJA: la hoja de resultados de la etapa 4603F0 (PS2 0x1ad004): 8 u32 voces por nota (0x3f7308), 5 f32 y de las
    # filas (0x3eff98), 5 u32 y de las cifras (0x3effb0), 8 u32 movimientos de la acompanante por nota (0x3f0b50; los
    # coches de 45B380: 0x3f0b70) y 8 u32 mas de la otra tabla
    tv = par(0x1ad3c0, 0x1ad3c8); assert tv == 0x3f7308, hex(tv)
    ty = par(0x1ad1d0, 0x1ad1e8); assert ty == 0x3eff98, hex(ty)
    tt = par(0x1ad1d4, 0x1ad1dc); assert tt == 0x3effb0, hex(tt)
    ta = par(0x1ad410, 0x1ad414); tb = par(0x1ad404, 0x1ad40c); assert (ta, tb) == (0x3f0b50, 0x3f0b70), (hex(ta), hex(tb))
    sec.append((b'HOJA', elf[V(tv):V(tv) + 32] + elf[V(ty):V(ty) + 20] + elf[V(tt):V(tt) + 20] + elf[V(ta):V(ta) + 32] +
                elf[V(tb):V(tb) + 32]))
    # CLAS: 26 u8, la clase de la mision 0x0F.. (45C7E0 = PS2 0x1af710, tabla de saltos 0x3f06e0: 1, 2 o 3) para el
    # rotulo de la mision especial que viene (2B003F..41)
    tj = par(0x1af748, 0x1af750); assert tj == 0x3f06e0, hex(tj)
    caso = {0x1af764: 1, 0x1af76c: 2, 0x1af774: 3, 0x1af77c: 0}
    sec.append((b'CLAS', bytes(caso[u32(tj + 4 * i)] for i in range(26))))
    # ARCO: 16 x 4 f32, los anchos de los arcos de la accion -11 (4611D0 con la clase 0x10 = PS2 0x1afa48: tabla 0x3f7218
    # + tab x 16 {inicio, medio, final, reparto}, x los carriles del primer arco)
    sec.append((b'ARCO', elf[V(0x3f7218):V(0x3f7218) + 256]))
    # RGIR: 2 x 8 u32, el movimiento de la acompanante por nota al acabar la peticion (45DED0 = PS2 0x1b3988: 0x3f0a68,
    # 0x3f0a88 con los coches de 45B380)
    ra = par(0x1b39c0, 0x1b39c8); rb_ = par(0x1b39f0, 0x1b39f4); assert (ra, rb_) == (0x3f0a68, 0x3f0a88), (hex(ra), hex(rb_))
    sec.append((b'RGIR', elf[V(ra):V(ra) + 32] + elf[V(rb_):V(rb_) + 32]))
    # EST1: 5 x 7 f32, los trozos de las estrellas del tipo 19 (461940 = PS2 0x1b11c8: la tabla 0x368630 por el carril
    # +CE, 0..6, apunta en todos a 0x3f7348; cada trozo {desvio[3], angulos x / y / z, escala})
    te = par(0x1b134c, 0x1b1360); assert te == 0x368630, hex(te)
    pe = {u32(te + 4 * k) for k in range(7)}; assert pe == {0x3f7348}, [hex(x) for x in pe]
    sec.append((b'EST1', elf[V(0x3f7348):V(0x3f7348) + 5 * 28]))
    # MARC: u16 x 2, los registros del banco 0x57 de las marcas de principio y fin de la accion -5 (463950 = PS2 0x1af13c:
    # 0x1a7c80 / 0x1a7cb0, tablas 0x3684d8 / 0x3684f0 por el idioma de 0x1f6548; el espanol, IDIOMA)
    sec.append((b'MARC', struct.pack('<2H', *marcas(u32, par))))
    # META: 2 x 41 u8, 1 si la mision (preset 0 / 1) tiene HAMpqq.DAT (lista_metas)
    lm = lista_metas(u32, par)
    sec.append((b'META', bytes(1 if (p, q) in lm else 0 for p in (0, 1) for q in range(41))))
    cab = 8 + 12 * len(sec); out = bytearray(b'HRX1' + struct.pack('<I', len(sec))); datos = b''
    for tag, d in sec:
        out += tag + struct.pack('<2I', cab + len(datos), len(d)); datos += d
        while len(datos) % 4: datos += b'\0'
    return bytes(out + datos)

# registros del banco 0x57 (obj_course_obj_common_pmt) que se convierten para los objetos de las peticiones: los
# conos 0x19 / 0x1e / 0x1f (tipos 7 / 24), las monedas 0x1c / 0x1d (15), el arco 0x2b (16 / 25), la caja 0x30 (19)
REGS_OBJ = (0x19, 0x1c, 0x1d, 0x1e, 0x1f, 0x2b, 0x30)
MONEDAS = (0x1c, 0x1d)
def aligera_arco(mod, reg=0x2b, sueltos=10):
    """DESVIACION pedida (poligonos): el arco 0x2b lleva los postes (nodo 5, 42 triangulos, opaco) y 4 nodos con la misma
    malla de destellos (transparente, 174 triangulos, animacion de vertices de 4 cuadros y cara a la camara): queda el
    primero de esos 4, solo su cuadro 0, y de sus tiras de 4 vertices (destellos sueltos) solo las `sueltos` primeras:
    42 + 146 = 188 triangulos (antes 42 + 4 x 174 = 738)"""
    import numpy as np
    V = np.concatenate(mod.verts)
    dest = [n for n in mod.nodos if (n[2] & 0xffff) == reg and (n[2] >> 16) & 0x20]
    if not dest: return
    cod = dest[0][4] & 0xff
    fuera = set(id(n) for n in dest[1:]); mod.nodos = [n for n in mod.nodos if id(n) not in fuera]
    nuevas = []; vs = []; k4 = 0
    for x in mod.mallas:
        if (x[7] & 0xff) == cod:
            if (x[7] >> 8) & 3: continue                    # cuadros 1..3 fuera
            if x[5] == 4:
                k4 += 1
                if k4 > sueltos: continue
        nuevas.append(list(x)); vs.append(V[x[4]:x[4] + x[5]])
    mod.mallas = []; mod.verts = []; mod.nv = 0
    for y, v in zip(nuevas, vs):
        y[4] = mod.nv; mod.mallas.append(y); mod.verts.append(v); mod.nv += len(v)
    t = sum(x[5] - 2 for x in mod.mallas if (x[7] & 0xff) == cod)
    print('  arco ligero: destellos %d triangulos (un nodo, cuadro 0)' % t)
def aligera_monedas(mod):
    """DESVIACION pedida (poligonos): la moneda y el corazon (registros 0x1c / 0x1d: un disco de 16 lados, canto en una
    tira de 35 vertices y las dos caras en tiras de 16 / 18: 63 triangulos) se rehacen como un disco de 8 lados con las
    mismas UV, radio, grosor, textura y color: canto 16 triangulos y caras 6 + 6 (28). Y una version lejana (registro +
    0x80, desde 40 m en la demo) solo con las dos caras: 12 triangulos"""
    import numpy as np
    V = np.concatenate(mod.verts); nuevas = []; vs = []; nv = 0
    cod_mon = {}
    for nd in mod.nodos:
        if (nd[2] & 0xffff) in MONEDAS: cod_mon[nd[4] & 0xff] = nd[2] & 0xffff
    mallas_de = {}
    for x in mod.mallas:
        if (x[7] & 0xff) in cod_mon and not (x[7] >> 8) & 3: mallas_de.setdefault(x[7] & 0xff, []).append(x)
    hecho = set(); lejos = {}
    def octo(x0, R, t, c, cara):
        ang = [np.pi - k * np.pi / 4 for k in range(8)]
        P = [(R * np.cos(a), R * np.sin(a)) for a in ang]
        if cara is None:                                # canto: pares (-t, +t) de 180 grados hacia abajo, cerrado
            idx = list(range(8)) + [0]; v = np.zeros(18, dtype=V.dtype)
            for j, k in enumerate(idx):
                th = 180.0 - 45.0 * j; u = -0.048 - th * 0 - (180.0 - th) / 22.5 * 0.01465
                for h, z in enumerate((-t, t)):
                    q = v[2 * j + h]; q['x'], q['y'], q['z'] = P[k][0], P[k][1], z; q['u'], q['v'] = u, 0.081 if z < 0 else 0.008; q['c'] = c
            return v
        orden = [0, 1, 7, 2, 6, 3, 5, 4] if cara > 0 else [6, 5, 7, 4, 0, 3, 1, 2]
        v = np.zeros(8, dtype=V.dtype)
        for j, k in enumerate(orden):
            q = v[j]; q['x'], q['y'], q['z'] = P[k][0], P[k][1], t * cara
            q['u'], q['v'] = 0.5 + P[k][0] / R * 0.49, 0.5 + P[k][1] / R * 0.49; q['c'] = c
        return v
    for x in mod.mallas:
        g = x[7] & 0xff
        if g in cod_mon and not (x[7] >> 8) & 3:
            if g in hecho: continue
            hecho.add(g); M = mallas_de[g]
            W = np.concatenate([V[m[4]:m[4] + m[5]] for m in M])
            R = float(np.sqrt(W['x'] ** 2 + W['y'] ** 2).max()); t = float(np.abs(W['z']).max()); c = int(W['c'][0])
            for cara in (None, 1, -1):
                v = octo(0, R, t, c, cara); y = list(x); y[4] = nv; y[5] = len(v); nuevas.append(y); vs.append(v); nv += len(v)
            lejos[g] = (x, R, t, c)
            continue
        y = list(x); v = V[x[4]:x[4] + x[5]]; y[4] = nv; y[5] = len(v); nuevas.append(y); vs.append(v); nv += len(v)
    # la version lejana: otra malla (codigo nuevo) y otro nodo con el registro + 0x80
    sig = max([n[4] & 0xff for n in mod.nodos] + [0]) + 1
    for g, (x, R, t, c) in lejos.items():
        cod = sig; sig += 1
        for cara in (1, -1):
            v = octo(0, R, t, c, cara); y = list(x); y[4] = nv; y[5] = len(v); y[7] = cod; nuevas.append(y); vs.append(v); nv += len(v)
        nd = next(n for n in mod.nodos if (n[4] & 0xff) == g)
        mod.nodos.append([nd[0], nd[1], (nd[2] & 0xffff0000) | (cod_mon[g] | 0x80), nd[3], cod | (nd[4] & 0xffff0000)])
    # (las mallas van por lista: se ordenan junto con sus vertices; antes se ordenaba solo nuevas y varias mallas se
    # quedaban con los vertices de otra)
    mod.mallas = []; mod.verts = []; mod.nv = 0
    for y, v in sorted(zip([list(n) for n in nuevas], vs), key=lambda p: p[0][0]):
        y[4] = mod.nv; mod.mallas.append(y); mod.verts.append(v); mod.nv += len(v)
    print('  monedas ligeras: %d registros, %d triangulos cerca y %d lejos por moneda (antes 63)' % (len(lejos), 16 + 12, 12))

def sigue_dibujos(u32, pc, fin, dib=0x1118b8):
    """los id (recurso << 16 | registro) que un caso del codigo pasa en a0 a 0x1118b8 (dibuja un objeto en la matriz de
    la pila): lui / ori de a0, jal y b con su hueco de retardo, hasta fin"""
    a0 = 0; ids = []; n = 0
    def paso(x):
        nonlocal a0
        if x >> 26 == 15 and (x >> 16) & 31 == 4: a0 = (x & 0xffff) << 16
        elif x >> 26 == 13 and (x >> 16) & 31 == 4 and (x >> 21) & 31 == 4: a0 |= x & 0xffff
    while pc != fin and n < 64:
        w = u32(pc); n += 1
        if w >> 26 == 3:                                # jal
            paso(u32(pc + 4))
            if (w & 0x3ffffff) << 2 == dib: ids.append(a0)
            pc += 8; continue
        if w >> 26 == 4 and (w >> 16) & 0x3ff == 0:     # b (beq zero, zero)
            paso(u32(pc + 4)); pc += 4 + ((w & 0xffff) - (0x10000 if w & 0x8000 else 0)) * 4; continue
        paso(w); pc += 4
    assert pc == fin, 'caso sin terminar en 0x%x' % pc
    return ids
def trocea(mod, maxv=18):
    """DESVIACION (poligonos): las tiras largas en trozos de maxv vertices (los dos ultimos de uno abren el siguiente), con
    su esfera: la demo dibuja solo los trozos cercanos y delante de la camara"""
    import numpy as np
    V = np.concatenate(mod.verts); M = []; vs = []
    for x in mod.mallas:
        n = x[5]; i0 = x[4]
        for a in (range(0, n - 2, maxv - 2) if n > maxv else (0,)):
            b = min(a + maxv, n) if n > maxv else n
            v = V[i0 + a:i0 + b].copy(); P = np.stack([v['x'], v['y'], v['z']], 1)
            c = (P.min(0) + P.max(0)) / 2; y = list(x); y[2] = c.astype(np.float32); y[3] = float(np.linalg.norm(P - c, axis=1).max())
            M.append(y); vs.append(v)
    mod.mallas = []; mod.verts = []; mod.nv = 0
    for y, v in zip(M, vs):
        y[4] = mod.nv; y[5] = len(v); mod.mallas.append(y); mod.verts.append(v); mod.nv += len(v)
def lista_metas(u32, par):
    """{(preset, mision): [id]} de 45ECA0 / 45E9B0 (ver metas)"""
    assert u32(0x1a9a4c) == 0x3c020044 and u32(0x1a9a5c) >> 26 == 3 and u32(0x1a9a6c) >> 26 == 3
    filtro = []
    for a in range(0x1a95d0, 0x1a9640, 8):              # addiu v0, zero, q / beq s0, v0 (la ultima con bnel)
        w = u32(a); assert w >> 16 == 0x2402, hex(a); filtro.append(w & 0xffff)
    t1 = par(0x1a9688, 0x1a9690); t0 = par(0x1a98c0, 0x1a98c8); assert (t1, t0) == (0x3efbf0, 0x3efc60), (hex(t1), hex(t0))
    assert u32(0x1a9680) == 0x2c62001a and u32(0x1a98b8) == 0x2c620016        # sltiu v0, v1, 26 / 22
    r = {}
    for pr, jt, n, fin in ((0, t0, 22, 0x1a9a24), (1, t1, 26, 0x1a982c)):
        for k in range(n):
            if pr == 1 and 15 + k not in filtro: continue
            ids = sigue_dibujos(u32, u32(jt + 4 * k), fin)
            if ids: r[(pr, 15 + k)] = ids
    return r
def metas(A, u32, par, sal):
    """HAMpqq.DAT: los modelos de la mision qq del preset p que el original dibuja sobre la etapa (45ECA0 / 45E9B0 = PS2
    0x1a9a48: [0x44350c] == 0 -> 0x1a9850, si no 0x1a95b0): los carriles pintados de azul y rojo de las misiones de los
    tipos 8 / 23 (y algun otro), registros de obj_course_obj_cs_<zona> en el marco de la etapa + (0, 0,1, 0). Se lee
    cada caso de las tablas de saltos 0x3efc60 (preset 0, misiones 15..36) y 0x3efbf0 (preset 1, 15..40, solo las de la
    lista de 0x1a95d0). SIN las misiones < 15 del preset 0 (por la etapa de 0x176700: 0x14 / 0x19 / 0x1d). ORD7 sin
    nodos, con las tiras en trozos (trocea)."""
    import modelo, conv_coche
    E = A.elf; tab = conv_coche.recursos(A); hechos = 0
    for (pr, q), ids in sorted(lista_metas(u32, par).items()):
        if True:
            assert len({i >> 16 for i in ids}) == 1, ids
            nombre = E.cadena(E.u32(tab + 4 * (ids[0] >> 16)))
            ruta = nombre[len('\\Stage'):] if nombre.lower().startswith('\\stage\\') else nombre
            blq = A.bloque(A.entrada('STAGE.PS2', ruta))
            mod = modelo.Modelo(); mod.bloque(blq, registros={i & 0xffff for i in ids}, sin_textura='material', con_gs=True)
            t0_ = sum(x[5] - 2 for x in mod.mallas); trocea(mod)
            mod.recorta_texturas(); nom = 'HAM%d%02d.DAT' % (pr, q); mod.escribe(os.path.join(sal, nom)); hechos += 1
            print('%s: %s %s: %d mallas, %d vertices, %d triangulos (%d antes de trocear), %d texturas, %d KB' % (nom, nombre.split('\\')[-1],
                  ' '.join('%x' % i for i in ids), len(mod.mallas), mod.nv, sum(x[5] - 2 for x in mod.mallas), t0_, len(mod.texs),
                  os.path.getsize(os.path.join(sal, nom)) >> 10))
    print('HAMpqq.DAT: %d ficheros' % hechos)

IDIOMA = 4                                              # 0x1f6548: el espanol (SPRACE, FRENCH, GERMAN, ITALIAN, SPANISH)
def marcas(u32, par):
    ta, tb = par(0x1a7c90, 0x1a7c98), par(0x1a7cc0, 0x1a7cc8); assert (ta, tb) == (0x3684d8, 0x3684f0), (hex(ta), hex(tb))
    a, b = u32(ta + 4 * IDIOMA), u32(tb + 4 * IDIOMA); assert a >> 16 == 0x57 and b >> 16 == 0x57, (hex(a), hex(b))
    return a & 0xffff, b & 0xffff

ESTRELLA = 0x30
def aligera_estrella(mod, reg=ESTRELLA):
    """DESVIACION pedida (poligonos): la estrella 0x30 (clase 0x12, tipo 19: una bipiramide de 10 puntas, 20 triangulos
    sueltos de 3 vertices) queda igual de cerca y lleva una version lejana (registro + 0x80) plana: las 5 puntas (un
    triangulo cada una) y el pentagono del centro (una tira de 5): 8 triangulos / 20 vertices, con las UV y el color
    medio de la original"""
    import numpy as np
    V = np.concatenate(mod.verts)
    nd = next((n for n in mod.nodos if (n[2] & 0xffff) == reg), None)
    if nd is None: return
    g = nd[4] & 0xff
    M = [x for x in mod.mallas if (x[7] & 0xff) == g and not (x[7] >> 8) & 3]
    W = np.concatenate([V[m[4]:m[4] + m[5]] for m in M])
    cuenta = {}
    for v in W:
        k = (round(float(v['x']), 3), round(float(v['y']), 3), round(float(v['z']), 3)); cuenta.setdefault(k, []).append(v)
    aro = [l[0] for l in cuenta.values() if len(l) == 4]; vert = [k for k, l in cuenta.items() if len(l) == 10]
    assert len(aro) == 10 and len(vert) == 2, (len(aro), len(vert))   # 10 puntas / huecos y los 2 vertices
    C = np.mean(np.array(vert), axis=0); Q = [np.array([v['x'], v['y'], v['z']]) - C for v in aro]
    r = [float(np.linalg.norm(q)) for q in Q]; e1 = Q[int(np.argmax(r))] / max(r)
    nrm = np.array(vert[0]) - C; nrm /= np.linalg.norm(nrm); e2 = np.cross(nrm, e1)
    orden = sorted(range(10), key=lambda i: np.arctan2(Q[i] @ e2, Q[i] @ e1) % (2 * np.pi))
    P = [aro[i] for i in orden]                         # P[0] una punta, alternando punta / hueco
    c = int(np.mean([((int(v['c']) >> 8) & 0xff) for v in W])); col = 0xff000000 | c << 16 | c << 8 | c
    def vs(lista):
        v = np.zeros(len(lista), dtype=V.dtype)
        for i, q in enumerate(lista):
            for n in ('x', 'y', 'z', 'u', 'v'): v[i][n] = q[n]
            v[i]['c'] = col
        return v
    sig = max([n[4] & 0xff for n in mod.nodos] + [0]) + 1
    nuevas = []
    for k in range(5): nuevas.append(vs([P[(2 * k - 1) % 10], P[2 * k], P[(2 * k + 1) % 10]]))
    i5 = [P[2 * k + 1] for k in range(5)]                # el pentagono interior: tira 0, 1, 4, 2, 3
    nuevas.append(vs([i5[0], i5[1], i5[4], i5[2], i5[3]]))
    for v in nuevas:
        y = list(M[0]); y[4] = mod.nv; y[5] = len(v); y[7] = sig; mod.mallas.append(y); mod.verts.append(v); mod.nv += len(v)
    mod.nodos.append([nd[0], nd[1], (nd[2] & 0xffff0000) | (reg | 0x80), nd[3], sig | (nd[4] & 0xffff0000)])
    print('  estrella lejana: %d triangulos / %d vertices (cerca 20 / 60)' % (sum(len(v) - 2 for v in nuevas), sum(len(v) for v in nuevas)))

def objetos(A, elf, V, u32, par, sal):
    """HAOSO.DAT y HAOBJ.DAT: los objetos de las peticiones (Rev 1.1).
    El fichero de objetos de la mision (45C470 = PS2 0x1af428, comprobado): preset 0 ([gp-0x7b64] == 0) y mision >= 15
    la tabla de saltos 0x3f05c0 por mision - 15; preset != 0 y mision >= 15 la 0x3f0630; mision < 15 solo con el preset
    0, la 0x3f06a0 por el descriptor de la clave - 15 (= la mision base). Cada caso: lui v0 / b / addiu v0 con el
    nombre ("oso_qt_*.sz" de OSO.PS2); el caso por defecto, ninguno. Los registros de 0x28 de cada fichero (4F0660 /
    4F0D10: hasta y < -10000): {f32 pos[3], f32 angulos[3], u32 clase +18, u32 tipo +1C, u32 +20, u32 +24}, en el
    marco de la etapa. Clase -> modelo y escala: 0x40a958 (8 bytes por clase; recurso << 16 | registro).
    HAOSO.DAT: 'HAO1', u32 ficheros, 2 x 41 u8 fichero por (preset, mision) (0xff: ninguno), 2 de relleno, 64 x {u16
      registro del banco 0x57 (0xffff otro recurso), u16 0, f32 escala} por clase, ficheros x {u32 desplazamiento,
      u32 objetos}, y los objetos: {u8 clase, u8 tipo, u16 0, f32 pos[3], f32 angulos[3]} (28 bytes).
    HAOBJ.DAT: ORD7 de conv_bo.modelo_conjunto con los registros REGS_OBJ del banco 0x57 (nodo: banderas << 16 |
      registro; relleno: codigo de malla)."""
    import conv_bo, conv_coche
    def caso(t):
        a, b, c = u32(t), u32(t + 4), u32(t + 8)
        if a >> 16 == 0x3c02 and b >> 26 == 4 and c >> 16 == 0x2442:
            d = ((a & 0xffff) << 16) + (c & 0xffff) - (0x10000 if c & 0x8000 else 0)
            return elf[V(d):].split(b'\0')[0].decode()
        return None
    assert par(0x1af46c, 0x1af474) == 0x3f05c0 and par(0x1af574, 0x1af57c) == 0x3f0630 and par(0x1af658, 0x1af660) == 0x3f06a0
    nombres = []; idx = bytearray([0xff] * 82)
    for pr, mi, jt, n in ((0, 15, 0x3f05c0, 26), (1, 15, 0x3f0630, 26), (0, 0, 0x3f06a0, 15)):
        for i in range(n):
            nm = caso(u32(jt + 4 * i))
            if nm is None: continue
            if nm not in nombres: nombres.append(nm)
            idx[pr * 41 + mi + i] = nombres.index(nm)
    tc = 0x40a958
    assert u32(tc + 8 * 9) == 0x57001e and u32(tc + 8 * 10) == 0x57001c, 'tabla de clases 0x40a958'
    clases = b''
    for c in range(64):
        mid = u32(tc + 8 * c); esc = struct.unpack('<f', elf[V(tc + 8 * c + 4):V(tc + 8 * c + 8)])[0]
        reg = mid & 0xffff if mid >> 16 == 0x57 and 0 < esc < 100 else 0xffff
        clases += struct.pack('<HHf', reg, 0, esc if reg != 0xffff else 0.0)
    fich = []; datos = b''
    for nm in nombres:
        d = A.datos(A.entrada('OSO.PS2', '\\' + nm)); o = 4; k = 0
        while o + 0x28 <= len(d):
            r = struct.unpack_from('<6f4I', d, o)
            if r[1] < -10000: break
            datos += struct.pack('<BBH6f', r[6], r[7], 0, *r[:6]); o += 0x28; k += 1
        fich.append(k)
    cab = 4 + 4 + 82 + 2 + len(clases) + 8 * len(fich)
    out = b'HAO1' + struct.pack('<I', len(fich)) + bytes(idx) + b'\0\0' + clases
    o = cab
    for k in fich: out += struct.pack('<2I', o, k); o += 28 * k
    out += datos
    open(os.path.join(sal, 'HAOSO.DAT'), 'wb').write(out)
    print('HAOSO.DAT: %d ficheros (%s), %d objetos, el mayor %d; %d bytes' % (len(fich), ' '.join(nombres), sum(fich), max(fich), len(out)))
    E = A.elf; nombre = E.cadena(E.u32(conv_coche.recursos(A) + 4 * 0x57)).split('\\')[-1]
    blq = A.bloque(A.entrada('COMMON.PS2', nombre))
    import modelo as _m
    esc0 = _m.Modelo.escribe
    def escribe(self, ruta):                            # las monedas ligeras (aligera_monedas) antes de escribir
        aligera_estrella(self); aligera_arco(self); aligera_monedas(self); return esc0(self, ruta)
    _m.Modelo.escribe = escribe
    try: mod = conv_bo.modelo_conjunto(A, blq, set(REGS_OBJ) | set(marcas(u32, par)), os.path.join(sal, 'HAOBJ.DAT'))
    finally: _m.Modelo.escribe = esc0
    print('HAOBJ.DAT: %d mallas, %d vertices, %d nodos, %d texturas (%d KB), %d KB' % (len(mod.mallas), mod.nv, len(mod.nodos),
          len(mod.texs), len(mod.blob) >> 10, os.path.getsize(os.path.join(sal, 'HAOBJ.DAT')) >> 10))

def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    iso = Iso(sys.argv[1]); sal = sys.argv[2]; os.makedirs(sal, exist_ok=True)
    lba, tam = iso.fich['SLES_539.98']; elf = bytes(iso.m[lba:lba + tam])
    V = lambda a: a - 0x100000 + 0x80
    assert elf[:4] == b'\x7fELF' and struct.unpack_from('<I', elf, 0x18)[0] == 0x101100, 'ELF inesperado'
    u32 = lambda a: struct.unpack_from('<I', elf, V(a))[0]

    def par(lui, add):
        """direccion que forman un lui y un addiu del codigo"""
        a, b = u32(lui), u32(add)
        assert a >> 26 == 15 and b >> 26 == 9, 'codigo inesperado en 0x%x / 0x%x' % (lui, add)
        return ((a & 0xffff) << 16) + (b & 0xffff) - (0x10000 if b & 0x8000 else 0)

    def li(a):
        w = u32(a); assert w >> 26 in (9, 13) and (w >> 21) & 31 == 0, 'se esperaba un li en 0x%x' % a
        return w & 0xffff
    if '--solo-x' in sys.argv:                          # solo INFARTOX.DAT
        open(os.path.join(sal, 'INFARTOX.DAT'), 'wb').write(extra(elf, V, u32, par)); return
    if '--solo-obj' in sys.argv:                        # solo HAOSO.DAT y HAOBJ.DAT
        from ps2.assets import Assets
        objetos(Assets(sys.argv[1]), elf, V, u32, par, sal); return
    if '--solo-metas' in sys.argv:                      # solo HAMpqq.DAT
        from ps2.assets import Assets
        metas(Assets(sys.argv[1]), u32, par, sal); return

    misiones = par(0x1a7bdc, 0x1a7be4)                  # 45E050 / quest_records: [0x44350c] == 0
    assert misiones == 0x3f0b98, hex(misiones)
    mis = elf[V(misiones):V(misiones) + 41 * 0x140]
    for q in range(41):                                 # cada mision: registros con tipo 0..25 o accion previa
        for r in range(32):
            s0, s1, k = struct.unpack_from('<3h', mis, q * 0x140 + r * 10)
            if not s0: break
            assert (s1 > s0 and 0 <= k < 26) or -19 <= s1 < 0, 'mision %d registro %d inesperado' % (q, r)
    t_pun = [par(0x1a8d6c, 0x1a8d78), par(0x1aaaf0, 0x1aab00), par(0x1aaaf4, 0x1aaafc), par(0x1aaf3c, 0x1aafa8)]
    assert t_pun == [0x3efb20, 0x3efd60, 0x3efd78, 0x3efda8], [hex(x) for x in t_pun]
    puntos = b''.join(elf[V(a):V(a) + 24] for a in t_pun)

    # anuncio: la tabla de saltos 0x3f0280 de 0x1af244 por tipo; cada caso carga la voz en a0 (o dos, por p6 == 1)
    jt = par(0x1af250, 0x1af258); assert jt == 0x3f0280
    anuncio = bytearray()
    for k in range(26):
        t = u32(jt + 4 * k); ids = []
        for i in range(8):
            w = u32(t + 4 * i)
            if w >> 26 == 9 and (w >> 16) & 31 == 4 and (w >> 21) & 31 == 0: ids.append(w & 0xffff)
            if w >> 26 == 4 and i > 0 and len(ids): break            # b / beq hacia la llamada
        if u32(t) == 0x86230006:                                  # lh v1, 6(s1): dos voces por p6 == 1
            si, no = li(t + 0xc), li(t + 0x14)
        elif t == 0x1af3c4 or not ids: si = no = 0
        else: si = no = ids[0]
        anuncio += struct.pack('<2H', no, si)
    # resultado: 0x1b3880, tabla 0x3f0a00 por tipo -> base de la tabla de 8 voces
    jr = par(0x1b3898, 0x1b38a0); assert jr == 0x3f0a00
    bases = [0x3f0960, 0x3f0980, 0x3f09a0, 0x3f09c0, 0x3f09e0]
    por_tipo = []
    for k in range(26):
        t = u32(jr + 4 * k); b = None
        for i in range(4):
            w = u32(t + 4 * i)
            if w >> 26 == 9 and (w >> 16) & 31 == 2: b = 0x3f0000 + (w & 0xffff)
        por_tipo.append(bases.index(b) if b in bases else 0xff)
    defecto = bases.index(par(0x1b395c, 0x1b3964))
    resultado = bytes(por_tipo) + b'\0\0' + b''.join(elf[V(a):V(a) + 32] for a in bases) + struct.pack('<I', defecto)
    # corazones del resultado (0x1b3708): tabla por el byte de nivel y suma por clave de etapa (tabla de saltos 0x3f0920)
    tn = par(0x1b37b8, 0x1b37bc); assert tn == 0x3f0908
    js = par(0x1b3774, 0x1b377c); assert js == 0x3f0920
    suma = [0] * 15
    for k in range(2, 15):
        t = u32(js + 4 * (k - 2)); w = u32(t + 4)
        suma[k] = w & 0xffff if w >> 26 == 9 else 0
    corazones = elf[V(tn):V(tn) + 16] + bytes(suma) + b'\0'
    tc = par(0x1b07f4, 0x1b07f8); assert tc == 0x3f07f0          # 0x1b07f0: tope de corazones por nota (PC 5B0038)
    corazones += elf[V(tc):V(tc) + 32]

    # bancos de voz HAM por clave y misiones base por clave
    tb_bancos = 0x352840
    assert elf[V(u32(tb_bancos + 4 * 0x43)):V(u32(tb_bancos + 4 * 0x43)) + 10] == b'HAM\\1A.PAK', 'tabla de bancos'
    nombres = [elf[V(u32(tb_bancos + 4 * (0x43 + k))):].split(b'\0')[0].decode() for k in range(15)]
    datos_b = [cs.fichero_paquete(iso, 'SOUND.PS2', '\\SPANISH\\' + n) for n in nombres]
    unicos = []
    for d in datos_b:
        h = hashlib.md5(d).digest()
        if h not in unicos: unicos.append(h)
    grupo = [unicos.index(hashlib.md5(d).digest()) for d in datos_b]
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from ps2.assets import Assets
    import conv_union
    A = Assets(sys.argv[1])
    arbol = {r[0]: r[1] for r in conv_union.arbol(A, 'PALM')}
    base, n, _ = conv_union.tablas(A)
    mis_base = []
    for k in range(15):
        d = u32(base + 4 * arbol[k]); q = u32(d)              # 44DC50: +0 del descriptor
        mis_base.append(q - 15 if q > 14 else q)
    grupos = bytes(grupo) + b'\0' + bytes(mis_base) + b'\0'

    # coches de las tablas de 45D710 / 45D470 (PS2 0x1b2e58 / 0x1b2b20): 64 tablas de 100 registros de 8 bytes
    # {u32 clase, s16 seccion, s8 carril, 0} del preset != 1 (0x35c958; el del preset 1, OutRun 2, en 0x362718) y los modelos de
    # la clase 3/4 (0x3f0850: 7 por el id del suceso; 0x3f0870 en el tramo de descriptor 0x1a)
    tt = par(0x1b2eac, 0x1b2eb0); assert tt == 0x35c958
    coches = elf[V(tt):V(tt) + 64 * 0x320]
    tm = par(0x1b2ce0, 0x1b2ce8); tm2 = par(0x1b2cbc, 0x1b2cc4); assert (tm, tm2) == (0x3f0850, 0x3f0870)
    modelos = elf[V(tm):V(tm) + 28] + elf[V(tm2):V(tm2) + 28]
    partes = [mis, puntos, bytes(anuncio), resultado, corazones, grupos, coches, modelos]
    off = 4 + 4 + 4 * 8 + 4; offs = []
    for p in partes: offs.append(off); off += len(p)
    out = b'HRT1' + struct.pack('<I', 41) + struct.pack('<8I', *offs) + struct.pack('<I', off) + b''.join(partes)
    open(os.path.join(sal, 'INFARTO.DAT'), 'wb').write(out)
    print('INFARTO.DAT: %d bytes; anuncio %s' % (len(out), [hex(x) for x in struct.unpack('<52H', anuncio)[::2]]))
    print('  resultado por tipo %s defecto %d; suma por clave %s; grupos %s (%s); misiones base %s' % (
        list(por_tipo), defecto, suma, grupo, nombres, mis_base))

    # INFARTOP.DAT: lo que cambia con el preset [0x44350c] (0x1760c0: 0 -> csc_data_cvt, OutRun 2 SP; 1 ->
    # csc_data_2, OutRun 2): misiones de 0x1a7bb8 con el preset != 0 (0x3f3ed8), coches de 0x1b2e58 con el preset 1
    # (0x362718) y la mision base de cada clave en los dos arboles (45C160 = 0x1aec08: 176700(clave), > 14 resta 15)
    mis1 = par(0x1a7bc8, 0x1a7bd0); assert mis1 == 0x3f3ed8, hex(mis1)
    m1 = elf[V(mis1):V(mis1) + 41 * 0x140]
    for q in range(41):
        for r in range(32):
            s0, s1, k = struct.unpack_from('<3h', m1, q * 0x140 + r * 10)
            if not s0: break
            assert (s1 > s0 and 0 <= k < 26) or -19 <= s1 < 0, 'mision %d registro %d inesperado (preset 1)' % (q, r)
    tt1 = par(0x1b2e94, 0x1b2e9c); assert tt1 == 0x362718, hex(tt1)
    coches1 = elf[V(tt1):V(tt1) + 64 * 0x320]
    bases = b''
    for fich in ('csc_data_2.bin', 'csc_data_cvt.bin'):
        arb = {r[0]: r[1] for r in conv_union.arbol(A, 'PALM', fich)}; bb = []
        for k in range(15):
            q = u32(u32(base + 4 * arb[k])); bb.append(q - 15 if q > 14 else q)
        bases += bytes(bb) + b'\0'
        print('  misiones base de %s: %s' % (fich, bb))
    assert bases[:15] == bytes(mis_base)
    partes = [m1, bases, coches1]
    off = 4 + 4 * len(partes) + 4; offs = []
    for p_ in partes: offs.append(off); off += len(p_)
    outp = b'HRP1' + struct.pack('<%dI' % len(partes), *offs) + struct.pack('<I', off) + b''.join(partes)
    open(os.path.join(sal, 'INFARTOP.DAT'), 'wb').write(outp)
    print('INFARTOP.DAT: %d bytes (misiones 0x%x, coches 0x%x)' % (len(outp), mis1, tt1))
    open(os.path.join(sal, 'INFARTOX.DAT'), 'wb').write(extra(elf, V, u32, par))
    objetos(A, elf, V, u32, par, sal)
    metas(A, u32, par, sal)
    print('INFARTOX.DAT: %d bytes' % os.path.getsize(os.path.join(sal, 'INFARTOX.DAT')))

    # HAHUD.DAT
    import numpy as np, conv_hud as ch
    D = ch.Disco(sys.argv[1]); ag, Tg, _ = D.banco(0x2b)
    hud = ch.Hud(); hud.taco = [0xff] * 16; grupos = []
    # rotulos de 460970 / 460AD0 (PS2 0x1ad998): textos de la tabla 0x3f0aa8 (37 tokens; el 12, 0x2B003D, es el de modo 0)
    # y marcos de 0x3f0b40 (3), cada uno con su escena entera (solo los cuadros donde cambia: racha en flags >> 4)
    tt = par(0x1ad9fc, 0x1ada10); assert tt == 0x3f0aa8
    tf = par(0x1ad99c, 0x1ad9a4); assert tf == 0x3f0b40
    rot = [('HA_T%02d' % i, struct.unpack_from('<I', elf, V(tt + 4 * i))[0]) for i in range(37)]
    rot += [('HA_M%d' % i, struct.unpack_from('<I', elf, V(tf + 4 * i))[0]) for i in range(3)]
    # corazones de 0x3f0b20 (2B000F..2B0015, el del centro por corazones % 7 y los efectos de 45B950: cuadros 0x1F..0x3C,
    # 0x4C..0x5A y 0x5B..0x69) y la rotura 2B000E (0..59)
    tc = par(0x1ac8d8, 0x1ac8dc); assert tc == 0x3f0b20
    tcor = struct.unpack_from('<7I', elf, V(tc)); assert tcor == tuple(range(0x2b000f, 0x2b0016)), 'corazones de 0x3f0b20'
    escenas = [('HA_CORAZON', 0x15, range(0, 0x6a)), ('HA_ROTO', 0x0e, range(60))]
    escenas += [('HA_C%d' % i, tcor[i] & 0xffff, range(0x1f, 0x6a)) for i in range(6)]   # (cuadro - 0x1F)
    for nombre, tok in rot:
        if tok >> 16 != 0x2b: continue
        k = tok & 0xffff; sc = ag.escenas[k]; nc, pc = struct.unpack_from('<2I', ag.a, sc); raiz = pc + (nc - 1) * 0x24
        escenas.append((nombre, k, range(int(struct.unpack_from('<f', ag.a, raiz + 8)[0]))))
    # panel 2C002A (NAVI_PUB 0x1bd544, variante 2: en lugar de 2C0028 / 2C0029) y las filas 2C0037..2C003B (4BB9E0 =
    # PS2 0x1bd920, tabla 0x404418) del banco 0x2C, cuadros 0 y 1
    assert u32(0x1bd55c) == 0x3c04002c and u32(0x1bd570) == 0x3484002a, 'panel 2C002A'
    tfil = par(0x1bd924, 0x1bd944); assert tfil == 0x404418
    fil = struct.unpack_from('<5I', elf, V(tfil)); assert fil == tuple(range(0x2c0037, 0x2c003c)), 'filas 2C0037..'
    a2c, T2c, _ = D.banco(0x2c)
    escenas = [(n, k, f, 0x2b) for n, k, f in escenas] + [('HA_PANEL', 0x2a, range(1), 0x2c)]
    escenas += [('HA_FILA%d' % i, fil[i] & 0xffff, range(2), 0x2c) for i in range(5)]
    # Rev 1.1: el panel de la nota que entra (45BDF0 = PS2 0x1ad6f4, tabla 0x3f0010 por la nota 7F8B2C) y la hoja de
    # resultados de la etapa (4603F0 = PS2 0x1ad004): titulo del nivel (0x3eff60), peticiones del nivel 2C0090..92, nota
    # de cada fila (0x3eff78; la que corre: la misma tabla por (t >> 2) & 7), "Ranking en la etapa" 2C0081 y su animacion
    # (0x3effc8), el rotulo de la nota media (0x3effe8, banco 0x2B, entero: 428320 con el modo 3) y el de la mision
    # especial que viene (2B003F..41, enteros). Las del banco 0x2C, solo el cuadro 0 (429530 con el cuadro 0)
    def tabla(lui, add, esp, n):
        a = par(lui, add); assert a == esp, 'tabla 0x%x != 0x%x' % (a, esp)
        return struct.unpack_from('<%dI' % n, elf, V(a))
    def todos(bk_, tok):
        am = ag if bk_ == 0x2b else a2c
        sc = am.escenas[tok & 0xffff]; nc, pc = struct.unpack_from('<2I', am.a, sc); raiz = pc + (nc - 1) * 0x24
        return range(int(struct.unpack_from('<f', am.a, raiz + 8)[0]))
    for i, tok in enumerate(tabla(0x1ad764, 0x1ad76c, 0x3f0010, 8)):
        assert tok >> 16 == 0x2c; escenas.append(('HA_CARA%d' % i, tok & 0xffff, range(1), 0x2c))
    for i, tok in enumerate(tabla(0x1ad0cc, 0x1ad0d4, 0x3eff60, 5)):
        assert tok >> 16 == 0x2c; escenas.append(('HA_HN%d' % i, tok & 0xffff, range(1), 0x2c))
    assert [u32(a) & 0xffff for a in (0x1ad178, 0x1ad184, 0x1ad1a8)] == [0x90, 0x91, 0x92], 'peticiones del nivel 2C0090..92'
    for i in range(3): escenas.append(('HA_HC%d' % (i + 1), 0x90 + i, range(1), 0x2c))
    for i, tok in enumerate(tabla(0x1ad24c, 0x1ad254, 0x3eff78, 8)):
        assert tok >> 16 == 0x2c; escenas.append(('HA_HG%d' % i, tok & 0xffff, range(1), 0x2c))
    assert u32(0x1ad440) & 0xffff == 0x81; escenas.append(('HA_HE', 0x81, range(1), 0x2c))
    for i, tok in enumerate(tabla(0x1ad4fc, 0x1ad504, 0x3effc8, 8)):
        assert tok >> 16 == 0x2c; escenas.append(('HA_HA%d' % i, tok & 0xffff, range(1), 0x2c))
    for i, tok in enumerate(tabla(0x1ad3d8, 0x1ad3e0, 0x3effe8, 8)):
        assert tok >> 16 == 0x2b; escenas.append(('HA_HB%d' % i, tok & 0xffff, todos(0x2b, tok), 0x2b))
    assert [u32(a) & 0xffff for a in (0x1ad5e4, 0x1ad5f8, 0x1ad61c)] == [0x3f, 0x40, 0x41], 'misiones especiales 2B003F..41'
    for i in range(3): escenas.append(('HA_HS%d' % (i + 1), 0x3f + i, todos(0x2b, 0x2b003f + i), 0x2b))
    # 429530 (x, y) con 4294C0 (la mitad del tamano de la escena, PS2 0x1156c0): el uso lleva (w / 2 - 320, h / 2 - 240)
    # y la demo pasa la x / y del original sin el 4294C0 (hud.c: escena centrada en (320, 240))
    desp = {}
    for nombre, k, _, bk in escenas:
        if not (nombre.startswith('HA_CARA') or nombre.startswith('HA_H')): continue
        am = ag if bk == 0x2b else a2c
        sc = am.escenas[k]; nc, pc = struct.unpack_from('<2I', am.a, sc); raiz = pc + (nc - 1) * 0x24
        w, h = struct.unpack_from("<hh", am.a, raiz); desp[nombre] = (int(round(w * 0.5 - 320)), int(round(h * 0.5 - 240)))
    for nombre, k, fotos, bk in escenas:
        am = ag if bk == 0x2b else a2c
        lst = []; ant = None
        for i, f in enumerate(fotos):
            ps = ch.escena(am, k, float(f))
            firma = [(p['tex'], tuple(np.round(p['M'].ravel(), 3)), round(p['alfa'], 3), tuple(p['src'])) for p in ps]
            if firma != ant: lst.append([i, ps, 1]); ant = firma
            else: lst[-1][2] += 1
        grupos.append((nombre, len(fotos), lst, bk))
    # un atlas por grupo (sus texturas se suben solo mientras se ve: la VRAM fuera de la zona del curso es ~75 KB), VQ
    hud.vq = lambda i, a, fmt, lista: ch.vq_pequeno(i, a, fmt, lista, 'HAHUD: ')
    # los corazones y la rotura comparten casi todo (resplandores 2B:0..3 y 92; cada color solo su icono de 30x30): un
    # atlas para todos (se suben juntos al empezar la carrera; VRAM fuera de la zona del curso)
    COR = ('HA_CORAZON', 'HA_ROTO') + tuple('HA_C%d' % i for i in range(6))
    at_cor = None
    for nombre, nf, lst, bk in grupos:
        am, Tm = (ag, Tg) if bk == 0x2b else (a2c, T2c)
        if nombre in COR and at_cor is not None:
            at = at_cor
        else:
            at = ch.Atlas(); at.prefijo = 'ha_' + nombre; cl = []
            for n2, _, l2, b2 in (grupos if nombre in COR else [(nombre, nf, lst, bk)]):
                if n2 not in COR and nombre in COR: continue
                for _, ps, _ in l2:
                    for p in ps:
                        if p['alfa'] >= 0.004 and abs(np.linalg.det(p['M'][:2, :2])) >= 1e-6:
                            at.registra(b2, Tm, p); cl.append(at._clave(b2, p))
            t0 = len(hud.tex); pl = at.plan(cl); at.empaqueta(hud, pl)
            print('  HAHUD: %s texturas %d..%d, %d KB sin VQ' % ('corazones' if nombre in COR else nombre, t0, len(hud.tex) - 1,
                                                               at.bytes_plan(pl) >> 10))
            if nombre in COR: at_cor = at
        g = hud.grupo(nombre, nf)
        for c, ps, n in lst:
            k0 = len(hud.pie); hud.piezas_escena(g, c, bk, Tm, ps, atlas=at)
            vis = [p for p in ps if p['alfa'] >= 0.004 and abs(np.linalg.det(p['M'][:2, :2])) >= 1e-6]
            assert len(vis) == len(hud.pie) - k0
            for j, p in zip(range(k0, len(hud.pie)), vis):  # la pieza vale n cuadros (hud.c: flags >> 4); bit 2: la capa
                q = list(hud.pie[j])                        # SPRANI lleva el 4 en +0x48 (resplandor que se suma)
                q[3] = (q[3] & 11) | (4 if am.a[p['L'] + 0x48] & 4 else 0) | (min(n, 4095) << 4); hud.pie[j] = tuple(q)
        hud.uso(nombre, g, *desp.get(nombre, (0, 0)), capa=4)
    hud.escribe(os.path.join(sal, 'HAHUD.DAT'))
    print('HAHUD.DAT: %d bytes, %d texturas' % (os.path.getsize(os.path.join(sal, 'HAHUD.DAT')), len(hud.tex)))

    # VOCESH.DAT
    irx = cs.fichero_paquete(iso, 'SYSTEM.PS2', '\\IRX\\SOUNDCD.IRX')
    tb, td, _ = cs.elf_seccion_irx(irx)
    se = lambda i: struct.unpack_from('<HH', irx, td + cs.IRX_SE - tb + 12 * i)
    muestras = {}
    def banco(d, desp):
        A_, B_, (ta, tbb) = cs.banco_tablas(d)
        for datos, t in ((A_, ta), (B_, tbb)):
            for i, o in t.items():
                p, v = se(i)
                if not (0x100 <= p <= 0x3fff) or v == 0: continue
                if i + desp not in muestras: muestras[i + desp] = (cs.muestra_banco(datos, o), cs.frecuencia(p), v)
    for nb in ('HC.PAK', 'PERH.pak'):
        banco(cs.fichero_paquete(iso, 'SOUND.PS2', '\\SPANISH\\' + nb), 0)
    vistos = set()
    for k in range(15):
        if grupo[k] in vistos: continue
        vistos.add(grupo[k]); banco(datos_b[k], 0x200 * (grupo[k] + 1))
    filas = []; blob = bytearray()
    for i in sorted(muestras):
        pcm, f, v = muestras[i]
        while len(blob) % 32: blob.append(0)
        datos = cs.enc_yamaha(pcm)
        assert len(pcm) < 65536, 'voz 0x%x demasiado larga para el AICA' % i
        filas.append(struct.pack('<HH4I', i, v, len(blob), len(datos), f, len(pcm)))
        blob += datos
    off_m = 32; off_t = off_m + 20 * len(filas); off_d = (off_t + 31) & ~31
    out = bytearray(b'VOZ1' + struct.pack('<5I', len(filas), off_m, off_t, off_d, off_d + len(blob)) +
                    b'SPANISH\0')
    out += b''.join(filas); out += bytes(off_d - len(out)) + blob
    open(os.path.join(sal, 'VOCESH.DAT'), 'wb').write(out)
    print('VOCESH.DAT: %d bytes, %d muestras, %.1f s' % (len(out), len(filas),
          sum(len(m[0]) / m[1] for m in muestras.values())))

if __name__ == '__main__':
    main()
