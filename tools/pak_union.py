"""pak_union.py entrada/ salida/ [BKnn.DAT ...]: las uniones (BKnn.DAT, ORD7 de conv_union.py) sin repetir lo que
tienen igual, para que quepan todas las de las misiones en el GD-ROM. Lo que se ve no cambia: la demo recompone
cada fichero byte a byte (comun/gd.c, gd_carga_en) y se comprueba aqui mismo.

Por que se repiten: las 194 LRBK de las misiones son el mismo modelo (cs_LRBK_PALM) con la luz de la zona cocida en el
color de los vertices, los mismos porticos con los carteles de sus dos destinos y el cielo de su etapa; las texturas,
la geometria, las mallas, la ruta y el cielo se repiten entre uniones, y el color de los vertices entre las de la
misma zona.

Salida:
 UNIONES.PAK  los trozos distintos de todas, uno tras otro (en el orden en que aparecen)
 BKnn.DAT     descriptor de la union: 'PAKD', tamano del ORD7 recompuesto, numero de operaciones, inicio y numero de
              los vertices del ORD7, nombre del paquete (16 bytes) y las operaciones (u32 origen en el paquete, u32
              longitud, u32 destino | tipo << 30), ordenadas por origen. Tipo 0: bytes del ORD7 en el destino; 1: del
              flujo de x, y, z, u, v de los vertices (20 bytes por vertice; destino = posicion en el flujo); 2: del
              flujo del color de los vertices (4 bytes por vertice)
Los vertices se separan en esos dos flujos porque la luz solo cambia el color: la posicion y las coordenadas de
textura se comparten entre zonas. Los trozos se cortan por contenido (huella movil de 32 bytes, corte cuando sus
bits bajos son 0; entre MIN y MAX bytes) para que lo igual se encuentre aunque este en otro sitio del fichero."""
import os, struct, sys, hashlib
import numpy as np

PAK = 'UNIONES.PAK'
MIN, MAX, MASCARA, VENT = 256, 16384, (1 << 10) - 1, 32      # trozos de ~1,3 KB de media

_G = np.random.RandomState(0x0c2c).randint(0, 1 << 32, 256, dtype=np.uint64).astype(np.uint32)

def cortes(d):
    """posiciones de corte de d (bytes) por su contenido"""
    n = len(d)
    if n <= MIN: return [n]
    a = np.frombuffer(d, np.uint8); h = np.zeros(n, np.uint32)
    for k in range(VENT):                                    # h[i] = suma de G[d[i - k]] girado k bits
        g = _G[a[:n - k]]; g = (g << np.uint32(k)) | (g >> np.uint32(32 - k)) if k else g
        h[k:] += g
    cand = np.nonzero((h & MASCARA) == 0)[0] + 1
    sal = []; p = 0; j = 0
    while p < n:
        while j < len(cand) and cand[j] < p + MIN: j += 1
        c = int(cand[j]) if j < len(cand) and cand[j] <= p + MAX else min(p + MAX, n)
        c = min(c, n); sal.append(c); p = c
    return sal

def flujos(b):
    """el ORD7 en sus cuatro partes: (tipo, base, bytes) con base el destino del primer byte (tipo 0) o 0 (flujos)"""
    assert b[:4] == b'ORD7'
    h = struct.unpack('<13I', b[4:56]); nm, nv, nt = h[0], h[1], h[4]
    pv = 56 + 24 * nt + 32 * nm
    V = np.frombuffer(b[pv:pv + 24 * nv], np.uint8).reshape(nv, 24)
    return pv, nv, [(0, 0, b[:pv]), (1, 0, V[:, :20].tobytes()), (2, 0, V[:, 20:].tobytes()), (0, pv + 24 * nv, b[pv + 24 * nv:])]

def recompone(desc, pak):
    """el ORD7 a partir del descriptor y el paquete (como comun/gd.c)"""
    tam, nops, pv, nv = struct.unpack('<4I', desc[4:20]); sal = bytearray(tam); F = {1: bytearray(20 * nv), 2: bytearray(4 * nv)}
    for k in range(nops):
        s, l, q = struct.unpack('<3I', desc[36 + 12 * k:48 + 12 * k]); t = q >> 30; q &= 0x3FFFFFFF
        (sal if t == 0 else F[t])[q:q + l] = pak[s:s + l]
    V = np.frombuffer(sal, np.uint8, 24 * nv, pv).reshape(nv, 24).copy()
    V[:, :20] = np.frombuffer(bytes(F[1]), np.uint8).reshape(nv, 20); V[:, 20:] = np.frombuffer(bytes(F[2]), np.uint8).reshape(nv, 4)
    sal[pv:pv + 24 * nv] = V.tobytes()
    return bytes(sal)

def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    ent, sal = a[0], a[1]
    nombres = a[2:] or sorted(f for f in os.listdir(ent) if f.startswith('BK') and f[2:3].isdigit() and f.endswith('.DAT'))
    os.makedirs(sal, exist_ok=True)
    trozos = {}; pak = bytearray(); antes = 0; descs = {}; saltos_max = 0
    for nm in nombres:
        b = open(os.path.join(ent, nm), 'rb').read(); antes += len(b)
        if b[:4] != b'ORD7': raise SystemExit('%s no es ORD7 (ya empaquetado?)' % nm)
        pv, nv, fl = flujos(b); ops = []
        for t, base, d in fl:
            p = 0
            for c in cortes(d):
                x = d[p:c]; k = hashlib.sha1(x).digest()
                if k not in trozos: trozos[k] = len(pak); pak += x
                ops.append([trozos[k], len(x), base + p, t]); p = c
        ops.sort()
        un = []                                              # los seguidos en el paquete y en el destino, en uno
        for o in ops:
            if un and un[-1][3] == o[3] and un[-1][0] + un[-1][1] == o[0] and un[-1][2] + un[-1][1] == o[2]: un[-1][1] += o[1]
            else: un.append(o)
        d = b'PAKD' + struct.pack('<4I', len(b), len(un), pv, nv) + PAK.encode().ljust(16, b'\0')
        d += b''.join(struct.pack('<3I', s, l, q | t << 30) for s, l, q, t in un)
        descs[nm] = d
        saltos = sum(1 for i in range(1, len(un)) if un[i][0] - (un[i - 1][0] + un[i - 1][1]) > 65536)
        saltos_max = max(saltos_max, saltos)
        print('%s: %d KB -> %d operaciones, %d saltos de mas de 64 KB en el paquete' % (nm, len(b) >> 10, len(un), saltos))
    for nm, d in descs.items():
        assert recompone(d, pak) == open(os.path.join(ent, nm), 'rb').read(), nm
        open(os.path.join(sal, nm), 'wb').write(d)
    open(os.path.join(sal, PAK), 'wb').write(pak)
    despues = len(pak) + sum(-(-len(d) // 2048) * 2048 for d in descs.values())
    print('%d uniones: %.1f MiB -> %.1f MiB (paquete %.1f MiB); saltos maximos %d; recompuestas iguales' %
          (len(descs), antes / 2**20, despues / 2**20, len(pak) / 2**20, saltos_max))

if __name__ == '__main__':
    main()
