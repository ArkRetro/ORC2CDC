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

Salida (formato ORD7 de modelo.py, cada fichero en su propio marco; la demo los coloca al leerlos):
 SEGnn.DAT las etapas (n = segmento de STAGE.PS2; conv_tramo.py), empiezan en el origen hacia -z
 BKkk.DAT  la union de la etapa de clave kk: bifurcacion (cs_BK_PALM: tronco y ramas, sin su entorno cs_ENVBK_PALM, que
           es del reflejo del coche; los porticos de COMMON obj_course_obj_bk_palm con los carteles obj_OBJ_<ZONA>_BUNKI_TX de sus dos destinos, el cielo de la
           etapa y la ruta de la rama izquierda) o meta (cs_BK_<zona> con su colision, el cielo de la etapa y su ruta)
 BKD.DAT   solo la ruta de la rama derecha de la bifurcacion (la misma en todas)
 CURSO.DAT el arbol: zona, SEGnn, nivel, hijos, colocaciones, metros por posicion de carretera, meta, niebla y entorno
           (banderas de efectos, transicion de la union, las tres luces, niebla del cielo)
 SOL.DAT   el sol y el destello (banco 0x57: sol_dat)
Las rutas de la union salen de la colision: el poligono principal de cada seccion (8 m) da el centro y los bordes de
la calzada; limites, franjas y alturas como en conv_tramo.py (superficies), con la colision de las dos ramas; los
carriles, los del trafico del original (tablas_so). --solo-carriles: solo esos carriles en lo ya convertido."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps2.assets import Assets, hash_nombre
from ps2 import stage
import modelo, conv_tramo, pvs
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

def objetos_etapa(A, d0):
    """[(bloque, {registro})]: lo que la geometria de la etapa del descriptor d0 dibuja de mas en su marco. PC
    race_area_geometry_44de10: p.obj([5D4DC8 + [desc] * 4]) sin condiciones (la decompilacion lo llama "sky"); PS2
    0x172780: la tabla 0x3f9b90 (lui/addiu fp, -0x6470), un id por descriptor: recurso << 16 | registro, -1 sin el.
    El recurso es el obj_course_obj_cs_<zona> de la zona (tabla de recursos 0x58 + zona): su registro 1 lleva la pancarta
    CHECK POINT del principio de la etapa (z = 0, 9 m de alto) con sus postes, los carteles SLOW DOWN y las flechas de
    las curvas. La tabla se localiza por su contenido (0x580001, 0x590001, 0x5a0001: PALM, LAKE, INDU). Los otros
    registros no son de la escena: el de la tabla 0x3f9c98 (PC 5D4ED0) solo va en el cubo del reflejo (44CD30), el 0
    (0x3fa1d0 + id * 0x14, PC 5D5408) es el cartel de los destinos del final con las texturas de los BUNKI (PENDIENTE)"""
    import conv_coche
    E = A.elf
    t = E.busca(struct.pack('<3I', 0x580001, 0x590001, 0x5a0001))
    if not t: raise SystemExit('no se encuentra la tabla de objetos de etapa (0x3f9b90)')
    v = u32(E, t[0] + 4 * u32(E, d0))
    if v == 0xffffffff: return []
    ruta = E.cadena(u32(E, conv_coche.recursos(A) + 4 * (v >> 16)))
    ruta = ruta[len('\\Stage'):] if ruta.lower().startswith('\\stage\\') else ruta
    return [(A.bloque(A.entrada('STAGE.PS2', ruta)), {v & 0xffff})]

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

