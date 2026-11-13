#!/usr/bin/env python3
"""conv_coche.py: coches del jugador de CARS.PS2 al formato de modelo de la Dreamcast (modelo.py, con VQ).

Uso:
  conv_coche.py <iso> --todos <dir> [--fichas 0,9,..] [--tope 11000] [--vista dir]
      los 15 coches del modo OUTRUN: COCHEnn.DAT (nn = ficha del menu) y COCHES.DAT ('CCH1' y el modelo de cada
      ficha, 0xFF si no esta; lo lee demo/main.c)
  conv_coche.py <iso> --modelo m <salida.dat> [--tope n] [--vista dir]   un modelo (byte car+0x11)

Montaje (todo se lee del disco al convertir; aqui no hay datos del juego): el de 46A560 (el coche en la seleccion; en
carrera 46AE70 / 469600 hacen lo mismo) portado tal cual en llamadas(): pila de matrices del PC (Pila), piezas en su
orden y con su matriz. Comprobado contra un port independiente de vehicle_model_draw.cpp: mismas piezas y matrices en
los 15 coches, en reposo, con freno y con luces.
 - Tabla de disposiciones por modelo del ELF (30 punteros consecutivos a registros de 0x128 bytes; en el PC 5B2F68 ->
   5B0CB8). Se localiza por firma: 30 punteros seguidos con paso 0x128. Cada pieza es un id `recurso << 16 |
   registro del bloque`; el recurso es el indice de la tabla de ficheros del ELF (punteros a cadenas; la 0 es
   \\COMMON\\obj_common_pmt.sz), y de ahi el nombre de la entrada de CARS.PS2. Campos:
     +00 carroceria; +14 pieza extra (o -1: en el F40 una copia entera de la carroceria con la pintura, dibujada antes:
     la tapa la carroceria);
     +18 pieza en +1C (palanca, o el aleron del modelo 5 girado por la velocidad: parado, 0);
     +28 y +38: {pieza, x, y, z} puertas en su bisagra;
     +48 volante en +4C, girado en x +58 (y en z por car+204: 0);
     +64 faros escamoteables en +68 (cerrados: car+2FC = 0); +60 luz de los faros (noche y abiertos);
     +7C pilotos; +80 pilotos con luces (noche o tunel, car+4 & 0x300); +84 con el freno (car+38 != 0);
     +88 + 0x20 k: rueda k {buje (llanta y neumatico), pinza, disco, disco caliente, x, y, z, caida}: pinza sin el giro
     de la rueda, buje y disco con el; las del lado derecho son registros propios (no hay escalas negativas).
   Las luces van todas, cada una con su grupo (CEX2), y la demo enciende las que tocan (coche_luces); igual los dos
   discos (+90 y el caliente +94).
 - Piezas animadas (animadas(), bloque ANI1 detras de SVL1): volante, aleron del modelo 5, faros escamoteables, pinzas
   y ruedas van en su marco (la inversa de su matriz de reposo) con la pieza en el campo del nodo (<< 8); la demo pone
   la matriz de cada cuadro como 46AE70 (demo/main.c coche_anima_matrices). El vaciado mira 6 posturas (poses()).
   +0C la sombra plana: en el PC es un RET, pero SLES-53998 la dibuja (0x114630: car+B0 y 5 cm arriba). +08/+10
   los volumenes de sombra: mallas cerradas que el original extruye por la luz (sombra con plantilla: 0x1138b8 de
   SLES-53998, 422550/421740 del PC); van en el bloque SVL1 (volumenes()). No se dibuja (como en el original con
   vm = 1): +04 la vista interior.
 - Registros: cada pieza es un registro con un solo nodo sin matriz (tabla 0, campo +1C = -1) cuyas tiras son todas
   las del registro (tabla 1: opacas y de mezcla); comprobado en los 360 registros que montan coches y trafico, asi que
   no hay jerarquia ni matrices de la tabla 6 dentro de las piezas. Las tiras se parten donde el original no dibuja
   (bandera ADC del VU1: modelo.py / vif.no_dibuja).
 - Materiales del original (materiales()): tabla 4 del registro (material, textura de la capa 1 y de la capa 2) y
   tabla 5 (difuso, ambiente, especular, potencia, emision; 128 = 1). La pintura es una textura de 8x8 de un color
   cuya clave (recurso << 16 | textura) esta en la lista de colores del modelo (PC 64D120 y 408C80; en SLES-53998 la
   tabla de 30 listas de 0x35ad48, localizada por estructura en tabla_colores): {clave, colores 1..8}, cada color otra
   textura de 8x8 (de \\COMMON\\obj_pc_color_pmt.sz o del propio coche); car+0x12 = color elegido + 1. La carroceria
   lleva en la capa 2 el mapa de entorno esferico del coche; cristales, espejos y cromados no tienen UV y su capa 1 es
   ese mapa. Todo va al bloque CEX2 (extension()) que usa demo/main.c (coche_cex, coche_color, coche_entorno,
   coche_luces).
 - Tope del TA: TOPE vertices dibujados a la vez por coche (11000; ver docs/DREAMCAST.md). Enteros como el original
   dibujan 9320-12080 (el F40 17376, por la copia de la carroceria). Si pasan del tope, fuera lo que no se ve: con
   todas las piezas montadas y en el orden del original se pinta cada triangulo con un z-buffer (tools/c/visible.c; a
   igual profundidad gana el dibujado despues) desde las camaras del juego: la de la seleccion de coche (6 m, 1,4 m de
   alto, vuelta entera), la de carrera (6,5 m detras y 2,4 m arriba, con la deriva del derrape a los lados) y una
   vuelta a 0,9 m (choques; si aun no cabe, sin ella). Los cristales, los recortes y las luces no tapan; las luces no
   se quitan. Se quitan los triangulos que no se ven desde ninguna (bajos, caras de dentro, el lado interior de las
   ruedas, la copia tapada del F40...). Las tiras se cortan donde hay mas de 2 triangulos seguidos ocultos. No se
   tapa, no se anade y no se cambia ninguna pieza: el render desde esas camaras sale igual que el del original entero.
La fisica no cambia: sale de PARAM.DAT por modelo (conv_param.py). Sale de tu disco: no se distribuye."""
import ctypes, os, struct, subprocess, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets
from ps2 import tex, vif
import modelo

