#!/usr/bin/env python3
"""conv_chr.py: el piloto y la acompanante de la carrera (DRIVER/CHR.PS2, COMMON.PS2 bone.bin y motdata_table.bin,
ANIMS.PS2 mot_*) al formato de la Dreamcast: CHR.DAT (modelo ORD7 de tools/modelo.py con las texturas y las mallas en
la postura de enlace, y el bloque RBT1 en la parte de la ruta con esqueletos, pesos, movimientos y tablas; lo lee
demo/robot.c).

Uso:
  conv_chr.py <iso> <salida CHR.DAT>      convierte
  conv_chr.py <iso> <BAND.DAT> --banderero  el banderero de la salida (ROB03) solo, en su fichero
  conv_chr.py <iso> --info                los movimientos de conducir y de mirar de cada modelo y la lista de agarre

Formato de RBT1 (todo u32 / float salvo lo dicho; desplazamientos desde el principio del bloque):
  'RBT1', n personajes (2), n movimientos, n piezas, n mallas, desp. piezas, mallas, vertices, tablas;
  por personaje {personaje, n huesos, desp. huesos (48 bytes: u8 tipo, ik, giro unidad, n hijos, 6 hijos, 2 de
  relleno, 9 floats de reposo), n inversas, desp. inversas (4x4 de fila), hueso de la mano izquierda, derecha, asiento};
  por movimiento (ordenados por id) {id, u16 cuadros, u16 banderas, desp. y bytes de la tabla de canales, desp. y bytes
  de las claves} (los bytes tal cual de mot_*_bin); piezas de 20 bytes {u8 personaje, tipo (0 rigida, 1 con pesos, 2
  puno, 3 mano abierta), n matrices, 0, u16 hueso[4], u16 inversa[4]}; por malla (el campo del nodo de cada malla del
  modelo) {u16 pieza, u16 material}; por vertice del modelo {s8 normal x 127 [3], 0, u8 peso x 255 [4]}; tablas
  {4 letras, n, n u32} hasta 'FIN ' (ver tablas_logica: MODC, MIRA, AGAR, CHQE, CHQ7, CHQ6, CERR, LISA, LISC, ASNT,
  METR, METE (la meta del piloto), MATE: difuso, ambiente y emision / 128 de cada material distinto).

Todo sale del disco al convertir (aqui no hay datos del juego). Origen de cada cosa (SLES-53998, contrastado con la
decompilacion del PC, platform/race_robots.cpp, rob_disp.cpp, rob_motion*.cpp):
 - Sucesos de los robots: tabla de 0x14 bytes {init, control, dibujo, 0, fin} en 0x3eebe4.. del ELF: el piloto (PC
   4884D0 / 488DD0 / 4879B0) = 0x201b10 / 0x201c40 / 0x202bf0; la acompanante (PC 4885A0 / 489A90) = 0x202c58 /
   0x202de0 / 0x204618. El dibujo (PS2 0x202bf0 y 0x204618, PC 4879B0) solo con car+4 & 0xC000 == 0x4000: las vistas
   exteriores de la camara (PC 4A2BA0: 0x8000 la interior); no mira el modelo: va en todos los coches, abiertos y
   cerrados (se ven por los cristales). Nada mas: no hay LOD ni distancia (el original no tiene otro modelo).
 - Personaje (work+8): indice de la tabla de ficheros \\chr\\CHR_*.bin del ELF (registros de 16 bytes {ruta, 0,
   recurso, 0} en 0x36f7d8; el cargador 0x205370 = PC 488B80). El piloto (PS2 0x205518 = PC 487EE0): 0xC en el modo 3,
   si no (sin la bandera de 0x59acac, la de la partida en red) 0xB = CHR_DR_M00 (el piloto de pelo castano). La
   acompanante (PS2 0x2055b8 = PC 487F50): en la variante 1 (OutRun) y 3, 4, 5 el 9 = CHR_DR_L00 (la chica rubia).
   Sus modelos: el recurso del registro (0xBE y 0xBF: obj_chr_dr_m00_pmt / obj_chr_dr_l00_pmt de CHR.PS2).
 - Esqueleto (PS2 0x205038 = PC 487C30): la entrada (tabla de saltos 0x40e3f0 por personaje - 3) de \\COMMON\\bone.bin:
   el piloto la 3 (53 huesos), la acompanante la 2 (50). Registros de hueso de 0x38 bytes: +04 tipo (0/1 claves, 2
   cadena con IK, 3 matriz fija, 4 claves de giro), +06 IK (2 de dos huesos, 1 apuntar), +07 giro unidad, +0C..+2C
   reposo de los 9 canales (traslacion, giro, escala), +30 hijos, +34 lista de hijos (PC 522DD0).
 - CHR_*.bin (PC 487E10 / 514280): +02 n piezas rigidas, +04 n piezas con pesos, +06 n ids, +08 rigidas {u32 hueso,
   u32 objeto}, +0C con pesos (0x14 bytes: +02 n huesos, +04 hasta 6 ids u16, +10 objeto), +10 ids, +18 matrices
   inversas de enlace (una por id). El objeto es el registro del pmt del personaje. Rigidas: v M(hueso) W; con pesos:
   paleta de hasta 4 matrices inv(enlace) M(hueso) W (514280 / PS2 0x2076b8).
 - Pesos (VU1 de SLES-53998): la clave de la malla (tabla 3 del registro, palabra 0) dice cuantas matrices: 0x200
   una, 0x202 dos (UNPACK V2-16 detras de las normales: w0 / 32767, w1 = 1 - w0), 0x203 tres (V2-16: w0, w1; w2 el
   resto), 0x204 cuatro (V3-16: w0, w1, w2; w3 el resto); despues las UV.
 - Manos (PC 513840 / 513A30 / 513BA0; PS2 0x206218 / 0x206250 con las mismas tablas): objetos del pmt en los huesos
   +50 / +54 de la obra de dibujo (rob_disp_init 514BF0: personajes 9 = 0x2D / 0x1D y 11 = 0x15 / 0x20); puno o mano
   abierta segun la lista ordenada de 34 movimientos de agarre (PS2 0x36f658, PC 6546C0; 487940).
 - Movimientos: \\COMMON\\motdata_table.bin (grupo = indice de ANIMS.PS2, id = grupo << 16 | n) y mot_*_bin: u32
   tamano y entradas de 0x18 bytes desde +4 {u16 esqueleto, u16 banderas, u16 cuadros, 0, u32 bytes de la tabla de
   canales, u32 tabla de canales, u32 ?, u32 claves} (desplazamientos desde +4; PC 520750). Motor portado de
   rob_motion_engine.cpp (522C20 Hermite, 51A770 IK, 51B420 arbol, 519DD0 mezcla). Que movimiento y cuando: la logica
   de 488DD0 / 489A90 (demo/robot.c) con las tablas del ELF de PS2 que se leen aqui (tablas()).
 - Asiento (PC 488AF0 / 46BC10, PS2 0x204f30 / 0x204ff0): T(+108 + 16 n de la disposicion del coche: n = 0 el piloto,
   1 la acompanante) sobre car+F0 y car+B0 (el cuerpo del coche: ani_m[0] de demo/main.c).
Sale de tu disco: no se distribuye."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets
from ps2 import tex, vif
import modelo

SIXTIETH = np.float32(1.0) / np.float32(60.0)
MASK = (0x20, 0x10, 0x8, 0x4, 0x2, 0x1, 0x100, 0x80, 0x40)      # canales: t x y z, r x y z, s x y z (522F70)

# ------------------------------------------------------------------ matrices de fila (pila del PC: M = op M)
def rx(a): c, s = np.cos(a), np.sin(a); return np.array([[1, 0, 0, 0], [0, c, s, 0], [0, -s, c, 0], [0, 0, 0, 1.0]])
def ry(a): c, s = np.cos(a), np.sin(a); return np.array([[c, 0, -s, 0], [0, 1, 0, 0], [s, 0, c, 0], [0, 0, 0, 1.0]])
def rz(a): c, s = np.cos(a), np.sin(a); return np.array([[c, s, 0, 0], [-s, c, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1.0]])
def tr(v): T = np.eye(4); T[3, :3] = v; return T
def sc(v): return np.diag([v[0], v[1], v[2], 1.0])

# ------------------------------------------------------------------ ficheros
class Esqueleto:
    """entrada k de bone.bin: huesos {tipo, ik, giro unidad, hijos, reposo[9], nombre}"""
    def __init__(self, bone, k):
        _, n, _, _, off, _ = struct.unpack_from('<HHHHII', bone, 16 * k)
        self.huesos = []
        for i in range(n):
            r = struct.unpack_from('<IBBBBI9fII', bone, off + 0x38 * i)
            nm = bone[r[0]:bone.index(b'\0', r[0])].decode('latin1')
            hijos = [struct.unpack_from('<H', bone, r[16] + 2 * j)[0] for j in range(r[15])]
            self.huesos.append(dict(nombre=nm, tipo=r[1], ik=r[3], unidad=r[4], hijos=hijos, reposo=list(r[6:15])))

class Grupo:
    """un mot_*_bin de ANIMS.PS2 (base = +4)"""
    def __init__(self, d):
        self.d = d; self.base = 4
    def entrada(self, k):
        e = struct.unpack_from('<HHHHIIII', self.d, self.base + 0x18 * k)
        return dict(esq=e[0], banderas=e[1], cuadros=e[2], ncan=e[4], canales=e[5], claves=e[7])
    def u16(self, o): return struct.unpack_from('<H', self.d, self.base + o)[0]
    def u8(self, o): return self.d[self.base + o]

def half(h):
    """522AA0"""
    if h == 0: return 0.0
    v = (((h & 0x7fff) + 0x1c000) << 13) | ((h & 0x8000) << 16)
    return struct.unpack('<f', struct.pack('<I', v & 0xffffffff))[0]

# ------------------------------------------------------------------ motor (rob_motion_engine.cpp)
class Canal:
    __slots__ = ('k0', 'k1', 'keys', 'left', 'stream', 'cursor', 'rest')
    def __init__(s, rest): s.k0 = [0.0] * 4; s.k1 = [0.0] * 4; s.keys = 0; s.left = 0; s.stream = 0; s.cursor = 0; s.rest = rest

class Motor:
    """un objeto RobMotion (82F5F0 + n 0xA4): esqueleto, movimiento puesto, cuadro, mezcla"""
    def __init__(s, esq, grupos):
        s.esq = esq; s.grupos = grupos; s.n = len(esq.huesos)
        s.can = [[Canal(h['reposo'][k]) for k in range(9)] for h in esq.huesos]
        s.M = [np.eye(4) for _ in range(s.n)]
        s.T = [list(h['reposo'][0:3]) for h in esq.huesos]; s.R = [list(h['reposo'][3:6]) for h in esq.huesos]
        s.S = [list(h['reposo'][6:9]) for h in esq.huesos]; s.ikt = [[0.0] * 3 for _ in range(s.n)]
        s.sT = [[0.0] * 3 for _ in range(s.n)]; s.sR = [[0.0] * 3 for _ in range(s.n)]; s.sS = [[1.0] * 3 for _ in range(s.n)]
        s.sik = [[0.0] * 3 for _ in range(s.n)]
        s.id = -1; s.fl = 0; s.f0c = s.f10 = 0.0; s.f14 = s.f1c = 0.0; s.vel = 1.0; s.bucles = 0
        s.c_id = 0; s.c_t = 0.0; s.c_f = 0.0; s.c_len = 0.0; s.blend = False; s.fac = 0.0; s.bflags = 0

    # claves
    def _key(s, g, cur):
        w = g.u16(cur); cur += 2; k = [((w & 0x1fff)) * float(SIXTIETH), 0.0, 0.0, 0.0]
        t = w >> 14
        if t == 1: k[3] = half(g.u16(cur)); cur += 2
        elif t == 2: k[3] = half(g.u16(cur)); k[1] = k[2] = half(g.u16(cur + 2)); cur += 4
        elif t == 3: k[3] = half(g.u16(cur)); k[1] = half(g.u16(cur + 2)); k[2] = half(g.u16(cur + 4)); cur += 6
        return k, cur
    def _rewind(s, ch):
        if not ch.keys: return
        ch.left = ch.keys - 2; ch.cursor = ch.stream
        ch.k0, ch.cursor = s._key(s.g, ch.cursor); ch.k0[0] = 0.0
        ch.k1, ch.cursor = s._key(s.g, ch.cursor)
    def set_motion(s, mid):
        g = s.grupos[mid >> 16]; e = g.entrada(mid & 0x7fff); s.g = g
        s.id = mid; s.bflags = e['banderas']; s.frames = e['cuadros']
        ancho = (e['banderas'] & 2) != 0
        p = e['claves']; c = e['canales']
        for i in range(s.n):
            mask = 0
            if ancho:
                if i == g.u16(c): mask = g.u16(c + 2); c += 4
            else:
                if i == g.u8(c): mask = g.u8(c + 1); c += 2
            for k in range(9):
                ch = s.can[i][k]
                if not (mask & MASK[k]): ch.keys = 0; continue
                n = 0; ch.stream = p
                while True:
                    w = g.u16(p); ini = (w & 0x2000) != 0
                    if ini and (w & 0x1fff) == 1: break
                    if n and (w & 0x1fff) == 0: break
                    p += 2; n += 1
                    if ini and (w & 0x1fff) == 0:
                        ch.k0 = [0.0, 0.0, 0.0, half(g.u16(p))]; p += 2; break
                    p += (w >> 14) * 2
                ch.keys = n
                if n >= 2: s._rewind(ch)
        s.fl &= 0xffffffc2; s.f0c = s.f10 = 0.0; s.f14 = s.f1c = float(s.frames); s.bucles = 0

    def _eval(s, ch, t):
        k0, k1 = ch.k0, ch.k1
        if ch.keys == 1: return k0[3]
        if k0[0] > t:
            if ch.left == ch.keys - 2: return k0[3]
            s._rewind(ch)
            if ch.k0[0] > t: return ch.k0[3]
        elif abs(k0[0] - t) < 1.1920929e-07: return k0[3]
        while not (ch.k1[0] > t):
            if abs(ch.k1[0] - t) < 1.1920929e-07: return ch.k1[3]
            if not ch.left: return ch.k1[3]
            ch.left -= 1; ch.k0 = ch.k1
            ch.k1, ch.cursor = s._key(s.g, ch.cursor)
        k0, k1 = ch.k0, ch.k1
        u = t - k0[0]; dt = k1[0] - k0[0]; inv = 1.0 / dt
        u2 = u * u; s2 = u2 * inv * inv; ha = s2 * 3; s2u = s2 * u; u2inv = u2 * inv
        a = s2u - u2inv; b = s2u * inv * 2
        return ((a - u2inv) + u) * k0[2] + ((b - ha) + 1) * k0[3] + (ha - b) * k1[3] + a * k1[1]
    def _eval3(s, i, k0, t):
        return [s.can[i][k].rest if s.can[i][k].keys == 0 else s._eval(s.can[i][k], t) for k in range(k0, k0 + 3)]

    # mezcla (519970 / 519CE0 / 519DD0)
    @staticmethod
    def _rzyx(a): return rx(a[0]) @ ry(a[1]) @ rz(a[2])
    @staticmethod
    def _angulos(m):
        if abs(m[2, 2]) > 1e-8:
            ax = np.arctan2(m[1, 2] / m[2, 2], 1.0)
            y = np.arctan2(-m[0, 2], m[2, 2] / np.cos(ax))
            z = np.arctan2(m[0, 1], m[0, 0]) if abs(y) < np.pi / 2 else np.arctan2(-m[0, 1], -m[0, 0])
            return [ax, y, z]
        if abs(m[1, 2]) > 1e-8: return [np.pi / 2, np.arctan2(-m[0, 2], m[1, 2]), np.arctan2(m[2, 0], -m[2, 1])]
        return [0.0, -np.pi / 2 if m[0, 2] > 0 else np.pi / 2, np.arctan2(-m[1, 0], m[1, 1])]
    def _mezcla_rot(s, ang, sav, f):
        a = s._rzyx(ang); b = s._rzyx(sav); o = np.eye(4)
        # 519970 trabaja con columnas del bloque 3x3 en memoria (filas de la matriz de fila: m[0..2][0], ...)
        X = a[:3, 0] * f + b[:3, 0] * (1 - f); Y = b[:3, 1] * (1 - f) + a[:3, 1] * f
        if np.linalg.norm(X) > 0.01: Xn = X / np.linalg.norm(X)
        else: Xn = X
        Z = np.cross(X, Y); W = np.cross(Z, X)
        o[:3, 0] = Xn
        if np.linalg.norm(W) > 0.01: o[:3, 1] = W / np.linalg.norm(W)
        if np.linalg.norm(Z) > 0.01: o[:3, 2] = Z / np.linalg.norm(Z)
        return s._angulos(o)

    def _trs(s, i, t, P):
        s.T[i] = s._eval3(i, 0, t); s.R[i] = s._eval3(i, 3, t); s.S[i] = s._eval3(i, 6, t)
        if s.blend:
            s.T[i] = [(v - w) * s.fac + w for v, w in zip(s.T[i], s.sT[i])]
            s.R[i] = s._mezcla_rot(s.R[i], s.sR[i], s.fac)
            s.S[i] = [(v - w) * s.fac + w for v, w in zip(s.S[i], s.sS[i])]
        M = tr(s.T[i]) @ P
        if s.esq.huesos[i]['unidad']: M[:3, :3] = np.eye(3)
        M = s._rzyx(s.R[i]) @ M
        if any(abs(1 - v) > 0.01 for v in s.S[i]): M = sc(s.S[i]) @ M
        return M
    def _sin_tras(s, i): return all(s.can[i][k].keys == 0 for k in range(3))
    def _escala(s, j, t, M):
        s.S[j] = s._eval3(j, 6, t)
        if s.blend: s.S[j] = [(v - w) * s.fac + w for v, w in zip(s.S[j], s.sS[j])]
        if any(abs(1 - v) > 0.01 for v in s.S[j]): M = sc(s.S[j]) @ M
        return M
    @staticmethod
    def _apunta(v):
        a, b, c = v; q = b * b + a * a; inv = 1 / np.sqrt(q)
        Z = np.eye(4); Z[0, 0] = inv * a; Z[0, 1] = inv * b; Z[1, 0] = -inv * b; Z[1, 1] = inv * a   # 40AA50(sin, cos)
        inv2 = 1 / np.sqrt(c * c + q); co = (inv * q) * inv2; sn = 0.0 - inv2 * c
        Y = np.eye(4); Y[0, 0] = co; Y[0, 2] = -sn; Y[2, 0] = sn; Y[2, 2] = co                        # 40A9B0(sin, cos)
        return Y @ Z
    def _objetivo(s, e, keys, fb, M):
        if fb & 1: p = (np.array(list(keys) + [1.0]) @ M)[:3]
        else: p = np.array(keys, float)
        s.ikt[e] = list(p)
        if s.blend: s.ikt[e] = [(v - w) * s.fac + w for v, w in zip(s.ikt[e], s.sik[e])]
        return (np.array(s.ikt[e] + [1.0]) @ np.linalg.inv(M))[:3]
    def _ik(s, i, t, fb, M):
        H = s.esq.huesos
        if H[i]['ik'] == 2:
            j0 = H[i]['hijos'][0]; j1 = H[j0]['hijos'][0]; e = H[j1]['hijos'][0]
            if s._sin_tras(e):
                s.M[j0] = s._trs(j0, t, M); s.M[j1] = s._trs(j1, t, s.M[j0]); return
            M = tr([s.can[j0][k].rest for k in range(3)]) @ M
            keys = s._eval3(e, 0, t)
            v = s._objetivo(e, keys, fb, M)
            ln = float(np.sqrt(v[2] * v[2] + v[1] * v[1] + v[0] * v[0]))
            A = s._apunta(v)
            l1 = s.can[j1][0].rest; l2 = s.can[e][0].rest; bend = H[j1]['ik']
            ca = ((l1 * l1 + ln * ln) - l2 * l2) / ((l1 * ln) * 2); ca = min(1.0, max(-1.0, ca))
            sa = np.sqrt(1 - ca * ca)
            if bend == 0: sa = -sa
            Z = np.eye(4); Z[0, 0] = ca; Z[0, 1] = sa; Z[1, 0] = -sa; Z[1, 1] = ca
            M = (Z @ A) @ M
            M = s._escala(j0, t, M); s.M[j0] = M
            M = tr([s.can[j1][k].rest for k in range(3)]) @ M
            cb = ((l1 * l1 + l2 * l2) - ln * ln) / ((l1 * l2) * 2); cb = min(1.0, max(-1.0, cb))
            sb = np.sqrt(1 - cb * cb)
            if 1 - bend == 0: sb = -sb
            Z = np.eye(4); Z[0, 0] = -cb; Z[0, 1] = sb; Z[1, 0] = -sb; Z[1, 1] = -cb
            M = Z @ M
            M = s._escala(j1, t, M); s.M[j1] = M
        elif H[i]['ik'] == 1:
            j0 = H[i]['hijos'][0]; e = H[j0]['hijos'][0]
            if s._sin_tras(e): s.M[j0] = s._trs(j0, t, M); return
            M = tr([s.can[j0][k].rest for k in range(3)]) @ M
            v = s._objetivo(e, s._eval3(e, 0, t), fb, M)
            M = s._apunta(v) @ M
            M = s._escala(j0, t, M); s.M[j0] = M
    def _hueso(s, i, t, fb, P):
        h = s.esq.huesos[i]; k = h['tipo']
        if k in (0, 1): M = s._trs(i, t, P)
        elif k == 2: M = s._trs(i, t, P); s._ik(i, t, fb, M)
        elif k == 3: M = s.M[i]
        elif k == 4:
            s.T[i] = [s.can[i][j].rest for j in range(3)]
            M = tr(s.T[i]) @ P
            if h['unidad']: M[:3, :3] = np.eye(3)
            s.R[i] = s._eval3(i, 3, t)
            if s.blend: s.R[i] = s._mezcla_rot(s.R[i], s.sR[i], s.fac)
            M = s._rzyx(s.R[i]) @ M
            M = s._escala(i, t, M)
        else: M = P
        s.M[i] = M
        for c in h['hijos']: s._hueso(c, t, fb, M)
    def huesos(s, cuadro, blend=False, fac=0.0):
        s.blend = blend; s.fac = fac
        s._hueso(0, cuadro * float(SIXTIETH), s.bflags, np.eye(4))
        s.blend = False
        return s.M

# ------------------------------------------------------------------ mallas del personaje
def mallas_registro(blq, r):
    """tiras del registro r: [(clave, P, N, pesos (n, 4), UV, k de la tabla 4)]"""
    m, A = blq.m, blq.A; reg = blq.registros()[r]; c, o = reg[3]; t2 = reg[2][1]; res = []
    for i in range(c):
        e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24])
        k = struct.unpack('<i', m[A + t2 + 16 * i:A + t2 + 16 * i + 4])[0]
        clave = e[0] & 0xff
        for pk in vif.datos(m, A + e[2], A + blq.w[2]):
            lista = pk.get('orden', []); j = 0
            while j < len(lista):
                if lista[j][0] != 0x6c: j += 1; continue
                nxt = next((q for q in range(j + 1, len(lista)) if lista[q][0] == 0x6c), len(lista))
                tira = lista[j + 1:nxt]; j = nxt
                P = tira[0][1].astype(np.float32); N = tira[1][1].astype(float) / 32768.0
                n = len(P); W = np.zeros((n, 4)); resto = tira[2:]
                if clave == 0: W[:, 0] = 1
                elif clave in (2, 3):
                    w = resto[0][1].astype(float) / 32767.0; resto = resto[1:]
                    if clave == 2: W[:, 0] = w[:, 0]; W[:, 1] = 1 - w[:, 0]
                    else: W[:, 0] = w[:, 0]; W[:, 1] = w[:, 1]; W[:, 2] = 1 - w[:, 0] - w[:, 1]
                elif clave == 4:
                    w = resto[0][1].astype(float) / 32767.0; resto = resto[1:]
                    W[:, :3] = w; W[:, 3] = 1 - w.sum(1)
                else: raise SystemExit('clave de malla 0x%x sin estudiar (registro %d)' % (e[0], r))
                uv = resto[0][1][:, :2].astype(float) / 4096.0 if resto else np.zeros((n, 2))
                res.append((clave, P, N, np.clip(W, 0, 1), uv, k, e[0]))
    return res

class Personaje:
    """CHR_*.bin + su pmt: piezas (rigidas y con pesos) y manos"""
    def __init__(s, A, idx, esq):
        E = A.elf; rec = TABLA_CHR + 16 * idx
        ruta = E.cadena(E.u32(rec)); s.recurso = E.u32(rec + 8)
        s.nombre = ruta.split('\\')[-1]
        d = A.datos(A.entrada('CHR.PS2', s.nombre))
        h = struct.unpack_from('<HHHH6I', d, 0)
        s.nid = h[3]; ids = struct.unpack_from('<%dh' % s.nid, d, h[6])
        s.ids = [i & 0xffff for i in ids]
        s.inv = [np.frombuffer(d[h[8] + 64 * i:h[8] + 64 * i + 64], '<f4').reshape(4, 4).astype(float) for i in range(s.nid)]
        s.rigidas = [struct.unpack_from('<II', d, h[4] + 8 * i) for i in range(h[1])]
        s.pesos = []
        for i in range(h[2]):
            x = struct.unpack_from('<HH6HI', d, h[5] + 0x14 * i); s.pesos.append((list(x[2:2 + x[1]]), x[8]))
        if h[9]: raise SystemExit('%s tiene cadenas osage (pelo): sin portar' % s.nombre)
        s.pmt = 'obj_' + s.nombre[:-4].lower() + '_pmt.sz'
        s.blq = A.bloque(A.entrada('CHR.PS2', s.pmt)); s.esq = esq
    def idx_id(s, b):
        return s.ids.index(b)

# ------------------------------------------------------------------ ELF
TABLA_CHR = 0x36f7d8          # se comprueba en elf() por estructura
def elf(A):
    """direcciones de SLES-53998 que usa la conversion, comprobadas por su contenido"""
    E = A.elf; g = globals()
    def cad(va):
        try: return E.cadena(E.u32(va))
        except Exception: return ''
    # tabla de ficheros de personaje: registros {ruta, 0, recurso, 0} hasta ruta 0 (488B80 / PS2 0x205370)
    for s in E.busca(b'\\chr\\CHR_AUT01.bin\0'):
        for r in E.referencias(s):
            if E.u32(r + 4) == 0 and cad(r + 16 * 16).endswith('CHR_MAL.bin'): g['TABLA_CHR'] = r; break
    if not cad(TABLA_CHR).endswith('CHR_AUT01.bin'): raise SystemExit('no se encuentra la tabla de personajes')
    return E

def _bloque_mips(E, va, n=12):
    """valores que un trozo lineal de codigo del ELF (con saltos b y su hueco) guarda con sw r, d($s0): {d: valor}.
    Solo lui / ori / addiu desde $zero; para las tablas de saltos de las manos (0x206758 / 0x2068d8) y de sus huesos
    (0x206390)"""
    reg = {0: 0}; out = {}; pc = va; destino = None
    for _ in range(n):
        w = E.u32(pc); op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; imm = w & 0xffff
        simm = imm - 0x10000 if imm & 0x8000 else imm
        salta, destino = destino, None
        if op == 0x0f: reg[rt] = imm << 16
        elif op == 0x0d and rs in reg: reg[rt] = reg[rs] | imm
        elif op == 0x09 and rs == 0: reg[rt] = simm & 0xffffffff
        elif op == 0x2b and rs == 16 and rt in reg: out[simm] = reg[rt]
        elif op == 0x04 and rs == 0 and rt == 0: destino = pc + 4 + 4 * simm          # b (beq $zero, $zero)
        elif op in (0x02, 0x03) or w == 0x03e00008: break
        pc = salta if salta is not None else pc + 4                                     # el hueco ya se ejecuto
    return out

def manos(A, kind):
    """(hueso izquierdo, hueso derecho, objetos del puno (gu), objetos de la mano abierta (pa)) del personaje kind:
    rob_disp_init (PS2 0x206390, tabla de saltos 0x40f130) y 513A30 / 513BA0 (PS2 0x206758 / 0x2068d8, tablas
    0x40f360 / 0x40f3a0); objeto = recurso << 16 | registro"""
    E = A.elf
    if kind < 5: return 0xffff, 0xffff, (), ()     # 0x206390: sin manos fuera de los personajes 5.. (el banderero)
    h = _bloque_mips(E, E.u32(0x40f130 + 4 * (kind - 5)))
    g = _bloque_mips(E, E.u32(0x40f360 + 4 * (kind - 5)))
    p = _bloque_mips(E, E.u32(0x40f3a0 + 4 * (kind - 5)))
    return h[0x50], h[0x54], (g[0x58], g[0x5c]), (p[0x58], p[0x5c])

def tablas(A):
    """tablas de la logica de 488DD0 / 489A90 en SLES-53998 (PS2 0x201c40 / 0x202de0); ver demo/robot.c"""
    E = A.elf; T = {}
    T['mov_modelo'] = list(E.u32(0x40e638, 30))         # PC 5C0B80: el movimiento de conducir de cada modelo (lb 0x11 x 4)
    T['mira'] = [list(E.u32(0x40e728 + 16 * m, 4)) for m in range(30)]   # PC 5C0C70: mirar alrededor {ida, ?, vuelta, fin}
    T['agarre'] = sorted(E.u32(0x36f658, 34))           # PC 6546C0 (qsort de 487EE0): movimientos con el puno cerrado
    return T

def tablas_logica(A):
    """valores de las tablas de 488DD0 / 489A90 que usa demo/robot.c, leidos del ELF de SLES-53998 (direcciones de
    las instrucciones que los cargan en el comentario de cada uno). Claves de 4 letras del bloque TAB de RBT1"""
    E = A.elf; T = tablas(A); t = {}
    t['MODC'] = T['mov_modelo']                                     # 0x201b60: lb car+0x11, 0x40e638
    t['MIRA'] = [x for m in range(30) for x in T['mira'][m]]       # 0x201c74: 0x40e728 + modelo x 16 {ida, -, mano en la puerta, vuelta}
    t['AGAR'] = T['agarre']                                         # 0x201aa8: busqueda binaria en 0x36f658 (34)
    # choque del piloto (0x202000..0x202150): estado = jt 0x40e230[car+2F0 >> 2 & 31 - 1] (7 o 6); variante 1:
    # estado 7 -> 0x40e938[PickJump + 2 cerrado], estado 6 -> 0x40e918[PickLand + 2 cerrado]
    jt = E.u32(0x40e230, 15)
    est = {0x202038: 7, 0x202084: 7, 0x2020d0: 6}
    t['CHQE'] = [est[x] for x in jt]
    t['CHQ7'] = list(E.u32(0x40e938, 4)); t['CHQ6'] = list(E.u32(0x40e918, 4))
    # cerrado (45B380 = PS2 0x1aa3a0, tabla de saltos 0x3efd00 por modelo - 2): 1 en los de techo (GTO, 512 BB, ...)
    jt2 = E.u32(0x3efd00, 23)
    t['CERR'] = [1 if 0 <= m - 2 < 23 and jt2[m - 2] == 0x1aa3d8 else 0 for m in range(30)]
    # listas de reacciones de la acompanante (0x202e98: variante != 2, cerrado ? 0x40eb68 : 0x40eb28): 8 pares
    # {lista, cuenta}; se guardan como 8 x {cuenta, 4 ids}
    for clave, base in (('LISA', 0x40eb28), ('LISC', 0x40eb68)):
        v = []
        for k in range(8):
            p, n = E.u32(base + 8 * k, 2)
            ids = [E.u32(p + 4 * j) for j in range(min(n, 4))] if p else []
            v += [n] + ids + [0xffffffff] * (4 - len(ids))
        t[clave] = v
    # asientos (46BC10: +108 + 16 n de la disposicion del modelo; 0 el piloto, 1 la acompanante), floats
    import conv_coche
    t['ASNT'] = [struct.unpack('<I', struct.pack('<f', float(x)))[0]
                 for m in range(30) for a in conv_coche.disposicion(A, m)[0]['asientos'] for x in a]
    # meta del piloto (estado 9 de 488DD0; PS2 0x20250c..0x2027cc): fila del modelo (lb car+0x11, 0x40e9b8) y por
    # clase de curso (44DC50 = 0x176700 de la clave: 0x1b -> 0x40e978, 0x1a / 0x1c -> 0x40e998, si no 0x40e958) 4 filas
    # de 2 registros {id, bucle, fin} (puntero 0: sin registro -> 0xFFFFFFFE en los tres)
    t['METR'] = list(E.u32(0x40e9b8, 30))
    v = []
    for base in (0x40e958, 0x40e978, 0x40e998):
        for k in range(8):
            r = E.u32(base + 4 * k)
            v += [0xfffffffe] * 3 if r == 0 else [x & 0xffffffff for x in E.u32(r, 3)]
    t['METE'] = v
    # banderero (ROB03; PS2 init 0x2046f0, control 0x204888, huida 0x204ca0, dibujo 0x204c28; preset [gp-0x7b64]):
    # por preset 0..4 {movimiento inicial 0x36f778, huida 0x36f790 (si sigue en el inicial) / 0x36f7a4, lista de 6
    # 0x36f700 + 0x18 p, posicion 0x40ef88 + 12 p, la de los siguientes 0x40efc8 + 12 p, giro 0x40f008 + 12 p (x, y,
    # z: se aplica z, y, x)}; y {distancia de dibujo 0x36f7c4, de huida 0x36f7c8, minima 0x36f7cc, eje 0x40e3c8 (3),
    # punto 0x40e3d8 (3)} (floats como u32)
    v = []
    for p in range(5):
        v += [E.u32(0x36f778 + 4 * p), E.u32(0x36f790 + 4 * p), E.u32(0x36f7a4 + 4 * p)] + list(E.u32(0x36f700 + 0x18 * p, 6))
        v += list(E.u32(0x40ef88 + 12 * p, 3)) + list(E.u32(0x40efc8 + 12 * p, 3)) + list(E.u32(0x40f008 + 12 * p, 3))
    v += list(E.u32(0x36f7c4, 3)) + list(E.u32(0x40e3c8, 3)) + list(E.u32(0x40e3d8, 3))
    if BANDERERO: t['BAND'] = [x & 0xffffffff for x in v]
    return t

def ids_movimientos(t):
    """movimientos que usa la logica (los de las tablas y las constantes de 488DD0 / 489A90, comprobadas en el codigo
    de PS2: 0x60000 0x201e5c, 0x25000c 0x202d04, 0x6004b 0x202cd4, 0x6005f 0x203224, 0x250003 0x20346c, 0x220007 /
    0x230003 0x203510.., 0x60036 0x203088, 0x60049 / 0x25000c 0x204128 / 0x2040e8, 0x60055 0x203df8)"""
    if BANDERERO:                                   # BAND.DAT: los del banderero (personaje 4, grupo 0x21)
        return sorted(set(x for p in (0, 2, 4) for x in t['BAND'][18 * p:18 * p + 9] if x))
    s = set(x for x in t['MODC'] + t['MIRA'] + t['CHQ7'] + t['CHQ6'] if x != 0xffffffff)
    s |= {0x60000, 0x25000c, 0x6004b, 0x6005f, 0x250003, 0x220007, 0x230003, 0x60036, 0x60049}
    s |= set(x for x in t['METE'] if x not in (0xffffffff, 0xfffffffe))      # meta del piloto (estado 9)
    for c in ('LISA', 'LISC'):
        v = t[c]
        for k in range(8): s |= set(x for x in v[5 * k + 1:5 * k + 5] if x != 0xffffffff)
    return sorted(s)

def datos_movimiento(g, k):
    """(cuadros, banderas, tabla de canales, claves) de la entrada k del grupo g: los bytes que lee 520750 / 522EE0"""
    e = g.entrada(k)
    ancho = (e['banderas'] & 2) != 0
    c = e['canales']; p = e['claves']; n = 0
    while True:                                     # recorre las claves como 522EE0 para saber donde acaban
        if ancho: i, mask = struct.unpack_from('<HH', g.d, g.base + c + 4 * n) if 4 * n < e['ncan'] else (None, 0)
        else: i, mask = (g.u8(c + 2 * n), g.u8(c + 2 * n + 1)) if 2 * n < e['ncan'] else (None, 0)
        if i is None: break
        n += 1
        for kk in range(9):
            if not (mask & MASK[kk]): continue
            m = 0
            while True:
                w = g.u16(p); ini = (w & 0x2000) != 0
                if ini and (w & 0x1fff) == 1: break
                if m and (w & 0x1fff) == 0: break
                p += 2; m += 1
                if ini and (w & 0x1fff) == 0: p += 2; break
                p += (w >> 14) * 2
    fin = min(len(g.d) - g.base, p + 2)
    return e['cuadros'], e['banderas'], bytes(g.d[g.base + c:g.base + c + e['ncan']]), bytes(g.d[g.base + e['claves']:g.base + fin])

# ------------------------------------------------------------------ conversion
# --banderero: BAND.DAT, el mismo formato con solo el banderero de la salida (en CHR.DAT no cabe en la RAM: la demo lo
# lee al empezar la carrera en el hueco de la union; ver docs/PARIDAD.md, Escena de la salida)
BANDERERO = '--banderero' in sys.argv
PERSONAJES = ((4, 4, 0xff),) if BANDERERO else ((11, 3, 0), (9, 2, 1))
# (personaje, esqueleto de bone.bin, asiento): el piloto, la acompanante y el banderero de la salida (ROB03: PS2
# 0x2046f0 = PC 488710 pone el personaje 4 = CHR_AUT04_CVT con el preset 0, 2 y 4; 0x205038 le da la entrada 4 de
# bone.bin por la tabla de saltos 0x40e3f0; sin asiento: va en el mundo)

def convierte(A, sal, vista=None):
    elf(A)
    bone = A.datos(A.entrada('COMMON.PS2', 'bone.bin'))
    grupos = {}
    def grupo(k):
        if k not in grupos: grupos[k] = Grupo(A.datos(A.paquete('ANIMS.PS2')[k]))
        return grupos[k]
    t = tablas_logica(A)
    mod = modelo.Modelo(); mod.vq = True
    piezas = []; mvert = []                       # piezas; por malla: (pieza, P, N, W, material)
    texbase = {}
    pers = []
    for kind, ek, asiento in PERSONAJES:
        esq = Esqueleto(bone, ek); P = Personaje(A, kind, esq)
        hl, hr, gu, pa = manos(A, kind)
        for o in gu + pa:
            if o >> 16 != P.recurso: raise SystemExit('mano de otro recurso: %x' % o)
        pers.append((kind, esq, P, hl, hr, gu, pa, asiento))
        blq = P.blq; base = len(mod.texs)
        for h in tex.cabeceras(blq):
            a = tex.imagen(blq, h); fmt, li, d = modelo.textura_dc(a)
            mod.texs.append((a.shape[1], a.shape[0], fmt, li, len(mod.blob), len(d))); mod.blob += d; mod.rgba.append(a)
        ntb = len(mod.texs) - base
        if kind == 4:                             # la tela de la bandera: textura 1 del recurso (401B80 / 448810:
            if ntb < 2: raise SystemExit('%s sin la textura de la bandera' % P.nombre)    # [[r+24]][1])
            mod.mallas.append([mod.texs[base + 1][3], base + 1, np.zeros(3), 0.0, mod.nv, 0, blq, -1])
        R = blq.registros()
        lista_p = []                              # (registro, tipo, huesos, inversas)
        for b, obj in P.rigidas:
            if b & 0x8000: raise SystemExit('hueso de cadena osage en %s' % P.nombre)
            lista_p.append((obj & 0xffff, 0, [b], [0xffff]))
        for bs, obj in P.pesos:
            lista_p.append((obj & 0xffff, 1, bs, [P.idx_id(b) for b in bs]))
        for j, o in enumerate(gu): lista_p.append((o & 0xffff, 2, [(hl, hr)[j]], [0xffff]))
        for j, o in enumerate(pa): lista_p.append((o & 0xffff, 3, [(hl, hr)[j]], [0xffff]))
        for r, tipo, bs, inv in lista_p:
            ip = len(piezas); piezas.append((len(pers) - 1, tipo, bs, inv, r))
            for clave, Pp, N, W, uv, k, key in mallas_registro(blq, r):
                if clave and len(bs) < clave: raise SystemExit('malla de %d huesos en una pieza de %d' % (clave, len(bs)))
                reg = R[r]
                tl = struct.unpack('<i', blq.m[blq.A + reg[4][1] + 36 * k + 12:blq.A + reg[4][1] + 36 * k + 16])[0] \
                    if 0 <= k < reg[4][0] else -1
                ti = base + tl if 0 <= tl < ntb else -1
                mat = modelo.material_completo(blq, reg, k)
                li = mod.texs[ti][3] if ti >= 0 else 0      # la lista de la textura (como los coches: modelo._trozo)
                dib = vif.triangulos_dibujados(Pp); j = 0; n = len(dib)
                while j < n:                       # tiras que dibuja de verdad el GS (bandera ADC, tools/modelo.py)
                    if not dib[j]: j += 1; continue
                    a0 = j
                    while j < n and dib[j]: j += 1
                    sl = slice(a0, j + 2); nn = j + 2 - a0
                    v = np.zeros(nn, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
                    q = Pp[sl].astype(np.float32); v['x'], v['y'], v['z'] = q[:, 0], q[:, 1], q[:, 2]
                    v['u'], v['v'] = uv[sl, 0], uv[sl, 1]; v['c'] = 0xFFFFFFFF
                    cc = q.mean(0); rad = float(np.sqrt(((q - cc) ** 2).sum(1).max()))
                    mod.mallas.append([li, ti, cc, rad, mod.nv, nn, blq, len(mvert)])
                    mod.verts.append(v); mod.nv += nn
                    mvert.append((ip, q, N[sl], W[sl], mat))
    # texturas: solo las usadas, en VQ si cabe (como los coches)
    mod.recorta_texturas()
    if BANDERERO: t['BTEX'] = [x[1] for x in mod.mallas if x[5] == 0 and x[7] == -1][:1]   # la tela (tras recortar)
    mod.mallas.sort(key=lambda x: (x[0], x[1]))
    # bloque RBT1
    blob = bytearray()
    def al(n=4):
        while len(blob) % n: blob.append(0)
    secs = {}
    # movimientos
    ids = ids_movimientos(t)
    movs = []
    for mid in ids:
        fr, fl, can, cla = datos_movimiento(grupo(mid >> 16), mid & 0x7fff)
        al(4); oc = len(blob); blob += can; al(2); ok = len(blob); blob += cla
        movs.append((mid, fr, fl, oc, len(can), ok, len(cla)))
    # personajes: huesos e inversas
    per_hdr = []
    for kind, esq, P, hl, hr, gu, pa, asiento in pers:
        al(4); oh = len(blob)
        hijos_all = []
        for h in esq.huesos:
            if len(h['hijos']) > 6: raise SystemExit('hueso con mas de 6 hijos')
            hj = h['hijos'] + [0xff] * (6 - len(h['hijos']))
            blob += struct.pack('<4B6B2x9f', h['tipo'], h['ik'], h['unidad'], len(h['hijos']), *hj, *h['reposo'])
        al(4); oi = len(blob)
        for M in P.inv: blob += M.astype('<f4').tobytes()
        per_hdr.append((kind, len(esq.huesos), oh, len(P.inv), oi, hl, hr, asiento))
    # piezas
    al(4); opz = len(blob)
    for per, tipo, bs, inv, r in piezas:
        b4 = list(bs) + [0xffff] * (4 - len(bs)); i4 = list(inv) + [0xffff] * (4 - len(inv))
        blob += struct.pack('<4B4H4H', per, tipo, len(bs), 0, *b4, *i4)
    # por malla (indice = campo del nodo): pieza y material (difuso, ambiente y emision / 128, una vez cada uno: la
    # demo hace por cuadro una tabla de color por material); por vertice (en el orden de mod.verts) normal y pesos
    mats = []
    al(4); oml = len(blob)
    for idx in range(len(mvert)):                  # en el orden de creacion: el campo del nodo de cada malla
        ip, q, N, W, mat = mvert[idx]
        md = (mat[0:3] / 128) if mat is not None else np.ones(3); ma = (mat[4:7] / 128) if mat is not None else np.ones(3)
        me = (mat[12:15] / 128) if mat is not None else np.zeros(3)
        clave = tuple(np.float32(np.r_[md, ma, me]))
        if clave not in mats: mats.append(clave)
        blob += struct.pack('<HH', ip, mats.index(clave))
    t['MATE'] = [struct.unpack('<I', struct.pack('<f', float(x)))[0] for c in mats for x in c]
    al(4); ovt = len(blob)
    for idx in range(len(mvert)):                  # normal x 127 y pesos x 255 (la posicion y las UV, en mod.verts)
        ip, q, N, W, mat = mvert[idx]
        n8 = np.clip(np.round(N * 127), -127, 127).astype(np.int8)
        w8 = np.clip(np.round(W * 255), 0, 255).astype(np.uint8)
        for i in range(len(q)):
            blob += struct.pack('<3bx4B', *n8[i], *w8[i])
    # tablas
    al(4); otb = len(blob)
    if BANDERERO: t = {k: t[k] for k in ('BAND', 'BTEX', 'MATE')}
    for k, v in t.items():
        blob += k.encode() + struct.pack('<I', len(v)) + struct.pack('<%dI' % len(v), *[x & 0xffffffff for x in v])
    blob += b'FIN ' + struct.pack('<I', 0)
    # cabecera
    cab = bytearray(b'RBT1') + struct.pack('<7I', len(pers), len(movs), len(piezas), len(mod.mallas), opz, oml, ovt) + \
        struct.pack('<I', otb)
    for x in per_hdr: cab += struct.pack('<8I', *x)
    for x in movs: cab += struct.pack('<IHHIIII', *x)
    off = len(cab)
    # los desplazamientos de blob van tras la cabecera: se guardan relativos al principio del bloque
    cab = bytearray(cab)
    def corrige(o): return o + off
    cab[20:36] = struct.pack('<4I', corrige(opz), corrige(oml), corrige(ovt), corrige(otb))
    p = 36
    for x in per_hdr:
        y = list(x); y[2] = corrige(y[2]); y[4] = corrige(y[4]); cab[p:p + 32] = struct.pack('<8I', *y); p += 32
    for x in movs:
        y = list(x); y[3] = corrige(y[3]); y[5] = corrige(y[5]); cab[p:p + 24] = struct.pack('<IHHIIII', *y); p += 24
    bloque = bytes(cab) + bytes(blob)
    bloque += b'\0' * (-len(bloque) % 4)
    mod.ruta = np.frombuffer(bloque, '<f4').reshape(-1, 1)
    mod.escribe(sal)
    nvd = sum(x[5] for x in mod.mallas)
    print('%s: %d mallas, %d vertices, %d texturas (%d KB), %d movimientos, RBT1 %d KB' % (
        sal, len(mod.mallas), nvd, len(mod.texs), len(mod.blob) >> 10, len(movs), len(bloque) >> 10))
    return mod

# ------------------------------------------------------------------ informacion
def info(A):
    E = elf(A); T = tablas(A)
    t = A.datos(A.entrada('COMMON.PS2', 'motdata_table.bin'))
    def cad(o): return t[o:t.index(b'\0', o)].decode()
    def nombre(mid):
        n, l, c, _ = struct.unpack_from('<4I', t, 16 * (mid >> 16))
        return cad(struct.unpack_from('<I', t, l + 4 * (mid & 0xffff))[0]) if (mid & 0xffff) < c else '?'
    print('tabla de personajes 0x%x' % TABLA_CHR)
    for m in range(15): print('modelo %2d conduce %08x %s  mira %s' % (m, T['mov_modelo'][m], nombre(T['mov_modelo'][m]),
                                                                   ' '.join('%08x' % x for x in T['mira'][m])))
    print('agarre', ' '.join(nombre(x) for x in T['agarre']))

if __name__ == '__main__':
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(1)
    A = Assets(a[0])
    if '--info' in a: info(A); sys.exit(0)
    convierte(A, a[1])