# Carriles de la union del original: las tablas de muestras que lee 470120 (PC: 0x80A670, 12 bloques de 0x70C por fila,
# ficheros "\\OCP\\ocp" + grupo + "_bra" + curso + "_othcar" + "_01".."_12" + "_tgt.sz" de 44C680; en SLES-53998 son
# las entradas sin nombre de BK.PS2 que empiezan por 'SO' + u16 numero de muestras): una muestra de 6 bytes por fila
# de la union (x, z, y con el bit 15 = "no hay carril", en 1/4096 km = 0,244140625 m: K62818C x K5A29EC), en el marco
# de la union. Carriles 0..11: en la bifurcacion (242 muestras) 0..2 siguen la rama izquierda y 3..5 la derecha; 6..8
# (izquierda) y 9..11 (derecha) solo existen en las ultimas filas, donde cada rama pasa a 6 carriles; al pasar a la
# etapa siguiente (46FE70) la rama izquierda lleva 0..2 -> 3..5 y 6..8 -> 0..2, la derecha 3..5 -> 0..2 y 9..11 ->
# 3..5. En la meta (86 muestras, 'SOV') 6 carriles rectos. Los grupos estan repetidos (las 12 de la bifurcacion dos
# veces, las de la meta tres: _othcar / _rvlcar).
SO_ESCALA = conv_tramo.SO_ESCALA

def tablas_so(A):
    """{numero de muestras: [12 arrays (n, 3) x, z, valido]} con el primer grupo de 12 entradas 'SO' de BK.PS2 de cada
    tamano"""
    E = A.paquete('BK.PS2'); res = {}; i = 0
    while i + 12 <= len(E):
        ds = [bytes(A.datos(E[i + k], 4)) for k in range(12)]
        if all(d[:2] == b'SO' for d in ds):
            g = []
            for k in range(12):
                d = bytes(A.datos(E[i + k])); n = struct.unpack_from('<H', d, 2)[0]
                v = np.frombuffer(d[4:4 + 6 * n], '<u2').reshape(-1, 3)
                g.append(np.stack([v[:, 0].astype(np.int16) * SO_ESCALA, v[:, 1].astype(np.int16) * SO_ESCALA,
                                   (v[:, 2] & 0x8000) == 0], 1))
            res.setdefault(len(g[0]), g); i += 12
        else: i += 1
    return res

def carriles_union(so, cam):
    """las 6 columnas de carriles de la ruta de la union para la demo: los carriles 0..5 del original (en la ruta de
    la rama izquierda 3..5 se van por la otra rama y al reves; la demo los cierra al salirse de la calzada)"""
    return conv_tramo.carriles_originales(so[:6], cam)

class Coli:
    """lo minimo de un segmento para conv_tramo.superficies: una entrada COLI0200"""
    def __init__(self, d): self.m = d; self.ent = [(0, 0, len(d))]

def ruta_rama(d_rama, d_todo, so=None):
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
    # carriles: los del original (tablas_so) si estan; si no, repartidos por la anchura del asfalto
    car = carriles_union(so, cam) if so is not None else conv_tramo.carriles_uniformes(an)
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
# bytes de un registro de CURSO.DAT: el de antes (con el sol de la sombra) y el entorno (escribe_curso)
REG = 4 + 4 + 16 + 32 + 8 + 4 + 24 * NIEBLA_MAX + 8 + 4 + 4 + 4 + 3 * 64 + 4 + 24 * NIEBLA_MAX

def zona_de(ruta):
    """codigo de zona de una ruta del ejecutable (\\Stage\\ZONA\\...)"""
    return ruta.split('\\')[2]

def niebla(A, zona, lista=1):
    """tramos de niebla de la etapa (scn_env_fog_<zona>, lista 1: la de la carretera, entorno 1 del original; lista
    0/2: lo de lejos y los coches). Registros de 0xb0 bytes hasta la marca 0xffff (0xfffe: hueco), como los lee
    course_environment_progress_44b020 del PC: +0x00 posicion de carretera inicial, +0x10 final, +0x34 modo (1 exp,
    2 exp2, 3 lineal), +0x38 inicio, fin y densidad, +0x48 color RGB; +0x54 / +0x64 los mismos al final del tramo.
    Devuelve [(p0, p1, modo, densidad0, densidad1, color0, color1)]"""
    d = A.datos(A.entrada('STAGE.PS2', '\\%s\\scn_env_fog_%s_bin' % (zona, zona)))
    o = struct.unpack_from('<I', d, 4 * lista)[0]; res = []
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

