"""Modelos para la Dreamcast a partir de bloques de PS2 (etapas de STAGE.PS2 y coches de CARS.PS2), formato ORD7:
 'ORD7', nmallas, nvert, nruta, floats por punto de ruta, ntex, inicio de cada lista (opaca, recorte, transparente,
 cielo, fin), nnodos, ncarteles, nsecciones;
 texturas (ancho, alto, formato, lista, desplazamiento, tamano: 24 bytes); formato: bits 0-7 0 ARGB1555 / 1 RGB565 /
 2 ARGB4444, bit 8 comprimida VQ (vq.py: libro de N entradas de 8 bytes y un byte por bloque de 2x2), bit 9 con mipmaps
 (datos_mip / vq.codifica_mip), bit 10 mezcla aditiva, bit 11 malla de la escena con el estado del GS (z GEQUAL, alfa
 de textura x vertice), bits 16-23 N - 1;
 mallas ordenadas por lista y textura (cx, cy, cz, radio float, primer vertice, cuenta, textura o -1, nodo o -1: 32
 bytes);
 vertices (x, y, z, u, v float, color ARGB: 24 bytes); ruta (x, y, z, ancho izquierdo, ancho derecho y el perfil
 de alturas de la calzada a lo ancho: NPERF alturas de -izquierdo-2 a derecho+2...);
 nodos del original (cx, cy, cz, radio, banderas: bits 0-3 umbral de LOD, bit 4 orientado a la camara, bits 8-9 rama;
 cartel o -1, 2 de relleno: 32 bytes); carteles orientados a la camara (matriz 4x4 de vectores fila del nodo; en la
 columna 3 el eje z del padre: 64 bytes; las tiras de esos nodos van en el marco del nodo);
 si hay secciones: inicio de la lista de cada seccion (nsecciones + 1 u32), las listas (u16, nodos visibles en la
 seccion: la tabla de visibilidad del original) y la seccion de cada punto de la ruta (u16), relleno a 4; con el bit 31
 de nsecciones es la visibilidad precalculada por mallas de tools/pvs.py (listas XOR en bytes; ver alli);
 relleno a 32; texeles twiddled de 16 bits o VQ, cada textura alineada a 32.
(ORD6 era lo mismo sin nodos ni secciones y con una distancia maxima en lugar del nodo; ORD5, ademas sin VQ.)
Cada malla es una tira de triangulos que dibuja el GS: una tira del VU1 (un UNPACK de posiciones) partida por las
banderas ADC (vif.no_dibuja); su textura: la tabla 2 del registro da una entrada de la tabla 4 (lista de texturas del
registro, 36 bytes), cuyo cuarto campo es el indice de la textura en el bloque."""
import os, struct, time
import numpy as np
from ps2 import tex, vif

LUZ = np.array([0.3, 0.8, 0.5]) / np.linalg.norm([0.3, 0.8, 0.5])

