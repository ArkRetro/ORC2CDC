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
     ficha del coche 0x44007E (logo, clase, cambios y barras; coche k quieto en 20(k+1), 15 coches; Testarossa k=8),
     cabeceras rojas 0x440070 (main menu), 0x4400C4 (mode select), 0x440072 (car select), fondo 0x4400AB,
     boton START = imagen 1480 del XST (textura 354).
 - Pantalla de titulo: en el PS2 es la pelicula \\mv\\ts60.pss (MV.PS2, MPEG-2 640x480 a 29,97 fps, 17,7 s) con el logo
   "OutRun2006 Coast 2 Coast" sobre blanco; el texto "Press Start" no existe en TEXT.PS2. Se extrae con ffmpeg un
   cuadro quieto del logo (t = 14 s) y se recorta (TITULO_LOGO, opaca); START parpadeante = boton START del frontal.
 - El coche de la seleccion es el modelo 3D (COCHE.DAT); los textos de la barra de ayuda son de la fuente del juego
   (no se exportan aqui).

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
from conv_hud import Disco, Ani, Atlas, Hud, texturas, escena, imagen_xst, dibuja_grupo, _t, _s

B = 0x44                                # SUMO_FE_CVT
MAX_CUADROS, MAX_PIEZAS = 1000, 12000   # limites por grupo (evitan conversiones desbocadas)

def banco_menu(D, b):
    """(Ani, texturas, xst) del banco b con el indice de SPRITE.PS2 corregido (ver docstring)"""
    nom = D.ani[b - 0x20]; clave = nom[len('ani_SPRANI_'):].lower()
    ia = D.n_ani.index(nom.lower() + '.sz'); lo = D.n_spr
    ix = next(lo.index(c) for c in ('spr_sprani_%s_exst.sz' % clave, 'spr_sprani_%s_xst.sz' % clave) if c in lo)
    if len(D.p_spr) == len(lo) + 1 and ix >= lo.index('spr_sprani_route_cvt_exst.sz'): ix += 1
    xst = D._ent(D.p_spr, ix); a = Ani(D._ent(D.p_ani, ia)); T = texturas(xst)
    assert struct.unpack_from('<I', xst, 12)[0] == len(T) == 380
    return a, T, xst

def tabla(D, va, n):
    """n registros {token, primero, ultimo} del ELF"""
    w = D.elf32(va, 3 * n); return [tuple(w[3 * i:3 * i + 3]) for i in range(n)]

def comprueba_tablas(D):
    """los tokens y cuadros usados salen de estas tablas: si el ejecutable no es el esperado, se para"""
    assert tabla(D, 0x418ca8, 2) == [(0x440000, 0, 20), (0x440000, 20, 20)], 'tabla del menu principal'
    assert tabla(D, 0x41c048, 1)[0] == (0x440029, 0, 20), 'tabla de un jugador'
    t = tabla(D, 0x4209a0, 9)
    assert t[0] == (0x440064, 0, 25) and t[3] == (0x440064, 95, 115) and t[6][1:] == (116, 138), 'tabla del cambio'
    w = D.elf32(0x419298, 9)
    assert w[0] == 0x4400a6 and w[4] == 0x4400a7 and w[7] == 0x440066 and w[8] == 5, 'tabla de la musica'
    assert tabla(D, 0x4195d0, 1)[0] == (0x44002f, 0, 110) and tabla(D, 0x419628, 1)[0] == (0x44002e, 0, 110)

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

def firma(ps):
    return tuple((p['tex'], tuple(round(v, 2) for v in p['src']), tuple(np.round(p['M'][:, :2].ravel(), 2)),
                  round(p['alfa'], 3)) for p in ps if p['alfa'] >= 0.004)

class Menu(Hud):
    def __init__(s):
        Hud.__init__(s); s.taco = [0xff] * 16; s.pant = []; s.tramos = []; s.grupos_ini = {}

