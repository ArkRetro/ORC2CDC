"""Menus del modo principal de OutRun 2006 Coast 2 Coast (PS2) -> MENU.DAT para la Dreamcast.

Uso: python3 conv_menu.py <iso> <dir_salida> [dir_previas]
Escribe <dir_salida>/MENU.DAT y, en dir_previas (por defecto el de salida), una maqueta PNG por pantalla
(menu_<pantalla>.png) y una hoja de cuadros por grupo animado (menu_hoja_<pantalla>.png).

Origen (todo se lee del disco al convertir; aqui solo hay tokens de escena, direcciones y numeros de cuadro):
 - SPRANI banco 0x44 = SUMO_FE_CVT (frontal de C2C) con su XST spr_sprani_sumo_fe_cvt_exst. OJO: SPRITE.PS2 tiene una
   entrada sin nombre (copia de ranking_cvt_exst) delante de route_cvt_exst, asi que desde route_cvt_exst el indice
   real del paquete es el del nombre + 1 (banco_menu()). conv_hud.Disco.banco() no lo corrige (no lo necesita).
 - Los tokens del PS2 NO son los del port de PC (la escena 0x440045 del PC es la 0x440064 del PS2): se han sacado de
   las tablas {token, primer cuadro, ultimo cuadro} del ELF (carruseles 51B7B0 del PC) y comprobado a ojo:
     menu principal 0x440000 (tabla 0x418CA8: 5 opciones, entra 0-20, opcion i quieta en 20+20i, 0-132),
     un jugador 0x440029 (tabla 0x41C048: COAST 2 COAST, OUTRUN, HEART ATTACK, TIME ATTACK, SHOWROOM, igual),
     cambio 0x440064 (tabla 0x4209A0: entra 0-25, AT 25-45 en bucle, MT 95-115 en bucle, AT ok 46-70, MT ok 116-138,
       AT atras 75-90, MT atras 145-160), radio de musica 0x4400A7 (PC 0x440088: entra 0-25, sale al reves x2),
     ayuda de musica 0x4400A6 (PC 0x440087, 0-35), listas de musica (tabla 0x419298): carrusel 0x4400C6 / 0x4400C7 /
       0x440033 (9 / 7 / 12 temas: entra 0-20, tema k quieto en 20+40k), titulos 0x440030 / 0x44002F / 0x44002E
       (110 cuadros por tema), nombre de lista 0x4400C8 (cuadros 1, 2, 0), candado 0x440066 cuadro 5,
     ficha del coche 0x4400E5 / 0x4400E6 (normales / "_t", del codigo 0x2445D8 = 4C9010 del PC; logo, clase, cambios y
       barras; coche k quieto en 20(k+1), 15 coches en el orden de la tabla 0x375968; el 0x44007E de antes es otro
       carrusel, con otro orden),
     cabeceras rojas 0x440070 (main menu), 0x4400C4 (mode select), 0x440072 (car select), 0x44006E (music select;
       tabla de 0x23D4B8 por clave de pantalla), fondo 0x4400AB,
     boton START = imagen 1480 del XST (textura 354).
 - Pantalla de titulo: en el PS2 es la pelicula \\mv\\ts60.pss (MV.PS2, MPEG-2 640x480 a 29,97 fps, 17,7 s) con el logo
   "OutRun2006 Coast 2 Coast" sobre blanco; el texto "Press Start" no existe en TEXT.PS2. Se extrae con ffmpeg un
   cuadro quieto del logo (t = 14 s) y se recorta (TITULO_LOGO, opaca); START parpadeante = boton START del frontal.
 - El coche de la seleccion es el modelo 3D (COCHE.DAT).
 - Barra de ayuda (AYUDA_<pantalla>, en COMUN): iconos de los botones (banco 0x48 SUMO_LOADING, tabla del ELF
   0x374A60, cuadro 25) con la letra del boton de la Dreamcast, y sus textos (TEXT.PS2, formato 'txet') con la fuente
   14 del juego (spr_font_xst y metricas e interletraje del ELF, tabla 0x344D18), en los huecos de 0x4168F0, como
   0x2394E0 / 0x2398B8. Los textos se componen aqui: cada glifo es una pieza del grupo (sin codigo de texto en la DC).
   Idioma: OR2_IDIOMA=es (por defecto): textos de SPANISH.bin y XST del frontal en espanol (_sxst); en = ingles. La
   version PAL elige el idioma por el de la consola; el port lo fija al convertir (menu y HUD en espanol).
 - Cabeceras: 0-40 entrada, 41-132 destello sobre el rotulo (tramo BRILLO, 40-199 en bucle).
 - Texturas: arreglado el desordenado de las de mas de una fila de paginas (conv_hud.texturas: ancho de bufer de la
   subida = ancho / 128); con ello el fondo y los rotulos salen enteros. Las grandes van en VQ (tools/vq.py) si el
   error es pequeno (VQ_UMBRAL); VQ=0 en el entorno: todo en 16 bits.
 - Capas mascara de SPRANI (flag 8): ninguna escena de estas pantallas las usa (se comprueba al convertir); si
   apareciera una, conv_hud.escena la hornea en la pieza siguiente (ver conv_hud._capa).

MENU.DAT: mismo formato que HUD.DAT (ver conv_hud.py; cabecera 'HUD1', texturas, sprites, grupos, piezas, usos y los
16 bytes de tacometros, aqui todos 0xFF), mas un bloque 'MNU1' justo detras de los tacometros (off_tacometros + 16),
que el lector de comun/hud.c no mira:
  'MNU1', u32 n_pantallas, u32 n_tramos, u32 0
  pantallas 48 bytes: char nombre[16]; u16 primer grupo, n grupos, primer uso, n usos, primera textura, n texturas;
    u32 bytes de VRAM de sus texturas; u32 0, 0, 0, 0
  tramos 32 bytes: char nombre[16]; u16 uso, cuadro inicial, cuadro final, modo (0 una vez y se queda en el final,
    1 bucle, 2 quieto); u32 0, 0
Cada pantalla tiene sus propias texturas (atlas recortados al alfa, clase conv_hud.Atlas): un cargador de menus sube
solo las de la pantalla activa (y las de COMUN) y las libera al salir. Un grupo con n cuadros guarda el cuadro c de la
escena (60 fps) como cuadro c - inicio del tramo del grupo; los usos estan en (0, 0) salvo el candado del coche.
"""
import os, struct, subprocess, sys, tempfile, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conv_hud import Disco, Ani, Atlas, Hud, texturas, escena, hornea, imagen_xst, dibuja_grupo, _t, _s

B = 0x44                                # SUMO_FE_CVT
LETRA = ['s']                           # XST del idioma (OR2_IDIOMA=es por defecto: los _sxst y SPANISH.bin)
MAX_CUADROS, MAX_PIEZAS = 1400, 40000   # limites por grupo (evitan conversiones desbocadas)

def banco_menu(D, b):
    """(Ani, texturas, xst) del banco b con el indice de SPRITE.PS2 corregido (ver docstring)"""
    nom = D.ani[b - 0x20]; clave = nom[len('ani_SPRANI_'):].lower()
    ia = D.n_ani.index(nom.lower() + '.sz'); lo = D.n_spr
    ix = next(lo.index(c) for c in ('spr_sprani_%s_%sxst.sz' % (clave, LETRA[0]), 'spr_sprani_%s_exst.sz' % clave,
                                     'spr_sprani_%s_xst.sz' % clave) if c in lo)
    if len(D.p_spr) == len(lo) + 1 and ix >= lo.index('spr_sprani_route_cvt_exst.sz'): ix += 1
    xst = D._ent(D.p_spr, ix); a = Ani(D._ent(D.p_ani, ia)); T = texturas(xst)
    assert struct.unpack_from('<I', xst, 12)[0] == len(T) and (b != B or len(T) == 380), 'XST del banco %X dudoso' % b
    return a, T, xst

