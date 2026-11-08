"""Capa unica de acceso a los assets del DVD de PS2 de OutRun 2006 Coast 2 Coast (tu ISO).

Uso rapido (desde tools/, o con tools/ en sys.path):
    from ps2.assets import Assets
    A = Assets(ruta_iso)                 # o Assets() con la variable de entorno OR2_ISO
    A.lista()                            # ficheros ISO 9660: [(nombre, desplazamiento, tamano)]
    A.fichero('STAGE.PS2')               # memoryview del fichero entero (sin copiar: mmap)
    A.paquete('SPRANI.PS2')              # [Entrada] en orden de desplazamiento (el indice que usan los conv_*.py)
    e = A.entrada('SPRANI.PS2', 'ani_SPRANI_ETC_CVT')   # por nombre (sin directorio ni extension vale), indice o hash
    A.datos(e)                           # bytes descomprimidos (zlib del paquete y zlib de los .sz)
    A.tipo(e)                            # ('sprani', {...}) segun la cabecera
    A.busca('coli_*')                    # entradas cuyo nombre encaja (glob, sin distinguir mayusculas)
    A.elf.lee(0x3526a4, 4), A.elf.u32(va, n), A.elf.busca(b'firma'), A.elf.cadenas()
    lectores: A.xst(e), A.xst_imagen(e, k), A.sprani(e), A.escena(e, k, f), A.bloque(e), A.coli(e), A.segmentos(),
              A.adpcm(e) (PS-ADPCM a PCM), A.banco_ics(), A.irx(nombre)

Paquetes *.PS2: u32 n (repetido 4 veces) y n entradas de 16 bytes {u32 hash, u32 desplazamiento, u32 tamano,
u32 tamano guardado}, ordenadas por hash. Si el tamano guardado es menor que el tamano, la entrada esta comprimida
con zlib por el propio paquete (GHOST, GHOSTS, SCRIPTS); los ficheros *.sz son zlib por si mismos. El hash es el de
la funcion 0x170048 del ELF: h = h * 131 + toupper(c) recorriendo de atras adelante la ruta relativa al directorio
del paquete con '\\' delante (p. ej. '\\IRX\\SOUNDCD.IRX' en SYSTEM.PS2, '\\PALM\\coli_CS_PALM_bin' en STAGE.PS2).
Equivale a h = suma(c_i * 131^i) mod 2^32, lo que permite componer prefijos y sufijos sin recalcular.

Los nombres no estan en el disco: se reconstruyen (resolver()) con las cadenas del ejecutable, las cadenas de
formato expandidas con sus vecinas, los directorios conocidos, las cadenas de los propios ficheros y variantes de
plantilla (numeros y codigos de zona) de los nombres ya hallados. El resultado se guarda en una cache local
(OR2_CACHE o ~/.cache/or2_assets): nunca en el repositorio."""
import fnmatch, json, os, re, struct, sys, time, zlib

from . import iso as _iso

M32 = 0xffffffff
_P = [1]
for _k in range(600): _P.append(_P[-1] * 131 & M32)


def hash_nombre(s):
    """hash de los paquetes (funcion 0x170048 del ELF)"""
    h = 0
    for c in reversed(s.upper().encode('latin1', 'replace')):
        h = (h * 131 + c) & M32
    return h


def _pot(n):
    while n >= len(_P): _P.append(_P[-1] * 131 & M32)
    return _P[n]


def _compone(h1, n1, h2):
    """hash de la concatenacion a + b conociendo hash(a), len(a) y hash(b)"""
    return (h1 + _pot(n1) * h2) & M32


def es_zlib(d):
    return len(d) >= 2 and d[0] == 0x78 and d[1] in (0x01, 0x5e, 0x9c, 0xda)


def descomprime(d, maximo=0):
    """inflado zlib si la cabecera lo es (si no, los mismos bytes); maximo > 0 limita la salida (para cabeceras)"""
    if not es_zlib(d): return bytes(d)
    o = zlib.decompressobj()
    try:
        return o.decompress(bytes(d), maximo) if maximo else o.decompress(bytes(d))
    except zlib.error:
        return bytes(d)


class Entrada:
    """una entrada de un paquete *.PS2"""
    __slots__ = ('paquete', 'indice', 'hash', 'off', 'tam', 'guardado', 'nombre', 'segmento')

    def __init__(self, paquete, indice, h, off, tam, guardado):
        self.paquete, self.indice, self.hash = paquete, indice, h
        self.off, self.tam, self.guardado = off, tam, guardado
        self.nombre = None; self.segmento = None

    @property
    def comprimida(self):
        """comprimida por el paquete (zlib, tamano guardado < tamano)"""
        return self.guardado < self.tam

    def __repr__(self):
        return '<%s #%d %08x %s %d bytes>' % (self.paquete, self.indice, self.hash, self.nombre or '?', self.tam)