def convierte(iso, salida, previas):
    D = Disco(iso); comprueba_tablas(D)
    a, T, xst = banco_menu(D, B)
    m = Menu(); hojas = {}

    def anim(at, frames, k):
        """lista de piezas por cuadro (registradas en el atlas)"""
        out = []
        for f in frames:
            ps = [q for p in escena(a, k, float(f)) if p['alfa'] >= 0.004 and abs(np.linalg.det(p['M'][:2, :2])) >= 1e-6
                  for q in parte(p)]
            for p in ps: at.registra(B, T, p)
            out.append(ps)
        return out

    def recorta(lst, frames):
        """quita los cuadros finales iguales al anterior (se queda uno)"""
        while len(lst) > 1 and firma(lst[-1]) == firma(lst[-2]): lst.pop(); frames = frames[:len(lst)]
        return lst, frames

    def pantalla(nombre, elems, extra=None):
        """elems: (grupo, token, cuadros, capa, (x, y), [(tramo, ini, fin, modo)], recortar). Crea el atlas y los grupos"""
        at = Atlas(); datos = []
        for g, k, frames, capa, xy, tr, rec in elems:
            frames = list(frames)
            assert len(frames) <= MAX_CUADROS, '%s: demasiados cuadros' % g
            t0_ = time.time(); lst = anim(at, frames, k & 0xffff)
            npz = sum(len(x) for x in lst)
            print('  %s/%s escena %06X: %d cuadros, %d piezas, %d rect. de atlas (%.1f s)' % (
                nombre, g, k, len(frames), npz, len(at.reg), time.time() - t0_), flush=True)
            assert npz <= MAX_PIEZAS, '%s: demasiadas piezas' % g
            if rec: lst, frames = recorta(lst, frames)
            datos.append((g, k, frames, capa, xy, tr, lst))
        if extra: extra(at, datos)
        claves = [at._clave(B, p) for d in datos for ps in d[6] for p in ps]
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
            for c, ps in enumerate(lst): m.piezas_escena(gi, c, B, T, ps, atlas=at)
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

    # COMUN: fondo del frontal (lo comparten todas las pantallas de menu)
    pantalla('COMUN', [('FONDO', 0x4400ab, [0], 0, (0, 0), [], False)])

    def cab(nombre, tok):
        return (nombre, tok, R(0, 40), 5, (0, 0), [('ENTRA', 0, 30, U), ('QUIETA', 40, 40, Q)], True)
    # MENU PRINCIPAL: SINGLE PLAYER, MULTIPLAYER, OUTRUN2SP, RANKINGS, OPTIONS
    sel = lambda n, paso, ini: [('ENTRA', 0, ini, U)] + [('SEL%d' % i, ini + paso * i, ini + paso * i, Q) for i in range(n)] + \
        [('MUEVE%d%d' % (i, i + 1), ini + paso * i, ini + paso * (i + 1), U) for i in range(n - 1)]
    pantalla('MENU', [cab('CABECERA_MENU', 0x440070),
                      ('MODO_PRINCIPAL', 0x440000, R(0, 132), 2, (0, 0), sel(5, 20, 20), True)])
    # UN JUGADOR: COAST 2 COAST, OUTRUN, HEART ATTACK, TIME ATTACK, SHOWROOM
    pantalla('MODO', [cab('CABECERA_MODO', 0x4400c4),
                      ('MODO_UN_JUGADOR', 0x440029, R(0, 110), 2, (0, 0), sel(5, 20, 20), True)])
    # COCHE: ficha del coche (15 coches, k quieto en 20(k+1)); el coche 3D lo dibuja el juego
    pantalla('COCHE', [cab('CABECERA_COCHE', 0x440072),
                       ('COCHE_INFO', 0x44007e, R(0, 300), 4, (0, 0),
                        [('COCHE%d' % k, 20 * (k + 1), 20 * (k + 1), Q) for k in range(15)] +
                        [('MUEVE%d%d' % (k, k + 1), 20 * (k + 1), 20 * (k + 2), U) for k in range(14)], False),
                       ('COCHE_CANDADO', 0x440066, [5], 5, (-275, -24), [], False)])
    # CAMBIO AT / MT (un grupo; los usos CAMBIO_AT y CAMBIO_MT comparten grupo y se distinguen por tramo)
    pantalla('CAMBIO', [('CAMBIO', 0x440064, R(0, 168), 11, (0, 0),
                         [('ENTRA', 0, 25, U), ('AT', 25, 45, BU), ('MT', 95, 115, BU), ('AT_OK', 46, 70, U),
                          ('MT_OK', 116, 138, U), ('AT_ATRAS', 75, 90, U), ('MT_ATRAS', 145, 160, U)], False)])
    gc = next(i for i, x in enumerate(m.grp) if x['nombre'] == 'CAMBIO')
    for n in ('CAMBIO_AT', 'CAMBIO_MT'):
        m.uso(n, gc, 0, 0, capa=11); m.pant[-1]['nu'] += 1
    # MUSICA: radio, ayuda, lista 0 completa (la demo tiene sus temas), listas 1 y 2 solo quietas; titulos de la lista 0
    tr_lista = lambda n: [('ENTRA', 0, 20, U)] + [('TEMA%d' % k, 20 + 40 * k, 20 + 40 * k, Q) for k in range(n)] + \
        [('MUEVE%d%d' % (k, k + 1), 20 + 40 * k, 60 + 40 * k, U) for k in range(n - 1)]
    pantalla('MUSICA', [('MUSICA_RADIO', 0x4400a7, R(0, 25), 5, (0, 0), [('ENTRA', 0, 25, U), ('SALE', 25, 0, U)], False),
                        ('MUSICA_AYUDA', 0x4400a6, R(0, 35), 6, (0, 0), [('ENTRA', 0, 35, U)], False),
                        ('MUSICA_L0', 0x4400c6, R(0, 340), 3, (0, 0), tr_lista(9), False),
                        ('MUSICA_T0', 0x440030, R(0, 989), 6, (0, 0),
                         [('TEMA%d' % k, 110 * k, 110 * k + 109, BU) for k in range(9)], False),
                        ('MUSICA_LISTA', 0x4400c8, [1, 2, 0], 6, (0, 0), [], False),
                        ('MUSICA_CANDADO', 0x440066, [5], 6, (0, 0), [], False)])
    # listas 1 y 2 (temas 1986/1989 y remezclas; en la demo, bloqueadas): aparte para no pasar de ~1,2 MB de VRAM
    pantalla('MUSICA_EXTRA', [('MUSICA_L1', 0x4400c7, [20 + 40 * k for k in range(7)], 3, (0, 0), [], False),
                              ('MUSICA_L2', 0x440033, [20 + 40 * k for k in range(12)], 3, (0, 0), [], False),
                              ('MUSICA_T1', 0x44002f, [110 * k + 55 for k in range(7)], 6, (0, 0), [], False),
                              ('MUSICA_T2', 0x44002e, [110 * k + 55 for k in range(12)], 6, (0, 0), [], False)])
    os.makedirs(salida, exist_ok=True)
    texs = escribe(m, os.path.join(salida, 'MENU.DAT'))
    maquetas(m, previas, hojas)
    return m, texs

