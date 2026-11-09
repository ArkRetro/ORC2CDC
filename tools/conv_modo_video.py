"""Pantalla de eleccion del modo de video del arranque de OutRun 2006 Coast 2 Coast (PS2 PAL) -> MODOV.DAT.

Uso: python3 conv_modo_video.py <iso> <dir_salida> [dir_previas]
Escribe <dir_salida>/MODOV.DAT y, en dir_previas (por defecto el de salida), modov_<estado>.png (maquetas a 640x480).

Origen (SLES-53998; todo se lee del disco al convertir, aqui solo hay direcciones y constantes del codigo MIPS):
 - Arranque: la maquina de estados 0x23B190 (tabla 0x416B10), estado 4 crea la pantalla (0x104AC8, descriptor
   0x3BF8C8: init 0x104E40, paso 0x105110, dibujo 0x105390) y el 5 la corre hasta que el paso devuelve 1; despues el
   6 carga el banco 0x47 (SPLASH_CVT, logos). Se sale en todos los arranques (no mira la region de la consola).
 - 0x107370 = [0x328A94]: 0 si el disco es SCEE (\\SLES_539.98, 0x1072E0), 1 SCEA. En la PAL la lista es "50Hz",
   "60Hz - Test", "60Hz Progressive (480P) - Test" (0x104C68: textos 0x32F, 0x330, 0x331), cursor en la primera.
 - Ventana 0x271800 (x 100, y 150, ancho 440, alto 180, flags 1 -> +32 de alto y boton de aceptar), titulo 0x32E en
   (x+6, y+5) y texto del boton 0x294 en (x+45, y+h-38), fuente 14 blanca; icono del boton: tabla 0x41F368 indice 5 =
   escena 0x480009 (cruz) cuadro 25 centrada en (x+27, y+h-30) (aqui el icono A de conv_menu.iconos_dc, como la barra
   de ayuda del port). Despues 0x272470 la deja en (100, 140, 440, s1 + 144) con s1 = max(alto(0x334),
   alto(0x333) + 48) (alto de un texto ajustado a 403 = ancho - 37 de interior, lineas de 14).
 - Dibujo de la ventana 0x271CA8: piezas de spr_etc_xst (banco 3) 0x4A (relleno, un texel), 0x49 (franja bajo la
   cabecera), 0x40 (banda del boton, y+h-46), 0x4F / 0x43 (cabecera y pie estirados con su columna izquierda),
   esquinas 0x4E 0x4D 0x42 0x41, lados 0x4C 0x4B (estirados con su fila de arriba); 0x117B88: modo 1 = una columna,
   2 = una fila. Mismas cuentas que 48C5F0 del PC (frontend_window.cpp).
 - Texto de ayuda (+0x98): en (x+17, y+38), ajustado a 403, color 0xFF3F474A, lineas de 14 (26BE80 / 26C160 / 26C7A8
   = 48E640 / 48F3C0 / 48EB60 del PC). Por cursor (0x104D70): 0x441, 0x333 (aviso de 60 Hz), 0x334. En la prueba de
   60 Hz (0x105390): 0x335 con %d = segundos que quedan: 15 con t < 0, (900 - t) / 60 + 1 hasta t = 900, 0 despues.
 - Lista (+0x4C, 26A940 / 26AE98 / 26BC58 = 4ECFB0 / 4ED3E0 del PC): ancho 403 en (x+17, y+38+alto_interior-29),
   filas cada 16, texto en (x+7, y) fuente 14 color 0xFF3F474A; resalte de tres piezas 0x45 (14x15), 0x46 estirada
   (ancho-28), 0x44 (14x15) con color 0xFFFFCE0A en la fila del cursor y 0xFF8B9091 en las demas. En la prueba de
   60 Hz la lista es 0x1F2 ("No"), 0x1F1 ("Si"), cursor en "No".
 - Fuente 14 (spr_font_xst, metricas e interletraje del ELF): conv_menu.Fuente (42C480 / 42C610 / 42C860).
 - Capas (0x117708: 21 listas por capa, en orden de envio): ventana y sus textos 4, icono 5, resalte 12, textos 13.
   Fondo negro.
 - Colores 0xAARRGGBB (los mismos literales en el PC, que los usa asi); las texturas se modulan por el color.

MODOV.DAT (little endian):
 cabecera 32 bytes: 'MDV1', u16 x, y, ancho, alto (rectangulo de la base en 640x480), u32 desp. base (RGB565 ancho x
   alto: la ventana con titulo, boton e icono sobre negro), u32 n fuentes de imagen, desp. fuentes, n grupos,
   desp. grupos
 fuentes de imagen 12 bytes: u16 ancho, alto, 0, 0; u32 desp. (ARGB8888, a,r,g,b en un u32 0xAARRGGBB)
 grupos 8 bytes: u32 primera orden (desde el final de la tabla de grupos), n ordenes. Ordenes 20 bytes: u16 fuente,
   sx, sy, w, h; i16 x, y (en pantalla); u16 0; u32 color 0xAARRGGBB que multiplica (la orden se pinta 1:1, con mezcla alfa)
 grupos: 0..2 ayuda de la fila 0..2 de la lista PAL; 3..19 ayuda de la prueba de 60 Hz con 0..16 segundos;
   20 + 2 fila + sel: filas 0..2 de la lista PAL (sel 1 = cursor), 26 + 2 fila + sel: "No", "Si".
"""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conv_hud import Disco, texturas, imagen_xst
from conv_menu import Fuente, fuentes_xst, textos, iconos_dc, IDIOMAS

