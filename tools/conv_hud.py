"""HUD de carrera (modo OutRun) de OutRun 2006 Coast 2 Coast (PS2) -> HUD.DAT para la Dreamcast.

Uso: python3 conv_hud.py <iso> <dir_salida> [dir_previas]
Escribe <dir_salida>/HUD.DAT y, en dir_previas (por defecto el directorio de salida), PNG de las texturas con
los rectangulos de los sprites y una maqueta del HUD completo a 640x480.

Origen de los datos (todo se lee del disco al convertir):
 - SPRANI.PS2: animaciones 2D "SPRANI" (tipo After Effects). El paquete esta en orden alfabetico (sin distinguir
   mayusculas) de los nombres ani_SPRANI_*.sz que aparecen en el ejecutable. Banco 0x2C = ETC_CVT (casi todo el HUD),
   bancos 0x23..0x2A, 0x30, 0x31 = METER_<coche> (cuentarrevoluciones).
 - SPRITE.PS2: ficheros XST con las texturas de cada banco (mismo orden alfabetico de spr_*xst.sz, sin las
   variantes J; los "_Exst" son los ingleses).
 - SLES_539.98: tablas del modulo NAVI (fuentes de digitos, marchas, tacometro por modelo). Las coordenadas de
   pantalla son constantes del codigo MIPS (iguales a las del port de PC, race_hud_navi.cpp).

Formato SPRANI (tras la palabra de tamano; punteros relativos a ese punto): tabla de escenas terminada en 0;
escena {n comp, comp*, n footage, footage*}; componente 0x24 {i16 ancho, alto, f32 inicio, fin, fps, .., +1C n
capas, +20 capas*}; capa 0x4C {i16 primero, ultimo, desfase, estiramiento/4096, +8 footage o componente, +C padre,
+10 ancla xy, +18 pos xy, +20 escala xy %, +28 rot grados, +2C opacidad %, +30 6 canales (pos x, y, escala x, y,
rot, opacidad), +48 flags (1 footage, 2 visible, 8 mascara), +49 mascara de canales}; canal {i16 n, cursor,
u8 desplazamiento valor, desplazamiento tangente, claves {t, v, entrada, salida} i16, Hermite}; footage 0x18
{ancho, alto, ritmo, n cuadros, cuadros*}; cuadro 0x14 {textura, f32 izq, arr, der, abj en pixeles}.
La escena es su ultimo componente, centrado en 640x480; 429530(token, x, y, capa, cuadro) la traslada (x, y).
Formato XST: u32 tamano; cabecera {0, meta, n tex, 0, n grupos, grupos*, n imagenes, imagenes*} (relativa a +4);
en +4+meta una tabla de n tex desplazamientos a cabeceras de 56 bytes (las de STAGE.PS2: 0 datos, 1 paleta,
3 qwords subidos, 11 log2 ancho/psm, 12 log2 alto); bit 0x8000 de la palabra 11 = datos comprimidos LZ
(bit de bandera 1: 12 bits de distancia + 4 de longitud+3; distancia 0: relleno de 16 bits+16 veces un byte).
Imagen 0x1C {textura, f32 u0 v0 u1 v1, u16 izq arr der abj}.

HUD.DAT (little endian):
 cabecera 48 bytes: 'HUD1', n_tex, n_spr, n_grp, n_pie, n_lay, off_tex, off_spr, off_grp, off_pie, off_lay,
   off_tacometros (u32 cada uno)
 texturas 16 bytes: u16 ancho, alto, formato (0 ARGB1555, 1 RGB565, 2 ARGB4444), lista (0 opaca, 1 recorte,
   2 transparente); u32 desplazamiento (desde el inicio del fichero, alineado a 32), tamano. Texeles twiddled.
 sprites 24 bytes: u16 textura, ancho, alto (pixeles en pantalla a 640x480), 0; f32 u0, v0, u1, v1.
 grupos (elementos graficos) 32 bytes: char nombre[16]; u16 n_cuadros, primera pieza, n piezas, 0;
   f32 paso (grados por cuadro de las piezas que giran), u32 0.
 piezas 40 bytes: u16 grupo, cuadro, sprite, flags (1 = gira con el cuadro: angulo + paso * cuadro);
   f32 x, y, ax, ay, angulo, sx, sy, alfa. Un punto p del sprite (pixeles 0..ancho, 0..alto) va a pantalla en
   ((p - (ax, ay)) * (sx, sy)) girado 'angulo' grados en sentido horario (y hacia abajo) + (x, y) + posicion de
   uso. Las piezas de un grupo con varios cuadros solo se dibujan si su cuadro es el pedido.
 usos (disposicion) 32 bytes: char nombre[16]; u16 grupo, grupo alternativo (0xFFFF = ninguno); i16 x, y
   (posicion a 640x480 que se suma a las piezas), avance (pixeles por caracter en los numeros, 0 si no es numero),
   capa (orden de dibujo: menor primero); u16 n caracteres, 0.
   Numeros: el caracter i se dibuja con el cuadro = digito en (x + i * avance, y); un espacio solo avanza.
 tacometros: 16 bytes: indice de grupo TACO_* para cada modelo de coche (byte car+0x11), 0xFF = ninguno.
"""
import os, re, struct, sys, zlib
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.iso import Iso
from ps2 import gs, tex
from modelo import textura_dc

