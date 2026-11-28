#!/usr/bin/env python3
"""cielos_bk.py: los cielos de las uniones (bifurcaciones y metas, BKnn.DAT) a 64x32 en VQ (DESVIACION pedida por el
usuario, 2026-11-28: "mantén las que estén por debajo de 64 iguales y las que lo sobrepasen modificalas para que estén
64x32 todas y todas comprimidas en VQ").

Uso: cielos_bk.py entrada/ salida/ trabajo/
  entrada/ es la carpeta de datos entera (con UNIONES.PAK, UNIONESTA.PAK, UNIONESR.PAK y sus BKnn.DAT). En salida/ van
  los tres paquetes rehechos y todos sus descriptores BKnn.DAT (y los BKnn.DAT sueltos que cambien).
Las texturas de las mallas de la lista del cielo (lista 3) con algun lado de mas de 64 pasan a 64x32: el lado que baja,
por promedio; el que sube, por interpolacion (remip.remuestrea, con su CLAMP), en VQ de 128 entradas, sin mipmaps (las
banderas 0xFC00 se quedan; el D adjust no hace falta sin mipmaps). Las de 64 o menos no cambian. La misma textura
(mismos bytes) da el mismo resultado en todas las uniones, para que la VRAM la siga compartiendo.
Idempotente: si ya no queda ningun cielo de mas de 64 de lado, no cambia nada."""
import os, sys, shutil, subprocess, mmap
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remip, vq, aligera, pak_union

W_CIELO, H_CIELO, N_LIBRO = 64, 32, 128


def cielos(b, d, hechas):
    li = remip.lista_de(d); texs = d['texs']; res = {}
    ts = sorted(set(int(m['ti']) for k, m in enumerate(d['M']) if li[k] == 3 and 0 <= int(m['ti']) < len(texs)))
    for t in ts:
        w, h, fmt, l, off, tam = texs[t]
        if max(w, h) <= 64 or (fmt & 0xff) > 2: continue
        dat = b[d['blob0'] + off:d['blob0'] + off + tam]; hu = remip.huella(texs[t], dat)
        if hu not in hechas:
            a = aligera.base(dat, w, h, fmt); fb = fmt & 0xff
            a2 = remip.remuestrea(a, W_CIELO, H_CIELO, fmt)
            dd, err = vq.codifica(np.ascontiguousarray(a2), fb, N_LIBRO)
            hechas[hu] = (W_CIELO, H_CIELO, fb | 0x100 | (N_LIBRO - 1) << 16 | (fmt & 0xFC00), dd, err)
        res[t] = hechas[hu]
    return res


def main():
    if len(sys.argv) != 4: raise SystemExit(__doc__)
    E, D, W = sys.argv[1:]
    os.makedirs(D, exist_ok=True); os.makedirs(W, exist_ok=True)
    hechas = {}; tot = [0, 0, 0]
    num = lambda f: int(''.join(c for c in f if c.isdigit()))
    bks = sorted((f for f in os.listdir(E) if remip.es_modelo(f, 'BK')), key=num)
    def tr(f, b):
        d = aligera.parte(b); n = cielos(b, d, hechas)
        if not n: return b
        tot[0] += 1; tot[1] += len(n)
        tot[2] += sum(remip.A32(len(x[3])) - remip.A32(d['texs'][t][5]) for t, x in n.items())
        return aligera.reescribe(b, d, [], n)
    for pak, pre, op in remip.PAQUETES:
        if pre != 'BK': continue
        P = None
        if os.path.isfile(os.path.join(E, pak)):
            fp = open(os.path.join(E, pak), 'rb'); P = mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ)
        e, s = os.path.join(W, 'ent_' + pak), os.path.join(W, 'sal_' + pak)
        for x in (e, s):
            if os.path.isdir(x): shutil.rmtree(x)
            os.makedirs(x)
        cambia = 0
        for f in bks:
            b = open(os.path.join(E, f), 'rb').read()
            if b[:4] == b'ORD7':
                if pak != 'UNIONES.PAK': continue
                nb = tr(f, b)
                if nb != b: open(os.path.join(D, f), 'wb').write(nb)
                continue
            if b[:4] != b'PAKD' or b[20:36].rstrip(b'\0').decode() != pak or P is None: continue
            b = pak_union.recompone(b, P); nb = tr(f, b); cambia += nb != b
            open(os.path.join(e, f), 'wb').write(nb)
        if P is not None: P.close(); fp.close()
        print('%s: %d uniones cambian' % (pak, cambia))
        if cambia:
            subprocess.run([sys.executable, '-u', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pak_union.py'), e, s] + op,
                           check=True, stdout=subprocess.DEVNULL)
            for f in os.listdir(s): shutil.copyfile(os.path.join(s, f), os.path.join(D, f))
        shutil.rmtree(e); shutil.rmtree(s)
    print('cielos: %d uniones, %d texturas a %dx%d VQ (%d distintas); VRAM de las uniones %+.0f KB en total'
          % (tot[0], tot[1], W_CIELO, H_CIELO, len(hechas), tot[2] / 1024))


if __name__ == '__main__':
    main()