FUENTE = 14
VX, VY, VW, VH, VFLAGS = 100, 150, 440, 180, 1          # 0x104E40 -> 0x271800
VY2 = 140                                               # 0x272470
LINEA = 14                                              # +0x4A0 de 26BE80
C_TEXTO, C_SEL, C_NOSEL, C_BLANCO = 0xff3f474a, 0xffffce0a, 0xff8b9091, 0xffffffff
T_TITULO, T_BOTON, T_LISTA_PAL, T_AYUDA, T_PRUEBA, T_SINO = 0x32e, 0x294, (0x32f, 0x330, 0x331), (0x441, 0x333, 0x334), 0x335, (0x1f2, 0x1f1)

def ancho(F, t):
    """42C480 con el flag 0x100 (el de la fuente: conv_menu.Fuente.ancho)"""
    return F.ancho(t)

def ajusta(F, texto, w):
    """lineas de 26C7A8 (= 48EB60 del PC): 16 lineas de 128, se corta en ' ', '-' y salto"""
    lineas = [bytearray(128) for _ in range(16)]
    st = dict(linea=0, com=0, usado=0)
    def escribe(off, d):
        if 0 <= st['linea'] < 16 and off >= 0 and off + len(d) <= 128: lineas[st['linea']][off:off + len(d)] = d
    def fin():
        escribe(st['com'], b'\0'); st['linea'] += 1; st['com'] = st['usado'] = 0
    pal = bytearray(); i = 0
    while i <= len(texto) and st['linea'] < 16:
        c = texto[i] if i < len(texto) else 0
        if c not in (32, 45, 10, 0) and st['com'] + len(pal) != 127:
            pal.append(c)
            if st['usado'] + ancho(F, bytes(pal)) <= w: i += 1; continue
            if st['com'] == 0:
                pal.pop(); escribe(0, bytes(pal)); st['com'] = len(pal); fin(); pal = bytearray(); continue   # --i
            fin(); i += 1; continue
        escribe(st['com'], bytes(pal)); st['com'] += len(pal)
        if c in (10, 0) or st['com'] == 127: fin(); pal = bytearray()
        else:
            pal.append(c); st['usado'] += ancho(F, bytes(pal)); escribe(st['com'], bytes([c])); st['com'] += 1
            pal = bytearray()
        i += 1
    for k in range(max(st['linea'], 0), 16): lineas[k][0] = 0
    return [bytes(l[:l.index(0)]) if 0 in l else bytes(l) for l in lineas]

def alto(F, texto, w):
    """26BEF8: (ultima linea no vacia + 1) * 14; 14 si no hay ninguna"""
    r = LINEA
    for i, l in enumerate(ajusta(F, texto, w)):
        if l: r = (i + 1) * LINEA
    return r

def glifos(F, texto, x, y, color):
    """ordenes de un texto sin alinear (flags 1) en (x, y)"""
    return [('F', int(p['src'][0]), int(p['src'][1]), F.w, F.h, int(round(p['M'][2, 0])), int(round(p['M'][2, 1])), color)
            for p in F.piezas(texto, x, y, 1)]

def caja(F, texto, x, y, w, color):
    out = []
    for i, l in enumerate(ajusta(F, texto, w)):
        if l: out += glifos(F, l, x, y + i * LINEA, color)
    return out

def pinta(lienzo, src, sx, sy, w, h, x, y, color, a_col=True):
    """mezcla alfa de src[sy:sy+h, sx:sx+w] (RGBA) modulado por color en lienzo (RGB float)"""
    a = src[sy:sy + h, sx:sx + w].astype(float) / 255
    c = np.array([(color >> 16) & 255, (color >> 8) & 255, color & 255]) / 255.0; ca = (color >> 24) / 255.0
    H, W = lienzo.shape[:2]
    x0, y0, x1, y1 = max(0, x), max(0, y), min(W, x + a.shape[1]), min(H, y + a.shape[0])
    if x1 <= x0 or y1 <= y0: return
    a = a[y0 - y:y1 - y, x0 - x:x1 - x]; al = a[..., 3:4] * ca
    lienzo[y0:y1, x0:x1] = lienzo[y0:y1, x0:x1] * (1 - al) + a[..., :3] * c * 255 * al