def tabla(D, va, n):
    """n registros {token, primero, ultimo} del ELF"""
    w = D.elf32(va, 3 * n); return [tuple(w[3 * i:3 * i + 3]) for i in range(n)]

COCHE_MODELOS = 0x375968   # 30 u32: modelo (car+0x11) de cada ficha (15 normales y 15 "_t")
COCHE_DESBLOQUEO = 0x3759e0  # 30 i16: bit de la licencia (+0x28) que desbloquea la ficha (las 2 primeras, siempre)
COCHE_COLORES = 0x375a1c   # 30 x 8 i16: bit de cada color (-1 siempre, -2 nunca)
COCHE_COLOR_INI = 0x375c00  # 30 i8: color por defecto de cada ficha

def carrusel(D, va, n):
    """tramos de un carrusel del ELF (51B7B0 del PC): registros {token, primero, ultimo} que alternan entrada o
    movimiento y parada; el primero es la entrada. Devuelve (token, ENTRA, [cuadro de cada opcion])"""
    t = tabla(D, va, n); ini = t[0][1:]; paradas = []
    for tok, a, b in t:
        if a == b and (not paradas or paradas[-1] != a) and b >= ini[1]: paradas.append(a)
    return t[0][0], ini, paradas

# listas de musica: (carrusel, titulos, temas) del PS2 (0x419298, 0x4195D0, 0x419628; 68F36C del PC: 9 / 7 / 12)
MUSICA_LISTAS = [(0x4400c6, 0x440030, 9), (0x4400c7, 0x44002f, 7), (0x440033, 0x44002e, 12)]

def comprueba_tablas(D):
    """los tokens y cuadros usados salen de estas tablas: si el ejecutable no es el esperado, se para"""
    assert tabla(D, 0x418ca8, 2) == [(0x440000, 0, 20), (0x440000, 20, 20)], 'tabla del menu principal'
    assert tabla(D, 0x41c048, 1)[0] == (0x440029, 0, 20), 'tabla de un jugador'
    t = tabla(D, 0x4209a0, 9)
    assert t[0] == (0x440064, 0, 25) and t[3] == (0x440064, 95, 115) and t[6][1:] == (116, 138), 'tabla del cambio'
    w = D.elf32(0x419298, 9)
    assert w[0] == 0x4400a6 and w[4] == 0x4400a7 and w[7] == 0x440066 and w[8] == 5, 'tabla de la musica'
    assert tabla(D, 0x4195d0, 1)[0] == (0x44002f, 0, 110) and tabla(D, 0x419628, 1)[0] == (0x44002e, 0, 110)
    # temas por lista (68F36C del PC), pista de MUSIC.PS2 de cada tema (68F370: la misma) y bit de desbloqueo de cada
    # tema en la licencia (5C9AB0; menu.c lleva la misma tabla: DESBLOQ_TEMA)
    assert D.elf32(0x375da8)[0] & 0xffffff == 0x0c0709 and D.elf32(0x375db0, 28) == tuple(range(28)), 'temas de musica'
    assert D.elf32(0x4191e8, 28) == (174,) * 7 + (172, 173) + tuple(range(174, 193)), 'desbloqueo de los temas'
    # seleccion de coche (0x2445D8, el 4C9010 del PC): ori t0,t0,0xE5 / ori a3,a3,0xE6 (normales y "_t") y la tabla de
    # modelos por ficha (el 68EA10 del PC)
    assert D.elf32(0x244640)[0] == 0x350800e5 and D.elf32(0x244644)[0] == 0x34e700e6, 'seleccion de coche'
    assert D.elf32(COCHE_MODELOS, 3) == (1, 4, 0), 'tabla de modelos por ficha'
    t = tabla(D, 0x416ea8, 23)                                  # editor de licencia (el 4DD7C0 del PC)
    assert t[0] == (0x440086, 0, 0) and t[1] == (0x44008b, 0, 0) and t[2] == (0x44008a, 0, 0), 'tabla del editor'
    assert t[3] == (0x440087, 0, 0x33) and t[10] == (0x440091, 1, 1) and t[11] == (0x44008c, 1, 1), 'tabla del editor'
    assert t[12] == (0x440089, 1, 1) and t[14] == (0x440090, 2, 2) and t[15] == (0x44009f, 0, 0x33), 'tabla del editor'
    t = tabla(D, 0x41f8e0, 6)                                   # elegir licencia (el 4E2970 del PC)
    assert t[1] == (0x440088, 0, 0) and t[3] == (0x44008d, 2, 0x16) and t[5] == (0x44008e, 2, 0x16), 'tabla de licencias'
    assert tabla(D, 0x41d908, 1)[0] == (0x4400a3, 0, 15) and tabla(D, 0x41c260, 1)[0] == (0x44002a, 0, 14)

# ---------------------------------------------------------------- pelicula del titulo
def logo_titulo(D):
    """cuadro quieto del logo de ts60.pss (la entrada de MV.PS2 de ~17,7 s a 29,97 fps), recortado a su contenido"""
    iso = D.iso; cand = []
    with tempfile.TemporaryDirectory() as tmp:
        for i, (h, o, s) in enumerate(iso.paquete('MV.PS2')):
            f = os.path.join(tmp, 'mv%d.pss' % i); open(f, 'wb').write(iso.m[o:o + s])
            r = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries',
                                'stream=r_frame_rate:format=duration', '-of', 'csv=p=0', f], capture_output=True, text=True)
            vals = r.stdout.replace(',', ' ').split()
            try:
                n_, d_ = vals[0].split('/'); fps = float(n_) / float(d_); dur = float(vals[-1])
            except (IndexError, ValueError, ZeroDivisionError): fps = dur = 0.0
            cand.append((abs(dur - 17.7) + abs(fps - 29.97), f))
        f = min(cand)[1]
        r = subprocess.run(['ffmpeg', '-v', 'error', '-ss', '14', '-i', f, '-frames:v', '1', '-f', 'rawvideo',
                            '-pix_fmt', 'rgb24', '-'], capture_output=True)
    img = np.frombuffer(r.stdout, np.uint8)[:640 * 480 * 3].reshape(480, 640, 3)
    oscuro = img.max(axis=2) < 110                              # trazo azul marino del logo (el cielo es claro)
    ys, xs = np.nonzero(oscuro)
    L, R, T, Bt = max(0, xs.min() - 6), min(640, xs.max() + 7), max(0, ys.min() - 6), min(480, ys.max() + 7)
    rgba = np.full((Bt - T, R - L, 4), 255, np.uint8); rgba[..., :3] = img[T:Bt, L:R]
    return rgba, (L, T)

# ---------------------------------------------------------------- textos y fuente del juego
FUENTES = 0x344d18      # 15 punteros a registros de fuente (0x117320 elige la fuente por su numero)
AYUDA_XY = 0x4168f0     # 4 huecos de la barra de ayuda {f32 x, y} (relativos al centro de la pantalla)
AYUDA_DX = 0x416910     # desplazamiento de cada texto (u32, se resta)
AYUDA_ICONO = 0x374a60  # {boton, escena del icono} x 8
FUENTE_AYUDA = 14       # 0x2398b8: fuente 14, escala 1, color blanco; huecos 0-1 flags 9, huecos 2-3 flags 10
IDIOMAS = {'en': (0, 'e'), 'es': (4, 's')}   # entrada de TEXT.PS2 y letra del XST del frontal

def textos(D, idioma):
    """cadenas del fichero 'txet' del idioma (TEXT.PS2: english_us, french, german, italian, spanish); cada caracter
    va en 16 bits (el byte bajo es el codigo de la fuente)"""
    e = D.iso.paquete('TEXT.PS2')[IDIOMAS[idioma][0]]
    d = bytes(D.iso.m[e[1]:e[1] + e[2]])
    assert d[:4] == b'txet', 'TEXT.PS2: no es txet'
    pri = struct.unpack_from('<I', d, 8)[0]; out = []
    for o in range(8, pri, 4):
        off = struct.unpack_from('<I', d, o)[0]
        if not off: break
        c = bytearray()
        while d[off] or d[off + 1]: c.append(d[off]); off += 2
        out.append(bytes(c))
    return out