class Elf:
    """ELF de MIPS (el ejecutable SLES_*, o un IRX del IOP): secciones, segmentos y lectura por direccion virtual"""

    def __init__(self, d):
        self.d = bytes(d)
        assert self.d[:4] == b'\x7fELF', 'no es un ELF'
        (self.tipo, self.maquina, _, self.entrada, phoff, shoff, _, _, phsz, phn, shsz, shn,
         shstr) = struct.unpack_from('<HHIIIIIHHHHHH', self.d, 16)
        self.segmentos = []                                   # (vaddr, desplazamiento, filesz, memsz)
        for i in range(phn):
            t, off, va, _, fs, ms = struct.unpack_from('<6I', self.d, phoff + i * phsz)
            if t == 1: self.segmentos.append((va, off, fs, ms))
        self.secciones = {}                                   # nombre -> (addr, desplazamiento, tamano, tipo)
        if shoff and shn:
            sec = [struct.unpack_from('<10I', self.d, shoff + i * shsz) for i in range(shn)]
            st = sec[shstr][4] if shstr < shn else 0
            for s in sec:
                nm = self.d[st + s[0]:self.d.index(b'\0', st + s[0])].decode('latin1') if st else ''
                if nm: self.secciones[nm] = (s[3], s[4], s[5], s[1])

    def off(self, va):
        """desplazamiento en el fichero de la direccion virtual va (None si no esta en un segmento cargado)"""
        for v, o, fs, ms in self.segmentos:
            if v <= va < v + fs: return va - v + o
        for a, o, n, t in self.secciones.values():           # IRX: sin segmentos con direcciones utiles
            if t != 8 and a <= va < a + n: return va - a + o
        return None

    def va(self, off):
        for v, o, fs, ms in self.segmentos:
            if o <= off < o + fs: return off - o + v
        return None

    def lee(self, va, n):
        o = self.off(va)
        if o is None: raise ValueError('direccion 0x%x fuera del ELF' % va)
        return self.d[o:o + n]

    def u32(self, va, n=1):
        r = struct.unpack_from('<%dI' % n, self.d, self.off(va)); return r if n > 1 else r[0]

    def f32(self, va, n=1):
        r = struct.unpack_from('<%df' % n, self.d, self.off(va)); return r if n > 1 else r[0]

    def cadena(self, va):
        o = self.off(va); return self.d[o:self.d.index(b'\0', o)].decode('latin1')

    def busca(self, firma, alin=1, seccion=None):
        """direcciones virtuales (o desplazamientos si no hay segmento) donde aparece firma (bytes o regex
        compilada de bytes)"""
        a, b = 0, len(self.d)
        if seccion: _, a, n, _ = self.secciones[seccion]; b = a + n
        pat = firma if hasattr(firma, 'finditer') else re.compile(re.escape(firma))
        r = []
        for m in pat.finditer(self.d, a, b):
            if m.start() % alin: continue
            v = self.va(m.start()); r.append(v if v is not None else m.start())
        return r

    def referencias(self, va):
        """palabras de 32 bits alineadas del ELF que valen va (punteros en .data)"""
        return self.busca(struct.pack('<I', va), 4)

    def cadenas(self, minimo=3):
        """[(va, texto)] de las cadenas imprimibles terminadas en cero"""
        r = []
        for m in re.finditer(rb'[\x20-\x7e]{%d,}(?=\0)' % minimo, self.d):
            v = self.va(m.start()); r.append((v if v is not None else m.start(), m.group().decode('latin1')))
        return r

    def modulo(self):
        """nombre del modulo de un IRX (seccion .iopmod: +0x1a)"""
        s = self.secciones.get('.iopmod')
        if not s: return None
        o = s[1] + 0x1a; return self.d[o:self.d.index(b'\0', o)].decode('latin1')


# ---------------------------------------------------------------- deteccion de tipos
_NOM_TIPO = [('scn_env_fog', 'niebla'), ('scn_env_sun', 'sol'), ('maya_spl', 'spline'), ('\\cs_cs_', 'tramo_datos'),
             ('\\cs_env_', 'entorno_datos'), ('\\cs_bk_', 'fondo_datos'), ('\\bin\\', 'script'),
             ('object_db', 'objetos_db'), ('bone.bin', 'huesos'), ('lens_flare', 'destello')]
_SUF_TIPO = {'.pss': 'pss', '.irx': 'irx', '.rec': 'fantasma', '.txt': 'texto', '.ico': 'icono'}


def _bloque_pmt(d):
    """cabecera de bloque de geometria (STAGE/CARS/OBJ...): n registros, n texturas, tamano A, tamano B"""
    if len(d) < 16: return None
    w = struct.unpack_from('<4I', d, 0)
    if 1 <= w[0] <= 0x400 and w[1] <= 0x1000 and w[2] + w[3] + 16 == len(d) and w[2] >= 72 * w[0]:
        return w
    return None


