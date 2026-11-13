"""Compresion VQ de la PowerVR (texturas twiddled de 16 bits) con numpy.

Formato de la PowerVR: libro de codigos de hasta 256 entradas de 8 bytes (4 texeles de 16 bits en el orden twiddled
de un bloque de 2x2: arriba-izquierda, abajo-izquierda, arriba-derecha, abajo-derecha) seguido de un byte por bloque
de 2x2, en el mismo orden twiddled que los texeles de una textura sin comprimir (vale para las rectangulares: el
orden twiddled agrupa siempre los texeles de 4 en 4 en bloques de 2x2). Es lo mismo que hace pvrtex de KallistiOS
(utils/pvrtex: twiddle y luego vectores de 4 texeles seguidos).

Libro reducido (como el formato .DT de pvrtex): con N < 256 entradas solo se guardan las N ultimas (indices 256-N a
255) y la textura se direcciona 2048 - 8 N bytes antes de los datos; la PowerVR nunca lee las entradas que faltan.

Codificador: k-means (Lloyd) ponderado sobre los vectores distintos de la textura (16 dimensiones RGBA, o 12 sin
alfa), arranque k-means++ sobre una muestra, con un tope de iteraciones y de tiempo; luego el libro se cuantifica al
formato de 16 bits y cada bloque se reasigna a la entrada cuantificada mas cercana."""
import time
import numpy as np

PESO = np.array([0.85, 1.0, 0.6, 1.0])              # R, G, B, A: algo de peso perceptual (el verde manda)