NINGUNO = 0xFFFFFFFF
# fichas del menu (comun/menu.c, orden de la escena 0x44007E del frontal) -> modelo (car+0x11). Visto en los logos de
# cada ficha (cuadro 20 (k + 1)) y en los nombres de fichero de la tabla de recursos de cada modelo.
FICHA_MODELO = [7, 14, 2, 9, 8, 12, 5, 0, 10, 6, 11, 3, 13, 1, 4]
TOPE = 11000                                    # vertices por coche (docs/DREAMCAST.md: tope por coche)

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
NADA, G_PILOTOS, G_NOCHE, G_FRENO, G_FARO = 0, 1, 2, 3, 4   # grupos de piezas que la demo enciende y apaga (CEX2)
G_DISCO, G_DISCO_CAL = 5, 6     # disco de freno +90 de la rueda o el caliente +94 (c38 > 200 y car+1F4 != 0)
# piezas animadas (bloque ANI1, campo del nodo de cada malla << 8): sus vertices van en el marco de la pieza (el de la
# ultima operacion de la pila antes del giro animado) y la demo pone la matriz de cada cuadro (coche_anima)
P_CUERPO, P_VOLANTE, P_ALERON, P_FAROS, P_PINZA, P_RUEDA, NPIEZAS = 0, 1, 2, 3, 4, 8, 12   # pinza y rueda: + k (0..3)
P_SOMBRA = 12   # la sombra plana +0C: pieza propia (fuera de car+F0), sin datos en ANI1 (que sigue con NPIEZAS)
# margenes del vaciado con piezas animadas (no son datos del juego: rangos que cubren lo que se ve al moverse):
# direccion +-30 grados (el tope de car+32 en 4A4710: 5461 = 30 grados), volante +-5461 x 6 x 0,41667 (46AE70)
DIR_MAX = 5461 * 2 * np.pi / 65536; VOL_MAX = 5461 * 6 * 2 * np.pi / 65536 * 0.416666657; ALERON_MIN = -0.305432618

class Pila:
    """pila de matrices 4x4 de fila del PC (driving/pc_matrix_stack.cpp): cada operacion multiplica por la izquierda
    (D3DXMatrixMultiply(cur, op, cur)): el punto se transforma p @ op @ cur"""
    def __init__(s): s.m = [np.eye(4)]
    def push(s): s.m.append(s.m[-1].copy())
    def pop(s): s.m.pop()
    def _izq(s, L): s.m[-1] = L @ s.m[-1]
    def translate(s, v): T = np.eye(4); T[3, :3] = v; s._izq(T)
    def rot_x(s, a): c, n = np.cos(a), np.sin(a); s._izq(np.array([[1, 0, 0, 0], [0, c, n, 0], [0, -n, c, 0], [0, 0, 0, 1.0]]))
    def rot_y(s, a): c, n = np.cos(a), np.sin(a); s._izq(np.array([[c, 0, -n, 0], [0, 1, 0, 0], [n, 0, c, 0], [0, 0, 0, 1.0]]))
    def rot_z(s, a): c, n = np.cos(a), np.sin(a); s._izq(np.array([[c, n, 0, 0], [-n, c, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1.0]]))

def llamadas(A, m, pose=None):
    """las piezas que dibuja el original y con que matriz: 46A560 (el coche en la seleccion; 46AE70/469600 en carrera
    hacen lo mismo con vm = 1), en su orden, con el coche parado: matriz local car+F0 = identidad, volante, ruedas y
    aleron del modelo 5 sin girar (car+204, giros de 82E7F0 y car+1C4 a 0), faros escamoteables cerrados (car+2FC = 0),
    ruedas en car+130 = su sitio de la disposicion +98 (lo que usa 469FF0, el dibujo de los finales, sin fisica).
    Las luces van todas, cada una con su grupo; el original elige (vm = 1):
      +60 (G_FARO) si car+4 & 0x100 (noche) y 0x800 (faros abiertos del todo, o sin escamoteables);
      c38 (car+38, pedal de freno) = 0: +80 (G_NOCHE) si car+4 & 0x300 (noche o tunel; 4A25F0), si no +7C (G_PILOTOS);
      c38 != 0: +84 (G_FRENO) y, en el modelo 5, 0x4001a en +1C con el angulo del aleron (G_FRENO).
    +0C (sombra plana bajo el coche): en el PC 49A650 es un RET, pero SLES-53998 la dibuja. En carrera (0x1886e0, el
    dibujo del coche del jugador, el de 469600: 0x189040..0x18920c) y en la seleccion (0x18a138: 0x18a4c4) con 0x114630:
    matriz car+B0 del coche del jugador ([0x557108] + 0xB0: sin car+F0, ni la suspension ni la subida de 4A2910) y
    T(0, 0,05, 0) (0x3d4ccccd), material con alfa 0,5 x f12 que 0x111998 solo usa si [0x4be37c] != 0 (nadie lo
    escribe: alfa 1). Va como pieza P_SOMBRA en su marco (reposo T(0, 0,05, 0)): la demo le pone esa matriz sin car+F0
    (demo/main.c coche_anima_matrices). (Antes iba con car+F0 subida 0,01 como en 0x1893f8 / 0x189904, otro dibujo que
    no es el de carrera: con el balanceo y la subida se metia bajo la calzada.)
    +08/+10 (volumenes de sombra) no son piezas: van aparte (volumenes(), bloque SVL1 de CEX2).
    No va (como en el original con vm = 1): +04 (vista interior, vm = 2). Discos: +90 (G_DISCO) o el caliente +94
    (G_DISCO_CAL) con c38 > 200 y car+1F4 (km/h del modelo) != 0, los dos con su grupo (la demo elige).
    Animadas (pose: dict con 'giro' y 'dir' (4 ruedas), 'volante', 'aleron', 'faros' en radianes; None = parado):
    volante girado en z por car+204, aleron del modelo 5 en x (roof_angle, car+2FC), faros en x (car+2FC), ruedas en
    car+130 con la caida, la direccion en y (pinza) y el giro en x (buje y discos). Las ruedas van fuera de car+F0.
    Devuelve [(id, matriz 4x4, nombre, grupo, pieza)]"""
    E = A.elf; L = disposiciones(A) + 0x128 * m
    u = lambda o: E.u32(L + o); f = lambda o: E.f32(L + o)
    g3 = lambda o: np.array(E.f32(L + o, 3), float)
    s = Pila(); res = []
    q = pose or {}; giro = q.get('giro', [0.0] * 4); dire = q.get('dir', [0.0] * 4)
    def dib(ident, nombre, grupo=NADA, pieza=P_CUERPO):
        if ident != NINGUNO: res.append((ident, s.m[-1].copy(), nombre, grupo, pieza))
    s.push()                                                    # 409FD0: car+F0 (identidad)
    for i in range(2):                                          # puertas en su bisagra
        s.push(); s.translate(g3(0x2c + 0x10 * i)); dib(u(0x28 + 0x10 * i), 'puerta'); s.pop()
    dib(u(0x14), 'extra')
    dib(u(0), 'carroceria')
    s.push(); s.translate(g3(0x4c)); s.rot_x(f(0x58)); s.rot_z(q.get('volante', 0.0)); dib(u(0x48), 'volante', pieza=P_VOLANTE)
    s.pop()
    if u(0x18) != NINGUNO:
        s.push(); s.translate(g3(0x1c))
        if m == 5:                                              # aleron: roof_angle (car+1C4 = 0: 0)
            s.rot_x(q.get('aleron', 0.0)); dib(u(0x18), 'aleron', pieza=P_ALERON)
        else: dib(u(0x18), 'palanca')
        s.pop()
    if u(0x64) != NINGUNO:
        s.push(); s.translate(g3(0x68)); s.rot_x(q.get('faros', 0.0)); dib(u(0x64), 'faros', pieza=P_FAROS); s.pop()
    s.push(); s.translate((0.0, 0.05, 0.0)); dib(u(0xc), 'sombra', pieza=P_SOMBRA); s.pop()   # 0x114630: +0C en car+B0 + 5 cm
    dib(u(0x60), 'faro_luz', G_FARO)
    dib(u(0x7c), 'pilotos', G_PILOTOS); dib(u(0x80), 'pilotos_noche', G_NOCHE); dib(u(0x84), 'pilotos_freno', G_FRENO)
    if m == 5:
        s.push(); s.translate(g3(0x1c)); s.rot_x(q.get('aleron', 0.0)); dib(0x4001a, 'aleron_freno', G_FRENO, P_ALERON)
        s.pop()
    s.pop()
    for k in range(4):                                          # ruedas: pinza (sin el giro de la rueda), buje, disco
        w = 0x8c + 0x20 * k
        s.push(); s.translate(g3(w + 0xc))                      # car+130 (parado: +98)
        if f(w + 0x18) != 0: s.rot_z(f(w + 0x18))              # caida
        if dire[k] != 0: s.rot_y(dire[k])                       # direccion (rueda +32)
        dib(u(w), 'pinza', pieza=P_PINZA + k)
        if giro[k] != 0: s.rot_x(giro[k])                       # giro (rueda +30)
        dib(u(0x88 + 0x20 * k), 'buje', pieza=P_RUEDA + k)
        if u(w + 4) != NINGUNO:
            dib(u(w + 4), 'disco', G_DISCO, P_RUEDA + k); dib(u(w + 8), 'disco_cal', G_DISCO_CAL, P_RUEDA + k)
        s.pop()
    return res