# ---------------------------------------------------------------- disco
def lz(src, n):
    """descompresor LZ de las texturas XST (rutina 0x165788 del ejecutable)"""
    out = bytearray(); i = 0; bits = 0; f = 0
    while len(out) < n and i < len(src):
        if bits == 0: f = src[i]; i += 1; bits = 8
        bits -= 1; b = f & 0x80; f <<= 1
        if b:
            b1, b2 = src[i], src[i + 1]; i += 2
            dist = (b1 << 4) | (b2 >> 4)
            if dist:
                for _ in range((b2 & 15) + 3): out.append(out[-dist])
            else:
                ln = ((b2 << 8) | src[i]) + 16; out += bytes([src[i + 1]]) * ln; i += 2
        else:
            out.append(src[i]); i += 1
    return bytes(out)

class Disco:
    def __init__(self, ruta):
        self.iso = Iso(ruta)
        o, s = self.iso.fich['SLES_539.98']; self.elf = bytes(self.iso.m[o:o + s])
        nombres = lambda pat: sorted(set(m.group().decode().split('\\')[-1].lower()
                                         for m in re.finditer(pat, self.elf)))
        self.ani = re.findall(rb'\\SPRANI\\(ani_[A-Za-z0-9_]+)\.sz', self.elf)   # orden de bancos 0x20..
        self.ani = [x.decode() for x in self.ani]
        self.n_ani = nombres(rb'\\SPRANI\\ani_[A-Za-z0-9_]+\.sz')
        self.n_spr = [n for n in nombres(rb'(?i)\\sprite\\spr_[A-Za-z0-9_]+\.sz') if not n.lower().endswith('_jxst.sz')]
        self.p_ani = self.iso.paquete('SPRANI.PS2'); self.p_spr = self.iso.paquete('SPRITE.PS2')
        assert len(self.p_ani) == len(self.n_ani), 'SPRANI.PS2 no cuadra con los nombres del ejecutable'
        # SPRITE.PS2 tiene una entrada mas (sin nombre en el ejecutable) detras de ranking_cvt: los bancos del HUD
        # estan antes y no se ven afectados (se comprueba en banco())
        assert len(self.p_spr) in (len(self.n_spr), len(self.n_spr) + 1), 'SPRITE.PS2 no cuadra'

    def _ent(self, p, k):
        return zlib.decompress(self.iso.m[p[k][1]:p[k][1] + p[k][2]])

    def banco(self, b):
        """(Ani, texturas RGBA, datos XST) del banco b (0x20..): animacion y XST ingles"""
        nom = self.ani[b - 0x20]                                # ani_SPRANI_ETC_CVT
        clave = nom[len('ani_SPRANI_'):].lower()
        ia = self.n_ani.index(nom.lower() + '.sz')
        cand = ['spr_sprani_%s_exst.sz' % clave, 'spr_sprani_%s_xst.sz' % clave]
        lo = self.n_spr
        ix = next(lo.index(c) for c in cand if c in lo)
        assert ix < lo.index('spr_sprani_ranking_cvt_exst.sz') or len(self.p_spr) == len(lo), 'indice de XST dudoso'
        xst = self._ent(self.p_spr, ix)
        return Ani(self._ent(self.p_ani, ia)), texturas(xst), xst

    def elf32(self, va, n=1):
        """palabras de .data del ejecutable (direccion virtual)"""
        o = va - 0x328700 + 0x228780
        return struct.unpack_from('<%dI' % n, self.elf, o)

