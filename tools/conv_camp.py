#!/usr/bin/env python3
"""conv_camp.py <iso> <dir_salida>: los campeonatos de COAST 2 COAST (variante 5, las peticiones de la acompanante en
los grupos 4..6 de las categorias) -> CAMP.DAT para comun/campeonato.c. Sale de tu disco: no se distribuye.

Del original (SLES-53998): el gestor es un suceso (0x190: inicio 0x229e88, control 0x22a048, dibujo 0x22a2b0, fin
0x22b470; objeto en 0x616238) que por cada etapa lee el guion Req_<etapa>.bin de SCRIPTS.PS2 (nombres 0x374010 por el
tipo de la etapa, listas REQUEST_NORMAL / SPECIAL1..3 de 0x374138, 0x22ac00 / 0x22b600): cada peticion es un
registro de 0x74 bytes {s16 inicio, s16 fin (posiciones de carretera de la etapa), ..., +0x58 puntero a sus
parametros}; los parametros (bloques de 0x88 con su propio nombre en el guion) {+0 tipo 1..24, +4 subtipo, +0x5c
puntero a los coches de la peticion, +0x60.. enteros / flotantes que lee el tipo, p. ej. +0x7c la velocidad del tipo
1 o el % del rebufo del tipo 12, +0x80 / +0x84 los coches del tipo 3}. Aqui no se copian guiones ni tablas al codigo:
se sacan del disco y del ELF (cada tabla con su direccion, comprobada por el codigo que la lee).

CAMP.DAT (little endian):
  'CMP1', u32 version 1, u32 n_pet, u32 desplazamiento de las peticiones
  f32 umbral[7]   (0x374190: nota k si el medidor menos la penalizacion >= umbral[k]; 0x22e670)
  i32 tope        (0x3741c8: tope de los corazones de la peticion; 0x22e728)
  i32 extra_lista[4] (0x3741d0: corazones de mas con nota >= 4 por la lista de la etapa)
  i32 extra_dif[5]   (0x3741e0: por la dificultad +0xc de la licencia, tope 4)
  i32 millas[7]      (0x3741f8: millas por la nota de la etapa, x (lista + 1); 0x22edd8)
  f32 penal[17][2][6] (0x374240: penalizaciones por tipo / subtipo {choque, golpe de uno mas rapido, golpe, muro x
                       dt, rueda fuera x dt, roce x dt}; 0x2323e8 elige la fila, 0x22c818 las suma)
  u16 voz_nota[4][7] (0x423df0: voz del fin de la peticion por fila +0xf0 del gestor y nota; 0x2a65a8)
  u16 voz_animo[2][5] (0x423ee0 tipos 1 y 2, 0x423ef8 los demas; 0x2a6240)
  u8  rot_nota[7]   (0x22f7a0: texto de la nota, indice HA_Tnn de HAHUD.DAT (tabla 0x3f0aa8) o 0xff)
  u8  marco_nota[2] (2B001F con nota < 4, 2B001E con 4 o mas: indice HA_Mn de 0x3f0b40)
  u8  marco_anuncio (2B0020), u8 rot_generico (2B003D), u8 0
  u16 lista[60][4]  (por tipo de etapa (0x374010) y lista: primera peticion << 4 | cuenta; 0xffff sin guion)
  u8  arbol[3][15]  (el tipo de la etapa de cada registro de Request_Course_SP / _OR2 / Req_Course_MIX_PS2)
  peticiones de 52 bytes: s16 inicio, s16 fin, u8 tipo, u8 subtipo, u8 rotulo (HA_Tnn del anuncio, 0x22faec; 0xff
     ninguno), u8 banco46 (1: el rotulo es del banco 0x46, que no esta en la DC), u16 voz del anuncio (0x2a6358; 0 ninguna),
     u16 0, i32 p[10] (+0x60..+0x84 de sus parametros)
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_menu as cm

LISTAS = ['REQUEST_NORMAL', 'REQUEST_SPECIAL1', 'REQUEST_SPECIAL2', 'REQUEST_SPECIAL3']   # 0x374138


def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    D = ch.Disco(sys.argv[1]); sal = sys.argv[2]; os.makedirs(sal, exist_ok=True)
    E = D.elf
    u = lambda a: struct.unpack_from('<I', E, a - 0xfff80)[0]
    i_ = lambda a: struct.unpack_from('<i', E, a - 0xfff80)[0]
    f = lambda a: struct.unpack_from('<f', E, a - 0xfff80)[0]
    def par(a, b):
        x, y = u(a), u(b); assert x >> 26 == 15 and y >> 26 == 9, 'codigo inesperado en 0x%x / 0x%x' % (a, b)
        return ((x & 0xffff) << 16) + (y & 0xffff) - (0x10000 if y & 0x8000 else 0)
    # las tablas, por el codigo que las lee
    assert par(0x22e674, 0x22e688) == 0x374190, 'umbrales (0x22e670)'
    assert u(0x22e74c) >> 16 == 0x3c03 and u(0x22e754) & 0xffff == 0x41c8, 'tope (0x22e728)'
    assert par(0x22e77c, 0x22e784) == 0x3741d0 and par(0x22e7ac, 0x22e7b4) == 0x3741e0, 'extras (0x22e728)'
    assert par(0x22ee10, 0x22ee20) == 0x3741f8, 'millas (0x22edd8)'
    assert par(0x232518, 0x23251c) == 0x374240, 'penalizaciones (0x2323e8)'
    assert par(0x2a6668, 0x2a6670) == 0x423df0, 'voces de la nota (0x2a65a8)'
    assert par(0x2a62e8, 0x2a62f0) == 0x423ee0 and par(0x2a6310, 0x2a6318) == 0x423ef8, 'voces de animo (0x2a6240)'
    assert par(0x22ac94, 0x22ac98) == 0x374010 and par(0x22b63c, 0x22b648) == 0x374138, 'nombres (0x22ac00 / 0x22b600)'
    assert u(0x3741a8) == 0x42a00000, 'medidor lleno 80 (0x3741a8)'
    out = bytearray(b'CMP1') + struct.pack('<3I', 1, 0, 0)
    W = lambda fmt, *v: out.extend(struct.pack('<' + fmt, *v))
    W('7f', *[f(0x374190 + 4 * k) for k in range(7)])
    W('i', i_(0x3741c8)); W('4i', *[i_(0x3741d0 + 4 * k) for k in range(4)]); W('5i', *[i_(0x3741e0 + 4 * k) for k in range(5)])
    W('7i', *[i_(0x3741f8 + 4 * k) for k in range(7)])
    W('204f', *[f(0x374240 + 4 * k) for k in range(204)])
    W('28H', *[u(0x423df0 + 4 * k) for k in range(28)])
    W('10H', *([u(0x423ee0 + 4 * k) for k in range(5)] + [u(0x423ef8 + 4 * k) for k in range(5)]))
    # rotulos: los del banco 0x2B que convierte conv_infarto.py en HAHUD.DAT (HA_Tnn = tabla 0x3f0aa8, HA_Mn = 0x3f0b40)
    tt = [u(0x3f0aa8 + 4 * k) for k in range(37)]; tm = [u(0x3f0b40 + 4 * k) for k in range(3)]
    ti = lambda tok: tt.index(tok) if tok in tt else 0xff
    from ps2 import mips
    from ps2.assets import Assets, hash_nombre
    As = Assets(sys.argv[1])
    def corre(pc, regs, paradas, mem=None):
        m = mips.Mips(As.elf)
        for a, k in paradas.items(): m.escribe(a, struct.pack('<2I', 0x03e00008, 0x24030000 | k))   # jr ra; addiu v1, zero, k
        for k, v in regs.items(): m.r[k] = v
        if mem:
            for a, d in mem.items(): m.escribe(a, d)
        m.r[29] = m.PILA - 0x100; m.r[31] = 0xDEAD0000
        m._corre(pc, 20000); return m
    # el texto de la nota (0x22f7a0: el switch de 0x415590 deja el token en v0)
    assert u(0x22f8c0) & 0xffff == 0x5590
    rn = []
    for nota in range(7):
        m = corre(0x22f8b0, {16: nota}, {0x22f95c: 1, 0x22f964: 0}); rn.append(ti(m.r[2] & 0xffffffff) if m.r[3] == 1 else 0xff)
    W('7B', *rn); W('2B', tm.index(0x2b001f), tm.index(0x2b001e)); W('3B', tm.index(0x2b0020), ti(0x2b003d), 0)
    print('  notas: textos %s' % rn)
    # los guiones
    por_hash = {x.hash: x for x in As.paquete('SCRIPTS.PS2')}
    nombres = []
    for k in range(60):
        p = u(0x374010 + 4 * k); nombres.append(bytes(E[p - 0xfff80:p - 0xfff80 + 40]).split(b'\0')[0].decode())
    assert nombres[0] == 'Req_Palm_Beach' and nombres[59] == 'Req_Easter_Island_r'
    usadas = set()
    for n_ in cm.CURSOS_REQ:
        d_ = bytes(As.datos(por_hash[hash_nombre('\\bin\\%s.bin' % n_)])); nn, oo = cm._r077(d_, n_)
        usadas.update(struct.unpack_from('<I', d_, oo + 0x78 * k + 0x1c)[0] for k in range(nn))
    PAR = mips.Mips.PILA - 0x8000                            # parametros en la memoria del interprete (la pila)
    lista = []; pets = bytearray(); npet = 0; tipos = {}
    for k in range(60):
        h_ = hash_nombre('\\bin\\%s.bin' % nombres[k])
        if h_ not in por_hash: assert k not in usadas, nombres[k]; lista += [0xffff] * 4; continue
        d = bytes(As.datos(por_hash[h_])); nc, nr = struct.unpack_from('<2I', d, 0)
        rel = dict(struct.unpack_from('<2I', d, 8 + 8 * q) for q in range(nr))
        for li, nom in enumerate(LISTAS):
            try: c_, o_ = cm._r077(d, nom)
            except SystemExit: c_, o_ = 0, 0
            assert c_ <= 3 and npet < 0x1000; lista.append(npet << 4 | c_)
            for q in range(c_):
                r = o_ + 0x74 * q; ini, fin = struct.unpack_from('<2h', d, r)
                pp = rel[r + 0x58]; blk = d[pp:pp + 0x88]; tipo, sub = struct.unpack_from('<2I', blk, 0)
                assert 1 <= tipo <= 24 and 0 <= ini < fin, (nombres[k], q, tipo, ini, fin)
                # el rotulo del anuncio (0x22f9d0: switch 0x4155b0 por el tipo, deja el token en a1 y el banco en a2)
                m = corre(0x22faec, {19: tipo, 16: sub, 17: PAR}, {0x22fe08: 0, 0x22fe10: 1}, {PAR: blk})
                tok = m.r[5] & 0xffffffff if m.r[3] == 1 else 0
                # la voz del anuncio (0x2a6358: switch 0x423d70 por el tipo, a0 = la voz en 0x2a6548)
                try:
                    mv = corre(0x2a636c, {16: PAR}, {0x2a6548: 1, 0x2a656c: 0, 0x2a6570: 0}, {PAR: blk})
                    voz = mv.r[4] & 0xffff if mv.r[3] == 1 else 0
                except RuntimeError:                          # (la del tipo que la elige al azar: 0x21d3c8, sin portar)
                    voz = 0xffff
                rot = ti(tok) if tok >> 16 == 0x2b else 0xff
                pets.extend(struct.pack('<2h4B2H10i', ini, fin, tipo, sub, rot, 1 if tok >> 16 == 0x46 else 0, voz, 0,
                                        *struct.unpack_from('<10i', blk, 0x60)))
                tipos.setdefault(tipo, set()).add((sub, hex(tok), hex(voz)))
                npet += 1
    W('240H', *lista)
    # los tipos de las etapas de los tres arboles (grupos 4, 5, 6: registros de 0x78, +0x1c el descriptor de tramo; el
    # nodo del mapa i es el registro i - 1). Request_Course_OR2 es el arbol del modo OutRun (csc_data_2) y
    # Request_Course_SP el de OUTRUN2SP (csc_data_cvt, el 1 de CURSO.DAT): la carrera los usa
    arb = []
    for n_ in cm.CURSOS_REQ:
        d_ = bytes(As.datos(por_hash[hash_nombre('\\bin\\%s.bin' % n_)])); nn, oo = cm._r077(d_, n_); assert nn == 15
        arb.append([struct.unpack_from('<30i', d_, oo + 0x78 * k) for k in range(15)])
        W('15B', *[r[7] for r in arb[-1]])
    d2 = bytes(As.datos([x for x in As.busca('*csc_data_2.bin') if x.paquete.startswith('SCRIPTS')][0]))
    n2, o2 = cm._r077(d2, 'csc_data_2_course'); assert n2 == 15
    c2 = [struct.unpack_from('<30i', d2, o2 + 0x78 * k) for k in range(15)]
    # (las 5 metas de Request_Course_OR2 llevan el hijo izquierdo 0 y el derecho -1; las de csc_data_2, -1 y -1: la
    # carrera de la DC acaba en la meta como el modo OutRun, PROBABLE que el 0 no se use)
    q = lambda r: (r[1], r[7], r[11], r[12]) if r[12] >= 0 else (r[1], r[7], -1, -1)
    assert [q(r) for r in c2] == [q(r) for r in arb[1]], 'Request_Course_OR2 no es csc_data_2'
    d1 = bytes(As.datos([x for x in As.busca('*csc_data_cvt.bin') if x.paquete.startswith('SCRIPTS')][0]))
    n1, o1 = cm._r077(d1, 'csc_data_cvt_course'); assert n1 == 15
    c1 = [struct.unpack_from('<30i', d1, o1 + 0x78 * k) for k in range(15)]
    assert [q(r) for r in c1] == [q(r) for r in arb[0]], 'Request_Course_SP no es csc_data_cvt'
    while len(out) % 4: out.append(0)
    struct.pack_into('<2I', out, 8, npet, len(out)); out += pets
    open(os.path.join(sal, 'CAMP.DAT'), 'wb').write(out)
    for t in sorted(tipos): print('  tipo %2d: %s' % (t, sorted(tipos[t])))
    print('CAMP.DAT: %d peticiones, %d bytes' % (npet, len(out)))


if __name__ == '__main__':
    main()