def sol(A, zona):
    """luz del sol de la etapa que proyecta la sombra del coche: la luz 1 (lista 1 de scn_env_sun_<zona>, la de la
    carretera; el coche usa 4082B0(1, 0, 0) del PC: luz 1 del entorno 0). Registros de 0xb0 bytes como los de la
    niebla (course_environment_progress_44b020 del PC, carril 1): +0x30 16 floats al principio (+0x70 al final): difuso,
    especular, +6C, +78, inclinacion y rumbo (grados, +0x60 / +0x64), +8C y alcance. Direccion = (0, 0, -1) girada en x
    por la inclinacion y en y por el rumbo (44A430), en el marco de la etapa. En las 15 etapas la lista 1 tiene un solo
    registro con los mismos valores al principio y al final. Devuelve (inclinacion, rumbo) en grados"""
    d = A.datos(A.entrada('STAGE.PS2', '\\%s\\scn_env_sun_%s_bin' % (zona, zona)))
    o = struct.unpack_from('<I', d, 4)[0]
    while o + 0xb0 <= len(d):
        p0 = struct.unpack_from('<H', d, o)[0]
        if p0 == 0xffff: break
        if p0 != 0xfffe: return struct.unpack_from('<2f', d, o + 0x60)
        o += 0xb0
    return (0.0, 0.0)

def soles(A, zona):
    """las tres luces del entorno de la etapa (scn_env_sun_<zona>, listas 0, 1 y 2; 16 floats de +0x30 del primer
    registro: difuso, especular, color del lado de la luz +6C, del lado de la sombra +78, inclinacion, rumbo, +8C y
    potencia). Que es cada una: el entorno e del original (40D840(e): niebla 7D3A10 + e 0x1C y luces 899B98 + e 0xA0)
    lo elige cada capa del dibujo (449050, tabla 5A27B4 {1,1,-1,0,1,1,2,2,2}): el 0 es el de la capa 3 (el cielo), el 1
    el de las capas de la escena (la etapa y su reflejo) y el 2 el de los coches (46C140 / 46BD30 llaman 40D840(2)).
    En las 15 etapas cada lista tiene un solo registro con la posicion inicial igual a la final (fijo en toda la etapa:
    course_environment_progress_44b020 usa entonces los valores del principio)"""
    d = A.datos(A.entrada('STAGE.PS2', '\\%s\\scn_env_sun_%s_bin' % (zona, zona))); res = []
    for k in range(3):
        o = struct.unpack_from('<I', d, 4 * k)[0]; v = [0.0] * 16
        while o + 0xb0 <= len(d):
            p0 = struct.unpack_from('<H', d, o)[0]
            if p0 == 0xffff: break
            if p0 != 0xfffe: v = list(struct.unpack_from('<16f', d, o + 0x30)); break
            o += 0xb0
        res.append(v)
    return res

def cielo_de(A, E, d0):
    """bloque del cielo de la etapa y sus objetos (descriptor de tramo: +0x44 el banco obj_course_obj_sky_<zona>,
    +0x4C / +0x50 / +0x54 los objetos que dibuja el cielo (race_sky_ids_44c420 / 4521C0 del PC; 0xFFFFFFFF: ninguno),
    +0x48 el del reflejo del coche): (bloque, registros del grupo 0, del grupo 1) para conv_tramo.pon_cielo"""
    import conv_coche
    rec = u32(E, d0 + 0x44); ids = [u32(E, d0 + o) for o in (0x4c, 0x50, 0x54)]
    nombre = E.cadena(u32(E, conv_coche.recursos(A) + 4 * rec))       # \Stage\ZONA\obj_course_obj_sky_<zona>_pmt
    blq = A.bloque(A.entrada('STAGE.PS2', '\\' + nombre.split('\\', 2)[2]))
    regs = [i & 0xffff if i != 0xffffffff and i >> 16 == rec else None for i in ids]
    return blq, [r for r in regs[:2] if r is not None], [r for r in regs[2:] if r is not None]

