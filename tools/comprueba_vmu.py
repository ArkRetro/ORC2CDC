"""Comprobador de una imagen de VMU (128 KB, 256 bloques de 512 bytes) como la leeria el gestor de memoria de la BIOS.

Uso: python3 comprueba_vmu.py <imagen.bin> [fichero (OR2C2C)] [--extrae salida.bin]
La imagen es la de Flycast (<XDG_DATA_HOME>/flycast/T-00000_vmu_save_A1.bin con un disco sin id de producto, o
vmu_save_A1.bin) o un volcado de una VMU real. Sale con 1 si encuentra algo que la BIOS rechazaria.

Comprueba (especificacion de la VMU de Marcus Comstedt, "Visual Memory Unit file system" y "VMS file header"; la
misma que siguen vmufs.c y vmu_pkg.c de KallistiOS):
 - bloque raiz (255): 16 x 0x55, FAT (254, 1 bloque), directorio (253, 13 bloques), bloques de usuario (200);
 - FAT: los bloques del sistema encadenados (directorio 253 -> 241, FAT y raiz fin de cadena), libres = 0xFFFC,
   cada fichero una cadena sin bucles ni cruces que acaba en 0xFFFA con tantos bloques como dice el directorio;
 - directorio: tipo 0x33 (datos) / 0xCC (juego), proteccion 0x00 / 0xFF, primer bloque, nombre (12 bytes ASCII
   rellenos con 0 o espacios), fecha BCD valida, tamano en bloques, offset de la cabecera (0 en los datos), relleno 0;
 - cabecera VMS del fichero pedido (o de todos): descripciones de 16 y 32 bytes imprimibles rellenas con espacios,
   id de aplicacion (16, relleno con 0), iconos 1..3, velocidad, eyecatch 0..3 con su tamano, CRC (CRC-16 CCITT de
   la BIOS, polinomio 0x1021, valor inicial 0, sobre cabecera + iconos + eyecatch + datos con el campo CRC a 0),
   longitud de datos dentro del fichero, reservado a 0, paleta (alfa) e iconos dentro del fichero.
"""
import struct, sys

