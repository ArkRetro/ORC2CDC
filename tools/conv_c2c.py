#!/usr/bin/env python3
"""conv_c2c.py <iso> <dir_salida>: las pantallas de COAST 2 COAST del original (categorias, mapa de misiones y detalle
de la mision; variante 6) -> C2C.DAT para comun/menu.c. Sale de tu disco: no se distribuye.

C2C.DAT: el banco SPRANI 0x44 (ani_SPRANI_MENU) recortado a las escenas que usan estas pantallas (conv_sprani.escribe
con solo=True: la animacion entera son 1,1 MB; con las 22 escenas, ~43 KB), para el reproductor comun/sprani.c, mas:
 - el uso 'F14': los glifos de la fuente 14 (la de los textos de estas pantallas: 0x117320(0xe) en 0x242d48 y el
   cuadro de texto 0x26cd40(0xe) en 0x2436c0) que usan sus textos, un cuadro por caracter (codigo - primero) con el
   glifo ya desplazado -metrica[0] como el FUENTE de MENU.DAT (las metricas e interletraje son los de MNU2: la misma
   fuente), en una textura propia sin VQ;
 - el bloque 'C2C1' (extra de SPR1) con las tablas del ELF de SLES-53998 (cada una con su direccion; las del PC al
   lado: outrun2-decomp platform/frontend_categories.cpp 4E8130.., frontend_missions.cpp 4E9160.., tablas 5CE3A0..
   5CE718 y 69E2F4..69E878: los mismos valores con otros tokens) y los textos (TEXT.PS2 del idioma, codigos de la
   fuente). Formato (little endian, todo u32 / i32 / f32 salvo los textos):
     'C2C1', u32 desplazamiento de los textos (desde 'C2C1')
     categorias (pantalla del suceso 0x3a, PS2 0x242340..0x242fe8):
       desc sel[7] (0x417a28), disp[7] (0x4177e8), bloq[7] (0x417848), foto_d[7] (0x417968), foto_b[7] (0x4179c8),
         pie_d[7] (0x4178a8), pie_b[7] (0x417908)       (desc = {token, primero, ultimo})
       f32 pos[7][2] (0x417b08), i32 vecinos[7][4] (0x417a88: arriba, abajo, derecha, izquierda), u32 token de la foto
         de los grupos 0-3 (0x4400cf: lui / ori 0x242c48 / 0x242c50)
       u32 texto normal[7] (0x375358), completo[7] (0x375378), bloqueado[7] (0x375338), rango[8] (0x3753b8): desplaz.
         en los textos (0: ninguno)
     mapa y detalle (suceso 0x3b, PS2 0x242ff0..0x2443b0):
       u32 cuenta[4] (0x417df8), base[4] (0x417e08)
       desc fondo[4] (0x417bc8), encima[4] (0x417bf8), tipo[7] (0x417c28), boton[4] (0x417ca0), pie[4] (0x417cd0),
         nodo[4] (0x417d00), panel[4] (0x417d30), elegida[5] (0x417d60), nota[7] (0x417da0; la 8.a, nota 7, es el token
         0: nada)
       u32 rotulo[7] (0x417c80 -> TEXT.PS2: desplaz. en los textos)
       f32 carrera[5][2] (0x3753e0), nota_xy[5][2] (0x375408), resalta[4][15][2] (0x375930 ->), nodo_xy[4][15][2]
         (0x375940 ->); i32 vecinos[4][15][4] (0x375950 ->: arriba, abajo, derecha, izquierda; -1 ninguno)
       u32 token del cursor (0x375960 = 0x4400d8, cuadros 0..0x34: 0x2430c0), token del nombre (0x4400d6, lui / ori
         0x244240 / 0x244268) y su primer cuadro (categoria + 0x1e, 0x243d50); f32 su posicion (-83, 148: 0x24424c /
         0x244258)
     peticiones (los campeonatos de los grupos 4..6, suceso 0x3c; PS2 0x240d68 inicio, 0x240e20 control, 0x241228
     dibujo, 0x240bc8 cursor = 4EA330 / 4EA3C0 / 4EA610 / 4EA210 del PC, outrun2-decomp platform/frontend_requests.cpp:
     el mismo codigo con otros tokens y los textos una posicion antes), bloque 'REQ1' detras del mapa:
       'REQ1', u32 tokens[9]: fondo 0x4400cc, encima 0x4400cd, nodo 0x4400df, casilla 0x4400d5, icono 0x4400d3, panel
         0x4400d2, titulo 0x4400d6, lista 0x4400e1 (lui / ori del dibujo) y cursor ([0x375328])
       f32 cursor[16][2] (0x4170b0), etapa[16][2] (0x417130: la etapa i usa la i + 1), rotulo[3][2] (0x4171b0),
         casilla[8][2] (0x3752e8: 4 casillas y 4 notas, en pantalla: el dibujo resta (320, 240))
       i32 vecinos[16][4] (0x4171c8: arriba, abajo, derecha, izquierda), u32 resultados[16] (0x4172c8, por nodo)
       desc listas[4] (0x417308), notas[7] (0x417338), pies[3] (0x417480), titulos[3] (0x4174b0), tit_lista[4] (0x4174e0)
       u32 iconos[30] (0x417390), cursos[30] (0x417408)
       u8 tipo[3][15] (+0x1c de los registros de 0x78 de Request_Course_SP / _OR2 / Req_Course_MIX_PS2: 0x62a880), 3 x 0
       u32 textos: inicio (0x461), con resultado (0x462), sin el (0x380) y los 33 rotulos de peticion (0x417510)
       u8 pet[60][16]: por tipo de curso (su guion Req_*.bin, nombres 0x374010), las cuentas de REQUEST_NORMAL /
         SPECIAL1..3 (4 bytes; 0xff sin guion) y el rotulo de cada peticion (4 x 3 bytes: 0x240a08 con su tipo y subtipo)
     textos: cadenas de bytes (codigos de la fuente 14, '\\n' = salto de linea) acabadas en 0
Valores que van en el codigo (las instrucciones se comprueban aqui): texto de la categoria en (65, 210) (0x242d80 /
0x242d78) color 0xFF3F474A (0x242d64), rango en (520, 363) (0x242e34 / 0x242e2c) centrado (0x242e3c) color 0xFF4D5559
(0x242e20); cuadro de texto del detalle en (300, 330) (0x2436e4 / 0x2436f0), rectangulo (300, 330, 570, 370)
(0x243700..0x24370c), color 0xFF3F474A (0x2436d4), lineas de 14 (0x26be94 / 0x26beb8, +0x4a0)."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_menu as cm
import conv_sprani as cs

BANCO = 0x44
CATEG = [0xce, 0xcf, 0xd0, 0xd1, 0xf1]
MAPA = [0xca, 0xcb, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xf0]
PETIC = [0xcc, 0xcd, 0xe1]                                  # las demas escenas de las peticiones ya van en MAPA
LISTAS_REQ = ['REQUEST_NORMAL', 'REQUEST_SPECIAL1', 'REQUEST_SPECIAL2', 'REQUEST_SPECIAL3']   # 0x374138
FUENTE = 14


def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__.split('\n')[0])
    D = ch.Disco(sys.argv[1]); sal = sys.argv[2]; os.makedirs(sal, exist_ok=True)
    E = D.elf
    u = lambda a: struct.unpack_from('<I', E, a - 0xfff80)[0]
    i = lambda a: struct.unpack_from('<i', E, a - 0xfff80)[0]
    f = lambda a: struct.unpack_from('<f', E, a - 0xfff80)[0]
    def lui_ori(a, b):
        x, y = u(a), u(b); assert x >> 26 == 15 and y >> 26 == 13, 'codigo inesperado en 0x%x' % a
        return (x & 0xffff) << 16 | (y & 0xffff)
    def inm(a, v):                                            # addiu rt, zero, v
        w = u(a); assert w >> 21 == 0x120 and (w & 0xffff) == (v & 0xffff), 'codigo inesperado en 0x%x: %08x' % (a, w)
    def lui(a, v):
        w = u(a); assert w >> 26 == 15 and (w & 0xffff) == v, 'codigo inesperado en 0x%x: %08x' % (a, w)
    # las direcciones de las tablas, de su par lui / addiu en el codigo
    def par(a, b):
        x, y = u(a), u(b); assert x >> 26 == 15 and y >> 26 == 9, 'codigo inesperado en 0x%x / 0x%x' % (a, b)
        return ((x & 0xffff) << 16) + (y & 0xffff) - (0x10000 if y & 0x8000 else 0)
    assert par(0x242364, 0x242368) == 0x417a28 and par(0x242acc, 0x242adc) == 0x4177e8
    assert par(0x242ac4, 0x242ad4) == 0x417848 and par(0x242aec, 0x242b04) == 0x417b08
    assert par(0x242c84, 0x242c94) == 0x417968 and par(0x242ce0, 0x242cf0) == 0x4179c8
    assert par(0x242e64, 0x242e74) == 0x4178a8 and par(0x242ef8, 0x242f04) == 0x417908
    assert par(0x2429d4, 0x2429dc) == 0x417a88
    assert par(0x243c04, 0x243c0c) == 0x417e08 and par(0x243c64, 0x243c70) == 0x417bc8
    assert par(0x243ccc, 0x243cd4) == 0x417bf8 and par(0x243d48, 0x243d4c) == 0x417df8
    assert par(0x244090, 0x24409c) == 0x417c30 and par(0x243f54, 0x243f64) == 0x417ca0
    assert par(0x2442f0, 0x2442f8) == 0x417cd0 and par(0x243de8, 0x243df4) == 0x417d00
    assert par(0x244294, 0x2442a0) == 0x417d30 and par(0x243178, 0x243184) == 0x417d60
    assert par(0x244128, 0x244130) == 0x417da0 and par(0x244350, 0x244354) == 0x417c80
    assert par(0x244110, 0x244118) == 0x375408 and par(0x244000, 0x244010) == 0x3753e0
    assert par(0x243258, 0x243264) == 0x375950 and par(0x243da0, 0x243da8) == 0x375940
    assert par(0x243064, 0x24306c) == 0x375930
    assert par(0x242df0, 0x242df4) == 0x375358 and par(0x242db8, 0x242dc0) == 0x375378
    assert par(0x242ec0, 0x242ec4) == 0x375338 and par(0x242e40, 0x242e48) == 0x3753b8
    # inmediatos que van en el codigo de menu.c
    inm(0x242d80, 0x41); inm(0x242d78, 0xd2); inm(0x242e34, 0x208); inm(0x242e2c, 0x16b); inm(0x242d44, 0xe)
    lui(0x242d64, 0xff3f); lui(0x242e20, 0xff4d); inm(0x2436c4, 0xe); inm(0x243700, 0x12c); inm(0x243704, 0x14a)
    inm(0x243708, 0x23a); inm(0x24370c, 0x172); lui(0x2436e4, 0x4396); lui(0x2436f0, 0x43a5); inm(0x26be94, 0xe)
    inm(0x2430c0, 0x34)
    assert u(0x243d50) == 0x27c4001e, 'nombre: categoria + 0x1e'      # addiu a0, fp, 0x1e
    lui(0x24424c, 0xc2a6); lui(0x244258, 0x4314)

    # el mapa lee la nota de la carrera i de la categoria c en la licencia +0x1DD + 5 c + i (0x241ed0, 0x243a68) y la
    # carrera guarda la de la mision (clave, subclave) en +0x1D7 + 5 clave + subclave (495A20 = 0x1f81cc): las dos
    # coinciden porque la clave de la categoria c (RACE_MAPPING_ARRAY de RaceAssignment.bin) es c + 1
    from ps2.assets import Assets
    As = Assets(sys.argv[1])
    a_ = As.datos([x for x in As.busca('*RaceAssignment.bin') if x.paquete.startswith('SCRIPTS')][0])
    na, oa = cm._r077(a_, 'RACE_MAPPING_ARRAY'); assert na == 40
    assert [struct.unpack_from('<I', a_, oa + 8 * k)[0] for k in range(na)] == list(range(1, 41)), 'claves de categoria'

    out = bytearray(b'C2C1') + bytes(4)
    W = lambda *v: out.extend(struct.pack('<%dI' % len(v), *[x & 0xffffffff for x in v]))
    F = lambda *v: out.extend(struct.pack('<%df' % len(v), *v))
    def desc(a, n):
        for k in range(n): W(u(a + 12 * k), u(a + 12 * k + 4), u(a + 12 * k + 8))
    cad = cm.textos(D, os.environ.get('OR2_IDIOMA', 'es'))
    usados = set(); ref = []                                 # (posicion en out, cadena)
    def texto(s):
        o = len(out); W(0); ref.append((o, s)); usados.update(s)
    # ---- categorias
    for a in (0x417a28, 0x4177e8, 0x417848, 0x417968, 0x4179c8, 0x4178a8, 0x417908): desc(a, 7)
    assert u(0x417a28) == 0x4400d0 and u(0x4177e8) == 0x4400d1 and u(0x4178a8) == 0x4400ce and u(0x417968 + 48) == 0x4400f1
    for k in range(7): F(f(0x417b08 + 8 * k), f(0x417b0c + 8 * k))
    vec = [[i(0x417a88 + 16 * s + 4 * k) for k in range(4)] for s in range(7)]
    assert vec[0] == [-1, 4, 1, -1], vec
    for v in vec: W(*v)
    tf = lui_ori(0x242c48, 0x242c50); assert tf == 0x4400cf; W(tf)
    for a in (0x375358, 0x375378, 0x375338):
        for k in range(7):
            t = i(a + 4 * k)
            if t > 0: texto(cad[t])
            else: W(0)
    for k in range(8):
        p = u(0x3753b8 + 4 * k); s = bytes(E[p - 0xfff80:p - 0xfff80 + 8]).split(b'\0')[0]
        if s: texto(s)
        else: W(0)
    # ---- mapa
    cuenta = [u(0x417df8 + 4 * k) for k in range(4)]; base = [u(0x417e08 + 4 * k) for k in range(4)]
    assert cuenta == [5, 9, 11, 15] and base == [0, 5, 14, 25]
    W(*cuenta); W(*base)
    for a, n in ((0x417bc8, 4), (0x417bf8, 4), (0x417c28, 7), (0x417ca0, 4), (0x417cd0, 4), (0x417d00, 4), (0x417d30, 4),
                 (0x417d60, 5), (0x417da0, 7)):
        desc(a, n)
    assert u(0x417da0 + 84) == 0, 'nota 7: token 0'
    for k in range(7): texto(cad[u(0x417c80 + 4 * k)])
    for k in range(5): F(f(0x3753e0 + 8 * k), f(0x3753e4 + 8 * k))
    for k in range(5): F(f(0x375408 + 8 * k), f(0x37540c + 8 * k))
    for tb in (0x375930, 0x375940):
        for g in range(4):
            p = u(tb + 4 * g)
            for k in range(15): F(*((f(p + 8 * k), f(p + 8 * k + 4)) if k < cuenta[g] else (0.0, 0.0)))
    for g in range(4):
        p = u(0x375950 + 4 * g)
        for k in range(15): W(*([i(p + 16 * k + 4 * q) for q in range(4)] if k < cuenta[g] else [-1] * 4))
    assert u(0x375960) == 0x4400d8 and u(0x375930) == 0x375430 and u(0x375940) == 0x375570 and u(0x375950) == 0x3756b0
    tn = lui_ori(0x244240, 0x244268); assert tn == 0x4400d6
    W(u(0x375960), tn, 0x1e); F(-83.0, 148.0)
    # ---- peticiones (campeonatos)
    assert par(0x240c0c, 0x240c14) == 0x4170b0 and par(0x2413fc, 0x241404) == 0x417130
    assert par(0x241ac8, 0x241ad4) == 0x4171b0 and par(0x2410c8, 0x2410cc) == 0x4171c8
    assert par(0x2415c4, 0x2415cc) == 0x4172c8 and par(0x240c74, 0x240c80) == 0x417308
    assert par(0x241400, 0x241408) == 0x417338 and par(0x2419d4, 0x2419dc) == 0x417480
    assert par(0x241960, 0x241968) == 0x4174b0 and par(0x24189c, 0x2418a8) == 0x4174e0
    assert par(0x2415e8, 0x2415ec) == 0x417390 and par(0x2418f8, 0x241900) == 0x417408
    assert par(0x241acc, 0x241ad0) == 0x417510 and par(0x240ce0, 0x240ce4) == 0x3752e8
    assert par(0x240e90, 0x240e98) == 0x62a880 and par(0x240a0c, 0x240a1c) == 0x374138
    out.extend(b'REQ1')
    tk = [lui_ori(a, b) for a, b in ((0x241354, 0x24135c), (0x2413b8, 0x2413c4), (0x2414a0, 0x2414c8), (0x24162c, 0x24163c),
                                     (0x241688, 0x241690), (0x24180c, 0x241814), (0x24190c, 0x24192c), (0x241a90, 0x241a98))]
    assert tk == [0x4400cc, 0x4400cd, 0x4400df, 0x4400d5, 0x4400d3, 0x4400d2, 0x4400d6, 0x4400e1], [hex(x) for x in tk]
    assert u(0x375328) == 0x4400d8; W(*tk, u(0x375328))
    inm(0x240c44, 0x34); inm(0x241644, 3); inm(0x241780, 5); inm(0x241818, 3)   # cursor 0..0x34; casilla 3 / 5; panel 3
    lui(0x2418ac, 0xc2a6); lui(0x2418b8, 0x4314)             # titulo en (-83, 148)
    inm(0x241b50, 0x461); inm(0x241bf4, 0x462); inm(0x241bf0, 0x380)
    for a, n in ((0x4170b0, 16), (0x417130, 16), (0x4171b0, 3), (0x3752e8, 8)):
        for k in range(n): F(f(a + 8 * k), f(a + 8 * k + 4))
    vec = [[i(0x4171c8 + 16 * s + 4 * k) for k in range(4)] for s in range(16)]
    assert vec[0] == [-1, -1, 1, -1] and vec[15] == [14, -1, -1, 10], vec
    for v in vec: W(*v)
    res = [u(0x4172c8 + 4 * k) for k in range(16)]            # por nodo: el 0 (la salida) no tiene; la etapa i, el i + 1
    assert res == [0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4]; W(*res)
    for a, n in ((0x417308, 4), (0x417338, 7), (0x417480, 3), (0x4174b0, 3), (0x4174e0, 4)): desc(a, n)
    assert u(0x417308) == 0x4400d4 and u(0x417338) == 0x4400e0 and u(0x417480) == 0x4400f0 and u(0x4174b0) == 0x4400d6
    W(*[u(0x417390 + 4 * k) for k in range(30)]); W(*[u(0x417408 + 4 * k) for k in range(30)])
    # por el hash del nombre: Req_Desert_r.bin esta en el paquete pero no en la lista de nombres
    from ps2.assets import hash_nombre
    por_hash = {x.hash: x for x in As.paquete('SCRIPTS.PS2')}
    guion = lambda n_: bytes(As.datos(por_hash[hash_nombre('\\bin\\%s.bin' % n_)]))
    tipos = []
    for n_ in cm.CURSOS_REQ:
        d_ = guion(n_); nn, oo = cm._r077(d_, n_); assert nn == 15, n_
        tipos.append([struct.unpack_from('<I', d_, oo + 0x78 * k + 0x1c)[0] for k in range(15)])
    assert all(t < 60 for l in tipos for t in l), tipos
    for l in tipos: out.extend(bytes(l))
    out.extend(bytes(3))
    for t in (0x461, 0x462, 0x380): texto(cad[t])
    for k in range(33): texto(cad[u(0x417510 + 4 * k)])
    # el rotulo de una peticion: el trozo de 0x240a08 que mira su tipo (v0) y subtipo (a1), ejecutado tal cual
    from ps2 import mips
    def rotulo(tipo, sub):
        m = mips.Mips(As.elf); m.r[2] = tipo; m.r[5] = sub; m.r[29] = m.PILA - 0x100; m.r[31] = 0xDEAD0000
        m.escribe(m.r[29] + 8, struct.pack('<Q', 0xDEAD0000))
        m._corre(0x240a58, 2000); return m.r[2] & 0xffffffff
    assert u(0x240a58) == 0x2444ffff and rotulo(1, 0) == 0xd and rotulo(16, 4) == 0x1a and rotulo(5, 2) == 0xb
    nombres = []; a = 0x374010
    assert u(0x374138) and par(0x240a0c, 0x240a1c) - a == 4 * 74, 'tabla de nombres de los guiones'
    for k in range(60):
        p = u(a + 4 * k); nombres.append(bytes(E[p - 0xfff80:p - 0xfff80 + 40]).split(b'\0')[0].decode())
    assert nombres[0] == 'Req_Palm_Beach' and nombres[30] == 'Req_Palm_Beach_r' and nombres[59] == 'Req_Easter_Island_r', nombres
    usadas = set(t for l in tipos for t in l)
    for k in range(60):
        r_ = bytearray(b'\xff' * 16)
        if hash_nombre('\\bin\\%s.bin' % nombres[k]) in por_hash:
            d_ = guion(nombres[k]); nc, nr = struct.unpack_from('<2I', d_, 0)
            rel = dict(struct.unpack_from('<2I', d_, 8 + 8 * q) for q in range(nr))
            for li, nom in enumerate(LISTAS_REQ):
                try: c_, o_ = cm._r077(d_, nom)
                except SystemExit: c_, o_ = 0, 0
                assert c_ <= 3, (nombres[k], nom, c_); r_[li] = c_
                for q in range(c_):
                    t_ = rel[o_ + 0x74 * q + 0x58]; r_[4 + 3 * li + q] = rotulo(*struct.unpack_from('<2I', d_, t_))
                    assert r_[4 + 3 * li + q] < 33
        else: assert k not in usadas, 'falta el guion %s' % nombres[k]
        out.extend(r_)
    print('  peticiones: tipos %s' % tipos)
    # ---- textos (el primer byte es un 0: el desplazamiento 0 queda para "ninguno")
    t0 = len(out); struct.pack_into('<I', out, 4, t0)
    pool = bytearray(b'\0')
    for o, s_ in ref: struct.pack_into('<I', out, o, t0 + len(pool)); pool += s_ + b'\0'
    out += pool; out += bytes((-len(out)) % 4)
    for o, s_ in ref:
        q = struct.unpack_from('<I', out, o)[0]; assert bytes(out[q:q + len(s_) + 1]) == s_ + b'\0'

    # ---- glifos de la fuente 14 (sin VQ)
    Ft = cm.fuentes_xst(D); Fu = cm.Fuente(D, FUENTE, Ft)
    usados.discard(0x20); usados.discard(0x0a)
    chars = sorted(c for c in usados if 0 <= c - Fu.pri < Fu.n)
    falta = sorted(c for c in usados if not 0 <= c - Fu.pri < Fu.n); assert not falta, falta
    sin_vq = set()
    def antes(hud):
        por = 256 // Fu.w; filas = (len(chars) + por - 1) // por
        img = np.zeros((filas * Fu.h, por * Fu.w, 4), np.uint8)
        for j, c in enumerate(chars):
            k = c - Fu.pri; l, tp = (k & 15) * Fu.w, (k >> 4) * Fu.h
            x, y = (j % por) * Fu.w, (j // por) * Fu.h
            img[y:y + Fu.h, x:x + Fu.w] = Ft[FUENTE][tp:tp + Fu.h, l:l + Fu.w]
        ti = hud.textura(('C2C', 'F14'), img); sin_vq.add(ti)
        g = hud.grupo('F14', Fu.n)
        for j, c in enumerate(chars):
            x, y = (j % por) * Fu.w, (j // por) * Fu.h
            si = hud.sprite(ti, (x, y, x + Fu.w, y + Fu.h), Fu.w, Fu.h)
            hud.pie.append((g, c - Fu.pri, si, 0, float(-Fu.met[c - Fu.pri][0]), 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0))
        hud.uso('F14', g, 0, 0)
        print('  F14: %d glifos en una textura de %dx%d' % (len(chars), img.shape[1], img.shape[0]))
    vq = lambda k, a, fmt, lista: None if k in sin_vq else ch.vq_pequeno(k, a, fmt, lista)
    ruta = os.path.join(sal, 'C2C.DAT')
    cs.escribe(D, BANCO, ruta, [('CATEGORIAS', CATEG), ('MAPA', MAPA), ('PETICIONES', PETIC)], bytes(out), vq=vq, solo=True, antes=antes)
    print('C2C.DAT: %d bytes (tablas C2C1 %d bytes, %d textos)' % (os.path.getsize(ruta), len(out), len(ref)))


if __name__ == '__main__':
    main()