def transicion(A, E, d0):
    """cambio de entorno al entrar en la etapa (descriptor de tramo +0x58: u16 posicion de carretera de la union a la
    que empieza y s8 segundos que dura; 44C610 / 44C640 del PC, 100 y 2 si no hay) y banderas de efectos (+0x3C,
    weather_44bd50: 1 lluvia, 2 sol con destello, 4 sombra, 8 faros, 0x10 luces del coche)"""
    p = u32(E, d0 + 0x58)
    pos, seg = struct.unpack('<Hb', E.lee(p, 3)) if p else (100, 2)
    return pos, seg, u32(E, d0 + 0x3c)

def metros_a_posicion(d_rama, pos):
    """metros desde el principio de la union hasta el centro de la seccion pos de la colision de una rama (la
    "posicion de carretera" car+0x64 dentro de la union)"""
    V, L = principales(d_rama); secs = sorted(set(L))
    C = np.array([V[L == l].reshape(-1, 3).mean(0) for l in secs])
    Ls = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(C, axis=0), axis=1))]) + np.linalg.norm(C[0, [0, 2]])
    return float(np.interp(pos, secs, Ls))

def luces(sol, ry=0.0):
    """(luz de la escena, luz del cielo) del entorno para modelo.py, en un marco girado ry respecto al de la etapa"""
    def una(v):
        L = modelo.luz_entorno(v); d = L['dir']; c, s_ = np.cos(-ry), np.sin(-ry)
        L['dir'] = np.array([d[0] * c + d[2] * s_, d[1], -d[0] * s_ + d[2] * c]); return L
    return una(sol[1]), una(sol[0])

def sol_dat(A, sal):
    """SOL.DAT: el sol y el destello del original (40CBC0 de la decompilacion del PC, pase del sol de SCN_EFC): objetos
    del banco 0x57 (\\Common\\obj_course_obj_common_pmt): 2 el disco, 6 el brillo, 0x0F y 0x10 los del sol a lo lejos,
    3..5 y 7..0x0E los destellos. Cada registro con su numero en el campo del nodo y el alfa de su material en el del
    color de sus vertices (la demo los tine y les da la intensidad)"""
    import conv_coche
    nombre = E_ = A.elf; nombre = E_.cadena(E_.u32(conv_coche.recursos(A) + 4 * 0x57)).split('\\')[-1]
    blq = A.bloque(A.entrada('COMMON.PS2', nombre)); mod = modelo.Modelo()
    for r in range(2, 0x11):
        k0 = len(mod.mallas)
        mod.bloque(blq, registros={r}, lista=2, sin_textura=0xFFFFFFFF, nodo_fijo=r)
        reg = blq.registros()[r]; k = struct.unpack('<i', blq.m[blq.A + reg[2][1]:blq.A + reg[2][1] + 4])[0]
        mat = modelo.material_de(blq, reg, k); a = 255 if mat is None else min(255, int(round(mat[3] * 2)))
        for v in mod.verts[k0:]: v['c'] = (np.uint32(a) << 24) | np.uint32(0xFFFFFF)
    mod.recorta_texturas()
    # cada registro anade las texturas del bloque otra vez: una sola copia de cada una (memoria de video)
    uno = {}; texs = []; rgba = []; blob = b''; nuevo = {}
    for ti, (w, h, fmt, li, off, tam) in enumerate(mod.texs):
        k = mod.rgba[ti].tobytes()
        if k not in uno:
            uno[k] = len(texs); texs.append((w, h, fmt, li, len(blob), tam)); blob += mod.blob[off:off + tam]; rgba.append(mod.rgba[ti])
        nuevo[ti] = uno[k]
    for x in mod.mallas: x[1] = nuevo.get(x[1], -1)
    mod.texs, mod.rgba, mod.blob = texs, rgba, blob
    mod.escribe(sal)
    print('SOL.DAT: %d mallas, %d texturas (%d KB)' % (len(mod.mallas), len(mod.texs), len(mod.blob) >> 10))

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
            b += b'\0' * (4 + 4 + 16 + 32 + 8 + 4 + 24 * NIEBLA_MAX - len(b)) + struct.pack('<2f', *r['sol'])
            # entorno: banderas de efectos, transicion (posicion y segundos), metros de la union hasta ella, las tres
            # luces (soles) y la niebla del cielo (lista 0)
            b += struct.pack('<IHbxf', r['clima'], r['trans'][0], r['trans'][1], r['trans_m'])
            for v in r['soles']: b += struct.pack('<16f', *v)
            b += struct.pack('<I', len(r['niebla0']))
            for p0, p1, modo, d0, d1, c0, c1 in r['niebla0']: b += struct.pack('<2HB3x2f2I', p0, p1, modo, d0, d1, c0, c1)
            b += b'\0' * (REG - len(b)); assert len(b) == REG; f.write(b)

