"""Graficos de la VMU (Visual Memory Unit) de la Dreamcast a partir de OutRun 2006 Coast 2 Coast (PS2) -> VMU.DAT.

Uso: python3 conv_vmu.py <iso> <dir_salida> [dir_previas]
Escribe <dir_salida>/VMU.DAT y, en dir_previas (por defecto el de salida), previsualizaciones PNG ampliadas:
vmu_icono.png (los cuadros del icono x8 y la tira animada), vmu_eyecatch.png (x4), vmu_lcd.png (las imagenes de la
pantalla LCD de la VMU x8, con el aspecto de la pantallita: pixeles oscuros sobre verde) y vmu_todo.png (todo junto).

Nada se dibuja a mano: todo sale del disco al convertir.
 - Logo: el de la pantalla de titulo (cuadro quieto de la pelicula \\mv\\ts60.pss, el mismo que TITULO_LOGO de
   MENU.DAT: conv_menu.logo_titulo). Se recorta en sus partes por los huecos del propio dibujo (columnas y filas sin
   trazo): "OutRun" con las palmeras, "2006" y "Coast 2 Coast".
 - Icono del fichero de guardado (gestor de memoria de la BIOS): 32x32 de 16 colores (paleta ARGB4444 compartida por
   los cuadros), 2 cuadros: "OutRun" con las palmeras y "2006", cada uno encajado en 32x32 sobre el blanco del logo.
   Cuantizacion: k-medias en un espacio con peso perceptivo (sobre los pixeles de los dos cuadros) con la paleta
   ajustada a 4 bits por canal en cada iteracion (los colores reales del fichero), sin tramado (a 32x32 solo mete
   ruido). El icono del PS2 (SYSTEM.PS2 \\VIEW.ICO) es un modelo 3D del coche: no sirve para 32x32 plano.
 - Eyecatch (la imagen de 72x56 que la BIOS ensena al elegir el fichero): el logo entero, 256 colores ARGB4444.
 - LCD de la VMU (48x32, 1 bit): logo en dos imagenes que se alternan en el titulo y los menus ("OutRun" con las
   palmeras y "2006"; el logo entero a 48 pixeles de ancho no se lee: las letras del logo son muy estrechas); el pixel
   se enciende si lo oscuro (el trazo azul marino del logo) cubre bastante de su area. En carrera: velocidad con las
   cifras del velocimetro del HUD (DIG_VELOC, estilo 2 de la tabla 0x404818 del ELF, banco SPRANI 0x2C, como
   conv_hud.py) reducidas a 19 pixeles de alto (su relleno claro, sin el contorno), "%03d" como el HUD; debajo la caja
   de cambios del HUD (cifras de CAJA6, escena 0x2C009C, a 9 pixeles de alto, 5 o 6 casillas como CAJA5 / CAJA6) con
   un marco en la marcha actual, como el resalte MARCHA del original (en inverso no se leia). El rotulo km/h del HUD (0x2C00A9) se probo y a
   10 pixeles no se lee: no va.

VMU.DAT (little endian):
  +0  'VMU1'
  +4  u16 cuadros del icono, u16 velocidad de la animacion (campo icon_anim_speed de la cabecera VMS)
  +8  u16 tipo de eyecatch (VMUPKG_EC_*: 2 = 256 colores), u16 n imagenes LCD
  +12 u32 desplazamiento de la paleta del icono (16 x u16 ARGB4444)
  +16 u32 desplazamiento de los iconos (512 bytes por cuadro: 32x32 a 4 bits, nibble alto = pixel izquierdo)
  +20 u32 desplazamiento del eyecatch, +24 u32 su tamano (256 x u16 de paleta + 72x56 bytes)
  +28 u32 desplazamiento de la tabla de imagenes LCD (n x u32 desplazamiento)
  imagen LCD: u8 x, y (posicion en la pantalla del primer cuadro/caracter), ancho, alto, n cuadros, avance (pixeles
    entre caracteres), u16 0; despues n cuadros de alto filas de (ancho + 7) / 8 bytes (bit 7 = pixel izquierdo,
    1 = encendido), tal como se ve la pantalla (la VMU va al reves en el mando: comun/vmu.c gira 180 grados al
    enviarla). Imagenes: 0 LOGO_A, 1 LOGO_B (48x32), 2 VELOCIDAD (10 cifras), 3 MARCHA (cifras 1..6 de la caja de
    cambios: cuadro m - 1; x = margen de la cifra dentro de su casilla de 'avance' pixeles; la fila de casillas va
    centrada en la pantalla y la marcha actual lleva un marco de 1 pixel a 2 pixeles de la cifra).
"""
import os, struct, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conv_hud import Disco, Hud, escena, dibuja_grupo