def poses(A, m):
    """matrices de las piezas animadas en las posturas con que se hace el vaciado: giro de las ruedas cada 60 grados,
    direccion y volante a un lado, al centro y al otro, faros abiertos (+78) y aleron abajo en la mitad. Devuelve
    [{pieza: matriz}] (la primera es la de reposo)"""
    L = disposiciones(A) + 0x128 * m; arriba = A.elf.f32(L + 0x78) if A.elf.u32(L + 0x64) != NINGUNO else 0.0
    res = []
    for i in range(6):
        sg = (0, 1, -1)[i % 3]; ab = i % 2
        q = {'giro': [i * np.pi / 3] * 4, 'dir': [sg * DIR_MAX] * 2 + [0.0] * 2, 'volante': sg * VOL_MAX,
             'aleron': ALERON_MIN * ab, 'faros': arriba * ab}
        res.append({p: M for _, M, _, _, p in llamadas(A, m, q) if p != P_CUERPO})
    return res

def pieza(mod, blq, ident, R=None, t=None, nombre='', grupo=NADA, pieza_id=P_CUERPO):
    """anade el registro ident & 0xFFFF del bloque, girado (fila: p @ R) y desplazado; guarda en el extra de cada
    malla la pieza animada y su matriz de reposo ('pieza', 'M')"""
    if ident == NINGUNO: return
    r = ident & 0xFFFF
    if r >= blq.w[0]: return
    k0, m0, t0, b0 = len(mod.verts), len(mod.mallas), len(mod.texs), len(mod.blob)
    mod.bloque(blq, [r], sin_textura='material')
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
    if R is not None and np.linalg.det(R) < 0:                  # simetria: el orden de los vertices da la vuelta
        raise SystemExit('pieza con escala negativa: no la usa ninguna disposicion')
    V = np.concatenate(mod.verts) if mod.verts else None
    for x, ex in zip(mod.mallas[m0:], mod.extra[m0:]):
        if R is not None: ex['N'] = (ex['N'].astype(float) @ R).astype(np.float32)
        ex['grupo'] = grupo; ex['pieza'] = pieza_id
        M = np.eye(4)
        if R is not None: M[:3, :3] = R
        if t is not None: M[3, :3] = t
        ex['M'] = M
        x.append(nombre); x.append(ex)
        q = np.stack([V['x'][x[4]:x[4] + x[5]], V['y'][x[4]:x[4] + x[5]], V['z'][x[4]:x[4] + x[5]]], 1)
        x[2] = q.mean(0); x[3] = float(np.sqrt(((q - x[2]) ** 2).sum(1).max()))

def caja(b, r):
    P = [tr[0x68] for pk in b.mallas(r) for tr in vif.tiras(pk)]
    if not P: return None
    P = np.vstack(P).astype(float); return P.min(0), P.max(0)

def monta(A, m):
    """el coche entero como lo monta el original (llamadas), con todas sus luces"""
    d, recurso, nombre = disposicion(A, m)
    e = A.entrada('CARS.PS2', nombre.split('\\')[-1])
    blq = A.bloque(e)
    mod = modelo.Modelo(); mod.ruta = np.zeros((0, 5)); mod.extra = []; mod._blq_coche = blq
    mod.vq2 = None                  # sin la segunda pasada de la VQ (vq.UMBRAL2): insignias y logotipos de cerca
    for ident, M, nom, grupo, pz in llamadas(A, m):
        b = blq if ident >> 16 == recurso else _bloque_recurso(A, ident >> 16)
        pieza(mod, b, ident, R=M[:3, :3], t=M[3, :3], nombre=nom, grupo=grupo, pieza_id=pz)
    materiales(A, mod, m, recurso)
    return mod, e, d

