#!/usr/bin/env python3
"""conv_coche.py: coches del jugador de CARS.PS2 al formato de modelo de la Dreamcast (modelo.py, ORD6 con VQ).

Uso:
  conv_coche.py <iso> --todos <dir> [--fichas 0,9,..] [--tope 9200] [--vista dir]
      los 15 coches del modo OUTRUN: COCHEnn.DAT (nn = ficha del menu) y COCHES.DAT ('CCH1' y el modelo de cada
      ficha, 0xFF si no esta; lo lee demo/main.c)
  conv_coche.py <iso> --modelo m <salida.dat> [--tope n] [--vista dir]   un modelo (byte car+0x11)

Montaje (todo se lee del disco al convertir; aqui no hay datos del juego):
 - Tabla de disposiciones por modelo del ELF (30 punteros consecutivos a registros de 0x128 bytes; en el PC 5B2F68 ->
   5B0CB8, la usan 46AE70 / 46A560 para dibujar el coche en carrera y en la seleccion). Se localiza por firma: 30
   punteros seguidos con paso 0x128. Cada pieza es un id `recurso << 16 | registro del bloque`; el recurso es el
   indice de la tabla de ficheros del ELF (punteros a cadenas; la 0 es \\COMMON\\obj_common_pmt.sz), y de ahi el
   nombre de la entrada de CARS.PS2. Campos que se usan (desplazamiento en el registro):
     +00 carroceria; +14 pieza extra (o -1);
     +18 pieza en +1C (palanca, o el aleron del modelo 5) sin giro;
     +28 y +38: {pieza, x, y, z} puertas en su bisagra;
     +48 volante en +4C, girado en x +58;
     +64 faros escamoteables en +68 (cerrados: angulo 0), o -1;
     +7C pilotos traseros (sin freno); +80 con freno y +84 tras el choque no se usan;
     +88 + 0x20 k: rueda k {buje (llanta y neumatico), pinza, disco, disco borroso, x, y, z, caida}.
   No se dibujan (como en el original): +04 la vista interior, +08/+10 los volumenes de luz, +0C la sombra (en el PC
   es un RET), +60 (solo con luces).
 - La pintura ya va en las texturas (color por defecto); las mallas sin textura van como en modelo.py.
 - Tope y vaciado: TOPE vertices por coche (9200; ver docs/DREAMCAST.md). Los modelos altos de C2C, montados enteros,
   tienen 9400-11800 vertices (carroceria 6000-8500, ruedas 1500-3000); solo lo que se ve de la carroceria pasa de
   7000 en la mitad de ellos, asi que menos que eso no se alcanza sin rebajar el exterior. Si el coche pasa del tope
   se aplica por orden hasta que cabe:
   1. fuera lo que no se ve: con todas las piezas montadas se pinta cada triangulo con un z-buffer (tools/c/visible.c)
      desde las camaras del juego: la de la seleccion de coche (6 m, 1,4 m de alto, vuelta entera), la de carrera
      (6,5 m detras y 2,4 m arriba, con la deriva del derrape a los lados) y una vuelta a 0,9 m (choques). Los
      cristales y los recortes no tapan. Se quitan los triangulos que no se ven desde ninguna (bajos, caras de dentro,
      el lado interior de las ruedas...) y las astillas (triangulos de union de piezas de una tira, sin anchura);
   2. fuera el interior: un rectangulo oscuro a la altura de la cintura (de los asientos +108/+118 y el volante de
      la disposicion) cierra el habitaculo y tapa asientos, salpicadero y paneles de dentro;
   3. ruedas del modelo ligero (obj_rc_*, unos 90 vertices) en lugar de las del buje (unos 600);
   4. solo las camaras de la seleccion y de carrera.
   Las tiras se cortan donde hay mas de 2 triangulos seguidos ocultos (cortar cuesta 2 vertices).
La fisica no cambia: sale de PARAM.DAT por modelo (conv_param.py). Sale de tu disco: no se distribuye."""
import ctypes, os, subprocess, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets
from ps2 import vif
import modelo

