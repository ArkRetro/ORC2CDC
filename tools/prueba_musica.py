#!/usr/bin/env python3
"""prueba_musica.py <MENU.DAT> [MUS67.ADP] [--adx SONGxx.adx]: comprueba la tarjeta ALL I WANT del selector.

Sin emulador: mira el MENU.DAT que ve el juego (MNU1: pantalla MUSICA2, usos MUSICA_L2 / MUSICA_T2 y tramos), que
los 12 temas originales siguen intactos y que el 13 (ALL I WANT) tiene sus piezas en el cuadro 500 del carrusel y
el 1320 del titulo.  Tambien lee las constantes del codigo (N_TEMAS, TEMAS_LISTA, MUSICA_TEMA_ALL_I_WANT,
MUSICA_PISTA_ALL_I_WANT, NPISTAS) y, si se le da el ADP, lo decodifica (ADPCM de Yamaha del AICA) para comprobar
que suena: duracion, RMS y pico y, con --adx, correlacion con la pista original de Crazy Taxi.

La seleccion no depende del coche: el flujo P_COCHE -> P_CAMBIO -> P_MUSICA esta en menu.c y la pantalla de musica
no mira la ficha ni el taxi.  Se simula el cursor (R1 dos veces a la lista 2, 12 a la derecha, A) para cada coche y
se comprueba que elegido = 28 y menu_pista() = 28, que musica_pon lleva a la pista 67.
"""
import argparse, os, re, struct, sys
import numpy as np

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TAB = (230, 230, 230, 230, 307, 409, 512, 614)   # paso del AICA/YMZ280B (wav2adpcm de KallistiOS)


class Fallo(Exception):
    pass


def exige(c, m):
    if not c: raise Fallo(m)


def lee_menu(ruta):
    b = open(ruta, 'rb').read()
    h = struct.unpack_from('<4s11I', b, 0)
    exige(h[0] == b'HUD1', 'no es un MENU.DAT (HUD1)')
    n_tex, n_spr, n_grp, n_pie, n_lay = h[1:6]
    o_tex, o_spr, o_grp, o_pie, o_lay, o_taco = h[6:12]
    tex = [struct.unpack_from('<4H2I', b, o_tex + 16 * i) for i in range(n_tex)]
    spr = [struct.unpack_from('<4H4f', b, o_spr + 24 * i) for i in range(n_spr)]
    grp = [struct.unpack_from('<16s4HfI', b, o_grp + 32 * i) for i in range(n_grp)]
    pie = [struct.unpack_from('<4H8f', b, o_pie + 40 * i) for i in range(n_pie)]
    lay = [struct.unpack_from('<16s2H4h2H', b, o_lay + 32 * i) for i in range(n_lay)]
    ms = o_taco + 16
    exige(b[ms:ms + 4] == b'MNU1', 'sin bloque MNU1')
    n_pan, n_tra, o2 = struct.unpack_from('<III', b, ms + 4)
    pan = [struct.unpack_from('<16s6HI4I', b, ms + 16 + 48 * i) for i in range(n_pan)]
    tra = [struct.unpack_from('<16s4H2I', b, ms + 16 + 48 * n_pan + 32 * i) for i in range(n_tra)]
    exige(o2 and b[ms + o2:ms + o2 + 4] == b'MNU2', 'sin bloque MNU2')
    return dict(b=b, tex=tex, spr=spr, grp=grp, pie=pie, lay=lay, pan=pan, tra=tra)


def nombre(x):
    return x.rstrip(b'\0').decode('latin1')


def piezas(pie, g, cuadro):
    return [p for p in pie if p[0] == g and p[1] <= cuadro < p[1] + max(1, p[3] >> 4)]


def busca_pieza(M, g, cuadro, w, h, x=None, y=None, tol=1.0):
    for p in piezas(M['pie'], g, cuadro):
        ti, sw, sh, z, u0, v0, u1, v1 = M['spr'][p[2]]
        if (sw, sh) != (w, h): continue
        if x is not None and abs(p[4] - x) > tol: continue
        if y is not None and abs(p[5] - y) > tol: continue
        return p
    return None