def texturas(d):
    """texturas RGBA (alfa 0-255) de un XST"""
    H = d[4:]; meta, nt = struct.unpack_from('<II', H, 4); P = meta; res = []
    for k in range(nt):
        o = struct.unpack_from('<I', H, P + 4 * k)[0]
        h = struct.unpack_from('<14I', H, P + o)
        psm = (h[11] >> 16) & 0xff; W = 1 << (h[11] >> 24); Ht = 1 << (h[12] & 0xff); dd = P + h[0]
        lleno = W * Ht * {0: 32, 0x13: 8, 0x14: 4}[psm] // 8
        raw = lz(H[dd:], h[3] * 16) if h[11] & 0x8000 else H[dd:dd + h[3] * 16]
        raw = raw + bytes(lleno - len(raw))                     # lo no subido queda a cero
        if psm == 0: a = np.frombuffer(raw, 'u1').reshape(Ht, W, 4).copy()
        elif psm == 0x13:
            pal = np.frombuffer(H[P + h[1]:P + h[1] + 1024], 'u1').reshape(256, 4)
            a = pal[tex._SW[gs.desordena(raw, 0x13, W, Ht)]]
        else:
            pal = np.frombuffer(H[P + h[1]:P + h[1] + 96], 'u1').reshape(24, 4)
            a = pal[list(range(8)) + list(range(16, 24))][gs.desordena(raw, 0x14, W, Ht)]
        a = a.copy(); a[..., 3] = np.minimum(255, a[..., 3].astype(np.int32) * 2)
        res.append(a)
    return res

def imagen_xst(d, k):
    """(textura, izq, arr, der, abj) de la imagen k de un XST (tokens de 42D280)"""
    H = d[4:]; io = struct.unpack_from('<I', H, 0x1c)[0]
    e = struct.unpack_from('<I4f4H', H, io + 0x1c * k)
    return e[0], e[5], e[6], e[7], e[8]

# ---------------------------------------------------------------- SPRANI
class Ani:
    def __init__(s, d):
        s.a = d[4:]; s.escenas = []; t = 0
        while struct.unpack_from('<I', s.a, t)[0]:
            s.escenas.append(struct.unpack_from('<I', s.a, t)[0]); t += 4
    def u32(s, o): return struct.unpack_from('<I', s.a, o)[0]
    def i32(s, o): return struct.unpack_from('<i', s.a, o)[0]

def _t(x, y): m = np.eye(3); m[2, 0] = x; m[2, 1] = y; return m
def _s(x, y): return np.diag([x, y, 1.0])
def _r(g):
    c, s = np.cos(np.radians(g)), np.sin(np.radians(g)); return np.array([[c, s, 0], [-s, c, 0], [0, 0, 1.0]])

def canal(a, ch, f):
    """valor de un canal en el cuadro f (48B970: Hermite entre claves)"""
    n = struct.unpack_from('<h', a.a, ch)[0]; sv = 1.0 / (1 << a.a[ch + 4]); st = 1.0 / (1 << a.a[ch + 5])
    K = [struct.unpack_from('<4h', a.a, ch + 6 + 8 * j) for j in range(n)]
    if f <= K[0][0]: return K[0][1] * sv
    if f >= K[-1][0]: return K[-1][1] * sv
    j = 1
    while K[j][0] < f: j += 1
    p, c = K[j - 1], K[j]
    if p[1] == c[1] and p[3] == c[2]: return p[1] * sv
    dt = c[0] - p[0]; t = (f - p[0]) / dt
    return ((2 * t ** 3 - 3 * t ** 2 + 1) * p[1] * sv + (3 * t ** 2 - 2 * t ** 3) * c[1] * sv
            + dt * (t ** 3 - 2 * t ** 2 + t) * p[3] * st + dt * (t ** 3 - t ** 2) * c[2] * st)

def capa_mat(a, L, f):
    v = list(struct.unpack_from('<8f', a.a, L + 0x10)); m = a.a[L + 0x49]
    for k in range(6):
        if m >> k & 1: v[2 + k] = canal(a, a.u32(L + 0x30 + 4 * k), f)
    M = _t(-v[0], -v[1]) @ _s(v[4] / 100, v[5] / 100) @ _r(v[6]) @ _t(v[2], v[3])
    if a.u32(L + 0xc): M = M @ capa_mat(a, a.u32(L + 0xc), f)[0]
    return M, v[7] / 100, (v[0], v[1]), bool(m & 0x10)

