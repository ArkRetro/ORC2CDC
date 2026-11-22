#!/usr/bin/env python3
"""conv_fin.py <iso> <dir_salida> [dir_previas]: pantallas del fin de carrera (OutRun Miles, continuar y GAME OVER)
-> FIN.DAT y GOVER.DAT para demo/fin.c.

Formato: el de HUD.DAT (tools/conv_hud.py; comun/hud.c lo abre con hudd_abre, parcial: en la RAM solo las tablas). Un
atlas por grupo (sus texturas se suben solo mientras se ven) y cada pieza guarda en flags >> 4 los cuadros que abarca
(solo se escriben los cuadros donde algo cambia, como HAHUD.DAT de conv_infarto.py); bit 2 = capa aditiva (+0x48 & 4).

Origen (SLES-53998; los modos del original en la tabla 0x3f8e50, {init, control, salida, 0} por modo):
 - Modo 34 (0x22, OutRun Miles; 0x1bc270 / 0x1bc3d8 / 0x1bc618 = 49B470 / 49B570 / 49B7C0 del PC): 0x114b68 crea la
   escena 2B004C (capa 1, modo 5: se queda en su ultimo cuadro) -> grupo MILLAS (cuadros 0..80). Las cifras las dibuja
   el GAD (0x1a0c98 = 497FA0 del PC): 8 veces la escena 2B004B con 0x115768 (429530: capa 5) en el cuadro = contador de
   la cifra (0..cifra x 6) trasladada a (X - 320 - i x PASO, Y - 240) con X, Y, PASO = floats 0x35c8b8 / 0x35c8bc /
   0x35c8c0 del ELF (540, 210, 32) -> grupo MILLAS_CIFRA (cuadros 0..59) y usos MILLAS_C0..MILLAS_C7 (i = 0 las
   unidades) en esas posiciones.
 - Modo 35 (0x23, continuar; 0x1bc650 / 0x1bc700 / 0x1bc8c8 = 49B7E0 / 49D9C0 / 49DAE0): fuera de las misiones de
   Coast 2 Coast (0x1f7d80 = 4957F0) la escena 2B004D ("VOLVER A INTENTARLO?", modo 5) -> CONT_PREG (0..49) y, cuando
   acaba, 2B004A (REINTENTAR / ABANDONAR, modo 5) -> CONT_ELIGE (0..59). Los iconos de los botones del PS2 de 2B004A
   (cruz = bit 4 de 0x1b4548 = REINTENTAR, circulo = bit 8 = ABANDONAR) se cambian por los de la Dreamcast como en
   la barra de ayuda de los menus (conv_menu.iconos_dc: A y B): se reconocen comparando cada pieza con los iconos del
   PS2 de la tabla 0x374a60 (banco 0x48, cuadro 25).
 - Modo 27 (0x1b, GAME OVER; 0x1bafc8): el GAD (0x1a04f0 = 497960) crea 2C000B (capa 0, modo 1) con la consola
   (0x1b4878 = 43F860) -> GAME_OVER (cuadros de la escena entera; modo 1: se queda en el ultimo), aparte en GOVER.DAT
   (4029 piezas, ~97 KB de RAM: solo se abre en ese modo).
Las escenas y las texturas son las del banco del idioma (OR2_IDIOMA, espanol por defecto: XST _sxst).
"""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import conv_hud as ch
import conv_menu as cm

def u32(elf, va): return struct.unpack_from('<I', elf, va - 0xfff80)[0]
def f32(elf, va): return struct.unpack_from('<f', elf, va - 0xfff80)[0]
def lui_ori(elf, lui, ori):
    """token de un lui (lui) y su ori (ori) del codigo; comprueba las dos instrucciones"""
    a, b = u32(elf, lui), u32(elf, ori)
    assert a >> 26 == 0xf and b >> 26 == 0xd, 'lui/ori en %x / %x' % (lui, ori)
    return (a & 0xffff) << 16 | (b & 0xffff)

def duracion(A, k):
    sc = A.escenas[k]; nc, pc = struct.unpack_from('<2I', A.a, sc); raiz = pc + (nc - 1) * 0x24
    return int(struct.unpack_from('<f', A.a, raiz + 8)[0])