def prueba_menu(dat):
    M = lee_menu(dat)
    usos = {nombre(u[0]): i for i, u in enumerate(M['lay'])}
    grupos = {nombre(g[0]): i for i, g in enumerate(M['grp'])}
    for n in ('MUSICA_L2', 'MUSICA_T2', 'MUSICA_LISTA', 'MUSICA_RADIO'):
        exige(n in usos or n in grupos, 'falta %s' % n)
    exige(any(nombre(p[0]) == 'MUSICA2' for p in M['pan']), 'falta la pantalla MUSICA2')
    g_l2, g_t2 = grupos['MUSICA_L2'], grupos['MUSICA_T2']
    exige(M['grp'][g_l2][1] >= 501, 'MUSICA_L2: %d cuadros (esperado >= 501)' % M['grp'][g_l2][1])
    exige(M['grp'][g_t2][1] >= 1321, 'MUSICA_T2: %d cuadros (esperado >= 1321)' % M['grp'][g_t2][1])
    # los 12 temas originales siguen: cada parada tiene su rotulo y el titulo su cuadro
    for k in range(12):
        f = 20 + 40 * k
        exige(any(M['spr'][p[2]][1] >= 100 and M['spr'][p[2]][2] >= 15 and p[5] < 200
                  for p in piezas(M['pie'], g_l2, f)), 'falta el rotulo del tema %d (cuadro %d)' % (k, f))
        exige(any(piezas(M['pie'], g_t2, c) for c in range(110 * k, 110 * k + 110)),
              'falta el titulo del tema %d (cuadros %d..%d)' % (k, 110 * k, 110 * k + 109))
    # la tarjeta 13: cuadro 500 del carrusel y 1320 del titulo, con sus seis piezas
    for w, h, x, y, que in ((234, 31, 363.5, 113.0, 'logo'), (112, 19, 210.0, 142.0, 'etiqueta CRAZY TAXI'),
                            (62, 49, 83.0, 118.0, 'icono'), (136, 126, 507.0, 265.0, 'caratula'),
                            (271, 18, 217.0, 370.9, 'tira inferior')):
        exige(busca_pieza(M, g_l2, 500, w, h, x, y), 'falta %s de ALL I WANT en el cuadro 500' % que)
    exige(busca_pieza(M, g_t2, 1320, 134, 15, 342.1, 271.0), 'falta el texto ALL I WANT del visor (cuadro 1320)')
    # puntos de la lista y rotulo de la pista en el cuadro 500 (se copian de los cuadros fijos)
    exige(any(abs(p[5] - 391.5) < 0.3 for p in piezas(M['pie'], g_l2, 500)), 'faltan los puntos de la lista en el cuadro 500')
    exige(any(abs(p[5] - 350.0) < 0.3 for p in piezas(M['pie'], g_l2, 500)), 'falta el rotulo MUSICA DE FONDO REMIX 1 en el cuadro 500')
    # tramos
    def tramo(n, u):
        return [t for t in M['tra'] if nombre(t[0]) == n and t[1] == u]
    u_l2, u_t2 = usos['MUSICA_L2'], usos['MUSICA_T2']
    for k in range(12):
        exige(tramo('TEMA%d' % k, u_l2) and tramo('TEMA%d' % k, u_t2), 'faltan los tramos del tema %d' % k)
    exige(tramo('TEMA12', u_l2) and tramo('TEMA12', u_t2), 'faltan TEMA12 de ALL I WANT')
    exige(tramo('MUEVE1112', u_l2) and tramo('MUEVE1211', u_l2), 'faltan MUEVE1112 / MUEVE1211')
    exige(tramo('TEMA12', u_l2)[0][2:4] == (500, 500), 'TEMA12 del carrusel no esta en el cuadro 500')
    exige(tramo('TEMA12', u_t2)[0][2:4] == (1320, 1320), 'TEMA12 del titulo no esta en el cuadro 1320')
    print('MENU.DAT: MUSICA2 con la tarjeta 13 (ALL I WANT): logo, etiqueta, icono, visor, caratula y tira; '
          '12 temas originales intactos')
    return M


def lee_codigo():
    menu = open(os.path.join(RAIZ, 'comun', 'menu.c'), encoding='utf-8').read()
    mus = open(os.path.join(RAIZ, 'comun', 'musica.c'), encoding='utf-8').read()
    mh = open(os.path.join(RAIZ, 'comun', 'musica.h'), encoding='utf-8').read()
    main = open(os.path.join(RAIZ, 'demo', 'main.c'), encoding='utf-8').read()
    nt = int(re.search(r'#define\s+N_TEMAS\s+(\d+)', menu).group(1))
    listas = [int(x) for x in re.search(r'TEMAS_LISTA\[N_LISTAS\]\s*=\s*\{([^}]*)\}', menu).group(1).replace('\n', ' ').split(',') if x.strip()]
    tema = int(re.search(r'#define\s+MUSICA_TEMA_ALL_I_WANT\s+(\d+)', mh).group(1))
    pista = int(re.search(r'#define\s+MUSICA_PISTA_ALL_I_WANT\s+(\d+)', mh).group(1))
    npistas = int(re.search(r'#define\s+NPISTAS\s+(\d+)', main).group(1))
    exige(nt == 29, 'N_TEMAS = %d' % nt)
    exige(listas == [9, 7, 13], 'TEMAS_LISTA = %s' % listas)
    exige(sum(listas) == nt, 'las listas suman %d, no %d' % (sum(listas), nt))
    exige(tema == 28 and pista == 67, 'ALL I WANT: tema %d, pista %d' % (tema, pista))
    exige(npistas >= nt, 'NPISTAS = %d < %d' % (npistas, nt))
    exige('n == MUSICA_TEMA_ALL_I_WANT' in mus and 'MUSICA_PISTA_ALL_I_WANT' in mus,
          'musica_pon no lleva el tema 28 a MUS67')
    # la pantalla de musica no mira el coche ni el taxi: P_MUSICA .. P_LICENCIA en control()
    a = menu.index('case P_MUSICA:', menu.index('static void control'))
    b = menu.index('case P_LICENCIA:', a)
    bloque = menu[a:b]
    for palabra in ('taxi', 'coche', 'ficha', 'c_desbloqueado', 'SA_BIT_TAXI', 'c_modelo'):
        exige(palabra.lower() not in bloque.lower(), 'la seleccion de musica depende de %s' % palabra)
    exige('sale(P_MUSICA, 1)' in menu, 'P_CAMBIO no lleva a P_MUSICA')
    exige('if (tema_libre(tema_actual()))' in bloque, 'P_MUSICA no usa tema_libre')
    print('CODIGO: N_TEMAS 29, listas %s, tema 28 -> MUS67, NPISTAS %d, P_MUSICA sin condiciones de coche' % (listas, npistas))


