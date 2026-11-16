"""menu_musica.py: la tarjeta ALL I WANT del selector de musica (MUSICA2).

El selector del original trae 12 temas en la lista 2 (MUSICA_L2 / MUSICA_T2, escenas 0x440033 y 0x44002E) y
cada tema es un cuadro quieto del carrusel (20 + 40 k) y su titulo (110 k).  ALL I WANT no existe en el arte
del PS2: aqui se compone una tarjeta mas con la fuente 14 del propio MENU.DAT, en el mismo sitio y con el
mismo tratamiento (rotulo, etiqueta de genero, icono, visor de la radio, caratula y tira inferior), y se
anaden el cuadro 20 + 40 * 12 = 500 del carrusel y el 110 * 12 = 1320 del titulo, con sus tramos TEMA12.

`genera` solo produce las imagenes (lo usan conv_menu.py y el parche de datos); `anade` las mete en un
Menu de conv_menu.py antes de escribirlo.  No depende de fuentes del sistema: usa los glifos F14 que ya
lleva MENU.DAT.
"""
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

TEMA = 12
CUADRO_CARRUSEL = 20 + 40 * TEMA          # 500
CUADRO_TITULO = 110 * TEMA                # 1320
NOMBRE = b'ALL I WANT'
GENERO = b'CRAZY TAXI'

AZUL = (52, 76, 72, 255)                  # el verde-azulado de los rotulos de las otras tarjetas
AMARILLO = (255, 214, 0, 255)
NEGRO = (14, 14, 14, 255)
BLANCO = (255, 255, 255, 255)
ROJO = (196, 36, 32, 255)
MODO_TAXI = 'emoji'                       # 'emoji' (peticion) o 'negro' (antiguo)
_EMOJI = None


def _emoji_taxi():
    """🚕 de Segoe UI Emoji, recortado a su tinta."""
    global _EMOJI
    if _EMOJI is None:
        from PIL import ImageFont
        f = ImageFont.truetype(r'C:\Windows\Fonts\seguiemj.ttf', 512)
        im = Image.new('RGBA', (768, 768), (0, 0, 0, 0))
        ImageDraw.Draw(im).text((128, 128), '🚕', font=f, embedded_color=True)
        bb = im.getbbox()
        if not bb:
            raise RuntimeError('Segoe UI Emoji no ha dibujado el taxi')
        _EMOJI = im.crop(bb)
    return _EMOJI


def _emoji_fit(w, h):
    e = _emoji_taxi()
    s = min(w / e.width, h / e.height)
    return e.resize((max(1, int(round(e.width * s))), max(1, int(round(e.height * s)))), Image.LANCZOS)

# tamano de cada pieza y su sitio en la tarjeta (los mismos que los temas del original: tema 11 / NIGHT FLIGHT)
PIEZAS = (
    ('logo', 234, 31, 363.5, 113.0, 243.0, 15.0),
    ('tag', 112, 19, 210.0, 142.0, 87.5, 9.5),
    ('icono', 62, 49, 83.0, 118.0, 31.0, 25.0),
    ('portada', 136, 126, 507.0, 265.0, 68.0, 63.0),
    ('tira', 271, 18, 217.0, 370.9, 151.1, 8.0),
)
LCD = ('lcd', 134, 15, 342.1, 271.0, 171.0, 7.0)


def _g(g):
    return (g['nombre'], g['n'], g['pri']) if isinstance(g, dict) else (g[0], g[1], g[2])


def grupo(grp, nombre):
    for i, g in enumerate(grp):
        if _g(g)[0] == nombre: return i
    return -1


def _piezas(pie):
    d = {}
    for p in pie: d.setdefault((p[0], p[1]), []).append(p)
    return d


def _par(F, c, sig):
    i = c - F.pri
    if F.ker and sig and 0 <= sig - F.pri < F.n:
        return F.met[i][1] - F.ker[i * F.n + sig - F.pri]
    return F.met[i][1] + F.esp


def ancho(F, s):
    a = 0
    for i, c in enumerate(s):
        sig = s[i + 1] if i + 1 < len(s) else 0
        if c == 32: a += int(F.w * 0.3); continue
        a = _par(F, c, 0 if sig == 32 else sig) + a + F.esp
    return a


