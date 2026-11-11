#!/usr/bin/env python3
"""conv_sp.py <iso> <dir_salida> [dir_previas]: graficos del juego de OUTRUN2SP ([0x59acac] = 0) fuera del frontal de
recreativa -> CARGASP.DAT (pantalla de carga) para demo/sp.c y GOVSP.DAT (GAME OVER) para demo/fin.c; y SPFE.DAT, el
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
   -> grupos CARGA_P0..CARGA_P3 (120 cuadros), usos con el mismo nombre en (0, 0).
 - GAME OVER (modo 27, 0x1bafc8): el GAD (0x1a04f0) crea con 1b4878 = 0 y 450240 != 7 la escena 34001F (lui / ori de
   0x1a0520 / 0x1a052c; banco 0x34 ROUTE_CVT, leido por el modo 25: PC 49A790 42DEB0(0x34, 9)) en la capa 0, modo 1 (se
   queda en el ultimo cuadro); el modo dura 0xF0 pasos (0x1bafe0: 1b4a10(0xF0)) -> grupo GAME_OVER_SP (cuadros 0..0xEF),
   aparte en GOVSP.DAT (solo se abre en ese modo).
Las escenas y las texturas son las del banco del idioma (OR2_IDIOMA, espanol por defecto: XST _sxst).
"""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_fin as cf
import conv_sprani as cs

def vq_sp(i, a, fmt, lista):                                    # VQ si el error es pequeno (como FIN.DAT)
    import vq
    datos, err = vq.codifica(a, fmt, 256)
    print('  textura %d %dx%d VQ error %.1f %s' % (i, a.shape[1], a.shape[0], err, 'si' if err <= 16 else 'no'))
    return datos if err <= 16 else None

def hud_nuevo():
    hud = ch.Hud(); hud.taco = [0xff] * 16; hud.vq = vq_sp
    return hud

def grupo_escena(hud, nombre, A, T, bk, k, n=None, x=0, y=0, capa=1):
    """hornea la escena k del banco bk (cuadros 0..n-1, todos los de la escena por defecto) en el grupo nombre con
    su atlas y un uso del mismo nombre en (x, y)"""
    if n is None: n = cf.duracion(A, k)
    lst = []; ant = None
    for f in range(n):
        ps = [ch.hornea(p, T) if 'mascara' in p else p for p in ch.escena(A, k, float(f))]
        firma = [(str(p['tex']), tuple(np.round(p['M'].ravel(), 3)), round(p['alfa'], 3), tuple(p['src'])) for p in ps]
        if firma != ant: lst.append([f, ps, 1]); ant = firma
        else: lst[-1][2] += 1
    at = ch.Atlas(); at.prefijo = 'sp_' + nombre; cl = []
    vis = lambda ps: [p for p in ps if p['alfa'] >= 0.004 and abs(np.linalg.det(p['M'][:2, :2])) >= 1e-6]
    def grande(p):                                              # no cabe en una textura del atlas (512x256 / 256x512):
        l, t, r, b = p['src']; w, h = r - l, b - t              # va con su textura de origen entera (fondos)
        return not ((w <= 510 and h <= 254) or (w <= 254 and h <= 510))
    for _, ps, _ in lst:
        for p in vis(ps):
            if grande(p): continue
            b2, T2 = (p['bk'], p['tx']) if 'bk' in p else (bk, T)
            at.registra(b2, T2, p); cl.append(at._clave(b2, p))
    t0 = len(hud.tex); pl = at.plan(cl); at.empaqueta(hud, pl)
    print('  %s: escena %X:%04X, %d cuadros (%d distintos), texturas %d..%d, %d KB' % (
        nombre, bk, k, n, len(lst), t0, len(hud.tex) - 1, at.bytes_plan(pl) >> 10))
    g = hud.grupo(nombre, n)
    for c, ps, nf in lst:
        k0 = len(hud.pie)
        for p in vis(ps):
            b2, T2 = (p['bk'], p['tx']) if 'bk' in p else (bk, T)
            hud.piezas_escena(g, c, b2, T2, [p], atlas=None if grande(p) else at)
        v = vis(ps)
        assert len(v) == len(hud.pie) - k0
        for j, p in zip(range(k0, len(hud.pie)), v):            # cuadros que abarca y capa aditiva
            q = list(hud.pie[j]); L = p.get('L')
            adi = 4 if L is not None and A.a[L + 0x48] & 4 else 0
            q[3] = (q[3] & 11) | adi | (min(nf, 4095) << 4); hud.pie[j] = tuple(q)
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
    hud = hud_nuevo()
    for p, t in enumerate(toks):
        assert t >> 16 == 0x2f
        grupo_escena(hud, 'CARGA_P%d' % p, A, T, 0x2f, t & 0xffff, n=0x78)
    f = os.path.join(sal, 'CARGASP.DAT'); hud.escribe(f)
    print('CARGASP.DAT: %d bytes, %d texturas, %d piezas' % (os.path.getsize(f), len(hud.tex), len(hud.pie)))
    return hud

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
    cs.escribe(D, 0x2e, ruta, GRUPOS_FE, tablas_fe(D), vq=vq_sp)

def maquetas(hud, d):
    from PIL import Image
    os.makedirs(d, exist_ok=True)
    for p in range(4):
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
