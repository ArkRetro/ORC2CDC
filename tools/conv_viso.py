#!/usr/bin/env python3
"""conv_viso.py tu.iso datos/ [salida.dat]: VISO.DAT, los nodos que dibuja el original de cada etapa en su primera y
en su ultima seccion de carretera (las tablas cs_CS_<zona>_bin del segmento), para cuando la camara esta fuera de la
etapa: en la union. Sale de tu disco: no se distribuye. Lee los SEGnn.DAT de datos/ (recompuestos de SEGS.PAK).

Como lo hace el original (race_area_geometry_44de10 del PC; SLES-53998 0x172780 -> 0x1118b8 / 0x1112f8 / 0x1114e0):
la geometria de la etapa se dibuja con la lista de nodos de la seccion del coche (car+0x64); con el coche en la union
(car+0x5C != 0) la seccion es 0 si la etapa nueva ya esta cargada ([7D3064] = 1, estado 0xB de race_area_control_44e590)
y si no el final del curso de la etapa que se deja (43D470(0)). 0x1112f8 rehace la mascara de nodos del registro
(256 bytes por registro en 0x4ce7a8 + registro x 0x110, una por recurso y registro) aplicando las listas de las
secciones 0..s en orden: cada lista son u16 n y n pares (byte i, mascara) y cada par ESCRIBE el byte i de la mascara
(sb, no es una suma ni un XOR: lo que no nombra se queda como estaba); al volver atras empieza de cero. Despues pasa
la mascara a la lista de u16 de los nodos con el bit puesto (nodo = 8 i + bit) terminada en -1 (0x70001000) y
0x1114e0 dibuja esos nodos. (conv_tramo.lee_visibilidad las leia como conjuntos de nodos por seccion y no casaban:
son cambios.)

VISO.DAT: 'VSO1', u32 n; n entradas {u32 segmento, u32 nodos del ORD7, u32 desplazamiento}; en cada desplazamiento
dos mascaras de (nodos + 7) / 8 bytes (bit k del byte j = nodo 8 j + k del ORD7): seccion 0 y ultima seccion. Los
nodos del ORD7 son los del bloque principal en el orden de modelo.Modelo.bloque (registros que la tabla nombra, en
orden, y en cada uno los nodos con tiras en el orden de modelo.nodos), comprobados esfera a esfera contra el
fichero; los de despues (objetos de la etapa: la pancarta CHECK POINT y los carteles, que el original dibuja aparte)
van siempre puestos."""
import glob, os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets
import conv_tramo, modelo, pak_union, pvs

def tabla_bruta(d):
    """[registro][seccion] -> [(byte, mascara)] de una tabla cs_CS_<zona>_bin (formato de conv_tramo.lee_visibilidad)"""
    w = lambda o: struct.unpack_from('<I', d, o)[0]
    p0 = w(8); res = []
    for k in range((p0 - 8) // 4):
        p = w(8 + 4 * k); nsec = w(p) // 4; secs = []
        for sc in range(nsec):
            o = p + w(p + 4 * sc); m = struct.unpack_from('<H', d, o)[0]
            secs.append([(d[o + 2 + 2 * j], d[o + 3 + 2 * j]) for j in range(m)])
        res.append(secs)
    return res

def mascaras(t, s):
    """las mascaras de bytes de cada registro en la seccion s (0x1112f8: las listas 0..s escriben sus bytes)"""
    m = [bytearray(256) for _ in t]
    for r, secs in enumerate(t):
        for sc in range(s + 1):
            for i, mk in secs[sc]: m[r][i] = mk
    return m

def de_segmento(seg, orden):
    """(nodos del ORD7, mascara seccion 0, mascara ultima) o None"""
    blq = [b for b in seg.bloques if b.o == seg.o][0]; regs = blq.registros()
    tv = conv_tramo.visibilidad_de(seg, blq)
    if tv is None: return None
    bruta = None
    for h, o, s in seg.ent:                                    # la misma tabla que elige visibilidad_de
        d = bytes(seg.m[o:o + s])
        if conv_tramo.lee_visibilidad(d) == tv: bruta = tabla_bruta(d); break
    if bruta is None: return None
    N = orden['N']; nn = len(N); ns = len(bruta[0])
    lista = []                                                 # (registro, nodo) por nodo del ORD7, como Modelo.bloque
    for r in range(blq.w[0]):
        if not any(tv[r]): continue
        for k, fl, W, (cc, rad), tiras, con_matriz in modelo.nodos(blq, regs[r]):
            if not tiras: continue
            if fl & 0x800 and con_matriz: rad = rad + float(np.linalg.norm(W[3, :3] - cc))   # (cartel: como Modelo.bloque)
            lista.append((r, k, cc, rad))
    assert len(lista) <= nn, 'mas nodos en el bloque (%d) que en el fichero (%d)' % (len(lista), nn)
    for j, (r, k, cc, rad) in enumerate(lista):
        assert np.allclose(N['c'][j], cc, atol=1e-3) and abs(N['r'][j] - rad) < 1e-3, 'nodo %d no casa' % j
    out = []
    for s in (0, ns - 1):
        m = mascaras(bruta, s); b = np.ones(nn, bool)          # (los de despues del bloque: siempre)
        for j, (r, k, cc, rad) in enumerate(lista): b[j] = bool(m[r][k >> 3] >> (k & 7) & 1)
        out.append(np.packbits(b, bitorder='little').tobytes())
    return nn, out[0], out[1]

def main():
    iso, datos = sys.argv[1], sys.argv[2]
    sal = sys.argv[3] if len(sys.argv) > 3 else os.path.join(datos, 'VISO.DAT')
    A = Assets(iso); S = A.segmentos()
    pak = open(os.path.join(datos, 'SEGS.PAK'), 'rb').read() if os.path.exists(os.path.join(datos, 'SEGS.PAK')) else None
    ent = []
    for f in sorted(glob.glob(os.path.join(datos, 'SEG[0-9]*.DAT'))):
        n = int(os.path.basename(f)[3:-4]); b = open(f, 'rb').read()
        if b[:4] != b'ORD7': b = pak_union.recompone(b, pak)
        tmp = '/dev/shm/viso_%d.dat' % os.getpid()
        open(tmp, 'wb').write(b)
        try: d = pvs.lee(tmp)
        finally: os.remove(tmp)
        r = de_segmento(S[n], d)
        if r is None: print('SEG%02d: sin tabla de nodos por secciones' % n); continue
        nn, m0, m1 = r
        print('SEG%02d: %d nodos, seccion 0: %d, ultima: %d' % (n, nn, sum(bin(x).count('1') for x in m0), sum(bin(x).count('1') for x in m1)))
        ent.append((n, nn, m0 + m1))
    cab = b'VSO1' + struct.pack('<I', len(ent)); o = len(cab) + 12 * len(ent); cuerpo = b''
    for n, nn, m in ent:
        cab += struct.pack('<3I', n, nn, o + len(cuerpo)); cuerpo += m + b'\0' * (-len(m) % 4)
    open(sal, 'wb').write(cab + cuerpo)
    print('%s: %d etapas, %d bytes' % (sal, len(ent), len(cab) + len(cuerpo)))

if __name__ == '__main__':
    main()