def _capa(a, L, f, M, al, out):
    if not a.u32(L + 8): return
    pri, ult, d4, e6 = struct.unpack_from('<4h', a.a, L); fl = a.a[L + 0x48]
    lm, op, anc, gira = capa_mat(a, L, f)
    if not (fl & 2 and pri <= f < ult) or fl & 8: return    # las mascaras no se exportan
    loc = (f - pri) * (e6 / 4096) + d4; al *= op; Mt = lm @ M; p = a.u32(L + 8)
    if fl & 1:
        w, h, ritmo, n, fr = struct.unpack_from('<iiiiI', a.a, p)
        if n < 1: return
        i = 0 if n == 1 else min(n - 1, int(ritmo * loc + 0.001))
        t, l, tp, r, b = struct.unpack_from('<I4f', a.a, fr + 20 * i)
        out.append(dict(tex=t & 0xffff, src=(l, tp, r, b), w=w, h=h, M=Mt, alfa=al, anc=anc, gira=gira, L=L, f=f))
    else:
        ini, fin = struct.unpack_from('<2f', a.a, p + 4)
        if not ini <= loc < fin: return
        pl = a.u32(p + 0x20)
        for k in range(a.i32(p + 0x1c) - 1, -1, -1): _capa(a, pl + k * 0x4c, loc, Mt, al, out)

def escena(a, k, f=0.0):
    """piezas de la escena k en el cuadro f, con la instancia en (0, 0)"""
    sc = a.escenas[k]; nc, pc = struct.unpack_from('<2I', a.a, sc); raiz = pc + (nc - 1) * 0x24
    w, h = struct.unpack_from('<hh', a.a, raiz); M = _t(320 - w / 2, 240 - h / 2); out = []
    pl = a.u32(raiz + 0x20)
    for q in range(a.i32(raiz + 0x1c) - 1, -1, -1): _capa(a, pl + q * 0x4c, f, M, 1.0, out)
    return out