EC_256COL = 2
VEL_ANIM = 30               # icon_anim_speed: cuadros de la BIOS entre cuadros del icono (~1 s)
LCD_W, LCD_H = 48, 32
VEL_ALTO = 18               # alto de las cifras de la velocidad en la LCD
MARCHA_ALTO, MARCHA_PASO = 9, 8    # cifras de la caja de cambios en la LCD
ESTILOS = 0x404818          # tabla de fuentes de digitos del HUD {token, avance, dx, dy} (conv_hud.convierte)
T_CAJA6 = 0x2c009c

# ---------------------------------------------------------------- logo
def lum(rgb):
    return rgb[..., :3].astype(float) @ [0.299, 0.587, 0.114]

def tramos(v):
    """[(ini, fin)] de los tramos True de un vector"""
    out = []; ini = None
    for i, x in enumerate(list(v) + [False]):
        if x and ini is None: ini = i
        if not x and ini is not None: out.append((ini, i)); ini = None
    return out

def partes_logo(logo):
    """recortes (x0, y0, x1, y1) de "OutRun" (con las palmeras), "2006", la linea de arriba entera, "Coast 2 Coast" y
    el logo entero, por los huecos del dibujo (trazo = luminancia < 200 sobre el blanco)"""
    t = lum(logo) < 200
    filas = tramos(t.any(1))
    # la linea de arriba (OutRun2006) y la de abajo (Coast 2 Coast): el hueco de filas mas ancho
    huecos = [(filas[i + 1][0] - filas[i][1], i) for i in range(len(filas) - 1)]
    _, k = max(huecos)
    y0, y1 = filas[0][0], filas[k][1]; y2, y3 = filas[k + 1][0], filas[-1][1]
    caja = lambda x0, x1, ya, yb: (lambda ys, xs: (x0 + xs[0], ya + ys[0], x0 + xs[-1] + 1, ya + ys[-1] + 1))(
        np.nonzero(t[ya:yb, x0:x1].any(1))[0], np.nonzero(t[ya:yb, x0:x1].any(0))[0])
    # en la linea de arriba, "2006" empieza en la columna casi vacia (la n de Run y el 2 solo se tocan por el brillo
    # de abajo) mas a la izquierda entre el 50 y el 70 % del ancho (las siguientes separan las cifras)
    W = logo.shape[1]; cuenta = t[y0:y1].sum(0)
    bajas = [(a, b) for a, b in tramos(cuenta <= 4) if 0.5 * W <= (a + b) / 2 <= 0.7 * W]
    assert bajas, 'logo: no se separa el 2006'
    x_2006 = bajas[0][1]
    outrun = caja(0, x_2006 - 1, y0, y1); dos = caja(x_2006, logo.shape[1], y0, y1)
    return dict(outrun=outrun, a2006=dos, arriba=caja(0, logo.shape[1], y0, y1),
                c2c=caja(0, logo.shape[1], y2, y3), todo=caja(0, logo.shape[1], y0, y3))

