"""revq.py fichero.DAT... : la segunda pasada de la VQ (vq.elige16, plan de recursos 2026-11-12) sobre modelos ORD7 ya
convertidos, en su sitio. Lo mismo que hace modelo.comprime al convertir desde la ISO: las texturas usadas por alguna
malla que siguen en 16 bits (sin mipmaps, de 32x32 o mas) se codifican desde sus texeles de 16 bits y pasan a VQ si el
error es <= vq.UMBRAL2 y ocupan menos; el resto del fichero no cambia (las texturas van al final, cada una alineada a
32). Sirve para no repetir la conversion entera (la visibilidad de las etapas tarda horas) y da los mismos bytes que
la conversion con el modelo.py de ahora. Los coches del jugador (COCHEnn.DAT) no: conv_coche.py no usa la segunda
pasada. Un fichero que no cambia no se escribe. Imprime por fichero las texturas convertidas y los KB ahorrados."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vq


def bloque_texeles(b):
    """desplazamiento del bloque de texeles de un ORD7 (como lee_modelo_en de la demo y vram.lee_dat)"""
    h = struct.unpack_from('<13I', b, 4)
    nmallas, nvert, nruta, paso, ntex = h[:5]; nnodos, ncart, nsec = h[10], h[11], h[12] & 0x7FFFFFFF
    p = 56 + 24 * ntex + 32 * nmallas + 24 * nvert + nruta * paso * 4 + 32 * nnodos + 64 * ncart
    if nsec:
        ini = struct.unpack_from('<%dI' % (nsec + 1), b, p); p += (nsec + 1) * 4
        if h[12] >> 31: p += ini[nsec]; p = (p + 1) & ~1
        else: p += ini[nsec] * 2
        p += nruta * 2
    p = (p + 3) & ~3
    return (p + 31) & ~31


def revq(b, umbral=vq.UMBRAL2):
    """bytes de un ORD7 -> (bytes nuevos o None si nada cambia, texturas convertidas, bytes ahorrados)"""
    if b[:4] != b'ORD7': raise ValueError('no es un ORD7')
    h = struct.unpack_from('<13I', b, 4); nmallas, ntex = h[0], h[4]
    texs = [list(struct.unpack_from('<6I', b, 56 + 24 * i)) for i in range(ntex)]
    p_mal = 56 + 24 * ntex
    usadas = {struct.unpack_from('<i', b, p_mal + 32 * i + 24)[0] for i in range(nmallas)}
    blob0 = bloque_texeles(b); blob = b''; n = 0
    for ti, (w, hh, fmt, li, off, tam) in enumerate(texs):
        d = b[blob0 + off:blob0 + off + tam]
        if ti in usadas and not fmt & 0x300:
            r = vq.elige16(d, w, hh, fmt & 0xff, umbral)
            if r is not None:
                d, nn, err = r; fmt = fmt | 0x100 | (nn - 1) << 16; n += 1
        d += b'\0' * (-len(d) % 32)
        texs[ti] = [w, hh, fmt, li, len(blob), len(d)]; blob += d
    if not n: return None, 0, 0
    tab = b''.join(struct.pack('<6I', *t) for t in texs)
    nuevo = b[:56] + tab + b[56 + 24 * ntex:blob0] + blob
    return nuevo, n, len(b) - len(nuevo)


def main():
    tot = 0
    for ruta in sys.argv[1:]:
        b = open(ruta, 'rb').read()
        if b[:4] != b'ORD7': print('%s: no es un ORD7 (sin cambios)' % ruta); continue
        nuevo, n, ah = revq(b)
        if nuevo is None: continue
        tmp = ruta + '.tmp'; open(tmp, 'wb').write(nuevo); os.replace(tmp, ruta)
        print('%s: %d texturas a VQ, %d KB menos' % (os.path.basename(ruta), n, ah >> 10)); tot += ah
    print('revq: %d KB menos' % (tot >> 10))


if __name__ == '__main__':
    main()
