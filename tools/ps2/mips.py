"""Interprete minimo del R5900 (MIPS III + ld/sd de 64 bits) para ejecutar en la conversion las rutinas de
inicializacion del ejecutable que rellenan tablas en la BSS (lo que el juego calcula al correr y no esta en el fichero).
Solo las instrucciones enteras que usan esas rutinas (copias de bloques, bucles, multiplicaciones y divisiones, llamadas): con
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
        self.r = [0] * 32; self.hi = self.lo = self.hi1 = self.lo1 = 0
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
                elif fn == 0x2b: r[rd] = 1 if (r[rs] & M64) < (r[rt] & M64) else 0      # sltu
                elif fn == 0x02: r[rd] = sx((r[rt] & 0xFFFFFFFF) >> sh)                  # srl
                elif fn == 0x03: r[rd] = sx(s32(r[rt]) >> sh)                           # sra
                elif fn == 0x04: r[rd] = sx(r[rt] << (r[rs] & 31))                      # sllv
                elif fn == 0x06: r[rd] = sx((r[rt] & 0xFFFFFFFF) >> (r[rs] & 31))       # srlv
                elif fn == 0x07: r[rd] = sx(s32(r[rt]) >> (r[rs] & 31))                 # srav
                elif fn == 0x0a:                                                         # movz
                    if r[rt] == 0: r[rd] = r[rs]
                elif fn == 0x0b:                                                         # movn
                    if r[rt] != 0: r[rd] = r[rs]
                elif fn == 0x10: r[rd] = self.hi                                        # mfhi
                elif fn == 0x12: r[rd] = self.lo                                        # mflo
                elif fn == 0x11: self.hi = r[rs]                                        # mthi
                elif fn == 0x13: self.lo = r[rs]                                        # mtlo
                elif fn in (0x18, 0x19):                                                 # mult, multu (EE: rd = lo)
                    a, b = (s32(r[rs]), s32(r[rt])) if fn == 0x18 else (r[rs] & 0xFFFFFFFF, r[rt] & 0xFFFFFFFF)
                    p_ = a * b; self.lo = sx(p_); self.hi = sx(p_ >> 32); r[rd] = self.lo
                elif fn in (0x1a, 0x1b):                                                 # div, divu
                    a, b = (s32(r[rs]), s32(r[rt])) if fn == 0x1a else (r[rs] & 0xFFFFFFFF, r[rt] & 0xFFFFFFFF)
                    if b:
                        q = abs(a) // abs(b); q = q if (a >= 0) == (b >= 0) else -q
                        self.lo = sx(q); self.hi = sx(a - q * b)
                elif fn == 0x24: r[rd] = r[rs] & r[rt]                                  # and
                elif fn == 0x26: r[rd] = (r[rs] ^ r[rt]) & M64                          # xor
                elif fn == 0x27: r[rd] = ~(r[rs] | r[rt]) & M64                         # nor
                elif fn == 0x2f: r[rd] = (r[rs] - r[rt]) & M64                          # dsubu
                elif fn == 0x38: r[rd] = (r[rt] << sh) & M64                            # dsll
                elif fn == 0x3a: r[rd] = (r[rt] & M64) >> sh                            # dsrl
                elif fn == 0x3b: v_ = r[rt] - (1 << 64) if r[rt] >> 63 else r[rt]; r[rd] = (v_ >> sh) & M64   # dsra
                elif fn == 0x3c: r[rd] = (r[rt] << (sh + 32)) & M64                     # dsll32
                elif fn == 0x3e: r[rd] = (r[rt] & M64) >> (sh + 32)                     # dsrl32
                elif fn == 0x3f: v_ = r[rt] - (1 << 64) if r[rt] >> 63 else r[rt]; r[rd] = (v_ >> (sh + 32)) & M64   # dsra32
                elif fn == 0x09: r[rd] = pc + 8; nsig = r[rs] & 0xFFFFFFFF               # jalr
                else: raise RuntimeError('mips: SPECIAL 0x%x en 0x%x' % (fn, pc))
            elif op == 0x09: r[rt] = sx(s32(r[rs]) + imm(w))                            # addiu
            elif op == 0x0a: r[rt] = 1 if s32(r[rs]) < imm(w) else 0                    # slti
            elif op == 0x1c and fn in (0x00, 0x20, 0x18, 0x10, 0x12, 0x11, 0x13):      # MMI: madd, madd1, mult1, m*hi1/lo1
                u1 = fn in (0x20, 0x18, 0x10, 0x12, 0x11, 0x13)
                if fn in (0x10, 0x12): r[rd] = (self.hi1 if fn == 0x10 else self.lo1) if u1 else 0
                elif fn == 0x11: self.hi1 = r[rs]
                elif fn == 0x13: self.lo1 = r[rs]
                else:
                    hi, lo = (self.hi1, self.lo1) if u1 else (self.hi, self.lo)
                    acc = 0 if fn == 0x18 else (((hi & 0xFFFFFFFF) << 32) | (lo & 0xFFFFFFFF))
                    if acc >> 63: acc -= 1 << 64
                    acc = (acc + s32(r[rs]) * s32(r[rt])) & M64
                    lo, hi = sx(acc), sx(acc >> 32)
                    if u1: self.hi1, self.lo1 = hi, lo
                    else: self.hi, self.lo = hi, lo
                    r[rd] = lo
            elif op == 0x0b: r[rt] = 1 if (r[rs] & M64) < (imm(w) & M64) else 0         # sltiu
            elif op == 0x0e: r[rt] = r[rs] ^ (w & 0xFFFF)                               # xori
            elif op == 0x19: r[rt] = (r[rs] + imm(w)) & M64                              # daddiu
            elif op == 0x20: v_ = self._u(ad, '<B'); r[rt] = sx(v_ - 256 if v_ & 0x80 else v_)     # lb
            elif op == 0x21: v_ = self._u(ad, '<H'); r[rt] = sx(v_ - 65536 if v_ & 0x8000 else v_)   # lh
            elif op in (0x16, 0x17):                                                    # blezl, bgtzl
                c = s32(r[rs]) <= 0 if op == 0x16 else s32(r[rs]) > 0
                if c: nsig = (pc + 4 + imm(w) * 4) & 0xFFFFFFFF
                else: salta_hueco = True
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
            elif op == 0x01 and rt in (0, 1, 2, 3):                                      # bltz, bgez, bltzl, bgezl
                c = s32(r[rs]) < 0 if rt in (0, 2) else s32(r[rs]) >= 0
                if c: nsig = (pc + 4 + imm(w) * 4) & 0xFFFFFFFF
                elif rt >= 2: salta_hueco = True
            elif op in (0x06, 0x07):                                                    # blez, bgtz
                c = s32(r[rs]) <= 0 if op == 0x06 else s32(r[rs]) > 0
                if c: nsig = (pc + 4 + imm(w) * 4) & 0xFFFFFFFF
            else: raise RuntimeError('mips: op 0x%x en 0x%x' % (op, pc))
            r[0] = 0
            if sig is not None: pc = sig; sig = None
            elif salta_hueco: pc += 8
            else: pc += 4
            if nsig is not None: sig = nsig
