#!/usr/bin/env python3
"""conv_union.py tu.iso salida/ [zona] [--sin-tramos]: la carretera que une un tramo con los dos siguientes (la bifurcacion) y los
dos tramos siguientes, todo colocado en el marco del tramo de salida (el de SEG<n>.DAT, sin transformar). Sale de tu
disco: no se distribuye.

Como lo hace el original (descriptores del ejecutable, como en el PC: runtime_apply_course_data_44d720 y
runtime_course_matrix_44c0d0 de la decompilacion):
 - una tabla de 66 punteros a descriptores de tramo de 0xb0 bytes (el PC los llama primarios), seguida de la de los
   77 descriptores de union (secundarios). En el de tramo: +0x00 su indice, +0x0c la colision (\\Stage\\ZONA\\coli_CS_*),
   +0x20.. el resto de ficheros del tramo y en +0x98 la colocacion de su union respecto al tramo: traslacion (x, y, z)
   y giros x, y, z (en el PC +0x80..+0x94: matriz 7D3190 = traslacion * giro y * giro x * giro z). En el de union:
   +0x0c la colision de las dos ramas (\\BK\\coli_BK_*), +0x18 / +0x1c la de la rama izquierda / derecha sola. Todas
   las bifurcaciones usan la misma geometria (BK.PS2: cs_BK_PALM_pmt; los cs_BK_<zona> que nombra el ejecutable no
   estan en el disco, salvo los de las metas).
 - el arbol del curso (SCRIPTS: \\bin\\csc_data_2.bin para el de OutRun 2; registros de 0x78 bytes: +0x04 clave,
   +0x1c descriptor de tramo, +0x20 de union, +0x2c / +0x30 clave del hijo izquierdo / derecho).
 - la union empieza en el final del tramo y sigue -z en su marco: avanza, se abre en dos ramas que giran 90 grados a
   cada lado y vuelven a -z; el tramo siguiente empieza en el final de su rama (su marco local tambien empieza en el
   origen hacia -z). Los tramos y la union se cargan a la vez en el original (cuatro raices de colision; las que no
   son la 0 comparten la matriz de la union).

Salida (formato ORD6 de modelo.py, en el marco del tramo de salida):
 BKI.DAT  la union (geometria, cielo del tramo de salida, ruta de la rama izquierda)
 BKD.DAT  solo la ruta de la rama derecha (sin mallas ni texturas)
 SEGnn.DAT los dos tramos siguientes (n = segmento de STAGE.PS2), ya colocados al final de su rama
Las rutas de la union salen de la colision: el poligono principal de cada seccion (8 m) da el centro y los bordes de
la calzada; limites, franjas y alturas como en conv_tramo.py (superficies), con la colision de las dos ramas."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets, hash_nombre
from ps2 import stage
import modelo, conv_tramo
from conv_tramo import NPERF, NCARRIL, NEXT, NBANDA

DESC = 0xb0                                         # tamano de un descriptor (tramo o union)

def u32(E, va): return struct.unpack('<I', E.lee(va, 4))[0]

def f32(E, va, n): return struct.unpack('<%df' % n, E.lee(va, 4 * n))

def tablas(A, zona0='PALM'):
    """(tabla de descriptores de tramo, numero, tabla de los de union): se localiza por el descriptor de la zona de
    salida, cuyo +0x0c apunta a la ruta de su colision"""
    E = A.elf
    for va in E.busca(('\\Stage\\%s\\coli_CS_%s_bin\0' % (zona0, zona0)).encode()):
        for r in E.referencias(va):
            d = r - 0x0c
            for t in E.referencias(d):
                base = t - 4 * u32(E, d); n = 0
                while u32(E, base + 4 * n) and u32(E, u32(E, base + 4 * n)) == n: n += 1
                return base, n, base + 4 * n
    raise SystemExit('no se encuentra el descriptor de %s en el ejecutable' % zona0)

def cadena(E, va): return E.cadena(u32(E, va))

def arbol(A, prim_zona0, fich='csc_data_2.bin'):
    """registros del arbol del curso: [(clave, tramo, union, clave izquierda, clave derecha)]"""
    e = [x for x in A.busca('*' + fich) if x.paquete.startswith('SCRIPTS')][0]; d = A.datos(e)
    nrel = struct.unpack_from('<I', d, 4)[0]; h, cnt, off = struct.unpack_from('<3I', d, 8 + 8 * nrel)
    regs = []
    for i in range(cnt):
        r = struct.unpack_from('<30i', d, off + 0x78 * i)
        regs.append((r[1], r[7], r[8], r[11], r[12]))
    return regs

def segmento_de(A, ruta_coli):
    """indice de STAGE.PS2 (stage.segmentos) del tramo cuya colision es esa ruta del ejecutable (\\Stage\\ZONA\\...)"""
    rel = '\\' + ruta_coli.split('\\', 2)[2]
    h = hash_nombre(rel); k = 0
    for hh, o, s in A.iso.paquete('STAGE.PS2'):
        if A.iso.m[o:o + 4] == b'COLI':
            if hh == h: return k
            k += 1
    raise SystemExit('tramo %s no encontrado' % rel)

def coli_bytes(A, ruta):
    return A.datos(A.entrada('BK.PS2', ruta.split('\\')[-1]))

def principales(d):
    """poligonos principales (la calzada, uno por seccion de 8 m en las ramas solas) y su seccion"""
    npol, nprim = struct.unpack_from('<2I', d, 8); sec = struct.unpack_from('<8I', d, 20)
    P = np.frombuffer(d[sec[2]:sec[2] + 64 * npol], np.uint8).reshape(npol, 64)
    V = P[:, :48].copy().view('<f4').reshape(npol, 4, 3).astype(float)
    L = np.frombuffer(d[sec[5]:sec[5] + 2 * npol], '<u2').astype(int)
    return V[:nprim], L[:nprim]

class Coli:
    """lo minimo de un segmento para conv_tramo.superficies: una entrada COLI0200"""
    def __init__(self, d): self.m = d; self.ent = [(0, 0, len(d))]

def ruta_rama(d_rama, d_todo):
    """ruta de una rama de la union: centro, bordes y carriles del poligono principal de cada seccion; limites,
    franjas y alturas de la colision de las dos ramas (para poder ir por cualquier lado antes de separarse)"""
    V, L = principales(d_rama)
    secs = sorted(set(L))
    C = np.array([V[L == l].reshape(-1, 3).mean(0) for l in secs])
    # remuestreo cada 4 m (con el primer punto en el borde de entrada: la seccion 0 empieza en z = 0)
    t0 = C[1] - C[0]; t0 /= np.linalg.norm(t0); t1 = C[-1] - C[-2]; t1 /= np.linalg.norm(t1)
    C = np.vstack([C[0] - t0 * 4.0, C, C[-1] + t1 * 4.0])                # de borde a borde
    Ls = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(C, axis=0), axis=1))])
    s = np.arange(0, Ls[-1] + 0.01, 4.0)
    cam = np.stack([np.interp(s, Ls, C[:, k]) for k in range(3)], 1)
    # suavizada (84 m): donde se separan las ramas el centro pasa de golpe al de la rama (8 m en una seccion); la ruta
    # solo es la referencia de los desvios (bordes, carriles, limites y franjas se miden desde ella)
    k = np.ones(21) / 21
    cam = np.stack([np.convolve(np.pad(cam[:, j], 10, mode='edge'), k, 'valid') for j in range(3)], 1)
    t = np.gradient(cam, axis=0); t[:, 1] = 0; t /= np.linalg.norm(t, axis=1, keepdims=True) + 1e-9
    der = np.stack([-t[:, 2], np.zeros(len(t)), t[:, 0]], 1)
    # bordes: desvio lateral de los vertices de los poligonos de la seccion mas cercana
    an = np.zeros((len(cam), 2)); Cs = C[1:-1]
    for i, c in enumerate(cam):
        j = int(np.argmin(np.linalg.norm(Cs - c, axis=1)))
        q = (V[L == secs[j]].reshape(-1, 3) - c) @ der[i]
        an[i] = (max(3.0, -q.min()), max(3.0, q.max()))
    k = np.ones(5) / 5
    an = np.stack([np.convolve(np.pad(an[:, j], 2, mode='edge'), k, 'valid') for j in range(2)], 1)
    # carriles: unos 3,7 m cada uno a lo ancho, repartidos en NCARRIL como en conv_tramo.ajusta_al_asfalto
    car = np.zeros((len(cam), NCARRIL))
    for i in range(len(cam)):
        a2, b2 = -an[i, 0] + 0.5, an[i, 1] - 0.5; w = b2 - a2; n = max(1, int(round(w / 3.7)))
        pos = [a2 + (m + 0.5) * w / n for m in range(n)]
        car[i] = [pos[int(round(kk * (len(pos) - 1) / (NCARRIL - 1)))] for kk in range(NCARRIL)]
    lim, ext, ban, hpf = conv_tramo.superficies(Coli(d_todo), cam, an)
    pf = np.where(np.isfinite(hpf), hpf, cam[:, 1:2])
    if not ext.any(): ext = np.repeat(pf[:, [0, -1]], NEXT, 1)
    return np.concatenate([cam, an, pf, car, lim, ext, ban], 1), C

def fin_rama(d_rama):
    """punto medio del borde de salida de la rama (donde empieza el tramo siguiente) y angulo de su direccion"""
    V, L = principales(d_rama)
    q = V[L == L.max()].reshape(-1, 3); p = V[L == L.max() - 2].reshape(-1, 3).mean(0)
    dirc = q.mean(0) - p; dirc[1] = 0; dirc /= np.linalg.norm(dirc)
    a = (q - q.mean(0)) @ dirc; borde = q[a > a.max() - 0.5].mean(0)
    return borde, float(np.arctan2(-dirc[0], -dirc[2]))      # giro y que lleva -z a esa direccion

def compone(M1, M2):
    """M1 * M2: primero M2 (en el marco de M1) y luego M1"""
    tx, ty, tz, ry = M1; c, s = np.cos(ry), np.sin(ry)
    x, y, z, r2 = M2
    return (tx + x * c + z * s, ty + y, tz - x * s + z * c, ry + r2)

def main():
    arg = [a for a in sys.argv[1:] if not a.startswith('--')]
    iso, dsal = arg[0], arg[1]; zona0 = arg[2] if len(arg) > 2 else 'PALM'
    A = Assets(iso); E = A.elf
    base, nprim, base2 = tablas(A, zona0)
    descs = [u32(E, base + 4 * i) for i in range(nprim)]
    i0 = [i for i, dd in enumerate(descs) if ('\\%s\\' % zona0) in cadena(E, dd + 0x0c)][0]
    regs = arbol(A, i0)
    raiz = [r for r in regs if r[1] == i0][0]
    porclave = {r[0]: r for r in regs}
    hijos = [porclave[raiz[3]], porclave[raiz[4]]]
    d0 = descs[i0]; tx, ty, tz, rx, ry, rz = f32(E, d0 + 0x98, 6)
    assert abs(rx) < 1e-4 and abs(rz) < 1e-4, 'union con giro en x o z: no soportado'
    M_bk = (tx, ty, tz, ry)
    dbk = u32(E, base2 + 4 * raiz[2])
    c_todo, c_izq, c_der = cadena(E, dbk + 0x0c), cadena(E, dbk + 0x18), cadena(E, dbk + 0x1c)
    print('salida %s (descriptor %d), union %s, ramas %s / %s; union en (%.2f, %.2f, %.2f) giro %.4f' %
          (zona0, i0, c_todo, cadena(E, descs[hijos[0][1]] + 0x0c), cadena(E, descs[hijos[1][1]] + 0x0c), tx, ty, tz, ry))
    d_todo, d_izq, d_der = coli_bytes(A, c_todo), coli_bytes(A, c_izq), coli_bytes(A, c_der)
    segs = A.segmentos(); s0 = segmento_de(A, cadena(E, d0 + 0x0c))
    # la union: geometria, el cielo del tramo de salida y la ruta izquierda
    mod = modelo.Modelo(); mod.limita = True
    nm = c_todo.split('\\')[-1].replace('coli_', 'cs_').replace('_bin', '_pmt')
    try: A.entrada('BK.PS2', nm)
    except KeyError: nm = 'cs_BK_PALM_pmt'
    mod.bloque(A.bloque(A.entrada('BK.PS2', nm)), objetos=True)
    for blq in segs[s0].bloques:
        if blq.o != segs[s0].o and conv_tramo.es_cielo(blq): mod.bloque(blq, lista=3)
    ri, Ci = ruta_rama(d_izq, d_todo); rd, Cd = ruta_rama(d_der, d_todo)
    sep = np.linalg.norm(ri[:min(len(ri), len(rd)), [0, 2]] - rd[:min(len(ri), len(rd)), [0, 2]], axis=1)
    print('rutas de la union: %d y %d puntos; se separan (1 m) en el punto %d' % (len(ri), len(rd), int(np.argmax(sep > 1.0))))
    mod.ruta = ri; mod.recorta_texturas(); conv_tramo.transforma(mod, M_bk)
    ini = mod.escribe(os.path.join(dsal, 'BKI.DAT'))
    print('BKI.DAT: %d mallas (%d cielo), %d vertices, %d texturas (%d KB), %d KB' % (len(mod.mallas), ini[4] - ini[3],
          mod.nv, len(mod.texs), len(mod.blob) >> 10, os.path.getsize(os.path.join(dsal, 'BKI.DAT')) >> 10))
    md = modelo.Modelo(); md.ruta = rd; conv_tramo.transforma(md, M_bk); md.escribe(os.path.join(dsal, 'BKD.DAT'))
    # los tramos siguientes, al final de cada rama
    if '--sin-tramos' in sys.argv: return
    for lado, (h, dr) in enumerate(zip(hijos, (d_izq, d_der))):
        p, a = fin_rama(dr)
        M = compone(M_bk, (p[0], p[1], p[2], a))
        n = segmento_de(A, cadena(E, descs[h[1]] + 0x0c))
        print('rama %s: %s (segmento %d) en (%.2f, %.2f, %.2f) giro %.4f' % ('izquierda' if lado == 0 else 'derecha',
              cadena(E, descs[h[1]] + 0x0c), n, *M))
        conv_tramo.convierte(segs[n], n, os.path.join(dsal, 'SEG%02d.DAT' % n), M)

if __name__ == '__main__':
    main()
