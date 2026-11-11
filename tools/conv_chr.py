#!/usr/bin/env python3
"""conv_chr.py: el piloto y la acompanante de la carrera (DRIVER/CHR.PS2, COMMON.PS2 bone.bin y motdata_table.bin,
ANIMS.PS2 mot_*) al formato de la Dreamcast: CHRppaa.DAT, una por pareja (modelo ORD7 de tools/modelo.py con las texturas y las mallas en
la postura de enlace, y el bloque RBT1 en la parte de la ruta con esqueletos, pesos, movimientos y tablas; lo lee
demo/robot.c).

Uso:
  conv_chr.py <iso> <dir> --pares         una CHRppaa.DAT por pareja que puede ir en la carrera (pares(): piloto pp y
                                          acompanante aa, indices de la tabla de personajes en decimal) y CHRPAR.DAT
                                          (las tablas que eligen la pareja: tabla_pares)
  conv_chr.py <iso> --lista               los nombres de esos ficheros
  conv_chr.py <iso> <salida> [--par p,a]  una pareja (por defecto 11,9: la de OUTRUN2SP)
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
  METR, METE (la meta del piloto), MATE: difuso, ambiente y emision / 128 de cada material distinto, OSG0 / OSG1:
  el osage del personaje 0 / 1 si lo tiene, ver osage_tablas). Los huesos 0x8000 | i de las piezas (nodos del osage)
  se guardan como n huesos + i (robot.c deja ahi sus matrices).

Todo sale del disco al convertir (aqui no hay datos del juego). Origen de cada cosa (SLES-53998, contrastado con la
decompilacion del PC, platform/race_robots.cpp, rob_disp.cpp, rob_motion*.cpp):
 - Sucesos de los robots: tabla de 0x14 bytes {init, control, dibujo, 0, fin} en 0x3eebe4.. del ELF: el piloto (PC
   4884D0 / 488DD0 / 4879B0) = 0x201b10 / 0x201c40 / 0x202bf0; la acompanante (PC 4885A0 / 489A90) = 0x202c58 /
   0x202de0 / 0x204618. El dibujo (PS2 0x202bf0 y 0x204618, PC 4879B0) solo con car+4 & 0xC000 == 0x4000: las vistas
   exteriores de la camara (PC 4A2BA0: 0x8000 la interior); no mira el modelo: va en todos los coches, abiertos y
   cerrados (se ven por los cristales). Nada mas: no hay LOD ni distancia (el original no tiene otro modelo).
 - Personaje (work+8): indice de la tabla de ficheros \\chr\\CHR_*.bin del ELF (registros de 16 bytes {ruta, 0,
   recurso, 0} en 0x36f7d8; el cargador 0x205370 = PC 488B80). Quienes: pares() (PS2 0x205518 = PC 487EE0 y
   0x2055b8 = 487F50): en OUTRUN2SP 0xB = CHR_DR_M00 y 9 = CHR_DR_L00 (6 = CHR_DR_G00_USA en las variantes 2, 6..);
   en el juego de la consola la licencia (+0x20) elige entre CHR_DR_M00 / S00 / W00 / L00 / G00 / H00 (11, 18, 17,
   9, 5, 19). Sus modelos: el recurso del registro (p. ej. 0xBE y 0xBF: obj_chr_dr_m00_pmt / obj_chr_dr_l00_pmt de
   CHR.PS2; los de S00 y W00 con registros de 64 bytes, Pmt64). G00 y G00_USA llevan osage (dos cadenas de pelo).
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
    z2 = 12 if getattr(blq, 'zancada64', False) else 16          # registros de la tabla 2 (Pmt64.material)
    for i in range(c):
        e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24])
        k = struct.unpack('<i', m[A + t2 + z2 * i:A + t2 + z2 * i + 4])[0]
        clave = e[0] & 0xff; color = False
        if clave & 0xf0 in (0x20, 0x60) and clave & 0xf in (0, 2, 3, 4, 8, 0xa, 0xb, 0xc):
            # 0x22n / 0x26n (S00, W00): las mismas UNPACK que 0x20n; con 8 (0x228, 0x22a..0x22c) ademas un V4-8 detras
            # de las posiciones (0x6e) y n = 8 una matriz. Otro microcodigo (tabla 0x3592b0: 0x202 -> 0x31edb0, 0x262 y
            # 0x228 -> 0x315760; 0x203 / 0x223 / 0x243 el mismo 0x313db0): lo que cambie de la luz, SIN PORTAR
            color = (clave & 8) != 0; clave = {0: 0, 2: 2, 3: 3, 4: 4, 8: 0, 0xa: 2, 0xb: 3, 0xc: 4}[clave & 0xf]
        for pk in vif.datos(m, A + e[2], A + blq.w[2]):
            lista = pk.get('orden', []); j = 0
            while j < len(lista):
                if lista[j][0] != 0x6c: j += 1; continue
                nxt = next((q for q in range(j + 1, len(lista)) if lista[q][0] == 0x6c), len(lista))
                tira = lista[j + 1:nxt]; j = nxt
                if color:                         # el V4-8 de 0x22[8a-c]: en SLES-53998 siempre (0, 0, 0, 127); si no,
                    if tira[1][0] != 0x6e or (tira[1][1] != (0, 0, 0, 127)).any():   # sin estudiar
                        raise SystemExit('registro %d: color por vertice distinto de (0, 0, 0, 127): sin portar' % r)
                    tira = tira[:1] + tira[2:]    # PROBABLE: sin efecto (el mismo en todos los vertices)
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

class Pmt64:
    """bloque con registros de 64 bytes (8 tablas, sin las 2 palabras de detras): obj_chr_dr_s00_pmt y
    obj_chr_dr_w00_pmt de CHR.PS2. Se reconoce por la tabla de texturas: su primer desplazamiento es el de la cabecera
    que va justo detras (zancada x n registros + 4 x n texturas; con 72 bytes en el resto). PROBABLE: el cargador
    que se ha mirado (0x118aa0) recoloca siempre de 0x48 en 0x48; con esa zancada estos dos no tienen sentido (tablas
    cruzadas) y con 64 todo cuadra (mallas, texturas, piezas del CHR_*.bin)"""
    @staticmethod
    def zancada(b):
        n, nt = b.w[0], b.w[1]
        ok = [z for z in (72, 64) if struct.unpack_from('<I', b.m, b.A + z * n)[0] == z * n + 4 * nt]
        if len(ok) != 1: raise SystemExit('bloque de personaje sin zancada clara de registros: %s' % ok)
        return ok[0]
    @classmethod
    def de(cls, b):
        if cls.zancada(b) == 72: return b
        b.registros = lambda: [[struct.unpack_from('<2I', b.m, b.A + 64 * i + 8 * k) for k in range(8)] for i in range(b.w[0])]
        b.zancada64 = True
        return b
    @staticmethod
    def material(b, reg, k, base, ntb):
        """(tl, ti, material) de la entrada k de la tabla 4 de un Pmt64. Tablas de S00 y W00 (las mismas parejas
        cuenta / desplazamiento): 2 con registros de 12 bytes (palabra 0: la entrada de la tabla 4, como la de 16 de
        los otros), 4 de 32 bytes {material, 0x5401 / 0x5407, 0x3040200, 1, 0x5040000, -0x10000, 0x6040000, -0x10000}
        y 5 de 72 bytes {difuso 4, ambiente 4, especular 4, emision 4, potencia, 0} en unidades de 1 (los de 72 bytes
        de registro: 64 en unidades del GS, la potencia en la palabra 11). x 128 queda como los otros (el de M00 es el
        de S00 x 128: difuso 1 -> 128, ambiente 1,3 -> 166,4, especular 0,1 -> 12,8, emision 0,2 -> 25,6; potencia 20
        los dos). Textura: una sola en los dos ficheros y la palabra 3 de la tabla 4 a 1 en todas: PROBABLE que sea
        "con textura" (la unica)"""
        n4, o4 = reg[4]; n5, o5 = reg[5]
        if not 0 <= k < n4: return -1, -1, None
        e = struct.unpack_from('<8I', b.m, b.A + o4 + 32 * k)
        if e[3] != 1 or ntb != 1 or not 0 <= e[0] < n5: raise SystemExit('Pmt64: entrada de la tabla 4 sin estudiar %s' % [hex(x) for x in e])
        v = np.array(struct.unpack_from('<18f', b.m, b.A + o5 + 72 * e[0]), float)
        mat = np.r_[v[0:11], v[16], v[12:16]] * 128.0; mat[11] = v[16]
        return 0, base, mat

def cabeceras(blq):
    """tex.cabeceras con la zancada del bloque (Pmt64)"""
    if not getattr(blq, 'zancada64', False): return tex.cabeceras(blq)
    m, A = blq.m, blq.A; base = A + 64 * blq.w[0]; res = []
    for o in struct.unpack('<%dI' % blq.w[1], m[base:base + 4 * blq.w[1]]):
        if o >= blq.w[2] or o < 64 * blq.w[0]: break
        res.append(struct.unpack('<14I', m[A + o:A + o + 56]))
    return res

def osage(d, o):
    """cadenas osage (el pelo con fisica) de un CHR_*.bin: cabecera en +1C (PC 514F60 / 515040, PS2 0x208b58 /
    0x208ca8) {n cadenas, ?, registros, parametros, indices}; registro de 0x24 {hueso, indice, n nodos, posicion,
    giro x y z}; parametros {id | 0x8000, largo} por nodo (5229A0 = PS2 0x290890: el nodo k de cada cadena toma el
    largo k desde el principio, todas las cadenas desde el mismo); indices: u32 por nodo hacia la tabla de esferas
    del personaje (515040)"""
    n, _, orr, opa, oix = struct.unpack_from('<5I', d, o)
    cad = []
    for i in range(n):
        r = struct.unpack_from('<3I6f', d, orr + 0x24 * i)
        cad.append(dict(hueso=r[0], indice=r[1], n=r[2], pos=list(r[3:6]), giro=list(r[6:9])))
    largo = lambda k: struct.unpack_from('<f', d, opa + 4 + 8 * k)[0]
    nidx = max(c['indice'] + c['n'] for c in cad) if cad else 0
    return dict(cadenas=cad, largos=[[largo(k) for k in range(c['n'])] for c in cad],
                indices=list(struct.unpack_from('<%dI' % nidx, d, oix)), nodos=sum(c['n'] for c in cad))

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
        s.osage = osage(d, h[9]) if h[9] else None
        s.pmt = 'obj_' + s.nombre[:-4].lower() + '_pmt.sz'
        s.blq = Pmt64.de(A.bloque(A.entrada('CHR.PS2', s.pmt))); s.esq = esq
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

def esqueleto_de(E, kind):
    """entrada de bone.bin del personaje: 487C30 = PS2 0x205038 salta por 0x40e3f0[kind - 3] (17 casos; fuera, el
    0x2050e8) a un trozo que deja la entrada en $v1 (addiu $v1, $zero, n o move $v1, $zero en el hueco del b)"""
    va = E.u32(0x40e3f0 + 4 * (kind - 3)) if 0 <= kind - 3 < 17 else 0x2050e8
    for k in range(4):
        w = E.u32(va + 4 * k); op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; rd = (w >> 11) & 31
        if op == 0x09 and rs == 0 and rt == 3: return w & 0xffff
        if op == 0 and (w & 0x3f) in (0x21, 0x25, 0x2d) and rd == 3 and rs == 0 and rt == 0: return 0
    raise SystemExit('0x205038: no se encuentra la entrada de bone.bin del personaje %d' % kind)

# osage (pelo con fisica: rob_osage, PS2 0x208b58 init / 0x208ca8 control / 0x290890 cadena / 0x290a60 colocar /
# 0x290c28 paso / 0x290e60 matrices; PC 514F60 / 515040 / 5229A0 / 522810 / 522620 / 522450): constantes del ELF
OSG_GRAVEDAD = 0x328708      # lwc1 -0x78f8(0x330000) en 0x290b38 (colocar) y 0x2910ec (5223B0 = PS2 0x2910b8)
OSG_AMORTIGUA = 0x36fb08     # lwc1 -0x4f8(0x370000) en 0x208c48 (init: +24 de los nodos 1.., 5222B0 = 0x290f98)
OSG_CALIENTA = 0x378368      # lw -0x7c98(0x380000) en 0x290bcc: pasos tras colocar (PC 722D40)
OSG_EJE = 0x422990           # 0x290a90 / 0x290c5c: el vector que gira el registro (1, 0, 0)
OSG_VIENTO = 0x36fb20        # addiu en 0x208f58 -> 0x291038: chain+14 (nadie lo escribe en el ELF)
OSG_UNICO = 0x36fb10         # lw en 0x208d78 / 0x2018d8 (PC 85A714; nadie lo escribe: 0)
OSG_SALTOS = 0x40f680        # 0x208d1c: tabla de saltos por personaje - 5 (11 casos)
OSG_GUARDA = 0x36fc30        # 0x208eb0: punteros a las esferas de cada hueco (0x291058: nodos 1.. +28)
def osage_tablas(E, kind, osg):
    """bloque OSG de RBT1 para el personaje (u32): {n cadenas, n nodos, pasos de calentar, gravedad, amortiguacion,
    eje (3), viento (3)}; por cadena {hueso, n, posicion (3), giro (3), primer nodo}; por nodo {largo, n esferas,
    3 x {hueso, tipo, radio, centro (3)}} (514EC0 = PS2 0x209078: hueso -1 = centro en el marco del personaje)"""
    f = lambda va: E.u32(va)
    if E.u32(OSG_UNICO): raise SystemExit('osage: 0x36fb10 != 0 en el ELF (lista unica de esferas): sin portar')
    defs = None
    if 0 <= kind - 5 < 11:
        caso = E.u32(OSG_SALTOS + 4 * (kind - 5))
        if caso == 0x208d38: defs = 0x40f6b0          # lui 0x41 / addiu -0x950: esferas, sin lista de amortiguacion
        elif caso != 0x208d74:                         # (0x208d74: sin esferas)
            raise SystemExit('osage: personaje %d con lista de amortiguacion (0x208d48 / 0x208d5c): sin portar' % kind)
    g0 = E.u32(OSG_GUARDA)
    v = [len(osg['cadenas']), osg['nodos'], E.u32(OSG_CALIENTA), f(OSG_GRAVEDAD), f(OSG_AMORTIGUA)]
    v += list(E.u32(OSG_EJE, 3)) + list(E.u32(OSG_VIENTO, 3))
    if [struct.unpack('<f', struct.pack('<I', x))[0] for x in E.u32(OSG_EJE, 3)] != [1.0, 0.0, 0.0]:
        raise SystemExit('osage: el eje de 0x422990 no es (1, 0, 0)')
    primero = 0
    for c in osg['cadenas']:
        v += [c['hueso'], c['n']] + [struct.unpack('<I', struct.pack('<f', x))[0] for x in c['pos'] + c['giro']] + [primero]
        primero += c['n']
    hueco = 0
    for ci, c in enumerate(osg['cadenas']):
        for k in range(c['n']):
            if E.u32(OSG_GUARDA + 4 * hueco) != g0 + 0x70 * hueco: raise SystemExit('osage: 0x36fc30 no va de 0x70 en 0x70')
            esf = []
            if defs:
                p = E.u32(defs + 4 * osg['indices'][c['indice'] + k])
                while p and E.u32(p + 4):
                    b, tipo = E.u32(p, 2); esf.append([b, tipo, E.u32(p + 8)] + list(E.u32(p + 0xc, 3))); p += 0x20
            if len(esf) > 3: raise SystemExit('osage: mas de 3 esferas en un hueco de 0x70')
            v += [struct.unpack('<I', struct.pack('<f', osg['largos'][ci][k]))[0], len(esf)]
            for e in esf + [[0] * 6] * (3 - len(esf)): v += e
            hueco += 1
    return v

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
    if BANDERERO:                                   # BAND.DAT / BAND1.DAT: los del banderero de esos presets
        return sorted(set(x for p in BAND_PRESETS for x in t['BAND'][18 * p:18 * p + 9] if x))
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
BANDERERO = '--banderero' in sys.argv or '--banderero-sp' in sys.argv
# --banderero-sp: BAND1.DAT, el de los presets 1 y 3 (cursos de OutRun 2 SP): 0x2046f0 (PS2) / 488710 (PC) pone el
# personaje 3 = CHR_AUT04 (recurso 0xC3, el de la tela que pide 0x100d40 con el preset 1 o 3) y 0x205038 le da la
# misma entrada 4 de bone.bin (tabla de saltos 0x40e3f0: personajes 3 y 4 -> 0x2050d8); su tela cuelga del hueso 0x11
BAND_SP = '--banderero-sp' in sys.argv
BAND_PERS = 3 if BAND_SP else 4
BAND_PRESETS = (1, 3) if BAND_SP else (0, 2, 4)
PERSONAJES = ((BAND_PERS, 4, 0xff),) if BANDERERO else ((11, 3, 0), (9, 2, 1))
# (personaje, esqueleto de bone.bin, asiento): el piloto, la acompanante y el banderero de la salida (ROB03: PS2
# 0x2046f0 = PC 488710 pone el personaje 4 = CHR_AUT04_CVT con el preset 0, 2 y 4; 0x205038 le da la entrada 4 de
# bone.bin por la tabla de saltos 0x40e3f0; sin asiento: va en el mundo). Sin --banderero el par lo da pares()

# ------------------------------------------------------------------ que pareja va en la carrera
# Piloto 487EE0 = PS2 0x205518 y acompanante 487F50 = PS2 0x2055b8 (personaje = indice de TABLA_CHR):
#  - [0x443510] (PC 78026C) == 3: el piloto 0xC y la acompanante 8 (sin la bandera de la consola): ese modo (rondas de
#    la demo, 49EED0) no esta en la demo de la Dreamcast: no se convierte (SIN PORTAR).
#  - sin 43F860 (= PS2 0x1b4878, [0x59acac] = PC [780260]: el juego de OUTRUN2SP): piloto 0xB; acompanante 9 en las
#    variantes 1, 3, 4 y 5 (tabla de saltos 0x40e620), si no 6.
#  - con 43F860 (el juego de la consola): la licencia activa +0x20 (PS2 [0x588688] = PC [7C2400], la foto) pasa por
#    la lista de la pila {0, 1, 2, 4, 3, 5} (copiada de 0x40e210: lui 0x41 / addiu -0x1df0 en 0x205550 / 0x2055d8) a k;
#    piloto = 0x36f928[k] (lui 0x37 / addiu -0x6d8 en 0x205590); acompanante = 0x36f940[j] (addiu -0x6c0 en 0x2056d4)
#    con j = 505340(k, n) = 0x378190[3 k + n] (PS2 0x28e888: lui 0x38 / addiu -0x7e70) y n = 1 en la variante 2, 2 en
#    la 9 y 0 en el resto; en la variante 5 j = 3 + 46C520 (PS2 0x22e048: [0x6162ac]) si vale 0..2, si no 6.
LISTA_LIC, PILOTOS, ACOMP, TABLA_505340 = 0x40e210, 0x36f928, 0x36f940, 0x378190
def pares(A):
    """[(piloto, acompanante)] de todas las parejas que pueden salir (la primera, la de OUTRUN2SP en la variante 1 y
    del juego de la consola con la licencia 0); los indices 6 de 0x36f940 (variante 5 con 46C520 fuera de 0..2: lee
    detras de la tabla, 0x879) no son personajes: no se convierten"""
    E = elf(A)
    lista = list(E.u32(LISTA_LIC, 6)); pil = list(E.u32(PILOTOS, 6)); aco = list(E.u32(ACOMP, 6)); t = list(E.u32(TABLA_505340, 18))
    if sorted(lista) != list(range(6)): raise SystemExit('0x40e210 no es la lista de la licencia')
    res = [(0xB, 9), (0xB, 6)]                     # OUTRUN2SP: variantes 1, 3, 4, 5 y las demas
    for slot in range(6):
        k = lista[slot]
        for j in [t[3 * k + n] for n in range(3)] + [3, 4, 5]:
            if (pil[k], aco[j]) not in res: res.append((pil[k], aco[j]))
    n = 0
    while E.u32(TABLA_CHR + 16 * n): n += 1
    if any(not (0 <= x < n) for p in res for x in p): raise SystemExit('pareja fuera de la tabla de personajes')
    return res
def nombre_par(p): return 'CHR%02d%02d.DAT' % p
def tabla_pares(A):
    """CHRPAR.DAT (lo lee demo/main.c para elegir el CHRppaa.DAT de la carrera; little endian, u32): 'CPR1', piloto
    de OUTRUN2SP (0x205548: addiu $v0, $zero, 0xb), acompanante de OUTRUN2SP por variante 0..9 (0x205700: tabla de
    saltos 0x40e620 [variante - 1] para 1..5: 0x205728 = 9, 0x205730 = 5 -> 6; fuera, 5 -> 6), lista de la licencia
    (6), pilotos 0x36f928 (6), acompanantes 0x36f940 (6), 505340 0x378190 (18)"""
    E = elf(A); jt = E.u32(0x40e620, 5); val = {0x205728: 9, 0x205730: 6}
    if any(x not in val for x in jt): raise SystemExit('0x40e620: destino sin estudiar')
    sp = [val[jt[v - 1]] if 1 <= v <= 5 else 6 for v in range(10)]
    v = [0xB] + sp + list(E.u32(LISTA_LIC, 6)) + list(E.u32(PILOTOS, 6)) + list(E.u32(ACOMP, 6)) + list(E.u32(TABLA_505340, 18))
    return b'CPR1' + struct.pack('<%dI' % len(v), *v)

_TL = {}
def convierte(A, sal, vista=None, personajes=None):
    elf(A)
    bone = A.datos(A.entrada('COMMON.PS2', 'bone.bin'))
    grupos = {}
    def grupo(k):
        if k not in grupos: grupos[k] = Grupo(A.datos(A.paquete('ANIMS.PS2')[k]))
        return grupos[k]
    if 't' not in _TL: _TL['t'] = tablas_logica(A)
    t = dict(_TL['t'])
    if personajes is None: personajes = PERSONAJES
    mod = modelo.Modelo(); mod.vq = True
    piezas = []; mvert = []                       # piezas; por malla: (pieza, P, N, W, material)
    texbase = {}
    pers = []
    for kind, ek, asiento in personajes:
        esq = Esqueleto(bone, ek); P = Personaje(A, kind, esq)
        if P.osage: t['OSG%d' % len(pers)] = osage_tablas(A.elf, kind, P.osage)
        hl, hr, gu, pa = manos(A, kind)
        for o in gu + pa:
            if o >> 16 != P.recurso: raise SystemExit('mano de otro recurso: %x' % o)
        pers.append((kind, esq, P, hl, hr, gu, pa, asiento))
        blq = P.blq; base = len(mod.texs)
        for h in cabeceras(blq):
            a = tex.imagen(blq, h); fmt, li, d = modelo.textura_dc(a)
            mod.texs.append((a.shape[1], a.shape[0], fmt, li, len(mod.blob), len(d))); mod.blob += d; mod.rgba.append(a)
        ntb = len(mod.texs) - base
        if kind == BAND_PERS and BANDERERO:       # la tela de la bandera: textura 1 del recurso (401B80 / 448810:
            if ntb < 2: raise SystemExit('%s sin la textura de la bandera' % P.nombre)    # [[r+24]][1])
            mod.mallas.append([mod.texs[base + 1][3], base + 1, np.zeros(3), 0.0, mod.nv, 0, blq, -1])
        R = blq.registros()
        lista_p = []                              # (registro, tipo, huesos, inversas)
        nh = len(esq.huesos); no = P.osage['nodos'] if P.osage else 0
        if nh + no > 56: raise SystemExit('%s: %d huesos y %d nodos de osage (ROBOT_HUESOS de comun/robot.h: 56)' % (P.nombre, nh, no))
        def hueso(b):                             # 514280: id | 0x8000 = la matriz del nodo id del osage (pool + id x 0x40)
            if not b & 0x8000: return b
            if (b & 0x7fff) >= no: raise SystemExit('%s: hueso de osage 0x%x sin nodo' % (P.nombre, b))
            return nh + (b & 0x7fff)              # (robot.c las deja detras de las del esqueleto)
        for b, obj in P.rigidas:
            lista_p.append((obj & 0xffff, 0, [hueso(b)], [0xffff]))
        for bs, obj in P.pesos:
            lista_p.append((obj & 0xffff, 1, [hueso(b) for b in bs], [P.idx_id(b) for b in bs]))
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
                if getattr(blq, 'zancada64', False):       # Pmt64: otras tablas 4 y 5, materiales en unidades de 1
                    tl, ti, mat = Pmt64.material(blq, reg, k, base, ntb)
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
    if BANDERERO: convierte(A, a[1]); sys.exit(0)
    E = elf(A)
    def par(d, c): return ((d, esqueleto_de(E, d), 0), (c, esqueleto_de(E, c), 1))
    if '--lista' in a: print(' '.join(['CHRPAR.DAT'] + [nombre_par(p) for p in pares(A)])); sys.exit(0)
    if '--pares' in a:
        os.makedirs(a[1], exist_ok=True)
        for p in pares(A): convierte(A, os.path.join(a[1], nombre_par(p)), personajes=par(*p))
        open(os.path.join(a[1], 'CHRPAR.DAT'), 'wb').write(tabla_pares(A))
        sys.exit(0)
    d, c = (int(x, 0) for x in a[a.index('--par') + 1].split(',')) if '--par' in a else (0xB, 9)
    convierte(A, a[1], personajes=par(d, c))