class Fuente:
    """fuente del juego: registro del ELF {id, metricas*, interletraje*, u16 ancho, alto de celda, primer caracter,
    i32 espaciado x, y, u16 n} y textura id de spr_font_xst (16 celdas por fila). Composicion como 42C480 / 42C610 /
    42C860 del PC (frontend_text.cpp) con el flag 0x100 (metricas): avance = ancho - interletraje(c, siguiente) +
    espaciado (al final o antes de un espacio, ancho + 2 espaciados); espacio = 0,3 celdas; cada glifo se desplaza
    -metrica[0]"""
    def __init__(s, D, fid, T):
        reg = next(q for q in D.elf32(FUENTES, 15) if q and D.elf32(q)[0] == fid)
        w = D.elf32(reg, 8); i32 = lambda v: v - (1 << 32) if v >> 31 else v
        s.id, s.w, s.h, s.pri = fid, w[3] & 0xffff, w[3] >> 16, w[4]
        s.esp = i32(w[5]); s.n = w[7] & 0xffff
        lee = lambda va, n: np.frombuffer(D.elf, np.int8, n, va - 0xfff80)
        s.met = lee(w[1], 2 * s.n).reshape(-1, 2).astype(int)
        s.ker = lee(w[2], s.n * s.n).astype(int) if w[2] else None
        s.T = T; assert T[fid].shape[1] >= 16 * s.w, 'textura de la fuente %d' % fid
    def _par(s, c, sig):
        i = c - s.pri
        if s.ker is not None and sig: return int(s.met[i][1] - s.ker[i * s.n + sig - s.pri])
        return int(s.met[i][1] + s.esp)
    def ancho(s, t):
        a = 0
        for i, c in enumerate(t):
            sig = t[i + 1] if i + 1 < len(t) else 0
            if c == 32: a += int(s.w * 0.3); continue
            a = int(s._par(c, 0 if sig == 32 else sig) + a + s.esp)
        return a
    def piezas(s, t, x, y, flags, alfa=1.0):
        """glifos de la cadena t (bytes) en (x, y) con los flags de 42CDD0 (1 sin alinear, 4 centrado, si no a la
        derecha; 8 centrado en vertical, 16 abajo)"""
        if not flags & 1: x -= s.ancho(t) // 2 if flags & 4 else s.ancho(t)
        if flags & 8: y += int(s.h * -0.5)
        elif flags & 16: y -= s.h
        out = []
        for i, c in enumerate(t):
            sig = t[i + 1] if i + 1 < len(t) else 0
            if c == 32: x += int(s.w * 0.3); continue
            k = c - s.pri
            if 0 <= k < s.n:
                l, tp = (k & 15) * s.w, (k >> 4) * s.h
                out.append(dict(tex=s.id, src=(l, tp, l + s.w, tp + s.h), w=s.w, h=s.h, M=_t(x - s.met[k][0], y),
                                alfa=alfa, anc=(0.0, 0.0), gira=False, bk='FUENTE', tx=s.T))
                x = int(s._par(c, 0 if sig == 32 else sig) + x + s.esp)
        return out

def fuentes_xst(D):
    ix = D.n_spr.index('spr_font_xst.sz')                       # antes de route_cvt_exst: indice sin corregir
    return texturas(D._ent(D.p_spr, ix))

# barra de ayuda de cada pantalla: (boton, texto) por hueco (0x239808 / 0x2398b8). Menu principal: 0x24AD50; un
# jugador: tabla de claves 0x374AA0 (claves 1-8: los mismos cuatro); coche: 0x25EC60 (SELECT, color, BACK); cambio:
# clave 0x10 (SELECT, BACK); musica: 0x24E434 (SELECT, RANDOM, BACK). Botones: 4 cruz, 8 circulo, 0x10 triangulo,
# 0x4000 cuadrado, 0x8000 (L1/R1, icono del triangulo). Textos: 0x294 SELECT, 0x295 BACK, 0x296 FRIENDS,
# 0x299 LICENSE, 0x29A COLOR, 0x29B RANDOM.
AYUDA = {
    'MENU':   [(4, 0x294), (0x8000, 0x296), (0x4000, 0x299), (8, 0x295)],
    'MODO':   [(4, 0x294), (0x8000, 0x296), (0x4000, 0x299), (8, 0x295)],
    'COCHE':  [(4, 0x294), (0x10, 0x29a), None, (8, 0x295)],
    'CAMBIO': [(4, 0x294), None, None, (8, 0x295)],
    'MUSICA': [(4, 0x294), (0x10, 0x29b), None, (8, 0x295)],
    'LICENCIA': [(4, 0x294), None, None, (8, 0x295)],        # 4E19E0: {4, SELECT, 8, BACK}
    'OPCIONES': [(4, 0x294), None, None, (8, 0x295)],
}
# en la Dreamcast: cruz -> A, circulo -> B, triangulo -> Y, cuadrado -> X (colores del mando: A rojo, B azul,
# X amarillo, Y verde). El icono del PS2 se reutiliza (boton redondo) borrando su simbolo y poniendo la letra
BOTON_DC = {4: ('A', (232, 52, 40)), 8: ('B', (40, 96, 232)), 0x10: ('Y', (60, 190, 60)), 0x8000: ('Y', (60, 190, 60)),
            0x4000: ('X', (240, 200, 30)), 0x20: ('X', (240, 200, 30))}

def iconos_dc(D, F):
    """{boton: RGBA del icono} (29x29 aprox.): base redonda del icono del PS2 (escena del banco 0x48, cuadro 25) sin
    el simbolo de color y con la letra del boton de la Dreamcast en la fuente del juego"""
    ab, Tb, _ = banco_menu(D, 0x48)
    w = D.elf32(AYUDA_ICONO, 16); mapa = {w[2 * i]: w[2 * i + 1] for i in range(8)}
    res = {}
    for bt, (letra, col) in BOTON_DC.items():
        ps = [q for q in escena(ab, mapa[bt] & 0xffff, 25.0) if q['alfa'] >= 0.004]
        assert len(ps) == 1, 'icono del boton %x' % bt
        p = ps[0]; l, t, r, b = (int(round(v)) for v in p['src'])
        a = Tb[p['tex']][t:b, l:r].astype(float)
        sat = a[..., :3].max(-1) - a[..., :3].min(-1)            # el simbolo es de color; la base, gris
        gris = np.median(a[..., :3][(sat < 30) & (a[..., 3] > 200)], axis=0)
        k = np.clip((sat - 30) / 60.0, 0, 1)[..., None]
        a[..., :3] = a[..., :3] * (1 - k) + gris * k
        g = F.T[F.id]; c = ord(letra) - F.pri; gl = g[(c >> 4) * F.h:(c >> 4) * F.h + F.h, (c & 15) * F.w:(c & 15) * F.w + F.w]
        al = gl[..., 3:4].astype(float) / 255
        ys, xs = np.nonzero(gl[..., 3] > 40); cy, cx = (ys.min() + ys.max()) / 2, (xs.min() + xs.max()) / 2
        oy, ox = int(round(a.shape[0] / 2 - cy)), int(round(a.shape[1] / 2 - cx))
        for y in range(F.h):
            for x in range(F.w):
                Y, X = y + oy, x + ox
                if 0 <= Y < a.shape[0] and 0 <= X < a.shape[1] and al[y, x, 0] > 0:
                    a[Y, X, :3] = a[Y, X, :3] * (1 - al[y, x, 0]) + np.array(col) * al[y, x, 0]
        res[bt] = (np.clip(a, 0, 255).astype(np.uint8), p)
    return res