def _cuantifica(c, fmt):
    """valores 0-255 (float, ... x 4 RGBA) -> palabras de 16 bits del formato y su valor decodificado en 0-255"""
    c = np.clip(np.rint(c), 0, 255).astype(np.uint32)
    r, g, b, a = c[..., 0], c[..., 1], c[..., 2], c[..., 3]
    if fmt == 1:                                     # RGB565
        R, G, B = (r * 31 + 127) // 255, (g * 63 + 127) // 255, (b * 31 + 127) // 255
        v = (R << 11) | (G << 5) | B
        dec = np.stack([R * 255 / 31, G * 255 / 63, B * 255 / 31, np.full(R.shape, 255.0)], -1)
    elif fmt == 0:                                   # ARGB1555
        R, G, B, A = (r * 31 + 127) // 255, (g * 31 + 127) // 255, (b * 31 + 127) // 255, (a >= 128).astype(np.uint32)
        v = (A << 15) | (R << 10) | (G << 5) | B
        dec = np.stack([R * 255 / 31, G * 255 / 31, B * 255 / 31, A * 255.0], -1)
    else:                                            # ARGB4444
        R, G, B, A = ((x * 15 + 127) // 255 for x in (r, g, b, a))
        v = (A << 12) | (R << 8) | (G << 4) | B
        dec = np.stack([R * 17.0, G * 17.0, B * 17.0, A * 17.0], -1)
    return v.astype(np.uint16), dec


def _dist(X, C):
    """distancias al cuadrado de cada fila de X a cada fila de C (por trozos para no gastar memoria)"""
    return (X * X).sum(1)[:, None] - 2 * X @ C.T + (C * C).sum(1)[None, :]


def _asigna(X, C, trozo=8192):
    idx = np.empty(len(X), np.int64); d = np.empty(len(X))
    for i in range(0, len(X), trozo):
        D = _dist(X[i:i + trozo], C); k = D.argmin(1); idx[i:i + trozo] = k; d[i:i + trozo] = D[np.arange(len(k)), k]
    return idx, np.maximum(d, 0)


def kmeans(X, w, k, iters=24, tiempo=4.0, semilla=1):
    """k-means ponderado (w: peso de cada fila). Devuelve los centros (k x dim)"""
    rng = np.random.default_rng(semilla)
    if len(X) <= k: return X.copy()
    # k-means++ sobre una muestra (los vectores con mas peso salen mas)
    m = min(len(X), 4096); S = X[rng.choice(len(X), m, replace=False, p=w / w.sum())] if len(X) > m else X
    C = [S[rng.integers(len(S))]]; d = ((S - C[0]) ** 2).sum(1)
    for _ in range(1, k):
        p = d / d.sum() if d.sum() > 0 else None
        c = S[rng.choice(len(S), p=p)]; C.append(c); d = np.minimum(d, ((S - c) ** 2).sum(1))
    C = np.array(C); t0 = time.time(); ant = None
    for _ in range(iters):
        idx, d = _asigna(X, C)
        err = float((d * w).sum())
        sw = np.bincount(idx, w, k); vacios = np.where(sw == 0)[0]
        for j in range(X.shape[1]): C[:, j] = np.where(sw > 0, np.bincount(idx, w * X[:, j], k) / np.maximum(sw, 1e-9), C[:, j])
        if len(vacios):                              # los vacios van a los vectores peor representados
            peores = np.argsort(-d * w)[:len(vacios)]; C[vacios] = X[peores]
        if ant is not None and ant - err <= ant * 1e-3: break
        ant = err
        if time.time() - t0 > tiempo: break
    return C


def _twiddled(a):
    """texeles RGBA (float) en el orden twiddled de la PowerVR"""
    from modelo import twiddle
    h, w = a.shape[:2]
    P = np.zeros((w * h, 4)); P[twiddle(w, h).ravel()] = a.reshape(-1, 4)
    return P


def codifica(a, fmt, n=256, tiempo=4.0):
    """RGBA (alto x ancho x 4, uint8) -> (datos: libro de n entradas + indices, error RMS en 0-255 frente al formato
    sin comprimir). Las entradas usadas son las n ultimas del libro (indices 256-n..255)"""
    return _codifica_P(_twiddled(a), fmt, n, tiempo)


def codifica_mip(niveles, fmt, n=256, tiempo=4.0):
    """VQ con mipmaps (niveles de 1x1 a la base, cuadrados): un libro para todos y los indices de cada nivel seguidos
    de menor a mayor, como los lee la PowerVR (el nivel de lado 2^k en 0x0, 0x1, 0x2, 0x6, 0x16, 0x56... tras el
    libro); el de 1x1 es un bloque de 2x2 de su color"""
    P = [_twiddled(np.repeat(np.repeat(x, 2, 0), 2, 1) if x.shape[0] == 1 else x) for x in niveles]
    return _codifica_P(np.concatenate(P, 0), fmt, n, tiempo)


def _codifica_P(P, fmt, n, tiempo):
    canales = 3 if fmt == 1 else 4
    pes = np.tile(np.sqrt(PESO[:canales]), 4)
    V = P.reshape(-1, 4, 4)[:, :, :canales].reshape(-1, 4 * canales)
    if fmt == 0: V = V.copy(); V[:, 3::4] = np.where(V[:, 3::4] >= 128, 255.0, 0.0) * 2  # el alfa de 1 bit no se mezcla
    U, inv, cnt = np.unique(V, axis=0, return_inverse=True, return_counts=True)
    C = kmeans(U * pes, cnt.astype(float), min(n, len(U)), tiempo=tiempo) / pes
    if fmt == 0: C[:, 3::4] /= 2
    C4 = np.zeros((len(C), 4, 4)); C4[..., 3] = 255
    C4[:, :, :canales] = C.reshape(-1, 4, canales)
    pal, dec = _cuantifica(C4, fmt)                  # (k, 4) palabras y (k, 4, 4) decodificado
    Dk = dec[:, :, :canales].reshape(len(C), -1)
    if fmt == 0: Dk = Dk.copy(); Dk[:, 3::4] *= 2
    idx, _ = _asigna(U * pes, Dk * pes)
    idx = idx[inv.ravel()]
    k = len(C)
    libro = np.zeros((n, 4), np.uint16); libro[:k] = pal  # se guardan n entradas aunque haya menos distintas
    datos = libro.tobytes() + (idx + 256 - n).astype(np.uint8).tobytes()
    # error frente a la misma textura sin comprimir en su formato de 16 bits
    _, ref = _cuantifica(P.reshape(-1, 4, 4), fmt)
    rec = dec[idx]
    err = float(np.sqrt(((rec - ref)[:, :, :canales] ** 2).mean()))
    return datos, err


def decodifica(datos, w, h, fmt, n=256):
    """datos VQ -> RGBA (alto x ancho x 4) para comprobar a ojo"""
    from modelo import twiddle
    libro = np.frombuffer(datos[:8 * n], np.uint16).reshape(n, 4).astype(np.uint32)
    idx = np.frombuffer(datos[8 * n:8 * n + w * h // 4], np.uint8).astype(int) - (256 - n)
    v = libro[idx].reshape(-1)
    if fmt == 1: rgba = [(v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31, v * 0 + 255]
    elif fmt == 0: rgba = [((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31, (v >> 15) * 255]
    else: rgba = [((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17, (v >> 12) * 17]
    P = np.stack(rgba, -1).astype(np.uint8)
    return P[twiddle(w, h).ravel()].reshape(h, w, 4)


UMBRAL = float(__import__('os').environ.get("VQ_UMBRAL", "9"))   # error RMS maximo (0-255) para aceptar la VQ


def elige(a, fmt):
    """VQ si ahorra memoria de video y el error es aceptable: (datos, n, error) o None (sin comprimir). Las de menos
    de 32x32 no ganan nada (el libro pesa 2 KB); las de menos de 64x64 llevan un libro reducido (1 entrada por cada
    4 bloques, minimo 16)"""
    h, w = a.shape[:2]; bloques = w * h // 4
    if w * h < 32 * 32 or min(w, h) < 8: return None
    n = 256 if bloques >= 1024 else max(16, (bloques // 4 + 3) & ~3)
    datos, err = codifica(a, fmt, n)
    if err > UMBRAL or len(datos) >= w * h * 2: return None
    return datos, n, err


def elige_mip(niveles, fmt):
    """como elige, para una textura con mipmaps (codifica_mip; niveles de 1x1 a la base)"""
    w = niveles[-1].shape[1]; h = niveles[-1].shape[0]; bloques = sum(max(1, x.shape[0] * x.shape[1] // 4) for x in niveles)
    if w * h < 32 * 32 or min(w, h) < 8: return None
    n = 256 if bloques >= 1024 else max(16, (bloques // 4 + 3) & ~3)
    datos, err = codifica_mip(niveles, fmt, n)
    if err > UMBRAL or len(datos) >= 6 + sum(x.shape[0] * x.shape[1] * 2 for x in niveles): return None
    return datos, n, err


def decodifica16(datos, w, h, fmt):
    """texeles twiddled de 16 bits del formato (0 ARGB1555, 1 RGB565, 2 ARGB4444) -> RGBA (alto x ancho x 4)"""
    from modelo import twiddle
    v = np.frombuffer(datos[:w * h * 2], np.uint16).astype(np.uint32)
    if fmt == 1: rgba = [(v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31, v * 0 + 255]
    elif fmt == 0: rgba = [((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31, (v >> 15) * 255]
    else: rgba = [((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17, (v >> 12) * 17]
    P = np.stack(rgba, -1).astype(np.uint8)
    return P[twiddle(w, h).ravel()].reshape(h, w, 4)


# Segunda pasada (plan de recursos, 2026-11-12): las texturas que se quedaron en 16 bits por pasar de UMBRAL (con
# ruido fino: rocas, montes, gente, hojas; error 9-17 medido en todo el arbol del modo OutRun y OUTRUN2SP) se
# codifican desde sus propios texeles de 16 bits y se aceptan hasta UMBRAL2, el de las metas desde 2026-11-08
# (conv_union.py UMBRAL_META). Partir de los 16 bits guardados (y no de la imagen de 8 bits) hace que la conversion
# desde la ISO y tools/revq.py sobre unos datos ya hechos den los mismos bytes. El codificador es determinista
# (semilla fija; sin tope de tiempo: kmeans acaba por su tolerancia en menos de 1 s en todas). Decision por clase:
# etapas, uniones, trafico, personajes, banderero y objetos de las uniones si; el coche del jugador no (insignias y
# logotipos que se ven de cerca en la seleccion: se queda con UMBRAL).
UMBRAL2 = 24.0


def elige16(datos, w, h, fmt, umbral=UMBRAL2):
    """como elige, para una textura de 16 bits ya guardada (sin mipmaps): (datos VQ, n, error) o None"""
    if w * h < 32 * 32 or min(w, h) < 8: return None
    a = decodifica16(datos, w, h, fmt)
    bloques = w * h // 4; n = 256 if bloques >= 1024 else max(16, (bloques // 4 + 3) & ~3)
    vq, err = codifica(a, fmt, n, tiempo=1e9)
    if err > umbral or len(vq) >= w * h * 2: return None
    return vq, n, err


# Cielos (revision 44, docs/PARIDAD.md: Cielo, escalones del degradado). La textura del cielo del original es de 8
# bits con paleta (p. ej. el azul alto de PALM: R 119..121, B 205..214 en pasos de 1) y en RGB565 el cielo casi liso
# quedaba en zonas de un mismo valor con un salto de 8 entre ellas (R 119 -> 115, 121 -> 123): lineas en el cielo.
# Aqui el k-means va sobre los texeles de 8 bits (como siempre) pero cada entrada del libro se cuantifica con un
# tramado ordenado de 2x2 dentro de su bloque (Bayer: umbrales 1/8, 7/8, 5/8, 3/8 en el orden twiddled arriba-izquierda,
# abajo-izquierda, arriba-derecha, abajo-derecha): la media del bloque guarda la entrada con 1/4 del paso del formato, y
# los bloques se asignan por la entrada sin cuantificar. Mismos bytes (libro de 2 KB e indices) que la VQ de antes: la
# memoria de video no cambia. En la pantalla el texel del cielo mide unos 2 px (PALM: 0,28 grados) y el filtro bilineal
# deja la trama de 4 px casi lisa. Determinista (sin tope de tiempo).
TRAMA2 = np.array([0.125, 0.875, 0.625, 0.375])


def _cuantifica_tramada(c, fmt):
    """como _cuantifica para bloques (k, 4 texeles twiddled, RGBA en 0-255 float) con el tramado TRAMA2 en el color;
    el alfa sin tramar. Devuelve (k, 4) palabras de 16 bits"""
    t = TRAMA2[None, :]
    def q(x, m): return np.clip(np.floor(x * m / 255.0 + t), 0, m).astype(np.uint32)
    r, g, b, a = c[..., 0], c[..., 1], c[..., 2], np.clip(np.rint(c[..., 3]), 0, 255).astype(np.uint32)
    if fmt == 1: v = (q(r, 31) << 11) | (q(g, 63) << 5) | q(b, 31)
    elif fmt == 0: v = ((a >= 128).astype(np.uint32) << 15) | (q(r, 31) << 10) | (q(g, 31) << 5) | q(b, 31)
    else: v = (((a * 15 + 127) // 255) << 12) | (q(r, 15) << 8) | (q(g, 15) << 4) | q(b, 15)
    return v.astype(np.uint16)


def codifica_tramada(a, fmt, n=256):
    """VQ del cielo (ver arriba): RGBA (alto x ancho x 4, uint8) -> (datos: libro de n entradas + indices, error RMS en
    0-255 de la media de cada bloque decodificado frente a la de la imagen de 8 bits)"""
    P = _twiddled(a); canales = 3 if fmt == 1 else 4
    pes = np.tile(np.sqrt(PESO[:canales]), 4)
    V = P.reshape(-1, 4, 4)[:, :, :canales].reshape(-1, 4 * canales)
    U, inv, cnt = np.unique(V, axis=0, return_inverse=True, return_counts=True)
    C = kmeans(U * pes, cnt.astype(float), min(n, len(U)), tiempo=1e9) / pes
    idx, _ = _asigna(U * pes, C * pes); idx = idx[inv.ravel()]
    C4 = np.zeros((len(C), 4, 4)); C4[..., 3] = 255; C4[:, :, :canales] = C.reshape(-1, 4, canales)
    pal = _cuantifica_tramada(C4, fmt)
    libro = np.zeros((n, 4), np.uint16); libro[:len(C)] = pal
    datos = libro.tobytes() + (idx + 256 - n).astype(np.uint8).tobytes()
    rec = decodifica(datos, a.shape[1], a.shape[0], fmt, n).astype(float)
    h, w = a.shape[:2]
    m = lambda x: x.reshape(h // 2, 2, w // 2, 2, 4)[..., :canales].mean((1, 3))
    return datos, float(np.sqrt(((m(rec) - m(a.astype(float))) ** 2).mean()))


def elige_cielo(a, fmt):
    """VQ tramada de una textura del cielo: (datos, n, error) o None si es pequena (menos de 32x32 o de lado < 8: va en
    16 bits tramados, modelo.codifica16_tramada). Siempre se acepta (el error es el de la media de cada bloque)"""
    h, w = a.shape[:2]; bloques = w * h // 4
    if w * h < 32 * 32 or min(w, h) < 8: return None
    n = 256 if bloques >= 1024 else max(16, (bloques // 4 + 3) & ~3)
    datos, err = codifica_tramada(a, fmt, n)
    return datos, n, err


def compara(a, datos, fmt, ruta):
    """PNG con la textura sin comprimir (izquierda) y en VQ (derecha)"""
    from PIL import Image
    h, w = a.shape[:2]; n = ((fmt >> 16) & 255) + 1
    b = decodifica(datos, w, h, fmt & 255, n)
    Image.fromarray(np.concatenate([a, b], 1)).save(ruta)
