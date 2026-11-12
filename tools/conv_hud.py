"""HUD de carrera (modo OutRun) de OutRun 2006 Coast 2 Coast (PS2) -> HUD.DAT para la Dreamcast.

Uso: python3 conv_hud.py <iso> <dir_salida> [dir_previas] [dir_previas_avisos]
Escribe <dir_salida>/HUD.DAT y, en dir_previas (por defecto el directorio de salida), PNG de las texturas con
los rectangulos de los sprites y una maqueta del HUD completo a 640x480; en dir_previas_avisos (por defecto
dir_previas) una tira por grupo de aviso (aviso_*.png), la hoja de todos (avisos_hoja.png) y los atlas.

Avisos (grupos y usos anadidos al final, mismo formato; ver avisos()): cuadro = cuadros de 60 Hz desde que empieza
el aviso (los de "paso 5" avanzan un cuadro cada 5):
 CUENTA (4 cuadros quietos: 3, 2, 1, GO) y CUENTA_PRE/_3/_2/_1/_GO (60 cuadros cada uno, escena 2C000A de 300
   cuadros cortada por segundos: t = 0..299 -> grupo t/60, cuadro t%60); META (GOAL, 60 cuadros, despues el 59;
   uso en (0,-130)); TIEMPO_EXT (EXTEND TIME, 48 cuadros de paso 5 = 240); BIFURCA ("<- EASY", bucle de 60);
   FIN_TIEMPO (TIME OVER, 48: 0..39 entrada, luego bucle 40..47); CAMBIA ("Shift up!!", bucle de 30, si cabe en
   el limite de 350 KB de texturas nuevas; GAME_OVER igual); REV_ON (piloto REV encendido, 1 cuadro, parpadea
   con el bit 3 del contador de cuadros). Sus texturas son atlas recortados al alfa (clase Atlas).

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
 texturas 16 bytes: u16 ancho, alto, formato (0 ARGB1555, 1 RGB565, 2 ARGB4444; + 0x100 comprimida VQ: libro de
   256 entradas de 8 bytes y un byte por bloque de 2x2, tools/vq.py), lista (0 opaca, 1 recorte, 2 transparente);
   u32 desplazamiento (desde el inicio del fichero, alineado a 32), tamano. Texeles twiddled.
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


        assert len(self.p_spr) in (len(self.n_spr), len(self.n_spr) + 1), 'SPRITE.PS2 no cuadra'

    def _ent(self, p, k):
        return zlib.decompress(self.iso.m[p[k][1]:p[k][1] + p[k][2]])

    def banco(self, b):
        """(Ani, texturas RGBA, datos XST) del banco b (0x20..): animacion y XST del idioma (OR2_IDIOMA: es por
        defecto, el XST _sxst; en = _exst; si el banco no tiene version del idioma, la inglesa o la unica). La PAL elige
        el idioma por el de la consola (TEXT.PS2 y XST por letra); el port lo fija al convertir"""
        nom = self.ani[b - 0x20]                                # ani_SPRANI_ETC_CVT
        clave = nom[len('ani_SPRANI_'):].lower()
        ia = self.n_ani.index(nom.lower() + '.sz')
        letra = {'en': 'e', 'es': 's'}.get(os.environ.get('OR2_IDIOMA', 'es'), 'e')
        cand = ['spr_sprani_%s_%sxst.sz' % (clave, letra), 'spr_sprani_%s_exst.sz' % clave, 'spr_sprani_%s_xst.sz' % clave]
        lo = self.n_spr
        ix = next(lo.index(c) for c in cand if c in lo)
        # la entrada sin nombre de SPRITE.PS2 es spr_sprani_ranking_cvt_xst (en orden, detras de la _sxst: mismo tamano
        # que la _exst): los XST de despues (route_cvt, selector_cvt...) van una entrada mas alla
        if len(self.p_spr) == len(lo) + 1 and ix > lo.index('spr_sprani_ranking_cvt_sxst.sz'):
            r = lo.index('spr_sprani_ranking_cvt_exst.sz')
            assert self.p_spr[lo.index('spr_sprani_ranking_cvt_sxst.sz') + 1][2] == self.p_spr[r][2], 'XST sin nombre?'
            ix += 1
        xst = self._ent(self.p_spr, ix)
        return Ani(self._ent(self.p_ani, ia)), texturas(xst), xst

    def elf32(self, va, n=1):
        """palabras de .data del ejecutable (direccion virtual)"""
        o = va - 0x328700 + 0x228780
        return struct.unpack_from('<%dI' % n, self.elf, o)

def desordena_xst(datos, psm, w, h, bw_sub):
    """como gs.desordena: el rectangulo PSMCT32/PSMCT16 se sube con su propio ancho de bufer bw_sub, la mitad del de
    la lectura PSMT8/PSMT4 (= ancho / 128, minimo 1; ver texturas()). Con otro ancho se pierden o se mezclan las filas
    de las paginas siguientes a la primera fila de paginas."""
    wu, hu = w // 2, h // 2
    bw = max(2, (w + 63) // 64); bw += bw & 1
    mem = np.zeros(bw * 64 * (h + 128) * 4, np.uint8)
    y, x = np.mgrid[0:hu, 0:wu]
    if psm == 0x13:
        mem.view(np.uint32)[gs.dir32(x, y, bw_sub)] = np.frombuffer(datos[:wu * hu * 4], '<u4').reshape(hu, wu)
        y, x = np.mgrid[0:h, 0:w]
        return mem[gs.dir8(x, y, bw)]
    mem.view(np.uint16)[gs.dir16(x, y, bw_sub)] = np.frombuffer(datos[:wu * hu * 2], '<u2').reshape(hu, wu)
    y, x = np.mgrid[0:h, 0:w]
    a = gs.dir4(x, y, bw)
    return (mem[a >> 1] >> ((a & 1) * 4)) & 15

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
        # ancho de bufer de la subida: ancho / 128 (las paginas PSMCT32/16 miden la mitad en pixeles subidos). La
        # palabra 2 de la cabecera (/64) NO es ese ancho: con ella se rompian, p. ej., los rotulos de 512x256 PSMT8
        # del frontal ("SINGLE PLAYER" partido a partir de la fila 64) y las texturas de 256 de ancho con 4 paginas
        bws = max(1, W // 128)
        if psm == 0: a = np.frombuffer(raw, 'u1').reshape(Ht, W, 4).copy()
        elif psm == 0x13:
            pal = np.frombuffer(H[P + h[1]:P + h[1] + 1024], 'u1').reshape(256, 4)
            a = pal[tex._SW[desordena_xst(raw, 0x13, W, Ht, bws)]]
        else:
            pal = np.frombuffer(H[P + h[1]:P + h[1] + 96], 'u1').reshape(24, 4)
            a = pal[list(range(8)) + list(range(16, 24))][desordena_xst(raw, 0x14, W, Ht, bws)]
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

def _capa(a, L, f, M, al, out, est):
    """capa L en el cuadro f (429350 / 428BD0 del PC). Mascaras (flag 8): la capa de footage con el flag se guarda
    (est['masc']) y no se dibuja; la siguiente pieza que salga lleva esa mascara (p['mascara']: el alfa de la mascara,
    con su matriz y su recorte, multiplica el de la pieza; fuera del rectangulo de la mascara, 0; los alfas de las dos
    se combinan con AND como los colores de 42A800). Con una mascara pendiente la capa siguiente se recorre aunque
    este fuera de su intervalo o sin el flag de visible (429350)"""
    if not a.u32(L + 8): return
    pri, ult, d4, e6 = struct.unpack_from('<4h', a.a, L); fl = a.a[L + 0x48]
    lm, op, anc, gira = capa_mat(a, L, f)
    if not (fl & 2 and pri <= f < ult) and est.get('masc') is None: return
    loc = (f - pri) * (e6 / 4096) + d4; al *= op; Mt = lm @ M; p = a.u32(L + 8)
    if fl & 1:
        w, h, ritmo, n, fr = struct.unpack_from('<iiiiI', a.a, p)
        if n < 1: return
        i = 0 if n == 1 else min(n - 1, int(ritmo * loc + 0.001))
        t, l, tp, r, b = struct.unpack_from('<I4f', a.a, fr + 20 * i)
        if t == 0xffffffff: return
        q = dict(tex=t & 0xffff, src=(l, tp, r, b), w=w, h=h, M=Mt, alfa=al, anc=anc, gira=gira, L=L, f=f)
        if fl & 8 and est.get('masc') is None: est['masc'] = q; return
        if est.get('masc') is not None:
            m = est['masc']; est['masc'] = None
            q['mascara'] = m; q['alfa'] = (int(al * 255.5) & int(m['alfa'] * 255.5)) / 255.0
        out.append(q)
    else:
        ini, fin = struct.unpack_from('<2f', a.a, p + 4)
        if not ini <= loc < fin: return
        pl = a.u32(p + 0x20)
        for k in range(a.i32(p + 0x1c) - 1, -1, -1): _capa(a, pl + k * 0x4c, loc, Mt, al, out, est)

def escena(a, k, f=0.0):
    """piezas de la escena k en el cuadro f, con la instancia en (0, 0). Las que llevan 'mascara' hay que hornearlas
    (hornea) antes de usarlas"""
    sc = a.escenas[k]; nc, pc = struct.unpack_from('<2I', a.a, sc); raiz = pc + (nc - 1) * 0x24
    w, h = struct.unpack_from('<hh', a.a, raiz); M = _t(320 - w / 2, 240 - h / 2); out = []; est = {}
    pl = a.u32(raiz + 0x20)
    for q in range(a.i32(raiz + 0x1c) - 1, -1, -1): _capa(a, pl + q * 0x4c, f, M, 1.0, out, est)
    return out

HORNO = {}      # texturas horneadas (pieza con mascara): clave -> RGBA

def hornea(p, texs):
    """pieza con mascara -> pieza equivalente sin ella: su rectangulo de textura con el alfa multiplicado por el de
    la mascara (muestreo bilineal en el espacio del sprite de la mascara, 0 fuera de su recorte), en una textura
    nueva de HORNO (tex = clave, 'bk' = 'HORNO', 'tx' = HORNO). Asi la Dreamcast no necesita multitextura ni
    volumenes modificadores; cada combinacion distinta de posicion relativa sale una vez"""
    m = p['mascara']; C = texs[p['tex']]; Mk = texs[m['tex']]
    l, t, r, b = p['src']; L, T = max(0, int(np.floor(l))), max(0, int(np.floor(t)))
    R, B = min(C.shape[1], int(np.ceil(r))), min(C.shape[0], int(np.ceil(b)))
    rel = p['M'] @ np.linalg.inv(m['M'])                    # sprite de la pieza -> sprite de la mascara
    clave = ('HORNO', p['tex'], (L, T, R, B), m['tex'], tuple(round(v, 2) for v in m['src']),
             tuple(np.round(rel[:, :2].ravel(), 2)), p['w'], p['h'], m['w'], m['h'])
    if clave not in HORNO:
        y, x = np.mgrid[T:B, L:R] + 0.5
        sx = (x - l) * p['w'] / (r - l); sy = (y - t) * p['h'] / (b - t)
        q = np.stack([sx, sy, np.ones_like(sx)], -1) @ rel
        ml, mt, mr, mb = m['src']
        u = ml + q[..., 0] * (mr - ml) / m['w'] - 0.5; v = mt + q[..., 1] * (mb - mt) / m['h'] - 0.5
        u0, v0 = np.floor(u).astype(int), np.floor(v).astype(int); fu, fv = u - u0, v - v0
        def al(yy, xx):
            ok = (xx >= int(np.floor(ml))) & (xx < int(np.ceil(mr))) & (yy >= int(np.floor(mt))) & (yy < int(np.ceil(mb)))
            ok &= (xx >= 0) & (yy >= 0) & (xx < Mk.shape[1]) & (yy < Mk.shape[0])
            return np.where(ok, Mk[np.clip(yy, 0, Mk.shape[0] - 1), np.clip(xx, 0, Mk.shape[1] - 1), 3] / 255.0, 0.0)
        k = (al(v0, u0) * (1 - fu) * (1 - fv) + al(v0, u0 + 1) * fu * (1 - fv) + al(v0 + 1, u0) * (1 - fu) * fv
             + al(v0 + 1, u0 + 1) * fu * fv)
        img = C[T:B, L:R].copy(); img[..., 3] = np.round(img[..., 3] * k).astype(np.uint8)
        HORNO[clave] = img
    q = {kk: vv for kk, vv in p.items() if kk != 'mascara'}
    q.update(tex=clave, src=(l - L, t - T, r - L, b - T), bk='HORNO', tx=HORNO)
    return q

# ---------------------------------------------------------------- conversion
class Atlas:
    """texturas nuevas de los avisos: cada rectangulo de textura usado se recorta a su alfa (con 1 pixel de margen) y
    se empaqueta en texturas potencia de 2 (estantes), para no subir texturas de 256x256 enteras a la PowerVR"""
    def __init__(s): s.reg = {}; s.donde = {}
    @staticmethod
    def _clave(banco, p): return (banco, p['tex'], tuple(round(v, 3) for v in p['src']))
    def registra(s, banco, texs, p):
        k = s._clave(banco, p)
        if k in s.reg: return
        a = texs[p['tex']]; l, t, r, b = p['src']
        L, T = max(0, int(np.floor(l))), max(0, int(np.floor(t)))
        R, B = min(a.shape[1], int(np.ceil(r))), min(a.shape[0], int(np.ceil(b)))
        al = a[T:B, L:R, 3] > 3
        if al.any():
            ys, xs = np.nonzero(al)
            L, R, T, B = max(L, L + xs.min() - 1), min(R, L + xs.max() + 2), max(T, T + ys.min() - 1), min(B, T + ys.max() + 2)
        else: R, B = L + 1, T + 1
        s.reg[k] = (a, (L, T, R, B))
    def plan(s, claves, margen=2):
        """[(ancho, alto, {clave: (x, y)})] de las texturas necesarias para las claves: el mejor de varios ordenes"""
        dim = lambda k: (s.reg[k][1][2] - s.reg[k][1][0], s.reg[k][1][3] - s.reg[k][1][1])
        ordenes = (lambda k: (-dim(k)[1], k), lambda k: (-dim(k)[0], k), lambda k: (-dim(k)[0] * dim(k)[1], k),
                   lambda k: (-max(dim(k)), k))
        return min((s._plan(sorted(set(claves), key=o), margen, dim) for o in ordenes), key=lambda pl: (s.bytes_plan(pl), len(pl)))
    def _plan(s, pend, margen, dim):
        res = []
        def coloca(W, H, ks):
            """horizonte (skyline) abajo-izquierda: cada rectangulo donde su borde superior quede mas bajo"""
            sky = [[0, 0, W]]; pos = {}; resto = []
            for k in ks:
                L, T, R, B = s.reg[k][1]; w, h = R - L + margen, B - T + margen
                mejor = None
                for i in range(len(sky)):
                    x = sky[i][0]
                    if x + w > W: break
                    y = 0; j = i; anc = 0
                    while anc < w: y = max(y, sky[j][1]); anc += sky[j][2]; j += 1
                    if y + h <= H + margen and (mejor is None or (y + h, x) < (mejor[0] + h, mejor[1])): mejor = (y, x)
                if mejor is None: resto.append(k); continue
                y, x = mejor; pos[k] = (x, y)
                nuevo = []; fin = x + w
                for sx, sy, sw in sky:
                    if sx + sw <= x or sx >= fin: nuevo.append([sx, sy, sw]); continue
                    if sx < x: nuevo.append([sx, sy, x - sx])
                    if sx + sw > fin: nuevo.append([fin, sy, sx + sw - fin])
                nuevo.append([x, y + h, w]); nuevo.sort(); sky = []
                for seg in nuevo:
                    if sky and sky[-1][1] == seg[1] and sky[-1][0] + sky[-1][2] == seg[0]: sky[-1][2] += seg[2]
                    else: sky.append(seg)
            return pos, resto
        tam = sorted(((1 << i, 1 << j) for i in range(5, 10) for j in range(5, 10) if i + j <= 17), key=lambda v: (v[0] * v[1], v))
        while pend:
            for W, H in tam:
                pos, resto = coloca(W, H, pend)
                if not resto: break
            else:
                # no cabe en una: se llena la de 512x256 o 256x512 que mas aproveche
                W, H, pos, resto = max(((W, H) + coloca(W, H, pend) for W, H in ((512, 256), (256, 512))),
                                       key=lambda v: sum(dim(k)[0] * dim(k)[1] for k in v[2]))
            res.append((W, H, pos)); pend = resto
        return res
    @staticmethod
    def bytes_plan(pl): return sum(W * H * 2 for W, H, _ in pl)
    def empaqueta(s, hud, pl):
        s.texs = [(W, H) for W, H, _ in pl]; s.bytes = s.bytes_plan(pl)
        for i, (W, H, pos) in enumerate(pl):
            img = np.zeros((H, W, 4), np.uint8)
            ti = hud.textura((getattr(s, 'prefijo', 'atlas'), i), img)
            for k, (x, y) in pos.items():
                a, (L, T, R, B) = s.reg[k]; img[y:y + B - T, x:x + R - L] = a[T:B, L:R]; s.donde[k] = (ti, x - L, y - T)
    def busca(s, banco, p):
        k = s._clave(banco, p); ti, ox, oy = s.donde[k]; L, T, R, B = s.reg[k][1]
        l, t, r, b = p['src']; kx, ky = p['w'] / (r - l), p['h'] / (b - t)
        sl, st, sr, sb = max(l, L), max(t, T), min(r, R), min(b, B)
        return (ti, (sl + ox, st + oy, sr + ox, sb + oy), int(round((sr - sl) * kx)), int(round((sb - st) * ky)),
                (sl - l) * kx, (st - t) * ky)

def avisos(hud, D, a, T):
    """avisos de carrera (grupos y usos nuevos, al final de los existentes). Tokens y parametros del codigo MIPS:
    cuenta atras 2C000A: guion de salida en .data 0x36D3A4/0x36D744/0x36DBEC {token, capa 4, modo 3};
    GOAL 2C0009: 0x19E7B4 crea (capa 2, modo 1, cuadros 0..90) con traslacion (0, -130);
    TIME OVER 2C000C: 0x19F3AC / 0x19F3C4 (capa 0, modo 1); GAME OVER 2C000B: 0x1A0538 (capa 0, modo 1);
    EXTEND TIME 2C00AC: 0x1BEC94 (temporizador 0x5A3A0E == 180: capa 6, modo 3, cuadros 0..240) y 0x231484;
    bifurcacion "<- EASY" 2B0000: 0x1C5130 / 0x1C51C0 (capa 0, modo 0 = bucle) con el avance car+0x102C entre
    0.65 y 0.72 (tramo 0) / 0.70 y 0.75 (tramo 1) (4BB890 del PC); aviso de marcha: 0x1BE20C dibuja el grupo 2 de
    spr_etc (banco 3) en (40, 18) con prioridad 5 si (cuadro & 8) y rpm (car+0x48) > 7900 y marcha < maxima (4B9010 del
    PC); "Shift up!!" 2C009A: 0x1C07B0 (capa 4, modo 0) si el cambio es manual y car+0xE80 (4BA070 del PC)."""
    ag, Tg, _ = D.banco(0x2b)
    etc = D._ent(D.p_spr, D.n_spr.index('spr_etc_xst.sz')); Te = texturas(etc)
    at = Atlas(); grupos = []          # (nombre, [(cuadro, banco, texs, piezas)])

    def anim(nombre, banco, A, Tx, k, fotos):
        lst = [(i, banco, Tx, escena(A, k, float(f))) for i, f in enumerate(fotos)]
        grupos.append((nombre, len(fotos), lst))
    tok = lambda t: t & 0xffff
    # cuenta atras: 300 cuadros; '3' entra 50..59, quieto 59..99, sale 100..119 (el siguiente entra a la vez)
    anim('CUENTA', 0x2c, a, T, tok(0x2c000a), [90, 150, 210, 270])
    for n, f0 in (('CUENTA_PRE', 0), ('CUENTA_3', 60), ('CUENTA_2', 120), ('CUENTA_1', 180), ('CUENTA_GO', 240)):
        anim(n, 0x2c, a, T, tok(0x2c000a), range(f0, f0 + 60))
    anim('META', 0x2c, a, T, tok(0x2c0009), range(60))              # 0..59 anima; 60..90 = cuadro 59
    anim('TIEMPO_EXT', 0x2c, a, T, tok(0x2c00ac), range(0, 240, 5)) # parpadeo: 0-29, 60-89, 120-239
    anim('BIFURCA', 0x2b, ag, Tg, 0, range(60))                     # bucle de 60
    anim('FIN_TIEMPO', 0x2c, a, T, tok(0x2c000c), range(48))        # 0..39 entra; 40..47 bucle (estrella 2.5/cuadro, 18 rayos)
    anim('GAME_OVER', 0x2c, a, T, tok(0x2c000b), range(0, 300, 5))
    anim('CAMBIA', 0x2c, a, T, tok(0x2c009a), range(30))            # "Shift up!!" bucle de 30
    # REV encendido: grupo 2 de spr_etc = imagen 0 (resplandor rojo 64x64) en (94, 283) + (40, 18)
    ng, pg = struct.unpack_from('<2I', etc, 4 + 0x10)
    n, pi = struct.unpack_from('<2I', etc, 4 + pg + 8 * 2); piezas = []
    for j in range(n):
        im, dx, dy = struct.unpack_from('<I2h', etc, 4 + pi + 16 * j)
        t, l, tp, r, b = imagen_xst(etc, im)
        piezas.append(dict(tex=t, src=(l, tp, r, b), w=int(r - l), h=int(b - tp), M=_t(dx + 40, dy + 18), alfa=1.0,
                           anc=(0.0, 0.0), gira=False))
    grupos.append(('REV_ON', 1, [(0, 3, Te, piezas)]))
    claves = {}
    for nombre, _, lst in grupos:
        claves[nombre] = []
        for _, banco, Tx, ps in lst:
            for p in ps:
                if p['alfa'] >= 0.004 and abs(np.linalg.det(p['M'][:2, :2])) >= 1e-6:
                    at.registra(banco, Tx, p); claves[nombre].append(at._clave(banco, p))
    # memoria de video: los pedidos siempre; los opcionales solo si el total sigue <= 350 KB
    opcionales = ('GAME_OVER', 'CAMBIA')
    usar = [g[0] for g in grupos if g[0] not in opcionales]
    pl = at.plan(sum((claves[n] for n in usar), []))
    for n in opcionales:
        p2 = at.plan(sum((claves[m] for m in usar + [n]), []))
        if at.bytes_plan(p2) <= 350 * 1024: usar.append(n); pl = p2
    grupos = [g for g in grupos if g[0] in usar]
    at.empaqueta(hud, pl)
    gi = {}
    for nombre, nf, lst in grupos:
        g = hud.grupo(nombre, nf); gi[nombre] = g
        for c, banco, Tx, ps in lst: hud.piezas_escena(g, c, banco, Tx, ps, atlas=at)
    for nombre, x, y, capa in (('CUENTA', 0, 0, 4), ('CUENTA_PRE', 0, 0, 4), ('CUENTA_3', 0, 0, 4),
                               ('CUENTA_2', 0, 0, 4), ('CUENTA_1', 0, 0, 4), ('CUENTA_GO', 0, 0, 4),
                               ('META', 0, -130, 2), ('TIEMPO_EXT', 0, 0, 6), ('BIFURCA', 0, 0, 0),
                               ('FIN_TIEMPO', 0, 0, 0), ('GAME_OVER', 0, 0, 0), ('CAMBIA', 0, 0, 4), ('REV_ON', 0, 0, 5)):
        if nombre in gi: hud.uso(nombre, gi[nombre], x, y, capa=capa)
    return at

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

    def piezas_escena(s, g, cuadro, banco, texs, piezas, cero_gira=None, atlas=None):
        for p in piezas:
            M = p['M']; ax, ay = p['anc']
            if atlas is not None:                               # avisos: atlas recortado y sin piezas invisibles
                if p['alfa'] < 0.004 or abs(np.linalg.det(M[:2, :2])) < 1e-6: continue
                ti, rect, w, h, dx, dy = atlas.busca(banco, p)  # sprite recortado: su (0,0) es el (dx,dy) original
                si = s.sprite(ti, rect, w, h)
                x, y = (np.array([ax, ay, 1.0]) @ M)[:2]
                ang = np.degrees(np.arctan2(M[0, 1], M[0, 0]))
                sx, sy = np.hypot(M[0, 0], M[0, 1]), np.hypot(M[1, 0], M[1, 1])
                s.pie.append((g, cuadro, si, 0, x, y, ax - dx, ay - dy, ang, sx, sy, p['alfa']))
                continue
            bk, tx = banco, texs
            if 'mascara' in p: p = hornea(p, texs); bk, tx = p['bk'], p['tx']
            ti = s.textura((bk, p['tex']), tx[p['tex']])
            si = s.sprite(ti, p['src'], p['w'], p['h'])
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
        texs = []; vq = getattr(s, 'vq', None)
        for i, a in enumerate(s.tex):
            h, w = a.shape[:2]; W = max(8, 1 << (w - 1).bit_length()); H = max(8, 1 << (h - 1).bit_length())
            if (W, H) != (w, h): b = np.zeros((H, W, 4), np.uint8); b[:h, :w] = a; a = b
            fmt, lista, d = textura_dc(a)
            if vq:                                          # VQ (formato | 0x100, libro de 256 entradas) si conviene
                r = vq(i, a, fmt, lista)
                if r is not None: fmt, d = fmt | 0x100, r
            texs.append((W, H, fmt, lista, d))
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

def convierte(iso, salida, previas, previas_av=None):
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
    ng0 = len(hud.grp); nt0 = len(hud.tex)
    at = avisos(hud, D, a, T)
    hud.vq = lambda i, a, fmt, lista: vq_avisos(i, a, fmt, lista, nt0)
    os.makedirs(salida, exist_ok=True)
    texs = hud.escribe(os.path.join(salida, 'HUD.DAT'))
    previews(hud, previas)
    previas_avisos(hud, ng0, previas_av or previas)
    print('avisos: %d grupos nuevos, %d texturas nuevas %s, %d bytes' % (len(hud.grp) - ng0, len(hud.tex) - nt0,
          at.texs, sum(len(t[4]) for t in texs[nt0:])))
    return hud, texs

# ---------------------------------------------------------------- escenas enteras (FIN.DAT, CARGASPn.DAT, GOVSP.DAT...)
def grande(src):
    """el rectangulo src no cabe en una textura del atlas (512x256 / 256x512): va con su textura de origen entera"""
    l, t, r, b = src; w, h = r - l, b - t
    return not ((w <= 510 and h <= 254) or (w <= 254 and h <= 510))

VQ_AVISOS = 12.0       # error RMS maximo (0-255) de la VQ de los atlas de los avisos (medido: 9,0 y 7,0)

def vq_avisos(i, a, fmt, lista, nt0):
    """Hud.vq de HUD.DAT (plan de recursos, 2026-11-12): solo los atlas de los avisos (texturas desde nt0: la cuenta
    atras, META, Se acabo el tiempo, Tiempo extra, <- FACIL...), letras grandes con degradado que en 16 bits ocupaban
    256 + 32 KB de VRAM en toda la carrera (576 y 208 colores: no caben en las paletas de comun/hud.c). Las demas
    (cifras, tacometros, marchas, paneles y el mapa) son de 16 colores o de 256 y van con paleta exacta: se quedan
    (la VQ emborrona las cifras pequenas y ahorraria menos de 10 KB por textura)"""
    if i < nt0: return None
    import vq
    datos, err = vq.codifica(a, fmt, 256, tiempo=1e9)
    print('  aviso: textura %d %dx%d VQ error %.1f %s' % (i, a.shape[1], a.shape[0], err, 'si' if err <= VQ_AVISOS else 'no'))
    return datos if err <= VQ_AVISOS and len(datos) < a.shape[0] * a.shape[1] * 2 else None

def vq_pequeno(i, a, fmt, lista, pre=''):
    """Hud.vq de los ficheros de escenas: VQ (tools/vq.py, libro de 256) si el error es pequeno"""
    import vq
    datos, err = vq.codifica(a, fmt, 256)
    print('  %stextura %d %dx%d VQ error %.1f %s' % (pre, i, a.shape[1], a.shape[0], err, 'si' if err <= 16 else 'no'))
    return datos if err <= 16 else None

def visibles(ps): return [p for p in ps if p['alfa'] >= 0.004 and abs(np.linalg.det(p['M'][:2, :2])) >= 1e-6]

def grupo_escena(hud, nombre, A, T, bk, k, n, prefijo='', cambia=None):
    """hornea los cuadros 0..n-1 de la escena k del banco bk (A, T de Disco.banco) en el grupo nombre de hud, con su
    atlas (las texturas del grupo se suben solo mientras se ve; las piezas grandes con su textura de origen entera:
    fondos), y devuelve el grupo (los usos los pone quien llama). Solo se escriben los cuadros donde algo cambia: cada
    pieza guarda en flags >> 4 los cuadros que abarca y en el bit 2 la capa aditiva (+0x48 & 4). cambia(p): la pieza
    que va en lugar de p (los iconos de los botones de conv_fin) o p"""
    lst = []; ant = None
    for f in range(n):
        ps = [hornea(p, T) if 'mascara' in p else p for p in escena(A, k, float(f))]
        if cambia: ps = [cambia(p) for p in ps]
        firma = [(str(p['tex']), tuple(np.round(p['M'].ravel(), 3)), round(p['alfa'], 3), tuple(p['src'])) for p in ps]
        if firma != ant: lst.append([f, ps, 1]); ant = firma
        else: lst[-1][2] += 1
    at = Atlas(); at.prefijo = prefijo + nombre; cl = []
    for _, ps, _ in lst:
        for p in visibles(ps):
            if grande(p['src']): continue
            b2, T2 = (p['bk'], p['tx']) if 'bk' in p else (bk, T)
            at.registra(b2, T2, p); cl.append(at._clave(b2, p))
    t0 = len(hud.tex); pl = at.plan(cl); at.empaqueta(hud, pl)
    print('  %s: escena %X:%04X, %d cuadros (%d distintos), texturas %d..%d, %d KB' % (
        nombre, bk, k, n, len(lst), t0, len(hud.tex) - 1, at.bytes_plan(pl) >> 10))
    g = hud.grupo(nombre, n)
    for c, ps, nf in lst:
        k0 = len(hud.pie); v = visibles(ps)
        for p in v:
            b2, T2 = (p['bk'], p['tx']) if 'bk' in p else (bk, T)
            hud.piezas_escena(g, c, b2, T2, [p], atlas=None if grande(p['src']) else at)
        assert len(v) == len(hud.pie) - k0
        for j, p in zip(range(k0, len(hud.pie)), v):            # cuadros que abarca y capa aditiva
            q = list(hud.pie[j]); L = p.get('L')
            adi = 4 if L is not None and A.a[L + 0x48] & 4 else 0
            q[3] = (q[3] & 11) | adi | (min(nf, 4095) << 4); hud.pie[j] = tuple(q)
    return g

# ---------------------------------------------------------------- previas
def dibuja_grupo(img, hud, g, cuadro, X, Y):
    from PIL import Image
    G = hud.grp[g]
    for p in hud.pie[G['pri']:]:
        if p[0] != g: break
        gg, f, si, fl, x, y, ax, ay, ang, sx, sy, al = p
        if G['n'] > 1 and not f <= cuadro < f + max(1, fl >> 4): continue   # fl >> 4: cuadros que abarca (MENU.DAT)
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

def previas_avisos(hud, g0, d):
    """hoja por grupo nuevo: hasta 12 cuadros repartidos, a media escala, con el uso aplicado"""
    from PIL import Image, ImageDraw
    os.makedirs(d, exist_ok=True); U = {u[1]: u for u in hud.lay}; filas = []
    for g in range(g0, len(hud.grp)):
        G = hud.grp[g]; u = U[g]; n = G['n']; cs = sorted(set(int(round(i * (n - 1) / 11)) for i in range(12))) if n > 1 else [0]
        fila = Image.new('RGBA', (12 * 320, 262), (0, 0, 0, 255)); dr = ImageDraw.Draw(fila)
        for j, c in enumerate(cs):
            img = Image.new('RGBA', (640, 480), (52, 70, 110, 255))
            ImageDraw.Draw(img).rectangle((0, 300, 639, 479), fill=(80, 80, 88, 255))
            dibuja_grupo(img, hud, g, c, u[3], u[4])
            fila.paste(img.resize((320, 240)), (j * 320, 22))
            dr.text((j * 320 + 4, 24), 'cuadro %d' % c, fill=(255, 255, 0, 255))
        dr.text((4, 4), '%s  (grupo %d, %d cuadros, uso (%d,%d) capa %d)' % (G['nombre'], g, n, u[3], u[4], u[6]),
                fill=(255, 255, 255, 255))
        fila.save(os.path.join(d, 'aviso_%s.png' % G['nombre'].lower())); filas.append(fila)
    hoja = Image.new('RGBA', (12 * 320, 262 * len(filas)))
    for i, f in enumerate(filas): hoja.paste(f, (0, 262 * i))
    hoja.save(os.path.join(d, 'avisos_hoja.png'))
    for ti in range(len(hud.tex)):
        if hud.texkey.get(('atlas', 0)) is not None and ti >= hud.texkey[('atlas', 0)]:
            a = hud.tex[ti]; im = Image.new('RGBA', (a.shape[1], a.shape[0]), (48, 48, 64, 255))
            im.alpha_composite(Image.fromarray(a)); im.save(os.path.join(d, 'atlas_%02d.png' % ti))

if __name__ == '__main__':
    if len(sys.argv) < 3: sys.exit(__doc__)
    hud, texs = convierte(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else sys.argv[2],
                          sys.argv[4] if len(sys.argv) > 4 else None)
    print('%d texturas (%d bytes), %d sprites, %d grupos, %d piezas, %d usos' % (
        len(texs), sum(len(t[4]) for t in texs), len(hud.spr), len(hud.grp), len(hud.pie), len(hud.lay)))