def iconos_ps2(D):
    """{boton: RGBA} de los iconos del PS2 de la barra de ayuda (tabla 0x374a60, banco 0x48, cuadro 25): 4 (cruz), 8
    (circulo) y 0x10 (el de REINTENTAR de 2B004F, la mision ya superada: 4959E0 lo lee con el bit 0x10; en la Dreamcast
    es Y, como en los menus: conv_menu.BOTON_DC)"""
    ab, Tb, _ = cm.banco_menu(D, 0x48)
    w = D.elf32(cm.AYUDA_ICONO, 16); mapa = {w[2 * i]: w[2 * i + 1] for i in range(8)}
    res = {}
    for bt in (4, 8, 0x10):
        ps = [q for q in ch.escena(ab, mapa[bt] & 0xffff, 25.0) if q['alfa'] >= 0.004]
        assert len(ps) == 1
        l, t, r, b = (int(round(v)) for v in ps[0]['src'])
        res[bt] = Tb[ps[0]['tex']][t:b, l:r]
    return res

def boton_de(p, T, ps2):
    """boton (4 / 8 / 0x10) del icono del PS2 que es la pieza p, o None"""
    from PIL import Image
    l, t, r, b = (int(round(v)) for v in p['src'])
    if not (20 <= r - l <= 40 and 20 <= b - t <= 40): return None
    a = T[p['tex']][t:b, l:r].astype(float); mejor = None
    for bt, ic in ps2.items():
        q = np.asarray(Image.fromarray(ic).resize((r - l, b - t), Image.BILINEAR)).astype(float)
        e = np.sqrt(np.mean((a - q) ** 2))
        if mejor is None or e < mejor[0]: mejor = (e, bt)
    return mejor[1] if mejor[0] < 40 else None

def convierte(iso, sal, previas=None):
    D = ch.Disco(iso); elf = D.elf
    # tokens del codigo (comprobados) y posiciones de las cifras
    t_millas = lui_ori(elf, 0x1bc3a0, 0x1bc3a8)
    t_cifra = lui_ori(elf, 0x1a0d0c, 0x1a0d24)
    t_preg = lui_ori(elf, 0x1bc670, 0x1bc6d0)
    t_elige = lui_ori(elf, 0x1bc774, 0x1bc78c)
    t_over = lui_ori(elf, 0x1a0530, 0x1a0538)
    assert (t_millas, t_cifra, t_preg, t_elige, t_over) == (0x2b004c, 0x2b004b, 0x2b004d, 0x2b004a, 0x2c000b), \
        [hex(x) for x in (t_millas, t_cifra, t_preg, t_elige, t_over)]
    X, Y, PASO = f32(elf, 0x35c8b8), f32(elf, 0x35c8bc), f32(elf, 0x35c8c0)
    assert (X, Y, PASO) == (540.0, 210.0, 32.0), (X, Y, PASO)
    ag, Tg, _ = D.banco(0x2b); ac, Tc, _ = D.banco(0x2c)
    ps2 = iconos_ps2(D)
    F = cm.Fuente(D, cm.FUENTE_AYUDA, cm.fuentes_xst(D)); dc = cm.iconos_dc(D, F)
    grupos = [('MILLAS', ag, Tg, 0x2b, t_millas & 0xffff), ('MILLAS_CIFRA', ag, Tg, 0x2b, t_cifra & 0xffff),
              ('CONT_PREG', ag, Tg, 0x2b, t_preg & 0xffff), ('CONT_ELIGE', ag, Tg, 0x2b, t_elige & 0xffff),
              ('GAME_OVER', ac, Tc, 0x2c, t_over & 0xffff)]
    cambiados = [0]; huds = {}
    def boton(p):                                               # icono del boton de la Dreamcast (A / B / Y) de CONT_ELIGE
        bt = boton_de(p, Tg, ps2) if 'bk' not in p else None
        if bt is None: return p
        img, _ = dc[bt]; cambiados[0] += 1
        q = dict(p); q.update(tex=('BOTON', bt), src=(0, 0, img.shape[1], img.shape[0]), bk='BOTON', tx={('BOTON', bt): img})
        return q
    for nombre, A, T, bk, k in grupos:
        fich = 'GOVER.DAT' if nombre == 'GAME_OVER' else 'FIN.DAT'
        if fich not in huds:
            huds[fich] = hud = ch.Hud(); hud.taco = [0xff] * 16; hud.vq = ch.vq_pequeno
        hud = huds[fich]
        g = ch.grupo_escena(hud, nombre, A, T, bk, k, duracion(A, k), 'fin_', boton if nombre == 'CONT_ELIGE' else None)
        if nombre == 'MILLAS_CIFRA':
            for i in range(8): hud.uso('MILLAS_C%d' % i, g, int(X - 320 - i * PASO), int(Y - 240), capa=5)
        else: hud.uso(nombre, g, 0, 0, capa=1)
    assert cambiados[0] > 0, 'no se encontraron los iconos de los botones en 2B004A'
    # FINC2C.DAT: el modo 35 con una mision de COAST 2 COAST (0x1bc650 / 0x1bc700, PC 49B7E0 / 49D9C0): la pregunta
    # 2B0068 (2B0069 si (tipo 1 o 495B80) y la nota 496510 >= 4 y != 7) y, cuando acaba, 2B004F si la mision ya esta
    # superada en la licencia (4959E0: nota >= 4 y != 7) o si no 2B004A (el CONT_ELIGE de FIN.DAT)
    m1, m2 = lui_ori(elf, 0x1bc6bc, 0x1bc6c8), lui_ori(elf, 0x1bc6a4, 0x1bc6b8)
    m_el = lui_ori(elf, 0x1bc774, 0x1bc784)
    assert (m1, m2, m_el) == (0x2b0068, 0x2b0069, 0x2b004f), [hex(x) for x in (m1, m2, m_el)]
    hud = huds['FINC2C.DAT'] = ch.Hud(); hud.taco = [0xff] * 16; hud.vq = ch.vq_pequeno
    for nombre, k in (('CONT_PREG_M1', m1), ('CONT_PREG_M2', m2), ('CONT_ELIGE_M', m_el)):
        g = ch.grupo_escena(hud, nombre, ag, Tg, 0x2b, k & 0xffff, duracion(ag, k & 0xffff), 'fin_',
                            boton if nombre == 'CONT_ELIGE_M' else None)
        hud.uso(nombre, g, 0, 0, capa=1)
    huds['GOALT.DAT'] = tabla(D, elf, iso)
    for fich, hud in huds.items():
        hud.escribe(os.path.join(sal, fich))
        print('%s: %d bytes, %d texturas, %d piezas' % (fich, os.path.getsize(os.path.join(sal, fich)), len(hud.tex), len(hud.pie)))
    if previas: maquetas(huds, previas)

