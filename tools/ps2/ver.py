#!/usr/bin/env python3
"""ver.py: inspeccion rapida de los assets del disco (tu ISO) a partir del indice de indexa.py.

  ver.py [-i indice.json] [-o dir] orden argumentos...
  lista [patron] [-t tipo] [-p paquete] [-n max]   entradas del indice (glob sobre la ruta, sin mayusculas)
  tipos                                            cuenta por paquete y tipo
  info REF                                         la entrada y su deteccion completa
  hex REF [desde] [n] [--crudo]                    volcado hexadecimal (descomprimido salvo --crudo)
  saca REF                                         escribe los bytes descomprimidos en el directorio de salida
  png REF [k]                                      texturas de un XST o de un modelo (_pmt) a PNG (todas o la k)
  escena REF k [cuadro]                            vista previa 640x480 de la escena k de una animacion SPRANI
  wav REF [k] [--seg S] [--hz F]                   muestra k (o todas) de un banco .PAK, o S segundos de musica
  elf VA [n]                                       volcado del ejecutable por direccion virtual (hex)
  firma HEX|"texto"                                direcciones del ejecutable donde aparece la firma
  cadenas patron                                   cadenas del ejecutable que encajan con la regex
REF: 'PAQUETE#indice', 'PAQUETE/ruta' (la del indice), o un nombre o glob que solo encaje con una entrada.
El indice por defecto es OR2_INDICE; la salida por defecto, OR2_SALIDA o <dir del indice>/ver."""
import fnmatch, json, os, re, struct, sys, time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def _args():
    a = sys.argv[1:]; op = {'i': os.environ.get('OR2_INDICE'), 'o': os.environ.get('OR2_SALIDA')}
    libres = []; k = 0
    while k < len(a):
        x = a[k]
        if x in ('-i', '-o', '-t', '-p', '-n', '--seg', '--hz'): op[x.lstrip('-')] = a[k + 1]; k += 2; continue
        if x.startswith('--'): op[x[2:]] = True; k += 1; continue
        libres.append(x); k += 1
    if not op['i']: raise SystemExit('falta el indice (-i o OR2_INDICE)')
    if not op['o']: op['o'] = os.path.join(os.path.dirname(os.path.abspath(op['i'])), 'ver')
    return op, libres