def barra_ayuda(D, F, cad, iconos, huecos):
    """piezas (un cuadro) de la barra de ayuda: icono de cada boton en la posicion del hueco (escena de 640x480
    centrada, como 0x2394E0) y su texto a la derecha (huecos 0-1, x + 17) o a la izquierda (2-3, x - 20)"""
    xy = D.elf32(AYUDA_XY, 8); dx = D.elf32(AYUDA_DX, 4)
    f = lambda v: struct.unpack('<f', struct.pack('<I', v))[0]
    ps = []
    for i, h in enumerate(huecos):
        if h is None: continue
        bt, ti = h; x, y = f(xy[2 * i]), f(xy[2 * i + 1])
        img, p = iconos[bt]; M = p['M'].copy(); M[2, 0] += x; M[2, 1] += y
        q = dict(p, tex=('BOTON', bt), src=(0, 0, img.shape[1], img.shape[0]), M=M, bk='BOTON', tx={('BOTON', bt): img})
        ps.append(q)
        tx = int(x) + 17 if i < 2 else int(x) - 20
        ps += F.piezas(cad[ti], tx - dx[i] + 320, int(y) + 240, 9 if i < 2 else 10)
    return ps

# ---------------------------------------------------------------- conversion
def parte(p, lim=500, lim2=250):
    """divide una pieza cuyo rectangulo de textura pase de lim texeles (las tiras de 1024 del fondo no caben en un
    atlas de 512) en trozos; cada trozo lleva su matriz desplazada y la misma ancla en pantalla"""
    l, t, r, b = p['src']; w, h = p['w'], p['h']
    if r - l > lim:
        n = int(np.ceil((r - l) / lim)); out = []
        for i in range(n):
            q = dict(p); l0, l1 = l + (r - l) * i / n, l + (r - l) * (i + 1) / n; x0 = w * i / n
            q['src'] = (l0, t, l1, b); q['w'] = w / n; q['M'] = _t(x0, 0) @ p['M']; q['anc'] = (p['anc'][0] - x0, p['anc'][1])
            out += parte(q, lim, lim2)
        return out
    if b - t > lim or (b - t > lim2 and r - l > lim2):      # el atlas mayor es 512x256 o 256x512
        n = int(np.ceil((b - t) / (lim2 if r - l > lim2 else lim))); out = []
        for i in range(n):
            q = dict(p); t0, t1 = t + (b - t) * i / n, t + (b - t) * (i + 1) / n; y0 = h * i / n
            q['src'] = (l, t0, r, t1); q['h'] = h / n; q['M'] = _t(0, y0) @ p['M']; q['anc'] = (p['anc'][0], p['anc'][1] - y0)
            out += parte(q, lim, lim2)
        return out
    return [p]

MAX_TRAMO = 4095            # cuadros que puede abarcar una pieza (12 bits altos de sus banderas)

def _orden(ks):
    """orden topologico de las claves de capa de unos cuadros (None si se contradicen)"""
    import heapq
    sig = {}; ent = {}; prim = {}
    for c, kk in enumerate(ks):
        for i, k in enumerate(kk):
            prim.setdefault(k, (c, i)); ent.setdefault(k, 0); sig.setdefault(k, set())
            if i and k not in sig[kk[i - 1]]: sig[kk[i - 1]].add(k); ent[k] += 1
    cola = [(prim[k], k) for k in ent if ent[k] == 0]; heapq.heapify(cola); orden = []
    while cola:
        _, k = heapq.heappop(cola); orden.append(k)
        for j in sig[k]:
            ent[j] -= 1
            if ent[j] == 0: heapq.heappush(cola, (prim[j], j))
    return orden if len(orden) == len(ent) else None

def tramos_piezas(lst):
    """[(cuadro, pieza, n cuadros)] de las piezas de un grupo animado sin repetir las que no cambian: cada pieza sale de
    una capa (p['L'] y su aparicion dentro del cuadro); en cada tramo de cuadros seguidos en que las capas guardan el
    mismo orden relativo se ponen en ese orden (topologico) y cada capa se guarda una vez por racha de cuadros iguales.
    Dentro de un cuadro el orden de dibujo es el de la escena (los tramos no se mezclan: van uno detras de otro)"""
    def claves(ps):
        vistas = {}; out = []
        for i, p in enumerate(ps):
            b = p.get('L', ('i', i)); k = vistas.get(b, 0); vistas[b] = k + 1; out.append((b, k))
        return out
    if len(lst) <= 1: return [(c, p, 1) for c, ps in enumerate(lst) for p in ps]
    ks = [claves(ps) for ps in lst]
    fp = lambda p: (p.get('bk'), p['tex'], tuple(round(v, 2) for v in p['src']), tuple(np.round(p['M'][:, :2].ravel(), 3)),
                    round(p['alfa'], 3), p['w'], p['h'], tuple(np.round(p['anc'], 3)))
    out = []; c0 = 0
    while c0 < len(lst):
        c1 = c0 + 1                                             # tramo [c0, c1) lo mas largo posible
        while c1 < len(lst) and _orden(ks[c0:c1 + 1]) is not None: c1 += 1
        orden = _orden(ks[c0:c1]); pos = {k: i for i, k in enumerate(orden)}; porclave = {}; sal = []
        for c in range(c0, c1):
            for p, k in zip(lst[c], ks[c]): porclave.setdefault(k, []).append((c, p))
        for k in orden:
            run = None
            for c, p in porclave[k]:
                if run and run[0] + run[2] == c and run[3] == fp(p) and run[2] < MAX_TRAMO: run[2] += 1; continue
                if run: sal.append((pos[k], run[0], run[1], run[2]))
                run = [c, p, 1, fp(p)]
            if run: sal.append((pos[k], run[0], run[1], run[2]))
        sal.sort(key=lambda v: (v[0], v[1])); out += [(c, p, n) for _, c, p, n in sal]; c0 = c1
    return out

def firma(ps):
    return tuple((p['tex'], tuple(round(v, 2) for v in p['src']), tuple(np.round(p['M'][:, :2].ravel(), 2)),
                  round(p['alfa'], 3)) for p in ps if p['alfa'] >= 0.004)

class Menu(Hud):
    def __init__(s):
        Hud.__init__(s); s.taco = [0xff] * 16; s.pant = []; s.tramos = []; s.grupos_ini = {}; s.vq = vq_menu