def escribe(m, ruta):
    """Hud.escribe + cabecera MNU1 detras de los tacometros"""
    Hud.escribe(m, ruta)
    d = bytearray(open(ruta, 'rb').read())
    c = struct.unpack_from('<12I', d, 0); off_taco = c[11]; o_dat = off_taco + 16
    blq = bytearray(b'MNU1' + struct.pack('<3I', len(m.pant), len(m.tramos), 0))
    for p in m.pant:
        blq += struct.pack('<16s6HI4I', p['nombre'].encode(), p['g0'], p['ng'], p['u0'], p['nu'], p['t0'], p['nt'],
                           p['vram'], 0, 0, 0, 0)
    for n, u, i0, i1, modo in m.tramos:
        blq += struct.pack('<16s4H2I', n.encode(), u, i0, i1, modo, 0, 0)
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
        'MENU': (False, [('FONDO', 0), ('CABECERA_MENU', 60), ('MODO_PRINCIPAL', tr('MODO_PRINCIPAL', 'SEL0'))]),
        'MODO': (False, [('FONDO', 0), ('CABECERA_MODO', 60), ('MODO_UN_JUGADOR', tr('MODO_UN_JUGADOR', 'SEL1'))]),
        'COCHE': (False, [('FONDO', 0), ('CABECERA_COCHE', 60), ('COCHE_INFO', tr('COCHE_INFO', 'COCHE8'))]),
        'CAMBIO': (False, [('FONDO', 0), ('CABECERA_COCHE', 60), ('CAMBIO', 40)]),
        'MUSICA': (False, [('FONDO', 0), ('MUSICA_RADIO', 25), ('MUSICA_AYUDA', 35), ('MUSICA_L0', 20),
                           ('MUSICA_T0', 55), ('MUSICA_LISTA', 0)]),
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