# caracteres de la fuente 4 que escribe la tabla de la meta (formatos 0x3ef3c8 "% 2d'%02d\"%03d", 0x3ef3e0 "%02d%%",
# 0x3ef3e8 "100%%"; la puntuacion 0x19ff08: cifras): cada uno es un cuadro del grupo T_CH (demo/fin.c TABLA_CH)
TABLA_CH = b"0123456789'\"% "

def tabla(D, elf, iso):
    """GOALT.DAT: la tabla de tiempos por etapa de la meta (GAD de la meta 0x19e6d0 = 498670 del PC: en las variantes
    0/1/7/8/9, cuando la voz de GOAL acaba, 0x1a05f8 = 4979E0 y despues los paneles de tiempo y puntuacion). Del codigo
    de 0x1a05f8 (comprobado aqui; la demo usa esos numeros: demo/fin.c TABLA_*):
     - una fila cada [0x35c7a0] = 30 cuadros, [0x35c7a4] = 5 filas a la vista, 38 px entre filas, y0 = -20 en la
       variante 1 (0 en las demas); por fila la escena 2C000D (fondo) y la de 0x3ef430[fila] (2C000E.. "Etapa n") en
       (0, y0 + 38 k) -> grupos T_FILA y T_E0..T_E14;
     - la barra: imagen 0x30002 (spr_etc, imagen 2) en (344, y0 + 195 + 38 k) y 0x30003 (imagen 3) en (346, y0 + 199 +
       38 k) estirada en x por el porcentaje (0x1179e0 = 42D200) -> T_BARRA0 / T_BARRA1; el porcentaje (0x19ffb0, fuente
       4, "%02d%%" o "100%%") en (470, misma y);
     - el tiempo de la etapa (0x19fc88: 4505A0 -> 0x3ef3c8, fuente 4, paso de 20 px) en (338, y0 + 195 + 38 k);
     - el nombre de la etapa: la escena 2C0047 en (267, y0 + 206 + 38 k) con el cuadro 0x35c7b0[descriptor de la zona]
       (44C8D0 de la clave de la etapa del nivel: su palabra 0 = el indice del descriptor de tramo) -> T_NOMBRE y los
       usos NZ_<zona> (x = cuadro);
     - variante 1, al acabar la tabla (0x19eb00): la escena 2C00AE en (0, y0) -> T_PANEL, el tiempo total (0x19fd68)
       en (338, y0 + 385) y la puntuacion (0x19ff08, "%07d", amarilla) en (x, y0 + 428) con x deslizandose (0x19fb08);
       variantes 0 / 7 / 8 (0x19ebe4): la escena 2C00AF en (0, 0) -> T_PANEL7 y el tiempo total amarillo en (x, 385).
    Glifos: fuente 4 (escala 1, modo 0 de 0x117558: alineada a la derecha del punto) -> T_CH, un cuadro por caracter de
    TABLA_CH"""
    import conv_union as cu
    from ps2.assets import Assets
    i32 = lambda v: v - (1 << 32) if v >> 31 else v
    # constantes de 0x1a05f8 / 0x19e6d0 que la demo lleva escritas: si el ejecutable es otro, se para aqui
    assert (u32(elf, 0x35c7a0), u32(elf, 0x35c7a4)) == (30, 5)
    assert lui_ori(elf, 0x1a0754, 0x1a0758) == 0x2c000d and lui_ori(elf, 0x1a0b04, 0x1a0b20) == 0x2c0047
    assert lui_ori(elf, 0x1a0880, 0x1a0894) == 0x30002 and lui_ori(elf, 0x1a08c0, 0x1a08c8) == 0x30003
    assert lui_ori(elf, 0x19eb1c, 0x19eb20) == 0x2c00ae
    assert [u32(elf, a) & 0xffff for a in (0x1a0898, 0x1a08cc, 0x1a0a30, 0x1a0900)] == [0x158, 0x15a, 0x152, 0x1d6]
    assert u32(elf, 0x1a0654) & 0xffff == 0xc1a0 and u32(elf, 0x1a0770) & 0xffff == 0x26
    assert [u32(elf, a) & 0xffff for a in (0x1a07c4, 0x1a07e4, 0x1a0ae4, 0x1a0b0c)] == [0x4343, 0x4347, 0x434e, 0x4385]
    assert u32(elf, 0x19ffdc) & 0xffff == 4 and u32(elf, 0x19fcf0) & 0xffff == 4     # fuente 4
    ac, Tc, _ = D.banco(0x2c)
    etiq = [u32(elf, 0x3ef430 + 4 * i) for i in range(15)]
    assert all(t >> 16 == 0x2c for t in etiq), [hex(t) for t in etiq]
    hud = ch.Hud(); hud.taco = [0xff] * 16; hud.vq = ch.vq_pequeno
    g = ch.grupo_escena(hud, 'T_FILA', ac, Tc, 0x2c, 0xd, 1, 'tab_'); hud.uso('T_FILA', g, 0, 0, capa=1)
    for i, t in enumerate(etiq):
        g = ch.grupo_escena(hud, 'T_E%d' % i, ac, Tc, 0x2c, t & 0xffff, 1, 'tab_'); hud.uso('T_E%d' % i, g, 0, 0, capa=1)
    A = Assets(iso)
    base, n, _ = cu.tablas(A)
    zonas = {}
    for i in range(n):
        try: z = cu.zona_de(cu.cadena(A.elf, cu.u32(A.elf, base + 4 * i) + 0xc))
        except Exception: continue
        if len(z) == 4 and z not in zonas: zonas[z] = u32(elf, 0x35c7b0 + 4 * i)
    nf = max(zonas.values()) + 1
    g = ch.grupo_escena(hud, 'T_NOMBRE', ac, Tc, 0x2c, 0x47, nf, 'tab_'); hud.uso('T_NOMBRE', g, 267, 206, capa=1)
    for z, f in sorted(zonas.items(), key=lambda v: v[1]): hud.uso('NZ_' + z, g, f, 0)
    g = ch.grupo_escena(hud, 'T_PANEL', ac, Tc, 0x2c, 0xae, 1, 'tab_'); hud.uso('T_PANEL', g, 0, 0, capa=1)
    assert lui_ori(elf, 0x19ebe8, 0x19ebfc) == 0x2c00af         # variantes 0 / 7 / 8 (0x19ebe4): el panel del tiempo
    g = ch.grupo_escena(hud, 'T_PANEL7', ac, Tc, 0x2c, 0xaf, 1, 'tab_'); hud.uso('T_PANEL7', g, 0, 0, capa=1)
    etc = D._ent(D.p_spr, D.n_spr.index('spr_etc_xst.sz')); Te = ch.texturas(etc)
    for i, im in ((0, 2), (1, 3)):                              # 42D280 / 42D200: la imagen con su (0, 0) en (x, y)
        t, l, tp, r, b = ch.imagen_xst(etc, im)                 # (recortada: spr_etc es una textura de 512x512)
        l, tp, r, b = (int(round(v)) for v in (l, tp, r, b))
        ti = hud.textura(('TBARRA', im), np.ascontiguousarray(Te[t][tp:b, l:r])); si = hud.sprite(ti, (0, 0, r - l, b - tp), r - l, b - tp)
        g = hud.grupo('T_BARRA%d' % i, 1); hud.pie.append((g, 0, si, 0, 0.0, 0.0, 0, 0, 0.0, 1.0, 1.0, 1.0))
        hud.uso('T_BARRA%d' % i, g, 0, 0, capa=1)
        print('  T_BARRA%d: imagen %d de spr_etc, %dx%d' % (i, im, r - l, b - tp))
    F = cm.Fuente(D, 4, cm.fuentes_xst(D))
    g = hud.grupo('T_CH', len(TABLA_CH))                       # los glifos en una tira (la textura de la fuente es grande)
    ps = [F.piezas(bytes([c]), 0, 0, 0) for c in TABLA_CH]
    W = sum(int(p['w']) for q in ps for p in q); H = max([int(p['h']) for q in ps for p in q] + [1])
    tira = np.zeros((H, max(W, 1), 4), np.uint8); x0 = 0; pos = []
    for q in ps:
        for p in q:
            l, tp, r, b = (int(round(v)) for v in p['src'])
            tira[:b - tp, x0:x0 + r - l] = F.T[p['tex']][tp:b, l:r]; pos.append((x0, r - l, b - tp)); x0 += r - l
    ti = hud.textura(('TCH', 4), tira); j = 0
    hud.vq = lambda i, a, fmt, lista: None if i == ti else ch.vq_pequeno(i, a, fmt, lista)   # cifras: sin VQ (las emborrona)
    for k, q in enumerate(ps):
        for p in q:
            x0, w, h = pos[j]; j += 1
            si = hud.sprite(ti, (x0, 0, x0 + w, h), w, h)
            hud.pie.append((g, k, si, 0, float(p['M'][2, 0]), float(p['M'][2, 1]), 0, 0, 0.0, 1.0, 1.0, 1.0))
    hud.uso('T_CH', g, 0, 0, capa=1)
    print('  T_CH: %d glifos de la fuente 4 (%dx%d)' % (j, W, H))
    print('GOALT.DAT: %d zonas con nombre (%d cuadros), %d etiquetas' % (len(zonas), nf, len(etiq)))
    return hud

def maquetas(huds, d):
    """PNG de comprobacion: millas 01234567 al acabar la cuenta, la pregunta con la eleccion y GAME OVER (cuadro 100)"""
    from PIL import Image
    os.makedirs(d, exist_ok=True)
    for nombre, fich, cosas in (('fin_millas', 'FIN.DAT', [('MILLAS', 80)] + [('MILLAS_C%d' % i, 6 * d) for i, d in enumerate((7, 6, 5, 4, 3, 2, 1, 0))]),
                                ('fin_continua', 'FIN.DAT', [('CONT_PREG', 49), ('CONT_ELIGE', 59)]),
                                ('fin_game_over', 'GOVER.DAT', [('GAME_OVER', 100)])):
        hud = huds[fich]; img = Image.new('RGBA', (640, 480), (40, 60, 90, 255))
        for n, c in cosas:
            u = next(u for u in hud.lay if u[0] == n); ch.dibuja_grupo(img, hud, u[1], c, u[3], u[4])
        img.save(os.path.join(d, nombre + '.png'))

if __name__ == '__main__':
    if len(sys.argv) < 3: raise SystemExit(__doc__)
    convierte(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