def convierte(iso, salida, previas):
    D = Disco(iso); comprueba_tablas(D)
    idioma = os.environ.get('OR2_IDIOMA', 'es'); LETRA[0] = IDIOMAS[idioma][1]
    a, T, xst = banco_menu(D, B)
    F = Fuente(D, FUENTE_AYUDA, fuentes_xst(D)); cad = textos(D, idioma); iconos = iconos_dc(D, F)
    m = Menu(); hojas = {}

    bancos = {B: (a, T)}
    def anim(at, frames, k):
        """lista de piezas por cuadro (registradas en el atlas); k = token (banco << 16 | escena); las de otro banco
        que el del frontal llevan su banco y sus texturas en 'bk' y 'tx'"""
        bk = k >> 16 or B
        if bk not in bancos: bancos[bk] = banco_menu(D, bk)[:2]
        a_, T_ = bancos[bk]; out = []
        for f in frames:
            ps = [q for p in escena(a_, k & 0xffff, float(f)) if p['alfa'] >= 0.004 and abs(np.linalg.det(p['M'][:2, :2])) >= 1e-6
                  for q in parte(hornea(p, T_) if 'mascara' in p else p)]
            for p in ps:
                if bk != B and 'bk' not in p: p['bk'] = 'SPRANI%02X' % bk; p['tx'] = T_   # (clave de texto: se ordena con las de las fuentes)
                at.registra(p.get('bk', B), p.get('tx', T), p)
            out.append(ps)
        return out

    def recorta(lst, frames):
        """quita los cuadros finales iguales al anterior (se queda uno)"""
        while len(lst) > 1 and firma(lst[-1]) == firma(lst[-2]): lst.pop(); frames = frames[:len(lst)]
        return lst, frames

    def pantalla(nombre, elems, extra=None):
        """elems: (grupo, token, cuadros, capa, (x, y), [(tramo, ini, fin, modo)], recortar) para una escena del banco
        B, o dict(g=grupo, lst=[piezas de cada cuadro], capa, xy, tr) para piezas hechas aqui (textos, botones), que
        llevan su banco y sus texturas en 'bk' y 'tx'. Crea el atlas y los grupos"""
        at = Atlas(); datos = []
        for e in elems:
            if isinstance(e, dict):
                for ps in e['lst']:
                    for p in ps: at.registra(p['bk'], p['tx'], p)
                datos.append((e['g'], 0, list(range(len(e['lst']))), e['capa'], e['xy'], e['tr'], e['lst']))
                continue
            g, k, frames, capa, xy, tr, rec = e
            frames = list(frames)
            assert len(frames) <= MAX_CUADROS, '%s: demasiados cuadros' % g
            t0_ = time.time(); lst = anim(at, frames, k)
            npz = sum(len(x) for x in lst)
            print('  %s/%s escena %06X: %d cuadros, %d piezas, %d rect. de atlas (%.1f s)' % (
                nombre, g, k, len(frames), npz, len(at.reg), time.time() - t0_), flush=True)
            assert npz <= MAX_PIEZAS, '%s: demasiadas piezas' % g
            if rec: lst, frames = recorta(lst, frames)
            datos.append((g, k, frames, capa, xy, tr, lst))
        if extra: extra(at, datos)
        claves = [at._clave(p.get('bk', B), p) for d in datos for ps in d[6] for p in ps]
        t0_ = time.time(); pl = at.plan(claves)
        print('  %s: atlas %d rect. -> %s (%.1f s)' % (nombre, len(set(claves)), [(W, H) for W, H, _ in pl], time.time() - t0_),
              flush=True)
        t0 = len(m.tex); g0 = len(m.grp); u0 = len(m.lay)
        # empaquetado con claves propias de la pantalla (Atlas.empaqueta usa ('atlas', i) y chocaria entre pantallas)
        at.texs = [(W, H) for W, H, _ in pl]; at.bytes = at.bytes_plan(pl)
        for i, (W, H, pos) in enumerate(pl):
            img = np.zeros((H, W, 4), np.uint8); ti = m.textura(('menu', nombre, i), img)
            for kk, (x, y) in pos.items():
                aa, (L, Tp, R, Bt) = at.reg[kk]; img[y:y + Bt - Tp, x:x + R - L] = aa[Tp:Bt, L:R]; at.donde[kk] = (ti, x - L, y - Tp)
        for g, k, frames, capa, xy, tr, lst in datos:
            gi = m.grupo(g, len(lst))
            for c, p, n in tramos_piezas(lst):
                i0 = len(m.pie); m.piezas_escena(gi, c, p.get('bk', B), p.get('tx', T), [p], atlas=at)
                if len(lst) > 1 and len(m.pie) > i0:          # la pieza vale en los cuadros [c, c + n)
                    q = list(m.pie[-1]); q[3] |= n << 4; m.pie[-1] = tuple(q)
            ui = len(m.lay); m.uso(g, gi, xy[0], xy[1], capa=capa)
            for n, i0, i1, modo in tr: m.tramos.append((n, ui, i0, i1, modo))
            hojas.setdefault(nombre, []).append(gi)
        m.pant.append(dict(nombre=nombre, g0=g0, ng=len(m.grp) - g0, u0=u0, nu=len(m.lay) - u0, t0=t0,
                           nt=len(m.tex) - t0, vram=at.bytes, texs=at.texs))
        return at

    R = lambda f0, f1: range(f0, f1 + 1)
    U, BU, Q = 0, 1, 2                                          # modos de tramo: una vez, bucle, quieto

    # TITULO: fondo blanco (textura 8x8 escalada), logo de la pelicula y START parpadeante
    logo, (lx, ly) = logo_titulo(D)
    t0 = len(m.tex); g0 = len(m.grp); u0 = len(m.lay)
    ti = m.textura(('menu', 'TITULO', 'blanco'), np.full((8, 8, 4), 255, np.uint8))
    g = m.grupo('TITULO_FONDO'); m.pie.append((g, 0, m.sprite(ti, (0, 0, 8, 8), 8, 8), 0, 0, 0, 0, 0, 0.0, 80.0, 60.0, 1.0))
    m.uso('TITULO_FONDO', g, 0, 0, capa=0)
    ti = m.textura(('menu', 'TITULO', 'logo'), logo); h, w = logo.shape[:2]
    g = m.grupo('TITULO_LOGO'); m.pie.append((g, 0, m.sprite(ti, (0, 0, w, h), w, h), 0, lx, ly, 0, 0, 0.0, 1.0, 1.0, 1.0))
    m.uso('TITULO_LOGO', g, 0, 0, capa=1)
    t, l, tp, r, b = imagen_xst(xst, 1480)                      # boton START (textura 354)
    sub = T[t][int(tp):int(b), int(l):int(r)].copy(); ti = m.textura(('menu', 'TITULO', 'start'), sub)
    g = m.grupo('TITULO_START')
    m.pie.append((g, 0, m.sprite(ti, (0, 0, r - l, b - tp), int(r - l), int(b - tp)), 0, 320, 400, (r - l) / 2, (b - tp) / 2,
                  0.0, 2.0, 2.0, 1.0))
    m.uso('TITULO_START', g, 0, 0, capa=2)
    m.tramos.append(('PARPADEO', len(m.lay) - 1, 0, 0, Q))
    vr = sum(max(8, 1 << (x.shape[1] - 1).bit_length()) * max(8, 1 << (x.shape[0] - 1).bit_length()) * 2
             for x in m.tex[t0:])
    m.pant.append(dict(nombre='TITULO', g0=g0, ng=len(m.grp) - g0, u0=u0, nu=len(m.lay) - u0, t0=t0, nt=len(m.tex) - t0,
                       vram=vr, texs=[x.shape[1::-1] for x in m.tex[t0:]]))
    hojas['TITULO'] = list(range(g0, len(m.grp)))

    # COMUN: fondo del frontal y barras de ayuda de cada pantalla (lo comparten todas las pantallas de menu)
    ayudas = [dict(g='AYUDA_' + n, lst=[barra_ayuda(D, F, cad, iconos, h)], capa=8, xy=(0, 0), tr=[])
              for n, h in AYUDA.items()]
    # fuente 14 para los textos que se escriben en la consola (nombre de la licencia, opciones...): un cuadro por
    # caracter (codigo - primero) con el glifo ya desplazado -metrica[0]; metricas e interletraje van en MNU2
    usados = set(TECLADO.encode())
    for i in TEXTOS: usados |= set(cad[i])
    glifos = [F.piezas(bytes([F.pri + k]), 0, 0, 1) if (F.pri + k) in usados else [] for k in range(F.n)]
    ayudas.append(dict(g='FUENTE', lst=glifos, capa=9, xy=(0, 0), tr=[]))
    pantalla('COMUN', [('FONDO', 0x4400ab, [0], 0, (0, 0), [], False)] + ayudas)
    m.mnu2 = mnu2(D, F, cad)

    def cab(nombre, tok):
        # 0-40 entra; 41-132 destello sobre el rotulo; 133-199 quieta (el original la pone en bucle: 0x23D5F0 la
        # crea sin rango y en modo 0); se guardan 0..132 y el tramo BRILLO (40-199) repite el destello
        return (nombre, tok, R(0, 199), 5, (0, 0), [('ENTRA', 0, 40, U), ('QUIETA', 40, 40, Q), ('BRILLO', 40, 199, BU)],
                True)
    # MENU PRINCIPAL: SINGLE PLAYER, MULTIPLAYER, OUTRUN2SP, RANKINGS, OPTIONS
    sel = lambda n, paso, ini: [('ENTRA', 0, ini, U)] + [('SEL%d' % i, ini + paso * i, ini + paso * i, Q) for i in range(n)] + \
        [('MUEVE%d%d' % (i, i + 1), ini + paso * i, ini + paso * (i + 1), U) for i in range(n - 1)]
    pantalla('MENU', [cab('CABECERA_MENU', 0x440070),
                      ('MODO_PRINCIPAL', 0x440000, R(0, 132), 2, (0, 0), sel(5, 20, 20), True)])
    # UN JUGADOR: COAST 2 COAST, OUTRUN, HEART ATTACK, TIME ATTACK, SHOWROOM
    pantalla('MODO', [cab('CABECERA_MODO', 0x4400c4),
                      ('MODO_UN_JUGADOR', 0x440029, R(0, 110), 2, (0, 0), sel(5, 20, 20), True)])
    # COCHE: carruseles del original (0x2445D8): 0x4400E5 los 15 normales y 0x4400E6 los 15 "_t" (boton de variante),
    # ficha k quieta en 20(k+1) y movimiento de 20k a 20(k+1); orden del menu = tabla COCHE_MODELOS. El coche 3D lo
    # dibuja el juego. Candado 0x440066 cuadro 5 en (-275, -24) (el 0x440047 del PC, 4C9290)
    # los movimientos van a 30 cuadros por segundo (el cuadro impar repite el par): la mitad de piezas en la RAM
    PARES = [f - (f & 1) for f in range(301)]
    fichas = lambda: [('COCHE%d' % k, 20 * (k + 1), 20 * (k + 1), Q) for k in range(15)] + \
        [('MUEVE%d%d' % (k, k + 1), 20 * (k + 1), 20 * (k + 2), U) for k in range(14)] + [('ENTRA', 0, 20, U)]
    pantalla('COCHE', [cab('CABECERA_COCHE', 0x440072),
                       ('COCHE_INFO', 0x4400e5, PARES, 4, (0, 0), fichas(), False),
                       ('COCHE_INFO_T', 0x4400e6, PARES, 4, (0, 0), fichas(), False),
                       ('COCHE_CANDADO', 0x440066, [5], 5, (-275, -24), [], False)])
    # CAMBIO AT / MT (un grupo; los usos CAMBIO_AT y CAMBIO_MT comparten grupo y se distinguen por tramo)
    pantalla('CAMBIO', [('CAMBIO', 0x440064, R(0, 168), 11, (0, 0),
                         [('ENTRA', 0, 25, U), ('AT', 25, 45, BU), ('MT', 95, 115, BU), ('AT_OK', 46, 70, U),
                          ('MT_OK', 116, 138, U), ('AT_ATRAS', 75, 90, U), ('MT_ATRAS', 145, 160, U)], False)])
    gc = next(i for i, x in enumerate(m.grp) if x['nombre'] == 'CAMBIO')
    for n in ('CAMBIO_AT', 'CAMBIO_MT'):
        m.uso(n, gc, 0, 0, capa=11); m.pant[-1]['nu'] += 1
    # MUSICA (4C9790 / 4C9C20 del PC = tabla del PS2 0x419298): lo comun (cabecera, radio, ayuda, nombre de la lista y
    # candado) y una pantalla por lista (MUSICA0/1/2: carrusel y titulos), para que en la VRAM solo este la lista que
    # se ve (las tres juntas pasan de 1,8 MB). Listas 0x4400C6 / 0x4400C7 / 0x440033 (9 / 7 / 12 temas: entra 0-20,
    # tema k quieto en 20+40k, MUEVEij entre ellos) y titulos 0x440030 / 0x44002F / 0x44002E (110 cuadros por tema, en
    # bucle); nombre de la lista 0x4400C8 (cuadro del grupo = lista: 1, 2, 0 del original, 5C9F18 del PC)
    tr_lista = lambda n: [('ENTRA', 0, 20, U)] + [('TEMA%d' % k, 20 + 40 * k, 20 + 40 * k, Q) for k in range(n)] + \
        [('MUEVE%d%d' % (k, k + 1), 20 + 40 * k, 60 + 40 * k, U) for k in range(n - 1)]
    pantalla('MUSICA', [cab('CABECERA_MUSICA', 0x44006e),          # "music select" (0x23D4B8: clave 4 -> 0x44006E)
                        ('MUSICA_RADIO', 0x4400a7, R(0, 25), 5, (0, 0), [('ENTRA', 0, 25, U), ('SALE', 25, 0, U)], False),
                        ('MUSICA_AYUDA', 0x4400a6, R(0, 35), 6, (0, 0), [('ENTRA', 0, 35, U)], False),
                        ('MUSICA_LISTA', 0x4400c8, [1, 2, 0], 6, (0, 0), [], False),
                        ('MUSICA_CANDADO', 0x440066, [5], 6, (0, 0), [], False)])
    for l, (tl, tt, nt) in enumerate(MUSICA_LISTAS):
        pantalla('MUSICA%d' % l, [('MUSICA_L%d' % l, tl, R(0, 20 + 40 * (nt - 1)), 3, (0, 0), tr_lista(nt), False),
                                  ('MUSICA_T%d' % l, tt, R(0, 110 * nt - 1), 6, (0, 0),
                                   [('TEMA%d' % k, 110 * k, 110 * k + 109, BU) for k in range(nt)], False)])
    # LICENCIA (elegir, 0x4E2970 del PC con la tabla 0x41F8E0; editar, 4DD7C0 con 0x416EA8): cabecera "license
    # select", tarjeta 0x440088 (entra 0-12, quieta hasta 24), flechas 0x44008D / 0x44008E (1 quieta, 2-22 al pulsar)
    # a +-200; editor: rayas y rotulos 0x440087 (entra 0-51, opcion i en 51+i, sale 51-0), tarjeta 0x44009F (igual, en
    # -8), rejillas 0x440091 (6 fotos), 0x44008C (26 banderas), 0x440089 (12 signos), cuadro = elegido + 1; foto,
    # bandera y signo pequenos de la tarjeta 0x440086 / 0x44008B / 0x44008A (cuadro = valor) en (-199,-8), (-216,44),
    # (-182,44); DONE 0x440090 (1, 2 resaltado) en (-84, 110)
    pantalla('LICENCIA', [cab('CABECERA_LICENCIA', 0x440069),
                          ('LIC_TARJETA', 0x440088, R(0, 24), 4, (77, 0), [('ENTRA', 0, 24, U)], True),
                          ('LIC_FLECHA_D', 0x44008d, R(1, 22), 5, (200, 0), [('QUIETA', 0, 0, Q), ('PULSA', 1, 21, U)], False),
                          ('LIC_FLECHA_I', 0x44008e, R(1, 22), 5, (-200, 0), [('QUIETA', 0, 0, Q), ('PULSA', 1, 21, U)], False),
                          ('LIC_EDITOR', 0x440087, R(0, 55), 3, (0, 0),
                           [('ENTRA', 0, 51, U), ('SALE', 51, 0, U)] + [('OPC%d' % i, 51 + i, 51 + i, Q) for i in range(5)], False),
                          ('LIC_TARJETA_E', 0x44009f, R(0, 55), 4, (-8, 0),
                           [('ENTRA', 0, 51, U), ('SALE', 51, 0, U)] + [('OPC%d' % i, 51 + i, 51 + i, Q) for i in range(5)], False),
                          ('LIC_REJ_FOTO', 0x440091, R(1, 6), 5, (0, 0), [], False),
                          ('LIC_REJ_BANDERA', 0x44008c, R(1, 26), 5, (0, 0), [], False),
                          ('LIC_REJ_SIGNO', 0x440089, R(1, 12), 5, (0, 0), [], False),
                          ('LIC_HECHO', 0x440090, R(1, 2), 5, (-84, 110), [], False),
                          ('LIC_FOTO', 0x440086, R(0, 5), 5, (-199, -8), [], False),
                          ('LIC_BANDERA', 0x44008b, R(0, 25), 5, (-216, 44), [], False),
                          ('LIC_SIGNO', 0x44008a, R(0, 11), 5, (-182, 44), [], False),
                          ('LIC_BARRA', 0x44000b, R(0, 11), 5, (0, 0), [('ENTRA', 0, 11, U)], False)])
    # carruseles de los otros menus (tablas del ELF): RANKINGS 0x4400A3 (0x41D908: un jugador, OUTRUN2SP,
    # multijugador) y MULTIJUGADOR 0x44002A (0x41C260: online, LAN); cabeceras "rankings" 0x440074, "multiplayer"
    # 0x44006F y "options" 0x44006D (tabla de 0x23D4B8)
    def carr(nombre, va, n):
        tok, (e0, e1), par = carrusel(D, va, n)
        tr = [('ENTRA', e0, e1, U)] + [('SEL%d' % i, f, f, Q) for i, f in enumerate(par)] + \
             [('MUEVE%d%d' % (i, i + 1), par[i], par[i + 1], U) for i in range(len(par) - 1)]
        return (nombre, tok, R(0, par[-1]), 2, (0, 0), tr, True)
    pantalla('RANKING', [cab('CABECERA_RANKING', 0x440074), carr('MODO_RANKING', 0x41d908, 7)])
    pantalla('MULTI', [cab('CABECERA_MULTI', 0x44006f), carr('MODO_MULTI', 0x41c260, 5)])
    pantalla('OPCIONES', [cab('CABECERA_OPCIONES', 0x44006d),
                          ('OPC_BARRA', 0x44000b, R(0, 11), 4, (0, 0), [('ENTRA', 0, 11, U)], False)])
    # PAUSA (menu de pausa de la carrera, el 4D5E40 / 4D7300 del PC con la raiz 3 = PS2 0x247b40 con la raiz 16): banco
    # 0x2C (ETC_CVT, XST en espanol) con la tabla de filas del ELF 0x4187F8 (PC 0x5CBF10): {token, primero, ultimo}.
    # Filas de la pausa 8, 7, 6, 10, 11, 14 (VOLVER A LA PARTIDA, REINTENTAR, ABANDONAR, CONFIGURACION, CONTROLES,
    # AUDIO: escena 0x2C00EE, cuadros 9, 8, 7, 0, 1, 4); barra de cada fila: entrada 1 (normal, 0x2C00ED cuadro 10) y 2
    # (elegida, cuadro 12); AUDIO (PS2 0x2492a0 = 4D78C0): filas 26 y 27 (0x2C00FB cuadros 12 y 13: VOLUMEN DE SONIDO y
    # VOLUMEN DE MUSICA) y el medidor de cada una (tabla 0x418988: 0x2C00EF cuadro = valor 0..10). Todo en la lista
    # (-13, -72) del centro, 16 pixeles por fila (FrontendChoiceList +0x20)
    t = tabla(D, 0x4187f8, 33)
    assert [t[i] for i in (8, 7, 6, 10, 11, 14)] == [(0x2c00ee, f, f) for f in (9, 8, 7, 0, 1, 4)], 'filas de la pausa'
    assert t[1] == (0x2c00ed, 10, 10) and t[2] == (0x2c00ed, 12, 12), 'barra de la lista'
    assert t[26] == (0x2c00fb, 12, 12) and t[27] == (0x2c00fb, 13, 13), 'filas de audio'
    assert D.elf32(0x418988, 33)[0::3] == tuple([0x2c00ef] * 11), 'medidor de volumen'
    usados_p = set(b"AB<>")                                # letras de los botones (A ELEGIR, B ATRAS) y flechas
    for i in TEXTOS_PAUSA: usados_p |= set(cad[i])
    glifos_p = [F.piezas(bytes([F.pri + k]), 0, 0, 1) if (F.pri + k) in usados_p else [] for k in range(F.n)]
    pantalla('PAUSA', [('PAUSA_FILA', 0x2c00ee, R(0, 14), 10, (-13, -72), [], False),
                       ('PAUSA_BARRA', 0x2c00ed, [10, 12], 9, (-13, -72), [], False),
                       ('PAUSA_OPC', 0x2c00fb, [12, 13], 10, (-13, -72), [], False),
                       ('PAUSA_VOL', 0x2c00ef, R(0, 10), 10, (-13, -72), [], False),
                       dict(g='FUENTE_PAUSA', lst=glifos_p, capa=11, xy=(0, 0), tr=[])])
    os.makedirs(salida, exist_ok=True)
    texs = escribe(m, os.path.join(salida, 'MENU.DAT'))
    maquetas(m, previas, hojas)
    return m, texs

