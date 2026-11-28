"""menu_musica.py MENU.DAT cancion imagen salida/ : una tarjeta mas (SPLASH WAVE REMASTER) en el selector de musica.

Uso: python3 -I tools/menu_musica.py <MENU.DAT de entrada> <cancion.mp3|.wav|...> <imagen.jpg|.png> <dir de salida>
Escribe en <dir de salida> el MENU.DAT parcheado, MUS70.ADP (el tema entero, para la carrera) y MUS71.ADP (su avance,
el que suena en el selector), y menu_musica2.png (maqueta de la pantalla MUSICA2 con la tarjeta, para mirarla).

Por que un parche y no conv_menu.py: el MENU.DAT de los datos lleva arte que no sale de la ISO (la portada de
OutRunners: usos OR_PORTADA / OR_CREDITO), asi que no se puede regenerar desde cero. Aqui se lee el MENU.DAT que hay
(formato HUD1 de conv_hud.Hud.escribe con el bloque MNU1 de conv_menu.escribe), se le anaden texturas, sprites,
piezas y tramos, y se vuelve a escribir con la misma disposicion. Nada mas cambia (los indices de sprites, grupos y
usos que ya habia se quedan igual; las texturas de las pantallas que van detras de MUSICA2 se corren). Si el MENU.DAT
ya lleva la tarjeta (tramo TEMA12 del uso MUSICA_L2), se quita antes y se pone otra vez: el parche es repetible.

La tarjeta (tema 28 de comun/menu.c, la ultima de la lista 2, la de las remezclas): el selector del original lleva
12 temas en la lista 2 (escenas 0x440033 / 0x44002E: grupos MUSICA_L2 / MUSICA_T2) y cada tema es un cuadro quieto
del carrusel (20 + 40 k) y su titulo de la radio (110 cuadros desde 110 k). Se compone una tarjeta mas en los mismos
sitios que las del original (las del tema 11, NIGHT FLIGHT INSTRUMENTAL): rotulo "SPLASH WAVE" con la fuente 14 del
propio MENU.DAT (grupo FUENTE y metricas de MNU2; sin fuentes del sistema), etiqueta tipo genero "REMASTER BY SUPNEO",
icono, visor LCD de la radio, portada = la imagen del usuario (recortada al cuadro y escalada, sin redibujarla) y la
tira inferior; los puntos de la lista (13: se anade uno) y el rotulo de la lista se copian del cuadro del tema 11. Se
anaden los cuadros 499 y 500 del carrusel y 1320..1429 del titulo y los tramos TEMA12 y MUEVE1112 (499 -> 500).

Audio: el mp3 se decodifica con el ffmpeg de imageio-ffmpeg (tools/nativo.ffmpeg) a PCM estereo de 16 bits a
44 100 Hz (musica.c de la demo reproduce todos los temas a 44,1 kHz, como el original con el pitch 0xEB3 de
SOUNDCD.IRX) y tools/c/musica.c (entrada "pcm") lo pasa a ADPCM de Yamaha estereo, el mismo formato que MUSn.ADP.
El avance: AVANCE_S segundos desde AVANCE_INI con fundido de entrada y de salida (los del original son trozos de
~25 s del tema que el selector repite en bucle).

Los ficheros del usuario (cancion e imagen) no van al repo: tools/regenera.py corre este paso solo si estan en la
carpeta extras/ (EXTRAS), y el programa de Windows igual."""
import os, struct, subprocess, sys, tempfile
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

TEMA = 12                                 # cuadro del tema en la lista 2 (los 12 del original son 0..11)
CUADRO_CARRUSEL = 20 + 40 * TEMA          # 500
# la tarjeta se pone en los cuadros 499 y 500 y MUEVE1112 va del 499 al 500: un movimiento de 1 cuadro (con uno de 0,
# 500 -> 500, el menu da el movimiento por acabado en el mismo paso, no pone el tramo TEMA12 ni pide el avance)
C0 = CUADRO_CARRUSEL - 1
CUADRO_TITULO = 110 * TEMA                # 1320
# Mod taxi: la lista 2 ya lleva ALL I WANT en el sitio 12 (tools/menu_allwant.py, conv_menu.py), asi que esta tarjeta va
# detras, en el 13 (cuadros 539 / 540, titulo 1430..1539, tramos TEMA13 y MUEVE1213; tema 29 de comun/menu.c) con la
# marca 'SWR2'. Con un MENU.DAT sin ALL I WANT (el de la clasica) o con la marca 'SWR1', en el 12 como en la clasica.
MARCA_12, MARCA_13 = 0x31525753, 0x32525753   # 'SWR1' / 'SWR2'
C_REF = 20 + 40 * 11                      # 460: el cuadro del tema 11 (rotulo de la lista, icono y portada de muestra)