def _ref(tex, spr, si):
    """(ti,w,h,u0,v0,u1,v1) tanto de un MENU.DAT (7 campos) como de un Menu de conv_menu (sprite recortado)"""
    s = spr[si]
    if len(s) == 7: return s
    ti, (l, t, r, b), w, h = s
    H, W = tex[ti].shape[:2]
    return ti, w, h, l / W, t / H, r / W, b / H


def _glifos(tex, spr, pie, F):
    """un RGBA de la celda F14 de cada caracter, ya con el desplazamiento -metrica de MENU.DAT"""
    d = _piezas(pie); out = {}
    for code in range(F.pri, F.pri + F.n):
        im = Image.new('RGBA', (F.w, F.h), (0, 0, 0, 0))
        for p in d.get((F.gf, code - F.pri), []):
            gg, fr, si, fl, x, y, ax, ay, ang, sx, sy, al = p
            ti, w, h, u0, v0, u1, v1 = _ref(tex, spr, si)
            a = tex[ti]; H, W = a.shape[:2]
            L, T, R, B = int(round(u0 * W)), int(round(v0 * H)), int(round(u1 * W)), int(round(v1 * H))
            g = Image.fromarray(a[T:B, L:R], 'RGBA').resize((max(1, w), max(1, h)))
            if al < 1:
                ar = np.array(g); ar[..., 3] = (ar[..., 3] * al).astype(np.uint8); g = Image.fromarray(ar)
            im.alpha_composite(g, (int(round(x - ax)), int(round(y - ay))))
        out[code] = im
    return out


def texto_rgba(s, escala, color, glifos, F, contorno=1, sombra=0, SS=4, bold=0, escala_x=1.0):
    """texto F14 a la escala pedida (escala_x solo estira en horizontal), con contorno y sombra"""
    w = max(1, int(np.ceil(ancho(F, s) * escala * escala_x)))
    h = max(1, int(np.ceil(F.h * escala)))
    pad = max(1, contorno + (1 if sombra else 0)) + 1
    W, H = (w + 2 * pad) * SS, (h + 2 * pad) * SS
    cap = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    x = pad
    for i, c in enumerate(s):
        sig = s[i + 1] if i + 1 < len(s) else 0
        if c == 32:
            x += int(F.w * 0.3 * escala * escala_x)
            continue
        g = glifos.get(c)
        if g is not None:
            gw, gh = max(1, int(round(F.w * escala * escala_x * SS))), max(1, int(round(F.h * escala * SS)))
            cap.alpha_composite(g.resize((gw, gh), Image.LANCZOS), (int(round(x * SS)), pad * SS))
        x += _par(F, c, 0 if sig == 32 else sig) * escala * escala_x
    fill = Image.new('RGBA', (W, H), color)
    fill.putalpha(cap.getchannel('A'))
    if bold:
        fill.putalpha(fill.getchannel('A').filter(ImageFilter.MaxFilter(2 * bold * SS + 1)))
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    if sombra:
        sh = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        sh.paste(fill, (SS * max(1, sombra), SS * max(1, sombra)))
        sh.putalpha(sh.getchannel('A').point(lambda v: int(v * 0.55)))
        out.alpha_composite(sh)
    if contorno:
        o = Image.new('RGBA', (W, H), (0, 0, 0, 0)); m = Image.new('L', (W, H), 0)
        m.paste(fill.getchannel('A'), (0, 0)); m = m.filter(ImageFilter.MaxFilter(2 * contorno * SS + 1))
        o.putalpha(m); o.paste(BLANCO, (0, 0), m); out.alpha_composite(o)
    out.alpha_composite(fill)
    return out.resize((w + 2 * pad, h + 2 * pad), Image.LANCZOS)


