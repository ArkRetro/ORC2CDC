"""cielo_tramado.py tu.iso datos/ trabajo/ | --prueba tu.iso fichero.DAT...: las texturas del cielo de las etapas y
uniones ya convertidas (SEGnn.DAT y BKnn.DAT, sueltas o en SEGS.PAK / UNIONES.PAK / UNIONESTA.PAK) otra vez desde los 8
bits del original, con la VQ tramada de vq.elige_cielo (revision 44, docs/PARIDAD.md: Cielo, escalones del degradado).
Las conversiones de ahora ya lo hacen (modelo.Modelo.comprime con modelo.cielos); esto evita repetir la conversion
entera (la visibilidad de las etapas tarda horas) en una carpeta de datos hecha antes (tools/regenera.py --interno cielo).

Como se encuentra la imagen de 8 bits de cada textura del cielo de un fichero: las de los bancos del cielo de todos los
descriptores de tramo del ejecutable (conv_union.cielo_de) y de todos los obj_course_obj_sky_* de STAGE.PS2, del mismo tamano,
la de menor error frente a la textura del fichero decodificada (la VQ de antes; tiene que bajar de EMAX). Solo cambian
los texeles de esas texturas (y su tamano en la tabla: el libro reducido es el mismo); el resto del fichero no."""
import os, struct, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import modelo, vq, revq

EMAX = 16.0                                                     # error RMS (0-255) maximo para dar por buena la imagen

_CAND = None
def candidatos(iso):
    """{(ancho, alto): [RGBA uint8]} de las texturas de los bancos del cielo de todos los descriptores de tramo"""
    global _CAND
    if _CAND is not None: return _CAND
    from ps2.assets import Assets
    from ps2 import tex
    import conv_union as U
    A = Assets(iso); E = A.elf; base, nprim, _ = U.tablas(A); vistos = set(); _CAND = {}; bloques = []
    for ti in range(nprim):
        try: bloques.append(U.cielo_de(A, E, U.u32(E, base + 4 * ti))[0])
        except Exception: continue
    for e in A.busca('*obj_course_obj_sky*', 'STAGE.PS2'):   # (y los demas bancos del cielo del disco)
        try: bloques.append(A.bloque(e))
        except Exception: continue
    for blq in bloques:
        if id(blq) in vistos: continue
        vistos.add(id(blq))
        for h in tex.cabeceras(blq):
            a = tex.imagen(blq, h)
            k = (a.shape[1], a.shape[0])
            if not any(np.array_equal(a, x) for x in _CAND.get(k, [])): _CAND.setdefault(k, []).append(a)
    return _CAND

def decodifica(d, w, h, fmt):
    if fmt & 0x100: return vq.decodifica(d, w, h, fmt & 0xff, ((fmt >> 16) & 0xff) + 1)
    return vq.decodifica16(d, w, h, fmt & 0xff)

def aplica_bytes(b, iso):
    """el ORD7 b con sus cielos tramados: (bytes nuevos, texturas cambiadas)"""
    h = struct.unpack_from('<13I', b, 4); nm, ntex = h[0], h[4]
    texs = [list(struct.unpack_from('<6I', b, 56 + 24 * i)) for i in range(ntex)]
    M = np.frombuffer(b, modelo_malla(), nm, 56 + 24 * ntex)
    ini = h[5:10]
    cielo = {int(x['tex']) for x in M[ini[3]:ini[4]] if x['tex'] >= 0 and x['nodo'] in (-256, -257)}
    blob0 = revq.bloque_texeles(b); blob = b''; n = 0
    for ti, (w, hh, fmt, li, off, tam) in enumerate(texs):
        d = b[blob0 + off:blob0 + off + tam]
        if ti in cielo and not fmt & 0x200:
            dec = decodifica(d, w, hh, fmt).astype(float)
            mejor, e = None, EMAX
            for a in candidatos(iso).get((w, hh), []):
                x = float(np.sqrt(((a[..., :3].astype(float) - dec[..., :3]) ** 2).mean()))
                if x < e: mejor, e = a, x
            if mejor is None: print('  textura %d (%dx%d): sin imagen del original (se queda)' % (ti, w, hh))
            else:
                fb = fmt & 0xff; r = vq.elige_cielo(mejor, fb)
                if r is not None: nd, nn, _ = r; nf = (fmt & ~0xFF01FF) | fb | 0x100 | (nn - 1) << 16
                else: nd = modelo.codifica16_tramada(mejor, fb); nf = (fmt & ~0xFF01FF) | fb
                nd = nd + b'\0' * (-len(nd) % 32)
                if nd != d + b'\0' * (-len(d) % 32) or nf != fmt: n += 1
                d, fmt = nd, nf
        d += b'\0' * (-len(d) % 32)
        texs[ti] = [w, hh, fmt, li, len(blob), len(d)]; blob += d
    if not n: return b, 0
    tab = b''.join(struct.pack('<6I', *t) for t in texs)
    return b[:56] + tab + b[56 + 24 * ntex:blob0] + blob, n

def modelo_malla():
    return np.dtype([('c', '<f4', 3), ('r', '<f4'), ('v0', '<u4'), ('n', '<u4'), ('tex', '<i4'), ('nodo', '<i4')])

def main():
    a = sys.argv[1:]
    if len(a) < 3: raise SystemExit(__doc__)
    if a[0] == '--prueba':                                      # en ficheros ORD7 sueltos, en su sitio
        for f in a[2:]:
            b = open(f, 'rb').read(); nb, n = aplica_bytes(b, a[1])
            if n: open(f + '.tmp', 'wb').write(nb); os.replace(f + '.tmp', f)
            print('%s: %d texturas del cielo' % (os.path.basename(f), n))
        return
    import grupos_nodos
    iso = a[0]
    grupos_nodos.datos(a[1], a[2], lambda b: aplica_bytes(b, iso)[0])

if __name__ == '__main__':
    main()