def pon_tema(t):
    """la tarjeta en el sitio t de la lista 2 (12 en la clasica, 13 detras de ALL I WANT en el Mod)"""
    global TEMA, CUADRO_CARRUSEL, C0, CUADRO_TITULO, MARCA
    TEMA = t; CUADRO_CARRUSEL = 20 + 40 * t; C0 = CUADRO_CARRUSEL - 1; CUADRO_TITULO = 110 * t
    MARCA = MARCA_13 if t == 13 else MARCA_12


def sitio(D):
    """13 si el MENU.DAT lleva ALL I WANT (un tramo TEMA12 en MUSICA_L2 sin la marca 'SWR1') o ya la marca 'SWR2'"""
    m = D.pan[D.p('MUSICA2')][8]
    if m == MARCA_13: return 13
    if m == MARCA_12: return 12
    u = D.u('MUSICA_L2')
    return 13 if any(t[1] == u and t[0].rstrip(b'\0') == b'TEMA12' for t in D.tra) else 12
PISTA = 70                                # MUS70.ADP: el tema (comun/musica.h MUSICA_PISTA_EXTRA)
PISTA_AVANCE = 71                         # MUS71.ADP: el avance (MUSICA_AVANCE_EXTRA)
NOMBRE = b'SPLASH WAVE'
GENERO = b'REMASTER BY SUPNEO'
AVANCE_INI, AVANCE_S, FUNDIDO_S = 30.0, 26.0, 1.5
EXTRAS = ('splash_wave_remaster.mp3', 'supneo.jpg')   # nombres en extras/ (regenera.py, windows/outrun2dc.py)

# colores medidos en las piezas del tema 11 (mediana de los texeles opacos): rotulo, tira y visor
COL_ROTULO = (68, 102, 102, 255)
COL_TIRA = (85, 102, 102, 255)
COL_LCD = (68, 187, 255, 255)
COL_TAG = (0, 136, 204, 255)              # (las del original: rojo, naranja, morado, verde; esta, azul)
BLANCO = (255, 255, 255, 255)
TAG_MAX = 150
PORTADA_DIBUJO = (13, 3, 132, 123)        # el dibujo dentro de la caja de CD de las portadas (136x126)

# (pieza, ancho, alto, x, y, ancla x, ancla y): los sitios de las piezas del tema 11 en el cuadro 460
PIEZAS = (
    ('logo', 234, 31, 363.5, 113.0, 243.0, 15.0),
    ('tag', 112, 19, 210.0, 142.0, 87.5, 9.5),            # (ancla x 87,5: el borde izquierdo en 122,5 como todas)
    ('portada', 136, 126, 507.0, 265.0, 68.0, 63.0),
    ('tira', 271, 18, 217.0, 370.9, 151.1, 8.0),
)
LCD = ('lcd', 134, 15, 342.1, 271.0, 171.0, 7.0)   # (el visor alterna: nombre / etiqueta, 55 cuadros cada uno)
TITULO_CUADROS = 110                      # cuadros del titulo de cada tema (en bucle, como los del original)