# ---------------------------------------------------------------- conversion
class Hud:
    def __init__(s):
        s.tex = []; s.texkey = {}; s.spr = []; s.sprkey = {}; s.grp = []; s.pie = []; s.lay = []

    def textura(s, clave, a):
        if clave not in s.texkey: s.texkey[clave] = len(s.tex); s.tex.append(a)
        return s.texkey[clave]

    def sprite(s, ti, rect, w, h):
        k = (ti, tuple(round(v, 3) for v in rect), w, h)
        if k not in s.sprkey: s.sprkey[k] = len(s.spr); s.spr.append(k)
        return s.sprkey[k]

    def grupo(s, nombre, nframes=1, paso=0.0):
        s.grp.append(dict(nombre=nombre, n=nframes, pri=len(s.pie), paso=paso)); return len(s.grp) - 1

    def piezas_escena(s, g, cuadro, banco, texs, piezas, cero_gira=None):
        for p in piezas:
            ti = s.textura((banco, p['tex']), texs[p['tex']])
            si = s.sprite(ti, p['src'], p['w'], p['h'])
            M = p['M']; ax, ay = p['anc']
            x, y = (np.array([ax, ay, 1.0]) @ M)[:2]
            ang = np.degrees(np.arctan2(M[0, 1], M[0, 0]))
            sx, sy = np.hypot(M[0, 0], M[0, 1]), np.hypot(M[1, 0], M[1, 1])
            fl = 1 if p['gira'] and cero_gira is not None else 0
            if fl: ang = cero_gira
            s.pie.append((g, cuadro, si, fl, x, y, ax, ay, ang, sx, sy, p['alfa']))

    def piezas_imagen(s, g, banco, texs, xst, k, x, y):
        t, l, tp, r, b = imagen_xst(xst, k)
        ti = s.textura((banco, t), texs[t]); si = s.sprite(ti, (l, tp, r, b), r - l, b - tp)
        s.pie.append((g, 0, si, 0, x, y, 0, 0, 0.0, 1.0, 1.0, 1.0))

    def uso(s, nombre, g, x, y, avance=0, capa=0, nchar=0, alt=0xffff):
        s.lay.append((nombre, g, alt, x, y, avance, capa, nchar))

    def escribe(s, ruta):
        texs = []
        for a in s.tex:
            h, w = a.shape[:2]; W = max(8, 1 << (w - 1).bit_length()); H = max(8, 1 << (h - 1).bit_length())
            if (W, H) != (w, h): b = np.zeros((H, W, 4), np.uint8); b[:h, :w] = a; a = b
            texs.append((W, H) + textura_dc(a))
        n = [len(texs), len(s.spr), len(s.grp), len(s.pie), len(s.lay)]
        offs = []; o = 48
        for tam in (16 * n[0], 24 * n[1], 32 * n[2], 40 * n[3], 32 * n[4], 16):
            offs.append(o); o += tam
        o = (o + 31) & ~31; datos = bytearray(); ttab = bytearray()
        for W, H, fmt, lista, d in texs:
            ttab += struct.pack('<4H2I', W, H, fmt, lista, o + len(datos), len(d)); datos += d
            datos += bytes((-len(datos)) % 32)
        out = bytearray(b'HUD1' + struct.pack('<11I', *n, *offs)) + ttab
        for ti, (l, tp, r, b), w, h in s.spr:
            W, H = texs[ti][:2]; out += struct.pack('<4H4f', ti, w, h, 0, l / W, tp / H, r / W, b / H)
        cuenta = {}
        for p in s.pie: cuenta[p[0]] = cuenta.get(p[0], 0) + 1
        for i, g in enumerate(s.grp):
            out += struct.pack('<16s4HfI', g['nombre'].encode(), g['n'], g['pri'], cuenta.get(i, 0), 0, g['paso'], 0)
        for p in s.pie: out += struct.pack('<4H8f', *p)
        for nom, g, alt, x, y, av, capa, nc in s.lay:
            out += struct.pack('<16s2H4h2H', nom.encode(), g, alt, x, y, av, capa, nc, 0)
        out += bytes(s.taco)
        out += bytes(((len(out) + 31) & ~31) - len(out)); assert len(out) == o
        out += datos
        open(ruta, 'wb').write(out)
        return texs

