"""conv_union.py tu.iso salida/ [--sin-tramos]: el curso entero del modo (las 15 etapas del arbol, sus uniones y metas)
para la demo: SEGnn.DAT, BKkk.DAT, BKD.DAT y CURSO.DAT (ver curso). Sale de tu disco: no se distribuye.

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

Salida (formato ORD6 de modelo.py, cada fichero en su propio marco; la demo los coloca al leerlos):
 SEGnn.DAT las etapas (n = segmento de STAGE.PS2; conv_tramo.py), empiezan en el origen hacia -z
 BKkk.DAT  la union de la etapa de clave kk: bifurcacion (cs_BK_PALM, su entorno cs_ENVBK_PALM, los porticos de
           COMMON obj_course_obj_bk_palm con los carteles obj_OBJ_<ZONA>_BUNKI_TX de sus dos destinos, el cielo de la
           etapa y la ruta de la rama izquierda) o meta (cs_BK_<zona> con su colision, el cielo de la etapa y su ruta)
 BKD.DAT   solo la ruta de la rama derecha de la bifurcacion (la misma en todas)
 CURSO.DAT el arbol: zona, SEGnn, nivel, hijos, colocaciones, metros por posicion de carretera, meta y niebla
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
    k = np.ones(21) / 21                                                  # (los extremos, en su sitio: impar)
    cam = np.stack([np.convolve(np.pad(cam[:, j], 10, mode='reflect', reflect_type='odd'), k, 'valid')
                    for j in range(3)], 1)
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
    car = conv_tramo.carriles_uniformes(an)
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

UMBRAL_META = 24.0                                  # error RMS de la VQ aceptado en las uniones de meta
NIEBLA_MAX = 8                                      # tramos de niebla por etapa en CURSO.DAT
REG = 4 + 4 + 16 + 32 + 8 + 4 + 24 * NIEBLA_MAX     # bytes de un registro de CURSO.DAT

def zona_de(ruta):
    """codigo de zona de una ruta del ejecutable (\\Stage\\ZONA\\...)"""
    return ruta.split('\\')[2]

def niebla(A, zona):
    """tramos de niebla de la etapa (scn_env_fog_<zona>, lista 1: la de la carretera, entorno 1 del original; lista
    0/2: lo de lejos y los coches). Registros de 0xb0 bytes hasta la marca 0xffff (0xfffe: hueco), como los lee
    course_environment_progress_44b020 del PC: +0x00 posicion de carretera inicial, +0x10 final, +0x34 modo (1 exp,
    2 exp2, 3 lineal), +0x38 inicio, fin y densidad, +0x48 color RGB; +0x54 / +0x64 los mismos al final del tramo.
    Devuelve [(p0, p1, modo, densidad0, densidad1, color0, color1)]"""
    d = A.datos(A.entrada('STAGE.PS2', '\\%s\\scn_env_fog_%s_bin' % (zona, zona)))
    o = struct.unpack_from('<I', d, 4)[0]; res = []
    while o + 0xb0 <= len(d) and len(res) < NIEBLA_MAX:
        p0 = struct.unpack_from('<H', d, o)[0]
        if p0 == 0xffff: break
        if p0 != 0xfffe:
            p1 = struct.unpack_from('<H', d, o + 0x10)[0]; modo = struct.unpack_from('<I', d, o + 0x34)[0]
            d0 = struct.unpack_from('<f', d, o + 0x40)[0]; d1 = struct.unpack_from('<f', d, o + 0x5c)[0]
            c0 = 0xFF000000 | int.from_bytes(d[o + 0x48:o + 0x4b], 'big')
            c1 = 0xFF000000 | int.from_bytes(d[o + 0x64:o + 0x67], 'big')
            res.append((p0, p1, modo, d0, d1, c0, c1))
        o += 0xb0
    return res

def m_por_posicion(seg):
    """metros de ruta por "posicion de carretera" (seccion 5 de la colision: una cada ~8 m)"""
    for h, o, s in seg.ent:
        d = bytes(seg.m[o:o + s])
        if d[:8] != b'COLI0200': continue
        n, nprim = struct.unpack('<2I', d[8:16]); sec = struct.unpack('<8I', d[20:52])
        L = np.frombuffer(d[sec[5]:sec[5] + 2 * n], '<u2')[:nprim]
        return conv_tramo.longitud(conv_tramo.recorrido_coli(seg)) / max(1, int(L.max()))
    return 8.0

def cambia_texturas(mod, base, nuevas):
    """sustituye las texturas base + i del modelo por las imagenes RGBA de nuevas {i: imagen} (carteles)"""
    for i, a in nuevas.items():
        fmt, li, d = modelo.textura_dc(a)
        mod.texs[base + i] = (a.shape[1], a.shape[0], fmt, li, len(mod.blob), len(d)); mod.blob += d
        mod.rgba[base + i] = a

def carteles(A, mod, izq, der):
    """objetos de la bifurcacion (COMMON obj_course_obj_bk_<zona>, en el disco solo el de PALM: los porticos de los
    carteles) con los carteles de los dos destinos: sus cuatro texturas de 8x8 son huecos que el juego llena con las de
    obj_OBJ_<ZONA>_BUNKI_TX de cada destino (icono, nombre con flecha a la izquierda, nombre con flecha a la derecha).
    Por la posicion de cada malla: 0 icono del derecho (x +10..+13), 1 icono del izquierdo (x -13..-10), 2 nombre del
    izquierdo (x -10..-4), 3 nombre del derecho (x +4..+10)"""
    from ps2 import tex
    e = A.entrada('COMMON.PS2', 'obj_course_obj_bk_palm_pmt.sz'); blq = A.bloque(e); base = len(mod.texs)
    mod.bloque(blq, objetos=True, sin_textura='material')
    def tx(z):
        b = A.bloque(A.entrada('COMMON.PS2', 'obj_OBJ_%s_BUNKI_TX_pmt.sz' % z))
        return [tex.imagen(b, h) for h in tex.cabeceras(b)]
    ti, td = tx(izq), tx(der)
    cambia_texturas(mod, base, {0: td[0], 1: ti[0], 2: ti[1], 3: td[2]})

def escribe_curso(sal, regs):
    with open(sal, 'wb') as f:
        f.write(b'CUR1' + struct.pack('<2I', len(regs), REG))
        for r in regs:
            b = r['zona'].encode()[:4].ljust(4, b'\0')
            b += struct.pack('<BBbb', r['seg'], r['nivel'], r['izq'], r['der'])
            b += struct.pack('<4f', *r['bk']) + struct.pack('<8f', *r['ramas'][0], *r['ramas'][1])
            b += struct.pack('<2f', r['m_pos'], r['meta']) + struct.pack('<I', len(r['niebla']))
            for p0, p1, modo, d0, d1, c0, c1 in r['niebla']: b += struct.pack('<2HB3x2f2I', p0, p1, modo, d0, d1, c0, c1)
            b += b'\0' * (REG - len(b)); f.write(b)

def curso(A, dsal, con_tramos=True):
    """todo el arbol del modo (csc_data_2: 15 etapas en 5 niveles): SEGnn.DAT de cada etapa en su marco (empieza en
    el origen hacia -z), BKkk.DAT la union de la etapa de clave kk en su marco (la bifurcacion con los carteles de sus
    dos destinos, o la meta en las etapas finales), BKD.DAT la ruta de la rama derecha de la bifurcacion (la misma en
    todas) y CURSO.DAT el arbol: por etapa su zona, fichero, nivel, hijos, colocacion de la union en la etapa (+0x98
    del descriptor), colocacion del principio de cada rama en la union, metros por posicion de carretera, sitio de la
    meta en la union y niebla. La demo encadena las etapas con esas colocaciones y recentra el marco en cada union."""
    E = A.elf; base, nprim, base2 = tablas(A)
    descs = [u32(E, base + 4 * i) for i in range(nprim)]
    regs = arbol(A, 0); porclave = {r[0]: r for r in regs}
    nivel = {regs[0][0]: 0}; cola = [regs[0][0]]
    while cola:                                       # nivel = profundidad en el arbol (0..4)
        k = cola.pop(0)
        for h in porclave[k][3:5]:
            if h >= 0 and h not in nivel: nivel[h] = nivel[k] + 1; cola.append(h)
    segs = A.segmentos()
    # la bifurcacion (la misma geometria y colisiones en todas): rutas de las dos ramas y el principio de cada etapa
    # siguiente en el marco de la union
    d_todo, d_izq, d_der = (coli_bytes(A, '\\BK\\coli_%s_PALM_bin' % x) for x in ('BK', 'LBK', 'RBK'))
    ri, _ = ruta_rama(d_izq, d_todo); rd, _ = ruta_rama(d_der, d_todo)
    md = modelo.Modelo(); md.ruta = rd; md.escribe(os.path.join(dsal, 'BKD.DAT'))
    ramas = []
    for dr in (d_izq, d_der):
        p, a = fin_rama(dr); ramas.append((float(p[0]), float(p[1]), float(p[2]), a))
    sal = []
    for clave, ti, ui, hi, hd in regs:
        d0 = descs[ti]; ruta_c = cadena(E, d0 + 0x0c); z = zona_de(ruta_c); n = segmento_de(A, ruta_c)
        tx, ty, tz, rx, ry, rz = f32(E, d0 + 0x98, 6)
        assert abs(rx) < 1e-4 and abs(rz) < 1e-4, 'union con giro en x o z: no soportado'
        dbk = u32(E, base2 + 4 * ui); c_todo = cadena(E, dbk + 0x0c)
        mod = modelo.Modelo(); mod.limita = True
        if hi >= 0:                                   # bifurcacion: la de PALM (las demas no estan en el disco)
            mod.bloque(A.bloque(A.entrada('BK.PS2', 'cs_BK_PALM_pmt')), objetos=True, sin_textura='material')
            mod.bloque(A.bloque(A.entrada('BK.PS2', '\\ENV\\cs_ENVBK_PALM_pmt')), objetos=True, sin_textura='material')
            zi = zona_de(cadena(E, descs[porclave[hi][1]] + 0x0c)); zd = zona_de(cadena(E, descs[porclave[hd][1]] + 0x0c))
            carteles(A, mod, zi, zd)
            mod.ruta = ri.copy(); meta = -1.0
        else:                                         # meta: cs_BK_<zona> con su colision (una calzada)
            nm = c_todo.split('\\')[-1].replace('coli_', 'cs_').replace('_bin', '_pmt')
            mod.bloque(A.bloque(A.entrada('BK.PS2', nm)), objetos=True, sin_textura='material')
            dm = coli_bytes(A, c_todo); mod.ruta, _ = ruta_rama(dm, dm)
            meta = 0.0                                # la pancarta GOAL esta en el principio (z = 0)
            zi = zd = '-'
        for blq in segs[n].bloques:                   # el cielo de la etapa que se deja
            if blq.o != segs[n].o and conv_tramo.es_cielo(blq): mod.bloque(blq, lista=3, sin_textura='material')
        mod.recorta_texturas()
        f = os.path.join(dsal, 'BK%02d.DAT' % clave)
        import vq; umbral = vq.UMBRAL
        # la meta y la etapa final no caben a la vez en la memoria de video con las texturas grandes de la meta sin
        # comprimir (las que pasan del error RMS 9: 512x512 del publico, 256x256 de METR): en la meta se aceptan
        # hasta UMBRAL_META (docs/DREAMCAST.md)
        if hi < 0: vq.UMBRAL = max(umbral, UMBRAL_META)
        try: mod.escribe(f)
        finally: vq.UMBRAL = umbral
        r = dict(zona=z, seg=n, nivel=nivel[clave], izq=hi, der=hd, bk=(tx, ty, tz, ry), ramas=ramas,
                 m_pos=m_por_posicion(segs[n]), meta=meta, niebla=niebla(A, z))
        sal.append(r)
        print('etapa %2d %s SEG%02d nivel %d hijos %2d %2d union %s (%.1f, %.1f, %.1f, %.3f) %s/%s %s: %d mallas, '
              '%d texturas (%d KB), %d KB; %.2f m/pos, %d tramos de niebla'
              % (clave, z, n, nivel[clave], hi, hd, c_todo.split('\\')[-1], tx, ty, tz, ry, zi, zd, os.path.basename(f),
                 len(mod.mallas), len(mod.texs), len(mod.blob) >> 10, os.path.getsize(f) >> 10, r['m_pos'], len(r['niebla'])))
    escribe_curso(os.path.join(dsal, 'CURSO.DAT'), sal)
    if con_tramos:
        for r in sal: conv_tramo.convierte(segs[r['seg']], r['seg'], os.path.join(dsal, 'SEG%02d.DAT' % r['seg']))

def main():
    """conv_union.py tu.iso salida/ [--sin-tramos]: todo el arbol del curso (curso); --sin-tramos: sin convertir las
    etapas (SEGnn.DAT, con conv_tramo.py aparte)"""
    arg = [a for a in sys.argv[1:] if not a.startswith('--')]
    curso(Assets(arg[0]), arg[1], '--sin-tramos' not in sys.argv)

if __name__ == '__main__':
    main()