# ---------------------------------------------------------------- MENU.DAT (HUD1 + MNU1)
class Dat:
    """MENU.DAT leido: tablas como listas de tuplas, texturas como (W, H, fmt, lista, bytes)"""

    def __init__(s, ruta):
        d = open(ruta, 'rb').read()
        assert d[:4] == b'HUD1', '%s: no es un HUD1' % ruta
        c = struct.unpack_from('<11I', d, 4)
        nt, ns, ng, npz, nl = c[:5]; o_t, o_s, o_g, o_p, o_l, o_taco = c[5:]
        s.tex = []
        for i in range(nt):
            W, H, fmt, li, off, tam = struct.unpack_from('<4H2I', d, o_t + 16 * i)
            s.tex.append((W, H, fmt, li, d[off:off + tam]))
        s.spr = [struct.unpack_from('<4H4f', d, o_s + 24 * i) for i in range(ns)]
        s.grp = []
        for i in range(ng):
            nom, n, pri, cnt, z, paso, z2 = struct.unpack_from('<16s4HfI', d, o_g + 32 * i)
            s.grp.append([nom.rstrip(b'\0').decode(), n, pri, cnt, paso])
        s.pie = [struct.unpack_from('<4H8f', d, o_p + 40 * i) for i in range(npz)]
        s.lay = [struct.unpack_from('<16s2H4h2H', d, o_l + 32 * i) for i in range(nl)]
        s.taco = d[o_taco:o_taco + 16]
        ini = min(t[4] and struct.unpack_from('<I', d, o_t + 16 * i + 8)[0] for i, t in enumerate(s.tex))
        x = d[o_taco + 16:ini]
        s.raw = None
        if x[:4] != b'MNU1':                            # (otro bloque, p. ej. SPR1 de SPFE.DAT: tal cual)
            s.raw = bytes(x); s.pan = []; s.tra = []; s.mnu2 = b''; return
        s.npan, s.ntra, s.o2 = struct.unpack_from('<3I', x, 4)
        s.pan = [list(struct.unpack_from('<16s6HI4I', x, 16 + 48 * i)) for i in range(s.npan)]
        o = 16 + 48 * s.npan
        s.tra = [list(struct.unpack_from('<16s4H2I', x, o + 32 * i)) for i in range(s.ntra)]
        s.mnu2 = bytes(x[s.o2:]) if s.o2 else b''

    # nombres
    def g(s, nombre): return next(i for i, g in enumerate(s.grp) if g[0] == nombre)
    def u(s, nombre): return next(i for i, u in enumerate(s.lay) if u[0].rstrip(b'\0').decode() == nombre)
    def p(s, nombre): return next(i for i, p in enumerate(s.pan) if p[0].rstrip(b'\0').decode() == nombre)

    def rgba(s, ti):
        import vq
        W, H, fmt, li, b = s.tex[ti]
        if fmt & 0x300: return vq.decodifica(b, W, H, fmt & 0xff, ((fmt >> 16) & 0xff) + 1 if fmt >> 16 else 256)
        return vq.decodifica16(b, W, H, fmt & 0xff)

    def fuente(s):
        """metricas de la fuente 14 (MNU2: u16 ancho, alto, primero, n; i16 espaciado, u16 hay interletraje;
        i8 metricas[2n]; i8 inter[n*n])"""
        h = struct.unpack_from('<8I', s.mnu2, 0); f = h[3]
        w, hh, pri, n, esp, ker = struct.unpack_from('<4HhH', s.mnu2, f)
        met = np.frombuffer(s.mnu2, np.int8, 2 * n, f + 12).reshape(-1, 2).astype(int)
        inter = np.frombuffer(s.mnu2, np.int8, n * n, f + 12 + 2 * n).astype(int) if ker else None
        return _F(w, hh, pri, n, esp, met, inter, s.g('FUENTE'))

    def escribe(s, ruta):
        """la misma disposicion que conv_hud.Hud.escribe + conv_menu.escribe"""
        n = [len(s.tex), len(s.spr), len(s.grp), len(s.pie), len(s.lay)]
        offs = []; o = 48
        for tam in (16 * n[0], 24 * n[1], 32 * n[2], 40 * n[3], 32 * n[4], 16):
            offs.append(o); o += tam
        o_taco = offs[5]
        if s.raw is not None: blq = bytearray(s.raw)
        else: blq = bytearray(b'MNU1' + struct.pack('<3I', len(s.pan), len(s.tra), (16 + 48 * len(s.pan) + 32 * len(s.tra))
                                              if s.mnu2 else 0))
        if s.raw is None:
            for p in s.pan: blq += struct.pack('<16s6HI4I', *p)
            for t in s.tra: blq += struct.pack('<16s4H2I', *t)
            blq += s.mnu2
        ini = (o_taco + 16 + len(blq) + 31) & ~31
        out = bytearray(b'HUD1' + struct.pack('<11I', *n, *offs)); datos = bytearray()
        for W, H, fmt, li, b in s.tex:
            out += struct.pack('<4H2I', W, H, fmt, li, ini + len(datos), len(b)); datos += b; datos += bytes((-len(datos)) % 32)
        for x in s.spr: out += struct.pack('<4H4f', *x)
        cuenta = {}
        for p in s.pie: cuenta[p[0]] = cuenta.get(p[0], 0) + 1
        for i, (nom, nn, pri, _, paso) in enumerate(s.grp):
            out += struct.pack('<16s4HfI', nom.encode(), nn, pri, cuenta.get(i, 0), 0, paso, 0)
        for p in s.pie: out += struct.pack('<4H8f', *p)
        for u in s.lay: out += struct.pack('<16s2H4h2H', *u)
        out += s.taco; assert len(out) == o_taco + 16
        out += blq; out += bytes(ini - len(out)); out += datos
        open(ruta, 'wb').write(out)
        return len(out)


class _F:
    def __init__(s, w, h, pri, n, esp, met, ker, gf):
        s.w, s.h, s.pri, s.n, s.esp, s.met, s.ker, s.gf = w, h, pri, n, esp, met, ker, gf


class Vista:
    """adaptador para conv_hud.dibuja_grupo (texturas RGBA y sprites en pixeles)"""
    def __init__(s, D):
        s.D = D; s._t = {}
        s.grp = [dict(nombre=g[0], n=g[1], pri=g[2], paso=g[4]) for g in D.grp]
        s.pie = D.pie
        s.spr = []
        for ti, w, h, _, u0, v0, u1, v1 in D.spr:
            W, H = D.tex[ti][:2]; s.spr.append((ti, (u0 * W, v0 * H, u1 * W, v1 * H), w, h))
        s.tex = _Texs(s)


class _Texs:
    def __init__(s, v): s.v = v
    def __getitem__(s, i):
        if i not in s.v._t: s.v._t[i] = s.v.D.rgba(i)
        return s.v._t[i]