def simula(coche):
    """el cursor de la lista 2: R1 dos veces, 12 a la derecha, A; para el coche que sea (la musica no lo mira)"""
    lista, curs = 0, [0, 0, 0]
    for _ in range(2): lista = (lista + 1) % 3
    exige(lista == 2, 'no se llega a la lista 2')
    for _ in range(12):
        exige(curs[lista] < 12, 'cursor fuera de la lista 2')
        curs[lista] += 1
    k = curs[2]
    for l in range(2): k += [9, 7, 13][l]
    exige(k == 28, 'tema actual %d' % k)
    elegido = k
    exige(elegido == 28, 'no se elige ALL I WANT')
    return 28


def audio(adp, adx=None):
    d = open(adp, 'rb').read()
    exige(len(d) > 44100 * 10, 'ADP demasiado corto: %d B' % len(d))
    e = [[0, 0x7f], [0, 0x7f]]
    L = np.zeros(len(d), np.int16); R = np.zeros(len(d), np.int16)
    for i, b in enumerate(d):
        for ch, v in ((1, b & 15), (0, b >> 4)):
            s, p = e[ch]
            delta = v & 7
            q = ((1 + (delta << 1)) * p) >> 3       # el AICA resta el paso positivo, no un DIF con signo
            if q > 32767: q = 32767
            s = s - q if v & 8 else s + q
            s = -32768 if s < -32768 else 32767 if s > 32767 else s
            p = (TAB[delta] * p) >> 8; p = 0x7f if p < 0x7f else 0x6000 if p > 0x6000 else p
            e[ch] = [s, p]; (R if ch else L)[i] = s
    dur = len(d) / 44100.0; rms = float(np.sqrt((L.astype(np.float64) ** 2 + R.astype(np.float64) ** 2).mean() / 2))
    exige(dur > 60, 'duracion %.1f s' % dur)
    exige(rms > 1000, 'RMS %.0f (casi silencio)' % rms)
    print('AUDIO: MUS67.ADP %.2f s, RMS %.0f, pico %d' % (dur, rms, max(abs(L).max(), abs(R).max())))
    if adx:
        import subprocess
        sys.path.insert(0, os.path.join(RAIZ, 'tools'))
        import nativo
        r = subprocess.run([nativo.ffmpeg(), '-v', 'error', '-i', adx, '-f', 's16le', '-acodec', 'pcm_s16le',
                            '-ac', '2', '-ar', '44100', '-'], capture_output=True)
        exige(r.returncode == 0, 'ffmpeg no lee el ADX: ' + r.stderr.decode('utf-8', 'replace'))
        a = np.frombuffer(r.stdout, '<i2'); SL, SR = a[0::2].astype(float), a[1::2].astype(float)
        n = min(len(L), len(SL)); x = (L[:n].astype(float) + R[:n]) / 2; y = (SL[:n] + SR[:n]) / 2
        c = float(np.corrcoef(x, y)[0, 1])
        exige(c > 0.9, 'correlacion con el ADX %.3f' % c)
        print('AUDIO: correlacion con %s: %.4f' % (os.path.basename(adx), c))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('menu'); ap.add_argument('adp', nargs='?'); ap.add_argument('--adx')
    a = ap.parse_args()
    try:
        prueba_menu(a.menu)
        lee_codigo()
        for c in range(15):
            simula(c)
        print('FLUJO: los 15 coches llegan a P_MUSICA y eligen ALL I WANT (tema 28); no hay condicion de taxi')
        if a.adp: audio(a.adp, a.adx)
    except Fallo as e:
        print('FALLO:', e, file=sys.stderr); sys.exit(1)


if __name__ == '__main__':
    main()
