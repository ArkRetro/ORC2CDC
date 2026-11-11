#!/usr/bin/env python3
"""conv_sp.py <iso> <dir_salida> [dir_previas]: graficos del juego de OUTRUN2SP ([0x59acac] = 0) fuera del frontal de
recreativa -> CARGASP0..3.DAT (pantalla de carga, una por preset) para demo/sp.c y GOVSP.DAT (GAME OVER) para demo/fin.c; y SPFE.DAT, el
frontal de recreativa (banco 0x2E SELECTOR_CVT entero para comun/sprani.c, ver conv_sprani.py, y las tablas de
arcade_attract.cpp en el bloque FSP1, ver tablas_fe) para comun/fsp.c.

Formato: el de HUD.DAT (tools/conv_hud.py; comun/hud.c lo abre con hudd_abre, parcial: en la RAM solo las tablas), como
FIN.DAT de conv_fin.py: un atlas por grupo (sus texturas se suben solo mientras se ven) y cada pieza guarda en
flags >> 4 los cuadros que abarca (solo se escriben los cuadros donde algo cambia); bit 2 = capa aditiva (+0x48 & 4).

Origen (SLES-53998):
 - Carga (0x1b8228, desde START al acabar de leer el curso, 0x1b64ac; PC 49DE4A -> 499CC0): fuera de la consola
   (0x1b4878 = 0) y de las variantes 3..5 (tabla de saltos 0x3f9720), 114d00(token, 2, 0, 0, 0x77): la escena del
   preset (gp-0x7b64 = 78024C) del banco 0x2F (LOADING_CVT, leido en 0x1b645c): preset 0 -> 2F0002, 1 -> 2F0003,
   2 -> 2F0000, 3 -> 2F0001 (los lui / ori / movz de 0x1b827c..0x1b82b4), capa 2, modo 0 (en bucle), cuadros 0..0x77
   -> grupo CARGA_Pn (120 cuadros) con un uso del mismo nombre en (0, 0), en CARGASPn.DAT (un fichero por preset: en la
   RAM solo las piezas del que se ve).
 - GAME OVER (modo 27, 0x1bafc8): el GAD (0x1a04f0) crea con 1b4878 = 0 y 450240 != 7 la escena 34001F (lui / ori de
   0x1a0520 / 0x1a052c; banco 0x34 ROUTE_CVT, leido por el modo 25: PC 49A790 42DEB0(0x34, 9)) en la capa 0, modo 1 (se
   queda en el ultimo cuadro); el modo dura 0xF0 pasos (0x1bafe0: 1b4a10(0xF0)) -> grupo GAME_OVER_SP (cuadros 0..0xEF),
   aparte en GOVSP.DAT (solo se abre en ese modo).
Las escenas y las texturas son las del banco del idioma (OR2_IDIOMA, espanol por defecto: XST _sxst).
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_fin as cf
import conv_sprani as cs

def hud_nuevo():
    hud = ch.Hud(); hud.taco = [0xff] * 16; hud.vq = ch.vq_pequeno
    return hud

def grupo_escena(hud, nombre, A, T, bk, k, n=None, x=0, y=0, capa=1):
    """ch.grupo_escena (cuadros 0..n-1, todos los de la escena por defecto) con un uso del mismo nombre en (x, y)"""
    g = ch.grupo_escena(hud, nombre, A, T, bk, k, cf.duracion(A, k) if n is None else n, 'sp_')
    hud.uso(nombre, g, x, y, capa=capa)
    return g

def carga(D, sal):
    elf = D.elf
    # 0x1b827c..0x1b82b4: s0 = 2F0002 (preset 0) / 2F0003 (1) / 2F0000 (2), a0 = 2F0001 (3)
    t2 = cf.lui_ori(elf, 0x1b827c, 0x1b8284); t3 = cf.lui_ori(elf, 0x1b8280, 0x1b8290)
    t0 = cf.u32(elf, 0x1b8298); t1 = cf.lui_ori(elf, 0x1b82a0, 0x1b82ac)
    assert t0 >> 26 == 0xf and (t2, t3, t1) == (0x2f0002, 0x2f0003, 0x2f0001), [hex(x) for x in (t2, t3, t1, t0)]
    t0 = (t0 & 0xffff) << 16
    w = cf.u32(elf, 0x1b82c4); assert w >> 26 == 9 and w & 0xffff == 0x77, hex(w)   # addiu t0, zero, 0x77
    toks = [t2, t3, t0, t1]
    A, T, _ = D.banco(0x2f)
    huds = []
    for p, t in enumerate(toks):
        assert t >> 16 == 0x2f
        hud = hud_nuevo()
        grupo_escena(hud, 'CARGA_P%d' % p, A, T, 0x2f, t & 0xffff, n=0x78)
        f = os.path.join(sal, 'CARGASP%d.DAT' % p); hud.escribe(f)
        print('CARGASP%d.DAT: %d bytes, %d texturas, %d piezas' % (p, os.path.getsize(f), len(hud.tex), len(hud.pie)))
        huds.append(hud)
    return huds

def game_over(D, sal):
    elf = D.elf
    t = cf.lui_ori(elf, 0x1a0520, 0x1a052c); assert t == 0x34001f, hex(t)
    w = cf.u32(elf, 0x1bafe4); assert w == 0x240400f0, hex(w)             # addiu a0, zero, 0xF0 (1b4a10)
    n = w & 0xffff
    A, T, _ = D.banco(t >> 16)
    hud = hud_nuevo()
    grupo_escena(hud, 'GAME_OVER_SP', A, T, t >> 16, t & 0xffff, n=n)
    f = os.path.join(sal, 'GOVSP.DAT'); hud.escribe(f)
    print('GOVSP.DAT: %d bytes, %d texturas, %d piezas' % (os.path.getsize(f), len(hud.tex), len(hud.pie)))
    return hud

# ---------------------------------------------------------------- frontal de recreativa (SELECTOR_CVT)
# Grupos de atlas del banco 0x2E: lo que se ve junto (conv_sprani: un rectangulo va en el del primer grupo que lo usa)
GRUPOS_FE = [('FONDO', [0x45, 0x46, 0x4e]), ('PANEL', [0x00, 0x05, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16]),
             ('COCHE', [0x17, 0x18, 0x19, 0x1a, 0x41, 0x42, 0x43, 0x44]), ('CAMBIO', [0x4f]),
             ('MODO', [0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26]),
             ('CONFIG', list(range(0x2b, 0x41))), ('MUSICA', [0x27, 0x28, 0x29, 0x2a, 0x50])]

def tablas_fe(D):
    """tablas del frontal de recreativa (arcade_attract.cpp) en el ELF de SLES-53998, con la direccion del PC al lado;
    bloque 'FSP1' que lee comun/fsp.c (todo u32 salvo los float marcados)"""
    elf = D.elf; u = lambda a: cf.u32(elf, a); f = lambda a: cf.f32(elf, a)
    out = bytearray(b'FSP1')
    W = lambda *v: out.extend(struct.pack('<%dI' % len(v), *v))
    F = lambda *v: out.extend(struct.pack('<%df' % len(v), *v))
    # 5C7170: 564 filas {token, primero, ultimo} (la secuencia 5C8BE0 va justo detras)
    assert (u(0x410c30), u(0x410c34), u(0x410c38)) == (0x2e0042, 1, 30)
    for i in range(564): W(u(0x410c30 + 12 * i), u(0x410c34 + 12 * i), u(0x410c38 + 12 * i))
    # 5C8BE0: la secuencia {0xB, 5, 6, -1, 4, 2, 8, 9, 0xA, 0}
    sec = [u(0x4126a0 + 4 * i) for i in range(10)]; assert sec[:4] == [0xb, 5, 6, 0xffffffff], sec; W(*sec)
    # [6898D8]: la tabla de sonidos (PS2 [0x373654] = 0x4126c8), 64 entradas
    ps = u(0x373654); assert ps == 0x4126c8; W(*[u(ps + 4 * i) for i in range(64)])
    # 5C8D00: 16 registros de la musica {id, pista del avance, filas +8 / +C / +10 / +14}
    for i in range(16):
        p = u(0x4127c0 + 4 * i); W(*[u(p + 4 * k) for k in range(6)])
    # 5C91DC: 10 coches {modelo, 9 filas}; 5C9204: sus colores (hasta 15 {argb, nombre, indice}, fin en argb 0)
    for i in range(10):
        p = u(0x412800 + 4 * i); W(*[u(p + 4 * k) for k in range(10)])
    for i in range(10):
        p = u(0x412828 + 4 * i); n = 0; idx = []
        while u(p + 12 * n): idx.append(u(p + 12 * n + 8)); n += 1
        assert 0 < n <= 15; W(n, *(idx + [0] * (15 - n)))
    W(*[u(0x412850 + 4 * i) for i in range(10)])          # 5C922C: rotulo de cada coche (2E000A..)
    W(*[u(0x410b68 + 4 * i) for i in range(10)])          # 5C70A8: color de partida (pagina 0)
    W(*[u(0x410950 + 4 * i) for i in range(10)])          # 5C9408: entrada de la tabla de sonidos del coche
    for b in (0x373938, 0x373a78):                        # 689B28 / 689C68: configuracion del coche (CONTRARRELOJ)
        assert u(b) == 0x1a4 or b != 0x373938
        W(*[u(b + 4 * k) for k in range(80)])
    assert u(0x373bb8) == 0x109 and u(0x373c60) == 0x2e001c
    W(*[u(0x373bb8 + 4 * k) for k in range(42)])           # 689DA8: modo de juego, 2 lados x 0x54
    W(*[u(0x373c60 + 4 * k) for k in range(8)])            # 689E50 cursores, 689E58, 689E60..6C
    F(*[f(0x373c80 + 4 * k) for k in range(4)])            # 689E70 / 74 / 78 / 7C (float)
    W(*[u(0x373d70 + 4 * k) for k in range(13)])           # 689F58: filas del fondo 2E0045
    assert u(0x373d70) == 0x17b
    W(*[struct.unpack_from('<H', elf, 0x412920 - 0xfff80 + 2 * k)[0] for k in range(11)])   # 5C9334 (u16)
    W(*[u(0x373890 + 4 * k) for k in range(8)])            # 689AAC / 689ABC
    F(f(0x3738b0), f(0x3738b4))                            # 689ACC / 689AD0
    W(*[u(0x3738b8 + 4 * k) for k in range(20)])           # 689AD4 modelos, 689AFC
    F(f(0x373860), f(0x373864), f(0x373868), f(0x373874), f(0x37387c), f(0x373dbc))   # 689AA0/A4/A8, FC4, FBC, FA0
    W(elf[0x373878 - 0xfff80], elf[0x373879 - 0xfff80], u(0x373dc4), u(0x373dd0), u(0x373dd4))  # FC0, 84A9FC, FA4, FB8, FB4
    # barra del color (213540..213590: 429530(2E0000, f x 4,37 + 563, 406 - f x 12, 0xA, 0))
    k = lambda a, b: struct.unpack('<f', struct.pack('<I', cf.lui_ori(elf, a, b)))[0]
    F(k(0x213554, 0x213558), k(0x213560, 0x213564), struct.unpack('<f', struct.pack('<I', (u(0x213570) & 0xffff) << 16))[0],
      struct.unpack('<f', struct.pack('<I', (u(0x213544) & 0xffff) << 16))[0])
    return bytes(out)

def frontal(D, sal):
    ruta = os.path.join(sal, 'SPFE.DAT')
    cs.escribe(D, 0x2e, ruta, GRUPOS_FE, tablas_fe(D), vq=ch.vq_pequeno)

# ---------------------------------------------------------------- resultados (modo 25: mapa de la ruta, ROUTE_CVT)
# Grupos de atlas del banco 0x34: el mapa del preset 0 / 2 (34000E / 3401A) y el del 1 / 3 (34000D / 340019), el rotulo
# (34001D), el tramo y la marca (34001B / 34001C), los coches del mapa (34000F..340018 y 340017, el de por defecto); las
# escenas de la Contrarreloj de presets 2 / 3 (340000..34000C, funcion 0x50) y el GAME OVER (34001F, GOVSP.DAT) van en
# RESTO (los presets 2 / 3 no salen: ver PARIDAD, [0x54c108])
GRUPOS_RUTA = [('MAPA_SP', [0x0e, 0x1a]), ('MAPA_OR2', [0x0d, 0x19]), ('RUTA', [0x1d, 0x1b, 0x1c]),
               ('COCHES', list(range(0x0f, 0x19)))]

def tablas_ruta(D):
    """tablas del mapa de la ruta (PC 4AE750 / 4AE960 de race_end_modes.cpp = PS2 0x209208 / 0x209518, funcion 0x15 del
    suceso 0x188; fila 0x14 de la tabla de funciones 0x3ee7b4) en el ELF de SLES-53998; bloque 'RSP1' que lee
    demo/finsp.c"""
    elf = D.elf; u = lambda a: cf.u32(elf, a); f = lambda a: cf.f32(elf, a)
    fn = 0x3ee7b4 + 0x14 * 0x14
    assert (u(fn), u(fn + 4), u(fn + 0x10)) == (0x209208, 0x209518, 0x2099f8), [hex(u(fn + 4 * k)) for k in range(5)]
    # 0x209280..0x2092ac: fila = [0x40fc10 + coche x 4] (lui 0x41 / addiu -0x3f0), token = [0x40fe68 + (fila - 1) x 0x78 +
    # coche x 4] (addiu -0x198, mult 0x78); sin coche (o 1bcfd0 = 0) [0x40fe68] = 340017
    assert u(0x209284) & 0xffff == 0xfc10 and u(0x209298) & 0xffff == 0xfe68 and u(0x209288) & 0xffff == 0x78
    out = bytearray(b'RSP1'); W = lambda *v: out.extend(struct.pack('<%dI' % len(v), *v))
    fila = [u(0x40fc10 + 4 * k) for k in range(30)]
    assert 1 <= min(fila) and max(fila) <= 3 and u(0x40fe68) == 0x340017
    W(*fila)
    W(*[u(0x40fe68 + 4 * k) for k in range(90)])                       # 3 filas de 30 tokens
    # nodos {de, a, siguiente 0, siguiente 1} (0x40fc88 / 8c / 90 / 94: s4 = 0x41 - 0x378...) y puntos (0x40fd70, x, y)
    assert u(0x209318) & 0xffff == (-0x378) & 0xffff and u(0x20934c) & 0xffff == (-8) & 0xffff
    W(*[u(0x40fc88 + 4 * k) for k in range(60)])                       # 15 nodos
    out.extend(struct.pack('<62f', *[f(0x40fd70 + 4 * k) for k in range(62)]))   # 31 puntos
    # constantes en el codigo: -10000 (0x2092c4: lui 0xc61c, ori 0x4000) y -9000 (0x209554: lui 0xc60c, ori 0xa000)
    assert cf.lui_ori(elf, 0x2092c4, 0x2092c8) == 0xc61c4000 and cf.lui_ori(elf, 0x209554, 0x209558) == 0xc60ca000
    return bytes(out)

def rutas(D, sal):
    cs.escribe(D, 0x34, os.path.join(sal, 'RUTASP.DAT'), GRUPOS_RUTA, tablas_ruta(D), vq=ch.vq_pequeno)

# ---------------------------------------------------------------- entrada de nombre (modo 26) y rankings de OUTRUN2SP
# PS2 (SLES-53998): modo 26 = 0x1ba968 / 0x1baaf8 / 0x1babe8 (tabla de modos 0x3f8e50); suceso 0x189 funcion 0x16 = fila
# 0x15 de la tabla 0x3ee7b4: 0x1fc560 init / 0x1fcd30 control / 0x1fd1e8 dibujo / 0x1fd528 fin (PC 4B2370 / 4B29E0 /
# 4B4F20 / 4B0950 de race_name_entry.cpp); el tablero 0x200398 (PC 4B46F0). Modulo en 0x5da108.. (PC 8423C0..), tablas de
# rankings 0x5d7548..0x5da108 (0x2BC0 bytes, PC 84DF40..850B00 en otro orden): variante 1 presets 0 / 1 0x5d7548, variante 2
# 0x5d7b88, variante 0 presets 0 / 1 0x5d81c8, variante 1 presets 2 / 3 0x5d9ac8, variante 0 presets 2 / 3 0x5d9c08;
# registros de 16 bytes {+0 puntos 24 | ruta << 24 | coche << 28, +4 tiempo, +8 cuenta 12 | coche de la licencia << 12 |
# cambio << 17 | color << 18 (24 bits), +0xB nombre (4)}.
GRUPOS_RANK = [('TABLA', [0x15, 0x16, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23] + list(range(0x02, 0x0c))),
               ('FILAS', list(range(0x0c, 0x11)) + [0x11, 0x14, 0x1a, 0x24, 0x25, 0x26, 0x27, 0x28])]
NOM_TEXTOS = (0x443, 0x444, 0x445)          # 0x239cd8 (465EB0): TIEMPO / PUNTUACION / CORAZONES (0x200ddc..0x200e10)
NOM_FUENTES = (3, 4)                        # 0x117320(3) nombres y cifras, (4) el rotulo de fuera del ranking

def tablas_nombre(D, A):
    """bloque 'NOM1' (comun/rksp.c y demo/nombre.c): las tablas de la entrada de nombre del ELF, los registros por defecto
    de los rankings (la rutina del PS2 0x1fa3b0 ejecutada con ps2/mips.py), las de 0x1ff740 / 0x2002b8 evaluadas y los
    descriptores (44DC50) de las claves de los arboles 0 y 1"""
    from ps2.mips import Mips
    import conv_union, conv_menu
    elf = D.elf; u = lambda a: cf.u32(elf, a); f = lambda a: cf.f32(elf, a)
    E = A.elf
    cad = lambda a: elf[a - 0xfff80:elf.index(b'\0', a - 0xfff80)]
    # comprobaciones de las direcciones en el codigo (lui 0x41 / addiu)
    lo = lambda a: u(a) & 0xffff
    assert lo(0x1fc5cc) == (0x40d248 - 0x410000) & 0xffff and lo(0x1fca6c) == (0x40d338 - 0x410000) & 0xffff
    assert lo(0x1fc830) == (0x40e040 - 0x410000) & 0xffff and lo(0x2003bc) == (0x40daa0 - 0x410000) & 0xffff
    assert lo(0x2008a8) == (0x40da88 - 0x410000) & 0xffff and lo(0x200b10) == (0x40da10 - 0x410000) & 0xffff
    assert lo(0x200c5c) == (0x40cfc8 - 0x410000) & 0xffff and lo(0x200810) == (0x40e028 - 0x410000) & 0xffff
    assert lo(0x200930) == (0x40dc80 - 0x410000) & 0xffff and lo(0x20093c) == (0x40ddb0 - 0x410000) & 0xffff
    assert lo(0x2016d8) == (0x40cfd8 - 0x410000) & 0xffff and lo(0x1fcb28) == (0x40c838 - 0x410000) & 0xffff
    assert lo(0x1fce10) == (0x40c840 - 0x410000) & 0xffff and lo(0x1fce1c) == (0x40c848 - 0x410000) & 0xffff
    assert lo(0x1fced0) == (0x40c860 - 0x410000) & 0xffff and lo(0x1fce44) == (0x36f4b8 - 0x370000) & 0xffff
    assert u(0x1fd0b0) == 0x240300f0                                   # 0x8426e4 = 0xF0 (estado 5)
    assert cad(0x40c8f0) == b'%2d' and cad(0x40c8b0) == b'%02d' and cad(0x40c8f8) == b'%03d'
    assert cad(0x40c900) == b'%7d' and cad(0x40c908) == b'9999999' and cad(0x40cfd0) == b'%s'
    assert u(0x200d44) == 0x24060193 and u(0x200da4) == 0x24050144 and u(0x200da8) == 0x24040032 and u(0x200db0) == 0x240501a3 and u(0x200d80) == 0x240601a3   # (0, 0x193), (0x144 / 0x32, 0x1a3)
    out = bytearray(b'NOM1'); W = lambda *v: out.extend(struct.pack('<%dI' % len(v), *v))
    F = lambda *v: out.extend(struct.pack('<%df' % len(v), *v))
    # registros por defecto: 0x1fa3b0 sin la lectura del libro (0x1fc080) ni su comprobacion (0x1b4640): tablas a cero
    m = Mips(E); stub = struct.pack('<2I', 0x03e00008, 0x24020000)
    m.escribe(0x1fc080, stub); m.escribe(0x1b4640, stub); m.llama(0x1fa3b0)
    defe = m.lee(0x5d7548, 0x2bc0)
    assert defe[11:15] == b'AM2\0' and struct.unpack_from('<I', defe, 4)[0] == 290000, defe[:16]
    out += defe
    # 0x1ff740(ruta, nivel) (PC 4B1680) para ruta 0..15, nivel 0..4; 0x2002b8(c) (el filtro de 4B1F40) para c 0..255
    for r in range(16):
        out += bytes(m.llama(0x1ff740, r, n) & 0xff for n in range(5))
    out += bytes(m.llama(0x2002b8, c - 256 if c >= 128 else c) & 0xff for c in range(256))
    W(*[u(0x40d248 + 4 * k) for k in range(60)])                       # fondo por descriptor (428320, capa 0)
    W(*[u(0x40e040 + 4 * k) for k in range(12)])                       # 0x34c = [0x40e040 + (cambio + color x 2 + variante x 4) x 4]
    for k in range(24):                                                # t = 0x40daa0 + (0x34c + preset x 6) x 16
        w = [u(0x40daa0 + 16 * k + 4 * q) for q in range(4)]
        W(w[0], w[1], *[u(w[3] + 4 * q) for q in range(7)])            # fondo, titulo y las lineas (+0xC)
    W(*[u(0x40da88 + 4 * k) for k in range(5)])                        # escena de la fila del puesto (puesto % 5)
    W(*[u(0x40da10 + 4 * k) for k in range(10)])                       # cuadro de 3D0024 por el coche (+0x1a)
    W(u(0x40cfc8), u(0x40cfcc))                                        # cuadro de 3D0028 por el color (+0x1c)
    F(*[f(0x40e028 + 4 * k) for k in range(5)])                        # y de las 5 3D0016
    for b in (0x40dc80, 0x40ddb0):                                     # filas: 5 x 0x3c bytes
        W(*[u(b + 4 * k) for k in range(75)])
    W(*[u(0x40cfd8 + 4 * k) for k in range(10)])                       # estilos de cifras {escena, paso}
    out += elf[0x40c838 - 0xfff80:0x40c838 - 0xfff80 + 8]              # nombre de la fila del jugador (lw + lh: 6 bytes)
    for a in (0x40c840, 0x40c848, 0x40c860):                           # quitar, recortar, sustituto
        s_ = cad(a); assert len(s_) < 32; out += s_ + bytes(32 - len(s_))
    pal = []; k = 0
    while u(0x36f4b8 + 4 * k): pal.append(cad(u(0x36f4b8 + 4 * k))); k += 1
    blq = b''.join(x + b'\0' for x in pal); blq += bytes((-len(blq)) % 4)
    W(len(pal), len(blq)); out += blq
    txt = conv_menu.textos(D, os.environ.get('OR2_IDIOMA', 'es'))
    for i in NOM_TEXTOS:
        s_ = txt[i]; assert len(s_) < 32; out += s_ + bytes(32 - len(s_))
    # fuentes: {id, ancho, alto, primer caracter, n, espaciado x, y} (0x117320: +0xc / +0xe / +0x1c / +0x14 / +0x18)
    for fid in NOM_FUENTES:
        reg = next(q for q in D.elf32(conv_menu.FUENTES, 15) if q and D.elf32(q)[0] == fid)
        w = D.elf32(reg, 8); i32 = lambda v: v - (1 << 32) if v >> 31 else v
        W(fid, w[3] & 0xffff, w[3] >> 16, w[4] & 0xffff, w[7] & 0xffff, i32(w[5]) & 0xffffffff, i32(w[6]) & 0xffffffff)
    # 44DC50 de cada registro (el +0 del descriptor = su indice; registro +0x1c) de los arboles 0 (csc_data_2) y 1
    # (csc_data_cvt), en el orden de los registros (el de CURSO.DAT, conv_union.cursos)
    base, nprim, _ = conv_union.tablas(A)
    for fich in ('csc_data_2.bin', 'csc_data_cvt.bin'):
        regs = conv_union.arbol(A, 0, fich); d = [0xffffffff] * 16
        for i, (k, ti, ui, hi, hd) in enumerate(regs):
            assert i < 16 and cf.u32(elf, cf.u32(elf, base + 4 * ti)) == ti; d[i] = ti
        print('  %s: %s' % (fich, d[:len(regs)]))
        W(*d)
    print('NOM1: %d bytes, %d palabras' % (len(out), len(pal)))
    return bytes(out)

def nombre(D, sal):
    from ps2.assets import Assets
    import conv_menu
    A = Assets(sys.argv[1])
    cs.escribe(D, 0x3d, os.path.join(sal, 'RANKSP.DAT'), GRUPOS_RANK, tablas_nombre(D, A), vq=ch.vq_pequeno)
    for b, l in zip(range(0x37, 0x3c), 'ABCDE'):                        # NAME5A..E: el fondo (escenas 0xC / 0xD)
        cs.escribe(D, b, os.path.join(sal, 'NOM5%s.DAT' % l), [('FONDO', [0x0c, 0x0d])], vq=ch.vq_pequeno)
    # NOMTX.DAT: los glifos de las fuentes 3 y 4 (un cuadro por caracter, la celda entera en (0, 0): el printf de la
    # entrada de nombre, 0x1175a8 -> 0x116fa0, no usa las metricas) y la imagen compuesta 0x150010 (spr_ranking_xst,
    # 117d48: sus partes {imagen, dx, dy})
    hud = hud_nuevo(); T = conv_menu.fuentes_xst(D)
    txt = conv_menu.textos(D, os.environ.get('OR2_IDIOMA', 'es'))
    for fid in NOM_FUENTES:
        reg = next(q for q in D.elf32(conv_menu.FUENTES, 15) if q and D.elf32(q)[0] == fid)
        w = D.elf32(reg, 8); cw, chh, pri, n = w[3] & 0xffff, w[3] >> 16, w[4] & 0xffff, w[7] & 0xffff
        usados = set(range(0x20, 0x80)) if fid == 3 else set(c for i in NOM_TEXTOS for c in txt[i])
        g = hud.grupo('F%d' % fid, n)
        for k in range(n):
            c = pri + k
            if c not in usados or c == 0x20 or c >= 0x80: continue      # 0x116fa0: signo (>= 0x80 no se dibuja), espacio
            l, tp = (k & 15) * cw, (k >> 4) * chh
            p = dict(tex=fid, src=(l, tp, l + cw, tp + chh), w=cw, h=chh, M=ch._t(0, 0), alfa=1.0, anc=(0.0, 0.0), gira=False)
            hud.piezas_escena(g, k, 'FUENTE', T, [p])
        hud.uso('F%d' % fid, g, 0, 0)
    ix = D.n_spr.index('spr_ranking_xst.sz'); xst = D._ent(D.p_spr, ix); TX = ch.texturas(xst); H = xst[4:]
    nc, tab = struct.unpack_from('<2I', H, 0x10); assert nc > 0x10
    c, pp = struct.unpack_from('<2I', H, tab + 8 * 0x10)
    g = hud.grupo('BANDA', 1)
    for j in range(c):
        im, dx, dy = struct.unpack_from('<I2h', H, pp + 16 * j)
        hud.piezas_imagen(g, 'RANKX', TX, xst, im, dx, dy)
    hud.uso('BANDA', g, 0, 0)
    fn = os.path.join(sal, 'NOMTX.DAT'); hud.escribe(fn)
    print('NOMTX.DAT: %d bytes, %d texturas, %d piezas' % (os.path.getsize(fn), len(hud.tex), len(hud.pie)))

def maquetas(huds, d):
    from PIL import Image
    os.makedirs(d, exist_ok=True)
    for p, hud in enumerate(huds):
        img = Image.new('RGBA', (640, 480), (0, 0, 0, 255))
        u = next(u for u in hud.lay if u[0] == 'CARGA_P%d' % p); ch.dibuja_grupo(img, hud, u[1], 60, u[3], u[4])
        img.save(os.path.join(d, 'carga_p%d.png' % p))

if __name__ == '__main__':
    if len(sys.argv) < 3: raise SystemExit(__doc__)
    D = ch.Disco(sys.argv[1])
    h = carga(D, sys.argv[2])
    if len(sys.argv) > 3: maquetas(h, sys.argv[3])
    game_over(D, sys.argv[2])
    frontal(D, sys.argv[2])
    rutas(D, sys.argv[2])
    nombre(D, sys.argv[2])