def encaja(img, c, W, H, margen=1, fondo=255):
    """el recorte c de img (RGB) reducido (LANCZOS) y centrado en W x H sobre el fondo"""
    im = Image.fromarray(img[..., :3]).crop(c); w, h = im.size
    s = min((W - 2 * margen) / w, (H - 2 * margen) / h); w2, h2 = max(1, round(w * s)), max(1, round(h * s))
    o = Image.new('RGB', (W, H), (fondo,) * 3); o.paste(im.resize((w2, h2), Image.LANCZOS), ((W - w2) // 2, (H - h2) // 2))
    return np.asarray(o)

def lcd_logo(logo, c, umbral=150, cubre=0.3):
    """recorte c del logo a 1 bit en 48x32: encendido si lo oscuro (luminancia < umbral) cubre mas de 'cubre' del
    pixel (reduccion por area); centrado"""
    x0, y0, x1, y1 = c; osc = (lum(logo[y0:y1, x0:x1]) < umbral).astype(np.uint8) * 255
    w, h = x1 - x0, y1 - y0; s = min(LCD_W / w, LCD_H / h); w2, h2 = max(1, round(w * s)), max(1, round(h * s))
    m = np.asarray(Image.fromarray(osc).resize((w2, h2), Image.BOX)) / 255.0 > cubre
    out = np.zeros((LCD_H, LCD_W), bool); oy, ox = (LCD_H - h2) // 2, (LCD_W - w2) // 2
    out[oy:oy + h2, ox:ox + w2] = m
    return out

# ---------------------------------------------------------------- cuantizacion
PESO = np.array([0.30, 0.59, 0.11]) ** 0.5     # distancia con peso perceptivo (luminancia)

def a4(c):
    """canales a 4 bits (lo que guarda ARGB4444) y de vuelta a 8"""
    return (np.clip(np.round(c / 17.0), 0, 15) * 17).astype(float)

def kmedias(px, k, iters=40):
    """paleta de k colores (ajustada a 4 bits) para los pixeles px (n x 3): arranque con el corte por la mediana de PIL
    y k-medias con peso perceptivo; los pixeles repetidos (el blanco del fondo) pesan como uno por color distinto mas la
    raiz de su cuenta, para que el fondo no se coma la paleta"""
    col, cnt = np.unique(px.astype(np.uint8), axis=0, return_counts=True)
    w = np.sqrt(cnt).astype(float)
    im = Image.fromarray(px.reshape(1, -1, 3).astype(np.uint8)).quantize(k, method=Image.Quantize.MEDIANCUT)
    pal = a4(np.array(im.getpalette()[:3 * k], float).reshape(-1, 3))
    c = col.astype(float)
    for _ in range(iters):
        d = (((c[:, None, :] - pal[None]) * PESO) ** 2).sum(2); a = d.argmin(1)
        nueva = pal.copy()
        for j in range(len(pal)):
            m = a == j
            if m.any(): nueva[j] = (c[m] * w[m, None]).sum(0) / w[m].sum()
            else: nueva[j] = c[d.min(1).argmax()]          # color vacio: al pixel peor representado
        nueva = a4(nueva)
        if np.array_equal(nueva, pal): break
        pal = nueva
    return np.unique(pal, axis=0)

def indices(img, pal):
    d = (((img[..., None, :3].astype(float) - pal[None, None]) * PESO) ** 2).sum(3)
    return d.argmin(2)

def argb4444(c, a=15):
    r, g, b = (int(v) // 17 for v in c[:3]); return (a << 12) | (r << 8) | (g << 4) | b

# ---------------------------------------------------------------- HUD (cifras, caja de cambios, km/h)
def hud_imagenes(D):
    """imagenes RGBA del HUD a 640x480: cifras de la velocidad (10, mismo recuadro) y caja de cambios de 6 marchas"""
    hud = Hud(); a, T, _ = D.banco(0x2c); tok = lambda t: t & 0xffff
    t, av = D.elf32(ESTILOS + 16 * 2, 2)                     # estilo 2: DIG_VELOC
    g = hud.grupo('DIG_VELOC', 10)
    for d in range(10): hud.piezas_escena(g, d, 0x2c, T, escena(a, tok(t), d))
    cifras = []
    for d in range(10):
        img = Image.new('RGBA', (640, 480)); dibuja_grupo(img, hud, g, d, 100, 200); cifras.append(np.asarray(img))
    def simple(tk):
        gg = hud.grupo('X%X' % tk); hud.piezas_escena(gg, 0, 0x2c, T, escena(a, tok(tk), 0.0))
        img = Image.new('RGBA', (640, 480)); dibuja_grupo(img, hud, gg, 0, 0, 0); return np.asarray(img)
    return cifras, av, simple(T_CAJA6)

def bbox(al):
    ys, xs = np.nonzero(al)
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1

def reduce_mascara(m, w2, h2, cubre=0.5):
    return np.asarray(Image.fromarray(m.astype(np.uint8) * 255).resize((w2, h2), Image.BOX)) / 255.0 > cubre

def lcd_carrera(D):
    """imagenes de la LCD en carrera: cifras de la velocidad y de la caja de cambios"""
    cifras, av, caja = hud_imagenes(D)
    # cifras: el relleno claro (sin el contorno azul marino, que las juntaria), el mismo recuadro para las 10 (linea
    # base comun), reducidas a VEL_ALTO de alto
    al = [(c[..., 3] > 96) & (lum(c) > 100) for c in cifras]
    x0 = min(bbox(m)[0] for m in al); y0 = min(bbox(m)[1] for m in al)
    x1 = max(bbox(m)[2] for m in al); y1 = max(bbox(m)[3] for m in al)
    s = VEL_ALTO / (y1 - y0); w2 = round((x1 - x0) * s); avl = int(round(av * s))
    dig = [reduce_mascara(m[y0:y1, x0:x1], w2, VEL_ALTO, 0.4) for m in al]
    vx = max(0, LCD_W - (2 * avl + w2))                     # arriba, alineada a la derecha
    # caja de cambios: casillas entre las lineas claras del marco (luminancia > 200 en todo el alto); la cifra de
    # cada casilla = lo gris claro dentro de ella (entre el fondo oscuro y el marco). En la LCD: fila de cifras de
    # MARCHA_ALTO y 5 de ancho con un paso de 8 pixeles; la marcha actual lleva un marco alrededor, como el resalte
    # MARCHA del original (un recuadro naranja sobre su casilla; en inverso, a este tamano, no se leia)
    l = lum(caja); ca = caja[..., 3] > 32; cx0, cy0, cx1, cy1 = bbox(ca)
    marco = l[cy0:cy1, cx0:cx1] > 200
    filas = tramos(~marco.all(1)); fy0, fy1 = max(filas, key=lambda r: r[1] - r[0])
    cols = [r for r in tramos(~marco[fy0:fy1].all(0)) if r[1] - r[0] > 4]
    assert len(cols) == 6, 'CAJA6: %d casillas' % len(cols)
    cif = [(lambda sub: (sub > 80) & (sub <= 200))(l[cy0 + fy0:cy0 + fy1, cx0 + a0:cx0 + a1]) for a0, a1 in cols]
    wmax = min(r[1] - r[0] for r in cols)
    cif = [c[:, :wmax] for c in cif]
    bx0 = min(bbox(c)[0] for c in cif); by0 = min(bbox(c)[1] for c in cif)
    bx1 = max(bbox(c)[2] for c in cif); by1 = max(bbox(c)[3] for c in cif)
    mw = min(MARCHA_PASO - 3, round((bx1 - bx0) * MARCHA_ALTO / (by1 - by0)))
    marcha = [reduce_mascara(c[by0:by1, bx0:bx1], mw, MARCHA_ALTO, 0.3) for c in cif]
    yb = LCD_H - MARCHA_ALTO - 2
    return [dict(nombre='VELOCIDAD', x=vx, y=0, avance=avl, cuadros=dig),
            dict(nombre='MARCHA', x=1, y=yb, avance=MARCHA_PASO, cuadros=marcha)]

# ---------------------------------------------------------------- salida
def bits(m):
    """filas de (ancho + 7) / 8 bytes, bit 7 = pixel izquierdo"""
    h, w = m.shape; o = bytearray()
    for y in range(h):
        for x in range(0, w, 8):
            b = 0
            for i in range(8):
                if x + i < w and m[y, x + i]: b |= 0x80 >> i
            o.append(b)
    return bytes(o)

def compone(lcd, vel, marcha, seis=True):
    """pantalla de carrera de ejemplo (para la previa; comun/vmu.c hace lo mismo)"""
    P = np.zeros((LCD_H, LCD_W), bool); I = {i['nombre']: i for i in lcd}
    t = '%03d' % vel; v = I['VELOCIDAD']
    for k, ch in enumerate(t):
        g = v['cuadros'][int(ch)]; x = v['x'] + k * v['avance']; h, w = g.shape
        P[v['y']:v['y'] + h, x:x + w] |= g[:, :max(0, min(w, LCD_W - x))]
    m = I['MARCHA']; n = 6 if seis else 5; x0 = (LCD_W - n * m['avance']) // 2
    for k in range(n):
        g = m['cuadros'][k]; h, w = g.shape; x = x0 + k * m['avance'] + m['x']; y = m['y']
        P[y:y + h, x:x + w] |= g
        if k + 1 == marcha:                                 # marco: 2 pixeles fuera de la cifra (como comun/vmu.c)
            xa, xb = max(0, x - 2), min(LCD_W - 1, x + w + 1)
            P[y - 2, xa:xb + 1] = P[y + h + 1, xa:xb + 1] = True
            if x - 2 >= 0: P[y - 2:y + h + 2, x - 2] = True
            if x + w + 1 < LCD_W: P[y - 2:y + h + 2, x + w + 1] = True
    return P

def previa_lcd(m, esc=8):
    """pixeles de la LCD como se ven: oscuros sobre verde, con rejilla"""
    h, w = m.shape; o = np.zeros((h * esc + 2 * esc, w * esc + 2 * esc, 3), np.uint8); o[:] = (150, 178, 140)
    for y in range(h):
        for x in range(w):
            c = (28, 40, 52) if m[y, x] else (140, 168, 130)
            o[esc + y * esc:esc + y * esc + esc - 1, esc + x * esc:esc + x * esc + esc - 1] = c
    return Image.fromarray(o)

def convierte(iso, salida, previas=None):
    from conv_menu import logo_titulo
    D = Disco(iso); previas = previas or salida
    logo, _ = logo_titulo(D)
    logo = logo[..., :3]
    P = partes_logo(logo)
    print('logo %dx%d, partes %s' % (logo.shape[1], logo.shape[0], P))
    # icono: 2 cuadros con una paleta de 16 colores
    cuadros = [encaja(logo, P['outrun'], 32, 32), encaja(logo, P['a2006'], 32, 32)]
    pal = kmedias(np.concatenate([c.reshape(-1, 3) for c in cuadros]), 16)
    assert len(pal) <= 16
    idx = [indices(c, pal) for c in cuadros]
    iconos = b''
    for ix in idx:
        iconos += bytes(((ix[y, 2 * x] << 4) | ix[y, 2 * x + 1]) for y in range(32) for x in range(16))
    pal16 = struct.pack('<16H', *([argb4444(c) for c in pal] + [0] * (16 - len(pal))))
    err = np.mean([np.abs(pal[i].astype(float) - c).mean() for i, c in zip(idx, cuadros)])
    print('icono: %d colores, error medio %.1f' % (len(pal), err))
    # eyecatch: logo entero en 72x56, 256 colores
    ec = encaja(logo, P['todo'], 72, 56, margen=2)
    pe = kmedias(ec.reshape(-1, 3), 256, iters=12)
    ie = indices(ec, pe)
    ecd = struct.pack('<256H', *([argb4444(c) for c in pe] + [0] * (256 - len(pe)))) + bytes(ie.astype(np.uint8).ravel())
    assert len(ecd) == 512 + 72 * 56
    print('eyecatch: %d colores, error medio %.1f' % (len(pe), np.abs(pe[ie] - ec).mean()))
    # LCD
    lcd = [dict(nombre='LOGO_A', x=0, y=0, avance=0, cuadros=[lcd_logo(logo, P['outrun'], 150, 0.45)]),
           dict(nombre='LOGO_B', x=0, y=0, avance=0, cuadros=[lcd_logo(logo, P['a2006'], 150, 0.45)])] + lcd_carrera(D)
    imgs = []
    for i in lcd:
        h, w = i['cuadros'][0].shape
        imgs.append(struct.pack('<6BH', i['x'], i['y'], w, h, len(i['cuadros']), i['avance'], 0) +
                    b''.join(bits(c) for c in i['cuadros']))
    # fichero
    o_pal = 32; o_ico = o_pal + 32; o_ec = o_ico + len(iconos); o_tab = o_ec + len(ecd)
    out = bytearray(b'VMU1' + struct.pack('<4H5I', len(cuadros), VEL_ANIM, EC_256COL, len(lcd), o_pal, o_ico, o_ec,
                                          len(ecd), o_tab))
    assert len(out) == 32
    out += pal16 + iconos + ecd
    o = o_tab + 4 * len(lcd); tab = b''
    for d in imgs: tab += struct.pack('<I', o); o += len(d)
    out += tab + b''.join(imgs)
    os.makedirs(salida, exist_ok=True)
    open(os.path.join(salida, 'VMU.DAT'), 'wb').write(out)
    print('VMU.DAT: %d bytes' % len(out))
    # previas
    os.makedirs(previas, exist_ok=True)
    fr = [Image.fromarray(pal[ix].astype(np.uint8)).resize((256, 256), Image.NEAREST) for ix in idx]
    ori = [Image.fromarray(c).resize((256, 256), Image.NEAREST) for c in cuadros]
    hoja = Image.new('RGB', (2 * 266, 2 * 266 + 40), (40, 60, 120))
    for k in range(2): hoja.paste(ori[k], (k * 266, 0)); hoja.paste(fr[k], (k * 266, 266))
    for j, c in enumerate(pal):
        hoja.paste(Image.new('RGB', (32, 32), tuple(int(v) for v in c)), (j * 33, 2 * 266 + 4))
    hoja.save(os.path.join(previas, 'vmu_icono.png'))
    fr[0].save(os.path.join(previas, 'vmu_icono.gif'), save_all=True, append_images=fr[1:], duration=1000, loop=0)
    Image.fromarray(pe[ie].astype(np.uint8)).resize((288, 224), Image.NEAREST).save(os.path.join(previas, 'vmu_eyecatch.png'))
    pant = [lcd[0]['cuadros'][0], lcd[1]['cuadros'][0], compone(lcd, 254, 5), compone(lcd, 87, 2, False)]
    pl = [previa_lcd(m) for m in pant]; w, h = pl[0].size
    hl = Image.new('RGB', (2 * w + 10, 2 * h + 10), 'white')
    for k, p in enumerate(pl): hl.paste(p, ((k % 2) * (w + 10), (k // 2) * (h + 10)))
    hl.save(os.path.join(previas, 'vmu_lcd.png'))
    todo = Image.new('RGB', (max(hl.size[0], 2 * 266 + 300), hl.size[1] + 2 * 266 + 60), (220, 220, 220))
    todo.paste(hoja, (0, 0)); todo.paste(Image.open(os.path.join(previas, 'vmu_eyecatch.png')), (2 * 266 + 10, 0))
    todo.paste(hl, (0, 2 * 266 + 50)); todo.save(os.path.join(previas, 'vmu_todo.png'))
    return out

if __name__ == '__main__':
    if len(sys.argv) < 3: print(__doc__); sys.exit(1)
    convierte(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
