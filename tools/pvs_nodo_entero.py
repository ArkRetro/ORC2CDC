#!/usr/bin/env python3
"""pvs_nodo_entero.py ORD7_entrada ORD7_salida: en la PVS que ya trae una etapa (tools/pvs.py, por mallas), cada nodo
grande (radio >= R_MIN m; ni carteles ni lejanos de lod_veg / lod_grande) se ve entero en las secciones en las que se ve
alguna de sus mallas. Rev 2.9.9.e (docs/PARIDAD.md, MAYA: la piramide): en SEG29 el nodo 194 (losas de piedra de 45 a
280 m que la PVS no deja ver desde la salida) tapaba en el calculo 53 de las 81 mallas de lo alto de la piramide (nodo
1417) y en la pantalla solo salian los escalones que asomaban, sueltos en el cielo. No cambia nada mas del fichero (ni
recortes ni aligerado). Lee y escribe los datos de tu disco: nada de esto va al repositorio."""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pvs
from lod_veg import NODO_LEJ
from rehace_pvs import guardada

R_MIN = 40.0

def main():
    src, dst = sys.argv[1], sys.argv[2]
    d = pvs.lee(src); M, N = d['M'], d['N']; g = M['nodo'].astype(int); gc = np.clip(g, 0, None)
    G = guardada(src, len(M))
    if G is None: raise SystemExit('%s: sin PVS' % src)
    util = (g >= 0) & (g < len(N)) & (M['n'] >= 3)
    grande = util & (N['r'][gc] >= R_MIN) & (N['cartel'][gc] < 0) & ((N['fl'][gc] & NODO_LEJ) == 0)
    sec = G.copy()
    for nd in np.unique(g[grande]):
        k = np.where(g == nd)[0]; sec[:, k] |= G[:, k].any(1, keepdims=True)
    ini, li, mapa, hg, s2 = pvs.listas(sec, util, len(d['R']))
    print('%s: %d mallas-seccion mas (%.1f %%), listas %d KB, huecos %d' % (os.path.basename(dst), (s2 & ~G).sum(),
          100 * (s2 & ~G).sum() / max(1, G.sum()), len(li) >> 10, hg))
    pvs.escribe(d, dst, ini, li, mapa)

if __name__ == '__main__':
    main()
