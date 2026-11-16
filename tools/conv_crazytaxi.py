#!/usr/bin/env python3
"""conv_crazytaxi.py: el taxi de Axel de Crazy Taxi (Dreamcast) como COCHEnn.DAT del port (ORD7).

Uso:
  conv_crazytaxi.py [--piezas dir] [--salida dir] [--param PARAM.DAT] [--sin-fisica] [--sin-vista]

Entrada (la saca _trabajo/crazytaxi_lee.py): taxi_cuerpo.npz, taxi_rueda_0..3.npz y tex_NNN.png.
Salida (por defecto _trabajo/coche_taxi):
  COCHE15.DAT     modelo ORD7: cuerpo (pieza 0) + 4 ruedas (piezas 8..11), con bloque CEX2 minimo y ANI1
                  (pivotes de rueda y radio 3,123 u). Texturas en VQ como los coches.
  COCHE15.png     vista de comprobacion (tres cuartos y perfil, con texturas)
  COCHE15.txt     resumen: mallas, vertices, texturas, tamano y comprobaciones
  PARAM_TAXI.BIN  columna de fisica propuesta (200 floats, lista para pegar en PARAM.DAT)
  PARAM_TAXI.DAT  copia de PARAM.DAT con la columna 0 (ficha 7, el Ferrari menos potente) cambiada por la del taxi
  PARAM_TAXI.txt  de donde sale cada valor

Escala y ejes: 1 unidad Naomi = 0,10625 m; el modelo Naomi mira a +z y el port a -z, asi que se gira 180 grados
en y (x -> -x, z -> -z): es una rotacion, no un espejo, y las texturas quedan bien. Las ruedas van como modelos
aparte con el pivote de la tabla de cabs (el origen de cada rueda es su buje; no se recentra en x).

Fisica: rendimiento del Ferrari mas potente de PARAM.DAT (columna 7 = modelo 8 = ficha 4 = COCHE04, el F40:
par x1,10, agarre 4,8/5,0, 0-100 en 4,05 s, punta 300,6 km/h de velocimetro) y geometria del taxi (batalla 3,29 m,
vias 1,47/1,38 m, radio 0,332 m, alt_cg 0,35 m, radio de giro 6,0 m). La masa se queda en 1290 kg (la del F40)
para conservar la aceleracion pedida; el informe del taxi proponia 1800 kg (se pierde aceleracion).

Solo lee del disco y escribe en _trabajo: no toca el codigo del juego, ni datos_r44, ni el GDI definitivo.
"""
import argparse, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import modelo
import vq

RAIZ = os.path.dirname(os.path.dirname(TOOLS))
DEF_PIEZAS = os.path.join(RAIZ, '_trabajo', 'crazytaxi')
DEF_SALIDA = os.path.join(RAIZ, '_trabajo', 'coche_taxi')
DEF_PARAM = os.path.join(RAIZ, '_trabajo', 'datos', 'PARAM.DAT')
ESCALA = 0.10625
PIVOTES = [(6.90, 2.84, 15.50), (-6.90, 2.84, 15.50), (6.50, 2.84, -15.50), (-6.50, 2.84, -15.50)]
RADIO_RUEDA = 3.123
# Tiras del volante dentro del cuerpo (taxi_cuerpo.npz): aro 252/253/254 (sintex), centro 173..176.
# Se sacan del cuerpo y se dibujan como pieza 3 (ANI1) para girarlas con la direccion.
TIRAS_VOLANTE = (173, 174, 175, 176, 252, 253, 254)
# Luces del taxi: tiras del modelo original (POLDC0 0x38E8, taxi_cuerpo.npz).  El registro 23
# (textura 106) tiene los dos pilotos del paragolpes trasero; los registros 35/36 (textura 20), los
# faros delanteros.  La posicion se lee de sus vertices: no hay coordenadas inventadas.
REC_PILOTOS = 23
TIRAS_PILOTOS = (232, 233)
REC_FAROS = (35, 36)
# Cada faro son dos lamparas redondas; las tiras de cada una salen del modelo original (por su x):
# registro 35 -> exterior (255,257,258) e interior (256,259,260); registro 36 -> exterior
# (262,263,264) e interior (261,265,266).  La caja de cada grupo da el rectangulo de esa lampara.
FAROS_TIRAS = {35: ((255, 257, 258), (256, 259, 260)),
               36: ((262, 263, 264), (261, 265, 266))}
LUCES_INFO = []
COCHE_RAM = 424 << 10
LUZ = np.array([0.3, 0.8, 0.5]) / np.linalg.norm([0.3, 0.8, 0.5])


def carga(nombre, dirpiezas):
    z = np.load(os.path.join(dirpiezas, nombre + '.npz'))
    return dict(tex=z['rec_tex'], lista=z['rec_lista'], spec=z['rec_spec'], amb=z['rec_amb'],
                face=z['rec_face'], offc=z['rec_offc'], verts=z['verts'], strip_off=z['strip_off'],
                strip_n=z['strip_n'], strip_g=z['strip_g'], strip_rec=z['strip_rec'],
                tsp=z['rec_tsp'] if 'rec_tsp' in z.files else np.zeros(len(z['rec_tex']), np.uint32))


def uv_y_color(mod, escala, giro_y=True):
    """vertices de una pieza en el marco del port (metros): gira 180 grados en y y calcula el color por vertice"""
    V = mod['verts'].astype(np.float64).copy()
    V[:, :3] *= escala
    if giro_y:
        V[:, 0] *= -1.0
        V[:, 2] *= -1.0
    N = V[:, 3:6].copy()
    if giro_y:
        N[:, 0] *= -1.0
        N[:, 2] *= -1.0
    N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
    # mascara de cada record (para el color y para el volteo U/V del TSP)
    mascaras = [np.zeros(len(V), bool) for _ in range(len(mod['tex']))]
    for s in range(len(mod['strip_off'])):
        ri = int(mod['strip_rec'][s])
        mascaras[ri][mod['strip_off'][s]:mod['strip_off'][s] + mod['strip_n'][s]] = True
    # TSP: bit 18 = volteo U, bit 16 = volteo V.  El PVR lo hace con muestreo espejo:
    # la UV se niega y el wrap (repeat) la refleja.  Sin esto, las tiras del lado izquierdo
    # del maletero (u = 1,2..1,85, TSP con U flip) caen en otra zona de la textura.
    # Las UV del modelo se usan tal cual: el registro 6 declara U-flip en el TSP, pero el
    # atlas 133 ya esta en el sentido que dibuja el coche; negarlas movia la librea y el
    # triangulo gris de la esquina del maletero.  Ese triangulo se corrige aparte, con un
    # parche amarillo sobre la tira 131 (mismo triangulo, texto aparte).
    # El panel trasero (textura 133: cromados y pilotos) se mete 2 mm hacia dentro: su borde
    # superior es coplanario con el borde amarillo del maletero (textura 127) y en la
    # PowerVR asomaba por una esquina (captura del usuario: falta un trozo de amarillo).
    for s in range(len(mod['strip_off'])):
        ri = int(mod['strip_rec'][s])
        if int(mod['tex'][ri]) in (133, 134):
            a = mod['strip_off'][s]; b = a + mod['strip_n'][s]
            if int(mod['tex'][ri]) == 133:
                V[a:b, 2] += 0.002  # el maletero gana el empate de profundidad (Camera en +z)
                V[a:b, 1] += 0.001
            else:
                V[a:b, 2] -= 0.002
                V[a:b, 1] -= 0.006  # el panel/piloto trasero asomaba por una esquina

    luz = 0.7 + 0.3 * np.clip(np.abs(N @ LUZ), 0, 1)
    col = np.empty((len(V), 4), np.float64)
    for ri in range(len(mod['tex'])):
        m = mascaras[ri]
        if not m.any():
            continue
        f = mod['face'][ri]
        if int(mod['tex'][ri]) >= 0:
            col[m, 0:3] = luz[m, None] * 255.0
            col[m, 3] = f[0] * 255.0
        else:
            col[m, 0:3] = f[1:4][None, :] * luz[m, None] * 255.0
            col[m, 3] = f[0] * 255.0
    col = np.clip(np.rint(col), 0, 255).astype(np.uint32)
    return V, col