# ---------------------------------------------------------------- texto con la fuente 14 del MENU.DAT
def _par(F, c, sig):
    i = c - F.pri
    if F.ker is not None and sig and 0 <= sig - F.pri < F.n: return int(F.met[i][1] - F.ker[i * F.n + sig - F.pri])
    return int(F.met[i][1] + F.esp)


def ancho(F, s, track=0.0):
    a = 0
    for i, c in enumerate(s):
        sig = s[i + 1] if i + 1 < len(s) else 0
        if c == 32: a += int(F.w * 0.3); continue
        a = _par(F, c, 0 if sig == 32 else sig) + a + F.esp + track
    return a


def glifos(D, F):
    """RGBA de la celda de cada caracter del grupo FUENTE (cuadro = codigo - primero), con su desplazamiento"""
    from PIL import Image
    V = Vista(D); out = {}
    G = D.grp[F.gf]
    for p in D.pie[G[2]:G[2] + G[3]]:
        gg, fr, si, fl, x, y, ax, ay, ang, sx, sy, al = p
        ti, (l, t, r, b), w, h = V.spr[si]
        a = V.tex[ti]
        g = Image.fromarray(a[int(round(t)):int(round(b)), int(round(l)):int(round(r))], 'RGBA').resize((max(1, w), max(1, h)))
        im = out.setdefault(F.pri + fr, Image.new('RGBA', (F.w, F.h), (0, 0, 0, 0)))
        im.alpha_composite(g, (int(round(x - ax)), int(round(y - ay))))
    return out


def texto_rgba(s, escala, color, gl, F, contorno=1, sombra=0, SS=4, bold=0, escala_x=1.0, track=0.0):
    """texto F14 a la escala pedida (escala_x solo estira en horizontal; track: pixeles de mas entre letras, a escala
    1; bold: engorde en pixeles de la salida), con contorno blanco y sombra"""
    from PIL import Image, ImageFilter
    w = max(1, int(np.ceil(ancho(F, s, track) * escala * escala_x))); h = max(1, int(np.ceil(F.h * escala)))
    pad = max(1, contorno + (1 if sombra else 0)) + 1
    W, H = (w + 2 * pad) * SS, (h + 2 * pad) * SS
    cap = Image.new('RGBA', (W, H), (0, 0, 0, 0)); x = pad
    for i, c in enumerate(s):
        sig = s[i + 1] if i + 1 < len(s) else 0
        if c == 32: x += int(F.w * 0.3 * escala * escala_x); continue
        g = gl.get(c)
        if g is not None:
            gw, gh = max(1, int(round(F.w * escala * escala_x * SS))), max(1, int(round(F.h * escala * SS)))
            cap.alpha_composite(g.resize((gw, gh), Image.LANCZOS), (int(round(x * SS)), pad * SS))
        x += (_par(F, c, 0 if sig == 32 else sig) + track) * escala * escala_x
    fill = Image.new('RGBA', (W, H), color); fill.putalpha(cap.getchannel('A'))
    if bold: fill.putalpha(fill.getchannel('A').filter(ImageFilter.MaxFilter(int(2 * bold * SS) | 1)))
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    if sombra:
        sh = Image.new('RGBA', (W, H), (0, 0, 0, 0)); sh.paste(fill, (SS * sombra, SS * sombra))
        sh.putalpha(sh.getchannel('A').point(lambda v: int(v * 0.55))); out.alpha_composite(sh)
    if contorno:
        m = fill.getchannel('A').filter(ImageFilter.MaxFilter(2 * contorno * SS + 1))
        o = Image.new('RGBA', (W, H), BLANCO); o.putalpha(m); out.alpha_composite(o)
    out.alpha_composite(fill)
    return out.resize((w + 2 * pad, h + 2 * pad), Image.LANCZOS)


# ---------------------------------------------------------------- las piezas de la tarjeta
def _cuadrada(img, w, h):
    """la imagen del usuario recortada al centro con la proporcion w:h y escalada (sin retocarla)"""
    from PIL import Image
    im = img.convert('RGBA'); W, H = im.size
    if W * h > H * w: nw = H * w // h; im = im.crop(((W - nw) // 2, 0, (W - nw) // 2 + nw, H))
    else: nh = W * h // w; im = im.crop((0, (H - nh) // 2, W, (H - nh) // 2 + nh))
    return im.resize((w, h), Image.LANCZOS)


def _ajusta(t, w):
    from PIL import Image
    return t.resize((w, t.height), Image.LANCZOS) if t.width > w else t


