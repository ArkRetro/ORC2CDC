"""libros.py fichero.DAT|patron... | --datos datos/ trabajo/ : libros VQ reducidos (Rev 2.2, plan de VRAM 2026-11-22).

Las texturas VQ pequenas llevan un libro de 256 entradas (2 KB) aunque tengan pocos bloques: en una de 32x32 el libro
es 8 veces los indices. La PowerVR acepta libros de N < 256 entradas (las ultimas: la textura se direcciona 2048 - 8 N
bytes antes, demo/main.c), asi que estas se vuelven a codificar con libros de 64 (hasta 32x32) o 128 entradas (hasta
128x64) con pvrtex (utils/pvrtex de KallistiOS, ELBG de FFmpeg: con el mismo libro da menos error que el k-means de
tools/vq.py, comparado con 28 texturas de 6 etapas: 5,2 contra 5,9 de RMS con 128 entradas).

Solo las texturas usadas por alguna malla, en VQ con libro de 256 y sin mipmaps. La fuente es la imagen decodificada
de la VQ de ahora (los texeles de 16 bits ya no estan en el fichero); se acepta el libro pequeno si su error RMS contra
ella (RGBA) es <= UMBRAL y el alfa de recorte (ARGB1555) cambia en <= 1 % de los texeles; primero 64 si se puede,
si no 128, si no se deja. El resto del fichero no cambia (como tools/revq.py).

pvrtex: OR2_PVRTEX, OR2_NATIVO/pvrtex(.exe) (el programa de Windows: windows/construye.sh lo compila con mingw-w64) o
$KOS_BASE/utils/pvrtex/pvrtex. Sin pvrtex no cambia nada (lo dice). Determinista: los mismos bytes en Linux y Windows
(comprobado con las mismas entradas).
--datos: en una carpeta de datos ya hecha, recomponiendo y reempaquetando los SEG / BK de los paquetes
(grupos_nodos.datos)."""
import glob, os, struct, subprocess, sys, tempfile
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vq, revq

UMBRAL = float(os.environ.get('LIBROS_UMBRAL', '8'))
FORMATO = {0: 'ARGB1555', 1: 'RGB565', 2: 'ARGB4444'}

def pvrtex():
    for p in (os.environ.get('OR2_PVRTEX'),
              os.environ.get('OR2_NATIVO') and os.path.join(os.environ['OR2_NATIVO'], 'pvrtex.exe' if sys.platform == 'win32' else 'pvrtex'),
              os.path.join(os.environ.get('KOS_BASE', '/opt/toolchains/dc/kos'), 'utils', 'pvrtex', 'pvrtex')):
        if p and os.path.isfile(p): return p
    return None

def entradas(w, h):
    """libros a probar para una textura de w x h (sin mipmaps)"""
    if w * h <= 32 * 32: return (64, 128)
    if w * h <= 128 * 64: return (128,)
    return ()

def recodifica(a, fmt, n, exe, tmp):
    """imagen RGBA (h, w, 4) -> datos VQ (libro de n + indices) con pvrtex, o None"""
    from PIL import Image
    h, w = a.shape[:2]
    ent, sal = os.path.join(tmp, 'e.png'), os.path.join(tmp, 's.dt')
    Image.fromarray(a, 'RGBA').save(ent)
    if os.path.exists(sal): os.remove(sal)
    r = subprocess.run([exe, '-i', ent, '-o', sal, '-f', FORMATO[fmt], '-c', str(n)], stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
    if r.returncode or not os.path.isfile(sal): return None
    d = open(sal, 'rb').read()
    if d[:4] != b'DcTx' or len(d) != 32 + 8 * n + w * h // 4: return None
    return d[32:]

def error(a, b, fmt):
    e = np.sqrt(((a.astype(np.float64) - b) ** 2).mean())
    alfa = (a[..., 3] >= 128) != (b[..., 3] >= 128)
    return e, (alfa.mean() if fmt == 0 else 0.0)

def libros(b, exe, umbral=UMBRAL):
    """bytes de un ORD7 -> (bytes nuevos o None, texturas cambiadas, bytes ahorrados)"""
    if b[:4] != b'ORD7': raise ValueError('no es un ORD7')
    h = struct.unpack_from('<13I', b, 4); nmallas, ntex = h[0], h[4]
    texs = [list(struct.unpack_from('<6I', b, 56 + 24 * i)) for i in range(ntex)]
    p_mal = 56 + 24 * ntex
    usadas = {struct.unpack_from('<i', b, p_mal + 32 * i + 24)[0] for i in range(nmallas)}
    blob0 = revq.bloque_texeles(b); blob = b''; n = 0
    with tempfile.TemporaryDirectory() as tmp:
        for ti, (w, hh, fmt, li, off, tam) in enumerate(texs):
            d = b[blob0 + off:blob0 + off + tam]
            if ti in usadas and fmt & 0x300 == 0x100 and (fmt >> 16) & 255 == 255 and fmt & 0xff in FORMATO:
                a = np.asarray(vq.decodifica(d, w, hh, fmt & 0xff, 256), dtype=np.uint8)
                for k in entradas(w, hh):
                    nd = recodifica(np.ascontiguousarray(a), fmt & 0xff, k, exe, tmp)
                    if nd is None: continue
                    e, al = error(a, np.asarray(vq.decodifica(nd, w, hh, fmt & 0xff, k)), fmt & 0xff)
                    if e <= umbral and al <= 0.01:
                        d = nd; fmt = (fmt & ~(0xff << 16)) | (k - 1) << 16; n += 1; break
            d = d + b'\0' * (-len(d) % 32)
            texs[ti] = [w, hh, fmt, li, len(blob), len(d)]; blob += d
    if not n: return None, 0, 0
    tab = b''.join(struct.pack('<6I', *t) for t in texs)
    nuevo = b[:56] + tab + b[56 + 24 * ntex:blob0] + blob
    return nuevo, n, len(b) - len(nuevo)

def aplica_bytes(b):
    exe = pvrtex()
    if not exe: return b
    nuevo, n, ah = libros(b, exe)
    return b if nuevo is None else nuevo

def main():
    a = sys.argv[1:]
    if not a: raise SystemExit(__doc__)
    exe = pvrtex()
    if not exe: print('libros: sin pvrtex (OR2_PVRTEX / OR2_NATIVO / KOS_BASE): sin cambios'); return
    if a[0] == '--datos':
        import grupos_nodos
        os.makedirs(a[2], exist_ok=True)
        return grupos_nodos.datos(a[1], a[2], aplica_bytes)
    tot = 0
    for ruta in sorted(f for p in a for f in (glob.glob(p) if any(c in p for c in '*?[') else [p])):
        b = open(ruta, 'rb').read()
        if b[:4] != b'ORD7': print('%s: no es un ORD7 (sin cambios)' % ruta); continue
        nuevo, n, ah = libros(b, exe)
        if nuevo is None: continue
        tmp = ruta + '.tmp'; open(tmp, 'wb').write(nuevo); os.replace(tmp, ruta)
        print('%s: %d libros reducidos, %d KB menos' % (os.path.basename(ruta), n, ah >> 10)); tot += ah
    print('libros: %d KB menos' % (tot >> 10))

if __name__ == '__main__':
    main()