def escudo(w, h, modo=None, texto=False):
    """escudo amarillo/negro con el taxi: emoji 🚕 por defecto o la
    aproximacion negra antigua (modo='negro'), para comparar."""
    modo = modo or MODO_TAXI
    k = 6; W, H = w * k, h * k
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.polygon([(2*k, 3*k), (W-2*k, 3*k), (W-2*k, int(H*0.78)), (W//2, H-2*k), (2*k, int(H*0.78))],
              fill=AMARILLO, outline=NEGRO)
    d.line([(2*k, 3*k), (W-2*k, 3*k), (W-2*k, int(H*0.78)), (W//2, H-2*k), (2*k, int(H*0.78)), (2*k, 3*k)],
           fill=NEGRO, width=2*k)
    n = max(4, w // 6)
    for j in range(n):
        x = int(4*k + j * (W - 8*k) / n); ww = max(1, int((W - 8*k) / n))
        d.rectangle([x, 6*k, x + ww - 1, 9*k], fill=(NEGRO if j % 2 == 0 else AMARILLO))
        d.rectangle([x, int(H*0.72), x + ww - 1, int(H*0.72)+3*k], fill=(AMARILLO if j % 2 == 0 else NEGRO))
    if modo == 'negro':
        d.rectangle([int(W*0.16), int(H*0.40), int(W*0.84), int(H*0.58)], fill=NEGRO)
        d.rectangle([int(W*0.30), int(H*0.29), int(W*0.70), int(H*0.41)], fill=NEGRO)
        d.rectangle([int(W*0.35), int(H*0.32), int(W*0.48), int(H*0.41)], fill=BLANCO)
        d.rectangle([int(W*0.52), int(H*0.32), int(W*0.65), int(H*0.41)], fill=BLANCO)
        for cx in (int(W*0.30), int(W*0.70)):
            d.ellipse([cx-5*k, int(H*0.53), cx+5*k, int(H*0.63)], fill=NEGRO)
            d.ellipse([cx-2*k, int(H*0.55), cx+2*k, int(H*0.61)], fill=BLANCO)
    else:
        if texto:
            area = (int(W*0.10), int(H*0.18), int(W*0.90), int(H*0.63))
        else:
            area = (int(W*0.10), int(H*0.14), int(W*0.90), int(H*0.70))
        aw, ah = area[2]-area[0], area[3]-area[1]
        taxi = _emoji_fit(aw, ah)
        cx = (area[0] + area[2]) // 2; cy = (area[1] + area[3]) // 2
        d.ellipse([cx-int(aw*0.40), area[3]-5*k, cx+int(aw*0.40), area[3]+k], fill=(0, 0, 0, 70))
        im.alpha_composite(taxi, (cx - taxi.width//2, cy - taxi.height//2))
    if texto:
        from PIL import ImageFont
        f = ImageFont.truetype(r'C:\Windows\Fonts\arialnb.ttf', max(8, int(h*0.18)))
        d.text((W//2, int(H*0.66)), 'TAXI', font=f, fill=NEGRO, anchor='mm')
    return im.resize((w, h), Image.LANCZOS)


def _banda(d, x, y, w, h, n):
    for j in range(n):
        xx = x + j * w // n
        d.rectangle([xx, y, xx + max(1, w // n) - 1, y + h - 1], fill=(NEGRO if j % 2 == 0 else AMARILLO))


def _logo(glifos, F):
    im = Image.new('RGBA', (234, 31), (0, 0, 0, 0))
    t = texto_rgba(NOMBRE, 1.55, AZUL, glifos, F, contorno=1, sombra=1, bold=1, escala_x=1.5)
    im.alpha_composite(t, (4, 0))
    _banda(ImageDraw.Draw(im), 4, 28, 212, 3, 42)
    return im


def _tag(glifos, F):
    im = Image.new('RGBA', (112, 19), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, 111, 18], radius=9, fill=ROJO, outline=BLANCO, width=1)
    t = texto_rgba(GENERO, 1.0, BLANCO, glifos, F, contorno=0, sombra=0)
    im.alpha_composite(t, ((112 - t.width) // 2, (19 - t.height) // 2))
    return im


def _icono():
    return escudo(62, 49)


def _lcd(glifos, F):
    im = Image.new('RGBA', (134, 15), (0, 0, 0, 0))
    t = texto_rgba(NOMBRE, 0.86, (36, 54, 44, 255), glifos, F, contorno=0, sombra=0, escala_x=1.3)
    im.alpha_composite(t, ((134 - t.width) // 2, (15 - t.height) // 2))
    return im


def _tira(glifos, F):
    im = Image.new('RGBA', (271, 18), (0, 0, 0, 0))
    t = texto_rgba(NOMBRE + b' - ' + GENERO + b' -', 0.98, AZUL, glifos, F, contorno=1, sombra=0, escala_x=1.15)
    im.alpha_composite(t, (2, (18 - t.height) // 2))
    return im


def _portada(glifos, F):
    """caratula de la radio: funda, cielo, sol, calzada, taxi y pegatina (136x126)"""
    W, H = 136, 126
    im = Image.new('RGBA', (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rectangle([4, 4, W-1, H-1], fill=(0, 0, 0, 110))
    d.rectangle([0, 0, W-8, H-8], fill=(190, 194, 198, 255), outline=(70, 72, 76, 255))
    d.rectangle([5, 5, W-13, H-13], fill=(18, 18, 18, 255), outline=BLANCO)
    x0, y0, x1, y1 = 9, 9, W-17, H-17
    for yy in range(y0, y1):
        t = (yy - y0) / max(1, y1 - y0)
        d.line([(x0, yy), (x1, yy)], fill=(int(28 + 40*t), int(60 + 60*t), int(96 - 30*t), 255))
    d.ellipse([x1-40, y0+8, x1-12, y0+36], fill=AMARILLO)
    d.polygon([(x0+20, y1-8), (x0+44, y0+52), (x0+64, y0+52), (x0+92, y1-8)], fill=(30, 30, 34, 255))
    for yy in range(y0+54, y1-6, 8):
        d.line([(x0+50, yy), (x0+58, yy)], fill=AMARILLO, width=2)
    for j in range(14):
        d.rectangle([x0+j*7, y1-22, x0+j*7+6, y1-14], fill=(NEGRO if j % 2 == 0 else AMARILLO))
    if MODO_TAXI == 'negro':
        d.rectangle([x0+18, y1-38, x0+72, y1-24], fill=NEGRO)
        d.rectangle([x0+27, y1-47, x0+63, y1-37], fill=NEGRO)
        d.rectangle([x0+31, y1-44, x0+43, y1-37], fill=BLANCO)
        d.rectangle([x0+47, y1-44, x0+59, y1-37], fill=BLANCO)
        for cx in (x0+28, x0+62):
            d.ellipse([cx-5, y1-27, cx+5, y1-17], fill=NEGRO)
            d.ellipse([cx-2, y1-25, cx+2, y1-19], fill=(200, 200, 200, 255))
    else:
        area = (x0 + 10, y1 - 58, x0 + 80, y1 - 22)
        taxi = _emoji_fit(area[2]-area[0], area[3]-area[1])
        cx = (area[0] + area[2]) // 2; cy = (area[1] + area[3]) // 2
        d.ellipse([cx-28, area[3]-3, cx+28, area[3]+3], fill=(0, 0, 0, 70))
        im.alpha_composite(taxi, (cx - taxi.width//2, cy - taxi.height//2))
    t = texto_rgba(GENERO, 0.92, AMARILLO, glifos, F, contorno=1, sombra=1)
    im.alpha_composite(t, (x0+3, y0+2))
    t = texto_rgba(NOMBRE, 1.25, BLANCO, glifos, F, contorno=1, sombra=1)
    im.alpha_composite(t, (x0+4, y1-78))
    d.ellipse([x0-1, y1-28, x0+30, y1+1], fill=AMARILLO, outline=NEGRO, width=1)
    t = texto_rgba(b'ALL I', 0.68, NEGRO, glifos, F, contorno=0, sombra=0)
    im.alpha_composite(t, (x0+5, y1-24))
    t = texto_rgba(b'WANT', 0.68, NEGRO, glifos, F, contorno=0, sombra=0)
    im.alpha_composite(t, (x0+3, y1-15))
    return im


def genera(tex, spr, grp, pie, F):
    """{nombre: RGBA} de las seis piezas de la tarjeta; F necesita w, h, pri, n, esp, ker, met y gf (grupo FUENTE)"""
    F = _Fuente(F, grupo(grp, 'FUENTE'))
    gl = _glifos(tex, spr, pie, F)
    return {'logo': _logo(gl, F), 'tag': _tag(gl, F), 'icono': _icono(), 'lcd': _lcd(gl, F),
            'tira': _tira(gl, F), 'portada': _portada(gl, F)}


class _Fuente:
    """adapta la F14 de conv_menu (atributos) o la de un MENU.DAT ya hecho"""
    def __init__(s, F, gf):
        s.w, s.h, s.pri, s.n = F.w, F.h, F.pri, F.n
        s.esp, s.ker, s.met, s.gf = F.esp, F.ker, F.met, gf


def _pon(m, g, cuadro, si, x, y, ax, ay, sx=1.0, sy=1.0, al=1.0):
    m.pie.append((g, cuadro, si, 1 << 4, x, y, ax, ay, 0.0, sx, sy, al))   # duracion 1 cuadro (flags >> 4)


def anade(m, F):
    """anade la tarjeta al Menu de conv_menu.py (antes de escribe); devuelve los sprites nuevos"""
    art = genera(m.tex, m.spr, m.grp, m.pie, F)
    spr = {}
    for k, im in art.items():
        ti = m.textura(('menu', 'MUSICA2', 'all_i_want_' + k), np.array(im))
        spr[k] = m.sprite(ti, (0, 0, im.width, im.height), im.width, im.height)
    g_l2, g_t2 = grupo(m.grp, 'MUSICA_L2'), grupo(m.grp, 'MUSICA_T2')
    for k, w, h, x, y, ax, ay in PIEZAS:
        _pon(m, g_l2, CUADRO_CARRUSEL, spr[k], x, y, ax, ay)
    k, w, h, x, y, ax, ay = LCD
    _pon(m, g_t2, CUADRO_TITULO, spr[k], x, y, ax, ay)
    # los puntos de la lista (los 12 fijos del cuadro 0 y el resaltado del 460) y el rotulo "MUSICA DE FONDO
    # REMIX 1" (cuadro 18) no llegan al cuadro 500: se repiten alli para que la tarjeta nueva salga como las demas
    for p in list(m.pie):
        if p[0] != g_l2 or p[1] >= CUADRO_CARRUSEL: continue
        _, fr, si, _, x, y, ax, ay, ang, sx, sy, al = p
        ti, _, w, h = m.spr[si]
        if abs(y - 391.5) < 0.2 and fr in (0, 460):
            m.pie.append((g_l2, CUADRO_CARRUSEL, si, 1 << 4, x, y, ax, ay, ang, sx, sy, al))
        elif abs(y - 350.0) < 0.2 and w == 126 and h == 17:
            m.pie.append((g_l2, CUADRO_CARRUSEL, si, 1 << 4, x, y, ax, ay, ang, sx, sy, al))
    m.grp[g_l2]['n'] = CUADRO_CARRUSEL + 1
    m.grp[g_t2]['n'] = CUADRO_TITULO + 1
    # las piezas de un grupo tienen que ir seguidas (hud.c dibuja desde grp.pri hasta el siguiente grupo): se
    # reordenan todas al final, conservando el orden de los grupos y el de las piezas dentro de cada uno
    orden = []
    for g in range(len(m.grp)):
        m.grp[g]['pri'] = len(orden)
        orden += [p for p in m.pie if p[0] == g]
    m.pie = orden
    u_l2 = next(i for i, u in enumerate(m.lay) if u[0] == 'MUSICA_L2')
    u_t2 = next(i for i, u in enumerate(m.lay) if u[0] == 'MUSICA_T2')
    m.tramos += [('TEMA%d' % TEMA, u_l2, CUADRO_CARRUSEL, CUADRO_CARRUSEL, 2),
                 ('MUEVE1112', u_l2, CUADRO_CARRUSEL, CUADRO_CARRUSEL, 0),
                 ('MUEVE1211', u_l2, CUADRO_CARRUSEL, CUADRO_CARRUSEL, 0),
                 ('TEMA%d' % TEMA, u_t2, CUADRO_TITULO, CUADRO_TITULO, 2)]
    if m.pant:
        p = next((x for x in m.pant if x.get('nombre') == 'MUSICA2'), m.pant[-1])
        p['nt'] = len(m.tex) - p['t0']
    return spr
