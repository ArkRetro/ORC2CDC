"""grupos_nodos.py fichero.DAT... | --datos datos/ trabajo/: grupos de nodos de las etapas y las uniones (ORD7) para el
reparto del TA de la demo (demo/main.c nodos_visibles_todos). DECISION DEL PORT (revision 44, docs/PARIDAD.md: Edificios
sin base): el original no tiene presupuesto del TA y dibuja cada nodo por su cuenta (walk_404d10 del PC: la prueba de
404B00 nodo a nodo, sin mirar al padre); la Dreamcast, cuando no cabe todo, quita nodos por su clase de distancia y de
tamano en la pantalla, y la base de un edificio (otra clase: mas vertices, otra esfera) se iba y la parte de arriba no
(trozos de torre "flotando" al final de PALM en la consola real). Aqui se juntan los nodos de un mismo objeto para que
la demo los dibuje o los quite a la vez:
 - caja de los vertices de cada nodo (sus tiras de las listas 0..2; sin los carteles, que van en el marco del nodo);
 - dos nodos van juntos si los dos miden menos de GRUPO_LADO en planta (x y z: lo grande, terreno y calzada, no
   arrastra a nada), sus plantas se solapan en al menos GRUPO_SOLAPE del area de la menor, la mayor no pasa de
   GRUPO_PROPORCION veces la menor (un trozo de suelo con un arbol o un banco encima no los junta, ni encadena una
   manzana entera), entre su caja y la otra en altura no hay mas de GRUPO_HUECO (apilados: la base y la torre, el
   tejado, las capas de ventanas) y son de la misma rama de la union (bits 8-9);
 - los grupos son las clases de esa relacion (union de conjuntos); el jefe es el nodo de menor indice.
En el fichero: bits 16-31 de las banderas del nodo = jefe + 1 (0 = el nodo va solo). Lo aplican conv_tramo.convierte
(SEGnn.DAT) y conv_union.py (BKnn.DAT) justo despues de escribir el modelo (los pasos seg / union / unionta de
tools/regenera.py); en una carpeta de datos ya hecha, --datos (tools/regenera.py --interno grupos datos/ [trabajo/]:
los SEG / BK sueltos y los de SEGS.PAK, UNIONES.PAK y UNIONESTA.PAK, que se vuelven a empaquetar). Idempotente."""
import os, struct, sys
import numpy as np

GRUPO_LADO = 80.0                                               # m: mayor lado en planta de un nodo que se agrupa
GRUPO_SOLAPE = 0.5                                              # parte del area en planta del menor que se solapa
GRUPO_HUECO = 1.0                                               # m: separacion en altura como mucho
GRUPO_PROPORCION = 4.0                                          # la planta mayor, como mucho 4 veces la menor (un trozo
                                                                # de suelo con algo encima no arrastra a lo de encima)

NODO = np.dtype([('c', '<f4', 3), ('r', '<f4'), ('fl', '<u4'), ('cartel', '<i4'), ('rel', '<u4', 2)])
MALLA = np.dtype([('c', '<f4', 3), ('r', '<f4'), ('v0', '<u4'), ('n', '<u4'), ('tex', '<i4'), ('nodo', '<i4')])

def partes(b):
    """(nodos, mallas, posiciones de los vertices, inicio de las listas, desplazamiento de la tabla de nodos)"""
    if b[:4] != b'ORD7': raise ValueError('no es ORD7')
    h = struct.unpack('<13I', b[4:56]); nm, nv, nr, pr, nt = h[:5]; ini = h[5:10]; nn = h[10]
    p = 56 + 24 * nt
    M = np.frombuffer(b, MALLA, nm, p); p += 32 * nm
    P = np.frombuffer(b, '<f4', nv * 6, p).reshape(nv, 6)[:, :3]; p += 24 * nv + 4 * nr * pr
    N = np.frombuffer(b, NODO, nn, p)
    return N, M, P, ini, p

def grupos(N, M, P, ini):
    """jefe de cada nodo (indice; el propio si va solo)"""
    nn = len(N); lo = np.full((nn, 3), np.inf); hi = np.full((nn, 3), -np.inf)
    for x in M[:ini[3]]:
        d = int(x['nodo'])
        if d < 0 or d >= nn or N[d]['cartel'] >= 0 or x['n'] < 3: continue
        q = P[x['v0']:x['v0'] + x['n']]
        lo[d] = np.minimum(lo[d], q.min(0)); hi[d] = np.maximum(hi[d], q.max(0))
    ax = hi[:, 0] - lo[:, 0]; az = hi[:, 2] - lo[:, 2]
    cand = np.nonzero(np.isfinite(ax) & (ax < GRUPO_LADO) & (az < GRUPO_LADO))[0]
    jefe = np.arange(nn)
    def raiz(i):
        while jefe[i] != i: jefe[i] = jefe[jefe[i]]; i = jefe[i]
        return i
    if len(cand) > 1:
        L, H = lo[cand], hi[cand]; area = np.maximum(ax[cand], 0.01) * np.maximum(az[cand], 0.01)
        rama = (N['fl'][cand] >> 8) & 3
        for a in range(len(cand) - 1):
            b = np.arange(a + 1, len(cand))
            ox = np.minimum(H[a, 0], H[b, 0]) - np.maximum(L[a, 0], L[b, 0])
            oz = np.minimum(H[a, 2], H[b, 2]) - np.maximum(L[a, 2], L[b, 2])
            hueco = np.maximum(L[a, 1], L[b, 1]) - np.minimum(H[a, 1], H[b, 1])
            ok = (ox > 0) & (oz > 0) & (ox * oz >= GRUPO_SOLAPE * np.minimum(area[a], area[b])) & (hueco <= GRUPO_HUECO) & \
                 (rama[b] == rama[a]) & (np.maximum(area[a], area[b]) <= GRUPO_PROPORCION * np.minimum(area[a], area[b]))
            for j in b[ok]:
                ra, rb = raiz(cand[a]), raiz(cand[j])
                if ra != rb: jefe[max(ra, rb)] = min(ra, rb)
    return np.array([raiz(i) for i in range(nn)])