def curso(A, dsal, con_tramos=True, solo_curso=False):
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
    so = tablas_so(A)
    ri, _ = ruta_rama(d_izq, d_todo, so.get(242)); rd, _ = ruta_rama(d_der, d_todo, so.get(242))
    md = modelo.Modelo(); md.ruta = rd; md.escribe(os.path.join(dsal, 'BKD.DAT'))
    ramas = []
    for dr in (d_izq, d_der):
        p, a = fin_rama(dr); ramas.append((float(p[0]), float(p[1]), float(p[2]), a))
    sal = []; cielos = {}
    for clave, ti, ui, hi, hd in regs:
        d0 = descs[ti]; ruta_c = cadena(E, d0 + 0x0c); z = zona_de(ruta_c); n = segmento_de(A, ruta_c)
        sol3 = soles(A, z); tpos, tseg, clima = transicion(A, E, d0); cielos[n] = (cielo_de(A, E, d0), sol3)
        # la transicion es la de la etapa a la que se entra: en la union de esta etapa, la de sus hijos (iguales en
        # las 15: posicion 100, 2 s); metros hasta ella en la rama izquierda (la derecha es simetrica)
        th = transicion(A, E, descs[porclave[hi][1]]) if hi >= 0 else None
        ent = dict(clima=clima, soles=sol3, niebla0=niebla(A, z, 0),
                   trans=(th[0], th[1]) if th else (tpos, tseg), trans_m=metros_a_posicion(d_izq, th[0]) if th else -1.0)
        tx, ty, tz, rx, ry, rz = f32(E, d0 + 0x98, 6)
        assert abs(rx) < 1e-4 and abs(rz) < 1e-4, 'union con giro en x o z: no soportado'
        dbk = u32(E, base2 + 4 * ui); c_todo = cadena(E, dbk + 0x0c)
        mod = modelo.Modelo()
        if solo_curso:                                # solo CURSO.DAT (las uniones ya estan)
            meta = -1.0 if hi >= 0 else 0.0
            r = dict(zona=z, seg=n, nivel=nivel[clave], izq=hi, der=hd, bk=(tx, ty, tz, ry), ramas=ramas,
                     m_pos=m_por_posicion(segs[n]), meta=meta, niebla=niebla(A, z), sol=sol(A, z), **ent)
            sal.append(r); print('etapa %2d %s sol %.1f %.1f clima %x transicion %d (%.0f m) %d s'
                                 % (clave, z, *r['sol'], clima, *r['trans'][:1], r['trans_m'], r['trans'][1])); continue
        if hi >= 0:                                   # bifurcacion: la de PALM (las demas no estan en el disco)
            # registros: 0 el tronco, 1 la rama izquierda, 2 la derecha; el original dibuja el 0 y, pasada la posicion
            # 0x1C de la union, solo el de la rama del coche (race_area_stage_draw_44e290: obj | 1 o | 2 segun 451350).
            # El entorno \ENV\cs_ENVBK_PALM no es de la escena: solo lo dibuja el cubo del reflejo del coche
            # (race_env_model_44cd30), y repite la calzada a la misma altura (parpadeaba)
            mod.luz = luces(sol3, ry)[0]                 # la luz de la etapa que se deja, en el marco de la union
            mod.bloque(A.bloque(A.entrada('BK.PS2', 'cs_BK_PALM_pmt')), objetos=True, sin_textura='material', rama={1: 1, 2: 2})
            mod.luz = None
            zi = zona_de(cadena(E, descs[porclave[hi][1]] + 0x0c)); zd = zona_de(cadena(E, descs[porclave[hd][1]] + 0x0c))
            carteles(A, mod, zi, zd)
            mod.ruta = ri.copy(); meta = -1.0
        else:                                         # meta: cs_BK_<zona> con su colision (una calzada)
            nm = c_todo.split('\\')[-1].replace('coli_', 'cs_').replace('_bin', '_pmt')
            mod.luz = luces(sol3, ry)[0]
            mod.bloque(A.bloque(A.entrada('BK.PS2', nm)), objetos=True, sin_textura='material')
            mod.luz = None
            dm = coli_bytes(A, c_todo); mod.ruta, _ = ruta_rama(dm, dm, so.get(86))
            meta = 0.0                                # la pancarta GOAL esta en el principio (z = 0)
            zi = zd = '-'
        # el cielo de la etapa que se deja: va con la camara y orientado con la etapa (el original lo dibuja con la
        # matriz de la etapa, 7D2DA0, guardada al cambiar de entorno: 451A30 / 451E90); la demo gira la lista 3 de la
        # union con la union, asi que aqui va girado -ry
        k0 = len(mod.mallas); conv_tramo.pon_cielo(mod, cielo_de(A, E, d0), luces(sol3)[1])
        c_, s_ = np.cos(ry), -np.sin(ry)
        for x, v in zip(mod.mallas[k0:], mod.verts[k0:]):
            vx, vz = v['x'].astype(float), v['z'].astype(float)
            v['x'] = vx * c_ + vz * s_; v['z'] = -vx * s_ + vz * c_
            x[2] = np.array([x[2][0] * c_ + x[2][2] * s_, x[2][1], -x[2][0] * s_ + x[2][2] * c_])
        mod.recorta_texturas()
        f = os.path.join(dsal, 'BK%02d.DAT' % clave)
        import vq; umbral = vq.UMBRAL
        # la meta y la etapa final no caben a la vez en la memoria de video con las texturas grandes de la meta sin
        # comprimir (las que pasan del error RMS 9: 512x512 del publico, 256x256 de METR): en la meta se aceptan
        # hasta UMBRAL_META (docs/DREAMCAST.md)
        if hi < 0: vq.UMBRAL = max(umbral, UMBRAL_META)
        try: mod.escribe(f)
        finally: vq.UMBRAL = umbral
        # visibilidad precalculada (tools/pvs.py): en la bifurcacion el ojo tambien va por la rama derecha (BKD.DAT)
        pvs.anade(f, [rd] if hi >= 0 else [])
        print('triangulos de las tiras del VU1: %d, sin dibujar por la bandera ADC: %d' % (mod.tri_total, mod.tri_adc))
        r = dict(zona=z, seg=n, nivel=nivel[clave], izq=hi, der=hd, bk=(tx, ty, tz, ry), ramas=ramas,
                 m_pos=m_por_posicion(segs[n]), meta=meta, niebla=niebla(A, z), sol=sol(A, z), **ent)
        r['_obj'] = objetos_etapa(A, d0)
        sal.append(r)
        print('etapa %2d %s SEG%02d nivel %d hijos %2d %2d union %s (%.1f, %.1f, %.1f, %.3f) %s/%s %s: %d mallas, '
              '%d texturas (%d KB), %d KB; %.2f m/pos, %d tramos de niebla'
              % (clave, z, n, nivel[clave], hi, hd, c_todo.split('\\')[-1], tx, ty, tz, ry, zi, zd, os.path.basename(f),
                 len(mod.mallas), len(mod.texs), len(mod.blob) >> 10, os.path.getsize(f) >> 10, r['m_pos'], len(r['niebla'])))
    escribe_curso(os.path.join(dsal, 'CURSO.DAT'), sal)
    sol_dat(A, os.path.join(dsal, 'SOL.DAT'))
    if con_tramos and not solo_curso:
        for r in sal:
            cb, sol3 = cielos[r['seg']]
            conv_tramo.convierte(segs[r['seg']], r['seg'], os.path.join(dsal, 'SEG%02d.DAT' % r['seg']), cielo=cb,
                                 luz=luces(sol3), objetos=r['_obj'])