def ordena_tiras(mod, V, col, excluir=(), excluir_tiras=(), solo_tiras=()):
    """devuelve [(lista, textura, i0, n, posiciones, colores)] con las listas de triangulos convertidas a tiras"""
    salida = []
    for ri in range(len(mod['tex'])):
        if ri in excluir:
            continue
        li = 2 if int(mod['lista'][ri]) == 2 else 0
        ti = int(mod['tex'][ri])
        for s in range(len(mod['strip_off'])):
            if int(mod['strip_rec'][s]) != ri or s in excluir_tiras:
                continue
            if solo_tiras and s not in solo_tiras:
                continue
            a, n, g = int(mod['strip_off'][s]), int(mod['strip_n'][s]), int(mod['strip_g'][s])
            P = V[a:a + n]
            C = col[a:a + n]




            if n < 3:
                continue
            if g & 8:                                  # lista de triangulos: a tiras con degenerados
                # T0 + por cada Ti: [ultimo de T(i-1), primero de Ti] + Ti
                q = [P[0], P[1], P[2]]
                qc = [C[0], C[1], C[2]]
                for j in range(3, n, 3):
                    q += [q[-1], P[j], P[j], P[j + 1], P[j + 2]]
                    qc += [qc[-1], C[j], C[j], C[j + 1], C[j + 2]]
                P = np.array(q)
                C = np.array(qc)
            salida.append((li, ti, P, C))
    return salida


def teselas(piezas, dirpiezas):
    """texturas usadas -> indice en la lista del modelo; carga los PNG y decide formato/lista"""
    usadas = {}
    for mod in piezas:
        for ri, t in enumerate(mod['tex']):
            t = int(t)
            if t < 0:
                continue
            usadas[t] = max(usadas.get(t, 0), 2 if int(mod['lista'][ri]) == 2 else 0)
    return usadas


def _placa_ark_retro():
    """Placa trasera 64x32: fondo claro, borde negro y ARK-RETRO legible."""
    a = Image.new('RGBA', (64, 32), (242, 242, 242, 255))
    d = ImageDraw.Draw(a)
    d.rectangle([0, 0, 63, 31], outline=(20, 20, 20, 255), width=2)
    d.rectangle([2, 2, 61, 3], fill=(28, 58, 145, 255))
    d.rectangle([2, 28, 61, 29], fill=(185, 30, 30, 255))
    txt = 'ARK-RETRO'
    font = None
    for ruta in ('C:/Windows/Fonts/arialbd.ttf', 'C:/Windows/Fonts/arial.ttf',
                 '/mnt/c/Windows/Fonts/arialbd.ttf', '/mnt/c/Windows/Fonts/arial.ttf'):
        try:
            for sz in range(9, 3, -1):
                f = ImageFont.truetype(ruta, sz)
                bb = d.textbbox((0, 0), txt, font=f)
                if bb[2] - bb[0] <= 50:
                    font = f
                    break
            if font is not None:
                break
        except Exception:
            pass
    if font is None:
        font = ImageFont.load_default()
    bb = d.textbbox((0, 0), txt, font=font)
    x = (64 - (bb[2] - bb[0])) // 2 - bb[0]
    y = (28 - (bb[3] - bb[1])) // 2 - bb[1]
    d.text((x, y), txt, font=font, fill=(15, 15, 45, 255))
    return np.asarray(a, np.float64).astype(np.uint8)