# textos de TEXT.PS2 que escribe la consola (ids del PS2): opciones (vista 3-5, unidades 6-7, dificultad 8-0xC), CREATE NEW
# LICENSE y el nombre por defecto 0x219 (4E2970 / 4E1CF0), menu de una licencia (Select / Reset / Edit License),
# datos de la tarjeta, si / no, NORMAL / TUNED AT / MT y el aviso de coche no disponible
TEXTOS = [3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0x2a, 0x2b, 0x218, 0x219, 0x233, 0x234, 0x237, 0x213, 0x214, 0x21b, 0x21d,
          0x2b1, 0x2b2, 0x2b3, 0x2b4, 0x32a, 0x327]
TEXTOS_PAUSA = [0x3a6, 0x294, 0x295]   # pausa: "Estas seguro?" (PS2 0x249d40, sin partida en red) y ELEGIR / ATRAS
TEXTOS += TEXTOS_PAUSA
TECLADO = '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ:%<>-'   # teclado de 468D40 (mayusculas) y signos de las cifras

def mnu2(D, F, cad):
    """bloque MNU2: textos, metricas de la fuente y tablas de la seleccion de coche (todo del disco)
      'MNU2', u32 n textos, u32 desp. textos, u32 desp. fuente, u32 desp. coches, u32 color de la tarjeta (RGB), 2 x 0
      textos: n x {u16 id, u16 largo, u32 desp. desde MNU2} y las cadenas (codigos de la fuente)
      fuente: u16 ancho, alto, primer caracter, n; i16 espaciado, u16 hay interletraje; i8 metricas[2n]; i8 inter[n*n]
      coches: u8 modelo[30], i16 desbloqueo[30], i16 colores[30][8], i8 color inicial[30]"""
    out = bytearray(32); tab = bytearray(); dat = bytearray()
    base = 32 + 8 * len(TEXTOS)
    for i in TEXTOS:
        tab += struct.pack('<2HI', i, len(cad[i]), base + len(dat)); dat += cad[i] + b'\0'
    out += tab + dat; out += bytes((-len(out)) % 4)
    o_f = len(out)
    out += struct.pack('<4HhH', F.w, F.h, F.pri, F.n, F.esp, F.ker is not None)
    out += F.met.astype(np.int8).tobytes()
    if F.ker is not None: out += F.ker.astype(np.int8).tobytes()
    out += bytes((-len(out)) % 4)
    o_c = len(out)
    rd = lambda va, fmt, n: struct.unpack_from('<%d%s' % (n, fmt), D.elf, va - 0x100000 + 0x80)
    out += bytes(rd(COCHE_MODELOS, 'I', 30))
    out += struct.pack('<30h', *rd(COCHE_DESBLOQUEO, 'h', 30)) + struct.pack('<240h', *rd(COCHE_COLORES, 'h', 240))
    out += struct.pack('<30b', *rd(COCHE_COLOR_INI, 'b', 30)); out += bytes((-len(out)) % 4)
    struct.pack_into('<4sIII', out, 0, b'MNU2', len(TEXTOS), 32, o_f); struct.pack_into('<I', out, 16, o_c)
    struct.pack_into('<I', out, 20, color_tarjeta(D))
    return bytes(out)