def twiddle(w, h):
    """orden de los texeles en una textura 'twiddled' de la PowerVR (y en los bits pares, x en los impares;
    las no cuadradas son bloques cuadrados de lado min(w, h) seguidos)"""
    s = min(w, h); y, x = np.mgrid[0:h, 0:w]
    def esp(v):
        r = np.zeros_like(v)
        for b in range(10): r |= ((v >> b) & 1) << (2 * b)
        return r
    blo = (x // s) if w > h else (y // s)
    return blo * s * s + esp(y % s) | (esp(x % s) << 1)

def codifica16(a, fmt, umbral=128):
    """RGBA -> texeles twiddled de 16 bits del formato (0 ARGB1555 con el alfa >= umbral, 1 RGB565, 2 ARGB4444)"""
    al = a[..., 3].astype(np.uint32); h, w = al.shape
    r, g, b = (a[..., k].astype(np.uint32) for k in range(3))
    if fmt == 1: v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    elif fmt == 0: v = ((al >= umbral).astype(np.uint32) << 15) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
    else: v = ((al >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)
    out = np.zeros(w * h, np.uint16); out[twiddle(w, h).ravel()] = v.ravel()
    return out.tobytes()

BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0 + 1 / 32.0

def codifica16_tramada(a, fmt):
    """como codifica16 con un tramado ordenado de 4x4 en el color (los cielos pequenos sin VQ: vq.elige_cielo)"""
    h, w = a.shape[:2]; t = BAYER4[np.arange(h)[:, None] % 4, np.arange(w)[None, :] % 4]
    def q(x, m): return np.clip(np.floor(a[..., x].astype(float) * m / 255.0 + t), 0, m).astype(np.uint32)
    al = a[..., 3].astype(np.uint32)
    if fmt == 1: v = (q(0, 31) << 11) | (q(1, 63) << 5) | q(2, 31)
    elif fmt == 0: v = ((al >= 128).astype(np.uint32) << 15) | (q(0, 31) << 10) | (q(1, 31) << 5) | q(2, 31)
    else: v = (((al * 15 + 127) // 255) << 12) | (q(0, 15) << 8) | (q(1, 15) << 4) | q(2, 15)
    out = np.zeros(w * h, np.uint16); out[twiddle(w, h).ravel()] = v.ravel()
    return out.tobytes()

def textura_dc(a):
    """RGBA -> (formato, lista, datos twiddled de 16 bits). Lista 0 opaca, 1 recorte (alfa de 1 bit), 2 transparente.
    Por el contenido de la textura; las mallas de las etapas y uniones deciden la lista por el estado del GS
    (estado_gs, lista_gs)"""
    al = a[..., 3].astype(int); h, w = al.shape
    if (al >= 240).all() or al.max() - al.min() < 64: lista, fmt = 0, 1   # RGB565 (alfa casi constante: no es
                                                                 # transparencia, lo usa el juego para otra cosa)
    elif ((al < 16) | (al >= 240)).mean() > 0.97: lista, fmt = 1, 0   # ARGB1555
    else: lista, fmt = 2, 2                                      # ARGB4444
    return fmt, lista, codifica16(a, fmt)

def cielos(mallas):
    """texturas del cielo (las de las mallas de la lista 3 con el grupo del cielo en el campo del nodo: -256 el que se
    ve siempre, -257 el del fundido; conv_tramo.pon_cielo): van con tramado (vq.elige_cielo)"""
    return {x[1] for x in mallas if x[0] == 3 and x[1] >= 0 and x[-1] in (-256, -257)}

def vq_umbral2():
    """umbral de la segunda pasada de la VQ (vq.UMBRAL2); VQ2=0 en el entorno la quita (los datos de antes)"""
    import vq
    return None if os.environ.get("VQ2", "1") == "0" else vq.UMBRAL2

def niveles_mip(a):
    """niveles de mip de una textura cuadrada, de 1x1 a la base (promedio de 2x2; la PS2 trae los suyos en el disco,
    hechos de la base del mismo modo)"""
    L = [a.astype(float)]
    while L[-1].shape[0] > 1 and L[-1].shape[1] > 1:
        x = L[-1]; L.append((x[0::2, 0::2] + x[1::2, 0::2] + x[0::2, 1::2] + x[1::2, 1::2]) / 4)
    return [np.clip(np.rint(x), 0, 255).astype(np.uint8) for x in L[::-1]]

def datos_mip(a, fmt, umbral=128):
    """textura de 16 bits con mipmaps de la PowerVR: 6 bytes de relleno y los niveles de 1x1 a la base, cada uno
    twiddled (el nivel de lado 2^k empieza en 0x6, 0x8, 0x10, 0x30, 0xB0, 0x2B0...: la tabla de desplazamientos de
    la PowerVR para las texturas de 16 bits)"""
    return b'\0' * 6 + b''.join(codifica16(x, fmt, umbral) for x in niveles_mip(a))

def estado_gs(blq, reg, k, capa=0):
    """estado del GS de una capa de la entrada k de la tabla 4 del registro (SLES-53998; ver docs/PARIDAD.md, Suelos).
    Tabla 4 (36 bytes): material, dos campos sin descifrar, y (textura, estado) de la capa 1 (+0C, +10), de la capa 2
    (+14, +18; -1 sin ella) y un ultimo par sin descifrar. Tabla 7 (48 bytes por estado): la plantilla del paquete
    del GS que manda el microcodigo del VU1: GIFtag (PRIM en los bits 47-57: bit 4 TME textura, bit 6 ABE mezcla), en
    +08 el registro ALPHA (0x44 (Cs - Cd) As + Cd, la normal; 0x48 Cs As + Cd, la aditiva; 0x58 Cs Ad + Cd y 0x89
    Cd As en las capas 2), +10 TEST (ATE, ATST, AREF con 0x80 = 1, AFAIL, ZTE y ZTST: GEQUAL en todos), +18 CLAMP y
    +20 TEX1 (MXL = niveles de mip - 1, MMAG, MMIN). None si no lo hay"""
    m, A = blq.m, blq.A; n4, o4 = reg[4]; n7, o7 = reg[7]
    if not 0 <= k < n4: return None
    x = struct.unpack('<9i', m[A + o4 + 36 * k:A + o4 + 36 * k + 36])
    ti, j = x[3 + 2 * capa], x[4 + 2 * capa]
    if not 0 <= j < n7: return None
    q = bytes(m[A + o7 + 48 * j:A + o7 + 48 * j + 48])
    prim = (struct.unpack('<Q', q[:8])[0] >> 47) & 0x7ff
    test = struct.unpack('<I', q[16:20])[0]; tex1 = struct.unpack('<Q', q[32:40])[0]
    return dict(textura=ti, prim=prim, abe=bool(prim & 0x40), alpha=q[8], ate=bool(test & 1), atst=(test >> 1) & 7,
                aref=(test >> 4) & 0xff, afail=(test >> 12) & 3, ztst=(test >> 17) & 3, mxl=int((tex1 >> 2) & 7))

def lista_gs(g, a=None):
    """lista de la PowerVR que corresponde al estado del GS g: con mezcla (ABE) la transparente; sin mezcla, con la
    prueba de alfa (ATE con GEQUAL o GREATER) y algun texel por debajo de AREF (a: RGBA 0-255, el alfa del GS 0x80 =
    255), la de recorte; si no, la opaca: sin mezcla ni prueba de alfa el GS escribe el texel entero y el alfa de la
    textura no tapa nada (en las calzadas es la mascara del brillo de la capa 2)"""
    if g['abe']: return 2
    if g['ate'] and g['atst'] in (5, 6) and a is not None and (a[..., 3] < min(255, 2 * g['aref'])).any(): return 1
    return 0

NODO = 56                                                       # registro de nodo de la tabla 0 (PmtNode del PC)
UMBRAL_CURSO = 6                                                # 4044E0(6): umbral de LOD de la etapa (race_area_display_44f120)

def nodos(blq, reg):
    """los nodos de un registro como los recorre el original (walk_404d10 de la decompilacion del PC, pc_render_queue.cpp;
    la tabla 0 es el PmtNode de 56 bytes: +00 banderas, +04 esfera x, y, z, radio, +14 sesgo de LOD, +18 animacion,
    +1C matriz (tabla 6, -1 ninguna), +20 primer hijo, +24 siguiente hermano, +28 malla de cada LOD (tabla 1)):
    desde el nodo 0, hermanos en orden y cada hijo antes que el hermano siguiente; la matriz del nodo se multiplica por
    la del padre (409FD0: nueva = m * actual, vectores fila: v * m * padre). Tabla 1 (16 bytes, la malla del nodo):
    cuenta y primera de las tiras opacas (tabla 3) y de las de mezcla: (n_op, n_mez, primera_op, primera_mez).
    Devuelve [(indice, banderas, matriz 4x4 o None, esfera (c, r) en el marco del bloque, [tiras de la tabla 3])]"""
    m, A = blq.m, blq.A
    n0, o0 = reg[0]; n1, o1 = reg[1]; n6, o6 = reg[6]; n3 = reg[3][0]
    res = []; vistos = set()
    def mat(i):
        return np.frombuffer(m[A + o6 + 64 * i:A + o6 + 64 * i + 64], '<f4').reshape(4, 4).astype(float) if 0 <= i < n6 else None
    def anda(k, padre):
        while 0 <= k < n0 and k not in vistos:
            vistos.add(k)
            x = struct.unpack('<I4f9i', m[A + o0 + NODO * k:A + o0 + NODO * k + NODO])
            M = mat(x[7]); W = padre if M is None else (M if padre is None else M @ padre)
            c = np.array(x[1:4], float); r = float(x[4])
            if W is not None:
                c = c @ W[:3, :3] + W[3, :3]
                r *= float(np.linalg.norm(np.full(3, 0.577350259) @ W[:3, :3]))
            tiras = []
            g = x[10]                                            # malla del LOD 0 (los de PS2 no traen otros)
            if 0 <= g < n1:
                nop, nmez, pop, pmez = struct.unpack('<4i', m[A + o1 + 16 * g:A + o1 + 16 * g + 16])
                for n, p in ((nop, pop), (nmez, pmez)):
                    if n > 0 and p >= 0: tiras += [i for i in range(p, p + n) if i < n3]
            res.append((k, x[0], W, (c, r), tiras, x[7] >= 0))
            if x[8] >= 0: anda(x[8], W)
            k = x[9]
    anda(0, None)
    return res

def material_de(blq, reg, k):
    """color del material de la entrada k de la lista de texturas del registro (tabla 4, 36 bytes: el primer campo es
    el material): tabla 5, 64 bytes, difuso r, g, b, a en unidades del GS (128 = 1), luego ambiente, especular,
    potencia y emision. None si no lo hay"""
    m, A = blq.m, blq.A; n4, o4 = reg[4]; n5, o5 = reg[5]
    if not 0 <= k < n4: return None
    mi = struct.unpack('<i', m[A + o4 + 36 * k:A + o4 + 36 * k + 4])[0]
    if not 0 <= mi < n5: return None
    return tuple(float(x) for x in np.frombuffer(m[A + o5 + 64 * mi:A + o5 + 64 * mi + 16], '<f4'))

def material_completo(blq, reg, k):
    """material entero de la entrada k de la tabla 4 del registro (tabla 5, 64 bytes): difuso RGBA, ambiente RGBA,
    especular RGB y potencia, emision RGBA, en unidades del GS (128 = 1). None si no lo hay"""
    m, A = blq.m, blq.A; n4, o4 = reg[4]; n5, o5 = reg[5]
    if not 0 <= k < n4: return None
    mi = struct.unpack('<i', m[A + o4 + 36 * k:A + o4 + 36 * k + 4])[0]
    if not 0 <= mi < n5: return None
    return np.frombuffer(m[A + o5 + 64 * mi:A + o5 + 64 * mi + 64], '<f4').astype(float)

def luz_entorno(sol, d=None):
    """luz de un entorno del original (un registro de scn_env_sun_<zona>, carril 1 de course_environment_progress_44b020
    del PC: 16 floats de +0x30) para luz_vertices: difuso (+8 de la luz), color del lado de la luz (+6C) y del lado de
    la sombra (+78) y la direccion (hacia donde va la luz) = (0, 0, -1) girada en x por la inclinacion y en y por el
    rumbo (44A430), en el marco del fichero (d la da ya girada si hace falta)"""
    v = np.asarray(sol, float)
    if d is None:
        p, y = np.radians(v[12]), np.radians(v[13])
        d = np.array([-np.cos(p) * np.sin(y), np.sin(p), -np.cos(p) * np.cos(y)])
    return dict(dif=v[0:3], lado=v[6:9], sombra=v[9:12], dir=np.asarray(d, float))

def luz_vertices(luz, N, mat):
    """color de los vertices con la luz del entorno, como el vertex shader de luz del PC (vs_15 de
    pc_shader_programs_generated: R0 = max(n.l, 0), R1 = (c1A n.l + c1B) x ambiente del material + emision, con
    c1A = (+6C - +78) / 2 y c1B = +78 + (+6C - +78) / 2 del 40DD70 / 410810): difuso del material x difuso de la luz x
    max(n.l, 0) + ambiente del material x (+78 + (+6C - +78) (1 + n.l) / 2) + emision; n.l con l hacia la luz. Material
    en unidades del GS (128 = 1; sin material: difuso y ambiente 1). Las normales de los datos de PS2 apuntan hacia
    dentro (la calzada: n.y = -1 en las 269 tiras horizontales del registro 0 de PALM; vs_15 las niega: dp3(-R10, c37)),
    asi que n.l = normal del fichero . direccion de la luz. Devuelve (n, 3), 1 = color neutro"""
    if mat is None: md = ma = np.ones(3); me = np.zeros(3)
    else: md, ma, me = mat[0:3] / 128, mat[4:7] / 128, mat[12:15] / 128
    nl = N.astype(float) @ luz['dir']
    lado = luz['sombra'][None, :] + (luz['lado'] - luz['sombra'])[None, :] * (1 + nl[:, None]) / 2
    return md[None, :] * luz['dif'][None, :] * np.clip(nl, 0, None)[:, None] + ma[None, :] * lado + me[None, :]

def texturas_de_carteles(blq):
    """texturas de los carteles del bloque: apaisadas (2:1) en mallas de 4 a 6 vertices, de 6 a 16 m de ancho y 3 a 9 de alto"""
    m, A = blq.m, blq.A; res = []
    for reg in blq.registros():
        c, o = reg[3]; t2 = reg[2][1]; n4, o4 = reg[4]
        for i in range(c):
            e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24])
            k = struct.unpack('<i', m[A + t2 + 16 * i:A + t2 + 16 * i + 4])[0]
            if not 0 <= k < n4: continue
            ti = struct.unpack('<i', m[A + o4 + 36 * k + 12:A + o4 + 36 * k + 16])[0]
            if ti < 0 or ti in res: continue
            h = tex.cabeceras(blq)[ti] if ti < len(tex.cabeceras(blq)) else None
            if h is None or (h[11] >> 24) < (h[12] & 0xff) + 1: continue      # los anuncios son apaisados (2:1)
            for pk in vif.datos(m, A + e[2], A + blq.w[2]):
                for tr in vif.tiras(pk):
                    P = tr[0x68]; ext = P.max(0) - P.min(0)
                    if 4 <= len(P) <= 6 and 6 < max(ext[0], ext[2]) < 16 and 3 < ext[1] < 9 and ti not in res: res.append(ti)
    return res

class Modelo:
    def __init__(self):
        self.texs = []; self.blob = b''; self.mallas = []; self.verts = []; self.nv = 0; self.ruta = np.zeros((0, 5))
        self.rgba = []                                          # imagen de cada textura (para la compresion VQ)
        self.vq = os.environ.get("VQ", "1") != "0"
        self.vq2 = vq_umbral2()                                 # segunda pasada de la VQ (vq.elige16); None: sin ella
        # nodos del original (bloque con objetos=True): esfera en el marco del fichero, banderas para la demo (bits 0-3
        # umbral de LOD, bit 4 orientado a la camara, bits 8-9 rama de la union: 1 izquierda, 2 derecha) e indice en
        # self.carteles (los orientados a la camara: matriz del nodo y eje z del padre) o -1
        self.nodos = []; self.carteles = []
        self.gid = {}                                           # (id del bloque, registro, nodo) -> indice en self.nodos
        self.secciones = []                                     # por seccion de carretera: lista de nodos visibles
        self.mapa = None                                        # por punto de la ruta: su seccion (u16)
        self.tri_adc = 0; self.tri_total = 0                    # triangulos sin dibujar por la bandera ADC / total
        # luz del entorno del original (luz_entorno) para las mallas sin color de vertice (etapas, uniones y cielo);
        # None: la luz de siempre (LUZ, coches y trafico)
        self.luz = None
        # estado del GS de las mallas de la escena (bloque con objetos=True: etapas y uniones; estado_gs): la lista, la
        # mezcla y los mipmaps salen de el. tvar: textura -> {(lista, aditiva): entrada de self.texs}; tdec: entrada ->
        # lo que necesita (lista, aditiva, umbral del recorte, niveles de mip, en el suelo)
        self.tvar = {}; self.tdec = {}; self._gs = None

    def _variante(self, ti, li, aditiva, umbral, mxl, suelo):
        """la entrada de textura de ti para una malla de la lista li con esa mezcla: la propia la primera vez; si la
        misma textura se usa en otra lista (tres en PALM), una copia (cada cabecera de la PowerVR es de una lista)"""
        v = self.tvar.setdefault(ti, {}); clave = (li, aditiva)
        if clave not in v:
            if not v: j = ti
            else:
                w, h, fmt, _, off, tam = self.texs[ti]; j = len(self.texs)
                self.texs.append((w, h, fmt, li, len(self.blob), tam)); self.blob += self.blob[off:off + tam]
                self.rgba.append(self.rgba[ti])
            v[clave] = j; self.tdec[j] = dict(li=li, aditiva=aditiva, umbral=umbral, mxl=mxl, suelo=0, total=0)
        d = self.tdec[v[clave]]
        d['umbral'] = max(d['umbral'], umbral); d['mxl'] = max(d['mxl'], mxl); d['suelo'] += suelo[0]; d['total'] += suelo[1]
        return v[clave]

    def ajusta_texturas(self):
        """formato de cada textura usada por las mallas con estado del GS (una vez, antes de recortar y comprimir): en
        la lista opaca RGB565 (el alfa no se usa), en la de recorte ARGB1555 con el umbral de AREF, en la transparente
        el de su contenido; bits del formato: 0x200 con mipmaps (las cuadradas del suelo cuyo estado del GS lleva
        niveles: TEX1 MXL > 0; la PowerVR solo los admite en las cuadradas), 0x400 mezcla aditiva (ALPHA 0x48), 0x800
        z con GEQUAL (ZTST del GS)"""
        if getattr(self, '_ajustadas', False): return
        self._ajustadas = True
        if not self.tdec: return
        blob = b''; texs = []
        for j, (w, h, fmt, li, off, tam) in enumerate(self.texs):
            d = self.tdec.get(j)
            if d is None:
                texs.append((w, h, fmt, li, len(blob), tam)); blob += self.blob[off:off + tam]; continue
            a = self.rgba[j]; li = d['li']
            fmt = 1 if li == 0 else (0 if li == 1 else textura_dc(a)[0])
            mip = w == h and w >= 8 and d['mxl'] > 0 and 2 * d['suelo'] >= d['total'] > 0
            datos = datos_mip(a, fmt, d['umbral']) if mip else codifica16(a, fmt, d['umbral'])
            datos += b'\0' * (-len(datos) % 32)
            fmt |= (0x200 if mip else 0) | (0x400 if d['aditiva'] else 0) | 0x800
            texs.append((w, h, fmt, li, len(blob), len(datos))); blob += datos
        self.texs, self.blob = texs, blob

    def bloque(self, blq, registros=None, lista=None, objetos=False, sin_textura=int(os.environ.get("SINTEX", "0x60202020"), 16),
               copias=None, vis=None, umbral=UMBRAL_CURSO, rama=None, nodo_fijo=None, tiras=None, cuadro=0,
               base_tex=None, con_gs=False):
        """anade los registros de un bloque (todos si registros es None). lista: fuerza la lista de sus mallas.
        sin_textura: color ARGB de las mallas sin textura, o None para iluminarlas en blanco por la normal, o
        'material' para darles el color de su material del original (material_de).
        copias: {registro: [desplazamiento (x, y, z), ...]} para repetir un registro (ruedas).
        objetos: como el original, solo las tiras de los nodos (nodos), cada una con la matriz de su nodo; las tiras que
        no son de ningun nodo no se dibujan. vis: {registro: conjunto de nodos} (los que nombra la tabla de visibilidad
        por secciones de la etapa, lee_visibilidad): los demas no se dibujan nunca. umbral: el de LOD del original (6 la
        etapa; la bandera 0x40 del nodo lo sube a 7). rama: {registro: 1 izquierda / 2 derecha} (uniones).
        nodo_fijo: valor del campo del nodo de todas sus mallas (sin objetos; el cielo: su grupo).
        Sin objetos, para los objetos animados del fondo (tools/conv_bo.py): tiras = solo esas de la tabla 3; cuadro = el
        de la animacion de vertices (las tiras con la bandera 0x800 traen 4 paquetes, uno por cuadro, en las palabras 2..5
        de su entrada; las demas solo el primero); base_tex = las texturas de este bloque ya estan en self.texs desde ese
        indice (no se repiten); con_gs = la lista y la mezcla del estado del GS como en las escenas"""
        m, A = blq.m, blq.A
        if base_tex is None:
            base = len(self.texs)
            for h in tex.cabeceras(blq):
                a = tex.imagen(blq, h); fmt, li, d = textura_dc(a)
                self.texs.append((a.shape[1], a.shape[0], fmt, li, len(self.blob), len(d))); self.blob += d
                self.rgba.append(a)
            ntb = len(self.texs) - base
        else: base = base_tex; ntb = len(tex.cabeceras(blq))
        self.base_tex = base
        for r, reg in enumerate(blq.registros()):
            if registros is not None and r not in registros: continue
            c, o = reg[3]; t2 = reg[2][1]
            def tira_de(i):
                e = struct.unpack('<6I', m[A + o + 24 * i:A + o + 24 * i + 24])
                k = struct.unpack('<i', m[A + t2 + 16 * i:A + t2 + 16 * i + 4])[0]
                ti = struct.unpack('<i', m[A + reg[4][1] + 36 * k + 12:A + reg[4][1] + 36 * k + 16])[0] \
                    if 0 <= k < reg[4][0] else -1                # la lista de texturas del registro (tabla 4)
                ti = base + ti if 0 <= ti < ntb else -1
                self._mat = material_de(blq, reg, k) if sin_textura == 'material' and ti < 0 else None
                self._matc = material_completo(blq, reg, k) if self.luz is not None else None
                self._origen = (r, i, k, e[0])                   # registro, malla, entrada de la tabla 4 y banderas
                # estado del GS de la capa 1 (tabla 7): manda la lista, la mezcla y los mipmaps (escena: objetos)
                self._gs = estado_gs(blq, reg, k) if (objetos or con_gs) and lista is None else None
                return e, ti
            if not objetos:
                self._nodo = -1 if nodo_fijo is None else nodo_fijo
                for i in range(c):
                    if tiras is not None and i not in tiras: continue
                    e, ti = tira_de(i)
                    pq = e[2 + cuadro] if cuadro and 0 < cuadro < 4 and e[2 + cuadro] else e[2]
                    for pk in vif.datos(m, A + pq, A + blq.w[2]):
                        for tr in vif.tiras(pk):
                            for dx in (copias or {}).get(r, [(0, 0, 0)]):
                                self._tira(tr, None, ti, lista, sin_textura, np.array(dx, np.float32), blq)
                continue
            visibles = None if vis is None else vis.get(r, set())
            for k, fl, W, (cc, rad), tiras, con_matriz in nodos(blq, reg):
                if visibles is not None and k not in visibles: continue
                if not tiras: continue
                cartel = bool(fl & 0x800) and con_matriz          # 404D10: face_camera solo si el nodo tiene matriz
                u = (7 if fl & 0x40 else umbral) | (0x10 if cartel else 0) | ((rama or {}).get(r, 0) << 8)
                ib = -1
                if cartel:                                       # vertices en el marco del nodo; la demo gira y coloca
                    ib = len(self.carteles); Wc = W.astype(np.float32).copy()
                    Wc[:3, 3] = (0, 0, 1); Wc[3, 3] = 1                  # columna 3: eje z del padre (el del bloque)
                    self.carteles.append(Wc)
                    rad = rad + float(np.linalg.norm(W[3, :3] - cc))
                self.gid[(id(blq), r, k)] = len(self.nodos)
                self._nodo = len(self.nodos); self.nodos.append([cc.astype(np.float32), rad, u, ib])
                for i in tiras:
                    e, ti = tira_de(i)
                    for pk in vif.datos(m, A + e[2], A + blq.w[2]):
                        for tr in vif.tiras(pk):
                            self._tira(tr, None if cartel else W, ti, lista, sin_textura, np.zeros(3, np.float32), blq)

    def _tira(self, tr, M, ti, lista, sin_textura, dx, blq):
        """una tira del VU1 partida en las tiras que dibuja de verdad el GS: el triangulo que cierra un vertice con la
        bandera ADC (vif.no_dibuja: bit 1 de la y de la posicion) no se dibuja; los demas seguidos forman una tira.
        M: matriz del nodo (vectores fila) o None"""
        P = tr[0x68]
        if len(P) < 3 or not np.isfinite(P).all(): return
        dib = vif.triangulos_dibujados(P)
        self.tri_total += len(dib); self.tri_adc += int((~dib).sum())
        if M is not None:
            tr = dict(tr); tr[0x68] = P.astype(float) @ M[:3, :3] + M[3, :3]
            if 0x69 in tr: tr[0x69] = tr[0x69].astype(float) @ M[:3, :3]
        j = 0; n = len(dib)
        while j < n:
            if not dib[j]: j += 1; continue
            a = j
            while j < n and dib[j]: j += 1
            self._trozo(tr, ti, lista, sin_textura, dx, blq, a, j + 2)   # triangulos a..j-1: vertices a..j+1

    def _trozo(self, tr, ti, lista, sin_textura, dx, blq, a, b):
        tr = {k: (v[a:b] if v is not None and k != 0x6c and len(v) == len(tr[0x68]) else v) for k, v in tr.items()}
        P = tr[0x68].astype(np.float32) + dx; nn = len(P)
        uv = np.zeros((nn, 2)); con_uv = False
        for cl in (0x6d, 0x65, 0x64):                           # UV en 1/4096 (16 o 32 bits)
            if cl in tr and len(tr[cl]) == nn: uv = tr[cl][:, :2].astype(float) / 4096; con_uv = True; break
        N = tr[0x69].astype(float) / 32768 if 0x69 in tr and len(tr[0x69]) == nn else np.zeros_like(P)
        lz = luz_vertices(self.luz, N, getattr(self, '_matc', None)) if self.luz is not None else None
        if 0x6e in tr and len(tr[0x6e]) == nn:
            col = np.clip(tr[0x6e][:, :3].astype(float) * 2, 0, 255)
        elif lz is not None:                                    # con textura: 1 = la textura tal cual (GS 128)
            col = np.clip(255 * lz, 0, 255)
        else:
            luz = 0.7 + 0.3 * np.clip(np.abs(N @ LUZ), 0, 1)
            col = np.repeat(255 * luz[:, None], 3, 1)
        li = self.texs[ti][3] if ti >= 0 else 2
        mat = getattr(self, '_mat', None) if isinstance(sin_textura, str) else None
        if ti < 0 and isinstance(sin_textura, str):            # color del material (GS: 255 = blanco, 128 = alfa 1)
            if mat is None: return                               # sin material no hay color que dar: fuera
            if lz is not None: col = np.clip(128 * lz, 0, 255)   # el difuso del material ya va en lz
            else:
                luz = 0.7 + 0.3 * np.clip(np.abs(N @ LUZ), 0, 1)
                col = np.clip(np.array(mat[:3])[None, :] * luz[:, None], 0, 255)
            li = 0 if mat[3] >= 127 else 2
            g = self._gs if lista is None else None
            if g is not None: li = 2 if g['abe'] and mat[3] < 127 else 0   # sin mezcla del GS: opaca
        elif ti < 0 and sin_textura is None: li = 0             # sin textura y sin color dado: blanco con luz
        col = col.astype(np.uint32)
        argb = (0xFF << 24) | (col[:, 0] << 16) | (col[:, 1] << 8) | col[:, 2]
        g = self._gs if lista is None else None
        if g is not None and ti >= 0:                           # escena: la lista y la mezcla del estado del GS
            li = lista_gs(g, self.rgba[ti])
            if g['abe'] and 0x6e in tr and len(tr[0x6e]) == nn:   # con mezcla, el alfa del vertice (GS: 0x80 = 1)
                al = np.clip(tr[0x6e][:, 3].astype(np.int64) * 2, 0, 255).astype(np.uint32)
                argb = (al << 24) | (argb & 0xFFFFFF)
            suelo = (int((np.abs(N[:, 1]) > 0.7).sum()), nn)    # vertices de suelo (para los mipmaps)
            ti = self._variante(ti, li, g['abe'] and g['alpha'] == 0x48, min(255, 2 * g['aref']) if li == 1 else 128,
                                g['mxl'], suelo)
        if ti < 0 and isinstance(sin_textura, str) and li == 2:
            argb = (np.uint32(min(255, int(mat[3] * 2))) << 24) | (argb & 0xFFFFFF)
        elif ti < 0 and sin_textura is not None and not isinstance(sin_textura, str): argb[:] = sin_textura
        if lista is not None: li = lista
        cc = P.mean(0); rad = float(np.sqrt(((P - cc) ** 2).sum(1).max()))
        self.mallas.append([li, ti, cc, rad, self.nv, nn, blq, getattr(self, '_nodo', -1)])
        if getattr(self, 'extra', None) is not None:            # coches (conv_coche.py): origen, UV y normales
            self.extra.append(dict(origen=getattr(self, '_origen', None), uv=con_uv, N=N.astype(np.float32)))
        v = np.zeros(nn, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
        v['x'], v['y'], v['z'], v['u'], v['v'], v['c'] = P[:, 0], P[:, 1], P[:, 2], uv[:, 0], uv[:, 1], argb
        self.verts.append(v); self.nv += nn

    def recorta_texturas(self):
        """deja solo las texturas que usan las mallas (y reordena el blob)"""
        self.ajusta_texturas()
        usadas = sorted({x[1] for x in self.mallas if x[1] >= 0}); nuevo = {}; blob = b''; texs = []
        for ti in usadas:
            w, h, fmt, li, off, tam = self.texs[ti]
            nuevo[ti] = len(texs); texs.append((w, h, fmt, li, len(blob), tam)); blob += self.blob[off:off + tam]
        for x in self.mallas: x[1] = nuevo.get(x[1], -1)
        self.rgba = [self.rgba[ti] for ti in usadas]
        self.texs, self.blob = texs, blob

    def comprime(self):
        """pasa a VQ las texturas que lo merecen (vq.elige) y rehace el blob; solo las que usa alguna malla. Con
        VQ_PNG=directorio guarda cada textura antes y despues para compararlas"""
        import vq
        usadas = {x[1] for x in self.mallas if x[1] >= 0}; blob = b''; texs = []; t0 = time.time(); antes = ahora = 0
        dpng = os.environ.get("VQ_PNG"); errs = []
        cielo = cielos(self.mallas)
        for ti, (w, h, fmt, li, off, tam) in enumerate(self.texs):
            d = self.blob[off:off + tam]
            if ti in usadas and ti in cielo and ti < len(self.rgba) and not fmt & 0x200:
                # el cielo: VQ (o 16 bits) con tramado, desde los 8 bits del original (vq.elige_cielo)
                antes += tam; r = vq.elige_cielo(self.rgba[ti], fmt & 0xff)
                if r is not None: d, n, err = r; fmt = fmt | 0x100 | (n - 1) << 16
                else: d = codifica16_tramada(self.rgba[ti], fmt & 0xff)
                ahora += len(d); d += b'\0' * (-len(d) % 32)
                texs.append((w, h, fmt, li, len(blob), len(d))); blob += d
                continue
            if ti in usadas:
                antes += tam
                r = None
                if ti < len(self.rgba):
                    if fmt & 0x200:                             # con mipmaps
                        r = vq.elige_mip(niveles_mip(self.rgba[ti]), fmt & 0xff)
                        if r is None and w * h >= 32 * 32:      # sin VQ costarian 2,7 veces mas: sin mipmaps
                            fmt &= ~0x200; d = codifica16(self.rgba[ti], fmt & 0xff)
                    if not fmt & 0x200: r = vq.elige(self.rgba[ti], fmt & 0xff)
                # segunda pasada desde los 16 bits guardados (vq.elige16; tambien las copias de ajusta_texturas, que
                # no tienen imagen): lo mismo que tools/revq.py sobre un fichero ya hecho
                if r is None and not fmt & 0x300 and self.vq2:
                    r = vq.elige16(d, w, h, fmt & 0xff, self.vq2)
                if r is not None:
                    d, n, err = r; fmt = fmt | 0x100 | (n - 1) << 16; errs.append(err)
                    if dpng and not fmt & 0x200 and ti < len(self.rgba):
                        vq.compara(self.rgba[ti], d, fmt, os.path.join(dpng, 'tex%03d_%dx%d.png' % (ti, w, h)))
                ahora += len(d)
            d += b'\0' * (-len(d) % 32)
            texs.append((w, h, fmt, li, len(blob), len(d))); blob += d
        self.texs, self.blob = texs, blob
        if errs: print('VQ: %d de %d texturas, %d KB -> %d KB, error RMS medio %.1f (max %.1f) en %.0f s'
                       % (len(errs), len(usadas), antes >> 10, ahora >> 10, np.mean(errs), max(errs), time.time() - t0))

    def escribe(self, sal):
        self.ajusta_texturas()
        if self.vq and not getattr(self, '_comprimido', False): self.comprime(); self._comprimido = True
        self.mallas.sort(key=lambda x: (x[0], x[1]))
        inicio = [sum(1 for x in self.mallas if x[0] < l) for l in range(5)]
        nsec = len(self.secciones) if self.mapa is not None else 0
        with open(sal, 'wb') as f:
            f.write(b'ORD7' + struct.pack('<13I', len(self.mallas), self.nv, len(self.ruta), self.ruta.shape[1],
                                          len(self.texs), *inicio, len(self.nodos), len(self.carteles), nsec))
            for t in self.texs: f.write(struct.pack('<6I', *t))
            for li, ti, cc, rad, i0, cnt, _, nodo in self.mallas:
                f.write(struct.pack('<4f2Iii', *cc, rad, i0, cnt, ti, nodo if isinstance(nodo, (int, np.integer)) else -1))
            for v in self.verts: f.write(v.tobytes())
            f.write(self.ruta.astype('<f4').tobytes())
            for nd in self.nodos: f.write(struct.pack('<4fIi2I', *nd[0], nd[1], nd[2], nd[3], nd[4] if len(nd) > 4 else 0, 0))
            for W in self.carteles:
                f.write(W.astype('<f4').tobytes())
            if nsec:
                ini = np.cumsum([0] + [len(L) for L in self.secciones]).astype('<u4')
                f.write(ini.tobytes())
                f.write(np.array([g for L in self.secciones for g in L], '<u2').tobytes())
                f.write(np.asarray(self.mapa, '<u2').tobytes())
                f.write(b'\0' * (-f.tell() % 4))
            f.write(b'\0' * (-f.tell() % 32)); f.write(self.blob)
        return inicio
