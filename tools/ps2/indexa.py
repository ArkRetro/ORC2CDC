#!/usr/bin/env python3
"""indexa.py <iso> <salida.json> [--rehacer-nombres]: catalogo de todo el disco (tu ISO) en JSON, sin extraer
ficheros a disco. La salida es tuya y no debe ir al repositorio (lleva nombres de ficheros del juego).

Por cada fichero ISO 9660: nombre, desplazamiento y tamano. Por cada entrada de cada paquete *.PS2:
  ruta ('PAQUETE/nombre' o 'PAQUETE#indice' si no se conoce el nombre), paquete, indice (orden de desplazamiento,
  el que usan los conv_*.py), hash, nombre, off (absoluto en la ISO), tam, guardado, comprimido (zlib del paquete o
  .sz), tipo (assets.detecta) e info: segun el tipo, texturas [(ancho, alto, psm)], registros, poligonos, escenas,
  muestras, segundos, modulo, segmento (STAGE.PS2)...
Los nombres se reconstruyen con assets.Assets.resolver() (cache en OR2_CACHE o ~/.cache/or2_assets)."""
import json, os, struct, sys, time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from ps2.assets import Assets, Elf, descomprime, es_zlib, detecta, _bloque_pmt
from ps2 import stage, tex

PSM = {0: 'CT32', 0x13: 'T8', 0x14: 'T4', 2: 'CT16'}


class _Mem:
    def __init__(self, d): self.m = d


def _tex_bloque(m, o, w):
    """(n registros, [(ancho, alto, psm)]) de un bloque de geometria en m[o:]"""
    b = stage.Bloque(_Mem(m), o, w)
    try: hs = tex.cabeceras(b)
    except Exception: hs = []
    return [(1 << (h[11] >> 24), 1 << (h[12] & 0xff), PSM.get((h[11] >> 16) & 0xff, (h[11] >> 16) & 0xff)) for h in hs]


def _xst(d):
    H = d[4:]; meta, nt = struct.unpack_from('<II', H, 4); r = []
    for k in range(nt):
        o = struct.unpack_from('<I', H, meta + 4 * k)[0]; h = struct.unpack_from('<14I', H, meta + o)
        r.append((1 << (h[11] >> 24), 1 << (h[12] & 0xff), PSM.get((h[11] >> 16) & 0xff, (h[11] >> 16) & 0xff)))
    imgs = struct.unpack_from('<I', H, 0x18)[0]
    return r, imgs


def _muestras(d, desde, n):
    """cuenta de muestras PS-ADPCM (bloques con la marca de fin) en d[desde:desde+n]"""
    import numpy as np
    a = np.frombuffer(d, np.uint8, count=max(0, min(n, len(d) - desde)) // 16 * 16, offset=desde)
    return int(np.count_nonzero(a[1::16] & 1))


def detalla(A, e, t, info):
    """datos basicos por tipo (lee la entrada entera solo cuando hace falta)"""
    if t == 'modelo':
        if e.comprimida or es_zlib(A.crudo(e, 2)):
            d = A.datos(e); w = _bloque_pmt(d)
            if w: info['texs'] = _tex_bloque(d, 0, w); info['registros'] = w[0]
        else:
            w = struct.unpack_from('<4I', A.m, e.off)
            if w[2] + w[3] + 16 == e.tam:
                info['texs'] = _tex_bloque(A.m, e.off, w); info['registros'] = w[0]
        info.pop('texturas', None)
    elif t == 'xst':
        try: info['texs'], info['imagenes'] = _xst(A.datos(e))
        except Exception as x: info['error'] = str(x)
        info.pop('texturas', None)
    elif t == 'sprani':
        d = A.datos(e); a = d[4:]; k = 0
        while k * 4 + 4 <= len(a) and struct.unpack_from('<I', a, 4 * k)[0]: k += 1
        info['escenas'] = k
    elif t in ('banco_ics', 'banco_se'):
        d = A.datos(e); info['muestras'] = _muestras(d, 16, info.get('adpcm', 0))
        if info.get('adpcm2'): info['muestras2'] = _muestras(d, 16 + info['adpcm'], info['adpcm2'])
    elif t == 'musica':
        info['segundos_48k'] = round(e.tam / 16 * 28 / 2 / 48000, 1)
    elif t == 'irx':
        try: info['modulo'] = Elf(A.datos(e)).modulo()
        except Exception: pass
    elif t == 'texto_idioma':
        d = A.datos(e, 64); info['primeros'] = list(struct.unpack_from('<4I', d, 8))
    return info


def indexa(ruta_iso, salida, rehacer=False, info=True):
    t0 = time.time()
    A = Assets(ruta_iso, nombres=False)
    if rehacer: A.resolver(forzar=True, info=info)
    else: A.resolver(info=info)
    t1 = time.time()
    if info: print('nombres: %.1f s' % (t1 - t0))
    ent = []
    for p in A.paquetes():
        for e in A.paquete(p):
            t, inf = A.tipo(e)
            inf = detalla(A, e, t, inf)
            r = dict(ruta=p + ('/' + e.nombre.lstrip('\\').replace('\\', '/') if e.nombre else '#%d' % e.indice),
                     paquete=p, indice=e.indice, hash='%08x' % e.hash, nombre=e.nombre, off=e.off, tam=e.tam,
                     guardado=e.guardado, comprimido=inf.pop('zlib', False), tipo=t)
            if e.segmento is not None: r['segmento'] = e.segmento
            if inf: r['info'] = inf
            ent.append(r)
        if info: print('  %-11s %4d entradas  %.1f s' % (p, len(A.paquete(p)), time.time() - t1))
    out = dict(iso=A.ruta, id=A.id, creado=time.strftime('%Y-%m-%d %H:%M:%S'),
               segundos=round(time.time() - t0, 1),
               ficheros=[dict(nombre=n, off=o, tam=s) for n, o, s in A.lista()],
               paquetes={p: dict(n=len(A.paquete(p)), off=A.iso.fich[p][0], tam=A.iso.fich[p][1]) for p in A.paquetes()},
               entradas=ent)
    os.makedirs(os.path.dirname(os.path.abspath(salida)), exist_ok=True)
    with open(salida, 'w') as f: json.dump(out, f, separators=(',', ':'))
    n = len(ent); con = sum(1 for x in ent if x['nombre']); tip = sum(1 for x in ent if x['tipo'] != 'desconocido')
    if info:
        print('%d entradas, %d con nombre (%.1f%%), %d con tipo (%.1f%%), %.1f s -> %s (%d KB)' % (
            n, con, 100.0 * con / n, tip, 100.0 * tip / n, time.time() - t0, salida, os.path.getsize(salida) >> 10))
    return out


def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    if len(a) < 2: raise SystemExit(__doc__.split('\n')[0])
    indexa(a[0], a[1], '--rehacer-nombres' in sys.argv)


if __name__ == '__main__':
    main()