NINGUNO = 0xFFFFFFFF
# fichas del menu (comun/menu.c, orden de la escena 0x44007E del frontal) -> modelo (car+0x11). Visto en los logos de
# cada ficha (cuadro 20 (k + 1)) y en los nombres de fichero de la tabla de recursos de cada modelo.
FICHA_MODELO = [7, 14, 2, 9, 8, 12, 5, 0, 10, 6, 11, 3, 13, 1, 4]
TOPE = 9200                                     # vertices por coche (docs/DREAMCAST.md: tope por coche)

# ------------------------------------------------------------------ ELF: disposiciones y ficheros
def disposiciones(A):
    """direccion del primer registro de la tabla de 30 disposiciones"""
    E = A.elf
    for va, off, fs, ms in E.segmentos:
        w = np.frombuffer(E.d[off:off + fs // 4 * 4], '<u4').astype(np.int64)
        d = np.diff(w)
        ok = np.convolve((d == 0x128).astype(int), np.ones(29, int), 'valid') == 29
        for i in np.nonzero(ok)[0]:
            if va <= w[i] < va + fs: return int(w[i])
    raise SystemExit('no se encuentra la tabla de disposiciones de los coches')

def recursos(A):
    """direccion de la tabla de ficheros por recurso"""
    E = A.elf
    for s in E.busca(b'\\COMMON\\obj_common_pmt.sz\0'):
        r = E.referencias(s)
        if r: return r[0]
    raise SystemExit('no se encuentra la tabla de recursos')

def disposicion(A, m):
    E = A.elf; base = disposiciones(A) + 0x128 * m
    w = E.u32(base, 74); f = E.f32(base, 74)
    def v(o): return w[o // 4]
    def p(o): return np.array(f[o // 4:o // 4 + 3], float)
    d = {'cuerpo': v(0), 'extra': v(0x14), 'palanca': (v(0x18), p(0x1c)),
         'puertas': [(v(0x28), p(0x2c)), (v(0x38), p(0x3c))], 'volante': (v(0x48), p(0x4c), f[0x58 // 4]),
         'faros': (v(0x64), p(0x68)), 'pilotos': v(0x7c),
         'ruedas': [(v(0x88 + 0x20 * k), v(0x8c + 0x20 * k), v(0x90 + 0x20 * k), p(0x98 + 0x20 * k), f[(0xa4 + 0x20 * k) // 4])
                    for k in range(4)],
         'asientos': [p(0x108), p(0x118)]}
    recurso = d['cuerpo'] >> 16
    nombre = E.cadena(E.u32(recursos(A) + 4 * recurso))
    return d, recurso, nombre

# ------------------------------------------------------------------ montaje
def rot_x(a):
    c, s = np.cos(a), np.sin(a); return np.array([[1, 0, 0], [0, c, s], [0, -s, c]])

def rot_z(a):
    c, s = np.cos(a), np.sin(a); return np.array([[c, s, 0], [-s, c, 0], [0, 0, 1]])

def pieza(mod, blq, ident, R=None, t=None, nombre=''):
    """anade el registro ident & 0xFFFF del bloque, girado (fila: p @ R) y desplazado"""
    if ident == NINGUNO: return
    r = ident & 0xFFFF
    if r >= blq.w[0]: return
    k0, m0, t0, b0 = len(mod.verts), len(mod.mallas), len(mod.texs), len(mod.blob)
    mod.bloque(blq, [r], sin_textura=None)
    base = getattr(mod, '_base_tex', {})
    if id(blq) in base:                                         # modelo.bloque vuelve a anadir todas las texturas
        for x in mod.mallas[m0:]:                               # del bloque: se usan las de la primera vez
            if x[1] >= t0: x[1] = x[1] - t0 + base[id(blq)]
        del mod.texs[t0:]; del mod.rgba[t0:]; mod.blob = mod.blob[:b0]
    else:
        base[id(blq)] = t0; mod._base_tex = base
    for v in mod.verts[k0:]:
        P = np.stack([v['x'], v['y'], v['z']], 1).astype(float)
        if R is not None: P = P @ R
        if t is not None: P = P + t
        v['x'], v['y'], v['z'] = P[:, 0], P[:, 1], P[:, 2]
    V = np.concatenate(mod.verts) if mod.verts else None
    for x in mod.mallas[m0:]:
        x.append(nombre)
        q = np.stack([V['x'][x[4]:x[4] + x[5]], V['y'][x[4]:x[4] + x[5]], V['z'][x[4]:x[4] + x[5]]], 1)
        x[2] = q.mean(0); x[3] = float(np.sqrt(((q - x[2]) ** 2).sum(1).max()))

def ruedas_ligeras(A, nombre):
    """bloque del modelo ligero del mismo coche (obj_rc_*) y sus dos ruedas (registros pequenos y redondos alrededor
    del origen): [(registro, radio, centro x)]"""
    e = A.entrada('CARS.PS2', nombre.split('\\')[-1].replace('plcar', 'rc'))
    b = A.bloque(e); r = []
    for k in range(b.w[0]):
        c = caja(b, k)
        if c is None: continue
        lo, hi = c
        if np.abs(lo).max() < 0.5 and np.abs(hi).max() < 0.5 and hi[1] - lo[1] > 0.4: r.append((k, hi[1], (lo[0] + hi[0]) / 2))
    return b, r[:2]

def caja(b, r):
    P = [tr[0x68] for pk in b.mallas(r) for tr in vif.tiras(pk)]
    if not P: return None
    P = np.vstack(P).astype(float); return P.min(0), P.max(0)

def monta(A, m, ligeras=False, tapa=None):
    """el coche entero segun su disposicion. ligeras: ruedas del modelo ligero (menos vertices) en lugar de las del
    buje; tapa: (x0, x1, z0, z1, y) rectangulo oscuro que cierra el habitaculo (el interior se ha quitado)"""
    d, recurso, nombre = disposicion(A, m)
    e = A.entrada('CARS.PS2', nombre.split('\\')[-1])
    blq = A.bloque(e)
    mod = modelo.Modelo(); mod.ruta = np.zeros((0, 5))
    pieza(mod, blq, d['cuerpo'], nombre='carroceria')
    if d['extra'] != NINGUNO and sum(len(tr[0x68]) for pk in blq.mallas(d['extra'] & 0xFFFF) for tr in vif.tiras(pk)) < 2000:
        pieza(mod, blq, d['extra'], nombre='extra')             # (la del F40 es una segunda capa de toda la carroceria)
    pieza(mod, blq, d['palanca'][0], t=d['palanca'][1], nombre='palanca')
    for pz, t in d['puertas']: pieza(mod, blq, pz, t=t, nombre='puerta')
    pz, t, a = d['volante']; pieza(mod, blq, pz, R=rot_x(a), t=t, nombre='volante')
    pz, t = d['faros']; pieza(mod, blq, pz, t=t, nombre='faros')
    pieza(mod, blq, d['pilotos'], nombre='pilotos')
    if ligeras: bl, rl = ruedas_ligeras(A, nombre)
    for k, (buje, pinza, disco, t, caida) in enumerate(d['ruedas']):
        R = rot_z(caida) if caida else np.eye(3)
        if not ligeras:
            pieza(mod, blq, pinza, R=R, t=t, nombre='rueda'); pieza(mod, blq, buje, R=R, t=t, nombre='rueda')
            pieza(mod, blq, disco, R=R, t=t, nombre='rueda')
            continue
        cb = caja(blq, buje & 0xFFFF); rad = cb[1][1] if cb else 0.33
        reg, _, cx = min(rl, key=lambda q: abs(q[1] - rad))     # la de radio mas parecido (delante / detras)
        lado = (cb[0][0] + cb[1][0]) / 2 if cb else -t[0]       # hacia donde mira la llanta del buje
        esp = np.diag([-1.0, 1, 1]) if (lado > 0) != (cx > 0) and abs(cx) > 1e-3 else np.eye(3)
        pieza(mod, bl, reg, R=esp @ R, t=t, nombre='rueda')
    if tapa is not None:
        x0, x1, z0, z1, y = tapa
        v = np.zeros(4, dtype=mod.verts[0].dtype)
        v['x'] = [x0, x1, x0, x1]; v['y'] = y; v['z'] = [z0, z0, z1, z1]; v['c'] = 0xFF101418
        mod.mallas.append([0, -1, np.array([(x0 + x1) / 2, y, (z0 + z1) / 2]), float(np.hypot(x1 - x0, z1 - z0) / 2),
                           mod.nv, 4, None, 0.0, 'tapa'])
        mod.verts.append(v); mod.nv += 4
    return mod, e, d

def tapa_de(mod, d):
    """rectangulo que cierra el habitaculo a la altura de la cintura: de los asientos (disposicion +108/+118) y el
    volante; ancho hasta la cara interior de las puertas"""
    a = np.array(d['asientos']); vol = d['volante'][1]
    xs = sorted([a[0][0], a[1][0]]); y = vol[1] + 0.2
    V = np.concatenate([v for v, x in zip(mod.verts, mod.mallas) if x[8] == 'carroceria'])
    P = np.stack([V['x'], V['y'], V['z']], 1)
    z0, z1 = vol[2] - 0.25, max(a[0][2], a[1][2]) + 0.6
    sel = P[(P[:, 2] > z0) & (P[:, 2] < z1) & (np.abs(P[:, 1] - y) < 0.08)]
    xm = np.percentile(np.abs(sel[:, 0]), 90) if len(sel) else 0.8
    xm = min(xm - 0.04, max(abs(xs[0]), abs(xs[1])) + 0.45)
    return (-xm, xm, z0, z1, y)

# ------------------------------------------------------------------ vaciado
def _lib():
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'c', 'visible.c')
    dst = os.path.join(os.environ.get('OR2_CACHE', os.path.expanduser('~/.cache/or2_assets')), 'visible.so')
    if not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(src):
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        subprocess.check_call(['cc', '-O2', '-shared', '-fPIC', '-o', dst, src, '-lm'])
    return ctypes.CDLL(dst)

def mira(ojo, punto):
    """matriz de fila 4x4 de una camara como la de la demo (640x480, tangente del medio angulo horizontal 0,75):
    (x, y, z, 1) -> (X, Y, Z, W) con X/W e Y/W en -1..1 dentro de la pantalla y W la distancia"""
    f = np.array(punto, float) - ojo; f /= np.linalg.norm(f)
    r = np.cross(f, [0, 1, 0]); r /= np.linalg.norm(r); u = np.cross(r, f)
    M = np.zeros((4, 4))
    M[:3, 0] = r / 0.75; M[:3, 1] = u / 0.5625; M[:3, 2] = f; M[:3, 3] = f
    M[3, 0] = -(ojo @ r) / 0.75; M[3, 1] = -(ojo @ u) / 0.5625; M[3, 2] = M[3, 3] = -(ojo @ f)
    return M.astype(np.float32)

def camaras(bajas=True):
    """las del juego (el morro del coche es -z): seleccion, carrera con deriva y una vuelta baja"""
    C = []
    for k in range(48):                                         # seleccion: 6 m, 1,4 m, mira a 0,2 m
        a = 2 * np.pi * k / 48; C.append(mira(np.array([6 * np.sin(a), 1.4, 6 * np.cos(a)]), [0, 0.2, 0]))
    for a in np.radians(np.arange(-100, 101, 10)):              # carrera: detras (+z) y arriba, con deriva
        for h, d in ((2.4, 6.5), (1.9, 6.5), (2.9, 6.5), (2.4, 4.5)):
            o = np.array([d * np.sin(a), h, d * np.cos(a)]); C.append(mira(o, -o * 0.9 + np.array([0, 1.1 + h * 0.0, 0])))
    for k in range(32 if bajas else 0):                         # vuelta algo mas baja (choques)
        a = 2 * np.pi * k / 32; C.append(mira(np.array([7 * np.sin(a), 0.9, 7 * np.cos(a)]), [0, 0.4, 0]))
    return np.array(C, np.float32)

def triangulos(mod):
    V = np.concatenate(mod.verts); T = []; dueno = []
    for k, x in enumerate(mod.mallas):
        i0, n = x[4], x[5]
        for j in range(n - 2):
            T.append([V['x'][i0 + j:i0 + j + 3], V['y'][i0 + j:i0 + j + 3], V['z'][i0 + j:i0 + j + 3]]); dueno.append(k)
    T = np.array(T, np.float32).transpose(0, 2, 1).reshape(-1, 9)
    return np.ascontiguousarray(T), np.array(dueno)

def visibilidad(mod, res=(960, 720), bajas=True):
    """triangulos que se ven desde alguna camara (los de la lista transparente y los de recorte no tapan)"""
    T, dueno = triangulos(mod)
    op = np.array([0 if mod.mallas[k][0] != 0 else 1 for k in dueno], np.uint8)
    C = camaras(bajas); cuenta = np.zeros(len(T), np.int32); lib = _lib()
    f = ctypes.POINTER(ctypes.c_float)
    lib.visibles(T.ctypes.data_as(f), len(T), op.ctypes.data_as(ctypes.POINTER(ctypes.c_ubyte)), C.ctypes.data_as(f),
                 len(C), res[0], res[1], cuenta.ctypes.data_as(ctypes.POINTER(ctypes.c_int)))
    a = T[:, 0:3]; b = T[:, 3:6]; c = T[:, 6:9]                 # astillas: los triangulos de union de piezas de
    area = np.linalg.norm(np.cross(b - a, c - a), axis=1) / 2   # una tira (casi sin area y largos) no se ven
    largo = np.maximum(np.linalg.norm(b - a, axis=1), np.maximum(np.linalg.norm(c - b, axis=1), np.linalg.norm(a - c, axis=1)))
    astilla = (largo > 0.25) & (area < 0.004 * largo)           # menos de ~8 mm de ancho
    return (cuenta > 0) & ~astilla, dueno

def vacia(mod, vis, dueno, hueco=2, maximo=256):
    """rehace las tiras con solo los triangulos visibles (corta si hay mas de hueco ocultos seguidos; tiras de
    hasta maximo vertices: la demo no dibuja mas de 256 por tira)"""
    V = np.concatenate(mod.verts); nuevas = []; verts = []; nv = 0; ti = 0
    for k, x in enumerate(mod.mallas):
        n = x[5]; v = vis[ti:ti + n - 2]; ti += n - 2
        tramos = []; a = None; oc = 0
        for j in range(n - 2):
            if v[j]:
                if a is None: a = j
                b = j; oc = 0
            elif a is not None:
                oc += 1
                if oc > hueco: tramos.append((a, b)); a = None; oc = 0
        if a is not None: tramos.append((a, b))
        for a, b in tramos:                                       # triangulos a..b: vertices a..b+2
            j = a
            while j <= b:
                e = min(b, j + maximo - 3)
                q = V[x[4] + j:x[4] + e + 3].copy(); verts.append(q)
                P = np.stack([q['x'], q['y'], q['z']], 1).astype(float); cc = P.mean(0)
                nuevas.append([x[0], x[1], cc, float(np.sqrt(((P - cc) ** 2).sum(1).max())), nv, len(q), x[6], x[7]] + x[8:])
                nv += len(q); j = e + 1
    mod.mallas, mod.verts, mod.nv = nuevas, verts, nv

def vertices(mod):
    return sum(x[5] for x in mod.mallas)

def triangulos_n(mod):
    return sum(x[5] - 2 for x in mod.mallas)

# ------------------------------------------------------------------ salida
def convierte(A, m, sal, tope=TOPE, vista=None):
    """escalera hasta caber en el tope: completo; sin lo oculto; + sin el interior (tapa a la altura de la cintura);
    + ruedas del modelo ligero; + sin las camaras bajas (solo seleccion y carrera)"""
    mod, e, d = monta(A, m)
    t0, v0 = triangulos_n(mod), vertices(mod); nota = 'completo'
    if v0 > tope:
        for paso, (lig, tap, baj) in enumerate(((False, False, True), (False, True, True), (True, True, True), (True, True, False))):
            mod, e, d = monta(A, m, ligeras=lig)
            if tap: mod, e, d = monta(A, m, ligeras=lig, tapa=tapa_de(mod, d))
            vis, dueno = visibilidad(mod, bajas=baj); vacia(mod, vis, dueno)
            nota = ('sin lo oculto', '+ sin el interior', '+ ruedas ligeras', '+ sin la vuelta baja')[paso]
            if vertices(mod) <= tope: break
    if vista:
        try:
            dibuja_vista(mod, os.path.join(vista, os.path.basename(sal).replace('.DAT', '.png')))
        except Exception as ex: print('vista:', ex)
    mod.mallas = [x[:8] for x in mod.mallas]                    # fuera el nombre de la pieza
    mod.recorta_texturas()
    mod.escribe(sal)
    kb = len(mod.blob) >> 10
    print('modelo %2d %-28s tri %5d -> %5d  vtx %5d -> %5d  tex %2d (%3d KB)  %s  -> %s' % (
        m, e.nombre, t0, triangulos_n(mod), v0, vertices(mod), len(mod.texs), kb, nota, os.path.basename(sal)))
    return dict(modelo=m, nombre=e.nombre, tri0=t0, tri=triangulos_n(mod), vtx0=v0, vtx=vertices(mod), kb=kb, nota=nota)

def dibuja_vista(mod, ruta):
    """perfil y tres cuartos (comprobacion rapida, sin z-buffer)"""
    from PIL import Image, ImageDraw
    V = np.concatenate(mod.verts); im = Image.new('RGB', (1000, 300), (50, 60, 70)); dr = ImageDraw.Draw(im)
    for vx, (yaw, pitch) in enumerate(((np.pi / 2, 0.0), (np.pi * 0.8, 0.35))):
        cy, sy, cp, sp = np.cos(yaw), np.sin(yaw), np.cos(pitch), np.sin(pitch)
        M = np.array([[1, 0, 0], [0, cp, sp], [0, -sp, cp]]) @ np.array([[cy, 0, -sy], [0, 1, 0], [sy, 0, cy]])
        L = []
        for li, ti, cc, r, i0, n, *_ in mod.mallas:
            p = V[i0:i0 + n]; q = np.stack([p['x'], p['y'] - 0.6, p['z']], 1) @ M.T
            for j in range(n - 2):
                c = int(p['c'][j]); col = ((c >> 16) & 255, (c >> 8) & 255, c & 255)
                if ti >= 0 and ti < len(mod.rgba):
                    t = mod.rgba[ti]; h, w = t.shape[:2]
                    px = t[int((p['v'][j:j + 3].mean() % 1) * h) % h, int((p['u'][j:j + 3].mean() % 1) * w) % w]
                    col = tuple(int(px[k]) * col[k] // 255 for k in range(3))
                L.append((q[j:j + 3, 2].mean(), [(250 + 500 * vx + a * 100, 150 - b * 100) for a, b in q[j:j + 3, :2]], col))
        L.sort(key=lambda x: -x[0])
        for _, pts, col in L: dr.polygon(pts, fill=col)
    im.save(ruta)

def main():
    a = sys.argv[1:]
    def op(n, defecto=None):
        if n in a: i = a.index(n); v = a[i + 1]; del a[i:i + 2]; return v
        return defecto
    tope = int(op('--tope', TOPE)); vista = op('--vista'); todos = op('--todos'); m = op('--modelo')
    solo = op('--fichas'); solo = None if solo is None else [int(x) for x in solo.split(',')]
    A = Assets(a[0] if a else None)
    if todos is not None:
        os.makedirs(todos, exist_ok=True)
        hechos = bytearray(b'\xff' * len(FICHA_MODELO))
        for k, mm in enumerate(FICHA_MODELO):
            if solo is not None and k not in solo: continue
            convierte(A, mm, os.path.join(todos, 'COCHE%02d.DAT' % k), tope, vista)
        for k, mm in enumerate(FICHA_MODELO):
            if os.path.exists(os.path.join(todos, 'COCHE%02d.DAT' % k)): hechos[k] = mm
        with open(os.path.join(todos, 'COCHES.DAT'), 'wb') as f: f.write(b'CCH1' + bytes(hechos))
    elif m is not None:
        convierte(A, int(m), a[1], tope, vista)
    else:
        print(__doc__)

if __name__ == '__main__':
    main()