# ------------------------------------------------------------------ pintura, colores y reflejo
def tabla_colores(A):
    """tabla de listas de colores de los 30 coches del jugador (PC 64D120; en SLES-53998 la de 0x35ad48): 30 punteros
    a listas terminadas en 0 de punteros a entradas {clave, color 1..8} (clave = recurso << 16 | textura). Se localiza
    por estructura: 30 punteros seguidos a listas cuya primera clave es del recurso de la carroceria del modelo"""
    if getattr(A, '_tcol', None): return A._tcol
    E = A.elf; rec = [disposicion(A, m)[1] for m in range(30)]
    def lista(p):
        r = []
        try:
            while E.u32(p) and len(r) < 8: r.append(E.u32(E.u32(p), 9)); p += 4
        except Exception: return None
        return r
    for va, off, fs, ms in E.segmentos:
        w = np.frombuffer(E.d[off:off + fs // 4 * 4], '<u4').astype(np.int64)
        dentro = ((w >= va) & (w < va + fs)).astype(int)
        for i in np.nonzero(np.convolve(dentro, np.ones(30, int), 'valid') == 30)[0]:
            ok = True
            for m in range(30):
                L = lista(int(w[i + m]))
                if not L or any((x[0] >> 16) != rec[m] for x in L): ok = False; break
            if ok: A._tcol = va + 4 * int(i); return A._tcol
    raise SystemExit('no se encuentra la tabla de colores de los coches')

def colores(A, m):
    """[(clave, [ARGB de los colores 1..8])] de la lista del modelo m: cada color es una textura de 8x8 de un solo
    color (la de \\COMMON\\obj_pc_color_pmt.sz o la del propio coche): se guarda su color"""
    E = A.elf; p = E.u32(tabla_colores(A) + 4 * m); R = recursos(A); res = []
    while E.u32(p):
        ent = E.u32(E.u32(p), 9); cols = []
        for alt in ent[1:]:
            b = _bloque_recurso(A, alt >> 16)
            h = tex.cabeceras(b)[alt & 0xFFFF]; a = tex.imagen(b, h).reshape(-1, 4).astype(float).mean(0)
            cols.append(0xFF000000 | int(round(a[0])) << 16 | int(round(a[1])) << 8 | int(round(a[2])))
        res.append((ent[0], cols)); p += 4
    return res

def _bloque_recurso(A, rec):
    cache = A.__dict__.setdefault('_brec', {})
    if rec not in cache:
        nombre = A.elf.cadena(A.elf.u32(recursos(A) + 4 * rec)).split('\\')
        cache[rec] = A.bloque(A.entrada(nombre[-2] + '.PS2', nombre[-1]))
    return cache[rec]

def _luz(N):
    return 0.7 + 0.3 * np.clip(np.abs(N.astype(float) @ modelo.LUZ), 0, 1)       # la misma que modelo.py

def _argb(rgb, luz, a=255):
    c = np.clip(np.asarray(rgb, float)[None, :3] * luz[:, None], 0, 255).astype(np.uint32)
    return (np.uint32(a) << 24) | (c[:, 0] << 16) | (c[:, 1] << 8) | c[:, 2]

def _rgb(argb):
    return np.array([(argb >> 16) & 255, (argb >> 8) & 255, argb & 255], float)

def _tex_copia(mod, ti, translucida):
    """copia de la textura ti del modelo (el mapa de entorno, que tiene alfa 0 fuera del circulo): opaca (RGB565,
    lista 0), para la lista translucida (True: su alfa por el del vertice) o 'suma' (lista translucida, sumada: sin
    alfa)"""
    cache = mod.__dict__.setdefault('_copias', {})
    if (ti, translucida) not in cache:
        a = mod.rgba[ti].copy()
        if translucida is not True:                                     # fuera del circulo (alfa 0): el color de su borde,
            fuera = a[..., 3] < 128                             # para que las UV del borde no cojan otro color
            if fuera.any() and not fuera.all():
                yy, xx = np.nonzero(~fuera); cy, cx = yy.mean(), xx.mean()
                R = 0.97 * np.sqrt(((yy - cy) ** 2 + (xx - cx) ** 2).max())
                fy, fx = np.nonzero(fuera); d = np.maximum(np.hypot(fy - cy, fx - cx), 1e-6)
                sy = np.clip(np.round(cy + (fy - cy) * R / d), 0, a.shape[0] - 1).astype(int)
                sx = np.clip(np.round(cx + (fx - cx) * R / d), 0, a.shape[1] - 1).astype(int)
                a[fy, fx] = a[sy, sx]
            a[..., 3] = 255
        fmt, li, dd = modelo.textura_dc(a)
        if translucida: li = 2
        mod.texs.append((a.shape[1], a.shape[0], fmt, li, len(mod.blob), len(dd))); mod.blob += dd; mod.rgba.append(a)
        cache[(ti, translucida)] = len(mod.texs) - 1
    return cache[(ti, translucida)]

ESP_COLOR, ESP_ENTORNO, ESP_SUMA = 1, 2, 3   # mallas especiales (CEX2): color de pintura; entorno; entorno sumado

def materiales(A, mod, m, recurso):
    """aplica el material del original a cada malla (tabla 4 del registro: material, texturas de las dos capas; tabla
    5: difuso, ambiente, especular, potencia, emision; 128 = 1):
     - pintura (la textura de la capa 1 es una clave de la lista de colores del modelo; textura de 8x8 de un color):
       sin textura, color de la pintura por la luz (se cambia en la demo con el color elegido). Si tiene capa 2 (el
       mapa de entorno esferico del bloque): textura = mapa de entorno (copia opaca) con UV de entorno calculadas en
       cada cuadro por la normal (demo/main.c) y la pintura por la luz como color de desplazamiento (suma): pintura +
       entorno x especular en una sola pasada;
     - mallas sin UV (cristales, espejos, cromados: la capa 1 es el mapa de entorno): UV de entorno; color base =
       especular con el alfa del difuso (translucida si es menor que 1), desplazamiento = difuso por la luz; las de
       bandera 0x800 (cristales) con alfa 1 se suman a lo de detras (SUPUESTO: mezcla aditiva del GS);
     - sin textura: difuso por la luz (y la emision), alfa del difuso;
     - el resto (textura con UV): como estaba (la capa 2, si la hay, no se dibuja).
    Deja en cada malla x[9] el dict extra con 'esp' = (tipo, ranura de pintura o -1, color fijo de desplazamiento)"""
    claves = colores(A, m); mod._colores = claves
    ranura = {c: k for k, (c, _) in enumerate(claves)}
    V = mod.verts; base_tex = mod._base_tex
    for k, x in enumerate(mod.mallas):
        ex = x[9]; ex['esp'] = None
        blq = x[6]; r, i, k4, fl = ex['origen']; reg = blq.registros()[r]
        if not 0 <= k4 < reg[4][0]: continue
        t4 = struct.unpack('<9i', blq.m[blq.A + reg[4][1] + 36 * k4:blq.A + reg[4][1] + 36 * k4 + 36])
        col = np.frombuffer(blq.m[blq.A + reg[5][1] + 64 * t4[0]:blq.A + reg[5][1] + 64 * t4[0] + 64], '<f4') \
            if 0 <= t4[0] < reg[5][0] else np.full(16, 128.0, np.float32)
        dif, esp, emi = col[0:4] * 2, col[8:11] * 2, col[12:15] * 2   # 128 = 1 -> 255
        luz = _luz(ex['N']); v = V[k]; b0 = base_tex.get(id(blq), 0)
        nt = len(tex.cabeceras(blq))
        t1 = t4[3] if 0 <= t4[3] < nt else -1; t2 = t4[5] if 0 <= t4[5] < nt else -1
        rec = recurso if blq is mod._blq_coche else -1
        sl = ranura.get((rec << 16) | t1, -1) if t1 >= 0 else -1
        alfa = int(min(255, dif[3]))
        if sl >= 0:
            pint = _rgb(claves[sl][1][0])                       # color 1: el de serie
            if t2 >= 0 and t2 != t1:
                x[1] = _tex_copia(mod, b0 + t2, False); x[0] = 0
                v['c'] = _argb(np.minimum(esp, 255), np.ones(len(v)))
                ex['esp'] = (ESP_ENTORNO, sl, 0)
            else:
                x[1] = -1; x[0] = 0; v['c'] = _argb(pint * dif[:3] / 255, luz)
                ex['esp'] = (ESP_COLOR, sl, 0)
        elif t1 >= 0 and not ex['uv']:
            suma = bool(fl & 0x800) and alfa >= 250             # cristal opaco de alfa: el reflejo se suma
            transl = alfa < 250
            x[1] = _tex_copia(mod, b0 + t1, 'suma' if suma else transl); x[0] = 2 if transl or suma else 0
            v['c'] = _argb(np.minimum(esp, 255), np.ones(len(v)), alfa if transl else 255)
            fijo = int(_argb(dif[:3], np.ones(1))[0])
            ex['esp'] = (ESP_SUMA if suma else ESP_ENTORNO, -1, fijo)
        elif t1 < 0:
            x[1] = -1; x[0] = 0 if alfa >= 250 else 2
            c = np.clip(dif[None, :3] * luz[:, None] + emi[None, :], 0, 255).astype(np.uint32)
            v['c'] = (np.uint32(alfa) << 24) | (c[:, 0] << 16) | (c[:, 1] << 8) | c[:, 2]

# ------------------------------------------------------------------ vaciado
def _lib():
    import nativo                                             # (en Windows, la visible.dll de windows/construye.sh)
    return nativo.lib('visible')

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

def posa(mod, D):
    """vertices (x, y, z) del coche con las piezas animadas en otra postura: D = {pieza: matriz de la postura}; cada
    malla de una pieza animada pasa de su matriz de reposo (extra 'M') a la de D"""
    V = np.concatenate(mod.verts).copy()
    for x in mod.mallas:
        p = x[9].get('pieza', P_CUERPO)
        if p == P_CUERPO or p not in D: continue
        T = np.linalg.inv(x[9]['M']) @ D[p]
        q = V[x[4]:x[4] + x[5]]; P = np.stack([q['x'], q['y'], q['z']], 1).astype(float) @ T[:3, :3] + T[3, :3]
        V['x'][x[4]:x[4] + x[5]], V['y'][x[4]:x[4] + x[5]], V['z'][x[4]:x[4] + x[5]] = P[:, 0], P[:, 1], P[:, 2]
    return V

def triangulos(mod, V=None):
    V = np.concatenate(mod.verts) if V is None else V; T = []; dueno = []
    for k, x in enumerate(mod.mallas):
        i0, n = x[4], x[5]
        for j in range(n - 2):
            T.append([V['x'][i0 + j:i0 + j + 3], V['y'][i0 + j:i0 + j + 3], V['z'][i0 + j:i0 + j + 3]]); dueno.append(k)
    T = np.array(T, np.float32).transpose(0, 2, 1).reshape(-1, 9)
    return np.ascontiguousarray(T), np.array(dueno)

def visibilidad(mod, res=(960, 720), bajas=True, posturas=None):
    """triangulos que se ven desde alguna camara (los de la lista transparente y los de recorte no tapan; las luces
    que se encienden y apagan (grupos) ni tapan ni se quitan: dos de ellas ocupan el mismo sitio; los dos discos (frio
    y caliente, en el mismo sitio) no tapan y se vacian como el resto).
    Con posturas (poses()): se ve si se ve en alguna de ellas (las piezas animadas destapan lo de detras al moverse)"""
    T, dueno = triangulos(mod)
    grupo = np.array([mod.mallas[k][9].get('grupo', NADA) for k in dueno])
    op = np.array([1 if mod.mallas[k][0] == 0 and grupo[i] == NADA else 0 for i, k in enumerate(dueno)], np.uint8)
    C = camaras(bajas); vis = np.zeros(len(T), bool); lib = _lib()
    f = ctypes.POINTER(ctypes.c_float)
    for D in (posturas or [None]):
        if D is not None: T, _ = triangulos(mod, posa(mod, D))
        cuenta = np.zeros(len(T), np.int32)
        lib.visibles(T.ctypes.data_as(f), len(T), op.ctypes.data_as(ctypes.POINTER(ctypes.c_ubyte)), C.ctypes.data_as(f),
                     len(C), res[0], res[1], cuenta.ctypes.data_as(ctypes.POINTER(ctypes.c_int)))
        vis |= cuenta > 0
    return vis | ((grupo != NADA) & (grupo != G_DISCO) & (grupo != G_DISCO_CAL)), dueno

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
                nuevas.append([x[0], x[1], cc, float(np.sqrt(((P - cc) ** 2).sum(1).max())), nv, len(q), x[6], x[7], x[8],
                               dict(x[9], N=x[9]['N'][j:e + 3])])
                nv += len(q); j = e + 1
    mod.mallas, mod.verts, mod.nv = nuevas, verts, nv

def vertices(mod):
    return sum(x[5] for x in mod.mallas)

def vertices_dibujados(mod):
    """los que se dibujan a la vez: todo lo fijo, el faro encendido, el mayor de los tres juegos de pilotos y el
    mayor de los dos juegos de discos"""
    g = {}
    for x in mod.mallas: k = x[9].get('grupo', NADA) if len(x) > 9 else NADA; g[k] = g.get(k, 0) + x[5]
    return g.get(NADA, 0) + g.get(G_FARO, 0) + max(g.get(G_PILOTOS, 0), g.get(G_NOCHE, 0), g.get(G_FRENO, 0)) + \
        max(g.get(G_DISCO, 0), g.get(G_DISCO_CAL, 0))

def triangulos_n(mod):
    return sum(x[5] - 2 for x in mod.mallas)

# ------------------------------------------------------------------ salida
def rebanadas(P, tris, holgura=0.03, minimo=0.005):
    """trozos convexos de una parte cerrada (vertices P, triangulos tris): rebanadas entre los planos z de sus vertices
    (los coches son mallas por secciones a lo largo de z: cada rebanada es casi convexa) con los puntos donde sus
    aristas cortan esos planos; las vecinas se juntan (la de menos volumen anadido primero) mientras la envolvente de
    las dos no pase de su volumen en mas de holgura (3 %) o minimo (5 litros: las placas finas quedan enteras, como
    antes). DECISION DEL PORT (la PowerVR no cuenta: docs/PARIDAD.md, Sombras y marcas). Devuelve [puntos (n, 3)]"""
    from scipy.spatial import ConvexHull
    def vol(V):
        try: return ConvexHull(V).volume
        except Exception: return 0.0
    def corte(z):
        Q = []
        for t in tris:
            for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
                za, zb = P[a][2] - z, P[b][2] - z
                if za * zb < 0: Q.append(P[a] + (P[b] - P[a]) * (za / (za - zb)))
        return Q
    vs = sorted(set(i for t in tris for i in t))
    zs = sorted(set(np.round(P[vs][:, 2], 4).tolist()))
    tr = []
    for z0, z1 in zip(zs[:-1], zs[1:]):
        V = np.array([P[i] for i in vs if z0 - 1e-5 <= P[i][2] <= z1 + 1e-5] + corte(z0) + corte(z1))
        tr.append([V, vol(V)])
    if not tr: return [P[vs]]
    while len(tr) > 1:
        mejor = None
        for i in range(len(tr) - 1):
            V = np.vstack([tr[i][0], tr[i + 1][0]]); s = tr[i][1] + tr[i + 1][1]; ex = vol(V) - s
            if ex <= max(holgura * s, minimo) and (mejor is None or ex / max(s, 1e-9) < mejor[0]):
                mejor = (ex / max(s, 1e-9), i, V, s)
        if mejor is None: break
        _, i, V, s = mejor; tr[i:i + 2] = [[V, s]]
    return [V for V, _ in tr]

def volumenes(A, m):
    """volumenes de sombra del coche (+08 y +10 de la disposicion; PC 46BA20 -> 420E50 / 4211F0, SLES-53998 0x1138b8
    -> 0x113bf8): mallas cerradas (o placas de dos caras) en el marco del coche, de los triangulos que dibuja el VU1 (sin
    la bandera ADC). El original cuenta en la plantilla (sube con las caras de delante, baja con las de detras: sombra
    si no es 0), asi que le valen mallas no convexas; la PowerVR solo sabe par / impar (una sombra donde la silueta
    extruida se dobla sobre si misma, por ejemplo entre el capo y la cabina, sale par: sin sombra). Por eso cada parte
    conexa de cada volumen va en trozos convexos (rebanadas()): la silueta extruida de un convexo no se dobla, par /
    impar da lo mismo que la cuenta del original y la PowerVR junta los volumenes (sombra si esta dentro de alguno),
    asi que la sombra es la de la union de los trozos. Antes cada parte iba entera como su envolvente convexa: en la
    carroceria la envolvente une por debajo los bultos de las ruedas (que bajan hasta y = 0) y su cara de abajo
    quedaba sobre la calzada: sin sombra bajo el coche y una raya punteada en el suelo por el lado del sol.
    Aristas como 4211F0 / 421680: cada triangulo con su normal de cara (hacia fuera) aporta sus 3 aristas y una arista se
    empareja con la inversa (segunda cara). Devuelve [(posiciones (n, 3), aristas [(a, b, n1, n2 o None)])]"""
    from scipy.spatial import ConvexHull
    L = disposiciones(A) + 0x128 * m; E = A.elf; res = []
    for off in (0x8, 0x10):
        ident = E.u32(L + off)
        if ident == NINGUNO: continue
        b = _bloque_recurso(A, ident >> 16)
        P = []; idx = {}; tris = []
        def pos(p):                                     # el bit 1 de la y es la bandera ADC: fuera al comparar
            k = tuple(np.round(p[:3].astype(np.float64), 4).tolist())
            if k not in idx: idx[k] = len(P); P.append(k)
            return idx[k]
        for pk in b.mallas(ident & 0xFFFF):
            for tr in vif.tiras(pk):
                Q = np.asarray(tr[0x68]); nd = vif.no_dibuja(Q)
                for j in range(len(Q) - 2):
                    if not nd[j + 2]: tris.append([pos(Q[j]), pos(Q[j + 1]), pos(Q[j + 2])])
        P = np.array(P, np.float64)
        padre = list(range(len(P)))                     # partes conexas (vertices compartidos)
        def raiz(i):
            while padre[i] != i: padre[i] = padre[padre[i]]; i = padre[i]
            return i
        for t in tris:
            for k in t[1:]: padre[raiz(k)] = raiz(t[0])
        partes = {}
        for t in tris: partes.setdefault(raiz(t[0]), set()).update(t)
        n0 = len(res)
        for vs in partes.values():
            if len(vs) < 4: continue
            for V in rebanadas(P, [t for t in tris if t[0] in vs]):
              try: H = ConvexHull(V)
              except Exception: continue
              usa = sorted(set(H.simplices.ravel())); nuevo = {v: i for i, v in enumerate(usa)}; aris = []
              for t, eq in zip(H.simplices, H.equations):
                  a, c, d = (nuevo[v] for v in t); n = eq[:3]
                  if np.dot(np.cross(V[t[1]] - V[t[0]], V[t[2]] - V[t[0]]), n) < 0: c, d = d, c   # hacia fuera
                  for x, y in ((a, c), (c, d), (d, a)):
                      for e in aris:                      # 421680: la inversa por posicion
                          if e[0] == y and e[1] == x and e[3] is None: e[3] = n; break
                      else: aris.append([x, y, n, None])
              res.append((V[usa].astype(np.float32), aris))
        print('   volumen +%02X: %d triangulos, %d partes; trozos convexos: %s' % (off, len(tris), len(partes),
              ', '.join('%d vertices %d aristas' % (len(v), len(a)) for v, a in res[n0:])))
    return res

def bloque_svl(vols):
    """SVL1 (detras de los grupos de CEX2): 'SVL1', u32 volumenes (partes convexas); por volumen u32 vertices, u32 aristas, los
    vertices (3 floats) y las aristas {u16 a, u16 b, s8 n1x, n1y, n1z, u8 caras (1 o 2), s8 n2x, n2y, n2z, 0}"""
    b = [b'SVL1', struct.pack('<I', len(vols))]
    for P, aris in vols:
        b.append(struct.pack('<2I', len(P), len(aris))); b.append(P.astype('<f4').tobytes())
        for a, c, n1, n2 in aris:
            q1 = np.clip(np.round(n1 * 127), -127, 127).astype(np.int8)
            q2 = np.clip(np.round((n2 if n2 is not None else np.zeros(3)) * 127), -127, 127).astype(np.int8)
            b.append(struct.pack('<2H3bB3bx', a, c, *q1.tolist(), 1 if n2 is None else 2, *q2.tolist()))
    return b''.join(b)

def registro_modelo(A, m):
    """registro de 4866C0 del modelo m (PC 650500, 0x44 bytes; en SLES-53998 localizado por firma como en
    conv_trafico.py): +08 radio de la rueda, +1C (car+30C = la mitad: balanceo vertical de 4A2910), +20/+24 cabeceo
    al frenar / acelerar y +28/+2C balanceo (4A2910 CheckReverseCar)"""
    import conv_trafico
    E = A.elf; tm = E.va(conv_trafico.tabla_modelos(E.d)); r = tm + 0x44 * m
    return E.f32(r + 8), E.f32(r + 0x1c), E.f32(r + 0x20, 4)

def animadas(A, mod, m):
    """pasa las mallas de las piezas animadas a su marco (la inversa de su matriz de reposo: vertices, normales,
    esfera) y les pone la pieza en el campo del nodo (pieza << 8); devuelve el bloque ANI1 (detras de SVL1 en CEX2):
    'ANI1', u32 palabras del bloque, u32 modelo, f32 radio de rueda (+08 de 4866C0), f32 car+30C (+1C x 0,5),
    f32 +20, +24, +28, +2C de 4866C0 (4A2910), y 12 piezas x 5 f32 {x, y, z, a, b}: 0 carroceria (0), 1 volante
    (+4C, a = giro en x +58), 2 aleron del modelo 5 (+1C), 3 faros escamoteables (+68, a = abiertos +78, b = cuadros
    +74; sin faros, b = 0), 4..7 pinza de la rueda k y 8..11 rueda k (+98 + 0x20 k = car+130 parado, a = caida +A4)"""
    E = A.elf; L = disposiciones(A) + 0x128 * m
    for x in mod.mallas: x[7] = x[9].get('pieza', P_CUERPO) << 8
    # los vertices de cada malla son una entrada de mod.verts (pieza() anade una por trozo de tira; vacia() igual)
    off = 0; trozos = []
    for v in mod.verts: trozos.append((off, v)); off += len(v)
    por_inicio = {o: v for o, v in trozos}
    for x in mod.mallas:
        p = x[9].get('pieza', P_CUERPO)
        if p == P_CUERPO: continue
        v = por_inicio[x[4]]; assert len(v) == x[5]
        Mi = np.linalg.inv(x[9]['M'])
        P = np.stack([v['x'], v['y'], v['z']], 1).astype(float) @ Mi[:3, :3] + Mi[3, :3]
        v['x'], v['y'], v['z'] = P[:, 0], P[:, 1], P[:, 2]
        x[9]['N'] = (x[9]['N'].astype(float) @ Mi[:3, :3]).astype(np.float32)
        x[2] = P.mean(0); x[3] = float(np.sqrt(((P - x[2]) ** 2).sum(1).max()))
    radio, k1c, k20 = registro_modelo(A, m)
    pz = np.zeros((NPIEZAS, 5), np.float32)
    pz[P_VOLANTE, :3] = E.f32(L + 0x4c, 3); pz[P_VOLANTE, 3] = E.f32(L + 0x58)
    pz[P_ALERON, :3] = E.f32(L + 0x1c, 3)
    if E.u32(L + 0x64) != NINGUNO: pz[P_FAROS, :3] = E.f32(L + 0x68, 3); pz[P_FAROS, 3] = E.f32(L + 0x78); pz[P_FAROS, 4] = E.f32(L + 0x74)
    for k in range(4):
        for b in (P_PINZA, P_RUEDA): pz[b + k, :3] = E.f32(L + 0x98 + 0x20 * k, 3); pz[b + k, 3] = E.f32(L + 0xa4 + 0x20 * k)
    cab = struct.pack('<I6f', m, radio, k1c * 0.5, *k20)
    n = (4 + len(cab) + pz.nbytes + 4) // 4
    return b'ANI1' + struct.pack('<I', n) + cab + pz.astype('<f4').tobytes()

def extension(mod):
    """bloque CEX2 (va en el sitio de la ruta del modelo, que los coches no tienen: nruta = palabras, 1 por punto):
    'CEX2', u32 colores por ranura (8), ranuras de pintura, mallas especiales, palabras de vertice, mallas con grupo;
    colores: ranuras x 8 ARGB (colores 1..8 de la lista del original; el 1 es el de serie, el que ya llevan los
    vertices); mallas: {u32 malla (orden del fichero), tipo (1 color de pintura, 2 entorno, 3 entorno sumado), i32
    ranura o -1, ARGB fijo de desplazamiento (si no hay ranura), u32 primera palabra de vertice}; vertices: por vertice
    de cada malla especial una palabra {s8 nx, s8 ny, s8 nz, u8 luz} (normal del modelo x 127, luz x 255); grupos:
    {u32 malla, u32 grupo} de las luces que se encienden y apagan (G_PILOTOS, G_NOCHE, G_FRENO, G_FARO: llamadas)"""
    cols = getattr(mod, '_colores', [])
    recs = []; vd = []; grs = []
    for k, x in enumerate(mod.mallas):
        if x[9].get('grupo', NADA): grs.append((k, x[9]['grupo']))
        esp = x[9].get('esp')
        if not esp: continue
        N = x[9]['N'].astype(float); N = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
        luz = _luz(N)
        q = (np.round(N * 127).astype(np.int8).view(np.uint8).astype(np.uint32))
        w = q[:, 0] | (q[:, 1] << 8) | (q[:, 2] << 16) | (np.round(luz * 255).astype(np.uint32) << 24)
        assert len(w) == x[5]
        recs.append((k, esp[0], esp[1], esp[2], len(vd))); vd.extend(w.tolist())
    pal = [b'CEX2', struct.pack('<5I', 8, len(cols), len(recs), len(vd), len(grs))]
    for _, c in cols: pal.append(struct.pack('<8I', *c))
    for r in recs: pal.append(struct.pack('<IIiII', *r))
    pal.append(struct.pack('<%dI' % len(vd), *vd))
    for g in grs: pal.append(struct.pack('<2I', *g))
    if getattr(mod, '_svol', None): pal.append(bloque_svl(mod._svol))
    if getattr(mod, '_ani', None): pal.append(mod._ani)
    b = b''.join(pal)
    return np.frombuffer(b, '<f4').reshape(-1, 1)

def sombra_clamp(A, mod, m):
    """CLAMP del GS (+18 del estado de la tabla 7) de los materiales de la sombra plana +0C: WMS / WMT = 1 (CLAMP) en
    la textura de sus mallas -> bits 0x1000 (u) / 0x2000 (v) del formato (demo/main.c compila_cab: PVR_UVCLAMP). Sin el,
    el borde de fuera (v = 0, alfa 0) se mezclaba con el centro (repetir: v = 1, alfa ~200): una raya oscura y
    punteada a lo largo del coche, 1,08 m a cada lado. Solo si la textura no la usa otra pieza"""
    L = disposiciones(A) + 0x128 * m; ident = A.elf.u32(L + 0xc)
    if ident == NINGUNO: return
    b = _bloque_recurso(A, ident >> 16); reg = list(b.registros())[ident & 0xFFFF]
    (n4, o4), (n7, o7) = reg[4], reg[7]; ms = mt = True
    for k in range(n4):
        j = struct.unpack('<i', b.m[b.A + o4 + 36 * k + 16:b.A + o4 + 36 * k + 20])[0]
        if not 0 <= j < n7: ms = mt = False; continue
        c = struct.unpack('<Q', b.m[b.A + o7 + 48 * j + 24:b.A + o7 + 48 * j + 32])[0]
        ms &= (c & 3) == 1; mt &= (c >> 2 & 3) == 1
    bits = (0x1000 if ms else 0) | (0x2000 if mt else 0)
    sus = {x[1] for x in mod.mallas if x[9].get('pieza') == P_SOMBRA and x[1] >= 0}
    otras = {x[1] for x in mod.mallas if x[9].get('pieza') != P_SOMBRA}
    for ti in sus - otras:
        w, h, fmt, li, off, tam = mod.texs[ti]; mod.texs[ti] = (w, h, fmt | bits, li, off, tam)
    print('   sombra plana: CLAMP %s%s (texturas %s)' % ('u' if ms else '-', 'v' if mt else '-', sorted(sus - otras)))

def convierte(A, m, sal, tope=TOPE, vista=None):
    """el coche montado como el original (llamadas) y, si lo que se dibuja a la vez pasa del tope del TA, sin lo que
    no se ve desde las camaras del juego (con la vuelta baja de los choques y, si aun no cabe, sin ella). No se tapa,
    no se anade y no se cambia ninguna pieza"""
    mod, e, d = monta(A, m)
    t0, v0 = triangulos_n(mod), vertices_dibujados(mod); nota = 'completo'
    if v0 > tope:
        for baj, nota in ((True, 'sin lo oculto'), (False, 'sin lo oculto ni la vuelta baja')):
            mod, e, d = monta(A, m)
            vis, dueno = visibilidad(mod, bajas=baj, posturas=poses(A, m)); vacia(mod, vis, dueno)
            print("   %s: %d vertices" % (nota, vertices_dibujados(mod)))
            if vertices_dibujados(mod) <= tope: break
    if vista:
        try:
            dibuja_vista(mod, os.path.join(vista, os.path.basename(sal).replace('.DAT', '.png')))
        except Exception as ex: print('vista:', ex)
    mod.recorta_texturas()
    sombra_clamp(A, mod, m)
    mod._svol = volumenes(A, m)
    mod._ani = animadas(A, mod, m)                              # piezas animadas a su marco y bloque ANI1
    mod.mallas.sort(key=lambda x: (x[0], x[1]))                # el orden de escribe (estable): el de CEX2
    mod.ruta = extension(mod)
    vesp = sum(x[5] for x in mod.mallas if x[9].get('esp') and x[9]['esp'][0] >= ESP_ENTORNO)
    vd = vertices_dibujados(mod); vt = vertices(mod)
    mod.mallas = [x[:8] for x in mod.mallas]                    # fuera el nombre de la pieza y lo demas
    mod.escribe(sal)
    kb = len(mod.blob) >> 10
    print('modelo %2d %-28s tri %5d -> %5d  vtx %5d -> %5d (en el fichero %5d)  tex %2d (%3d KB)  entorno %4d vtx  %s'
          '  -> %s' % (m, e.nombre, t0, triangulos_n(mod), v0, vd, vt, len(mod.texs), kb, vesp, nota, os.path.basename(sal)))
    return dict(modelo=m, nombre=e.nombre, tri0=t0, tri=triangulos_n(mod), vtx0=v0, vtx=vd, kb=kb, nota=nota, entorno=vesp)

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
