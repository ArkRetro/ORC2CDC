"""fsp_musica.py SPFE.DAT MENU.DAT imagen salida/ : la fila SPLASH WAVE REMASTER en "Elige la musica" del frontal de
recreativa de OUTRUN2SP (comun/fsp.c), como parche del SPFE.DAT que hay (formato de tools/conv_sprani.py: HUD1 con el
bloque SPR1, la animacion del banco 0x2E SELECTOR_CVT y el bloque FSP1 de conv_sp.tablas_fe).

Uso: python3 -I tools/fsp_musica.py <SPFE.DAT> <MENU.DAT> <imagen> <dir de salida> -> <salida>/SPFE.DAT parcheado y
fsp_musica*.png (maquetas). MENU.DAT solo se lee (la fuente 14, para el rotulo de la tarjeta). El audio (MUS70 / MUS71)
es el de tools/menu_musica.py.

La lista del original (escena 2E0028, 8 filas por lado: 7 temas y ALEATORIO; t = 90 cuadros por fila, lista_4bee90 /
lista_4c0eb0) se alarga a 9 filas con la tarjeta nueva al final de cada lado, sin tocar las capas de las 8 filas:
 - 2E0028: el tiempo de la raiz se abre (lado 0: 30..870 en vez de 30..780; el cambio de lado y el lado 1, 90 cuadros
   despues; la vuelta, 180; duracion 1780) y todo el carrusel se corre DX_LISTA px a la izquierda para que la novena
   quepa en la pantalla. Por cada capa de la raiz que dibuja un carrusel (detras: la otra lista; delante: las cartas
   pequenas; arriba: la grande, el marco y el destello) se anade otra con la misma animacion y una componente con la
   carta 9: copias de las capas de la carta 8 (ALEATORIO) corridas PASO px y 90 cuadros (2 en la entrada).
 - 2E0029 / 2E002A (la eleccion de cada lado, 30 cuadros por fila, salida en 240..255): la carta 9 quieta en el
   carrusel y su eleccion en 256..285 (capas nuevas con la animacion de la raiz de 210..239).
 - 2E0027 (el visor de la radio): el tema nuevo en 1600..1689 (rotulo "SPLASH WAVE REMASTER" hecho con las letras de
   los rotulos del propio visor y el ecualizador del tema 0); las capas del fondo hasta 1700.
 - FSP1: detras de las tablas, el bloque 'MUSX' (comun/fsp.c): filas 9, desfase del lado 1 (830), cuadros del cambio de
   lado en 2E0028 y un registro por lado {tema 70, avance 71, 4 filas (escena, primero, ultimo)}; y lo necesario para
   deshacer el parche (bytes cambiados de la animacion y tamanos de antes): repetible.
Arte: la carta pequena y la grande son las de SPLASH WAVE del original (sprites del cuadro de la fila 0) con la imagen
del usuario en el disco y "REMASTER" (fuente 14 de MENU.DAT) en la cinta de abajo; una textura nueva de 512x128."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import menu_musica as MM

PASO = 61.0                    # entre cartas (556,5 - 495,5)
DX_LISTA = 24.0                # el carrusel a la izquierda (la carta 9: 617,5 + 20 + 22 < 640)
T_FILA = 90
EXTRA = b'MUSX'
REG_TEMA, REG_AVANCE = 70, 71
LCD0, LCD1 = 1600, 1690        # el tema nuevo en 2E0027
ELIGE0 = 256                   # la eleccion de la fila 9 en 2E0029 / 2E002A (256..285)
ESC_LISTA, ESC_LCD, ESC_ELIGE, ESC_PANEL = 0x28, 0x27, (0x29, 0x2a), 0x13


# ---------------------------------------------------------------- SPR1
class Spr1:
    def __init__(s, D):
        b = D.raw; assert b[:4] == b'SPR1', 'sin bloque SPR1'
        (s.bk, ne, ns, o_ani, t_ani, o_frac, o_tex, o_masc, nm, o_ex, t_ex) = struct.unpack_from('<11I', b, 4)
        s.ani = bytearray(b[o_ani:o_ani + t_ani])
        s.frac = [struct.unpack_from('<4f', b, o_frac + 16 * i) for i in range(ns)]
        s.tex_esc = []
        for k in range(ne):
            o, n = struct.unpack_from('<2I', b, o_tex + 8 * k); s.tex_esc.append(list(struct.unpack_from('<%dH' % n, b, o)))
        s.masc = [b[o_masc + 12 * i:o_masc + 12 * i + 12] for i in range(nm)]
        s.extra = bytearray(b[o_ex:o_ex + t_ex])

    def bloque(s):
        ne = len(s.tex_esc); ani = s.ani
        o_ani = 48; o_frac = (o_ani + len(ani) + 3) & ~3; o_tex = o_frac + 16 * len(s.frac)
        listas = bytearray(); tab = bytearray()
        for l in s.tex_esc:
            tab += struct.pack('<2I', o_tex + 8 * ne + len(listas), len(l)); listas += struct.pack('<%dH' % len(l), *l)
        listas += bytes((-len(listas)) % 4)
        o_masc = o_tex + 8 * ne + len(listas); mt = b''.join(s.masc); o_ex = o_masc + len(mt)
        blq = bytearray(b'SPR1' + struct.pack('<11I', s.bk, ne, len(s.frac), o_ani, len(ani), o_frac, o_tex, o_masc,
                                              len(s.masc), o_ex, len(s.extra)))
        blq += ani; blq += bytes(o_frac - len(blq))
        for f4 in s.frac: blq += struct.pack('<4f', *f4)
        blq += tab + listas + mt + s.extra
        return bytes(blq)

    # animacion
    def u(s, o): return struct.unpack_from('<I', s.ani, o)[0]
    def h(s, o): return struct.unpack_from('<h', s.ani, o)[0]
    def f(s, o): return struct.unpack_from('<f', s.ani, o)[0]
    def pon(s, o, fmt, *v):
        """cambia bytes de la animacion de antes del parche apuntando lo que habia (para deshacerlo)"""
        n = struct.calcsize(fmt)
        if o < s.ani0: s.deshacer.append((o, bytes(s.ani[o:o + n])))
        struct.pack_into(fmt, s.ani, o, *v)
    def mete(s, b):
        while len(s.ani) % 4: s.ani.append(0)
        o = len(s.ani); s.ani += b; return o
    def raiz(s, k):
        sc = s.u(4 * k); nc = s.u(sc); pc = s.u(sc + 4); return pc + (nc - 1) * 0x24
    def capas(s, comp):
        n, pl = s.u(comp + 0x1c), s.u(comp + 0x20); return [pl + i * 0x4c for i in range(n)]

    # canales: {i16 n, 2 bytes, u8 escala valor, u8 escala tangente, n x {i16 t, v, tan entrada, tan salida}}
    def canal(s, ch):
        n = s.h(ch); return s.ani[ch:ch + 6], [list(struct.unpack_from('<4h', s.ani, ch + 6 + 8 * j)) for j in range(n)]
    def canal_nuevo(s, cab, claves):
        b = bytearray(cab); struct.pack_into('<h', b, 0, len(claves))
        for c in claves: b += struct.pack('<4h', *[int(round(v)) for v in c])
        return s.mete(bytes(b))
    def canal_cambia(s, ch, ft=lambda t: t, dv=0.0):
        cab, cl = s.canal(ch); e = 1 << cab[4]
        return s.canal_nuevo(cab, [[ft(t), v + dv * e, a, b] for t, v, a, b in cl])
    def canal_valor(s, ch, t):
        cab, cl = s.canal(ch); sv = 1.0 / (1 << cab[4]); st = 1.0 / (1 << cab[5])
        if t <= cl[0][0]: return cl[0][1] * sv
        if t >= cl[-1][0]: return cl[-1][1] * sv
        j = next(j for j in range(1, len(cl)) if cl[j][0] >= t); p, q = cl[j - 1], cl[j]
        if p[1] == q[1] and p[3] == q[2]: return p[1] * sv
        dt = q[0] - p[0]; x = (t - p[0]) / dt; x2 = x * x; x3 = x2 * x
        return ((2 * x3 - 3 * x2 + 1) * p[1] * sv + (3 * x2 - 2 * x3) * q[1] * sv + dt * (x3 - 2 * x2 + x) * p[3] * st
                + dt * (x3 - x2) * q[2] * st)
    def canal_ventana(s, ch, t0, n, d0, dv=0.0):
        """canal con los valores de [t0, t0 + n] (+ dv) puestos en [d0, d0 + n] (una clave por cuadro, sin tangentes)"""
        cab, cl = s.canal(ch); e = 1 << cab[4]
        return s.canal_nuevo(cab, [[d0 + i, (s.canal_valor(ch, t0 + i) + dv) * e, 0, 0] for i in range(n + 1)])

    def capa_copia(s, L, pri=None, ult=None, desf=None, dx=0.0, dy=0.0, ft=None, sprite=None, comp=None, canales=None):
        """copia de la capa L (0x4c bytes) con cambios; devuelve sus bytes (no la mete)"""
        c = bytearray(s.ani[L:L + 0x4c]); mk = c[0x49]
        if pri is not None: struct.pack_into('<h', c, 0, pri)
        if ult is not None: struct.pack_into('<h', c, 2, ult)
        if desf is not None: struct.pack_into('<h', c, 4, desf)
        for k in range(6):
            if not mk >> k & 1: continue
            ch = struct.unpack_from('<I', c, 0x30 + 4 * k)[0]
            if canales and k in canales: nch = canales[k]
            else:
                dv = dx if k == 0 else dy if k == 1 else 0.0
                nch = s.canal_cambia(ch, ft or (lambda t: t), dv) if (dv or ft) else ch
            struct.pack_into('<I', c, 0x30 + 4 * k, nch)
        if not mk & 1 and dx: struct.pack_into('<f', c, 0x18, struct.unpack_from('<f', c, 0x18)[0] + dx)
        if not mk & 2 and dy: struct.pack_into('<f', c, 0x1c, struct.unpack_from('<f', c, 0x1c)[0] + dy)
        if sprite is not None:                         # footage nuevo con un cuadro: el sprite
            fo = struct.unpack_from('<I', c, 8)[0]
            w, hh, ritmo, n, fr = struct.unpack_from('<5I', s.ani, fo)
            reg = bytearray(s.ani[fr:fr + 20]); struct.pack_into('<I', reg, 0, sprite)
            o_fr = s.mete(bytes(reg)); o_fo = s.mete(struct.pack('<5I', w, hh, ritmo, 1, o_fr))
            struct.pack_into('<I', c, 8, o_fo)
        if comp is not None: struct.pack_into('<I', c, 8, comp)
        return bytes(c)

    def comp_nueva(s, modelo, capas_b, fin=None):
        """componente (0x24 bytes, copia de la de modelo) con las capas dadas (bytes, en orden del array)"""
        pl = s.mete(b''.join(capas_b))
        c = bytearray(s.ani[modelo:modelo + 0x24])
        struct.pack_into('<I', c, 0x1c, len(capas_b)); struct.pack_into('<I', c, 0x20, pl)
        if fin is not None: struct.pack_into('<f', c, 8, fin)
        return s.mete(bytes(c))

    def comp_capas(s, comp, capas_b):
        """cambia (deshacible) las capas de una componente que ya estaba: array nuevo al final"""
        pl = s.mete(b''.join(capas_b))
        s.pon(comp + 0x1c, '<I', len(capas_b)); s.pon(comp + 0x20, '<I', pl)


def ultimo_x(S, L):
    """x final de una capa (canal 0 o fija)"""
    mk = S.ani[L + 0x49]
    if mk & 1: return S.canal_valor(S.u(L + 0x30), 1599)
    return S.f(L + 0x18)


def es_footage(S, L): return S.ani[L + 0x48] & 1


# ---------------------------------------------------------------- arte
def carta(base, frac, img, gl, F, footage_wh, disco, rotulo):
    """la carta del original con la imagen del usuario en el disco (circulo de centro y radio en px del footage) y
    rotulo en la cinta de abajo; devuelve (RGBA, frac)"""
    from PIL import Image, ImageDraw
    W, H = footage_wh
    x0, y0 = int(round(frac[0] * W)), int(round(frac[1] * H))
    lienzo = Image.new('RGBA', (W, H), (0, 0, 0, 0)); lienzo.alpha_composite(base, (x0, y0))
    cx, cy, r = disco
    foto = MM._cuadrada(img, 2 * r, 2 * r)
    m = Image.new('L', (8 * r, 8 * r), 0); ImageDraw.Draw(m).ellipse([0, 0, 8 * r - 1, 8 * r - 1], fill=255)
    m = m.resize((2 * r, 2 * r), Image.LANCZOS)
    a = np.array(lienzo)[..., 3]
    capa = Image.new('RGBA', (W, H), (0, 0, 0, 0)); capa.paste(foto, (cx - r, cy - r), m)
    ca = np.array(capa); ca[..., 3] = np.minimum(ca[..., 3], a)      # (dentro de la carta)
    lienzo.alpha_composite(Image.fromarray(ca))
    d = ImageDraw.Draw(lienzo)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(255, 255, 255, 255), width=max(1, r // 8))
    if rotulo:
        esc, (ry, rh) = rotulo
        t = MM.texto_rgba(b'REMASTER', esc, (232, 96, 16, 255), gl, F, contorno=1, bold=0.3, track=0.5)
        t = MM._ajusta(t, int(W * 0.62))
        lienzo.alpha_composite(t, (int(cx - t.width / 2), int(ry - t.height / 2)))
    bb = Image.fromarray(np.array(lienzo)).getbbox()
    out = lienzo.crop(bb)
    return out, (bb[0] / W, bb[1] / H, bb[2] / W, bb[3] / H)


def letras(im):
    """letras de un rotulo del visor: los nucleos (alfa > 230, sin el brillo) separados por columnas vacias ->
    [(x0, x1, n0, n1)]: el trozo con su brillo (hasta la mitad del hueco con las vecinas) y el nucleo"""
    a = np.array(im)[..., 3].max(0) > 230; nuc = []; x = 0
    while x < len(a):
        if a[x]:
            x1 = x
            while x1 < len(a) and a[x1]: x1 += 1
            nuc.append((x, x1)); x = x1
        else: x += 1
    out = []
    for i, (n0, n1) in enumerate(nuc):
        x0 = 0 if i == 0 else (nuc[i - 1][1] + n0) // 2
        x1 = len(a) if i == len(nuc) - 1 else (n1 + nuc[i + 1][0]) // 2
        out.append((x0, x1, n0, n1))
    return out


# ---------------------------------------------------------------- el parche
def quita(D, S):
    """deshace un parche anterior (el bloque MUSX lleva los bytes cambiados y los tamanos de antes)"""
    i = S.extra.find(EXTRA)
    if i < 0: return False
    x = bytes(S.extra[i:])
    n_ani, n_spr, n_tex, n_des = struct.unpack_from('<4I', x, 8 + 4 * 0x40)
    o = 8 + 4 * 0x40 + 16
    for _ in range(n_des):
        off, n = struct.unpack_from('<2I', x, o); o += 8
        S.ani[off:off + n] = x[o:o + n]; o += (n + 3) & ~3
    del S.ani[n_ani:]
    del S.frac[n_spr:]; del D.spr[n_spr:]; del D.tex[n_tex:]
    S.tex_esc = [[t for t in l if t < n_tex] for l in S.tex_esc]
    del S.extra[i:]
    return True


def parchea(D, S, menu, img):
    from PIL import Image
    from modelo import textura_dc
    quita(D, S)
    S.ani0 = len(S.ani); S.deshacer = []
    n_spr0, n_tex0 = len(D.spr), len(D.tex)
    Mn = MM.Dat(menu); F = Mn.fuente(); gl = MM.glifos(Mn, F)
    V = MM.Vista(D)
    def spr_img(si):
        ti, (l, t, r, b), w, h = V.spr[si]
        return Image.fromarray(V.tex[ti][int(round(t)):int(round(b)), int(round(l)):int(round(r))].copy(), 'RGBA')

    # --- la escena de la lista: raiz con una capa (L0) -> componente con 12 capas
    R = S.raiz(ESC_LISTA); L0 = S.capas(R)[0]; C0 = S.u(L0 + 8); raices = S.capas(C0)
    comps = {}
    for L in raices: comps.setdefault(S.u(L + 8), []).append(L)
    # lado de cada componente: la de la ventana del lado 0 (pri 30) y la del lado 1 (pri 800); delante = mas capas
    def lado_de(c): return 0 if any(S.h(L) == 30 for L in comps[c]) else 1
    def capas_c(c): return S.capas(c)
    def delante(c): return len(capas_c(c)) > 20
    # cartas pequenas (mk 1, footage 90x70, op 100) y grandes / marco / destello de la ultima fila de cada componente
    def ultima(c, cond):
        ls = [L for L in capas_c(c) if cond(L)]
        return max(ls, key=lambda L: (ultimo_x(S, L), S.h(L)))
    def fw(L): return S.u(S.u(L + 8)) if es_footage(S, L) else 0
    def peq(L): return es_footage(S, L) and fw(L) == 90 and S.ani[L + 0x49] == 1
    # sprite de la carta pequena y la grande de la fila 0 del lado 0 (SPLASH WAVE)
    c_del0 = next(c for c in comps if lado_de(c) == 0 and delante(c))
    c_det0 = next(c for c in comps if lado_de(c) == 0 and not delante(c))
    pq0 = min((L for L in capas_c(c_del0) if peq(L) and S.f(L + 0x2c) > 99), key=lambda L: ultimo_x(S, L))
    gr0 = next(L for L in capas_c(c_del0) if es_footage(S, L) and fw(L) == 130 and S.h(L) == 30
               and S.u(S.u(L + 8) + 4) == 106)
    def spr_de(L): return S.u(S.u(S.u(L + 8) + 16)) & 0xffff
    s_pq, s_gr = spr_de(pq0), spr_de(gr0)
    # arte nuevo
    pq_img, pq_frac = carta(spr_img(s_pq), S.frac[s_pq], img, gl, F, (90, 70), (45, 40, 13), (0.55, (60, 8)))
    # el icono del panel de la derecha (2E0013: un cuadro por registro de 5C8D00; el 16, el nuevo)
    R13 = S.raiz(ESC_PANEL); ic0 = next(L for L in S.capas(R13) if S.h(L) == 0); s_ic = spr_de(ic0)
    ic_img, ic_frac = carta(spr_img(s_ic), S.frac[s_ic], img, gl, F, (90, 50), (60, 29, 11), None)
    gr_img, gr_frac = carta(spr_img(s_gr), S.frac[s_gr], img, gl, F, (130, 106), (63, 58, 19), (0.75, (89, 12)))
    # rotulo del visor: "SPLASH WAVE" (fila 0) + " REMASTER" con letras de los rotulos del visor
    R27 = S.raiz(ESC_LCD); c27 = S.capas(R27)
    tit = [L for L in c27 if not es_footage(S, L) and S.u(S.u(L + 8) + 0x1c) == 1
           and S.u(S.u(S.capas(S.u(L + 8))[0] + 8)) == 362]
    tit.sort(key=lambda L: S.h(L))
    def tit_spr(L): return spr_de(S.capas(S.u(L + 8))[0])
    base = spr_img(tit_spr(tit[0])); bf = S.frac[tit_spr(tit[0])]
    # (solo los que el visor escribe en mayusculas: Risky Ride, Night Flight... van en minusculas)
    nombres = {0: b'SPLASH WAVE', 1: b'MAGICAL SOUND SHOWER', 2: b'PASSING BREEZE', 7: b'ALEATORIO'}
    glifo = {}
    for i, nom in nombres.items():
        L = tit[i]
        im = spr_img(tit_spr(L)); tr = letras(im); txt = nom.replace(b' ', b'')
        if len(tr) != len(txt): continue
        for (x0, x1, n0, n1), ch in zip(tr, txt): glifo.setdefault(ch, (im.crop((x0, 0, x1, im.height)), n0 - x0, n1 - n0))
    falta = [chr(c) for c in b'REMASTER' if c not in glifo]
    assert not falta, 'letras del visor que no salen: %s' % falta
    tr0 = letras(base)
    hueco = int(np.median([tr0[i + 1][2] - tr0[i][3] for i in (0, 1, 2, 3)]))     # entre letras (S-P-L-A-S)
    esp = tr0[6][2] - tr0[5][3]                                                   # el espacio (H / W)
    lc = Image.new('RGBA', (360, base.height), (0, 0, 0, 0)); lc.alpha_composite(base, (0, 0))
    x = tr0[-1][3] + esp                                                          # donde empieza el nucleo
    for ch in b'REMASTER':
        g, dn, wn = glifo[ch]; lc.alpha_composite(g, (x - dn, 0)); x += wn + hueco
    lc = lc.crop(Image.fromarray(np.array(lc)).getbbox() or (0, 0, 1, 1))
    W362, H42 = 362, 42
    if lc.width > W362 - 4: lc = lc.resize((W362 - 4, lc.height), Image.LANCZOS)
    y0 = int(round(bf[1] * H42)); x0 = (W362 - lc.width) // 2
    lcd_img = lc; lcd_frac = (x0 / W362, y0 / H42, (x0 + lc.width) / W362, (y0 + lc.height) / H42)
    # textura nueva (512 x 128) con los tres
    piezas = (('pq', pq_img), ('gr', gr_img), ('lcd', lcd_img), ('ic', ic_img)); sit = {}; x = y = fila = 0
    for k, im in piezas:                                  # en filas, 512 de ancho
        if x + im.width > 512: x, y, fila = 0, y + fila, 0
        sit[k] = (x, y, im.width, im.height); x += im.width; fila = max(fila, im.height)
    AH = max(8, 1 << (y + fila - 1).bit_length())
    at = np.zeros((AH, 512, 4), np.uint8)
    for k, im in piezas:
        x, y, w, h = sit[k]; at[y:y + h, x:x + w] = np.array(im)
    fmt, li, datos = textura_dc(at)
    ti = len(D.tex); D.tex.append((512, AH, fmt, li, bytes(datos)))
    nuevo = {}
    for k, fr in (('pq', pq_frac), ('gr', gr_frac), ('lcd', lcd_frac), ('ic', ic_frac)):
        x, y, w, h = sit[k]; nuevo[k] = len(D.spr)
        D.spr.append((ti, 0, 0, 0, x / 512, y / AH, (x + w) / 512, (y + h) / AH)); S.frac.append(fr)
    for k in (ESC_LISTA, ESC_LCD, ESC_PANEL) + ESC_ELIGE: S.tex_esc[k].append(ti)
    c13 = S.capas(R13)
    S.comp_capas(R13, [bytes(S.ani[L:L + 0x4c]) for L in c13] + [S.capa_copia(ic0, pri=16, ult=17, sprite=nuevo['ic'])])
    S.pon(R13 + 8, '<f', 17.0)

    # --- 2E0028: tiempos de la raiz, carrusel a la izquierda y la carta 9 en cada carrusel
    def g(t): return t if t < 780 else t + 90 if t < 1520 else t + 180
    d9 = lambda t: t + T_FILA
    def nueva_comp(c):
        """componente con la carta 9 de la componente c (copias de las capas de la fila 8)"""
        out = []
        if not delante(c):                              # detras: la otra lista (cartas y sus sombras)
            for cond in (lambda L: peq(L) and S.f(L + 0x2c) > 99, lambda L: peq(L) and S.f(L + 0x2c) <= 99):
                L = ultima(c, cond)
                out.append(S.capa_copia(L, dx=PASO, ft=lambda t: t + 2 if t < 30 else t,
                                        sprite=nuevo['pq'] if S.f(L + 0x2c) > 99 else None))
            return [S.comp_nueva(c, out[::-1])], []
        Ls = capas_c(c)
        cp = ultima(c, lambda L: peq(L) and S.f(L + 0x2c) > 99)
        fin8 = max(S.h(L) for L in Ls if S.h(L) < 1000 and S.h(L + 2) < 1000 and S.h(L) > 0)   # 660: la fila 8
        de8 = [L for L in Ls if S.h(L) == fin8]                                           # grande, marco, destello
        arriba = []
        for L in de8:
            gr = es_footage(S, L) and fw(L) == 130 and S.u(S.u(L + 8) + 4) == 106
            arriba.append(S.capa_copia(L, pri=d9(S.h(L)), ult=d9(S.h(L + 2)), dx=PASO, ft=d9,
                                       sprite=nuevo['gr'] if gr else None))
        abajo = [S.capa_copia(cp, dx=PASO, ft=lambda t: t + 2 if t < 30 else t, sprite=nuevo['pq'])]
        # orden del array: la ultima se dibuja primero; las de de8 van en el orden que tenian
        orden = sorted(range(len(de8)), key=lambda i: Ls.index(de8[i]))
        return [S.comp_nueva(c, abajo)], [S.comp_nueva(c, [arriba[i] for i in orden])]
    nuevas = {c: nueva_comp(c) for c in comps}
    dib = list(reversed(raices))                     # orden de dibujo (primero el de abajo)
    sal = []
    for L in dib:
        pri, ult, desf = S.h(L), S.h(L + 2), S.h(L + 4)
        mods = dict(pri=g(pri), ult=g(ult) if ult != 780 else 870, desf=g(desf) if desf == pri else desf,
                    dx=-DX_LISTA, ft=g)
        if pri == 30 and ult == 780: mods['ult'] = 870
        if pri == 800: mods['ult'] = 1700
        c = S.u(L + 8); abajo, arriba = nuevas[c]
        if not delante(c):
            sal.append(S.capa_copia(L, **mods))
            for nc in abajo: sal.append(S.capa_copia(L, comp=nc, **mods))
        else:
            for nc in abajo: sal.append(S.capa_copia(L, comp=nc, **mods))
            sal.append(S.capa_copia(L, **mods))
            for nc in arriba: sal.append(S.capa_copia(L, comp=nc, **mods))
    S.comp_capas(C0, sal[::-1])
    S.pon(C0 + 8, '<f', 1780.0); S.pon(L0 + 2, '<h', 1780); S.pon(R + 8, '<f', 1780.0)

    # --- 2E0029 / 2E002A: la carta 9 en el carrusel de la eleccion y su eleccion en 256..285
    for k in ESC_ELIGE:
        Rk = S.raiz(k); rl = S.capas(Rk)
        cs = [L for L in rl if not es_footage(S, L) and S.u(S.u(L + 8) + 0x1c) >= 16]
        detras = next(L for L in cs if S.u(S.u(L + 8) + 0x1c) == 16)
        frente = next(L for L in cs if S.u(S.u(L + 8) + 0x1c) > 16)
        cd, cf = S.u(detras + 8), S.u(frente + 8)
        # detras: carta y sombra de la fila 8 + PASO
        out = []
        for cond in (lambda L: peq(L) and S.f(L + 0x2c) > 99, lambda L: peq(L) and S.f(L + 0x2c) <= 99):
            L = ultima(cd, cond)
            out.append(S.capa_copia(L, dx=PASO, ft=lambda t: t + 2 if t < 30 else t,
                                    sprite=nuevo['pq'] if S.f(L + 0x2c) > 99 else None))
        nb = S.comp_nueva(cd, out[::-1])
        Lf = capas_c(cf)
        quietas = [L for L in Lf if es_footage(S, L) and fw(L) == 90]
        q8 = max(quietas, key=lambda L: S.f(L + 0x18))
        nq = S.comp_nueva(cf, [S.capa_copia(q8, dx=PASO, sprite=nuevo['pq'])])
        v8 = max(S.h(L) for L in Lf if S.h(L + 2) <= 240)                   # 210: la eleccion de la fila 8
        de8 = [L for L in Lf if S.h(L) == v8]
        arr = []
        for L in sorted(de8, key=lambda L: Lf.index(L)):
            gr = es_footage(S, L) and fw(L) == 130
            arr.append(S.capa_copia(L, pri=S.h(L) + 30, ult=S.h(L + 2) + 30, dx=PASO, ft=lambda t: t + 30,
                                    sprite=nuevo['gr'] if gr else None))
        nt = S.comp_nueva(cf, arr)
        dibk = list(reversed(rl)); sal = []
        def ventana(L, desf=None, dx=0.0):
            """la capa en 256..285 con la animacion de su raiz en 210..240 y su tiempo local de entonces"""
            mk = S.ani[L + 0x49]; can = {}
            for q in range(6):
                if mk >> q & 1: can[q] = S.canal_ventana(S.u(L + 0x30 + 4 * q), v8, 30, ELIGE0, dx if q == 0 else 0.0)
            if desf is None: desf = (v8 - S.h(L)) * S.h(L + 6) // 4096 + S.h(L + 4)
            return dict(pri=ELIGE0, ult=ELIGE0 + 30, desf=desf, canales=can, dx=dx)
        def rotulo(L):                                   # el rotulo del visor de una fila (componente con el de 362)
            if es_footage(S, L) or S.u(S.u(L + 8) + 0x1c) != 1: return False
            h0 = S.capas(S.u(L + 8))[0]
            return es_footage(S, h0) and S.u(S.u(h0 + 8)) == 362
        rot0 = min((L for L in rl if rotulo(L)), key=lambda L: S.h(L))
        for L in dibk:
            activa = S.h(L) <= v8 < S.h(L + 2)
            if L == detras:
                sal.append(S.capa_copia(L, dx=-DX_LISTA)); sal.append(S.capa_copia(L, comp=nb, dx=-DX_LISTA))
                w = ventana(L, dx=-DX_LISTA)
                sal.append(S.capa_copia(L, **w)); sal.append(S.capa_copia(L, comp=nb, **w))
            elif L == frente:
                sal.append(S.capa_copia(L, comp=nq, dx=-DX_LISTA)); sal.append(S.capa_copia(L, dx=-DX_LISTA))
                sal.append(S.capa_copia(L, comp=nt, dx=-DX_LISTA))
                w = ventana(L, desf=v8 + 30, dx=-DX_LISTA)          # (local 240..270: la carta 9 de nt)
                sal.append(S.capa_copia(L, comp=nq, **w)); sal.append(S.capa_copia(L, **w))
                sal.append(S.capa_copia(L, comp=nt, **w))
            elif rotulo(L):
                sal.append(bytes(S.ani[L:L + 0x4c]))
                if L == rot0:                              # el de la fila 9: SPLASH WAVE REMASTER
                    ct = S.u(L + 8); hijo = S.capa_copia(S.capas(ct)[0], sprite=nuevo['lcd'])
                    sal.append(S.capa_copia(L, pri=ELIGE0, ult=ELIGE0 + 30, comp=S.comp_nueva(ct, [hijo])))
            elif not es_footage(S, L) and S.u(S.u(L + 8) + 0x1c) == 8 and S.h(L) == 0 and S.h(L + 2) > 400:
                sal.append(bytes(S.ani[L:L + 0x4c]))           # el ecualizador: el de la fila 0
                sal.append(S.capa_copia(L, pri=ELIGE0, ult=ELIGE0 + 30, desf=0))
            else:
                sal.append(bytes(S.ani[L:L + 0x4c]))
                if activa: sal.append(S.capa_copia(L, **ventana(L)))
        S.comp_capas(Rk, sal[::-1])
    # --- 2E0027: el tema nuevo en 1600..1689
    R27 = S.raiz(ESC_LCD); c27 = S.capas(R27); sal = []
    for L in reversed(c27):
        b = bytearray(S.ani[L:L + 0x4c])
        if S.h(L + 2) >= 1600 and S.h(L) < 1600: struct.pack_into('<h', b, 2, LCD1 + 10)
        sal.append(bytes(b))
    t0 = tit[0]; ct = S.u(t0 + 8)
    hijo = S.capa_copia(S.capas(ct)[0], sprite=nuevo['lcd'])
    sal.append(S.capa_copia(t0, pri=LCD0, ult=LCD1, comp=S.comp_nueva(ct, [hijo])))
    eq = next(L for L in c27 if not es_footage(S, L) and S.h(L) == 60 and S.h(L + 2) == 780 and L not in tit)
    sal.append(S.capa_copia(eq, pri=LCD0, ult=LCD1, desf=0))
    S.comp_capas(R27, sal[::-1]); S.pon(R27 + 8, '<f', float(LCD1 + 10))

    # --- MUSX
    regs = b''
    for lado, k in enumerate(ESC_ELIGE):
        regs += struct.pack('<2I', REG_TEMA, REG_AVANCE) + struct.pack(
            '<12I', 0x2e0000 | ESC_LCD, LCD0, LCD0 + 14, 0x2e0000 | ESC_LCD, LCD0 + 15, LCD0 + 74,
            0x2e0000 | k, ELIGE0, ELIGE0 + 29, 0x2e0000 | ESC_LCD, LCD0 + 75, LCD0 + 89)
    cab = struct.pack('<8I', 9, 830, 870, 889, 1700, 1719, 2, 0) + regs
    cab += bytes(4 * 0x40 - len(cab))
    des = b''
    for off, b in S.deshacer: des += struct.pack('<2I', off, len(b)) + b + bytes((-len(b)) % 4)
    S.extra += EXTRA + struct.pack('<I', 0) + cab + struct.pack('<4I', S.ani0, n_spr0, n_tex0, len(S.deshacer)) + des
    return {'pq': pq_img, 'gr': gr_img, 'lcd': lcd_img}


# ---------------------------------------------------------------- ver (el reproductor de comun/sprani.c)
def dibuja(D, S, k, f, img):
    from PIL import Image
    V = MM.Vista(D)
    def mul(a, b):
        return [a[0] * b[0] + a[1] * b[2], a[0] * b[1] + a[1] * b[3], a[2] * b[0] + a[3] * b[2], a[2] * b[1] + a[3] * b[3],
                a[4] * b[0] + a[5] * b[2] + b[4], a[4] * b[1] + a[5] * b[3] + b[5]]
    def mat(L, t):
        v = [S.f(L + 0x10 + 4 * q) for q in range(8)]; mk = S.ani[L + 0x49]
        for q in range(6):
            if mk >> q & 1: v[2 + q] = S.canal_valor(S.u(L + 0x30 + 4 * q), t)
        import math
        sx, sy, gg = v[4] / 100, v[5] / 100, math.radians(v[6]); cs, sn = math.cos(gg), math.sin(gg)
        m = [sx * cs, sx * sn, -sy * sn, sy * cs, 0, 0]
        m[4] = -v[0] * m[0] - v[1] * m[2] + v[2]; m[5] = -v[0] * m[1] - v[1] * m[3] + v[3]
        if S.u(L + 0xc): m = mul(m, mat(S.u(L + 0xc), t)[0])
        return m, v[7] / 100
    def capa(L, t, M, al):
        p = S.u(L + 8)
        if not p: return
        pri, ult, d4, e6 = S.h(L), S.h(L + 2), S.h(L + 4), S.h(L + 6); fl = S.ani[L + 0x48]
        m, op = mat(L, t)
        if not ((fl & 2) and pri <= t < ult): return
        loc = (t - pri) * (e6 / 4096) + d4; al *= op; m = mul(m, M)
        if fl & 8: return
        if fl & 1:
            w, h, ritmo, n, fr = struct.unpack_from('<5I', S.ani, p)
            i = 0 if n == 1 else min(n - 1, max(0, int(ritmo * loc + 0.001)))
            s = S.u(fr + 20 * i)
            if s == 0xffffffff or al < 0.004: return
            s &= 0xffff; fr4 = S.frac[s]; ti, (l, tt, r, b), _, _ = V.spr[s]
            src = Image.fromarray(V.tex[ti]).crop((int(round(l)), int(round(tt)), int(round(r)), int(round(b))))
            q0, q1, q2, q3 = fr4[0] * w, fr4[1] * h, fr4[2] * w, fr4[3] * h
            src = src.resize((max(1, int(round(q2 - q0))), max(1, int(round(q3 - q1)))))
            if al < 1:
                a = np.array(src); a[..., 3] = (a[..., 3] * al).astype(np.uint8); src = Image.fromarray(a)
            A = np.array([[m[0], m[1], 0], [m[2], m[3], 0], [m[4] + q0 * m[0] + q1 * m[2], m[5] + q0 * m[1] + q1 * m[3], 1]])
            Ai = np.linalg.inv(A)
            img.alpha_composite(src.transform(img.size, Image.AFFINE, (Ai[0, 0], Ai[1, 0], Ai[2, 0], Ai[0, 1], Ai[1, 1],
                                                                        Ai[2, 1]), resample=Image.BILINEAR))
        else:
            ini, fin = S.f(p + 4), S.f(p + 8)
            if not (ini <= loc < fin): return
            for L2 in reversed(S.capas(p)): capa(L2, loc, m, al)
    R = S.raiz(k); M = [1, 0, 0, 1, 320 - S.h(R) / 2, 240 - S.h(R + 2) / 2]
    for L in reversed(S.capas(R)): capa(L, f, M, 1.0)


def tiene(ruta):
    try:
        D = MM.Dat(ruta); return D.raw is not None and EXTRA in Spr1(D).extra
    except Exception: return False


def main(a):
    if len(a) < 4: sys.exit(__doc__)
    from PIL import Image
    spfe, menu, imagen, sal = a[:4]
    os.makedirs(sal, exist_ok=True)
    D = MM.Dat(spfe); S = Spr1(D)
    arte_ = parchea(D, S, menu, Image.open(imagen))
    D.raw = S.bloque(); n = D.escribe(os.path.join(sal, 'SPFE.DAT'))
    print('SPFE.DAT: %d bytes; animacion %d bytes, tablas %d bytes, sprites %d, texturas %d' % (
        n, len(S.ani), 48 + len(D.raw) + 16 * len(D.tex) + 24 * len(D.spr), len(D.spr), len(D.tex)))
    D2 = MM.Dat(os.path.join(sal, 'SPFE.DAT')); S2 = Spr1(D2)
    for nom, k, fs in (('panel', ESC_PANEL, (0, 16)), ('lista', ESC_LISTA, (135, 765, 855, 1025)), ('elige', 0x29, (100, 230, 270)), ('elige2a', 0x2a, (270,)), ('visor', ESC_LCD, (100, 1640))):
        for f in fs:
            img = Image.new('RGBA', (640, 480), (40, 60, 110, 255)); dibuja(D2, S2, k, f, img)
            img.save(os.path.join(sal, 'fsp_%s_%d.png' % (nom, f)))


if __name__ == '__main__':
    main(sys.argv[1:])
