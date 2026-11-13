#!/usr/bin/env python3
"""conv_faros.py <iso> <salida/FAROS.DAT>: la luz de los faros del coche del jugador sobre la carretera (etapas de noche).

Del original (SLES-53998; el PC lo hace igual con otras constantes):
 - 0x188680 (PC 4695C0, llamado por 0x17c6b0 = PC 49F400 con las banderas del suceso & 3): si car+2C8 <= 0 y car+4 &
   0x900 == 0x900 (0x100 noche de 4A25F0 y 0x800 faros abiertos) carga car+B0 (0x164314) y llama a 0x106960 (PC
   416AA0).
 - 0x106960: textura 0x30009 (banco 3 = spr_etc_xst, textura 9), estados del GS (0x169ce0): ABE 1, mezcla del par
   (fuente 2, destino 1) de la tabla 0x3590f0 = ALPHA 0x48 (Cs As + Cd, aditiva), ZTST 1 (ALWAYS), ATE 0; y manda el
   paquete fijo [0x328a80] (0x218 qwords) que hizo al arrancar 0x106a98 (PC 4171F0 / 417550).
 - 0x106a98: V2 = (-tan [0x328a7c], 0, 0), V1 = (0, -tan [0x328a78], 0), V3 = [0x3bfab0] = (0, 0, 1), los tres girados
   en x por -[0x328a74] (0x1647e4 en la pila con la identidad, 0x1649ac: como D3DXMatrixRotationX del PC, vectores
   fila); para i = 0..7 (dos tandas de 4), a0 = i / 8 y a1 = (i + 1) / 8 (0x3e000000): P(t) = t V1 + V2 + V3 y su
   proyeccion al suelo 0x106eb0: Q = P [0x328a6c] / P.y - (0, 0,8, 2,0) ([0x328a68..0x328a70]).
 - 0x106f28: tira de 8 vertices, k = 0..3: (x1 s, y 0, -z1) y (x0 s, 0, -z0) con s = 1 - k / 3, ST = ((2 - k / 3) / 2,
   1 - a) (en 1/4096), color RGBA (0x50, 0x50, 0x48, 0x80) (0x80485050: textura x 0,625 / 0,625 / 0,5625 y alfa de la
   textura x 1 en el GS); otra igual con las x cambiadas de signo. 16 tiras.
Constantes del PS2: 0,56 / 0,2618 / 0,24 / 0,8 / 2,0 (PC 746B20 0,365, 746B1C 0,2418, 746B18 0,252, 746B10 1,2,
746B14 4,1; v = a x 0,85 en el PC).

Salida: un modelo ORD7 (tools/modelo.py) en el marco del coche (z = -adelante, como car+B0) con 16 mallas (el numero
de tira en el campo del nodo: 0..7 un lado, 8..15 el otro) y la textura (spr_etc_xst 9, 256x256)."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import modelo
from conv_hud import Disco, texturas

def f32(D, va):
    return np.float32(struct.unpack('<f', struct.pack('<I', D.elf32(va)[0]))[0])

def tiras(D):
    """las 16 tiras de 8 vertices de 0x106a98: [(x, y, z, u, v)] en float32 como el EE"""
    F = np.float32
    k7c, k78, k74 = f32(D, 0x328a7c), f32(D, 0x328a78), f32(D, 0x328a74)
    fp = np.array([f32(D, 0x328a68), f32(D, 0x328a6c), f32(D, 0x328a70)], F)
    v3 = np.array([f32(D, 0x3bfab0), f32(D, 0x3bfab4), f32(D, 0x3bfab8)], F)
    assert (k7c, k78, k74) == (F(0.56), F(0.2618), F(0.24)) and tuple(fp) == (0, F(0.8), F(2.0)) and tuple(v3) == (0, 0, 1), \
        'constantes de 0x106a98 distintas: no es SLES-53998 v1.02?'
    v2 = np.array([-np.tan(k7c, dtype=F), 0, 0], F)
    v1 = np.array([0, -np.tan(k78, dtype=F), 0], F)
    a = F(-k74); s, c = np.sin(a, dtype=F), np.cos(a, dtype=F)
    R = np.array([[1, 0, 0], [0, c, s], [0, -s, c]], F)    # D3DXMatrixRotationX (fila: v' = v R)
    v1, v2, v3 = v1 @ R, v2 @ R, v3 @ R
    def proy(t):
        p = (t * v1 + v2) + v3
        return p * (fp[1] / p[1]) - fp
    res = []
    for lado in (1, -1):
        for i in range(8):
            a0, a1 = F(i) * F(0.125), F(i + 1) * F(0.125)
            q1, q0 = proy(a1), proy(a0)
            T = []
            for k in range(4):
                sc = F(1) - F(k) * F(1 / 3)
                u = (sc + F(1)) * F(0.5)
                T.append((lado * sc * q1[0], 0.0, -q1[2], u, F(1) - a1))
                T.append((lado * sc * q0[0], 0.0, -q0[2], u, F(1) - a0))
            res.append(T)
    return res

def convierte(iso, sal):
    D = Disco(iso)
    etc = D._ent(D.p_spr, D.n_spr.index('spr_etc_xst.sz')); img = texturas(etc)[9]
    mod = modelo.Modelo(); mod.ruta = np.zeros((0, 5))
    fmt, li, d = modelo.textura_dc(img)
    mod.texs.append((img.shape[1], img.shape[0], fmt, 2, 0, len(d))); mod.blob = d; mod.rgba.append(img)
    col = np.uint32(0xFF9F9F8F)                                # (0x50, 0x50, 0x48, 0x80) del GS en ARGB de 0..255
    for n, T in enumerate(tiras(D)):
        v = np.zeros(len(T), dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
        for j, (x, y, z, u, w) in enumerate(T): v[j] = (x, y, z, u, w, col)
        P = np.stack([v['x'], v['y'], v['z']], 1).astype(float); cc = P.mean(0)
        mod.mallas.append([2, 0, cc, float(np.sqrt(((P - cc) ** 2).sum(1).max())), mod.nv, len(T), None, n])
        mod.verts.append(v); mod.nv += len(T)
    mod.escribe(sal)
    print('FAROS.DAT: %d tiras, textura %dx%d formato %x (%d KB)' % (len(mod.mallas), img.shape[1], img.shape[0],
          mod.texs[0][2], len(mod.blob) >> 10))

if __name__ == '__main__':
    if len(sys.argv) != 3: raise SystemExit(__doc__)
    convierte(sys.argv[1], sys.argv[2])