def _logo(gl, F):
    """el rotulo: como los del original (sprite 234x31, texto del color de los rotulos pegado a la izquierda)"""
    from PIL import Image
    im = Image.new('RGBA', (234, 31), (0, 0, 0, 0))
    t = texto_rgba(NOMBRE, 1.75, COL_ROTULO, gl, F, contorno=0, bold=0.75, track=1.5)
    im.alpha_composite(_ajusta(t, 234), (0, (31 - t.height) // 2 + 1))
    return im


def _tag(gl, F):
    """la etiqueta (la del genero en las del original: pastilla de color con el texto en blanco); ancho segun el texto"""
    from PIL import Image, ImageDraw
    t = texto_rgba(GENERO, 0.8, BLANCO, gl, F, contorno=0, bold=0.3, track=1.0)
    w = min(TAG_MAX, t.width + 10)
    im = Image.new('RGBA', (w, 19), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 1, w - 1, 17], radius=8, fill=COL_TAG)
    t = _ajusta(t, w - 8)
    im.alpha_composite(t, ((w - t.width) // 2, (19 - t.height) // 2))
    return im


def _lcd(gl, F, txt):
    """el visor de la radio: texto azul claro de puntos, como el del original (MUSICA_T2)"""
    from PIL import Image
    im = Image.new('RGBA', (134, 15), (0, 0, 0, 0))
    t = _ajusta(texto_rgba(txt, 0.85, COL_LCD, gl, F, contorno=0, escala_x=1.1, track=1.0), 134)
    im.alpha_composite(t, ((134 - t.width) // 2, (15 - t.height) // 2))
    a = np.array(im); yy, xx = np.mgrid[:15, :134]
    a[..., 3] = np.where((xx % 2 == 0) & (yy % 2 == 1), a[..., 3], (a[..., 3] * 0.25).astype(np.uint8))
    return Image.fromarray(a)


def _tira(gl, F):
    """la tira de abajo: el nombre grande y "- etiqueta -" mas pequeno, del color de la del original"""
    from PIL import Image
    im = Image.new('RGBA', (271, 18), (0, 0, 0, 0))
    a = texto_rgba(NOMBRE, 1.1, COL_TIRA, gl, F, contorno=0, bold=0.5, track=1.0)
    b = texto_rgba(b' - ' + GENERO + b' -', 0.75, COL_TIRA, gl, F, contorno=0, bold=0.3, track=1.0)
    x = 0; im.alpha_composite(a, (x, 18 - a.height + 1 if a.height < 19 else 0)); x += a.width - 6
    b = _ajusta(b, 271 - x)
    im.alpha_composite(b, (x, 18 - b.height))
    return im


def _portada(D, img):
    """la caratula de la radio: la caja de CD de la del tema 11 (sprite del cuadro 460, 136x126) con la imagen del
    usuario en el sitio del dibujo (13..132 x 3..123)"""
    from PIL import Image
    V = Vista(D); g = D.g('MUSICA_L2'); c11 = C_REF
    si = next(p[2] for p in D.pie if p[0] == g and p[1] <= c11 < p[1] + max(1, p[3] >> 4)
              and abs(p[4] - 507.0) < 0.5 and abs(p[5] - 265.0) < 0.5)
    ti, (l, t, r, b), w, h = V.spr[si]
    assert (w, h) == (136, 126), 'portada del tema 11: %dx%d' % (w, h)
    im = Image.fromarray(V.tex[ti][int(round(t)):int(round(b)), int(round(l)):int(round(r))].copy(), 'RGBA')
    x0, y0, x1, y1 = PORTADA_DIBUJO
    im.paste(_cuadrada(img, x1 - x0, y1 - y0), (x0, y0))
    return im


def genera(D, img):
    """{nombre: RGBA} de las piezas nuevas de la tarjeta (el icono es el de la lista 2, el del original)"""
    F = D.fuente(); gl = glifos(D, F)
    return {'logo': _logo(gl, F), 'tag': _tag(gl, F), 'lcd': _lcd(gl, F, NOMBRE), 'lcd2': _lcd(gl, F, b'(' + GENERO + b')'),
            'tira': _tira(gl, F), 'portada': _portada(D, img)}


# ---------------------------------------------------------------- el parche
MARCA = MARCA_13                         # 'SWR2' ('SWR1' en el 12) en el primer campo libre de MUSICA2 en MNU1 (menu.c)


def quita(D):
    """quita la tarjeta de mas si ya la hay (un MENU.DAT parcheado antes, o la ALL I WANT que quedo en los datos de
    la Rev 1.1): las piezas de los cuadros 500.. y 1320.., sus tramos, las texturas de MUSICA2 que solo usaban esas
    piezas y los sprites de esas texturas (al final de la tabla). Devuelve las texturas quitadas"""
    g_l2, g_t2 = D.g('MUSICA_L2'), D.g('MUSICA_T2')
    u_l2, u_t2 = D.u('MUSICA_L2'), D.u('MUSICA_T2')
    pm = D.p('MUSICA2'); t0, nt = D.pan[pm][5], D.pan[pm][6]
    mas = lambda p: (p[0] == g_l2 and p[1] >= C0) or (p[0] == g_t2 and p[1] >= CUADRO_TITULO)
    suyas = {D.spr[p[2]][0] for p in D.pie if mas(p)}
    otras = {D.spr[p[2]][0] for p in D.pie if not mas(p)}
    ts = sorted(t for t in suyas - otras if t0 <= t < t0 + nt)
    D.pie = [p for p in D.pie if not mas(p)]
    D.grp[g_l2][1] = min(D.grp[g_l2][1], CUADRO_CARRUSEL - 40 + 1); D.grp[g_t2][1] = min(D.grp[g_t2][1], CUADRO_TITULO)
    fuera = {b'TEMA%d' % TEMA, b'MUEVE%d%d' % (TEMA - 1, TEMA), b'MUEVE%d%d' % (TEMA, TEMA - 1)}
    if TEMA == 13: fuera -= {b'TEMA12'}               # (el TEMA12 de MUSICA_L2 / T2 es el de ALL I WANT: se queda)
    D.tra = [t for t in D.tra if not (t[1] in (u_l2, u_t2) and t[0].rstrip(b'\0') in fuera)]
    _reordena(D)
    for t in reversed(ts): _quita_tex(D, t)
    D.pan[pm][8] = 0
    return ts


def _quita_tex(D, a):
    """quita la textura a: sus sprites tienen que estar al final de la tabla (los indices de los demas no cambian)"""
    while D.spr and D.spr[-1][0] == a: D.spr.pop()
    assert not any(s[0] == a for s in D.spr), 'textura %d: sprites suyos en medio de la tabla' % a
    assert not any(D.spr[p[2]][0] == a for p in D.pie)
    D.spr = [tuple([s[0] - 1 if s[0] > a else s[0]] + list(s[1:])) for s in D.spr]
    D.pan[D.p('MUSICA2')][7] -= len(D.tex[a][4])
    del D.tex[a]
    for p in D.pan:
        if p[5] > a: p[5] -= 1
        elif p[5] <= a < p[5] + p[6]: p[6] -= 1


def _reordena(D):
    """las piezas de cada grupo seguidas y en el orden de los grupos (hud.c dibuja desde pri, cuenta piezas)"""
    orden = []
    for g in range(len(D.grp)):
        D.grp[g][2] = len(orden); ps = [p for p in D.pie if p[0] == g]; orden += ps; D.grp[g][3] = len(ps)
    D.pie = orden


def _pon(D, g, cuadro, si, x, y, ax, ay, sx=1.0, sy=1.0, al=1.0, ang=0.0, dura=1):
    D.pie.append((g, cuadro, si, dura << 4, x, y, ax, ay, ang, sx, sy, al))   # dura: cuadros (flags >> 4)


def anade(D, img):
    """anade la tarjeta al MENU.DAT leido; devuelve el arte, las texturas quitadas y la VRAM de MUSICA2 (antes, despues)"""
    from modelo import textura_dc
    pon_tema(sitio(D))
    pm = D.p('MUSICA2'); vram0 = D.pan[pm][7]
    quitadas = quita(D)
    art = genera(D, img)
    t0, nt0 = D.pan[pm][5], D.pan[pm][6]
    # una textura (atlas de 512 x 128) con las seis piezas, en 16 bits sin VQ (texto y foto se ven de cerca): la
    # portada a la izquierda y las demas en filas a su derecha
    W = 512; pos = {'portada': (0, 0)}; x0 = art['portada'].width; x, y, fila = x0, 0, 0
    for k in ('tira', 'logo', 'lcd', 'lcd2', 'tag'):
        im = art[k]
        if x + im.width > W: x, y, fila = x0, y + fila, 0
        pos[k] = (x, y); x += im.width; fila = max(fila, im.height)
    y = max(art['portada'].height, y + fila); fila = 0
    H = 1 << (y + fila - 1).bit_length()
    atlas = np.zeros((H, W, 4), np.uint8)
    for k, (x, y) in pos.items(): atlas[y:y + art[k].height, x:x + art[k].width] = np.array(art[k])
    fmt, li, datos = textura_dc(atlas)
    ti = t0 + nt0
    D.tex.insert(ti, (W, H, fmt, li, bytes(datos)))
    D.spr = [tuple([s[0] + 1 if s[0] >= ti else s[0]] + list(s[1:])) for s in D.spr]
    for i, p in enumerate(D.pan):
        if i != pm and p[5] >= ti: p[5] += 1
    D.pan[pm][6] += 1; D.pan[pm][7] += len(datos); D.pan[pm][8] = MARCA
    spr = {}
    for k, (x, y) in pos.items():
        w, h = art[k].size; spr[k] = len(D.spr)
        D.spr.append((ti, w, h, 0, x / W, y / H, (x + w) / W, (y + h) / H))
    g_l2, g_t2 = D.g('MUSICA_L2'), D.g('MUSICA_T2')
    c11 = CUADRO_CARRUSEL - 40
    # lo que no es de la tarjeta en el cuadro del tema 11: el rotulo de la lista (126x17 en y = 350) y los puntos de
    # la lista (y = 391,5: 12 apagados cada 16 px y el encendido en el del tema); con 13 temas, uno mas y el encendido
    # en el nuevo. En el Mod (sitio 13) los puntos del cuadro de ALL I WANT (500: los 12 y el encendido) y dos mas, con el
    # encendido en el 14; el rotulo y el icono de la lista 2 siguen siendo los del tema 11 (ALL I WANT lleva su icono)
    puntos = []
    for p in list(D.pie):
        if p[0] != g_l2: continue
        _, fr, si, fl, x, y, ax, ay, ang, sx, sy, al = p
        w, h = D.spr[si][1:3]
        if abs(y - 391.5) < 0.3 and al > 0.5:
            if p[1] <= c11 < p[1] + max(1, p[3] >> 4): puntos.append(p)
        elif not (p[1] <= C_REF < p[1] + max(1, p[3] >> 4)): continue
        elif (abs(y - 350.0) < 0.3 and w == 126 and h == 17) or (abs(x - 83.0) < 0.3 and abs(y - 118.0) < 0.3):
            # (el rotulo de la lista y el icono de la lista 2, el mismo en sus 12 temas)
            D.pie.append((g_l2, C0, si, (fl & 15) | 2 << 4, x, y, ax, ay, ang, sx, sy, al))
    from collections import Counter
    cnt = Counter(p[2] for p in puntos)
    apagado = max(cnt, key=cnt.get) if cnt else None
    encendido = [p for p in puntos if p[2] != apagado]
    xs = sorted(p[4] for p in puntos if p[2] == apagado)
    assert len(xs) == 12 and len(encendido) == 1, 'puntos de la lista: %d apagados, %d encendidos' % (len(xs), len(encendido))
    paso = (xs[-1] - xs[0]) / 11; x13 = xs[-1] + paso
    for p in puntos:
        if p[2] == apagado: D.pie.append((g_l2, C0, p[2], (p[3] & 15) | 2 << 4) + tuple(p[4:]))
    q = [p for p in puntos if p[2] == apagado][0]
    for k in range(TEMA - 11):                          # (12: el 13.o; 13: el 13.o y el 14.o)
        D.pie.append((g_l2, C0, apagado, (q[3] & 15) | 2 << 4, xs[-1] + paso * (k + 1)) + tuple(q[5:]))
    x13 = xs[-1] + paso * (TEMA - 11)
    e = encendido[0]
    D.pie.append((g_l2, C0, e[2], (e[3] & 15) | 2 << 4, x13) + tuple(e[5:]))
    for k, w, h, x, y, ax, ay in PIEZAS: _pon(D, g_l2, C0, spr[k], x, y, ax, ay, dura=2)
    k, w, h, x, y, ax, ay = LCD
    m = TITULO_CUADROS // 2
    D.pie.append((g_t2, CUADRO_TITULO, spr['lcd'], m << 4, x, y, ax, ay, 0.0, 1.0, 1.0, 1.0))
    D.pie.append((g_t2, CUADRO_TITULO + m, spr['lcd2'], (TITULO_CUADROS - m) << 4, x, y, ax, ay, 0.0, 1.0, 1.0, 1.0))
    D.grp[g_l2][1] = CUADRO_CARRUSEL + 1; D.grp[g_t2][1] = CUADRO_TITULO + TITULO_CUADROS
    _reordena(D)
    u_l2, u_t2 = D.u('MUSICA_L2'), D.u('MUSICA_T2')
    for n, u, a, b, modo in (('TEMA%d' % TEMA, u_l2, CUADRO_CARRUSEL, CUADRO_CARRUSEL, 2),
                             ('MUEVE%d%d' % (TEMA - 1, TEMA), u_l2, C0, CUADRO_CARRUSEL, 0),
                             ('TEMA%d' % TEMA, u_t2, CUADRO_TITULO, CUADRO_TITULO + TITULO_CUADROS - 1, 1)):
        D.tra.append([n.encode(), u, a, b, modo, 0, 0])
    return art, quitadas, (vram0, D.pan[pm][7])


# ---------------------------------------------------------------- audio
def pcm(cancion, ruta, ini=None, dur=None):
    """la cancion a PCM s16le estereo a 44,1 kHz con el ffmpeg de imageio-ffmpeg"""
    import nativo
    cmd = [nativo.ffmpeg(), '-hide_banner', '-loglevel', 'error', '-y']
    if ini is not None: cmd += ['-ss', '%.3f' % ini]
    if dur is not None: cmd += ['-t', '%.3f' % dur]
    cmd += ['-i', cancion, '-vn', '-ac', '2', '-ar', '44100', '-f', 's16le', ruta]
    subprocess.run(cmd, check=True)


# Nivel: los temas del original (MUSn.ADP de carrera) van de -11 a -16 dBFS RMS (mediana -13,6; medido en la pista
# entera ya en ADPCM de Yamaha, tal como la lee el AICA) y la cancion del usuario, tal cual sale del mp3, a -18,6 (pico
# -5,6 dBFS): 5 dB por debajo de los temas del juego, con lo que las voces y el motor (que se mezclan contra el nivel
# del original, docs/PARIDAD.md "Pausa y mezcla de audio") la tapaban. Se lleva la cancion entera (y su avance con la
# misma ganancia) a NIVEL_DBFS RMS sin pasar el pico de PICO_DBFS (sin recortar).
NIVEL_DBFS = -13.6
PICO_DBFS = -0.5


def ganancia(cancion):
    """el factor que deja la cancion entera en NIVEL_DBFS RMS sin que el pico pase de PICO_DBFS"""
    with tempfile.TemporaryDirectory() as tmp:
        r = os.path.join(tmp, 'in.pcm'); pcm(cancion, r)
        a = np.fromfile(r, '<i2').astype(np.float64)
    if not len(a): return 1.0
    rms = np.sqrt(np.mean(a * a)) / 32768; pico = np.abs(a).max() / 32768
    if rms <= 0 or pico <= 0: return 1.0
    return min(10 ** (NIVEL_DBFS / 20) / rms, 10 ** (PICO_DBFS / 20) / pico)


def adp(cancion, salida, ini=None, dur=None, fundido=0.0, gan=1.0):
    import nativo
    with tempfile.TemporaryDirectory() as tmp:
        r = os.path.join(tmp, 'in.pcm'); pcm(cancion, r, ini, dur)
        if fundido or gan != 1.0:
            a = np.fromfile(r, '<i2').reshape(-1, 2).astype(np.float32) * np.float32(gan); n = int(fundido * 44100)
            if fundido and len(a) > 2 * n:
                e = np.linspace(0, 1, n, dtype=np.float32)[:, None]; a[:n] *= e; a[-n:] *= e[::-1]
            a.round().clip(-32768, 32767).astype('<i2').tofile(r)
        subprocess.run([nativo.programa('musica'), r, salida, '-', 'pcm'], check=True)


def tiene(ruta):
    """1 si el MENU.DAT ya lleva la tarjeta (la marca de MUSICA2: 'SWR2' en el Mod, detras de ALL I WANT)"""
    try: D = Dat(ruta); return D.pan[D.p('MUSICA2')][8] == MARCA_13
    except (OSError, AssertionError, StopIteration, struct.error): return False


def maqueta(D, ruta):
    """pantalla MUSICA2 con la tarjeta (como conv_menu.maquetas) y el titulo de la radio"""
    from PIL import Image
    from conv_hud import dibuja_grupo
    V = Vista(D); img = Image.new('RGBA', (640, 480), (54, 62, 74, 255))
    lst = [('FONDO', 0), ('CABECERA_MUSICA', 90), ('MUSICA_RADIO', 25), ('MUSICA_AYUDA', 35),
           ('MUSICA_L2', CUADRO_CARRUSEL), ('MUSICA_T2', CUADRO_TITULO + 55), ('MUSICA_LISTA', 0), ('AYUDA_MUSICA', 0)]
    lay = {u[0].rstrip(b'\0').decode(): u for u in D.lay}
    for n, c in sorted(lst, key=lambda x: lay[x[0]][6]):
        u = lay[n]; G = V.grp[u[1]]; dibuja_grupo(img, V, u[1], min(c, G['n'] - 1), u[3], u[4])
    img.save(ruta)


def main(a):
    if len(a) < 4: sys.exit(__doc__)
    from PIL import Image
    menu, cancion, imagen, sal = a[:4]
    os.makedirs(sal, exist_ok=True)
    D = Dat(menu); img = Image.open(imagen)
    art, quitadas, (v0, v1) = anade(D, img)
    n = D.escribe(os.path.join(sal, 'MENU.DAT'))
    print('MENU.DAT: %d bytes; tarjeta en el sitio %d de la lista 2 (tema %d); texturas de una tarjeta anterior quitadas %s;'
          ' MUSICA2 VRAM %d -> %d KB' % (n, TEMA, 16 + TEMA, quitadas, v0 // 1024, v1 // 1024))
    maqueta(Dat(os.path.join(sal, 'MENU.DAT')), os.path.join(sal, 'menu_musica2.png'))
    g = ganancia(cancion)
    print('cancion: ganancia %.2f (%.1f dB) hasta %.1f dBFS RMS' % (g, 20 * np.log10(g), NIVEL_DBFS))
    adp(cancion, os.path.join(sal, 'MUS%d.ADP' % PISTA), gan=g)
    adp(cancion, os.path.join(sal, 'MUS%d.ADP' % PISTA_AVANCE), AVANCE_INI, AVANCE_S, FUNDIDO_S, gan=g)
    for k in (PISTA, PISTA_AVANCE):
        f = os.path.join(sal, 'MUS%d.ADP' % k); print('MUS%d.ADP: %d bytes (%.1f s a 44,1 kHz)' % (
            k, os.path.getsize(f), os.path.getsize(f) / 44100))


if __name__ == '__main__':
    main(sys.argv[1:])