def solo_carriles(A, dsal):
    """cambia en su sitio las columnas de carriles de la ruta de BKnn.DAT y BKD.DAT ya convertidos (formato ORD7 de
    modelo.py: la ruta va tras texturas, mallas y vertices) por los del original, sin volver a convertir los modelos.
    La ruta de cada fichero dice cual es: la rama izquierda acaba en x < 0, la derecha en x > 0, la meta recta"""
    so = tablas_so(A); col = 5 + NPERF; segs = A.segmentos()
    for f in sorted(os.listdir(dsal)):
        etapa = f.startswith('SEG') and f.endswith('.DAT') and f[3:5].isdigit()
        if not ((f.startswith('BK') or etapa) and f.endswith('.DAT')): continue
        ruta_f = os.path.join(dsal, f)
        with open(ruta_f, 'r+b') as fh:
            d = fh.read(60)
            if d[:4] != b'ORD7': continue
            nm, nv, nr, nc, nt = struct.unpack_from('<5I', d, 4); o = 4 + 52 + 24 * nt + 32 * nm + 24 * nv
            fh.seek(o); r = np.frombuffer(fh.read(4 * nr * nc), '<f4').reshape(nr, nc).copy()
            cam = r[:, :3].astype(float); fin = cam[-1]
            if etapa: g = conv_tramo.tablas_carriles(segs[int(f[3:5])])
            else: g = so.get(242) if abs(fin[0]) > 500 else so.get(86)
            if g is None or nc < col + NCARRIL: continue
            car = conv_tramo.carriles_originales(g, cam)
            antes = r[:, col:col + NCARRIL].copy(); r[:, col:col + NCARRIL] = car
            fh.seek(o); fh.write(r.astype('<f4').tobytes())
            print('%s: %d puntos, %s; carriles cambiados (max %.1f m)' % (f, nr, 'etapa' if etapa else 'bifurcacion' if g is so.get(242) else 'meta',
                                                                       float(np.abs(antes - car).max())))

def main():
    """conv_union.py tu.iso salida/ [--sin-tramos] [--solo-curso] [--solo-carriles]: todo el arbol del curso (curso);
    --sin-tramos: sin convertir las etapas (SEGnn.DAT, con conv_tramo.py aparte); --solo-curso: solo CURSO.DAT;
    --solo-carriles: solo los carriles de las rutas de las uniones ya convertidas (solo_carriles)"""
    arg = [a for a in sys.argv[1:] if not a.startswith('--')]
    if '--solo-carriles' in sys.argv: solo_carriles(Assets(arg[0]), arg[1]); return
    curso(Assets(arg[0]), arg[1], '--sin-tramos' not in sys.argv, '--solo-curso' in sys.argv)

if __name__ == '__main__':
    main()