def convierte(iso, salida, previas):
    D = Disco(iso); hud = Hud()
    a, T, xst = D.banco(0x2c)
    # tabla de fuentes de digitos (16 bytes: token o -1, avance, f32 dx, dy) y de marchas
    estilo = lambda i: D.elf32(0x404818 + 16 * i, 2)
    marchas = D.elf32(0x404478, 7)
    tacos = D.elf32(0x404a70, 16)
    assert all(m >> 16 == 0x2c for m in marchas), 'tabla de marchas no encontrada'
    tok = lambda t: t & 0xffff

    def simple(nombre, t, f=0.0):
        g = hud.grupo(nombre); hud.piezas_escena(g, 0, 0x2c, T, escena(a, tok(t), f)); return g
    def fuente(nombre, est):
        t, av = estilo(est); g = hud.grupo(nombre, 10)
        for d in range(10): hud.piezas_escena(g, d, 0x2c, T, escena(a, tok(t), d))
        return g, av
    def cuadros(nombre, t, n):
        g = hud.grupo(nombre, n)
        for f in range(n): hud.piezas_escena(g, f, 0x2c, T, escena(a, tok(t), f))
        return g

    # (coordenadas: constantes del codigo NAVI_PUB, 0x1bd440 y siguientes)
    g = simple('PANEL_TIEMPO', 0x2c0028); hud.uso('PANEL_TIEMPO', g, 0, 0, capa=3)
    g = simple('PANEL_PUNTOS', 0x2c0029); hud.uso('PANEL_PUNTOS', g, 0, 0, capa=3)
    g, av = fuente('DIG_TIEMPO', 0); hud.uso('TIEMPO', g, 301, 89, av, 9, 2)     # %02d; 3 cifras: x 276
    g, av = fuente('DIG_CRONO', 10); hud.uso('CRONO', g, 70, 73, av, 9, 10)      # "%2d %02d %03d"
    g1, av = fuente('DIG_PUNTOS', 1); g2, _ = fuente('DIG_PUNTOS_DERR', 12); g3, _ = fuente('DIG_PUNTOS_FUERA', 13)
    hud.uso('PUNTOS', g1, 481, 73, av, 9, 7, alt=g2)                             # %07d
    g1, av = fuente('DIG_VELOC', 2); g2, _ = fuente('DIG_VELOC_REBUF', 11)
    hud.uso('VELOCIDAD', g1, 120, 401, av, 9, 3, alt=g2)                        # %03d
    g1 = simple('KMH', 0x2c00a9); g2 = simple('KMH_REBUF', 0x2c00a8); hud.uso('KMH', g1, 0, 0, capa=5, alt=g2)
    g1 = simple('MPH', 0x2c00ab); g2 = simple('MPH_REBUF', 0x2c00aa); hud.uso('MPH', g1, 0, 0, capa=5, alt=g2)
    g = simple('REBUFO', 0x2c00c6); hud.uso('REBUFO', g, 0, 0, capa=5)
    g1 = simple('CAJA5', 0x2c009b); g2 = simple('CAJA6', 0x2c009c); hud.uso('CAJA', g1, 0, 0, capa=8, alt=g2)
    g = hud.grupo('MARCHA', 7)
    for m in range(7): hud.piezas_escena(g, m, 0x2c, T, escena(a, tok(marchas[m])))
    hud.uso('MARCHA', g, 0, 0, capa=9)
    g = hud.grupo('REV_TEXTO'); hud.piezas_imagen(g, 0x2c, T, xst, 0xd5, 0, 0); hud.uso('REV_TEXTO', g, 0x95, 0x132, capa=4)
    g = hud.grupo('REV_LUZ'); hud.piezas_imagen(g, 0x2c, T, xst, 0xd4, 0, 0); hud.uso('REV_LUZ', g, 0x95, 0x146, capa=4)
    for nom, (fo, cu, pa) in (('RUTA5', (0x2c0072, 0x2c0070, 0x2c006f)), ('RUTA15', (0x2c006e, 0x2c006d, 0x2c006c))):
        g = simple(nom + '_FONDO', fo); hud.uso(nom + '_FONDO', g, -241, -124, capa=2)
        g = cuadros(nom + '_ACTUAL', cu, 15); hud.uso(nom + '_ACTUAL', g, -241, -124, capa=3)
        g = cuadros(nom + '_HECHA', pa, 15); hud.uso(nom + '_HECHA', g, -241, -124, capa=3)
    # tacometros: escena 0 de cada banco METER, aguja = canal de rotacion (cuadro = rpm * 180 / 10000)
    hud.taco = [0xff] * 16; bancos = []
    for i, t in enumerate(tacos):
        b = t >> 16
        if not 0x23 <= b <= 0x31 or not D.ani[b - 0x20].startswith('ani_SPRANI_METER'): continue
        if b not in bancos:
            am, Tm, _ = D.banco(b); nombre = 'TACO_' + D.ani[b - 0x20][len('ani_SPRANI_METER_'):]
            p0 = escena(am, 0, 0.0); p1 = escena(am, 0, 179.0)
            ag = [p for p in p0 if p['gira']]; paso = 0.0; a0 = None
            if ag:
                ang = lambda p: np.degrees(np.arctan2(p['M'][0, 1], p['M'][0, 0]))
                a0 = canal(am, am.u32(ag[0]['L'] + 0x30 + 16), 0.0)
                paso = (canal(am, am.u32(ag[0]['L'] + 0x30 + 16), 179.0) - a0) / 179.0
            g = hud.grupo(nombre[:15], 1, paso); hud.piezas_escena(g, 0, b, Tm, p0, cero_gira=a0)
            hud.uso(nombre[:15], g, -219, 127, capa=2); bancos.append(b); hud.taco_g = getattr(hud, 'taco_g', {})
            hud.taco_g[b] = g
        hud.taco[i] = hud.taco_g[b]
    os.makedirs(salida, exist_ok=True)
    texs = hud.escribe(os.path.join(salida, 'HUD.DAT'))
    previews(hud, previas)
    return hud, texs