def detecta(d, nombre=None, completo=True, paquete=None):
    """(tipo, info) de unos datos ya descomprimidos. completo=False: solo la cabecera (d puede estar cortado).
    Tipos: irx, elf, coli, so, pss, texto_idioma, records, snr2, fantasma, banco_ics, banco_se, musica, modelo,
    xst, sprani, movimiento, oso, as, personaje_chr, icono, texto; por nombre (_NOM_TIPO): niebla, sol, spline,
    tramo_datos, entorno_datos, fondo_datos, script, objetos_db, huesos, destello; y binario, ceros, desconocido"""
    n = (nombre or '').lower(); info = {}; L = len(d)
    if d[:4] == b'\x7fELF':
        t = struct.unpack_from('<H', d, 16)[0] if L > 18 else 0
        if t == 0xff80:
            if completo:
                try: info['modulo'] = Elf(d).modulo()
                except Exception: pass
            return 'irx', info
        return 'elf', info
    if d[:4] == b'COLI':
        info['version'] = d[4:8].decode('latin1')
        info['poligonos'], info['calzada'] = struct.unpack_from('<II', d, 8); return 'coli', info
    if d[:2] == b'SO' and L >= 4:
        k = struct.unpack_from('<h', d, 2)[0]
        if not completo or 0 < 4 + 6 * k <= L: info['n'] = k; info['version'] = d[3]; return 'so', info
    if d[:4] == b'\x00\x00\x01\xba': return 'pss', info
    if d[:4] == b'txet':
        info['tam'] = struct.unpack_from('<I', d, 4)[0]; return 'texto_idioma', info
    if d[:4] == b'SEGA': return 'records', info
    if d[:4] == b'SNR2': return 'snr2', info
    if d[:4] in (b'QHOT', b'NHOT'): return 'fantasma', info
    if paquete == 'MUSIC.PS2' or (n.endswith('.ps2') and d[:2] == b'\x00\x02'):
        info['canales'] = 2; info['intercalado'] = 0x2000; return 'musica', info
    if L >= 16:
        w = struct.unpack_from('<4I', d, 0)
        if w[1] == 1 and w[2] == 0 and w[3] == 0 and (n.endswith('.pak') or paquete == 'SOUND.PS2'):
            info['adpcm'] = w[0]; return 'banco_ics', info
        if (n.endswith('.pak') or paquete == 'SOUND.PS2') and w[1] == 0 and w[3] == 0 and w[0] + w[2] + 16 <= L + 16:
            info['adpcm'] = w[0]; info['adpcm2'] = w[2]; return 'banco_se', info
    if completo and _bloque_pmt(d):
        w = _bloque_pmt(d); info.update(registros=w[0], texturas=w[1], A=w[2], B=w[3]); return 'modelo', info
    if L >= 0x20 and struct.unpack_from('<I', d, 4)[0] == 0 and ('xst' in n or paquete == 'SPRITE.PS2'):
        info['texturas'], = struct.unpack_from('<I', d, 12); return 'xst', info
    if 'sprani' in n or paquete == 'SPRANI.PS2':
        return 'sprani', info
    if '_pmt' in n:
        w = struct.unpack_from('<4I', d, 0) if L >= 16 else (0, 0, 0, 0)
        info.update(registros=w[0], texturas=w[1]); return 'modelo', info
    if n.startswith('\\mot_') or paquete == 'ANIMS.PS2' or 'motdata' in n: return 'movimiento', info
    if 'oso_' in n: return 'oso', info
    if n.startswith('\\as_') or paquete == 'AS.PS2': return 'as', info
    if n.startswith('\\chr_') or paquete == 'CHR.PS2': return 'personaje_chr', info
    for s, t in _SUF_TIPO.items():
        if n.endswith(s): return t, info
    for c, t in _NOM_TIPO:
        if c in n: return t, info
    if '\\bin\\' in n or n.endswith('.bin') or n.endswith('_bin'): return 'binario', info
    if not any(d[:64]): return 'ceros', info
    return 'desconocido', info


# ---------------------------------------------------------------- resolucion de nombres
_LIMPIO = re.compile(r'^[A-Za-z0-9][A-Za-z0-9_\-]*(\.[A-Za-z0-9]{1,4})?$')
_SUFIJOS = ['', '.sz', '.bin', '_bin', '_pmt', '_bin.sz', '_pmt.sz', '.pak', '.irx', '.rec', '.txt', '.ps2',
            '.pss', '.ico']


def _limpia(s):
    s = s.replace('/', '\\')
    for p in ('cdrom0:', 'host0:', 'd:', 'z:'):
        if s.lower().startswith(p.lower()): s = s[len(p):]
    if s.endswith(';1'): s = s[:-2]
    return s


def _colas(s):
    """las rutas relativas posibles de una cadena: cada cola tras una barra, con la barra delante"""
    partes = [p for p in s.split('\\') if p]
    return ['\\' + '\\'.join(partes[k:]) for k in range(len(partes))]


