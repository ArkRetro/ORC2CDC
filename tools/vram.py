"""vram.py datos/ [--coche COCHEnn.DAT] [--fijo KB] [--zona KB]: memoria de video de cada combinacion del curso con el
mismo calculo que la demo. Antes (un pvr_mem_malloc por textura; el dlmalloc de la VRAM de KallistiOS,
pvr_mem_core.c, gasta por bloque el tamano + 4 redondeado a 32: request2size) y ahora (demo/main.c sube_tramo: la zona
del curso, texturas alineadas a 32 y la meta comparte con su etapa las texturas iguales). --zona: el tamano de la zona
del curso medido en Flycast ("VRAM: zona del curso"), para dar lo libre de cada combinacion.

Combinaciones (curso_paso): etapa + su union (BKkk.DAT) y union + etapa de cada rama. BKD.DAT no sube texturas.
Ademas cuenta las texturas que se repiten (mismos bytes, ancho, alto y formato) entre la union y la etapa con la que
convive y entre las uniones, y lo que ocupa lo fijo (coche, trafico, personajes, efectos, sol). Lo que no esta en los
.DAT (HUD de carrera, letreros, humo, bufer del menu de pausa) se mide en Flycast: --fijo KB lo suma.

Sin argumentos de salida: imprime la tabla."""
import os, struct, sys, hashlib

def bloque(tam):
    """bytes que gasta pvr_mem_malloc(tam) (request2size de pvr_mem_core.c: SIZE_SZ 4, alineacion 32)"""
    return max(32, (tam + 4 + 31) & ~31)

def lee_dat(ruta):
    """texturas de un modelo ORD5/6/7 (como lee_modelo_en): [(w, h, fmt, lista, off, tam, sha1 de los texeles)]"""
    b = open(ruta, 'rb').read()
    v7 = b[:4] == b'ORD7'
    if not v7 and b[:4] not in (b'ORD5', b'ORD6'): raise SystemExit('%s: no es un modelo' % ruta)
    h = list(struct.unpack_from('<%dI' % (13 if v7 else 10), b, 4)) + [0] * 3
    nmallas, nvert, nruta, paso, ntex = h[:5]; nnodos, ncart, nsec = h[10], h[11], h[12] & 0x7FFFFFFF
    p = 56 if v7 else 44
    texs = [struct.unpack_from('<6I', b, p + 24 * i) for i in range(ntex)]
    p += 24 * ntex + 32 * nmallas + 24 * nvert + nruta * paso * 4 + 32 * nnodos + 64 * ncart
    if nsec:
        ini = struct.unpack_from('<%dI' % (nsec + 1), b, p); p += (nsec + 1) * 4
        if h[12] >> 31: p += ini[nsec]; p = (p + 1) & ~1
        else: p += ini[nsec] * 2
        p += nruta * 2
    p = (p + 3) & ~3
    blob = (p + 31) & ~31
    return [t + (hashlib.sha1(bytes([t[0] & 0xff, t[1] & 0xff, t[2] & 0xff]) + struct.pack('<3I', *t[:3]) +
                              b[blob + t[4]:blob + t[4] + t[5]]).hexdigest(),) for t in texs]

def kb(texs): return sum(bloque(t[5]) for t in texs) / 1024.0

def curso(d):
    b = open(os.path.join(d, 'CURSO.DAT'), 'rb').read()
    n, r = struct.unpack_from('<2I', b, 4)
    return [(b[12 + r * i:16 + r * i].decode(), b[16 + r * i], b[17 + r * i],
             struct.unpack_from('<b', b, 18 + r * i)[0], struct.unpack_from('<b', b, 19 + r * i)[0]) for i in range(n)]

def kb_zona(texs): return sum((t[5] + 31) & ~31 for t in texs) / 1024.0

def main():
    d = sys.argv[1]; arg = sys.argv[2:]
    coche = arg[arg.index('--coche') + 1] if '--coche' in arg else 'COCHE06.DAT'
    fijo_ext = float(arg[arg.index('--fijo') + 1]) if '--fijo' in arg else 0.0
    zona = float(arg[arg.index('--zona') + 1]) if '--zona' in arg else 0.0
    cur = curso(d)
    seg = {k: lee_dat(os.path.join(d, 'SEG%02d.DAT' % c[1])) for k, c in enumerate(cur)}
    bk = {k: lee_dat(os.path.join(d, 'BK%02d.DAT' % k)) for k in range(len(cur))}
    fijo = {}
    for f in [coche] + ['TRAF%d.DAT' % i for i in range(9)] + ['CHR.DAT', 'SOL.DAT']:
        if os.path.exists(os.path.join(d, f)): fijo[f] = kb(lee_dat(os.path.join(d, f)))
    tot_fijo = sum(fijo.values()) + fijo_ext
    print('fijo (%s): %.0f KB  [%s]' % (coche, tot_fijo, ', '.join('%s %.0f' % kv for kv in fijo.items())))
    # antes: un pvr_mem_malloc por textura (bloque) y nada compartido; despues: zona del curso (sin cabecera de
    # bloque, alineadas a 32) y la meta usa las texturas iguales de su etapa (demo/main.c sube_tramo)
    print('%-16s %6s %6s %7s %7s %7s%s' % ('combinacion', 'etapa', 'union', 'antes', 'compart', 'despues', '   libre' if zona else ''))
    filas = []
    for k, c in enumerate(cur):
        hijos = [h for h in (c[3], c[4]) if h >= 0]
        for nombre, e, u, meta in [('%s + BK%02d' % (c[0], k), seg[k], bk[k], not hijos)] + \
                                  [('BK%02d + %s' % (k, cur[h][0]), seg[h], bk[k], False) for h in hijos]:
            he = {t[6] for t in e}; rep = [t for t in u if t[6] in he] if meta else []
            des = kb_zona(e) + kb_zona(u) - kb_zona(rep)
            filas.append((des, nombre, kb(e), kb(u), kb(e) + kb(u), kb_zona(rep)))
    for des, nombre, e, u, antes, r in filas:
        print('%-16s %6.0f %6.0f %7.0f %7.0f %7.0f%s' % (nombre, e, u, antes, r, des, '%8.0f' % (zona - des) if zona else ''))
    t0 = {t[6]: t for k in bk for t in bk[k]}
    comun = None
    for k in bk:
        if cur[k][3] >= 0: hs = {t[6] for t in bk[k]}; comun = hs if comun is None else comun & hs
    print('texturas comunes a las bifurcaciones: %d, %.0f KB' % (len(comun or ()), kb([t0[h] for h in comun or ()])))
    peor = max(filas)
    print('peor: %s %.0f KB%s' % (peor[1], peor[0], ' (zona %.0f KB: libre %.0f KB)' % (zona, zona - peor[0]) if zona else ''))

if __name__ == '__main__':
    main()