def color_tarjeta(D):
    """color de los rotulos de la tarjeta de licencia (0x44009F, cuadro 51): mediana de sus pixeles oscuros opacos;
    con el se escriben los datos de la tarjeta"""
    a, T, _ = banco_menu(D, B); px = []
    for p in escena(a, 0x9f, 51.0):
        l, t, r, b = (int(v) for v in p['src']); c = T[p['tex']][t:b, l:r].reshape(-1, 4).astype(int)
        px.append(c[(c[:, 3] > 200) & (c[:, :3].max(1) < 110)][:, :3])
    m = np.median(np.concatenate(px), axis=0).astype(int)
    return int(m[0]) << 16 | int(m[1]) << 8 | int(m[2])

VQ_MIN = 128 * 128          # texturas menores: sin VQ (el libro de 2 KB pesa demasiado)
VQ_UMBRAL = (9.0, 6.5)      # error RMS maximo (0-255) para aceptar la VQ: opacas, con transparencia

def vq_menu(i, a, fmt, lista):
    """datos VQ de la textura (libro de 256 entradas) o None si no conviene. Usa tools/vq.py (el codificador del
    port); sin el o con VQ=0 en el entorno todo va en 16 bits"""
    if os.environ.get('VQ', '1') == '0' or a.shape[0] * a.shape[1] < VQ_MIN: return None
    try: import vq
    except ImportError: return None
    datos, err = vq.codifica(a, fmt, 256)
    ok = err <= VQ_UMBRAL[lista != 0]
    print('    textura %d %dx%d lista %d: VQ error %.1f %s' % (i, a.shape[1], a.shape[0], lista, err, 'si' if ok else 'no'),
          flush=True)
    if ok and os.environ.get('VQ_PNG'):
        os.makedirs(os.environ['VQ_PNG'], exist_ok=True)
        vq.compara(a, datos, fmt | 0xff0000, os.path.join(os.environ['VQ_PNG'], 'menu_tex%02d.png' % i))
    return datos if ok else None