def aplica_bytes(b):
    """el ORD7 b con los grupos en las banderas de sus nodos; (bytes nuevos, grupos, nodos agrupados)"""
    N, M, P, ini, p = partes(b)
    if not len(N): return b, 0, 0
    j = grupos(N, M, P, ini); nn = len(N)
    cuenta = np.bincount(j, minlength=nn); solo = cuenta[j] < 2
    fl = (N['fl'] & 0xFFFF) | np.where(solo, 0, (j + 1).astype(np.uint32) << 16).astype(np.uint32)
    N2 = N.copy(); N2['fl'] = fl
    return b[:p] + N2.tobytes() + b[p + 32 * nn:], int((cuenta >= 2).sum()), int((~solo).sum())

def aplica(fn):
    b = open(fn, 'rb').read(); nb, g, n = aplica_bytes(b)
    if nb != b:                                                 # (fichero nuevo: no el mismo si estaba enlazado)
        open(fn + '.tmp', 'wb').write(nb); os.replace(fn + '.tmp', fn)
    print('%s: %d grupos, %d de %d nodos agrupados' % (os.path.basename(fn), g, n, len(partes(b)[0])))

# paquetes de una carpeta de datos ya hecha: (paquete, prefijo, opciones de pak_union.py como en tools/regenera.py)
PAQUETES = (('UNIONES.PAK', 'BK', []), ('UNIONESTA.PAK', 'BK', ['--pak=UNIONESTA.PAK']), ('UNIONESR.PAK', 'BK', ['--pak=UNIONESR.PAK']),
            ('SEGS.PAK', 'SEG', ['--pak=SEGS.PAK', '--prefijo=SEG', '--tope=7842']))

def datos(D, W, f=None):
    """los grupos en una carpeta de datos ya hecha (D) sin convertir de nuevo: cada SEG / BK suelto (ORD7) se cambia en
    su sitio; los de cada paquete se recomponen (pak_union.recompone) en W, se cambian y se vuelven a empaquetar con
    pak_union.py como en tools/regenera.py (que comprueba la recomposicion byte a byte); despues el paquete y sus
    descriptores nuevos sustituyen a los de D. Idempotente (sin cambios no se reempaqueta). f: otra transformacion de
    los bytes de cada ORD7 en lugar de los grupos (tools/cielo_tramado.py)"""
    import shutil, subprocess, pak_union
    tr = f or (lambda b: aplica_bytes(b)[0])
    for nm in sorted(os.listdir(D)):
        if (nm.startswith('SEG') or nm.startswith('BK')) and nm[-4:] == '.DAT' and nm[3 if nm[0] == 'S' else 2:-4].isdigit():
            fn = os.path.join(D, nm); b = open(fn, 'rb').read()
            if b[:4] != b'ORD7': continue
            nb = tr(b)
            if nb != b: open(fn + '.tmp', 'wb').write(nb); os.replace(fn + '.tmp', fn)
            print('%s: suelto, %s' % (nm, 'cambia' if nb != b else 'igual'))
    for pak, pre, op in PAQUETES:
        if not os.path.isfile(os.path.join(D, pak)): continue
        P = open(os.path.join(D, pak), 'rb').read()
        e, s = os.path.join(W, 'ent_' + pak), os.path.join(W, 'sal_' + pak)
        for d in (e, s):
            if os.path.isdir(d): shutil.rmtree(d)
            os.makedirs(d)
        cambia = 0; n = 0
        for f in sorted(os.listdir(D)):
            if not (f.startswith(pre) and f[len(pre):-4].isdigit() and f[-4:] == '.DAT'): continue
            d = open(os.path.join(D, f), 'rb').read()
            if d[:4] != b'PAKD' or d[20:36].rstrip(b'\0').decode() != pak: continue
            b = pak_union.recompone(d, P); nb = tr(b); n += 1; cambia += nb != b
            open(os.path.join(e, f), 'wb').write(nb)
        print('%s: %d ficheros, %d cambian' % (pak, n, cambia))
        if not cambia: continue
        subprocess.run([sys.executable, '-u', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pak_union.py'), e, s] + op,
                       check=True, stdout=subprocess.DEVNULL)
        for f in os.listdir(s):                                 # (fichero nuevo en D: otro disco, o D enlazada)
            shutil.copyfile(os.path.join(s, f), os.path.join(D, f + '.tmp')); os.replace(os.path.join(D, f + '.tmp'), os.path.join(D, f))
        shutil.rmtree(e); shutil.rmtree(s)

def main():
    a = sys.argv[1:]
    if not a: raise SystemExit(__doc__)
    if a[0] == '--datos': return datos(a[1], a[2])
    for f in a: aplica(f)

if __name__ == '__main__':
    main()
