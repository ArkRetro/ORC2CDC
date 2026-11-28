"""auditar_vq_pvr.py datos/ [--csv informe.csv] [--rms]: auditoria de las texturas de la PowerVR de una carpeta de
datos (SEGnn / BKnn / TRAF / CHR / BAND / BO / COCHE..., sueltos o empaquetados en SEGS.PAK / UNIONES*.PAK).

Una fila por textura: fichero, indice, ancho, alto, formato (ARGB1555 / RGB565 / ARGB4444), VQ o 16 bits, entradas del
libro, mipmaps, bytes en VRAM (alineados a 32), si la usa alguna malla, y el estado:
 - libro_reducido: VQ con libro de menos de 256 entradas (tools/libros.py o vq.elige16);
 - libro_256_candidata: VQ con libro de 256, sin mipmaps y de 128x64 o menos (tools/libros.py la probo y la dejo por
   calidad, o los datos son de antes de la Rev 2.2); con --rms, el error RMS de los libros de 64 / 128 con pvrtex;
 - libro_256: VQ grande o con mipmaps (el libro pesa poco frente a los indices);
 - 16_bits: sin comprimir; sin_uso: ninguna malla la usa.
Al final, por la salida, el resumen: texturas y KB por estado y por tipo de fichero. Solo lee; nada del disco va al CSV
mas que numeros y nombres de fichero."""
import csv, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import revq

FMT = {0: 'ARGB1555', 1: 'RGB565', 2: 'ARGB4444'}
PREFIJOS = ('SEG', 'BK', 'TRAF', 'CHR', 'BAND', 'BO', 'COCHE')

def ficheros(D):
    paks = {}
    for f in sorted(os.listdir(D)):
        if not f.endswith('.DAT') or not f.startswith(PREFIJOS): continue
        b = open(os.path.join(D, f), 'rb').read()
        if b[:4] == b'PAKD':
            import pak_union
            p = b[20:36].rstrip(b'\0').decode()
            if p not in paks: paks[p] = open(os.path.join(D, p), 'rb').read()
            b = pak_union.recompone(b, paks[p])
        if b[:4] == b'ORD7': yield f, b

def texturas(b):
    h = struct.unpack_from('<13I', b, 4); nm, nt = h[0], h[4]
    us = {struct.unpack_from('<i', b, 56 + 24 * nt + 32 * i + 24)[0] for i in range(nm)}
    for i in range(nt):
        yield (i,) + struct.unpack_from('<6I', b, 56 + 24 * i) + (i in us,)

def estado(w, h, fmt, usada):
    if not usada: return 'sin_uso'
    if not fmt & 0x100: return '16_bits'
    if ((fmt >> 16) & 255) + 1 < 256: return 'libro_reducido'
    if not fmt & 0x200 and w * h <= 128 * 64: return 'libro_256_candidata'
    return 'libro_256'

def main():
    a = sys.argv[1:]
    if not a or a[0].startswith('-'): raise SystemExit(__doc__)
    D = a[0]; sal = a[a.index('--csv') + 1] if '--csv' in a else None; con_rms = '--rms' in a
    if con_rms:
        import numpy as np, tempfile, vq, libros
        exe = libros.pvrtex(); tmp = tempfile.mkdtemp()
        if not exe: raise SystemExit('--rms necesita pvrtex (OR2_PVRTEX / OR2_NATIVO / KOS_BASE)')
    filas = []; res = {}
    for f, b in ficheros(D):
        blob0 = revq.bloque_texeles(b); tipo = next(p for p in PREFIJOS if f.startswith(p))
        for i, w, h, fmt, li, off, tam, usada in texturas(b):
            e = estado(w, h, fmt, usada); vram = (tam + 31) & ~31; r64 = r128 = ''
            if con_rms and e == 'libro_256_candidata':
                img = np.asarray(vq.decodifica(b[blob0 + off:blob0 + off + tam], w, h, fmt & 0xff, 256), dtype=np.uint8)
                for k in libros.entradas(w, h):
                    nd = libros.recodifica(np.ascontiguousarray(img), fmt & 0xff, k, exe, tmp)
                    if nd is None: continue
                    rms = libros.error(img, np.asarray(vq.decodifica(nd, w, h, fmt & 0xff, k)), fmt & 0xff)[0]
                    if k == 64: r64 = '%.2f' % rms
                    else: r128 = '%.2f' % rms
            filas.append([f, i, w, h, FMT.get(fmt & 0xff, fmt & 0xff), 'VQ' if fmt & 0x100 else '16',
                          ((fmt >> 16) & 255) + 1 if fmt & 0x100 else '', 'si' if fmt & 0x200 else 'no', vram,
                          'si' if usada else 'no', e, r64, r128])
            for k in ((e, 'todas'), (e, tipo)):
                n, kb = res.get(k, (0, 0)); res[k] = (n + 1, kb + vram)
    if sal:
        with open(sal, 'w', newline='') as o:
            c = csv.writer(o)
            c.writerow(['fichero', 'textura', 'ancho', 'alto', 'formato', 'codificacion', 'entradas_libro', 'mipmaps',
                        'bytes_vram', 'usada', 'estado', 'rms_libro64', 'rms_libro128'])
            c.writerows(filas)
        print('%d texturas en %s' % (len(filas), sal))
    estados = ('libro_reducido', 'libro_256_candidata', 'libro_256', '16_bits', 'sin_uso')
    tipos = ['todas'] + [p for p in PREFIJOS if any(k[1] == p for k in res)]
    print('%-20s' % 'estado' + ''.join('%16s' % t for t in tipos))
    for e in estados:
        print('%-20s' % e + ''.join('%16s' % ('%d / %d KB' % (res.get((e, t), (0, 0))[0], res.get((e, t), (0, 0))[1] >> 10)) for t in tipos))

if __name__ == '__main__':
    main()