def _plausible(nm):
    partes = [p for p in nm.split('\\') if p]
    return bool(partes) and all(_LIMPIO.match(p) for p in partes) and all(len(p) >= 2 for p in partes[:-1])


def _clave_orden(n):
    """orden en que el empaquetador guarda los ficheros: recorrido del arbol, en cada directorio primero los
    ficheros y despues los subdirectorios, cada grupo en orden alfabetico en mayusculas"""
    c = [x.upper() for x in n.split('\\') if x]
    return [(1, x) for x in c[:-1]] + [(0, c[-1])]


class _Resolutor:
    """busca nombres para un conjunto de hashes (de todos los paquetes)"""

    def __init__(self, objetivos):
        self.obj = objetivos                 # hash -> [paquete]
        self.hall = {}                       # (paquete, hash) -> (rango, nombre)
        self.pruebas = 0

    def prueba(self, h, nombre, rango):
        self.pruebas += 1
        if h not in self.obj or not _plausible(nombre): return
        for p in self.obj[h]:
            v = self.hall.get((p, h))
            if v is None or rango < v[0]: self.hall[(p, h)] = (rango, nombre)

    def con_sufijos(self, base, rango, sufijos=_SUFIJOS):
        hb = hash_nombre(base); L = len(base)
        for s in sufijos:
            h = _compone(hb, L, hash_nombre(s)) if s else hb
            if h in self.obj: self.prueba(h, base + s, rango)

    def producto(self, pre, toks, suf, rango):
        """'pre' + t + 'suf' para cada t de toks [(texto, hash, len)]"""
        hp, lp, hs = hash_nombre(pre), len(pre), hash_nombre(suf)
        hpt = _pot(lp); obj = self.obj
        for t, ht, lt in toks:
            h = (hp + hpt * ((ht + _pot(lt) * hs) & M32)) & M32
            if h in obj: self.prueba(h, pre + t + suf, rango)


def _expande(fmt, vecinas):
    """opciones de cada trozo de una cadena de formato: %02d 0..99, %03d 0..999, %d 0..9, %s las cadenas vecinas
    (None si es la unica %s: el llamador la recorre con todos los tokens)"""
    trozos = re.split(r'(%0?\d*d|%s)', fmt)
    nsl = trozos[1::2].count('%s')
    opciones = []
    for i, t in enumerate(trozos):
        if i % 2 == 0: opciones.append([t]); continue
        if t == '%s': opciones.append(None if nsl == 1 else [''] + vecinas)
        else:
            w = int(t[1:-1]) if t[1:-1] else 1
            opciones.append(['%0*d' % (w, k) for k in range(10 ** min(w, 3))])
    return opciones