class Ver:
    def __init__(self, op):
        self.op = op; self.I = json.load(open(op['i'])); self.E = self.I['entradas']; self._A = None

    @property
    def A(self):
        if self._A is None:
            from ps2.assets import Assets
            self._A = Assets(self.I['iso'])
        return self._A

    def ref(self, r):
        if '#' in r:
            p, k = r.split('#'); p = p.upper()
            return next(e for e in self.E if e['paquete'] == p and e['indice'] == int(k))
        rl = r.lower().replace('\\', '/')
        for e in self.E:
            if e['ruta'].lower() == rl: return e
        c = [e for e in self.E if fnmatch.fnmatch(e['ruta'].lower(), '*' + rl + ('' if any(ch in rl for ch in '*?') else '*'))]
        if len(c) == 1: return c[0]
        exact = [e for e in c if e['ruta'].lower().split('/')[-1].split('.')[0] == rl.split('/')[-1]]
        if len(exact) == 1: return exact[0]
        raise SystemExit('%s: %d entradas encajan%s' % (r, len(c), ': ' + ', '.join(e['ruta'] for e in c[:8]) if c else ''))

    def entrada(self, e):
        return self.A.paquete(e['paquete'])[e['indice']]

    def salida(self, nombre):
        os.makedirs(self.op['o'], exist_ok=True)
        return os.path.join(self.op['o'], re.sub(r'[^A-Za-z0-9_.#-]+', '_', nombre))

    # ------------------------------------------------ ordenes
    def lista(self, pat=None):
        n = int(self.op.get('n') or 1 << 30); k = 0
        for e in self.E:
            if self.op.get('t') and e['tipo'] != self.op['t']: continue
            if self.op.get('p') and e['paquete'] != self.op['p'].upper(): continue
            if pat and not fnmatch.fnmatch(e['ruta'].lower(), ('*%s*' % pat.lower()) if not any(c in pat for c in '*?') else pat.lower()):
                continue
            inf = e.get('info', {}); ex = ''
            if 'texs' in inf: ex = '%d tex' % len(inf['texs'])
            for c in ('registros', 'poligonos', 'escenas', 'muestras', 'segundos_48k', 'modulo', 'n'):
                if c in inf: ex += ' %s=%s' % (c, inf[c])
            print('%-11s %4d %s %9d %s %-14s %s%s' % (e['paquete'], e['indice'], e['hash'], e['tam'], 'z' if e['comprimido'] else '-',
                                                    e['tipo'], (e['nombre'] or '?'), ('  [' + ex.strip() + ']') if ex else ''))
            k += 1
            if k >= n: break
        print('%d entradas' % k, file=sys.stderr)

    def tipos(self):
        import collections
        c = collections.Counter((e['paquete'], e['tipo']) for e in self.E)
        nom = collections.Counter(e['paquete'] for e in self.E if e['nombre'])
        tot = collections.Counter(e['paquete'] for e in self.E)
        for p in self.I['paquetes']:
            print('%-11s %4d entradas, %4d con nombre: %s' % (p, tot[p], nom[p], ', '.join(
                '%s %d' % (t, n) for (q, t), n in sorted(c.items(), key=lambda x: -x[1]) if q == p)))

    def info(self, r):
        e = self.ref(r); print(json.dumps(e, indent=1)[:4000])
        x = self.entrada(e); t, inf = self.A.tipo(x, completo=True); print('deteccion completa:', t, inf)

    def hex(self, r, desde='0', n='256'):
        e = self.ref(r); x = self.entrada(e); desde = int(desde, 0); n = int(n, 0)
        d = bytes(self.A.crudo(x)) if self.op.get('crudo') else self.A.datos(x, desde + n)
        for o in range(desde, min(len(d), desde + n), 16):
            b = d[o:o + 16]
            print('%08x  %-48s %s' % (o, ' '.join('%02x' % c for c in b), ''.join(chr(c) if 32 <= c < 127 else '.' for c in b)))

    def saca(self, r):
        e = self.ref(r); d = self.A.datos(self.entrada(e)); s = self.salida(e['ruta'].replace('/', '_'))
        open(s, 'wb').write(d); print(s, len(d))

    def png(self, r, k=None):
        from PIL import Image
        e = self.ref(r); x = self.entrada(e); res = []
        if e['tipo'] == 'xst':
            T = self.A.xst(x); res = list(enumerate(T))
        elif e['tipo'] == 'modelo':
            res = [(i, a) for i, (h, a) in enumerate(self.A.texturas_bloque(x)) if a is not None]
        else:
            raise SystemExit('png: %s es %s (hace falta xst o modelo)' % (e['ruta'], e['tipo']))
        base = e['ruta'].replace('/', '_')
        for i, a in res:
            if k is not None and i != int(k): continue
            s = self.salida('%s_%02d.png' % (base, i)); Image.fromarray(a, 'RGBA').save(s)
            print(s, '%dx%d' % (a.shape[1], a.shape[0]))
        if k is None and len(res) > 1:                       # hoja con todas
            W = 1024; x0 = y0 = fila = 0; pos = []
            for i, a in res:
                h, w = a.shape[:2]
                if x0 + w > W: x0 = 0; y0 += fila + 2; fila = 0
                pos.append((x0, y0)); x0 += w + 2; fila = max(fila, h)
            hoja = Image.new('RGBA', (W, y0 + fila), (40, 40, 40, 255))
            for (i, a), p in zip(res, pos): hoja.alpha_composite(Image.fromarray(a, 'RGBA'), p)
            s = self.salida(base + '_hoja.png'); hoja.save(s); print(s)

    def escena(self, r, k, cuadro='0'):
        import numpy as np
        from PIL import Image
        e = self.ref(r); x = self.entrada(e)
        if e['tipo'] != 'sprani': raise SystemExit('escena: %s no es SPRANI' % e['ruta'])
        T = self.A.xst(self.A.sprani_xst(x)); pz = self.A.escena(x, int(k), float(cuadro))
        lienzo = Image.new('RGBA', (640, 480), (24, 24, 32, 255))
        for p in pz:
            if p['tex'] >= len(T): continue
            l, t, rr, b = p['src']; tx = Image.fromarray(T[p['tex']], 'RGBA')
            sp = tx.crop((int(round(l)), int(round(t)), max(int(round(l)) + 1, int(round(rr))), max(int(round(t)) + 1, int(round(b)))))
            sp = sp.resize((max(1, int(round(p['w']))), max(1, int(round(p['h'])))))
            if p['alfa'] < 1:
                a = np.array(sp); a[..., 3] = (a[..., 3] * max(0.0, p['alfa'])).astype(np.uint8); sp = Image.fromarray(a, 'RGBA')
            try: Mi = np.linalg.inv(p['M'])
            except np.linalg.LinAlgError: continue
            capa = sp.transform((640, 480), Image.AFFINE, (Mi[0, 0], Mi[1, 0], Mi[2, 0], Mi[0, 1], Mi[1, 1], Mi[2, 1]),
                                resample=Image.BILINEAR)
            lienzo.alpha_composite(capa)
        s = self.salida('%s_escena%d_%s.png' % (e['ruta'].replace('/', '_'), int(k), cuadro)); lienzo.save(s)
        print(s, '%d piezas' % len(pz))

    def _wav(self, s, pcm, hz, canales=1):
        import wave
        w = wave.open(s, 'wb'); w.setnchannels(canales); w.setsampwidth(2); w.setframerate(hz)
        w.writeframes(struct.pack('<%dh' % len(pcm), *pcm)); w.close()

    def wav(self, r, k=None):
        e = self.ref(r); x = self.entrada(e); base = e['ruta'].replace('/', '_')
        if e['tipo'] == 'musica':
            import numpy as np
            seg = float(self.op.get('seg') or 20); hz = int(self.op.get('hz') or 48000)
            n = int(seg * hz / 28 * 16 * 2) // 0x4000 * 0x4000 + 0x4000
            d = self.A.datos(x, min(n, x.tam)); L = []; R = []
            for o in range(0, len(d) - 0x3fff, 0x4000):
                L.append(d[o:o + 0x2000]); R.append(d[o + 0x2000:o + 0x4000])
            l = self.A.adpcm(b''.join(L)); rr = self.A.adpcm(b''.join(R)); m = min(len(l), len(rr))
            pcm = np.empty(2 * m, np.int16); pcm[0::2] = l[:m]; pcm[1::2] = rr[:m]
            s = self.salida('%s_%ds.wav' % (base, seg)); self._wav(s, pcm.tolist(), hz, 2); print(s, '%.1f s' % (m / hz)); return
        if e['tipo'] not in ('banco_se', 'banco_ics'): raise SystemExit('wav: %s es %s' % (e['ruta'], e['tipo']))
        d = self.A.datos(x); n0 = struct.unpack_from('<I', d, 0)[0]
        hz = int(self.op.get('hz') or 22050); pitch = {}
        if e['tipo'] == 'banco_ics':                          # frecuencias de la tabla del IRX de sonido
            try:
                _, t = self.A.banco_ics(); pitch = {o + 16: p for o, n, p in t}
            except Exception: pass
        trozos = []; i = 16; ini = 16
        while i + 16 <= 16 + n0 and i + 16 <= len(d):
            if d[i + 1] & 1: trozos.append((ini, i + 16)); ini = i + 16
            i += 16
        trozos = [t for t in trozos if any(d[t[0] + 2:t[1]])]   # fuera los bloques mudos
        print('%d muestras' % len(trozos))
        for j, (a, b) in enumerate(trozos):
            if k is not None and j != int(k): continue
            f = round(pitch[a] * 48000 / 4096) if a in pitch else hz
            s = self.salida('%s_%03d.wav' % (base, j)); self._wav(s, self.A.adpcm(d, a, b), f); print(s, b - a, 'bytes', f, 'Hz')

    def elf(self, va, n='64'):
        E = self.A.elf; va = int(va, 0); d = E.lee(va, int(n, 0))
        for o in range(0, len(d), 16):
            b = d[o:o + 16]; print('%08x  %-48s %s' % (va + o, ' '.join('%02x' % c for c in b), ''.join(chr(c) if 32 <= c < 127 else '.' for c in b)))

    def firma(self, f):
        try: b = bytes.fromhex(f)
        except ValueError: b = f.encode('latin1')
        r = self.A.elf.busca(b); print(' '.join('0x%x' % v for v in r[:200]), '(%d)' % len(r))

    def cadenas(self, pat):
        rx = re.compile(pat, re.I)
        for v, s in self.A.elf.cadenas():
            if rx.search(s): print('0x%08x %s' % (v, s))


def main():
    op, a = _args()
    if not a: raise SystemExit(__doc__)
    V = Ver(op); t0 = time.time()
    getattr(V, a[0])(*a[1:])
    if op.get('tiempo'): print('%.2f s' % (time.time() - t0), file=sys.stderr)


if __name__ == '__main__':
    main()
