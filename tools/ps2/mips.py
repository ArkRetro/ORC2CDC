"""Interprete minimo del R5900 (MIPS III + ld/sd de 64 bits) para ejecutar en la conversion las rutinas de
inicializacion del ejecutable que rellenan tablas en la BSS (lo que el juego calcula al correr y no esta en el fichero).
Solo las instrucciones que usan esas rutinas (copias de bloques, bucles simples, llamadas a funciones hoja): con
cualquier otra se para con un error en vez de inventar. Memoria: los segmentos del ELF (fichero + BSS a cero) y una pila.
Uso: m = Mips(elf); m.llama(0x1f2110); m.lee(va, n)."""
import struct

class Mips:
    PILA = 0x02000000

    def __init__(self, elf):
        self.mem = {}                                       # va base del segmento -> bytearray (memsz)
        for va, off, fs, ms in elf.segmentos:
            b = bytearray(max(fs, ms)); b[:fs] = elf.d[off:off + fs]; self.mem[va] = b
        self.mem[self.PILA - 0x10000] = bytearray(0x10000)
        self.r = [0] * 32
        self.pasos = 0

    def _sitio(self, va, n):
        for b, m in self.mem.items():
            if b <= va and va + n <= b + len(m): return m, va - b
        raise ValueError('mips: direccion 0x%x fuera de la memoria' % va)

    def lee(self, va, n):
        m, o = self._sitio(va, n); return bytes(m[o:o + n])

    def escribe(self, va, d):
        m, o = self._sitio(va, len(d)); m[o:o + len(d)] = d

    def _u(self, va, f): return struct.unpack(f, self.lee(va, struct.calcsize(f)))[0]

    def llama(self, pc, *args, tope=5000000):
        """ejecuta la funcion pc con los argumentos a0..a3 hasta su jr ra; devuelve v0"""
        r = self.r
        r[29] = self.PILA - 0x100; r[31] = 0xDEAD0000
        for i, a in enumerate(args): r[4 + i] = a & 0xFFFFFFFFFFFFFFFF
        self._corre(pc, tope)
        return r[2] & 0xFFFFFFFF

    def _corre(self, pc, tope):
        r = self.r; M64 = 0xFFFFFFFFFFFFFFFF
        def s32(x): x &= 0xFFFFFFFF; return x - (1 << 32) if x & 0x80000000 else x
        def sx(x): return s32(x) & M64                       # resultado de 32 bits extendido a 64
        def imm(w): v = w & 0xFFFF; return v - 0x10000 if v & 0x8000 else v
        sig = None                                          # destino del salto pendiente (tras el hueco)
        while True:
            if pc == 0xDEAD0000: return
            self.pasos += 1
            if self.pasos > tope: raise RuntimeError('mips: demasiados pasos')
            w = self._u(pc, '<I'); op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; rd = (w >> 11) & 31
            sh = (w >> 6) & 31; fn = w & 63
            ad = (s32(r[rs]) + imm(w)) & 0xFFFFFFFF
            nsig = None; salta_hueco = False
            if w == 0: pass
            elif op == 0:
                if fn == 0x00: r[rd] = sx(r[rt] << sh)                                  # sll
                elif fn == 0x08: nsig = r[rs] & 0xFFFFFFFF                               # jr
                elif fn == 0x21: r[rd] = sx(r[rs] + r[rt])                              # addu
                elif fn == 0x23: r[rd] = sx(r[rs] - r[rt])                              # subu
                elif fn == 0x25: r[rd] = (r[rs] | r[rt]) & M64                          # or
                elif fn == 0x2d: r[rd] = (r[rs] + r[rt]) & M64                          # daddu (move)
                elif fn == 0x2a: r[rd] = 1 if s32(r[rs]) < s32(r[rt]) else 0             # slt
                else: raise RuntimeError('mips: SPECIAL 0x%x en 0x%x' % (fn, pc))
            elif op == 0x09: r[rt] = sx(s32(r[rs]) + imm(w))                            # addiu
            elif op == 0x0a: r[rt] = 1 if s32(r[rs]) < imm(w) else 0                    # slti
            elif op == 0x0c: r[rt] = r[rs] & (w & 0xFFFF)                               # andi
            elif op == 0x0d: r[rt] = r[rs] | (w & 0xFFFF)                               # ori
            elif op == 0x0f: r[rt] = sx((w & 0xFFFF) << 16)                             # lui
            elif op in (0x04, 0x05, 0x14, 0x15):                                        # beq, bne, beql, bnel
                c = (r[rs] == r[rt]) if op in (0x04, 0x14) else (r[rs] != r[rt])
                if c: nsig = (pc + 4 + imm(w) * 4) & 0xFFFFFFFF
                elif op >= 0x14: salta_hueco = True
            elif op == 0x02 or op == 0x03:                                              # j, jal
                if op == 0x03: r[31] = pc + 8
                nsig = (pc & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            elif op == 0x23: r[rt] = sx(self._u(ad, '<I'))                              # lw
            elif op == 0x25: r[rt] = self._u(ad, '<H')                                  # lhu
            elif op == 0x24: r[rt] = self._u(ad, '<B')                                  # lbu
            elif op == 0x37: r[rt] = self._u(ad, '<Q')                                  # ld
            elif op == 0x2b: self.escribe(ad, struct.pack('<I', r[rt] & 0xFFFFFFFF))    # sw
            elif op == 0x29: self.escribe(ad, struct.pack('<H', r[rt] & 0xFFFF))        # sh
            elif op == 0x28: self.escribe(ad, struct.pack('<B', r[rt] & 0xFF))          # sb
            elif op == 0x3f: self.escribe(ad, struct.pack('<Q', r[rt] & M64))           # sd
            elif op in (0x1a, 0x1b):                                                    # ldl, ldr (little endian)
                al = ad & ~7; k = ad & 7; q = self._u(al, '<Q')
                if op == 0x1a:                                                          # ldl: bytes altos
                    m = (M64 << (8 * (7 - k))) & M64; r[rt] = (r[rt] & ~m & M64) | ((q << (8 * (7 - k))) & m)
                else:                                                                   # ldr: bytes bajos
                    m = M64 >> (8 * k); r[rt] = (r[rt] & ~m & M64) | ((q >> (8 * k)) & m)
            elif op in (0x2c, 0x2d):                                                    # sdl, sdr
                al = ad & ~7; k = ad & 7; q = self._u(al, '<Q'); v = r[rt]
                if op == 0x2c: m = M64 >> (8 * (7 - k)); q = (q & ~m & M64) | ((v >> (8 * (7 - k))) & m)
                else: m = (M64 << (8 * k)) & M64; q = (q & ~m & M64) | ((v << (8 * k)) & m)
                self.escribe(al, struct.pack('<Q', q))
            elif op == 0x01 and rt in (0, 1):                                            # bltz, bgez
                c = s32(r[rs]) < 0 if rt == 0 else s32(r[rs]) >= 0
                if c: nsig = (pc + 4 + imm(w) * 4) & 0xFFFFFFFF
            elif op in (0x06, 0x07):                                                    # blez, bgtz
                c = s32(r[rs]) <= 0 if op == 0x06 else s32(r[rs]) > 0
                if c: nsig = (pc + 4 + imm(w) * 4) & 0xFFFFFFFF
            else: raise RuntimeError('mips: op 0x%x en 0x%x' % (op, pc))
            r[0] = 0
            if sig is not None: pc = sig; sig = None
            elif salta_hueco: pc += 8
            else: pc += 4
            if nsig is not None: sig = nsig