def construye(piezas, dirpiezas, piloto=True):
    mod = modelo.Modelo()
    mod.ruta = np.zeros((0, 5))
    mod.vq2 = None                      # como los coches: sin la segunda pasada de la VQ
    usadas = teselas(piezas, dirpiezas)
    mapa_tex = {}
    for t in sorted(usadas):
        a = np.asarray(Image.open(os.path.join(dirpiezas, 'tex_%03d.png' % t)).convert('RGBA'), np.float64).astype(np.uint8)
        if t == 133:
            # El atlas 133 es la chapa del maletero; sus UV originales mezclan el cromado y
            # los pilotos con el amarillo y salen la linea negra torcida y el rotulo.  El
            # usuario pide amarillo continuo: se sustituye SOLO esta textura por amarillo
            # liso.  La geometria, UV y el resto de materiales no se tocan; los pilotos,
            # paragolpes y parrilla son de otros records (129/134/3).
            a = a.copy()
            a[:, :, 0] = 255
            a[:, :, 1] = 246
            a[:, :, 2] = 16
            a[:, :, 3] = 255
        fmt, li, datos = modelo.textura_dc(a)
        li = usadas[t]
        if li == 2 and fmt == 1:        # en la lista transparente, aunque el alfa sea casi constante
            fmt = modelo.formato_mezcla(a)
            datos = modelo.codifica16(a, fmt)
        elif li == 1:
            fmt = 0
            datos = modelo.codifica16(a, fmt, 128)
        off = len(mod.blob)
        mod.texs.append((a.shape[1], a.shape[0], fmt, li, off, len(datos)))
        mod.blob += datos
        mod.rgba.append(a)
        mapa_tex[t] = len(mod.texs) - 1
    # sombra plana de contacto (pieza 12, PZ_SOMBRA): una textura suave y un cuadrilatero en el
    # suelo del coche, centrado en la caja del cuerpo.  No se usa SVL1: el volumen proyectado por
    # el sol alargaba la sombra fuera del coche (queja del usuario).
    ss = (np.mgrid[0:64, 0:128].astype(np.float64))
    sx = (ss[1] + 0.5) / 128.0 * 2.0 - 1.0
    sz = (ss[0] + 0.5) / 64.0 * 2.0 - 1.0
    d = (np.abs(sx) ** 4 + np.abs(sz) ** 4) ** 0.25
    al = np.clip(1.0 - d, 0.0, 1.0)
    al = (al * al * (3.0 - 2.0 * al) * 118.0).astype(np.uint8)
    sombra = np.zeros((64, 128, 4), np.uint8)
    sombra[:, :, 3] = al
    fmt_s, li_s, datos_s = modelo.textura_dc(sombra)
    off_s = len(mod.blob)
    mod.texs.append((128, 64, fmt_s, 2, off_s, len(datos_s)))
    mod.blob += datos_s
    mod.rgba.append(sombra)
    sombra_tex = len(mod.texs) - 1
    # La textura 31 la comparten la placa delantera (registro 29) y la trasera
    # (registro 30).  Se duplica solo para la trasera con ARK-RETRO; la delantera
    # se queda como estaba.
    placa = _placa_ark_retro()
    fmt_p, li_p, datos_p = modelo.textura_dc(placa)
    off_p = len(mod.blob)
    mod.texs.append((placa.shape[1], placa.shape[0], fmt_p, 0, off_p, len(datos_p)))
    mod.blob += datos_p
    mod.rgba.append(placa)
    placa_tex = len(mod.texs) - 1
    # mallas y vertices: cuerpo (pieza 0) y ruedas (8..11), cada una en su marco
    Vc, Cc = uv_y_color(piezas[0], ESCALA)
    # tira 128 (record 6, textura 133): la esquina trasera izquierda del maletero.  Su UV
    # (u 1,635..1,851) cae en los cromados/pilotos del atlas, no en el amarillo; se fuerza
    # el amarillo liso de la misma textura (en ordena_tiras) en vez de dejarla con el gris.
    tiras = ordena_tiras(piezas[0], Vc, Cc, excluir_tiras=TIRAS_VOLANTE)
    # el volante (aro + centro) va aparte, como la pieza 3, con su pivote en ANI1; main.c lo gira con la direccion
    for li, ti, P, C in ordena_tiras(piezas[0], Vc, Cc, solo_tiras=TIRAS_VOLANTE):
        tiras.append((li, ti, P, C, 3))
    nodos = []
    for k in range(4):
        Vk, Ck = uv_y_color(piezas[1 + k], ESCALA)
        for li, ti, P, C in ordena_tiras(piezas[1 + k], Vk, Ck):
            tiras.append((li, ti, P, C, 8 + k))
    # cuerpo primero, luego ruedas (el orden final lo pone escribe por lista y textura)
    for x in tiras:
        li, ti, P, C = x[0], x[1], x[2], x[3]
        nodo = 0 if len(x) == 4 else (x[4] << 8)
        cc = P[:, :3].mean(0)
        rad = float(np.sqrt(((P[:, :3] - cc) ** 2).sum(1).max()))
        n = len(P)
        m = np.zeros(n, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
        m['x'], m['y'], m['z'] = P[:, 0], P[:, 1], P[:, 2]
        m['u'], m['v'] = P[:, 6], P[:, 7]
        m['c'] = (C[:, 3] << 24) | (C[:, 0] << 16) | (C[:, 1] << 8) | C[:, 2]
        mod.verts.append(m)
        tex_m = mapa_tex.get(ti, -1)
        if ti == 31 and cc[2] > 0.0:                 # placa trasera: ARK-RETRO
            tex_m = placa_tex
            # La tira original trae las UV giradas; se rehace el quad limpio
            # (orden Z y UV 0..1) para que el texto se lea derecho.
            x0 = float(m['x'].min()); x1 = float(m['x'].max())
            y0 = float(m['y'].min()); y1 = float(m['y'].max()); z0 = float(m['z'].mean())
            m['x'] = [x0, x1, x0, x1]; m['y'] = [y0, y0, y1, y1]; m['z'] = z0
            m['u'] = [0.0, 1.0, 0.0, 1.0]; m['v'] = [1.0, 1.0, 0.0, 0.0]
            m['c'] = 0xFFFFFFFF
            cc = np.array([(x0 + x1) * 0.5, (y0 + y1) * 0.5, z0], np.float32)
            rad = float(np.sqrt(((np.stack([m['x'], m['y'], m['z']], 1) - cc) ** 2).sum(1).max()))
        mod.mallas.append([li, tex_m, cc, rad, mod.nv, n, None, int(nodo)])
        mod.nv += n
    # piloto (Axel, personaje 0x3B): estatico, sentado, en la pieza 0 (carroceria); ver tools/conv_axel.py
    pil = (0, 0, 0)
    if piloto:
        try:
            import conv_axel
            pil = conv_axel.anade(mod, dirpiezas, ESCALA)
        except Exception as ex:
            print('conv_crazytaxi: sin piloto: %s' % ex)
    # Parche del maletero: el triangulo (914, 915, 916) de la tira 131 (registro 6) tiene
    # el vertice 915 con UV u=1,775, que cae en el cromado gris del atlas 133; en la
    # PowerVR se ve como un triangulo sin amarillo (circulo del usuario).  Se duplica ese
    # mismo triangulo, 1-2 mm por fuera, con UV de amarillo liso y color blanco.  Va
    # despues de la carroceria (orden de origen) para que tape el original.
    # sombra plana de contacto y luces: caja real del cuerpo 5,715 x 2,121 m, centrada en
    # z = +0,331 (el cuerpo va de -2,527 a +3,189); va en la pieza 12 y la dibuja sombra_dibuja
    SV = np.zeros(4, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
    SV['x'] = [-1.0607, -1.0607, 1.0607, 1.0607]
    SV['z'] = [-2.5267, 3.1887, 3.1887, -2.5267]
    SV['y'] = 0.0
    SV['u'] = [0.0, 0.0, 1.0, 1.0]
    SV['v'] = [0.0, 1.0, 1.0, 0.0]
    SV['c'] = 0xFFFFFFFF
    mod.verts.append(SV)
    mod.mallas.append([2, sombra_tex, np.array([0.0, 0.0, 0.331]), 3.2, mod.nv, 4, None, 12 << 8])
    mod.nv += 4
    # orden del modelo original: el motor de destino ordena por lista y textura, pero eso
    # cambia el orden de dibujo de las tiras coplanarias (maletero/panel).  Se conserva el
    # orden de records/tiras de origen dentro de cada lista (modelo.Modelo lo respeta con
    # orden_original).
    mod.orden_original = True
    mod.piloto = pil
    mod.orden = {id(m): i for i, m in enumerate(mod.mallas)}
    mod.mallas.sort(key=lambda x: (x[0], mod.orden[id(x)]))
    return mod, mapa_tex


def _textura_luz(mod, rgb):
    """textura 8x8 de un color, en la lista transparente y aditiva (0x400)"""
    a = np.zeros((8, 8, 4), np.uint8)
    a[:, :, 0], a[:, :, 1], a[:, :, 2], a[:, :, 3] = rgb[0], rgb[1], rgb[2], 255
    fmt, li, datos = modelo.textura_dc(a)
    li = 2
    fmt |= 0x400                                  # aditiva (coche_luces / compila_cab)
    off = len(mod.blob)
    mod.texs.append((8, 8, fmt, 2, off, len(datos)))
    mod.blob += datos
    mod.rgba.append(a)
    return len(mod.texs) - 1


def _tira_port(cuerpo, s):
    """vertices (x,y,z) de una tira de taxi_cuerpo en el marco del port: escala y giro 180 en y,
    los mismos que uv_y_color usa para el cuerpo."""
    a, n = int(cuerpo['strip_off'][s]), int(cuerpo['strip_n'][s])
    P = cuerpo['verts'][a:a + n, :3].astype(np.float64).copy()
    P[:, 0] *= -1.0
    P[:, 2] *= -1.0
    return P * ESCALA


def _textura_copia(mod, rgba):
    """copia una textura del modelo en la lista transparente y aditiva (0x400), para las luces"""
    a = np.asarray(rgba, np.uint8)
    fmt, li, datos = modelo.textura_dc(a)
    fmt |= 0x400
    off = len(mod.blob)
    mod.texs.append((a.shape[1], a.shape[0], fmt, 2, off, len(datos)))
    mod.blob += datos
    mod.rgba.append(a)
    return len(mod.texs) - 1


def luces_taxi(mod, intensidad=0.80, cuerpo=None, mapa_tex=None):
    """Luces del taxi en el CEX2: solo freno (grupo 3) y faros (grupo 4).

    La posicion NO se inventa: sale de las tiras del modelo original de Crazy Taxi (POLDC0 0x38E8,
    taxi_cuerpo.npz).  El runtime (demo/main.c, coche_luces) enciende y apaga mallas por el par
    (malla, grupo) del CEX2; en los COCHEnn.DAT originales el grupo 3 es una copia de las tiras de
    luz del propio coche en su sitio (COCHE00: mallas 727/728 del grupo 3 con el mismo centro que
    las 572/573 del grupo 1).  El taxi hace lo mismo con sus tiras:
      - registro 23 (textura 106): tiras 232 y 233, los dos pilotos del paragolpes trasero;
      - registros 35 y 36 (textura 20): dos lamparas redondas por lado (6 tiras por registro).

    Devuelve [(malla, grupo)] para bloque_ani.  Si cuerpo es None se lee taxi_cuerpo.npz del
    directorio por defecto (para los scripts de vista, que no pasan las piezas).  mapa_tex es el
    mapa textura original -> indice del modelo; con el se usa la textura 20 (lente del faro) en la
    copia aditiva; sin el, un color blanco liso."""
    if cuerpo is None:
        cuerpo = carga('taxi_cuerpo', DEF_PIEZAS)
    roja = _textura_luz(mod, (255, 24, 24))
    blanca = _textura_luz(mod, (255, 245, 200))
    LUCES_INFO[:] = []

    def pon_quad(P, tex, col, grupo, fuente, dz=0.0):
        """copia una tira de 4 vertices (orden de la tira original) y la separa dz en z.
        Devuelve la malla y los vertices finales."""
        Q = np.asarray(P, np.float64).copy()
        Q[:, 2] += dz
        V = np.zeros(4, dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
        # mismo orden que la tira original: (x0,y0) (x1,y0) (x0,y1) (x1,y1); UV 0..1
        V['x'] = Q[:, 0]; V['y'] = Q[:, 1]; V['z'] = Q[:, 2]
        V['u'] = [0.0, 1.0, 0.0, 1.0]; V['v'] = [0.0, 0.0, 1.0, 1.0]
        V['c'] = col
        cc = Q.mean(0)
        rad = float(np.sqrt(((Q - cc) ** 2).sum(1).max()))
        m = [2, tex, cc.astype(np.float32), rad, mod.nv, 4, None, 0]
        mod.verts.append(V); mod.mallas.append(m); mod.nv += 4
        if getattr(mod, 'orden', None) is not None:
            mod.orden[id(m)] = len(mod.orden)
        return m, Q

    def c(a, r, g, b):
        return (a << 24) | (int(r * 255) << 16) | (int(g * 255) << 8) | int(b * 255)

    quads = []
    # pilotos traseros: tiras 232/233 del registro 23, con sus vertices Naomi girados y escalados igual
    # que el cuerpo.  Se copian las 4 esquinas reales (no una caja inventada): el rectangulo queda
    # inclinado como la lente (18,4 mm de fondo) y mide 0,1320 x 0,0526 m en proyeccion trasera.
    for s in TIRAS_PILOTOS:
        ri = int(cuerpo['strip_rec'][s])
        if ri != REC_PILOTOS:
            raise SystemExit('tira %d: se esperaba el registro %d, es el %d' % (s, REC_PILOTOS, ri))
        a, n = int(cuerpo['strip_off'][s]), int(cuerpo['strip_n'][s])
        if n != 4:
            raise SystemExit('tira %d: se esperaban 4 vertices, tiene %d' % (s, n))
        P = _tira_port(cuerpo, s)
        fuente = 'registro %d (textura %d), tira %d' % (ri, int(cuerpo['tex'][ri]), s)
        m, Q = pon_quad(P, roja, c(0xFF, 1.00, 0.90, 0.90), 3, fuente, dz=0.002)
        quads.append((m, 3))
        LUCES_INFO.append(dict(grupo=3, fuente=fuente, rec=ri, tiras=[s], naomi=cuerpo['verts'][a:a+n, :3].astype(np.float64),
                               port=P, quad=Q, tex=int(cuerpo['tex'][ri]),
                               x0=float(P[:, 0].min()), x1=float(P[:, 0].max()),
                               y0=float(P[:, 1].min()), y1=float(P[:, 1].max()),
                               z0=float(P[:, 2].min()), z1=float(P[:, 2].max()), z=float(Q[:, 2].mean())))
    # faros: dos lamparas redondas por lado (registros 35/36).  Cada lampara usa la caja real de sus
    # tiras y un rectangulo 2 mm por delante del faro (port -z); con la textura 20 original, aditiva.
    col_faro = c(0xFF, 1.0 * intensidad, 0.96 * intensidad, 0.78 * intensidad)
    tex_faro = blanca
    if mapa_tex and 20 in mapa_tex:
        tex_faro = _textura_copia(mod, mod.rgba[mapa_tex[20]])
    for ri in REC_FAROS:
        for ss in FAROS_TIRAS[ri]:
            ss = np.asarray(ss, int)
            P = np.vstack([_tira_port(cuerpo, s) for s in ss])
            x0, x1 = float(P[:, 0].min()), float(P[:, 0].max())
            y0, y1 = float(P[:, 1].min()), float(P[:, 1].max())
            z = float(P[:, 2].min()) - 0.002       # hacia delante en el port (-z)
            fuente = 'registro %d (textura %d), tiras %s (lampara)' % (ri, int(cuerpo['tex'][ri]), ','.join(str(int(s)) for s in ss))
            Q = np.array([[x0, y0, z], [x1, y0, z], [x0, y1, z], [x1, y1, z]], np.float64)
            m, Q = pon_quad(Q, tex_faro, col_faro, 4, fuente)
            quads.append((m, 4))
            LUCES_INFO.append(dict(grupo=4, fuente=fuente, rec=ri, tiras=[int(s) for s in ss],
                                   naomi=np.array([v[:3] for s in ss for v in cuerpo['verts'][int(cuerpo['strip_off'][s]):int(cuerpo['strip_off'][s]) + int(cuerpo['strip_n'][s])]], np.float64),
                                   port=P, quad=Q, tex=int(cuerpo['tex'][ri]),
                                   x0=x0, x1=x1, y0=y0, y1=y1, z0=z, z1=z, z=z))
    return quads


def informe_luces():
    """lineas del informe: de que registro/tira sale cada luz y la correspondencia vertices -> rectangulo"""
    L = []
    for x in LUCES_INFO:
        que = 'freno' if x['grupo'] == 3 else 'faros'
        L.append('%s (grupo %d): %s' % (que, x['grupo'], x['fuente']))
        L.append('  Naomi: %s' % '  '.join('(%.6f %.6f %.6f)' % tuple(v) for v in x['naomi']))
        L.append('  port : %s' % '  '.join('(%.6f %.6f %.6f)' % tuple(v) for v in x['port']))
        L.append('  rect : x %.6f..%.6f  y %.6f..%.6f  z %.6f..%.6f (fondo %.6f)  ancho %.6f  alto %.6f' %
                 (x['x0'], x['x1'], x['y0'], x['y1'], x['z0'], x['z1'], x['z1'] - x['z0'],
                  x['x1'] - x['x0'], x['y1'] - x['y0']))
        L.append('  quad : %s' % '  '.join('(%.6f %.6f %.6f)' % tuple(v) for v in x['quad']))
    return L


def _hull_volumen(P, tris):
    """un volumen convexo (vertices, aristas con normal) de los triangulos dados"""
    from scipy.spatial import ConvexHull
    if len(P) < 4:
        return None
    try:
        H = ConvexHull(P)
    except Exception:
        return None
    usa = sorted(set(H.simplices.ravel()))
    nuevo = {v: i for i, v in enumerate(usa)}
    aris = []
    for t, eq in zip(H.simplices, H.equations):
        a, c, d = (nuevo[v] for v in t)
        n = eq[:3]
        if np.dot(np.cross(P[t[1]] - P[t[0]], P[t[2]] - P[t[0]]), n) < 0:
            c, d = d, c
        for x, y in ((a, c), (c, d), (d, a)):
            for e in aris:
                if e[0] == y and e[1] == x and e[3] is None:
                    e[3] = n
                    break
            else:
                aris.append([x, y, n, None])
    return P[usa].astype(np.float32), aris


def volumenes_sombra(mod, pz):
    """SVL1 del taxi: trozos convexos de la carroceria por franjas de z + una envolvente por rueda.
    La PowerVR cuenta par/impar: la union de convexos da la sombra del coche sin dobleces."""
    vols = []
    # vertices en un solo array: mod.mallas esta ordenado por lista/textura y mod.verts no,
    # asi que hay que usar el i0 de cada malla, no el indice de la malla
    G = np.concatenate([np.stack([v['x'], v['y'], v['z']], 1) for v in mod.verts]).astype(np.float64)
    def vertices_malla(m):
        i0, n = m[4], m[5]
        return G[i0:i0 + n]
    # franjas de z; los triangulos se reconstruyen por malla (las mallas ya son tiras)
    tri = []
    for m in mod.mallas:
        li, ti, cc, rad, i0, n, _, nodo = m
        if nodo != 0:
            continue
        Q = vertices_malla(m)
        nv = len(Q)
        for j in range(nv - 2):
            if j & 1:
                tri.append((Q[j + 1], Q[j], Q[j + 2]))
            else:
                tri.append((Q[j], Q[j + 1], Q[j + 2]))
    for z0, z1 in ((-2.6, -1.5), (-1.5, -0.7), (-0.7, 0.1), (0.1, 0.9), (0.9, 1.9), (1.9, 3.3)):
        vs = set()
        for t in tri:
            zc = (t[0][2] + t[1][2] + t[2][2]) / 3.0
            if z0 <= zc <= z1:
                for p in t:
                    vs.add(tuple(np.round(p, 4)))
        if len(vs) < 4:
            continue
        V = np.array(sorted(vs), np.float64)
        h = _hull_volumen(V, [])
        if h:
            vols.append(h)
    # una envolvente por rueda (en su pivote)
    for k in range(4):
        Q = []
        for m in mod.mallas:
            if m[7] != ((8 + k) << 8):
                continue
            Q.append(vertices_malla(m))
        if not Q:
            continue
        Q = np.vstack(Q) + pz[8 + k, :3].astype(np.float64)
        h = _hull_volumen(Q, [])
        if h:
            vols.append(h)
    return vols


def bloque_svl(vols):
    """SVL1: 'SVL1', u32 volumenes; por volumen u32 vertices, u32 aristas, vertices (3 f32) y
    aristas {u16 a, u16 b, s8 n1x,y,z, u8 caras, s8 n2x,y,z, 0} (como conv_coche.bloque_svl)"""
    b = [b'SVL1', struct.pack('<I', len(vols))]
    for P, aris in vols:
        b.append(struct.pack('<2I', len(P), len(aris)))
        b.append(P.astype('<f4').tobytes())
        for a, c, n1, n2 in aris:
            q1 = np.clip(np.round(n1 * 127), -127, 127).astype(np.int8)
            q2 = np.clip(np.round((n2 if n2 is not None else np.zeros(3)) * 127), -127, 127).astype(np.int8)
            b.append(struct.pack('<2H3bB3bx', a, c, *q1.tolist(), 1 if n2 is None else 2, *q2.tolist()))
    return b''.join(b)


def bloque_ani(radio, base=0.0, svl=b'', grupos=(), piloto=True):
    """CEX2 minimo + grupos de luces + SVL1 + ANI1 con las 4 ruedas.  Los grupos son pares
    (indice de malla, grupo): 1 pilotos, 2 noche, 3 freno, 4 faros (coche_luces de main.c).
    piloto: el cuerpo (pieza 1) y la cabeza (pieza 2) de Axel con sus pivotes (cadera y cuello)."""
    pz = np.zeros((12, 5), np.float32)
    for k, (x, y, z) in enumerate(PIVOTES):
        # rotacion 180 en y: x y z cambian de signo; la y se queda
        pz[8 + k] = (-x * ESCALA, y * ESCALA, -z * ESCALA, 0.0, 0.0)
    if piloto:
        try:
            import conv_axel
            cadera, cuello = conv_axel.pivotes()
            pz[1] = (cadera[0], cadera[1], cadera[2], 0.0, 0.0)      # PZ_VOLANTE: cuerpo
            pz[2] = (cuello[0], cuello[1], cuello[2], 0.0, 0.0)      # PZ_ALERON: cabeza
        except Exception as ex:
            print('conv_crazytaxi: sin pivotes del piloto: %s' % ex)
    try:
        import conv_axel
        centro, eje = conv_axel.pivote_volante()
        pz[3] = (centro[0], centro[1], centro[2], eje[1], eje[2])    # PZ_FAROS: volante (eje x = 0)
    except Exception as ex:
        print('conv_crazytaxi: sin pivote del volante: %s' % ex)
    cab = struct.pack('<I6f', 15, radio, base, 0.0, 0.0, 0.0, 0.0)
    n = (4 + len(cab) + pz.nbytes + 4) // 4
    ani = b'ANI1' + struct.pack('<I', n) + cab + pz.astype('<f4').tobytes()
    grp = b''.join(struct.pack('<2I', int(i), int(g)) for i, g in grupos)
    cex = b'CEX2' + struct.pack('<5I', 8, 0, 0, 0, len(grupos)) + grp + svl + ani
    cex += bytes(-len(cex) % 4)
    return np.frombuffer(cex, '<f4').reshape(-1, 1), pz


# ------------------------------------------------------------------ vista de comprobacion (software, con texturas)
def vista(mod, ruta, ojo, mira, W=1100, H=520):
    def proyecta(P):
        f = np.array(mira, float) - ojo; f /= np.linalg.norm(f)
        r = np.cross(f, [0, 1, 0]); r /= np.linalg.norm(r); u = np.cross(r, f)
        q = P - ojo
        x, y, z = q @ r, q @ u, q @ f
        return np.stack([W / 2 + x / np.maximum(z, 1e-5) * (H / 2) / 0.5625,
                         H / 2 - y / np.maximum(z, 1e-5) * (H / 2) / 0.5625, z], 1)
    img = np.zeros((H, W, 3), np.float32) + np.array([45, 55, 65]) / 255
    zb = np.full((H, W), 1e9, np.float32)
    L = LUZ
    V = np.concatenate(mod.verts) if mod.verts else np.zeros(0, mod.verts[0].dtype if mod.verts else [])
    tex = [np.asarray(a, np.float32) / 255 for a in mod.rgba]
    for pasar in (0, 1):
        for li, ti, cc, rad, i0, n, _, _ in mod.mallas:
            if (li == 2) != (pasar == 1):
                continue
            P = np.stack([V['x'][i0:i0 + n], V['y'][i0:i0 + n], V['z'][i0:i0 + n]], 1).astype(float)
            U = V['u'][i0:i0 + n].astype(float); Vv = V['v'][i0:i0 + n].astype(float)
            C = V['c'][i0:i0 + n]
            t = tex[ti] if ti >= 0 else None
            for j in range(n - 2):
                if j % 2 == 0:
                    Q = P[j:j + 3]; Cq = C[j:j + 3]; Uq = U[j:j + 3]; Vq = Vv[j:j + 3]
                else:
                    idx = [j + 1, j, j + 2]
                    Q = P[idx]; Cq = C[idx]; Uq = U[idx]; Vq = Vv[idx]
                q = proyecta(Q)
                if (q[:, 2] <= 0.05).any():
                    continue
                x0 = max(0, int(np.floor(q[:, 0].min()))); x1 = min(W - 1, int(np.ceil(q[:, 0].max())))
                y0 = max(0, int(np.floor(q[:, 1].min()))); y1 = min(H - 1, int(np.ceil(q[:, 1].max())))
                if x1 < x0 or y1 < y0:
                    continue
                X, Y = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
                x0p, y0p, x1p, y1p, x2p, y2p = q[0, 0], q[0, 1], q[1, 0], q[1, 1], q[2, 0], q[2, 1]
                den = (y1p - y2p) * (x0p - x2p) + (x2p - x1p) * (y0p - y2p)
                if abs(den) < 1e-9:
                    continue
                w0 = ((y1p - y2p) * (X - x2p) + (x2p - x1p) * (Y - y2p)) / den
                w1 = ((y2p - y0p) * (X - x2p) + (x0p - x2p) * (Y - y2p)) / den
                w2 = 1 - w0 - w1
                m = (w0 >= -0.001) & (w1 >= -0.001) & (w2 >= -0.001)
                if not m.any():
                    continue
                z = w0 * q[0, 2] + w1 * q[1, 2] + w2 * q[2, 2]
                yy, xx = np.nonzero(m); zz = z[yy, xx]
                vis = zz < zb[yy + y0, xx + x0]
                if not vis.any():
                    continue
                yy, xx, zz = yy[vis], xx[vis], zz[vis]
                a0, a1, a2 = w0[yy, xx], w1[yy, xx], w2[yy, xx]
                def rgbde(c):
                    return np.array([(c >> 16) & 255, (c >> 8) & 255, c & 255], np.float64) / 255.0
                col = a0[:, None] * rgbde(Cq[0]) + a1[:, None] * rgbde(Cq[1]) + a2[:, None] * rgbde(Cq[2])
                al = np.ones(len(yy), np.float32)
                if t is not None:
                    h, w = t.shape[:2]
                    uu = (a0 * Uq[0] + a1 * Uq[1] + a2 * Uq[2]) % 1.0
                    vv = (a0 * Vq[0] + a1 * Vq[1] + a2 * Vq[2]) % 1.0
                    tx = np.clip((uu * w).astype(int), 0, w - 1); ty = np.clip((vv * h).astype(int), 0, h - 1)
                    col = col * t[ty, tx, :3]
                    al = t[ty, tx, 3]
                if pasar:
                    img[yy + y0, xx + x0] = img[yy + y0, xx + x0] * (1 - al[:, None]) + col * al[:, None]
                else:
                    img[yy + y0, xx + x0] = col
                    zb[yy + y0, xx + x0] = zz
    Image.fromarray(np.clip(img * 255, 0, 255).astype(np.uint8)).save(ruta)


def verifica(ruta, mod):
    """lector independiente del ORD7 (cabecera, mallas, vertices, texturas, CEX2/ANI1)"""
    d = open(ruta, 'rb').read()
    if d[:4] != b'ORD7':
        raise SystemExit('no es ORD7')
    h = struct.unpack('<13I', d[4:56])
    nm, nv, nr, pr, nt = h[0], h[1], h[2], h[3], h[4]
    ini = h[5:10]
    nnod, ncart, nsec = h[10], h[11], h[12] & 0x7FFFFFFF
    o = 56
    tex = []
    for k in range(nt):
        w, hh, fmt, li, off, tam = struct.unpack('<6I', d[o:o + 24]); o += 24
        if w == 0 or hh == 0 or off + tam > len(d):
            raise SystemExit('textura %d mal' % k)
        tex.append((w, hh, fmt, li, off, tam))
    mall = []
    for k in range(nm):
        cx, cy, cz, rad, i0, n, ti, nodo = struct.unpack('<4f2Iii', d[o:o + 32]); o += 32
        if i0 + n > nv or (ti >= nt) or (ti < -1):
            raise SystemExit('malla %d mal (i0 %d n %d tex %d)' % (k, i0, n, ti))
        mall.append((cx, cy, cz, rad, i0, n, ti, nodo))
    if o + nv * 24 > len(d):
        raise SystemExit('vertices fuera del fichero')
    v = np.frombuffer(d[o:o + nv * 24], dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'),
                                               ('u', '<f4'), ('v', '<f4'), ('c', '<u4')])
    o += nv * 24
    ruta_b = o
    o += nr * pr * 4
    for k in range(nnod * 32 + ncart * 64):
        pass
    o += nnod * 32 + ncart * 64
    if nsec:
        # no hay secciones en un coche: no se espera
        pass
    blob_ini = (o + 31) & ~31
    if blob_ini > len(d):
        raise SystemExit('sin blob de texturas')
    for (w, hh, fmt, li, off, tam) in tex:
        if blob_ini + off + tam > len(d):
            raise SystemExit('textura fuera del blob')
    # listas ordenadas y ANI1
    if not all(ini[k] <= ini[k + 1] for k in range(4)):
        raise SystemExit('inicios de lista no ordenados')
    ani = None; pz = None
    if nr > 0:
        q = ruta_b
        w = struct.unpack('<6I', d[q:q + 24])
        if d[q:q + 4] == b'CEX2' and w[1] == 8:
            p = q + 24 + 32 * w[2] + 20 * w[3] + 4 * w[4] + 8 * w[5]
            if d[p:p + 4] == b'SVL1':
                nvol = struct.unpack('<I', d[p + 4:p + 8])[0]
                p += 8
                for _ in range(nvol):
                    a, b = struct.unpack('<2I', d[p:p + 8]); p += 8 + 12 * a + 12 * b
            if d[p:p + 4] == b'ANI1':
                ani = struct.unpack('<I6f', d[p + 8:p + 8 + 28])
                pz = np.frombuffer(d[p + 36:p + 36 + 240], '<f4').reshape(12, 5)
    if ani is None:
        raise SystemExit('sin ANI1 (las ruedas no girarian)')
    return dict(nm=nm, nv=nv, nr=nr, pr=pr, nt=nt, ini=ini, nnod=nnod, ncart=ncart, nsec=nsec,
                tex=tex, mall=mall, verts=v, ani=ani, pz=pz, tam=len(d), blob_ini=blob_ini)


# ------------------------------------------------------------------ fisica
# offsets de la fila de PARAM.DAT (ver comun/or2_fisica.c, or2_carga_param)
CAMPOS = [
    (0x0000, 'masa', 'kg'), (0x0130, 'batalla', 'm'), (0x017c, 'via delantera', 'm'),
    (0x01c8, 'via trasera', 'm'), (0x0214, 'reparto (CG del/tras)', ''), (0x0260, 'alt_cg', 'm'),
    (0x0b48, 'radio rueda delantera', 'm'), (0x0b94, 'radio rueda trasera', 'm'),
    (0x0e40, 'mu_max delantera', ''), (0x0e8c, 'mu_max trasera', ''),
    (0x0ed8, 'mu_min delantera', ''), (0x0f24, 'mu_min trasera', ''),
    (0x1184, 'relacion 1a', ''), (0x11d0, 'relacion 2a', ''), (0x121c, 'relacion 3a', ''),
    (0x1268, 'relacion 4a', ''), (0x12b4, 'relacion 5a', ''), (0x1300, 'relacion 6a/tope', ''),
    (0x134c, 'grupo final', ''), (0x1690, 'w_max', 'rpm'), (0x1644, 'w_corte', 'rpm'),
    (0x1728, 'mult. par', ''), (0x193c, 'radio de giro', 'm'),
]


def fisica(param, salida):
    d = open(param, 'rb').read()
    C, FILAS = 19, 200
    f = np.frombuffer(d[:FILAS * C * 4], '<f4').reshape(FILAS, C).copy()
    o = (FILAS * C + 162 + 256) * 4
    if d[o:o + 4] != b'MAP1':
        raise SystemExit('PARAM.DAT sin MAP1')
    mapas = np.frombuffer(d[o + 4:o + 4 + 64 * 4], '<u4').reshape(4, 16).copy()
    col_f40 = int(mapas[1][8])       # modelo 8 = ficha 4 = COCHE04 (F40)
    taxi = f[:, col_f40].copy()
    # geometria del taxi (unidades del juego x 0.10625)
    def pon(off, val):
        taxi[off // 0x4c] = val
    pon(0x0130, 3.29)                 # batalla 31 u
    pon(0x017c, 1.47)                 # via delantera 13.8 u
    pon(0x01c8, 1.38)                 # via trasera 13.0 u
    pon(0x0b48, 0.332)                # radio 3.123 u
    pon(0x0b94, 0.332)
    pon(0x0260, 0.35)                 # alt_cg propuesto
    pon(0x0214, 1.00)                 # reparto 50/50: el CG cae en el centro de la batalla visual (ver abajo)
    pon(0x193c, 6.0)                  # radio de giro propuesto
    # inercias del taxi: la batalla 3,29 frente a 2,58 del F40 (x e y) y la via 1,47 frente a 1,62 (z)
    pon(0x004c, 1.74)                 # k_inercia x (cabeceo)
    pon(0x0098, 2.23)                 # k_inercia y (guinada)
    pon(0x00e4, 0.35)                 # k_inercia z (balanceo)
    # caja de colision del taxi (vehicle_body_parameters): el mismo rectangulo bajo que el F40,
    # con la planta real 2,121 x 5,715 (la COLI del original la usa para los muros)
    pon(0x1ab8, 1.061)                # medio ancho
    pon(0x1b04, 0.08)                 # altura del plano de la caja sobre el suelo
    # z228 = batalla / (reparto + 1) - batalla / 2 + 0,09: con reparto 1,0 son 0,09 m
    # el morro esta en -2,527 y la cola en +3,189 (caja del cuerpo de COCHE15.DAT)
    pon(0x1b9c, -2.527 + 0.09)
    pon(0x1be8, 3.189 + 0.09)
    # columna 0 (ficha 7, el Ferrari menos potente): el hueco de prueba
    f2 = f.copy(); f2[:, 0] = taxi
    cab = d[:FILAS * C * 4 + 162 * 4 + 256 * 4]
    par = d[FILAS * C * 4:FILAS * C * 4 + 162 * 4]
    fre = d[FILAS * C * 4 + 162 * 4:FILAS * C * 4 + 162 * 4 + 256 * 4]
    open(os.path.join(salida, 'PARAM_TAXI.BIN'), 'wb').write(taxi.astype('<f4').tobytes())
    # copia completa: misma cabecera y bloques, solo cambia la columna 0 de la tabla
    resto = d[FILAS * C * 4:]
    out = bytearray(f2.astype('<f4').tobytes() + resto)
    # CAM1: la vista 2 (persecucion por defecto) y las vistas 4..21 del modelo 0, mas la caja del
    # coche de la vista interior. El taxi mide 5,715 m: 7,8 m detras y 2,45 m de alto (antes 5,5 y 2,05).
    i = out.find(b'CAM1', FILAS * C * 4)
    if i >= 0:
        vm = i + 4 + 31 * 13 * 4
        def ponf(off, vals): struct.pack_into('<%df' % len(vals), out, off, *vals)
        ponf(vm + 0, [0.0, 2.45, 7.80,  0.0, 1.05, 0.0,  2.0, 16.0, -1.0])
        for v in range(4, 22):
            ponf(vm + ((v - 2) * 30) * 9 * 4, [0.0, 2.15, 7.20,  0.0, 1.00, 0.0,  1.0, 15.0, 0.0])
        mod0 = vm + 20 * 30 * 9 * 4
        ponf(mod0, [-0.353, 0.99, -0.345,  1.061, 1.541, 3.189,  -1.061, 0.144, -2.527])
    open(os.path.join(salida, 'PARAM_TAXI.DAT'), 'wb').write(out)
    # informe
    lineas = ['Columna del taxi: rendimiento de la columna %d (modelo 8, ficha 4, COCHE04 = F40) y geometria del taxi.' % col_f40,
              'La columna 0 de PARAM_TAXI.DAT es la ficha 7 (COCHES.DAT: 7 -> modelo 0), el Ferrari menos potente (empata con la columna 1).',
              '', '%-28s %12s  %-8s %s' % ('campo', 'valor', 'unidad', 'origen')]
    for off, nom, uni in CAMPOS:
        v = taxi[off // 0x4c]
        if off in (0x0130, 0x017c, 0x01c8, 0x0b48, 0x0b94, 0x0260, 0x0214, 0x193c):
            org = 'geometria del taxi (informe / propuesta)'
        else:
            org = 'F40 (columna %d)' % col_f40
        lineas.append('%-28s %12.4f  %-8s %s' % (nom, v, uni, org))
    lineas += ['', 'Las filas no listadas son copia literal de la columna %d (F40): motor, cambio, frenos, agarre,' % col_f40,
               'suspension y caja de colision. Medidas del taxi: batalla 31.0 u (3.29 m), via del. 13.8 u (1.47 m), via tras. 13.0 u (1.38 m),',
               'radio 3.123 u (0.332 m); 1 u = 0.10625 m (CRAZYTAXI_INFORME.md). Masa 1290 kg del F40 para conservar',
               'la aceleracion pedida; el informe proponia 1800 kg (se perderia aceleracion).']
    open(os.path.join(salida, 'PARAM_TAXI.txt'), 'w').write('\n'.join(lineas) + '\n')
    return taxi, col_f40, lineas


# ------------------------------------------------------------------ principal
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--piezas', default=DEF_PIEZAS)
    ap.add_argument('--salida', default=DEF_SALIDA)
    ap.add_argument('--param', default=DEF_PARAM)
    ap.add_argument('--sin-fisica', action='store_true')
    ap.add_argument('--sin-vista', action='store_true')
    ap.add_argument('--sin-piloto', action='store_true')
    a = ap.parse_args()
    os.makedirs(a.salida, exist_ok=True)
    piezas = [carga('taxi_cuerpo', a.piezas)] + [carga('taxi_rueda_%d' % k, a.piezas) for k in range(4)]
    mod, mapa_tex = construye(piezas, a.piezas, not a.sin_piloto)
    if os.environ.get('TAXI_SIN_VQ'):
        mod.vq = False                  # prueba: texturas a 16 bits (para comparar el artefacto del maletero)
    radio = RADIO_RUEDA * ESCALA
    # luces: freno (grupo 3) y faros (grupo 4); la posicion sale de las tiras del modelo original
    quads = luces_taxi(mod, float(os.environ.get('TAXI_LUZ_INT', '0.80')), cuerpo=piezas[0], mapa_tex=mapa_tex)
    mod.mallas.sort(key=lambda x: (x[0], mod.orden[id(x)]))
    grupos = []
    for m, g in quads:
        for i, x in enumerate(mod.mallas):
            if x is m:
                grupos.append((i, g)); break
    # sin SVL1: la sombra es la pieza 12 (plana, de contacto, bajo la caja del coche); el volumen
    # proyectado por el sol se alargaba fuera del coche (queja del usuario).  base 0: sin el
    # temblor de 4A2910 a mas de 100 km/h.
    mod.ruta, pz = bloque_ani(radio, 0.0, grupos=grupos, piloto=not a.sin_piloto)
    sal = os.path.join(a.salida, 'COCHE15.DAT')
    mod.escribe(sal)
    # comprobacion leyendo el fichero
    info = verifica(sal, mod)
    tri = sum(max(0, n - 2) for _, _, _, _, _, n, _, _ in info['mall'])
    tam = info['tam']
    cabe = tam + info['nv'] * 4 <= COCHE_RAM
    txt = ['COCHE15.DAT (taxi de Crazy Taxi, ORD7)', '',
           'mallas %d, vertices %d, texturas %d; CEX2+ANI1 de %d palabras (sin ruta de carrera), %d nodos, %d carteles, %d secciones'
           % (info['nm'], info['nv'], info['nt'], info['nr'] * info['pr'], info['nnod'], info['ncart'], info['nsec']),
           'triangulos %d (cuerpo 1282 + 4 ruedas de 98)' % tri,
           'inicios de lista: opaca %d, recorte %d, transparente %d, cielo %d, fin %d' % tuple(info['ini']),
           'ANI1: modelo %d, radio %.4f m (3.123 u), pivotes de rueda:' % (info['ani'][0], info['ani'][1])]
    for k in range(4):
        txt.append('  rueda %d: (%.4f, %.4f, %.4f)' % ((k,) + tuple(info['pz'][8 + k, :3])))
    txt += ['tamano %d B (%d KB); con los desplazamientos de vertice (%d B) %d B de %d B: %s'
            % (tam, tam >> 10, info['nv'] * 4, tam + info['nv'] * 4, COCHE_RAM,
               'cabe en el bloque del coche' if cabe else 'NO cabe'),
            'texturas: ' + ', '.join('%d:%dx%d fmt %d lista %d' % (k, t[0], t[1], t[2], t[3]) for k, t in enumerate(info['tex']))]
    # vista
    if not a.sin_vista:
        try:
            vista(mod, os.path.join(a.salida, 'COCHE15.png'), np.array([4.8, 2.2, -6.2]), np.array([0, 0.8, 0]))
        except Exception as ex:
            txt.append('vista: %s' % ex)
    # fisica
    if not a.sin_fisica:
        try:
            taxi, col, lineas = fisica(a.param, a.salida)
            txt += ['', 'fisica: columna %d (F40) + geometria del taxi; PARAM_TAXI.BIN y PARAM_TAXI.DAT' % col]
        except Exception as ex:
            txt.append('fisica: %s' % ex)
    # luces: de que registro/tira sale cada malla y correspondencia vertices -> rectangulo
    inf = informe_luces()
    txt += ['', 'luces: posiciones sacadas de las tiras del modelo original (POLDC0 0x38E8, no inventadas)'] + inf
    open(os.path.join(a.salida, 'LUCES_TAXI.txt'), 'w').write(
        '\n'.join(['Luces del taxi: posiciones sacadas del modelo original de Crazy Taxi (POLDC0 0x38E8).',
                   'Escala 1 u = %.5f m; giro 180 en y (x -> -x, z -> -z), el mismo que el cuerpo.' % ESCALA,
                   'El runtime (demo/main.c, coche_luces) no calcula posiciones: enciende/apaga las mallas del CEX2.',
                   'rect = caja de la tira original; quad = malla de luz (freno +0.002 m en z; faros -0.002 m).',
                   ''] + inf) + '\n')
    open(os.path.join(a.salida, 'COCHE15.txt'), 'w').write('\n'.join(txt) + '\n')
    print('\n'.join(txt))


if __name__ == '__main__':
    main()
