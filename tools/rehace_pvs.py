#!/usr/bin/env python3
"""rehace_pvs.py ORD7_entrada ORD7_salida [secciones.npy]: la PVS de una etapa ya hecha (aligerada, con lejanos de
lod_veg / lod_grande) calculada otra vez sobre su geometria de ahora (tools/pvs.py): los nodos que la PVS que trae no deja ver casi nunca siguen
como estaban y no tapan; los lejanos (NODO_LEJ) no tapan y ganan una seccion si la gana alguna malla de su completo. Con secciones.npy: guarda ahi el calculo, o lo usa si ya existe
(otra version de la misma etapa con la misma geometria: la del Mod Taxi). Rev 2.9.9.e (docs/PARIDAD.md, MAYA: la piramide).
Lee y escribe los datos de tu disco: nada de esto va al repositorio."""
import sys, os
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pvs
from lod_veg import NODO_LEJ, _decodifica_lista

def guardada(fn, nm):
    """las mascaras por seccion de la PVS que ya trae el fichero (nsecciones x nmallas) o None"""
    import struct
    b = open(fn, 'rb').read(); h = struct.unpack_from('<13I', b, 4)
    if not h[12] & pvs.XOR: return None
    ns = h[12] & ~pvs.XOR; p = 56 + 24 * h[4] + 32 * h[0] + 24 * h[1] + 4 * h[2] * h[3] + 32 * h[10] + 64 * h[11]
    ini = np.frombuffer(b, '<u4', ns + 1, p).astype(np.int64); p += 4 * (ns + 1); L = b[p:p + int(ini[-1])]
    S = np.zeros((ns, nm), bool); m = np.zeros(nm, bool)
    for s in range(ns):
        for k in _decodifica_lista(L[ini[s]:ini[s + 1]]): m[k] ^= True
        S[s] = m
    return S

def main():
    src, dst = sys.argv[1], sys.argv[2]
    d = pvs.lee(src); M, N = d['M'], d['N']; g = M['nodo'].astype(int)
    lej = (N['fl'] & NODO_LEJ) != 0; ml = (g >= 0) & np.isin(g, np.where(lej)[0])
    # los nodos que la PVS guardada no deja ver (casi) nunca (en menos de CASI_NUNCA de las secciones) siguen como estaban
    # y no tapan (MAYA: el 194, losas de
    # piedra de 45 a 280 m sobre la carretera que el original no dibuja desde dentro: VISO.DAT; dibujado, una roca
    # colgando sobre la salida)
    CASI_NUNCA = 0.1                                                # (fraccion de las secciones)
    G = guardada(src, len(M)); nunca_n = np.zeros(len(N), bool)
    if G is not None:
        sv = np.zeros(len(N)); k = np.where(g >= 0)[0]; np.maximum.at(sv, g[k], G[:, k].mean(0))   # secciones en las que se ve
        con = np.zeros(len(N), bool); con[g[k]] = True; nunca_n = con & (sv < CASI_NUNCA) & ~lej
    nm_n = (g >= 0) & nunca_n[np.clip(g, 0, None)]
    print('nodos que no se ven casi nunca (no tapan, como estaban):', int(nunca_n.sum()), 'mallas', int(nm_n.sum()))
    if len(sys.argv) > 3 and os.path.exists(sys.argv[3]): sec = np.load(sys.argv[3]); util = (g >= 0) & (g < len(N)) & (M['n'] >= 3)
    else:
        r = pvs.calcula(d, no_tapan=ml | nm_n, una_pasada=True); sec = r['sec']; util = r['util']
        if len(sys.argv) > 3: np.save(sys.argv[3], sec)
    sec = sec.copy(); sec[:, ml] = False
    if G is not None: sec[:, nm_n] = G[:, nm_n]
    for i in np.where(N['cartel'] <= -2)[0]:
        j = -2 - int(N['cartel'][i]); kc = np.where(g == i)[0]; kl = np.where(g == j)[0]
        if len(kc) and len(kl): sec[:, kl] |= sec[:, kc].any(1, keepdims=True)
    ini, li, mapa, hg, s2 = pvs.listas(sec, util, len(d['R']))
    pvs.escribe(d, dst, ini, li, mapa)
    print('%s: PVS rehecha, %d secciones, listas %d KB, huecos %d' % (os.path.basename(dst), len(ini) - 1, len(li) >> 10, hg))

if __name__ == '__main__':
    main()
