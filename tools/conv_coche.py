#!/usr/bin/env python3
"""conv_coche.py: coches del jugador de CARS.PS2 al formato de modelo de la Dreamcast (modelo.py, con VQ).

Uso:
  conv_coche.py <iso> --todos <dir> [--fichas 0,9,..] [--tope 11000] [--vista dir]
      los 15 coches del modo OUTRUN: COCHEnn.DAT (nn = ficha del menu) y COCHES.DAT ('CCH1' y el modelo de cada
      ficha, 0xFF si no esta; lo lee demo/main.c)
  conv_coche.py <iso> --modelo m <salida.dat> [--tope n] [--vista dir]   un modelo (byte car+0x11)
  --rebaja: variante A (texturas que no se ven de cerca con menos resolucion: rebaja_texturas); --carrera: variante B
      (las mallas que no se ven desde la persecucion con el bit NODO_CERCA: parte_carrera). Rev 2.9.6, DESVIACION
  --visible (implica --rebaja; con --todos): ademas COCHEnnL.DAT, el ligero de la carrera (variante C2): sin lo oculto
      siempre, menos triangulos en lo que se ve (simplifica) y las texturas visibles grandes a la mitad (clase
      'visible'); con --modelo, solo el ligero en <salida.dat>. Rev 2.9.7, DESVIACION. Rev 2.9.8 (P3, DESVIACION): en el
      ligero, ademas, lo que no se ve desde el cono trasero en tiras aparte con NODO_TRASERO (parte_trasera; --sin-trasero:
      sin eso, como en la 2.9.7)

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
G_INTERIOR = 7                  # el habitaculo +04 (vista interior, vm = 2): ni tapa ni se vacia ni se cuenta con el resto
# piezas animadas (bloque ANI1, campo del nodo de cada malla << 8): sus vertices van en el marco de la pieza (el de la
# ultima operacion de la pila antes del giro animado) y la demo pone la matriz de cada cuadro (coche_anima)
P_CUERPO, P_VOLANTE, P_ALERON, P_FAROS, P_PINZA, P_RUEDA, NPIEZAS = 0, 1, 2, 3, 4, 8, 12   # pinza y rueda: + k (0..3)
P_SOMBRA = 12   # la sombra plana +0C: pieza propia (fuera de car+F0), sin datos en ANI1 (que sigue con NPIEZAS)
P_INTERIOR = 13 # el habitaculo +04: pieza propia con la matriz local identidad (46AE70 con vm = 2), sin datos en ANI1
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
    +04 (el habitaculo: salpicadero, cuadro de instrumentos y capo) va como pieza P_INTERIOR del grupo G_INTERIOR: el
    original lo dibuja solo con vm = 2 (car+4 & 0xC000 = 0x8000, la vista interior de 4A2BA0), con la matriz local
    identidad (sin car+F0) y, de lo demas, solo los faros escamoteables +64 y su luz +60; con vm = 1 no. Discos: +90 (G_DISCO) o el caliente +94
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
    dib(u(4), 'interior', G_INTERIOR, P_INTERIOR)               # vm = 2: matriz local identidad (fuera de car+F0)
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
    return vis | ((grupo != NADA) & (grupo != G_DISCO) & (grupo != G_DISCO_CAL)), dueno   # (G_INTERIOR entero)

def ojo_interior(A, m):
    """el ojo de la vista interior del modelo m (tabla CAM1 por modelo de 484EE0, la de tools/conv_param.py: sus tres
    primeros floats; demo/camara.c lo usa como ojo y mira 1 m delante) en el marco del coche"""
    import conv_param
    d = A.elf.d; cam, _ = conv_param.busca_camara(d, conv_param.segmentos(d))
    if cam is None: return None
    mod_t = np.frombuffer(cam[0x64c + 0x5460:0x64c + 0x5460 + 0x438], '<f4').reshape(30, 9)
    return mod_t[m, :3].astype(float)

def vacia_interior(A, mod, m, res=(960, 720)):
    """del habitaculo (G_INTERIOR) quita lo que no se ve desde el ojo de la vista interior: camaras en el ojo y a
    +-0,15 m, girando +-50 grados a los lados y de -25 a +15 grados arriba (el cabeceo de la vista y los derrapes);
    lo opaco del habitaculo tapa. Solo se ve en esa vista: lo demas (bajos del salpicadero, caras de dentro) sobra"""
    ojo = ojo_interior(A, m)
    if ojo is None: print('   habitaculo: sin el ojo de la vista interior (entero)'); return
    T, dueno = triangulos(mod)
    es = np.array([mod.mallas[k][9].get('grupo', NADA) == G_INTERIOR for k in dueno])
    if not es.any(): return
    Ti = np.ascontiguousarray(T[es])
    op = np.array([1 if mod.mallas[k][0] == 0 else 0 for k in dueno[es]], np.uint8)
    C = []
    for dx in (-0.15, 0.0, 0.15):
        for dy in (-0.1, 0.0, 0.1):
            o = ojo + (dx, dy, 0.0)
            for yaw in np.radians(np.arange(-50, 51, 10)):
                for pit in np.radians((-25, -12, 0, 15)):
                    dr = np.array([np.sin(yaw) * np.cos(pit), np.sin(pit), -np.cos(yaw) * np.cos(pit)])
                    C.append(mira(o, o + dr))
    C = np.array(C, np.float32); lib = _lib(); f = ctypes.POINTER(ctypes.c_float)
    cuenta = np.zeros(len(Ti), np.int32)
    lib.visibles(Ti.ctypes.data_as(f), len(Ti), op.ctypes.data_as(ctypes.POINTER(ctypes.c_ubyte)), C.ctypes.data_as(f),
                 len(C), res[0], res[1], cuenta.ctypes.data_as(ctypes.POINTER(ctypes.c_int)))
    vis = np.ones(len(T), bool); vis[np.nonzero(es)[0]] = cuenta > 0
    n0 = sum(x[5] for x in mod.mallas if x[9].get('grupo') == G_INTERIOR)
    vacia(mod, vis, dueno)
    n1 = sum(x[5] for x in mod.mallas if x[9].get('grupo') == G_INTERIOR)
    print('   habitaculo: %d -> %d vertices (lo que se ve desde el ojo %.2f %.2f %.2f)' % (n0, n1, *ojo))

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

# ------------------------------------------------------------------ coches mas ligeros (Rev 2.9.6, DESVIACION pedida)
# Variante A (--rebaja): menos resolucion en las texturas que no se ven de cerca. Regla, por las piezas (nombres de
# llamadas()) que usan cada textura (ya vaciado el coche):
#  - FIJA (no se toca): la que usa alguna malla con pintura o reflejo (CEX2 'esp': carroceria y cristales, sus mapas de
#    entorno), alguna luz (grupos G_PILOTOS..G_FARO, piezas faros, faro_luz, pilotos*, aleron_freno), la sombra, y las
#    de recorte o transparentes (lista 1 / 2: emblemas, letras, matriculas, rejillas) salvo las de las ruedas;
#  - RUEDA: solo la usan buje (llanta y neumatico), pinza y discos: a 64 de lado como mucho (con su proporcion);
#  - INTERIOR: solo la usa el habitaculo +04 (G_INTERIOR, solo la vista interior): a la mitad de lado;
#  - OCULTA: opaca, solo de carroceria, extra, puertas, volante, palanca o habitaculo y con el 85 % o mas de su area
#    sin ver desde las camaras del juego (camaras(), seleccion, carrera y vuelta baja, en reposo) contando los cristales,
#    rejillas y recortes como opacos y sin el habitaculo: bajos, motor bajo el cristal o la rejilla y lo que solo se ve
#    por las ventanillas. Si la comparte el habitaculo, volante o palanca (salpicadero visto por el cristal): a la mitad;
#    si no (bajos y motor): a 64 de lado como mucho;
#  - el resto (carroceria, faros, pilotos, emblemas, matriculas y lo que se ve por fuera): FIJA.
# Las de 64 o menos de lado no cambian. La reduccion es un filtro de caja con el alfa premultiplicado.
PZ_RUEDA_N = {'buje', 'pinza', 'disco', 'disco_cal'}
PZ_LUZ_N = {'faros', 'faro_luz', 'pilotos', 'pilotos_noche', 'pilotos_freno', 'aleron_freno', 'sombra'}
PZ_CUERPO_N = {'carroceria', 'extra', 'puerta', 'volante', 'palanca', 'aleron', 'interior'}
PZ_HAB_N = {'interior', 'volante', 'palanca'}
OCULTA_MIN = 0.85                                               # parte del area sin ver para ser OCULTA

def _area(T):
    a = T[:, 3:6] - T[:, 0:3]; b = T[:, 6:9] - T[:, 0:3]
    return 0.5 * np.linalg.norm(np.cross(a, b), axis=1)

def clasifica_texturas(mod, visible=False):
    """{textura: (clase, ancho, alto nuevos, parte sin ver)} con la regla de arriba (las que cambian y las que no); con
    visible (C2, Rev 2.9.7) tambien la clase 'visible' (_mitad)"""
    T, dueno = triangulos(mod)
    grupo = np.array([mod.mallas[k][9].get('grupo', NADA) for k in dueno])
    fuera = grupo == G_INTERIOR                                 # el habitaculo no se dibuja por fuera
    Tv = np.ascontiguousarray(np.where(fuera[:, None], 0, T).astype(np.float32))
    op = (~fuera).astype(np.uint8)                              # cristales, rejillas y recortes: tambien tapan
    C = camaras(True); cuenta = np.zeros(len(T), np.int32); lib = _lib(); f = ctypes.POINTER(ctypes.c_float)
    lib.visibles(Tv.ctypes.data_as(f), len(Tv), op.ctypes.data_as(ctypes.POINTER(ctypes.c_ubyte)), C.ctypes.data_as(f),
                 len(C), 960, 720, cuenta.ctypes.data_as(ctypes.POINTER(ctypes.c_int)))
    area = _area(T); uso = {}
    for i, k in enumerate(dueno):
        x = mod.mallas[k]
        if x[1] < 0: continue
        u = uso.setdefault(x[1], {'pz': set(), 'fija': False, 'a': 0.0, 'oc': 0.0})
        u['pz'].add(x[8]); u['a'] += area[i]; u['oc'] += area[i] if cuenta[i] == 0 else 0.0
    for x in mod.mallas:                                        # (las mallas de 2 vertices no tienen triangulos)
        if x[1] < 0 or x[1] not in uso: continue
        if x[9].get('esp') or x[9].get('grupo', NADA) not in (NADA, G_INTERIOR, G_DISCO, G_DISCO_CAL) or x[8] in PZ_LUZ_N:
            uso[x[1]]['fija'] = True
    res = {}
    for ti, u in sorted(uso.items()):
        w, h, fmt, li = mod.texs[ti][:4]; pz = u['pz']; oc = u['oc'] / max(u['a'], 1e-9)
        if max(w, h) <= 64 or u['fija']: clase = 'fija'
        elif pz <= PZ_RUEDA_N: clase = 'rueda'
        elif li != 0: clase = 'fija'
        elif pz <= {'interior'}: clase = 'interior'
        elif pz <= PZ_CUERPO_N and oc >= OCULTA_MIN: clase = 'oculta_hab' if pz & PZ_HAB_N else 'oculta'
        else: clase = 'fija'
        if visible and clase == 'fija' and not u['fija'] and li == 0 and max(w, h) >= 256 and not pz <= {'interior'}:
            w2, h2, err = _mitad(mod.rgba[ti])
            res[ti] = ('visible', w2, h2, oc) if err <= VIS_TEX_MAX else ('visible', w, h, oc); continue
        if clase in ('rueda', 'oculta'): s = 64.0 / max(w, h)
        elif clase in ('interior', 'oculta_hab'): s = 0.5
        else: s = 1.0
        res[ti] = (clase, max(8, int(w * s)), max(8, int(h * s)), oc)
    return res

def _reduce(a, w2, h2):
    """la imagen RGBA a w2 x h2 con un filtro de caja y el alfa premultiplicado (sin bordes del color de fuera)"""
    from PIL import Image
    a = a.astype(np.float32); al = a[..., 3:4] / 255.0
    pm = np.concatenate([a[..., :3] * al, a[..., 3:4]], 2)
    r = np.stack([np.asarray(Image.fromarray(pm[..., c]).resize((w2, h2), Image.BOX)) for c in range(4)], 2)
    rgb = np.where(r[..., 3:4] > 0, r[..., :3] * 255.0 / np.maximum(r[..., 3:4], 1e-3), 0)
    return np.clip(np.round(np.concatenate([rgb, r[..., 3:4]], 2)), 0, 255).astype(np.uint8)

def _mitad(a):
    """C2: el tamano de la textura visible a la mitad en el lado en que menos se nota (error RMS del RGB al volver a
    ampliarla, bilineal, con repeticion) o en los dos si su error no pasa de VIS_TEX_ERR. Devuelve (ancho, alto, error)"""
    from PIL import Image
    h, w = a.shape[:2]
    def err(w2, h2):
        b = _reduce(a, w2, h2).astype(np.float32)
        t = np.tile(b, (3, 3, 1))                               # repetida, para que el borde no cuente de mas
        up = np.stack([np.asarray(Image.fromarray(t[..., c]).resize((3 * w, 3 * h), Image.BILINEAR)) for c in range(3)], 2)
        up = up[h:2 * h, w:2 * w]
        return float(np.sqrt(((up - a[..., :3].astype(np.float32)) ** 2).mean()))
    c = sorted((err(*s), s) for s in ((w // 2, h), (w, h // 2)))
    e2 = err(w // 2, h // 2)
    if e2 <= VIS_TEX_ERR: return w // 2, h // 2, e2
    return c[0][1][0], c[0][1][1], c[0][0]

def rebaja_texturas(mod, visible=False, cl=None):
    """variante A: aplica clasifica_texturas (antes de recorta_texturas y de la VQ); visible: con la clase 'visible' de
    C2; cl: las clases ya hechas. Devuelve {clase: (antes, despues)} en texeles"""
    cl = cl or clasifica_texturas(mod, visible); suma = {}
    for ti, (clase, w2, h2, oc) in cl.items():
        w, h, fmt, li, off, tam = mod.texs[ti]
        s = suma.setdefault(clase, [0, 0]); s[0] += w * h; s[1] += w2 * h2
        if (w2, h2) == (w, h) or ti not in {x[1] for x in mod.mallas}:
            if max(w, h) > 64 and (w2, h2) == (w, h): print('   textura %3d %-10s %3dx%-3d    igual  (sin ver %3.0f %%; %s)' % (
                ti, clase, w, h, 100 * oc, ' '.join(sorted({x[8] for x in mod.mallas if x[1] == ti}))))
            continue
        b = _reduce(mod.rgba[ti], w2, h2)
        d = modelo.codifica16(b, fmt & 0xff)
        mod.texs[ti] = (w2, h2, fmt, li, len(mod.blob), len(d)); mod.blob += d; mod.rgba[ti] = b
        print('   textura %3d %-10s %3dx%-3d -> %3dx%-3d (sin ver %3.0f %%; %s)' % (
            ti, clase, w, h, w2, h2, 100 * oc, ' '.join(sorted({x[8] for x in mod.mallas if x[1] == ti}))))
    return suma

# Variante B (--carrera): las mallas que solo se ven de cerca llevan el bit NODO_CERCA en el campo del nodo y la demo
# no las dibuja con la camara de persecucion en carrera (demo/main.c coche_dibuja); la seleccion, el salon, la salida,
# la meta y la vista interior dibujan todo. Sin el bit (coches de antes) se dibuja todo, como hasta ahora.
# Criterio (el del vaciado, con otras camaras): se ve en carrera el triangulo que gana algun pixel a 640x480 (la
# pantalla) en CERCA_MIN o mas de las camaras de carrera (las de camaras(): detras, a +-100 grados de deriva, 1,9-2,9 m
# de alto y 4,5-6,5 m) x las 6 posturas de las piezas animadas; las luces, siempre. Lo demas (lo que solo se ve de lado
# o de frente desde cerca, por debajo, desde la seleccion, y lo que solo asoma en un pixel de 1-3 camaras: piezas
# pequenas a la distancia de la persecucion) va aparte, con el bit. Las tiras se parten como en vacia (huecos de 2).
NODO_CERCA = 0x10000
CERCA_MIN = 4

def camaras_carrera():
    return camaras(False)[48:]                                  # sin las 48 de la seleccion ni la vuelta baja

def separa(mod, vis, marca):
    """parte las tiras de mod: lo de vis (por triangulo, en el orden de triangulos()) queda como estaba y lo demas va
    en tiras aparte con marca = True en el extra (cerca: NODO_CERCA, variante B; tras: NODO_TRASERO, Rev 2.9.8 P3);
    huecos de 2 como en vacia y 1 o 2 triangulos sueltos en las puntas se quedan (partir cuesta 2 vertices)"""
    V = np.concatenate(mod.verts); nuevas = []; verts = []; nv = 0; ti = 0
    def tira(x, a, b, cerca):                                   # triangulos a..b de la malla x (vertices a..b+2)
        nonlocal nv
        j = a
        while j <= b:
            e = min(b, j + 256 - 3)
            q = V[x[4] + j:x[4] + e + 3].copy(); verts.append(q)
            P = np.stack([q['x'], q['y'], q['z']], 1).astype(float); cc = P.mean(0)
            nuevas.append([x[0], x[1], cc, float(np.sqrt(((P - cc) ** 2).sum(1).max())), nv, len(q), x[6], x[7], x[8],
                           dict(x[9], N=x[9]['N'][j:e + 3], **{marca: cerca})])
            nv += len(q); j = e + 1
    for x in mod.mallas:
        n = x[5]; v = vis[ti:ti + n - 2]; ti += n - 2
        if n < 3: tira(x, 0, -1, False); continue
        tramos = []; a = None; oc = 0                           # lo de vis: como vacia (huecos de 2)
        for j in range(n - 2):
            if v[j]:
                if a is None: a = j
                b = j; oc = 0
            elif a is not None:
                oc += 1
                if oc > 2: tramos.append((a, b)); a = None; oc = 0
        if a is not None: tramos.append((a, b))
        if tramos and tramos[0][0] <= 2: tramos[0] = (0, tramos[0][1])          # 1 o 2 triangulos sueltos en las
        if tramos and tramos[-1][1] >= n - 5: tramos[-1] = (tramos[-1][0], n - 3)   # puntas: no vale la pena partir
        dentro = np.zeros(n - 2, bool)
        for a, b in tramos: dentro[a:b + 1] = True; tira(x, a, b, False)
        j = 0
        while j < n - 2:                                        # lo demas: aparte, con la marca
            if dentro[j]: j += 1; continue
            k = j
            while k + 1 < n - 2 and not dentro[k + 1]: k += 1
            tira(x, j, k, True); j = k + 1
    mod.mallas, mod.verts, mod.nv = nuevas, verts, nv

def parte_carrera(A, mod, m, res=(640, 480), minimo=CERCA_MIN):
    T, dueno = triangulos(mod)
    grupo = np.array([mod.mallas[k][9].get('grupo', NADA) for k in dueno])
    fuera = grupo == G_INTERIOR
    op = np.array([1 if mod.mallas[k][0] == 0 and grupo[i] == NADA else 0 for i, k in enumerate(dueno)], np.uint8)
    C = camaras_carrera(); lib = _lib(); f = ctypes.POINTER(ctypes.c_float); cuenta = np.zeros(len(T), np.int32)
    for D in poses(A, m):
        Tp, _ = triangulos(mod, posa(mod, D)); Tp = np.ascontiguousarray(np.where(fuera[:, None], 0, Tp).astype(np.float32))
        c = np.zeros(len(T), np.int32)
        lib.visibles(Tp.ctypes.data_as(f), len(Tp), op.ctypes.data_as(ctypes.POINTER(ctypes.c_ubyte)), C.ctypes.data_as(f),
                     len(C), res[0], res[1], c.ctypes.data_as(ctypes.POINTER(ctypes.c_int)))
        cuenta += c
    vis = (cuenta >= minimo) | ((grupo != NADA) & (grupo != G_DISCO) & (grupo != G_DISCO_CAL)) | fuera
    n0 = vertices_carrera(mod)
    separa(mod, vis, 'cerca')
    print('   carrera: %d -> %d vertices (en la seleccion %d)' % (n0, vertices_carrera(mod), vertices_dibujados(mod)))

# ------------------------------------------------------------------ coche trasero segun la camara (Rev 2.9.8 P3, DESVIACION)
# Tecnica de Crazy Taxi (0x8c0200e4..0x8c02017c: el cuerpo "bajo", sin el frontal, cuando la camara esta detras; aqui solo
# la idea, nada de su codigo ni de sus datos). Con --visible (y sin --sin-trasero), en el ligero COCHEnnL.DAT: los
# triangulos que no se ven desde ninguna camara del cono trasero van en tiras aparte (separa, huecos de 2) con el bit
# NODO_TRASERO en el campo del nodo, al final de su lista y textura (orden: lista, textura, trasero; ver convierte). La demo no las
# envia con la camara dentro del cono (demo/main.c coche_trasero, -DCOCHE_TRASERO); fuera de el, en la vista interior,
# la salida, la meta, el salon y los finales va todo. El original dibuja el coche entero.
#  - El cono, en el marco del coche (el morro es -z, detras +z) y desde su origen: guinada del ojo 180 +- TRAS_GUINADA
#    (35 grados) contada desde el morro; cabeceo de la camara de TRAS_CAB (-65..+5 grados: el ojo de 5 grados por debajo
#    a 65 por encima); alabeo +-TRAS_ALABEO (35) y el ojo a TRAS_DMIN (3 m) o mas. Lo que se ve desde un ojo no depende
#    de hacia donde mire la camara (si el coche cabe en la imagen): cada camara del cono mira al centro del coche con el
#    campo justo para que quepa, a distancias TRAS_DIST (de 3 a 60 m, casi una proyeccion paralela), guinada y cabeceo
#    cada TRAS_PASO grados, alabeo 0 y en los bordes +-35 (camaras_trasero). Por las 6 posturas de poses().
#  - Z-buffer por software (tools/c/visible.c, el de vacia y parte_carrera): tapa lo opaco de la lista 0 sin grupo; los
#    cristales, recortes, luces y la sombra no tapan pero se miran. El habitaculo (G_INTERIOR, solo la vista interior) y
#    la sombra plana (P_SOMBRA: va en el suelo, fuera de car+F0) no se separan.
#  - Borde (TRAS_ANILLO, 1): pasa a visible el triangulo oculto que comparte un vertice (posicion a 1 mm) con uno
#    visible: las astillas entre los angulos de la rejilla estan en el borde de lo que se ve. Sin el borde se separaba
#    el 4,6-6,8 % de los vertices pero la comprobacion encontraba 1-8 triangulos traseros que asomaban (aun con las
#    rondas); con el borde por aristas (y no por vertices), 2,3 % y 5 que asomaban (modelo 9); con el de vertices, 0.
#  - Rondas (TRAS_RONDAS, 4): con camaras y posturas al azar dentro del cono, lo que asoma pasa a visible (astillas de la
#    carroceria entre los angulos de la rejilla, que la primera version dejaba: 2-8 triangulos por coche).
#  - comprueba_trasero: otras camaras al azar dentro del cono (distancias, guinada, cabeceo, alabeo y punto al que
#    miran entre las de la rejilla) y posturas al azar (giro de las ruedas, direccion, volante, aleron y faros entre los
#    de poses()) a 1280x960: los triangulos NODO_TRASERO que ganan algun pixel (0 = la imagen dentro del cono es la
#    misma con y sin ellos).
NODO_TRASERO = 0x20000
TRAS_GUINADA, TRAS_CAB, TRAS_ALABEO, TRAS_DMIN = 35.0, (-65.0, 5.0), 35.0, 3.0
TRAS_DIST = (3.0, 3.6, 4.5, 5.5, 7.0, 9.0, 13.0, 20.0, 60.0)
TRAS_PASO = 5.0
TRAS_RONDAS = 4                 # rondas al azar de parte_trasera
TRAS_ANILLO = 1                 # anillos de triangulos visibles anadidos al borde de lo oculto

def mira_t(ojo, punto, alabeo=0.0, th=0.75):
    """como mira con alabeo (radianes, giro de la camara sobre su eje) y la tangente del medio angulo horizontal th"""
    f = np.array(punto, float) - ojo; f /= np.linalg.norm(f)
    r = np.cross(f, [0, 1, 0]); r /= np.linalg.norm(r); u = np.cross(r, f)
    r, u = r * np.cos(alabeo) + u * np.sin(alabeo), u * np.cos(alabeo) - r * np.sin(alabeo)
    tv = th * 0.75
    M = np.zeros((4, 4))
    M[:3, 0] = r / th; M[:3, 1] = u / tv; M[:3, 2] = f; M[:3, 3] = f
    M[3, 0] = -(ojo @ r) / th; M[3, 1] = -(ojo @ u) / tv; M[3, 2] = M[3, 3] = -(ojo @ f)
    return M.astype(np.float32)

def _ojo_t(d, gui, cab):
    """el ojo a d m del origen del coche con la guinada gui (0 = detras, +z) y el cabeceo de la camara cab (grados;
    negativo = mira hacia abajo: el ojo por encima)"""
    g, e = np.radians(gui), np.radians(-cab)
    return np.array([d * np.cos(e) * np.sin(g), d * np.sin(e), d * np.cos(e) * np.cos(g)])

def _encuadre(mod):
    V = np.concatenate(mod.verts); P = np.stack([V['x'], V['y'], V['z']], 1).astype(float)
    c = (P.min(0) + P.max(0)) / 2; return c, float(np.sqrt(((P - c) ** 2).sum(1).max())) + 0.3

def _cam_t(ojo, c, R, alabeo=0.0, mira_a=None):
    """camara en ojo que mira a c (o a mira_a) con el campo para que quepa la esfera (c, R)"""
    d = np.linalg.norm(ojo - c); s = np.sin(min(np.arcsin(min(R / d, 0.999)) + (0 if mira_a is None else 0.35), 1.45))
    th = max(0.75, s / np.sqrt(max(1 - s * s, 1e-6)) * 1.05 / 0.75)   # (el medio angulo vertical es th x 0,75)
    return mira_t(ojo, c if mira_a is None else mira_a, alabeo, th)

def camaras_trasero(mod, paso=TRAS_PASO):
    c, R = _encuadre(mod); C = []
    gs = np.arange(-TRAS_GUINADA, TRAS_GUINADA + 1e-6, paso); cs = np.arange(TRAS_CAB[0], TRAS_CAB[1] + 1e-6, paso)
    for d in TRAS_DIST:
        for g in gs:
            for cb in cs:
                o = _ojo_t(d, g, cb)
                if np.linalg.norm(o - c) <= R + 0.05: continue      # (no pasa: TRAS_DMIN deja el ojo fuera)
                C.append(_cam_t(o, c, R))
                if d == 5.5 and (abs(g) == TRAS_GUINADA or cb in TRAS_CAB or (g % 10 == 0 and cb % 10 == 0)):
                    for al in (-TRAS_ALABEO, TRAS_ALABEO): C.append(_cam_t(o, c, R, np.radians(al)))
    return np.array(C, np.float32)

def _cuenta_t(mod, C, posturas, res):
    """por triangulo (orden de triangulos()), en cuantas camaras x posturas gana algun pixel"""
    T, dueno = triangulos(mod)
    grupo = np.array([mod.mallas[k][9].get('grupo', NADA) for k in dueno])
    fuera = grupo == G_INTERIOR
    op = np.array([1 if mod.mallas[k][0] == 0 and grupo[i] == NADA else 0 for i, k in enumerate(dueno)], np.uint8)
    lib = _lib(); f = ctypes.POINTER(ctypes.c_float); cuenta = np.zeros(len(T), np.int32)
    for D in posturas:
        Tp, _ = triangulos(mod, posa(mod, D))
        Tp = np.ascontiguousarray(np.where(fuera[:, None], 0, Tp).astype(np.float32))
        c = np.zeros(len(T), np.int32)
        lib.visibles(Tp.ctypes.data_as(f), len(Tp), op.ctypes.data_as(ctypes.POINTER(ctypes.c_ubyte)), C.ctypes.data_as(f),
                     len(C), res[0], res[1], c.ctypes.data_as(ctypes.POINTER(ctypes.c_int)))
        cuenta += c
    return cuenta, dueno, grupo

def parte_trasera(A, mod, m, res=(960, 720), rondas=TRAS_RONDAS):
    """P3 (ver arriba): separa con NODO_TRASERO lo que no se ve desde el cono trasero. Despues de la rejilla, rondas de
    camaras y posturas al azar (_azar_t, otra semilla cada una): lo que asoma en alguna pasa a visible (astillas entre
    los angulos de la rejilla); hasta una ronda sin nada o TRAS_RONDAS"""
    C = camaras_trasero(mod)
    cuenta, dueno, grupo = _cuenta_t(mod, C, poses(A, m), res)
    pz = np.array([mod.mallas[k][9].get('pieza', P_CUERPO) for k in dueno])
    vis = (cuenta > 0) | (grupo == G_INTERIOR) | (pz == P_SOMBRA) | np.array([mod.mallas[k][5] < 3 for k in dueno], bool)
    oc0 = int((~vis).sum()); rec = []
    if TRAS_ANILLO:                                             # fuera del oculto el borde: lo que toca a algo visible
        T, _ = triangulos(mod); Q = np.round(T.reshape(-1, 3, 3).astype(np.float64), 3)
        clave = {}; vid = np.zeros((len(T), 3), np.int64)
        for i in range(len(T)):
            for j in range(3): vid[i, j] = clave.setdefault(tuple(Q[i, j]), len(clave))
        for _ in range(TRAS_ANILLO):
            pv = np.zeros(len(clave), bool); pv[vid[vis].ravel()] = True
            vis = vis | pv[vid].any(1)
    oc1 = int((~vis).sum())
    for r in range(rondas):
        Cr, Pr = _azar_t(A, mod, m, np.random.default_rng(1000 * (r + 1) + m))
        cr, _, _ = _cuenta_t(mod, Cr, Pr, (1280, 960))
        nuevos = (cr > 0) & ~vis; rec.append(int(nuevos.sum())); vis |= nuevos
        if not nuevos.any(): break
    n0 = vertices_carrera(mod); t0 = len(dueno)
    separa(mod, vis, 'tras')
    n1 = vertices_dibujados(mod, cerca=False, tras=False)
    print('   trasero: %d camaras x %d posturas; %d de %d triangulos no se ven desde el cono (%d sin el borde, %d tras las '
          'rondas al azar: %s); vertices %d -> %d con la camara en el cono (%.1f %%), %d mallas traseras' % (len(C),
          len(poses(A, m)), oc0, t0, oc1, int((~vis).sum()), '+'.join(map(str, rec)), n0, n1, 100.0 * (n0 - n1) / max(n0, 1),
          sum(1 for x in mod.mallas if x[9].get('tras'))))
    return n0, n1

def _azar_t(A, mod, m, rng, ncam=240, npos=4):
    """camaras al azar dentro del cono (distancia, guinada, cabeceo, alabeo y punto al que miran; una de cada 4 en el
    borde de la guinada y otra en el del cabeceo) y posturas al azar (poses_azar)"""
    c, R = _encuadre(mod); C = []
    for i in range(ncam):
        d = float(np.exp(rng.uniform(np.log(TRAS_DMIN), np.log(80.0))))
        g = rng.uniform(-TRAS_GUINADA, TRAS_GUINADA); cb = rng.uniform(*TRAS_CAB)
        if i % 4 == 0: g = TRAS_GUINADA * rng.choice((-1, 1))
        if i % 4 == 1: cb = TRAS_CAB[rng.integers(2)]
        o = _ojo_t(d, g, cb)
        if np.linalg.norm(o - c) <= R + 0.05: continue
        C.append(_cam_t(o, c, R, np.radians(rng.uniform(-TRAS_ALABEO, TRAS_ALABEO)), c + rng.uniform(-0.8, 0.8, 3)))
    return np.array(C, np.float32), poses_azar(A, m, npos, rng)

def poses_azar(A, m, n, rng):
    """posturas al azar entre las de poses(): giro de las ruedas, direccion, volante, aleron y faros"""
    L = disposiciones(A) + 0x128 * m; arriba = A.elf.f32(L + 0x78) if A.elf.u32(L + 0x64) != NINGUNO else 0.0
    res = []
    for i in range(n):
        sg = rng.uniform(-1, 1)
        q = {'giro': list(rng.uniform(0, 2 * np.pi, 4)), 'dir': [sg * DIR_MAX] * 2 + [0.0] * 2, 'volante': sg * VOL_MAX,
             'aleron': ALERON_MIN * rng.uniform(0, 1), 'faros': arriba * rng.uniform(0, 1)}
        res.append({p: M for _, M, _, _, p in llamadas(A, m, q) if p != P_CUERPO})
    return res

def comprueba_trasero(A, mod, m, res=(1280, 960), semilla=298):
    """camaras y posturas al azar dentro del cono (_azar_t, con otra semilla que las rondas de parte_trasera): devuelve
    (triangulos traseros que ganan algun pixel, camaras x posturas, triangulos traseros)"""
    C, P = _azar_t(A, mod, m, np.random.default_rng(semilla + m))
    cuenta, dueno, _ = _cuenta_t(mod, C, P, res)
    tr = np.array([bool(mod.mallas[k][9].get('tras')) for k in dueno])
    return int(((cuenta > 0) & tr).sum()), len(C) * len(P), int(tr.sum())

def vertices_carrera(mod):
    """los de vertices_dibujados sin las mallas solo de cerca"""
    return vertices_dibujados(mod, cerca=False)

def vertices(mod):
    return sum(x[5] for x in mod.mallas)

def vertices_dibujados(mod, cerca=True, tras=True):
    """los que se dibujan a la vez: todo lo fijo, el faro encendido, el mayor de los tres juegos de pilotos y el
    mayor de los dos juegos de discos (el habitaculo G_INTERIOR va solo, con la vista interior: no cuenta); cerca =
    False: sin las mallas solo de cerca (variante B, en carrera); tras = False: sin las traseras (P3, Rev 2.9.8)"""
    g = {}
    for x in mod.mallas:
        if not cerca and len(x) > 9 and x[9].get('cerca'): continue
        if not tras and len(x) > 9 and x[9].get('tras'): continue
        k = x[9].get('grupo', NADA) if len(x) > 9 else NADA; g[k] = g.get(k, 0) + x[5]
    return g.get(NADA, 0) + g.get(G_FARO, 0) + max(g.get(G_PILOTOS, 0), g.get(G_NOCHE, 0), g.get(G_FRENO, 0)) + \
        max(g.get(G_DISCO, 0), g.get(G_DISCO_CAL, 0))

def triangulos_n(mod):
    return sum(x[5] - 2 for x in mod.mallas)

# ------------------------------------------------------------------ coche visible mas ligero (Rev 2.9.7, C2, DESVIACION pedida)
# --visible: ademas del COCHEnn.DAT de siempre (con --rebaja: el de la seleccion de coche y el salon, sin cambios), el
# ligero COCHEnnL.DAT que la demo carga para la carrera (demo/main.c coche_carga: coche_pide_ligero): menos triangulos en
# lo que se ve del coche y menos resolucion en sus texturas visibles grandes, para dejar TA y VRAM a la etapa. De cerca
# (seleccion) se notaba en los reflejos de la pintura (capo, paragolpes): por eso dos ficheros; los dos modelos en uno
# (con NODO_CERCA) no caben en el bloque del coche (COCHE_RAM: 660-740 KB frente a 496). El original lleva la geometria
# y las texturas enteras.
#  - Siempre el vaciado de lo que no se ve (visibilidad() con la vuelta baja, el de cuando se pasa del tope): no cambia
#    la imagen desde las camaras del juego.
#  - simplifica(): solo las mallas opacas (lista 0) de carroceria, extra, puertas, volante, palanca, aleron, ruedas
#    (llanta y neumatico), pinzas y discos; no las luces, ni lo de recorte o transparente (emblemas, letras, matriculas,
#    cristales, rejillas de recorte), ni la sombra, ni el habitaculo. Por pieza (las animadas se mueven aparte) una malla
#    de posiciones con "cunas" (los atributos de un vertice en un grupo de mallas con el mismo material, textura y
#    pieza). Las tiras del original no comparten vertices y en la misma posicion cada una trae su normal y su luz cocida,
#    que difieren un poco (0xBE frente a 0xC0): se juntan en una cuna las de la misma posicion y grupo con UV a menos de
#    VIS_SUELDA_UV, color a menos de VIS_SUELDA_C por canal y normales a menos de ~25 grados (con la media); si no, son
#    otra cuna (costura: arista viva, otra UV u otro grupo). Colapso de aristas u -> v del mas barato al mas caro: u
#    desaparece y v queda donde estaba, y cada cuna de u pasa a la cuna de v del otro lado de la arista (no hay UV,
#    color ni normal nuevos); el coste es el error cuadrico generalizado (Garland-Heckbert 1998) de posicion, UV, color y
#    normal de las cunas (la luz cocida y la normal, de la que sale el reflejo de la pintura) con el area de peso, mas el de los planos de las aristas de borde,
#    costura o limite de grupo (perpendiculares a la cara, peso VIS_BORDE): esas lineas solo se acortan a lo largo de si
#    mismas y sin doblarse. Fuera el colapso que deje una cuna de u sin pareja (esquinas de costura), que en un borde no
#    vaya a lo largo de el, que gire una cara mas de 37 grados (VIS_GIRO), la deje sin area o rompa la variedad
#    (condicion del enlace); fijas las posiciones que usan las mallas que no se simplifican (cristales, luces, emblemas,
#    matriculas), las que estan a menos de VIS_CERCA de ellas (para que lo de alrededor no las tape) y las de aristas de
#    mas de dos caras. Hasta dejar VIS_RAZON de los triangulos o llegar al error VIS_TOPE.
#  - Lo simplificado sale en tiras nuevas (tiras(): voraz, desde el triangulo libre con menos vecinos libres, hacia los
#    dos lados, hasta 256 vertices; la demo no descarta caras, asi que el sentido de la tira da igual).
#  - Texturas (clasifica_texturas con visible, con las clases del coche de siempre): las de --rebaja y ademas (clase
#    'visible') las opacas de fuera con un lado de 256 o mas, sin pintura, reflejo, luces ni sombra: a la mitad en el
#    lado en que menos se nota (menor error al volver a ampliarla) y en los dos si ese error no pasa de VIS_TEX_ERR; si
#    aun la mejor mitad pasa de VIS_TEX_MAX (atlas de motor y salpicadero con detalle), entera. Siguen con la VQ
#    (comprobado: todas las rebajadas salen con VQ).
VIS_RAZON = 0.6                 # parte de los triangulos simplificables que queda
VIS_TOPE = 1e-3                 # error maximo de un colapso (m2 de area x m2 de distancia; casi nunca llega)
VIS_GIRO = 0.8                  # coseno minimo entre la normal de una cara antes y despues de un colapso
VIS_UV, VIS_COLOR = 0.5, 0.06   # peso de las UV (1 = 0,5 m) y del color (255 = 6 cm) en el error
VIS_NORMAL = 0.1                # peso de la normal (el reflejo de la pintura sale de ella: 0,1 rad = 1 cm)
VIS_SUELDA_UV, VIS_SUELDA_C = 1 / 512, 12   # vertices de tiras distintas que se sueldan (UV y color por canal)
VIS_CERCA = 0.04                # fijas las posiciones a menos de esto (m) de lo que no se simplifica
VIS_TEX_MAX = 8.0               # textura 'visible' que no se baja si aun la mejor mitad pasa de este error RMS (atlas
                                # de motor y salpicadero con detalle: el 360, 575, F430 y 550, que en carrera se notaban)
VIS_BORDE = 10.0                # peso de los planos de las aristas de borde, costura y limite
VIS_TEX_ERR = 3.0               # error RMS (0-255) para bajar una textura en los dos lados
PZ_SIMPLIFICA = {'carroceria', 'extra', 'puerta', 'volante', 'palanca', 'aleron', 'buje', 'pinza', 'disco', 'disco_cal'}

def _cuadrica(P):
    """cuadrica generalizada del triangulo de puntos P (3 x n, las 3 primeras la posicion): (A, b, c) por su area"""
    p, q, r = P
    e1 = q - p; l1 = np.linalg.norm(e1)
    if l1 < 1e-12: return None
    e1 = e1 / l1; e2 = r - p - np.dot(e1, r - p) * e1; l2 = np.linalg.norm(e2)
    if l2 < 1e-12: return None
    e2 = e2 / l2
    g = P[:, :3]; area = 0.5 * np.linalg.norm(np.cross(g[1] - g[0], g[2] - g[0]))
    pe1, pe2 = np.dot(p, e1), np.dot(p, e2)
    A = np.eye(len(p)) - np.outer(e1, e1) - np.outer(e2, e2)
    return A * area, (pe1 * e1 + pe2 * e2 - p) * area, (np.dot(p, p) - pe1 * pe1 - pe2 * pe2) * area

def _normal(a, b, c):
    n = np.cross(b - a, c - a); l = np.linalg.norm(n)
    return (n / l if l > 1e-14 else None), l

def _grupos_vis(mod, vale=None):
    """{clave: [mallas]} de lo que se simplifica (mismo material, textura, pieza y grupo); vale: otra regla de que
    mallas se simplifican (conv_trafico.py --ligero) en lugar de la de los coches"""
    g = {}
    for k, x in enumerate(mod.mallas):
        ex = x[9]
        if x[5] < 3: continue
        if vale is not None:
            if not vale(x): continue
        elif x[0] != 0 or x[8] not in PZ_SIMPLIFICA or ex.get('grupo', NADA) not in (NADA, G_DISCO, G_DISCO_CAL):
            continue
        clave = (x[1], id(x[6]), x[7], x[8], ex.get('grupo', NADA), ex.get('pieza', P_CUERPO), ex.get('esp'), ex.get('uv'),
                 ex.get('cerca', False))
        g.setdefault(clave, []).append(k)
    return g

def simplifica(mod, razon=VIS_RAZON, tope=VIS_TOPE, vale=None, objetivo=None, giro=False):
    """variante C2 (ver arriba): simplifica las mallas de _grupos_vis y las rehace en tiras (en lugar de las de antes).
    Devuelve los triangulos de lo simplificable antes y despues. vale: la regla de _grupos_vis; objetivo: triangulos
    simplificables que deben quedar (en lugar de razon; conv_trafico.py --ligero); giro: el de tiras()"""
    import heapq
    diag = os.environ.get('VIS_DIAG')
    V = np.concatenate(mod.verts); grupos = sorted(_grupos_vis(mod, vale).values())
    if not grupos: return 0, 0
    en_grupo = {k: gi for gi, ks in enumerate(grupos) for k in ks}
    def clave_pos(v, pz): return (pz, round(float(v['x']), 4), round(float(v['y']), 4), round(float(v['z']), 4))
    # posiciones que usan las mallas que no se simplifican (luces, cristales, emblemas, matriculas, otra pieza...): fijas,
    # y tambien las que estan a menos de VIS_CERCA de su superficie (muestras cada 2 cm): la carroceria de alrededor no
    # se mueve y no puede taparlas (sin esto, el hueco de la matricula del 360 se aplanaba y la tapaba a medias)
    from scipy.spatial import cKDTree
    ajenas = set(); muestras = {}
    for k, x in enumerate(mod.mallas):
        if k in en_grupo: continue
        pz = x[9].get('pieza', P_CUERPO)
        for j in range(x[5]): ajenas.add(clave_pos(V[x[4] + j], pz))
        if x[9].get('grupo', NADA) == G_INTERIOR or x[8] == 'sombra': continue
        Q = np.stack([V['x'][x[4]:x[4] + x[5]], V['y'][x[4]:x[4] + x[5]], V['z'][x[4]:x[4] + x[5]]], 1).astype(float)
        L = muestras.setdefault(pz, [])
        for j in range(x[5] - 2):
            a, b, c = Q[j:j + 3]; n = int(min(25, np.ceil(max(np.linalg.norm(b - a), np.linalg.norm(c - a), np.linalg.norm(c - b)) / 0.02)))
            for i in range(n + 1):
                for h in range(n + 1 - i): L.append(a + (b - a) * (i / max(n, 1)) + (c - a) * (h / max(n, 1)))
    arboles = {pz: cKDTree(np.array(L)) for pz, L in muestras.items() if L}
    # posiciones (por pieza animada: cada una se mueve aparte) y cunas (wedges: los atributos de un vertice en un grupo;
    # las tiras del original no comparten vertices y en la misma posicion cada una trae su normal y su luz cocida, que
    # difieren un poco: se juntan los de la misma posicion y grupo con UV a menos de VIS_SUELDA_UV, color a menos de
    # VIS_SUELDA_C por canal y normales a menos de ~25 grados, con la media de color y normal; si no, otra cuna: costura)
    pos = {}; P = []; pfijo = []
    cunas = []                                                  # [posicion, grupo, vertice, normal, suma color, suma N, n]
    por_pos = {}; caras = {}; vistos = set()
    for gi, ks in enumerate(grupos):
        for k in ks:
            x = mod.mallas[k]; i0, n = x[4], x[5]; N = x[9]['N']; pz = x[9].get('pieza', P_CUERPO); ids = []
            for j in range(n):
                v = V[i0 + j]; c = int(v['c']); nj = N[j].astype(float); kp = clave_pos(v, pz)
                if kp not in pos:
                    pos[kp] = len(P); P.append((float(v['x']), float(v['y']), float(v['z']))); pfijo.append(kp in ajenas)
                p = pos[kp]
                col = np.array([(c >> 24) & 255, (c >> 16) & 255, (c >> 8) & 255, c & 255], float)
                el = None
                for i in por_pos.get((p, gi), ()):
                    w = cunas[i]; a = w[2]
                    if abs(a['u'] - v['u']) <= VIS_SUELDA_UV and abs(a['v'] - v['v']) <= VIS_SUELDA_UV and \
                            np.abs(w[4] / w[6] - col).max() <= VIS_SUELDA_C and \
                            np.dot(w[5], nj) >= 0.9 * np.linalg.norm(w[5]) * max(np.linalg.norm(nj), 1e-9):
                        el = i; break
                if el is None:
                    el = len(cunas); cunas.append([p, gi, v.copy(), nj, col.copy(), nj.copy(), 1])
                    por_pos.setdefault((p, gi), []).append(el)
                else:
                    w = cunas[el]; w[4] += col; w[5] += nj; w[6] += 1
                ids.append(el)
            for j in range(n - 2):
                t = (ids[j], ids[j + 1], ids[j + 2]); tp = frozenset(cunas[i][0] for i in t)
                if len(tp) < 3 or tp in vistos: continue        # degenerados y repetidos (caras dobles: sin descarte, sobran)
                vistos.add(tp); caras[len(caras)] = list(t)
    P = np.array(P); pfijo = np.array(pfijo, bool); nP = len(P); nw = len(cunas)
    pz_de = np.array([kp[0] for kp in sorted(pos, key=pos.get)])
    for pz, t in arboles.items():
        i = np.nonzero(pz_de == pz)[0]
        if len(i): pfijo[i[t.query(P[i], distance_upper_bound=VIS_CERCA)[0] <= VIS_CERCA]] = True
    for w in cunas:                                             # media del color y de la normal de lo soldado
        if w[6] > 1:
            q = np.round(w[4] / w[6]).astype(np.uint32); w[2]['c'] = q[0] << 24 | q[1] << 16 | q[2] << 8 | q[3]
            ln = np.linalg.norm(w[5]); w[3] = w[5] / ln * np.linalg.norm(w[3]) if ln > 1e-9 else w[3]
        w[2]['x'], w[2]['y'], w[2]['z'] = P[w[0]]
    cp = np.array([w[0] for w in cunas])
    X = np.zeros((nw, 11))                                      # posicion, UV, color y normal de cada cuna, con su peso
    for i, w in enumerate(cunas):
        c = int(w[2]['c']); n = np.asarray(w[3], float); ln = np.linalg.norm(n); n = n / ln if ln > 1e-9 else n
        X[i] = (*P[w[0]], VIS_UV * w[2]['u'], VIS_UV * w[2]['v'], VIS_COLOR * ((c >> 16) & 255) / 255,
                VIS_COLOR * ((c >> 8) & 255) / 255, VIS_COLOR * (c & 255) / 255, *(VIS_NORMAL * n))
    # cuadricas generalizadas por cuna; las de las aristas de borde, costura o limite de grupo (planos por la arista
    # perpendiculares a la cara, con peso VIS_BORDE): guardan la forma de esas lineas
    QA = np.zeros((nw, 11, 11)); Qb = np.zeros((nw, 11)); Qc = np.zeros(nw)
    de = [set() for _ in range(nP)]; quitar = []
    for f, t in caras.items():
        q = _cuadrica(X[t])
        if q is None: quitar.append(f); continue
        for i in t: QA[i] += q[0]; Qb[i] += q[1]; Qc[i] += q[2]; de[cp[i]].add(f)
    for f in quitar: del caras[f]
    nt0 = len(caras)
    PA = np.zeros((nP, 3, 3)); Pb = np.zeros((nP, 3)); Pc = np.zeros(nP)   # las de las aristas, por posicion
    arista = {}
    for f, t in caras.items():
        for a, b in ((0, 1), (1, 2), (2, 0)): arista.setdefault(frozenset((cp[t[a]], cp[t[b]])), []).append((f, t[a], t[b]))
    for e, L in arista.items():
        if len(L) > 2: pfijo[list(e)] = True; continue          # aristas de mas de dos caras: fijas
        if len(L) == 2 and {L[0][1], L[0][2]} == {L[1][1], L[1][2]}: continue   # interior, sin costura
        for f, a, b in L:                                       # borde, costura o limite: plano de la arista
            t = caras[f]; pa, pb = P[cp[a]], P[cp[b]]; n, _ = _normal(*P[cp[t]])
            d = pb - pa; ld = np.linalg.norm(d)
            if n is None or ld < 1e-9: continue
            m = np.cross(d / ld, n); A3 = np.outer(m, m) * VIS_BORDE * ld * ld; b3 = -A3 @ pa; c3 = pa @ A3 @ pa
            for i in (cp[a], cp[b]): PA[i] += A3; Pb[i] += b3; Pc[i] += c3
    vivo = np.ones(nP, bool); ver = np.zeros(nP, int); heap = []
    if diag: print('   simplifica: %d posiciones (%d fijas), %d cunas, %d caras, %d grupos' % (nP, pfijo.sum(), nw, nt0, len(grupos)))
    def vecinos(p):
        s = set()
        for f in de[p]: s.update(cp[caras[f]])
        s.discard(p); return s
    def bordes(p):                                              # aristas abiertas (de una cara) en p
        c = {}
        for f in de[p]:
            for i in cp[caras[f]]:
                if i != p: c[i] = c.get(i, 0) + 1
        return {i for i, n in c.items() if n == 1}
    def colapso(u, v):
        """(coste, comunes, mapa de cunas de u a cunas de v) o None si no vale"""
        comunes = [f for f in de[u] if v in cp[caras[f]]]
        if not comunes or len(comunes) > 2: return None
        bu = bordes(u)
        if bu and (len(bu) != 2 or v not in bu): return None    # en un borde, solo a lo largo de el
        if not bu and len(comunes) != 2: return None
        mapa = {}
        for f in de[u]:
            for i in caras[f]:
                if cp[i] == u: mapa.setdefault(i, set())
        for f in comunes:
            t = caras[f]; wu = [i for i in t if cp[i] == u][0]; wv = [i for i in t if cp[i] == v][0]; mapa[wu].add(wv)
        if any(len(s) != 1 for s in mapa.values()): return None   # una cuna de u sin la arista, o que ira a dos
        mapa = {a: next(iter(s)) for a, s in mapa.items()}
        if len(set(mapa.values())) != len(mapa): return None
        x = P[v]; c = float(x @ (PA[u] + PA[v]) @ x + 2 * (Pb[u] + Pb[v]) @ x + Pc[u] + Pc[v])
        for a, b in mapa.items():
            x = X[b]; c += float(x @ (QA[a] + QA[b]) @ x + 2 * (Qb[a] + Qb[b]) @ x + Qc[a] + Qc[b])
        return c, comunes, mapa
    def mete(u, v):
        if pfijo[u]: return
        r = colapso(u, v)
        if r is not None: heapq.heappush(heap, (r[0], u, v, ver[u], ver[v]))
    for u in range(nP):
        for v in vecinos(u): mete(u, v)
    objetivo = int(round(nt0 * razon)) if objetivo is None else int(objetivo); quedan = nt0; cmax = 0.0
    while heap and quedan > objetivo:
        c, u, v, vu, vv = heapq.heappop(heap)
        if c > tope:
            if diag: print('   simplifica: tope de error con %d caras' % quedan)
            break
        if not (vivo[u] and vivo[v]) or ver[u] != vu or ver[v] != vv: continue
        r = colapso(u, v)
        if r is None: continue
        _, comunes, mapa = r
        terceros = {i for f in comunes for i in cp[caras[f]]} - {u, v}
        if vecinos(u) & vecinos(v) != terceros: continue        # condicion del enlace
        nuevas = []
        for f in de[u]:
            if f in comunes: continue
            t = caras[f]; t2 = [mapa.get(i, i) for i in t]
            n0, _ = _normal(*P[cp[t]]); n1, l1 = _normal(*P[cp[t2]])
            if n0 is None or n1 is None or l1 < 1e-10 or np.dot(n0, n1) < VIS_GIRO: break
            nuevas.append((f, t2))
        else:
            for f in comunes:
                for i in caras[f]: de[cp[i]].discard(f)
                del caras[f]; quedan -= 1
            for f, t2 in nuevas: caras[f] = t2; de[v].add(f)
            de[u] = set(); vivo[u] = False
            for a, b in mapa.items(): QA[b] += QA[a]; Qb[b] += Qb[a]; Qc[b] += Qc[a]
            PA[v] += PA[u]; Pb[v] += Pb[u]; Pc[v] += Pc[u]
            ver[v] += 1; cmax = max(cmax, c)
            for w in vecinos(v): mete(v, w); mete(w, v)
    if diag: print('   simplifica: %d -> %d caras, coste maximo %.2g' % (nt0, quedan, cmax))
    # tiras nuevas de cada grupo (detras de las mallas que no se tocan)
    por_grupo = {}
    for t in caras.values(): por_grupo.setdefault(cunas[t[0]][1], []).append(t)
    nuevas = []; verts = []; n2 = 0
    for k, x in enumerate(mod.mallas):
        if k in en_grupo: continue
        verts.append(V[x[4]:x[4] + x[5]].copy()); nuevas.append(x[:4] + [n2] + x[5:]); n2 += x[5]
    for gi, ks in enumerate(grupos):
        x0 = mod.mallas[ks[0]]
        for s in tiras(por_grupo.get(gi, []), giro=giro):
            q = np.array([cunas[i][2] for i in s]); verts.append(q)
            Q = np.stack([q['x'], q['y'], q['z']], 1).astype(float); cc = Q.mean(0)
            nuevas.append([x0[0], x0[1], cc, float(np.sqrt(((Q - cc) ** 2).sum(1).max())), n2, len(q), x0[6], x0[7], x0[8],
                           dict(x0[9], N=np.array([cunas[i][3] for i in s], np.float32))])
            n2 += len(q)
    mod.mallas, mod.verts, mod.nv = nuevas, verts, n2
    return nt0, quedan

def tiras(tris, maximo=256, giro=False):
    """tiras voraces de los triangulos (tripletes de indices): desde el triangulo libre con menos vecinos libres, por la
    arista de los dos ultimos vertices hacia delante y luego hacia atras (sin descarte de caras el sentido da igual);
    de las tres formas de empezar, la que mas triangulos coge. Devuelve [indices de cada tira].
    giro (conv_trafico.py --ligero; solo para mallas opacas): cuando la tira no puede seguir por su ultima arista sigue
    por la otra arista libre del ultimo triangulo (p q r -> p q r p d: repite un vertice, y con el el triangulo p q r,
    que al ser opaco y el mismo no cambia la imagen): un vertice de mas en lugar de los dos de empezar otra tira, y una
    malla menos que recorrer en la demo"""
    import heapq
    por_arista = {}
    for f, t in enumerate(tris):
        for e in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])): por_arista.setdefault(frozenset(e), []).append(f)
    libre = [True] * len(tris)
    def grado(f):
        t = tris[f]
        return sum(1 for e in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])) for g in por_arista[frozenset(e)] if g != f and libre[g])
    def sigue(s, usados):
        while len(s) < maximo:
            cand = [g for g in por_arista.get(frozenset((s[-2], s[-1])), ()) if libre[g] and g not in usados]
            if not cand:
                if not giro or len(s) + 2 > maximo: break
                cand = [g for g in por_arista.get(frozenset((s[-3], s[-1])), ()) if libre[g] and g not in usados]
                if not cand: break
                g = min(cand, key=grado); r = [i for i in tris[g] if i != s[-3] and i != s[-1]]
                if len(r) != 1: break
                s += [s[-3], r[0]]; usados.add(g); continue
            g = min(cand, key=grado); r = [i for i in tris[g] if i != s[-2] and i != s[-1]]
            if len(r) != 1: break
            s.append(r[0]); usados.add(g)
        return s
    res = []; orden = [(grado(f), f) for f in range(len(tris))]; heapq.heapify(orden)
    while orden:
        _, f = heapq.heappop(orden)
        if not libre[f]: continue
        mejor = None
        for r in range(3):
            t = tris[f]; us = {f}; s = sigue([t[r], t[(r + 1) % 3], t[(r + 2) % 3]], us)
            s.reverse(); s = sigue(s, us)
            if mejor is None or len(us) > len(mejor[1]): mejor = (s, us)
        for g in mejor[1]: libre[g] = False
        res.append(mejor[0])
    return res

def triangulos_dibujados(mod, cerca=False):
    """triangulos con area de lo que se dibuja a la vez en carrera (como vertices_dibujados)"""
    V = np.concatenate(mod.verts); g = {}
    for x in mod.mallas:
        if not cerca and x[9].get('cerca'): continue
        i0, n = x[4], x[5]
        if n < 3: continue
        P = np.stack([V['x'][i0:i0 + n], V['y'][i0:i0 + n], V['z'][i0:i0 + n]], 1).astype(float)
        a = np.linalg.norm(np.cross(P[1:-1] - P[:-2], P[2:] - P[:-2]), axis=1)
        k = x[9].get('grupo', NADA); g[k] = g.get(k, 0) + int((a > 1e-9).sum())
    return g.get(NADA, 0) + g.get(G_FARO, 0) + max(g.get(G_PILOTOS, 0), g.get(G_NOCHE, 0), g.get(G_FRENO, 0)) + \
        max(g.get(G_DISCO, 0), g.get(G_DISCO_CAL, 0))

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

def fantasma(vols, tol=0.01):
    """GHO1 (detras de ANI1 en CEX2): el coche fantasma de la Contrarreloj (demo/main.c gh_dibuja, Rev 2.9.2.a): las
    caras de las envolventes de los trozos convexos de los volumenes de sombra (la forma del coche en ~200-400
    triangulos, sin texturas), sin las interiores (las que comparte con otro trozo con la normal opuesta: en translucido
    se verian como franjas). 'GHO1', u32 palabras del bloque, u32 triangulos; por triangulo 9 i16 (vertices en mm en el
    marco del coche) y u16 luz (0..255, de la normal): 20 bytes; relleno hasta 4"""
    from scipy.spatial import ConvexHull
    caras = []
    for k, (P, _) in enumerate(vols):
        try: H = ConvexHull(P)
        except Exception: continue
        c = P.mean(0)
        for t, eq in zip(H.simplices, H.equations):
            n, d = eq[:3], eq[3]
            T = P[t]
            if np.dot(np.cross(T[1] - T[0], T[2] - T[0]), n) < 0: T = T[[0, 2, 1]]
            caras.append((k, n, d, T))
    fuera = []
    for i, (k, n, d, T) in enumerate(caras):
        interior = any(k2 != k and np.dot(n, n2) < -0.999 and abs(d + d2) < tol for k2, n2, d2, _ in caras)
        if not interior: fuera.append((n, T))
    L = np.array([0.3, 0.8, 0.5]); L /= np.linalg.norm(L)
    datos = b''.join(np.clip(np.round(T * 1000), -32767, 32767).astype('<i2').tobytes() +
                     struct.pack('<H', int(round(255 * (0.45 + 0.55 * max(0.0, float(np.dot(n, L))))))) for n, T in fuera)
    datos += bytes(-len(datos) % 4)
    return b'GHO1' + struct.pack('<2I', 3 + len(datos) // 4, len(fuera)) + datos, len(caras), len(fuera)

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
    for x in mod.mallas: x[7] = x[9].get('pieza', P_CUERPO) << 8 | (NODO_CERCA if x[9].get('cerca') else 0) | \
        (NODO_TRASERO if x[9].get('tras') else 0)
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
    if getattr(mod, '_gho', None): pal.append(mod._gho)
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

def convierte(A, m, sal, tope=TOPE, vista=None, rebaja=False, carrera=False, visible=False, trasero=False):
    """el coche montado como el original (llamadas) y, si lo que se dibuja a la vez pasa del tope del TA, sin lo que
    no se ve desde las camaras del juego (con la vuelta baja de los choques y, si aun no cabe, sin ella). No se tapa,
    no se anade y no se cambia ninguna pieza. rebaja: variante A (rebaja_texturas); carrera: variante B (parte_carrera);
    visible: C2 (Rev 2.9.7: siempre sin lo oculto, simplifica y rebaja_texturas con la clase 'visible'; implica rebaja)"""
    mod, e, d = monta(A, m)
    t0, v0 = triangulos_n(mod), vertices_dibujados(mod); nota = 'completo'; td0 = triangulos_dibujados(mod)
    if v0 > tope:
        for baj, nota in ((True, 'sin lo oculto'), (False, 'sin lo oculto ni la vuelta baja')):
            mod, e, d = monta(A, m)
            vis, dueno = visibilidad(mod, bajas=baj, posturas=poses(A, m)); vacia(mod, vis, dueno)
            print("   %s: %d vertices" % (nota, vertices_dibujados(mod)))
            if vertices_dibujados(mod) <= tope: break
    vacia_interior(A, mod, m)
    cl = None
    if visible:
        # C2: las clases de las texturas, con el coche de siempre (lo vaciado dejaria por vistas las de los bajos); luego
        # siempre sin lo oculto (con la vuelta baja) y simplificado
        cl = clasifica_texturas(mod, True); td0 = triangulos_dibujados(mod)
        if nota == 'completo':
            mod, e, d = monta(A, m); nota = 'sin lo oculto'
            vis, dueno = visibilidad(mod, bajas=True, posturas=poses(A, m)); vacia(mod, vis, dueno)
            print("   %s: %d vertices" % (nota, vertices_dibujados(mod)))
            vacia_interior(A, mod, m)
        td1, vd1 = triangulos_dibujados(mod), vertices_dibujados(mod)
        a, b = simplifica(mod); nota += ', simplificado'
        print('   simplificado: %d -> %d triangulos simplificables; dibujados %d -> %d (vaciado) -> %d; vertices %d -> %d'
              % (a, b, td0, td1, triangulos_dibujados(mod), vd1, vertices_dibujados(mod)))
    if rebaja or visible: rebaja_texturas(mod, visible, cl)
    if carrera: parte_carrera(A, mod, m)
    tras = None
    if trasero:                                                 # (Rev 2.9.8 P3) lo que no se ve desde el cono trasero
        n0, n1 = parte_trasera(A, mod, m); malos, nc, ntr = comprueba_trasero(A, mod, m)
        print('   trasero, comprobacion: %d de %d triangulos traseros ganan algun pixel en %d camaras x posturas al azar'
              % (malos, ntr, nc))
        tras = dict(vtx_tras0=n0, vtx_tras=n1, tras_malos=malos, tras_tri=ntr)
    if vista:
        try:
            dibuja_vista(mod, os.path.join(vista, os.path.basename(sal).replace('.DAT', '.png')))
        except Exception as ex: print('vista:', ex)
    mod.recorta_texturas()
    sombra_clamp(A, mod, m)
    mod._svol = volumenes(A, m)
    mod._ani = animadas(A, mod, m)                              # piezas animadas a su marco y bloque ANI1
    mod._gho, nc, nf = fantasma(mod._svol)                      # el coche fantasma (GHO1, detras de ANI1)
    print('   fantasma: %d triangulos (%d caras de los trozos, sin las interiores)' % (nf, nc))
    mod.mallas.sort(key=lambda x: (x[0], x[1], bool(x[9].get('tras'))))   # el orden de escribe (estable): el de CEX2
    # (P3: las traseras al final de su lista y textura; escribe vuelve a ordenar por lista y textura y CEX2 va por el
    # numero de malla, asi que no pueden ir al final de la lista: con (lista, trasera, textura) la pintura, el reflejo
    # y las luces de CEX2 caian en otras mallas, visto en las capturas congeladas)
    mod.ruta = extension(mod)
    vesp = sum(x[5] for x in mod.mallas if x[9].get('esp') and x[9]['esp'][0] >= ESP_ENTORNO)
    vd = vertices_dibujados(mod); vt = vertices(mod); vc = vertices_carrera(mod); td = triangulos_dibujados(mod)
    mod.mallas = [x[:8] for x in mod.mallas]                    # fuera el nombre de la pieza y lo demas
    mod.escribe(sal)
    kb = len(mod.blob) >> 10
    print('modelo %2d %-28s tri %5d -> %5d  vtx %5d -> %5d (carrera %5d, en el fichero %5d)  tex %2d (%3d KB)  entorno %4d vtx'
          '  %s  -> %s' % (m, e.nombre, t0, triangulos_n(mod), v0, vd, vc, vt, len(mod.texs), kb, vesp, nota, os.path.basename(sal)))
    if visible: print('   dibujados: %d -> %d triangulos con area' % (td0, td))
    return dict(modelo=m, nombre=e.nombre, tri0=t0, tri=triangulos_n(mod), vtx0=v0, vtx=vd, vtx_carrera=vc, kb=kb, nota=nota,
                entorno=vesp, tdib0=td0, tdib=td, **(tras or {}))

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
    rebaja = '--rebaja' in a; carrera = '--carrera' in a; visible = '--visible' in a; trasero = '--sin-trasero' not in a
    a = [x for x in a if x not in ('--rebaja', '--carrera', '--visible', '--sin-trasero')]
    A = Assets(a[0] if a else None)
    if todos is not None:
        os.makedirs(todos, exist_ok=True)
        hechos = bytearray(b'\xff' * len(FICHA_MODELO))
        for k, mm in enumerate(FICHA_MODELO):
            if solo is not None and k not in solo: continue
            convierte(A, mm, os.path.join(todos, 'COCHE%02d.DAT' % k), tope, vista, rebaja or visible, carrera)
            if visible:                                         # (Rev 2.9.7) el ligero de la carrera, aparte
                convierte(A, mm, os.path.join(todos, 'COCHE%02dL.DAT' % k), tope, vista, True, False, True, trasero)
        for k, mm in enumerate(FICHA_MODELO):
            if os.path.exists(os.path.join(todos, 'COCHE%02d.DAT' % k)): hechos[k] = mm   # (COCHEnnL.DAT: opcional)
        with open(os.path.join(todos, 'COCHES.DAT'), 'wb') as f: f.write(b'CCH1' + bytes(hechos))
    elif m is not None:
        convierte(A, int(m), a[1], tope, vista, rebaja, carrera, visible, visible and trasero)
    else:
        print(__doc__)

if __name__ == '__main__':
    main()