def escribe(m, ruta):
    """Hud.escribe + cabecera MNU1 detras de los tacometros"""
    texs = Hud.escribe(m, ruta)
    for p in m.pant:                                        # VRAM real de cada pantalla (con la VQ)
        p['vram'] = sum(len(x[4]) for x in texs[p['t0']:p['t0'] + p['nt']])
    d = bytearray(open(ruta, 'rb').read())
    c = struct.unpack_from('<12I', d, 0); off_taco = c[11]; o_dat = off_taco + 16
    o2 = 16 + 48 * len(m.pant) + 32 * len(m.tramos)
    blq = bytearray(b'MNU1' + struct.pack('<3I', len(m.pant), len(m.tramos), o2 if getattr(m, 'mnu2', None) else 0))
    for p in m.pant:
        blq += struct.pack('<16s6HI4I', p['nombre'].encode(), p['g0'], p['ng'], p['u0'], p['nu'], p['t0'], p['nt'],
                           p['vram'], 0, 0, 0, 0)
    for n, u, i0, i1, modo in m.tramos:
        blq += struct.pack('<16s4H2I', n.encode(), u, i0, i1, modo, 0, 0)
    if getattr(m, 'mnu2', None): blq += m.mnu2
    # el bloque va entre los tacometros y los datos de textura: se desplazan los datos y sus desplazamientos
    ini = (o_dat + 31) & ~31
    datos = d[ini:]; nuevo_ini = (o_dat + len(blq) + 31) & ~31; dx = nuevo_ini - ini
    out = d[:o_dat] + blq; out += bytes(nuevo_ini - len(out)); out += datos
    o_tex = c[6]
    for i in range(c[1]):
        o = o_tex + 16 * i + 8; struct.pack_into('<I', out, o, struct.unpack_from('<I', out, o)[0] + dx)
    open(ruta, 'wb').write(out)
    return out

# ---------------------------------------------------------------- maquetas
def maquetas(m, d, hojas):
    from PIL import Image, ImageDraw
    os.makedirs(d, exist_ok=True)
    U = {u[0]: u for u in m.lay}
    def fondo(blanco=False):
        return Image.new('RGBA', (640, 480), (255, 255, 255, 255) if blanco else (54, 62, 74, 255))
    def uso(img, n, c=0):
        u = U[n]; g = u[1]; n_c = m.grp[g]['n']; dibuja_grupo(img, m, g, min(c, n_c - 1), u[3], u[4])
    def tr(n, t):
        u = next(i for i, x in enumerate(m.lay) if x[0] == n)
        return next(x[2] for x in m.tramos if x[1] == u and x[0] == t)
    esc = {
        'TITULO': (True, [('TITULO_FONDO', 0), ('TITULO_LOGO', 0), ('TITULO_START', 0)]),
        'MENU': (False, [('FONDO', 0), ('CABECERA_MENU', 90), ('MODO_PRINCIPAL', tr('MODO_PRINCIPAL', 'SEL0')),
                         ('AYUDA_MENU', 0)]),
        'MODO': (False, [('FONDO', 0), ('CABECERA_MODO', 90), ('MODO_UN_JUGADOR', tr('MODO_UN_JUGADOR', 'SEL1')),
                         ('AYUDA_MODO', 0)]),
        'COCHE': (False, [('FONDO', 0), ('CABECERA_COCHE', 90), ('COCHE_INFO', tr('COCHE_INFO', 'COCHE9')),
                          ('AYUDA_COCHE', 0)]),
        'CAMBIO': (False, [('FONDO', 0), ('CABECERA_COCHE', 90), ('CAMBIO', 40), ('AYUDA_CAMBIO', 0)]),
        'MUSICA': (False, [('FONDO', 0), ('CABECERA_MUSICA', 90), ('MUSICA_RADIO', 25), ('MUSICA_AYUDA', 35), ('MUSICA_L0', 20),
                           ('MUSICA_T0', 55), ('MUSICA_LISTA', 0), ('AYUDA_MUSICA', 0)]),
    }
    for p, (bl, lst) in esc.items():
        img = fondo(bl)
        for n, c in sorted(lst, key=lambda x: U[x[0]][6]): uso(img, n, c)
        img.save(os.path.join(d, 'menu_%s.png' % p.lower()))
    for p, gs in hojas.items():
        filas = []
        for g in gs:
            G = m.grp[g]; nn = G['n']; cs = sorted(set(int(round(i * (nn - 1) / 7)) for i in range(8))) if nn > 1 else [0]
            u = next(x for x in m.lay if x[1] == g)
            fila = Image.new('RGBA', (8 * 240, 196), (0, 0, 0, 255)); dr = ImageDraw.Draw(fila)
            for j, c in enumerate(cs):
                img = fondo(p == 'TITULO'); dibuja_grupo(img, m, g, c, u[3], u[4])
                fila.paste(img.resize((240, 180)), (j * 240, 16)); dr.text((j * 240 + 2, 2), '%s c%d/%d' % (G['nombre'], c, nn),
                                                                        fill=(255, 255, 0, 255))
            filas.append(fila)
        hoja = Image.new('RGBA', (8 * 240, 196 * len(filas)))
        for i, f in enumerate(filas): hoja.paste(f, (0, 196 * i))
        hoja.save(os.path.join(d, 'menu_hoja_%s.png' % p.lower()))

if __name__ == '__main__':
    if len(sys.argv) < 3: sys.exit(__doc__)
    m, out = convierte(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else sys.argv[2])
    print('MENU.DAT: %d bytes, %d texturas, %d sprites, %d grupos, %d piezas, %d usos, %d tramos' % (
        len(out), len(m.tex), len(m.spr), len(m.grp), len(m.pie), len(m.lay), len(m.tramos)))
    for p in m.pant:
        print('  %-7s grupos %2d usos %2d texturas %2d VRAM %4d KB %s' % (p['nombre'], p['ng'], p['nu'], p['nt'],
              p['vram'] // 1024, p['texs']))
