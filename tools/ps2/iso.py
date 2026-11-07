"""Lectura del DVD de PS2 de OutRun 2006 Coast 2 Coast (tu ISO): ficheros ISO 9660 y los paquetes *.PS2.
Paquete: cuenta (x4), y por entrada (hash, desplazamiento, tamano, tamano) de 16 bytes."""
import mmap, struct

class Iso:
    def __init__(self, ruta):
        self.f = open(ruta, 'rb'); self.m = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
        pvd = self.m[16 * 2048:17 * 2048]
        assert pvd[1:6] == b'CD001', 'no es un ISO 9660'
        rl = struct.unpack('<I', pvd[158:162])[0]; rs = struct.unpack('<I', pvd[166:170])[0]
        self.fich = {}
        b = self.m[rl * 2048:rl * 2048 + rs]; i = 0
        while i < len(b):
            n = b[i]
            if n == 0: i = (i // 2048 + 1) * 2048; continue
            l = struct.unpack('<I', b[i + 2:i + 6])[0]; s = struct.unpack('<I', b[i + 10:i + 14])[0]
            nm = b[i + 33:i + 33 + b[i + 32]].decode('latin1').split(';')[0]
            if nm not in ('\0', '\1'): self.fich[nm] = (l * 2048, s)
            i += n

    def paquete(self, nombre):
        """(base, [(hash, base+desplazamiento, tamano)] ordenadas por desplazamiento)"""
        base, _ = self.fich[nombre]
        n = struct.unpack('<I', self.m[base:base + 4])[0]
        e = [struct.unpack('<4I', self.m[base + 16 + 16 * i:base + 32 + 16 * i]) for i in range(n)]
        return sorted(((h, base + o, s) for h, o, s, _ in e), key=lambda x: x[1])