BLK = 512
LIBRE, FIN = 0xFFFC, 0xFFFA
EC_TAM = {0: 0, 1: 72 * 56 * 2, 2: 512 + 72 * 56, 3: 32 + 72 * 56 // 2}


def crc_bios(buf):
    """CRC de la cabecera VMS como la calcula la BIOS (bit a bit, MSB primero)"""
    n = 0
    for b in buf:
        n ^= b << 8
        for _ in range(8):
            n = ((n << 1) ^ 0x1021) if n & 0x8000 else (n << 1)
            n &= 0xFFFF
    return n


def bcd(v, lo, hi):
    a, b = v >> 4, v & 15
    return a < 10 and b < 10 and lo <= a * 10 + b <= hi


def comprueba(img, quiero=None, extrae=None):
    errores, avisos = [], []
    err = errores.append
    if len(img) != 256 * BLK:
        err("la imagen tiene %d bytes (deben ser %d)" % (len(img), 256 * BLK)); return errores, avisos
    blq = lambda n: img[n * BLK:(n + 1) * BLK]
    raiz = blq(255)
    if raiz[:16] != b"\x55" * 16: err("raiz: los 16 primeros bytes no son 0x55 (VMU sin formatear)")
    fat_loc, fat_tam, dir_loc, dir_tam, icono, n_usu = struct.unpack_from("<6H", raiz, 0x46)
    print("raiz: FAT %d (%d), directorio %d (%d), icono %d, bloques de usuario %d, formato %s" % (
        fat_loc, fat_tam, dir_loc, dir_tam, icono, n_usu, raiz[0x30:0x38].hex()))
    if (fat_loc, fat_tam, dir_loc, dir_tam, n_usu) != (254, 1, 253, 13, 200):
        err("raiz: disposicion distinta de la estandar (254/1, 253/13, 200)")
    fat = struct.unpack("<256H", blq(fat_loc))
    # cadenas del sistema
    if fat[255] != FIN or fat[254] != FIN: err("FAT: raiz o FAT no marcados como fin de cadena (%04x %04x)" % (fat[255], fat[254]))
    b = dir_loc
    for i in range(dir_tam):
        sig = fat[b]
        esp = FIN if i == dir_tam - 1 else b - 1
        if sig != esp: err("FAT: el directorio en el bloque %d apunta a %04x (debe %04x)" % (b, sig, esp)); break
        b = sig
    for k in range(n_usu, dir_loc - dir_tam + 1):
        if fat[k] not in (LIBRE,): avisos.append("FAT: bloque %d fuera del area de usuario con %04x" % (k, fat[k]))
    libres = sum(1 for k in range(n_usu) if fat[k] == LIBRE)
    # directorio: bloque 253 primero, 16 entradas por bloque
    ents = []
    for i in range(dir_tam):
        d = blq(dir_loc - i)
        for j in range(16):
            ents.append(d[j * 32:(j + 1) * 32])
    usados = {}
    ficheros = []
    for i, e in enumerate(ents):
        tipo = e[0]
        if tipo == 0:
            if any(e): avisos.append("directorio %d: entrada vacia con bytes no nulos" % i)
            continue
        prot, pri = e[1], struct.unpack_from("<H", e, 2)[0]
        nom = e[4:16]; fecha = e[16:24]
        tam, hoff = struct.unpack_from("<HH", e, 24)
        nomt = nom.rstrip(b"\0 ").decode("ascii", "replace")
        pre = "fichero %r (entrada %d)" % (nomt, i)
        if tipo not in (0x33, 0xCC): err("%s: tipo %02x (debe ser 0x33 datos o 0xCC juego)" % (pre, tipo))
        if prot not in (0, 0xFF): err("%s: proteccion %02x" % (pre, prot))
        if not all(32 <= c < 127 or c == 0 for c in nom): err("%s: nombre con caracteres no ASCII" % pre)
        if e[28:32] != b"\0\0\0\0": err("%s: bytes 0x1C..0x1F no nulos (%s)" % (pre, e[28:32].hex()))
        cs, an, me, di, ho, mi, se, ds = fecha
        if not (bcd(cs, 19, 20) and bcd(an, 0, 99) and bcd(me, 1, 12) and bcd(di, 1, 31) and bcd(ho, 0, 23)
                and bcd(mi, 0, 59) and bcd(se, 0, 59) and bcd(ds, 0, 6)):
            err("%s: fecha BCD no valida %s" % (pre, fecha.hex()))
        if tipo == 0x33 and hoff != 0: err("%s: offset de la cabecera %d (0 en un fichero de datos)" % (pre, hoff))
        if tipo == 0xCC and hoff != 1: avisos.append("%s: juego con offset de cabecera %d" % (pre, hoff))
        # cadena de la FAT
        cad, b = [], pri
        while b != FIN and len(cad) <= 256:
            if b >= 256 or b == LIBRE: err("%s: cadena rota en %04x tras %d bloques" % (pre, b, len(cad))); break
            if b in usados: err("%s: el bloque %d es tambien de %r" % (pre, b, usados[b])); break
            if b >= n_usu: err("%s: bloque %d fuera del area de usuario" % (pre, b)); break
            usados[b] = nomt; cad.append(b); b = fat[b]
        if len(cad) != tam: err("%s: la FAT tiene %d bloques y el directorio dice %d" % (pre, len(cad), tam))
        if tipo == 0x33 and cad and cad[0] != max(cad):
            avisos.append("%s: datos que no empiezan por arriba (la BIOS los pone desde el bloque 199 hacia abajo)" % pre)
        print("%s: tipo %02x prot %02x primer %d bloques %d cabecera %d fecha %02x%02x-%02x-%02x %02x:%02x:%02x (%d) cadena %s" % (
            pre, tipo, prot, pri, tam, hoff, cs, an, me, di, ho, mi, se, ds,
            " ".join(map(str, cad[:6])) + (" ..." if len(cad) > 6 else "")))
        ficheros.append((nomt, tipo, hoff, b"".join(blq(k) for k in cad)))
    sueltos = [k for k in range(n_usu) if fat[k] != LIBRE and k not in usados]
    if sueltos: err("FAT: bloques ocupados sin fichero: %s" % sueltos[:20])
    print("bloques libres %d (la BIOS los cuenta asi: los %d de usuario a 0xFFFC)" % (libres, n_usu))
    encontrado = False
    for nomt, tipo, hoff, datos in ficheros:
        if quiero and nomt != quiero: continue
        encontrado = True
        if extrae: open(extrae, "wb").write(datos)
        cabecera(nomt, datos[hoff * BLK:], errores, avisos)
    if quiero and not encontrado: err("no esta el fichero %r en el directorio" % quiero)
    return errores, avisos


def cabecera(nom, f, errores, avisos):
    err = errores.append
    pre = "cabecera VMS de %r" % nom
    if len(f) < 128: err("%s: fichero de menos de 128 bytes" % pre); return
    corta, larga, app = f[0:16], f[16:48], f[48:64]
    nic, vel, ec, crc, dlen = struct.unpack_from("<HHHHI", f, 64)
    res = f[76:96]
    pal = struct.unpack_from("<16H", f, 96)
    print("%s: corta %r larga %r app %r iconos %d velocidad %d eyecatch %d crc %04x datos %d" % (
        pre, corta.decode("ascii", "replace"), larga.decode("ascii", "replace"), app.rstrip(b"\0").decode("ascii", "replace"),
        nic, vel, ec, crc, dlen))
    for n, c in (("corta", corta), ("larga", larga)):
        if not all(32 <= x < 127 for x in c): err("%s: descripcion %s con bytes no imprimibles (sin rellenar con espacios?): %s" % (pre, n, c.hex()))
    if not all(32 <= x < 127 or x == 0 for x in app): err("%s: id de aplicacion con bytes raros" % pre)
    if 0 in app and any(app[app.index(0):]): err("%s: id de aplicacion sin relleno de ceros" % pre)
    if not 1 <= nic <= 3: err("%s: %d iconos (la BIOS anima de 1 a 3)" % (pre, nic))
    if nic > 1 and vel == 0: avisos.append("%s: icono animado con velocidad 0" % pre)
    if ec not in EC_TAM: err("%s: tipo de eyecatch %d" % (pre, ec)); return
    if any(res): err("%s: reservado (+0x4C..0x5F) no nulo" % pre)
    tot = 128 + 512 * nic + EC_TAM[ec] + dlen
    if tot > len(f): err("%s: cabecera + iconos + eyecatch + datos = %d bytes y el fichero tiene %d" % (pre, tot, len(f))); return
    if (tot + BLK - 1) // BLK != len(f) // BLK:
        avisos.append("%s: %d bytes utiles en %d bloques (sobran bloques enteros)" % (pre, tot, len(f) // BLK))
    b = bytearray(f[:tot]); b[70:72] = b"\0\0"
    c = crc_bios(b)
    if c != crc: err("%s: CRC %04x y el de la BIOS da %04x" % (pre, crc, c))
    else: print("%s: CRC bien (%04x sobre %d bytes)" % (pre, c, tot))
    if all(p == 0 for p in pal): err("%s: paleta del icono vacia" % pre)
    if not any(p >> 12 for p in pal): err("%s: paleta del icono sin alfa (icono invisible)" % pre)
    ico = f[128:128 + 512 * nic]
    if len(set(ico)) < 2: avisos.append("%s: icono liso" % pre)
    if ec:
        e = f[128 + 512 * nic:128 + 512 * nic + EC_TAM[ec]]
        if len(set(e)) < 2: avisos.append("%s: eyecatch liso" % pre)


def main():
    a = [x for x in sys.argv[1:]]
    extrae = None
    if "--extrae" in a:
        i = a.index("--extrae"); extrae = a[i + 1]; del a[i:i + 2]
    if not a: print(__doc__); sys.exit(2)
    img = open(a[0], "rb").read()
    quiero = a[1] if len(a) > 1 else None
    errores, avisos = comprueba(img, quiero, extrae)
    for x in avisos: print("AVISO:", x)
    for x in errores: print("ERROR:", x)
    print("resultado: %s" % ("MAL (%d errores)" % len(errores) if errores else "bien"))
    sys.exit(1 if errores else 0)


if __name__ == "__main__":
    main()
