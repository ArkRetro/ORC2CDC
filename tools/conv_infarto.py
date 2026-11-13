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
        hud.uso(nombre, g, 0, 0, capa=4)
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
