#!/usr/bin/env python3
"""exporta_obj.py: un modelo ORD7 de este port (COCHEnn.DAT, COCHEnnL.DAT, SEGnn.DAT sueltas...) a Wavefront OBJ.

Uso: exporta_obj.py MODELO.DAT carpeta_salida/ [--zip]
  Escribe modelo.obj (vertices en metros, y arriba, coordenadas de textura y color por vertice como extension
  "v x y z r g b"), modelo.mtl (un material por textura) y tex_NNN.png (cada textura decodificada: VQ o 16 bits, con o
  sin mipmaps; el nivel mayor). Con --zip, ademas carpeta_salida.zip con todo.
  Las mallas son tiras de triangulos (el orden alterna en cada triangulo; los degenerados se quitan). En los coches, las
  piezas animadas (volante, ruedas, pinzas: bloque ANI1, conv_coche.animadas) se ponen en su sitio en reposo. Lee los datos del disco del propio usuario: aqui no
  hay datos del juego. Con un fichero dentro de SEGS.PAK (descriptor PAKD), pasar la carpeta que tiene SEGS.PAK al lado.
"""
import os, sys, mmap, zipfile
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remip, aligera

LISTAS = ('opaca', 'recorte', 'transparente', 'cielo')


def lee(ruta):
    b = open(ruta, 'rb').read()
    if b[:4] == b'PAKD':
        import pak_union
        pak = os.path.join(os.path.dirname(ruta), b[20:36].rstrip(b'\0').decode())
        with open(pak, 'rb') as fp:
            P = mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ); b = pak_union.recompone(b, P); P.close()
    return b


def ani1(b):
    """las 12 piezas {x, y, z, a, b} del bloque ANI1 de un coche (conv_coche.animadas), o None"""
    import struct
    i = b.rfind(b'ANI1')
    while i >= 0:
        n = struct.unpack_from('<I', b, i + 4)[0]
        if n == (4 + 28 + 240 + 4) // 4 and i + 8 + 28 + 240 <= len(b):
            return np.frombuffer(b, '<f4', 60, i + 8 + 28).reshape(12, 5).astype(np.float64)
        i = b.rfind(b'ANI1', 0, i)
    return None


def exporta(ruta, sal, con_zip=False):
    from PIL import Image
    b = lee(ruta); d = remip.parte(b); M, V, texs = d['M'], d['V'], d['texs']; li = remip.lista_de(d)
    os.makedirs(sal, exist_ok=True); base = os.path.splitext(os.path.basename(ruta))[0].lower()
    usadas = sorted(set(int(t) for t in M['ti'] if 0 <= int(t) < len(texs)))
    for t in usadas:
        Image.fromarray(np.ascontiguousarray(aligera.imagen(b, d, t)), 'RGBA').save(os.path.join(sal, 'tex_%03d.png' % t))
    with open(os.path.join(sal, base + '.mtl'), 'w') as f:
        f.write('# %s: un material por textura\nnewmtl sin_textura\nKd 0.8 0.8 0.8\n' % os.path.basename(ruta))
        for t in usadas:
            f.write('\nnewmtl tex_%03d\nKd 1 1 1\nmap_Kd tex_%03d.png\nmap_d tex_%03d.png\n' % (t, t, t))
    P = V['p'].astype(np.float64); UV = V['uv'].astype(np.float64); C = V['c']
    pz = ani1(b)
    if pz is not None:                               # piezas animadas (coches): de su marco a su sitio en reposo
        for m in M:
            k = (int(m['nodo']) >> 8) & 0xff if int(m['nodo']) >= 0 else 0
            if 0 < k < len(pz):
                i0, n = int(m['i0']), int(m['n']); q = P[i0:i0 + n]
                if k == 1: c, s = np.cos(pz[k, 3]), np.sin(pz[k, 3]); q = q @ np.array([[1, 0, 0], [0, c, s], [0, -s, c]])
                P[i0:i0 + n] = q + pz[k, :3]
    ntri = 0
    with open(os.path.join(sal, base + '.obj'), 'w') as f:
        f.write('# %s (ORD7): %d mallas, %d vertices\nmtllib %s.mtl\n' % (os.path.basename(ruta), len(M), len(V), base))
        for i in range(len(V)):
            c = int(C[i]); f.write('v %.5f %.5f %.5f %.4f %.4f %.4f\n' % (P[i, 0], P[i, 1], P[i, 2],
                                    ((c >> 16) & 255) / 255, ((c >> 8) & 255) / 255, (c & 255) / 255))
        for i in range(len(V)): f.write('vt %.5f %.5f\n' % (UV[i, 0], 1.0 - UV[i, 1]))
        for k, m in enumerate(M):
            i0, n, t = int(m['i0']), int(m['n']), int(m['ti'])
            if n < 3: continue
            f.write('g malla_%04d_%s\nusemtl %s\n' % (k, LISTAS[li[k]], 'tex_%03d' % t if t in usadas else 'sin_textura'))
            for j in range(n - 2):
                a, bb, c = i0 + j, i0 + j + 1, i0 + j + 2
                if j & 1: a, bb = bb, a
                if a == bb or bb == c or a == c or np.allclose(P[a], P[bb]) or np.allclose(P[bb], P[c]) or np.allclose(P[a], P[c]): continue
                f.write('f %d/%d %d/%d %d/%d\n' % (a + 1, a + 1, bb + 1, bb + 1, c + 1, c + 1)); ntri += 1
    print('%s: %d vertices, %d triangulos, %d texturas -> %s' % (os.path.basename(ruta), len(V), ntri, len(usadas), sal))
    if con_zip:
        z = sal.rstrip('/\\') + '.zip'
        with zipfile.ZipFile(z, 'w', zipfile.ZIP_DEFLATED) as zf:
            for n in sorted(os.listdir(sal)): zf.write(os.path.join(sal, n), os.path.join(os.path.basename(sal.rstrip('/\\')), n))
        print('zip: %s' % z)


def main():
    a = sys.argv[1:]; z = '--zip' in a; a = [x for x in a if x != '--zip']
    if len(a) != 2: raise SystemExit(__doc__)
    exporta(a[0], a[1], z)


if __name__ == '__main__':
    main()