# ---------------------------------------------------------------- previas
def dibuja_grupo(img, hud, g, cuadro, X, Y):
    from PIL import Image
    G = hud.grp[g]
    for p in hud.pie[G['pri']:]:
        if p[0] != g: break
        gg, f, si, fl, x, y, ax, ay, ang, sx, sy, al = p
        if G['n'] > 1 and f != cuadro: continue
        if fl & 1: ang = ang + G['paso'] * cuadro
        ti, (l, tp, r, b), w, h = hud.spr[si]
        src = Image.fromarray(hud.tex[ti]).crop((int(l), int(tp), int(r), int(b))).resize((max(1, w), max(1, h)))
        if al < 1:
            arr = np.array(src); arr[..., 3] = (arr[..., 3] * max(0, al)).astype('u1'); src = Image.fromarray(arr)
        M = _t(-ax, -ay) @ _s(sx, sy) @ _r(ang) @ _t(x + X, y + Y)
        A = np.linalg.inv(M)
        img.alpha_composite(src.transform(img.size, Image.AFFINE, (A[0, 0], A[1, 0], A[2, 0], A[0, 1], A[1, 1], A[2, 1]),
                                          resample=Image.BILINEAR))

def previews(hud, d):
    from PIL import Image, ImageDraw
    os.makedirs(d, exist_ok=True)
    for ti, a in enumerate(hud.tex):
        im = Image.new('RGBA', (a.shape[1], a.shape[0]), (48, 48, 64, 255)); im.alpha_composite(Image.fromarray(a))
        im = im.resize((a.shape[1] * 2, a.shape[0] * 2), Image.NEAREST); dr = ImageDraw.Draw(im)
        for si, (t, (l, tp, r, b), w, h) in enumerate(hud.spr):
            if t == ti: dr.rectangle((l * 2, tp * 2, r * 2 - 1, b * 2 - 1), outline=(255, 0, 255, 255)); dr.text((l * 2 + 2, tp * 2 + 1), str(si), fill=(0, 255, 0, 255))
        im.save(os.path.join(d, 'tex_%02d.png' % ti))
    # maqueta: TIME 62, crono 1'23"456, puntos, 254 km/h, 5a, 7000 rpm, etapa 3
    img = Image.new('RGBA', (640, 480))
    px = np.zeros((480, 640, 4), np.uint8); yy = np.arange(480)[:, None]
    px[..., 0] = 70 + yy // 6; px[..., 1] = 110 + yy // 5; px[..., 2] = 200 - yy // 8; px[..., 3] = 255
    px[300:, :, :3] = [90, 90, 96]; img = Image.fromarray(px)
    G = {g['nombre']: i for i, g in enumerate(hud.grp)}; U = {u[0]: u for u in hud.lay}
    def uso(n, cuadro=0, g=None):
        u = U[n]; dibuja_grupo(img, hud, u[1] if g is None else g, cuadro, u[3], u[4])
    def numero(n, txt, x=None):
        u = U[n]; X = u[3] if x is None else x
        for i, c in enumerate(txt):
            if c.isdigit(): dibuja_grupo(img, hud, u[1], int(c), X + i * u[5], u[4])
    tac = hud.taco[1] if hud.taco[1] != 0xff else next(t for t in hud.taco if t != 0xff)
    orden = [(2, lambda: uso('RUTA5_FONDO')), (2, lambda: dibuja_grupo(img, hud, tac, 126, -219, 127)),
             (3, lambda: uso('PANEL_TIEMPO')), (3, lambda: uso('PANEL_PUNTOS')),
             (3, lambda: [uso('RUTA5_HECHA', f) for f in (0, 1)]), (3, lambda: uso('RUTA5_ACTUAL', 3)),
             (4, lambda: uso('REV_TEXTO')), (4, lambda: uso('REV_LUZ')),
             (5, lambda: uso('KMH')), (8, lambda: uso('CAJA')), (9, lambda: uso('MARCHA', 5)),
             (9, lambda: numero('TIEMPO', '62')), (9, lambda: numero('CRONO', ' 1 23 456')),
             (9, lambda: numero('PUNTOS', '0123456')), (9, lambda: numero('VELOCIDAD', '254'))]
    for _, f in sorted(orden, key=lambda x: x[0]): f()
    img.save(os.path.join(d, 'maqueta_hud.png'))

if __name__ == '__main__':
    if len(sys.argv) < 3: sys.exit(__doc__)
    hud, texs = convierte(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else sys.argv[2])
    print('%d texturas (%d bytes), %d sprites, %d grupos, %d piezas, %d usos' % (
        len(texs), sum(len(t[4]) for t in texs), len(hud.spr), len(hud.grp), len(hud.pie), len(hud.lay)))