class Assets:
    def __init__(self, ruta_iso=None, cache=None, nombres=True):
        ruta_iso = ruta_iso or os.environ.get('OR2_ISO')
        if not ruta_iso: raise SystemExit('falta la ISO (argumento o variable OR2_ISO)')
        self.ruta = os.path.abspath(ruta_iso)
        self.iso = _iso.Iso(self.ruta); self.m = self.iso.m
        self.cache = cache or os.environ.get('OR2_CACHE') or os.path.expanduser('~/.cache/or2_assets')
        st = os.stat(self.ruta)
        self.id = '%d_%d' % (st.st_size, int(st.st_mtime))
        self._paq = {}; self._elf = None; self._nom = None; self._quiere_nombres = nombres
        self._datos = {}                                     # cache LRU pequena de datos descomprimidos

    # ------------------------------------------------ disco
    def lista(self):
        return sorted(((n, o, s) for n, (o, s) in self.iso.fich.items()), key=lambda x: x[1])

    def fichero(self, nombre):
        o, s = self.iso.fich[nombre]; return memoryview(self.m)[o:o + s]

    def paquetes(self):
        return [n for n, o, s in self.lista() if n.endswith('.PS2')]

    def paquete(self, nombre):
        if nombre not in self._paq:
            base, tam = self.iso.fich[nombre]
            n = struct.unpack_from('<I', self.m, base)[0]
            e = [struct.unpack_from('<4I', self.m, base + 16 + 16 * i) for i in range(n)]
            e.sort(key=lambda x: x[1])
            self._paq[nombre] = [Entrada(nombre, k, h, base + o, s, g) for k, (h, o, s, g) in enumerate(e)]
            if self._nom is not None: self._pon_nombres(nombre)
            if nombre == 'STAGE.PS2': self._pon_segmentos()
        if self._quiere_nombres and self._nom is None:
            self.resolver()
        return self._paq[nombre]

    def entradas(self):
        """todas las entradas de todos los paquetes"""
        r = []
        for p in self.paquetes(): r += self.paquete(p)
        return r

    def entrada(self, paquete, clave):
        """por indice (int), hash (int con hash=True no hace falta: se prueba si no es indice valido), nombre exacto,
        nombre sin directorio, o nombre sin extension"""
        E = self.paquete(paquete)
        if isinstance(clave, int):
            if 0 <= clave < len(E): return E[clave]
            for e in E:
                if e.hash == clave: return e
            raise KeyError('%s: no hay entrada %r' % (paquete, clave))
        c = clave.lower()
        if not c.startswith('\\'): c2 = '\\' + c
        else: c2 = c
        h = hash_nombre(c2)
        for e in E:
            if e.hash == h: return e
        for e in E:
            if not e.nombre: continue
            n = e.nombre.lower(); b = n.split('\\')[-1]
            if c in (n, b, b.split('.')[0], n.split('.')[0]) or b.startswith(c + '.') or b.startswith(c + '_bin') \
                    or b.startswith(c + '_pmt'):
                return e
        raise KeyError('%s: no hay entrada %r' % (paquete, clave))

    def busca(self, patron, paquete=None):
        """entradas cuyo nombre (o paquete:indice) encaja con el glob o la regex (re:...) dada"""
        r = []
        rx = re.compile(patron[3:], re.I) if patron.startswith('re:') else None
        for p in ([paquete] if paquete else self.paquetes()):
            for e in self.paquete(p):
                n = e.nombre or ''
                if rx is not None:
                    if rx.search(n): r.append(e)
                elif fnmatch.fnmatch(n.lower(), patron.lower()) or fnmatch.fnmatch(n.split('\\')[-1].lower(), patron.lower()):
                    r.append(e)
        return r

    def crudo(self, e, maximo=None):
        """bytes tal como estan en el disco"""
        n = e.guardado if maximo is None else min(e.guardado, maximo)
        return self.m[e.off:e.off + n]

    def datos(self, e, maximo=0):
        """bytes descomprimidos (zlib del paquete y zlib de los .sz); maximo > 0: solo el principio"""
        if not maximo and (e.paquete, e.indice) in self._datos: return self._datos[(e.paquete, e.indice)]
        if maximo:
            d = self.crudo(e, max(4096, maximo * 2) if e.comprimida or es_zlib(self.crudo(e, 2)) else maximo)
            d = descomprime(d, maximo)
            if es_zlib(d): d = descomprime(d, maximo)
            return d[:maximo]
        d = descomprime(self.crudo(e))
        if es_zlib(d): d = descomprime(d)
        if len(self._datos) > 8: self._datos.pop(next(iter(self._datos)))
        self._datos[(e.paquete, e.indice)] = d
        return d

    descomprime = staticmethod(descomprime)

    def tipo(self, e, completo=False):
        """(tipo, info); completo=True descomprime entero (mas lento, mas datos)"""
        d = self.datos(e) if completo else self.datos(e, 256)
        t, info = detecta(d, e.nombre, completo, e.paquete)
        if t in ('desconocido', 'binario', 'so') and not completo and e.tam < (1 << 20):
            t, info = detecta(self.datos(e), e.nombre, True, e.paquete)
        info['zlib'] = bool(e.comprimida or es_zlib(self.crudo(e, 2)))
        return t, info

    # ------------------------------------------------ ejecutable
    @property
    def elf(self):
        if self._elf is None:
            nombre = next(k for k in self.iso.fich if k[:4] in ('SLES', 'SLUS', 'SLPM', 'SCES', 'SCUS'))
            self._elf = Elf(self.fichero(nombre)); self._elf.nombre = nombre
        return self._elf

    def irx(self, nombre):
        """ELF de un modulo del IOP de SYSTEM.PS2 (nombre del fichero o del modulo)"""
        try: return Elf(self.datos(self.entrada('SYSTEM.PS2', nombre)))
        except KeyError: pass
        for e in self.paquete('SYSTEM.PS2'):
            d = self.datos(e)
            if d[:4] == b'\x7fELF':
                x = Elf(d)
                if (x.modulo() or '').lower() == nombre.lower(): return x
        raise KeyError(nombre)

    # ------------------------------------------------ nombres
    def _ruta_cache(self):
        return os.path.join(self.cache, 'nombres_%s.json' % self.id)

    def _pon_nombres(self, p):
        for e in self._paq[p]:
            e.nombre = self._nom.get('%s:%08x' % (p, e.hash))

    def _pon_segmentos(self):
        seg = -1
        for e in self._paq['STAGE.PS2']:
            if self.m[e.off:e.off + 4] == b'COLI' or seg < 0: seg += 1
            e.segmento = seg

    def resolver(self, forzar=False, info=False):
        """nombres de todas las entradas: de la cache, o reconstruidos (ver la cabecera del modulo)"""
        ruta = self._ruta_cache()
        if not forzar and self._nom is None and os.path.exists(ruta):
            try:
                self._nom = json.load(open(ruta))['nombres']
            except Exception:
                self._nom = None
        if self._nom is None or forzar:
            t0 = time.time(); q = self._quiere_nombres; self._quiere_nombres = False; self._nom = None
            try: self._nom = self._reconstruye(info)
            finally: self._quiere_nombres = q
            try:
                os.makedirs(self.cache, exist_ok=True)
                json.dump({'iso': self.ruta, 'segundos': round(time.time() - t0, 1), 'nombres': self._nom},
                          open(ruta, 'w'))
            except OSError:
                pass
        for p in self._paq: self._pon_nombres(p)
        return self._nom

    def _reconstruye(self, info=False):
        obj = {}
        for p in self.paquetes():
            for e in self.paquete(p): obj.setdefault(e.hash, []).append(p)
        R = _Resolutor(obj); t0 = time.time()
        cad = self.elf.cadenas(2)
        textos = [(_limpia(s), v) for v, s in cad]
        # 1. cadenas del ejecutable (cada cola de la ruta, con y sin extension)
        dirs = set()
        for s, v in textos:
            if '%' in s: continue
            for c in _colas(s):
                R.con_sufijos(c, 0)
            ps = [x for x in s.split('\\') if x]
            for x in (ps if len(ps) > 1 and '.' not in ps[-1] else ps[:-1]):
                if _LIMPIO.match(x): dirs.add(x)
        # cadenas sueltas de los modulos del IOP (nombres de IRX) y de los ficheros pequenos de los paquetes
        tokens = set()
        for p in self.paquetes():
            if p in ('MUSIC.PS2', 'MV.PS2', 'STAGE.PS2', 'SOUND.PS2', 'SPRITE.PS2'): continue
            for e in self.paquete(p):
                if e.tam > (1 << 21): continue
                try: d = self.datos(e)
                except Exception: continue
                if d[:4] == b'\x7fELF':
                    try:
                        mo = Elf(d).modulo()
                        if mo: tokens.add(mo); tokens.add(mo.upper() + '.IRX')
                    except Exception: pass
                for m in re.finditer(rb'[A-Za-z0-9_\\/.\-]{4,64}', d):
                    tokens.add(m.group().decode('latin1'))
        for s, v in textos:
            if '%' not in s and _LIMPIO.match(s.split('\\')[-1] or 'x'): tokens.add(s)
        limpios = set()
        for t in tokens:
            t = _limpia(t)
            for c in _colas(t):
                if _plausible(c):
                    limpios.add(c[1:])
                    if '.' in c: limpios.add(c[1:].rsplit('.', 1)[0])
        lim = [(t, hash_nombre(t), len(t)) for t in limpios]
        for t, ht, lt in lim:
            for s in _SUFIJOS:
                h = _compone(_compone(92, 1, ht), lt + 1, hash_nombre(s)) if s else _compone(92, 1, ht)
                if h in obj: R.prueba(h, '\\' + t + s, 3)
        if info: print('  cadenas: %d nombres, %.1f s' % (len(R.hall), time.time() - t0))
        # 2. cadenas de formato: %d/%02d/%03d numeros; %s con las cadenas vecinas (o con todos los tokens)
        vec_dirs = set(); vocab = set()
        for s, v in textos:
            if '%' not in s or len(s) > 80: continue
            o = self.elf.off(v) if v is not None else None
            if o is None: o = self.elf.d.find(s.encode('latin1'))
            zona = self.elf.d[max(0, o - 0x100):o + 0x100]
            vec = [_limpia(x.decode('latin1')) for x in re.findall(rb'(?<=\0)[\x21-\x7e]{1,24}(?=\0)', zona)]
            vec = [x for x in vec if '%' not in x]
            vec = [x for x in vec if re.match(r'^[A-Za-z0-9_.\\]*$', x)]
            f = re.sub(r'^%s\\?', '\\\\', s)
            if '%s\\' in f:
                for x in vec:
                    if re.match(r'^[A-Za-z0-9_]{2,16}$', x): vec_dirs.add(x)
            if f.count('%s') >= 2:
                for x in vec:
                    if re.match(r'^[A-Za-z0-9_]{1,8}$', x): vocab.add(x)
            ops = _expande(f, vec)
            n = 1
            for o in ops: n *= len(o) if o is not None else 1
            if n > 300000: continue
            # producto cartesiano; la unica %s (None) se recorre con todos los tokens limpios
            import itertools
            hueco = [i for i, o in enumerate(ops) if o is None]
            for combo in itertools.product(*[o if o is not None else [None] for o in ops]):
                if hueco:
                    i = hueco[0]; pre = ''.join(combo[:i]); suf = ''.join(combo[i + 1:])
                    for c in _colas(pre + '\x00' + suf):
                        if '\x00' not in c: continue
                        a, b = c.split('\x00')
                        R.producto(a, lim, b, 1)
                else:
                    for c in _colas(''.join(combo)): R.con_sufijos(c, 1, ('',))
        dirs |= vec_dirs
        if info: print('  formatos: %d nombres, %.1f s' % (len(R.hall), time.time() - t0))
        # 3. directorios conocidos delante de los nombres con extension o ruta
        conext = [(t, h, n) for t, h, n in lim if '.' in t or '\\' in t or t.endswith('_bin') or t.endswith('_pmt')]
        for D in sorted(dirs):
            R.producto('\\' + D + '\\', conext, '', 2)
        if info: print('  directorios (%d): %d nombres, %.1f s' % (len(dirs), len(R.hall), time.time() - t0))
        # 4. plantillas: cambiar numeros y codigos de zona de los nombres hallados
        for ronda in range(3):
            antes = len(R.hall)
            hall = [n for _, n in R.hall.values()]
            zonas = set()
            for n in hall:
                for z in re.findall(r'(?<![A-Za-z])([A-Za-z]{4})(?:_[A-Za-z]{1,2})?(?![A-Za-z])', n):
                    zonas.add(z.upper())
            zonas = sorted(zonas)
            vistos = set()
            for n in hall:
                for m in re.finditer(r'\d+', n):
                    w = len(m.group())
                    if w > 3: continue
                    pl = (n[:m.start()], n[m.end():])
                    if pl in vistos: continue
                    vistos.add(pl)
                    R.producto(pl[0], [('%0*d' % (w, k),) * 1 + (hash_nombre('%0*d' % (w, k)), w)
                                       for k in range(10 ** w)], pl[1], 4)
                for m in re.finditer(r'(?<![A-Za-z])[A-Za-z]{4}(?![A-Za-z])', n):
                    pl = (n[:m.start()], n[m.end():])
                    if pl in vistos: continue
                    vistos.add(pl)
                    zt = [(z, hash_nombre(z), 4) for z in zonas]
                    R.producto(pl[0], zt, pl[1], 4)
                    for v in ('_R', '_T', '_BR', '_BT', 'L', 'R', 'LR'):
                        R.producto(pl[0], [(z + v, hash_nombre(z + v), 4 + len(v)) for z in zonas], pl[1], 4)
            # recombinacion: cada palabra (entre separadores) de un nombre se cambia por las palabras de los
            # nombres del mismo paquete, por las de las cadenas de formato con varias %s y por pares de estas;
            # y se prueban esas palabras delante de la extension (el ultimo %s de los fantasmas)
            cuenta = {}
            for (p, h) in R.hall: cuenta[p] = cuenta.get(p, 0) + 1
            falta = {p for p in self.paquetes() if cuenta.get(p, 0) < len(self.paquete(p))}
            voc = sorted(v for v in vocab if re.match(r'^[a-z0-9_]{1,5}$', v))
            voc2 = sorted(set([a + b for a in voc for b in voc] + voc))
            palabras = {}
            for (p, h), (r, n) in R.hall.items():
                if p in falta:
                    palabras.setdefault(p, set()).update(re.findall(r'(?<=[_\\])[A-Za-z0-9]+(?=[_.]|$)', n))
            for (p, h), (r, n) in list(R.hall.items()):
                if p not in falta: continue
                vt = [(v, hash_nombre(v), len(v)) for v in sorted(palabras[p]) + voc2]
                for m in re.finditer(r'(?<=[_\\])[A-Za-z0-9]+(?=[_.]|$)', n):
                    pl = (n[:m.start()], n[m.end():])
                    if pl in vistos: continue
                    vistos.add(pl); R.producto(pl[0], vt, pl[1], 5)
                k = n.rfind('.')
                if k > 0 and (n[:k], 'ext', n[k:]) not in vistos:
                    vistos.add((n[:k], 'ext', n[k:]))
                    R.producto(n[:k], [(v, hash_nombre(v), len(v)) for v in voc2], n[k:], 5)
            if len(R.hall) == antes: break
        self._valida_orden(R)
        if info: print('  plantillas: %d nombres, %.1f s, %d pruebas' % (len(R.hall), time.time() - t0, R.pruebas))
        return {'%s:%08x' % (p, h): n for (p, h), (r, n) in R.hall.items()}

    def _valida_orden(self, R):
        """en los paquetes cuyos nombres van casi todos en orden de arbol (_clave_orden) por desplazamiento, se
        descartan los que quedan fuera de la subsecuencia creciente mas larga (falsos positivos del hash)"""
        import bisect
        for p in self.paquetes():
            E = [e for e in self._paq[p] if (p, e.hash) in R.hall]
            if len(E) < 4: continue
            K = [_clave_orden(R.hall[(p, e.hash)][1]) for e in E]
            if sum(1 for a, b in zip(K, K[1:]) if a < b) < 0.9 * (len(K) - 1): continue
            colas, idx, padre = [], [], [-1] * len(K)          # subsecuencia creciente mas larga
            for i, k in enumerate(K):
                j = bisect.bisect_left(colas, k)
                if j: padre[i] = idx[j - 1]
                if j == len(colas): colas.append(k); idx.append(i)
                else: colas[j] = k; idx[j] = i
            buenos = set(); i = idx[-1]
            while i >= 0: buenos.add(i); i = padre[i]
            for i, e in enumerate(E):
                if i not in buenos: del R.hall[(p, e.hash)]

    # ------------------------------------------------ lectores de alto nivel (reutilizan los modulos existentes)
    @staticmethod
    def _tools():
        d = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if d not in sys.path: sys.path.insert(0, d)

    def xst(self, e):
        """texturas RGBA (alfa 0-255) de un XST (conv_hud.texturas)"""
        self._tools(); import conv_hud
        return conv_hud.texturas(self.datos(e))

    def xst_cabeceras(self, e):
        """[(ancho, alto, psm, comprimida_lz)] de las texturas de un XST sin decodificarlas"""
        d = self.datos(e); H = d[4:]; meta, nt = struct.unpack_from('<II', H, 4); r = []
        for k in range(nt):
            o = struct.unpack_from('<I', H, meta + 4 * k)[0]; h = struct.unpack_from('<14I', H, meta + o)
            r.append((1 << (h[11] >> 24), 1 << (h[12] & 0xff), (h[11] >> 16) & 0xff, bool(h[11] & 0x8000)))
        return r

    def xst_imagen(self, e, k):
        self._tools(); import conv_hud
        return conv_hud.imagen_xst(self.datos(e), k)

    def sprani(self, e):
        """animacion SPRANI (conv_hud.Ani); sus texturas estan en el XST spr_sprani_<clave>_[E]xst.sz"""
        self._tools(); import conv_hud
        return conv_hud.Ani(self.datos(e))

    def sprani_xst(self, e):
        """el XST (ingles si hay variantes) de una animacion SPRANI"""
        clave = e.nombre.split('\\')[-1][len('ani_SPRANI_'):].split('.')[0].lower()
        for c in ('spr_sprani_%s_exst' % clave, 'spr_sprani_%s_xst' % clave):
            try: return self.entrada('SPRITE.PS2', c)
            except KeyError: pass
        raise KeyError('sin XST para ' + e.nombre)

    def escena(self, e, k, f=0.0):
        self._tools(); import conv_hud
        return conv_hud.escena(self.sprani(e), k, f)

    def bloque(self, e):
        """bloque de geometria (stage.Bloque) de una entrada de modelo (_pmt), comprimida o no"""
        from . import stage
        d = self.datos(e); w = struct.unpack_from('<4I', d, 0)

        class _Mem:
            pass
        mm = _Mem(); mm.m = d
        return stage.Bloque(mm, 0, w)

    def texturas_bloque(self, e):
        """[(cabecera, RGBA)] de las texturas de un bloque de modelo (tex.cabeceras / tex.imagen)"""
        from . import tex
        b = self.bloque(e); r = []
        for h in tex.cabeceras(b):
            try: r.append((h, tex.imagen(b, h)))
            except Exception: r.append((h, None))
        return r

    def coli(self, e):
        """(vertices (n, 4, 3), material (n,)) de una colision COLI0200 (como stage.coli)"""
        import numpy as np
        d = self.datos(e)
        n = struct.unpack_from('<I', d, 8)[0]; sec = struct.unpack_from('<8I', d, 20)
        mat = np.frombuffer(d[sec[1]:sec[1] + n], np.uint8) & 31
        P = np.frombuffer(d[sec[2]:sec[2] + 64 * n], np.uint8).reshape(n, 64)
        return P[:, :48].copy().view('<f4').reshape(n, 4, 3).astype(float), mat.astype(int)

    def segmentos(self):
        """los segmentos de carretera de STAGE.PS2 (stage.segmentos)"""
        from . import stage
        return stage.segmentos(self.iso)

    def adpcm(self, datos, desde=0, hasta=None):
        """PS-ADPCM (bloques de 16 bytes) a enteros de 16 bits (conv_sonido.dec_psx)"""
        self._tools(); import conv_sonido
        return conv_sonido.dec_psx(datos[desde:hasta])

    def banco_ics(self):
        """[(desplazamiento, bytes, pitch)] de las muestras de \\ICS.PAK con la tabla del IRX de sonido"""
        self._tools(); import conv_sonido
        ics = self.datos(self.entrada('SOUND.PS2', '\\ICS.PAK')); irx = self.datos(self.entrada('SYSTEM.PS2', '\\IRX\\SOUNDCD.IRX'))
        tb, td, _ = conv_sonido.elf_seccion_irx(irx)
        t = [struct.unpack_from('<IH', irx, td + conv_sonido.IRX_TABLA - tb + 8 * k) for k in range(68)]
        d = ics[16:16 + struct.unpack_from('<I', ics, 0)[0]]; r = []
        for o, p in t:
            i = o
            while i + 16 < len(d) and not d[i + 1] & 1: i += 16
            r.append((o, i + 16 - o, p))
        return d, r