def pieza(T, xst, k, modo, x, y, w, h):
    """imagen (RGBA) de la pieza k como la dibuja 0x117B88: modo 1 su columna izquierda, 2 su fila de arriba"""
    t, l, a, r, b = imagen_xst(xst, k)
    if modo & 1: r = l + 1
    if modo & 2: b = a + 1
    src = T[t][a:b, l:r]
    ys = (np.arange(h) * (b - a) // h) if modo & 2 == 0 else np.zeros(h, int)
    xs = (np.arange(w) * (r - l) // w) if modo & 1 == 0 else np.zeros(w, int)
    return src[ys][:, xs], x, y

def ventana(T, xst, x, y, w, h, boton):
    """piezas de 0x271CA8 en orden (modo, imagen, x, y, w, h)"""
    ps = [(3, 0x4a, x + 3, y + 26, w - 9, h - 42), (1, 0x49, x + 3, y + 32, w - 9, 6)]
    if boton: ps.append((1, 0x40, x + 3, y + h - 46, w - 9, 30))
    ps += [(1, 0x4f, x + 9, y, w - 115, 26), (1, 0x43, x + 9, y + h - 16, w - 20, 16),
           (0, 0x4e, x, y, 9, 26), (0, 0x4d, x + w - 106, y, 106, 26), (0, 0x42, x, y + h - 16, 9, 16),
           (0, 0x41, x + w - 11, y + h - 16, 11, 16), (2, 0x4c, x, y + 26, 3, h - 42), (2, 0x4b, x + w - 6, y + 26, 6, h - 42)]
    return [pieza(T, xst, k, m, px, py, pw, ph) for m, k, px, py, pw, ph in ps]

def resalte(T, xst, w, color):
    """las tres piezas de 26BC58 (0x45, 0x46 estirada, 0x44) en una imagen RGBA de w x 15 ya multiplicada por color"""
    img = np.zeros((15, w, 4), float)
    c = np.array([(color >> 16) & 255, (color >> 8) & 255, color & 255, color >> 24]) / 255.0
    for k, m, px, pw in ((0x45, 0, 0, 14), (0x46, 1, 14, w - 28), (0x44, 0, w - 14, 14)):
        p, _, _ = pieza(T, xst, k, m, px, 0, pw, 15)
        img[:, px:px + pw] = p.astype(float) * c
    return np.clip(np.round(img), 0, 255).astype(np.uint8)

def convierte(iso, salida, previas):
    from PIL import Image
    idioma = os.environ.get('OR2_IDIOMA', 'es')
    D = Disco(iso); cad = textos(D, idioma); F = Fuente(D, FUENTE, fuentes_xst(D))
    xst = D._ent(D.p_spr, D.n_spr.index('spr_etc_xst.sz')); T = texturas(xst)
    w_int = VW - 37                                     # 0x2723D0
    # tamano final de la ventana (0x104E40)
    h0 = VH + (32 if VFLAGS & 7 else 0)
    s1 = max(alto(F, cad[T_AYUDA[2]], w_int), alto(F, cad[T_AYUDA[1]], w_int) + 0x30)
    hi0 = h0 - 32 - 80                                  # 0x2723E0 con el boton
    x, y, w, h = VX, VY2, VW, s1 - (hi0 - 0xf4)
    hi = h - 32 - 80
    ix, iy = x + 3 + 14, y + 38                         # 0x272428 / 0x272448
    lx, ly = ix, iy + hi - 24 - 5                       # 0x104B80
    # base: ventana, titulo, texto e icono del boton (capas 4 y 5) sobre negro
    lienzo = np.zeros((480, 640, 3), float)
    for img, px, py in ventana(T, xst, x, y, w, h, True):
        pinta(lienzo, img, 0, 0, img.shape[1], img.shape[0], px, py, C_BLANCO)
    Ff = F.T[F.id]
    for o in glifos(F, cad[T_TITULO], x + 6, y + 5, C_BLANCO) + glifos(F, cad[T_BOTON], x + 45, y + h - 38, C_BLANCO):
        pinta(lienzo, Ff, *o[1:])
    ico, p = iconos_dc(D, F)[4]                         # boton 4 (cruz del PS2, A en la Dreamcast)
    M = p['M'].copy(); M[2, 0] += x + 27 - 320; M[2, 1] += y + h - 30 - 240
    ii = Image.fromarray(ico).resize((int(round(p['w'])), int(round(p['h']))))
    A = np.linalg.inv(M); capa = Image.new('RGBA', (640, 480), (0, 0, 0, 0))
    capa.alpha_composite(ii.transform((640, 480), Image.AFFINE, (A[0, 0], A[1, 0], A[2, 0], A[0, 1], A[1, 1], A[2, 1]),
                                      resample=Image.BILINEAR))
    capa = np.array(capa); pinta(lienzo, capa, 0, 0, 640, 480, 0, 0, C_BLANCO)
    bx0, by0, bx1, by1 = x, y, x + w, y + h
    # fuentes de imagen: 0 la fuente, 1..2 resalte sin y con cursor
    fuentes = [Ff, resalte(T, xst, w_int, C_NOSEL), resalte(T, xst, w_int, C_SEL)]
    grupos = []
    for k in range(3): grupos.append(caja(F, cad[T_AYUDA[k]], ix, iy, w_int, C_TEXTO))
    for n in range(17): grupos.append(caja(F, cad[T_PRUEBA].replace(b'%d', str(n).encode()), ix, iy, w_int, C_TEXTO))
    def fila(i, texto, sel):
        return [(1 + sel, 0, 0, w_int, 15, lx, ly + 16 * i, C_BLANCO)] + glifos(F, texto, lx + 7, ly + 16 * i, C_TEXTO)
    for i, t in enumerate(T_LISTA_PAL):
        for sel in (0, 1): grupos.append(fila(i, cad[t], sel))
    for i, t in enumerate(T_SINO):
        for sel in (0, 1): grupos.append(fila(i, cad[t], sel))
    grupos = [[(0 if o[0] == 'F' else o[0],) + tuple(o[1:]) for o in g] for g in grupos]
    fuentes[0] = Ff[:max(o[2] + o[4] for g in grupos for o in g if o[0] == 0), :16 * F.w]   # solo las filas usadas
    for g in grupos:
        for o in g: assert bx0 <= o[5] and o[5] + o[3] <= bx1 and by0 <= o[6] and o[6] + o[4] <= by1, 'fuera de la base'
    # fichero
    base = lienzo[by0:by1, bx0:bx1]
    q = np.clip(np.round(base), 0, 255).astype(int)
    b565 = ((q[..., 0] >> 3) << 11 | (q[..., 1] >> 2) << 5 | (q[..., 2] >> 3)).astype('<u2').tobytes()
    out = bytearray(32)
    def alinea(): out.extend(bytes(-len(out) % 32))
    alinea(); o_base = len(out); out += b565
    alinea(); o_fu = len(out); out += bytes(12 * len(fuentes)); dat = []
    for i, f in enumerate(fuentes):
        alinea(); off = len(out); a = f.astype(np.uint32)
        out += ((a[..., 3] << 24) | (a[..., 0] << 16) | (a[..., 1] << 8) | a[..., 2]).astype('<u4').tobytes()
        struct.pack_into('<HHHHI', out, o_fu + 12 * i, f.shape[1], f.shape[0], 0, 0, off)
    alinea(); o_gr = len(out); out += bytes(8 * len(grupos)); n = 0; ords = bytearray()
    for i, g in enumerate(grupos):
        struct.pack_into('<II', out, o_gr + 8 * i, n, len(g)); n += len(g)
        for o in g: ords += struct.pack('<5H2hHI', *o[:7], 0, o[7])
    out += ords
    struct.pack_into('<4s4H5I', out, 0, b'MDV1', bx0, by0, bx1 - bx0, by1 - by0, o_base, len(fuentes), o_fu, len(grupos), o_gr)
    os.makedirs(salida, exist_ok=True)
    open(os.path.join(salida, 'MODOV.DAT'), 'wb').write(out)
    print('MODOV.DAT %d bytes, ventana (%d, %d) %dx%d, s1 %d, %d grupos' % (len(out), x, y, w, h, s1, len(grupos)))
    # maquetas (lo que pinta modo_video.c)
    if previas:
        os.makedirs(previas, exist_ok=True)
        def maqueta(nombre, gs):
            l = lienzo.copy()
            for g in gs:
                for o in grupos[g]: pinta(l, fuentes[o[0]], *o[1:])
            Image.fromarray(np.clip(np.round(l), 0, 255).astype(np.uint8)).save(os.path.join(previas, 'modov_%s.png' % nombre))
        maqueta('pal0', [0, 21, 22]); maqueta('pal1', [1, 20, 23])
        maqueta('prueba15', [3 + 15, 27, 28]); maqueta('prueba15_si', [3 + 15, 26, 29])

if __name__ == '__main__':
    if len(sys.argv) < 3: print(__doc__); sys.exit(1)
    convierte(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else sys.argv[2])
