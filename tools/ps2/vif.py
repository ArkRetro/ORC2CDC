import struct
# VIF: tamano en palabras de los datos de cada UNPACK (vn, vl)
def unpack_words(cmd,num):
    vn=(cmd>>2)&3; vl=cmd&3
    bits=(32,16,8,5)[vl]*(vn+1)
    return (bits*num+31)//32
def recorre(m,p,fin):
    """paquetes DMA RET/CNT desde p: lista de (tipo, num, addr) de los UNPACK y bytes recorridos"""
    out=[]; q=p
    while q<fin:
        tag=struct.unpack('<I',m[q:q+4])[0]; qwc=tag&0xFFFF; tid=(tag>>28)&7
        d=q+8; e=q+16+qwc*16   # los dos ultimos words del tag pueden llevar codigos VIF
        x=d
        while x<e:
            v=struct.unpack('<I',m[x:x+4])[0]; cmd=(v>>24)&0x7f; num=(v>>16)&0xff; imm=v&0xffff; x+=4
            if cmd>=0x60:
                n=num or 256; out.append((cmd&0x6f,n,imm)); x+=4*unpack_words(cmd,n)
            elif cmd in (0x20,): x+=4
            elif cmd in (0x30,0x31): x+=16
            elif cmd in (0x4a,): x+=8*(num or 256)
            elif cmd in (0x50,0x51): x+=16*(imm or 65536)
        q=e
        if tid in (6,7): break     # RET / END
    return out,q-p
def datos(m,p,fin):
    """paquetes desde p (hasta RET): por paquete, dict tipo->[(direccion, array (num, comps))] de los UNPACK.
    Aplica STROW/STMOD (modo 1 suma la fila; modo 2 acumula: las UV van en diferencias)."""
    import numpy as np
    res=[]; q=p; fila=np.zeros(4,np.int64); modo=0
    while q<fin:
        tag=struct.unpack('<I',m[q:q+4])[0]; qwc=tag&0xFFFF; tid=(tag>>28)&7
        x=q+8; e=q+16+qwc*16; pk={}
        while x<e:
            v=struct.unpack('<I',m[x:x+4])[0]; cmd=(v>>24)&0x7f; num=(v>>16)&0xff; imm=v&0xffff; x+=4
            if cmd>=0x60:
                n=num or 256; vn=((cmd>>2)&3)+1; vl=cmd&3; nw=unpack_words(cmd,n)
                raw=m[x:x+4*nw]; x+=4*nw; usn=(imm>>14)&1
                if vl==0: a=np.frombuffer(raw,'<f4' if (cmd&0x6f) in (0x68,0x6c) else '<i4')[:n*vn].reshape(n,vn)
                elif vl==1: a=np.frombuffer(raw,'<u2' if usn else '<i2')[:n*vn].reshape(n,vn)
                elif vl==2: a=np.frombuffer(raw,'u1' if usn else 'i1')[:n*vn].reshape(n,vn)
                else: a=None
                if a is not None and modo and (vl or (cmd&0x6f) not in (0x68,0x6c)):
                    a=a.astype(np.int64)
                    if modo==1: a=a+fila[:vn]
                    else: a=fila[:vn]+np.cumsum(a,0); fila[:vn]=a[-1]
                pk.setdefault(cmd&0x6f,[]).append((imm&0x3ff,a)); pk.setdefault('orden',[]).append((cmd&0x6f,a))
            elif cmd==0x05: modo=imm&3
            elif cmd==0x20: x+=4
            elif cmd==0x30: fila[:]=struct.unpack('<4i',m[x:x+16]); x+=16
            elif cmd==0x31: x+=16
            elif cmd==0x4a: x+=8*(num or 256)
            elif cmd in (0x50,0x51): x+=16*(imm or 65536)
        res.append(pk); q=e
        if tid in (6,7): break
    return res

def no_dibuja(P):
    """bandera de no dibujar de cada vertice de una tira (V3-32 de posiciones, float32): el bit 1 de la palabra de la y.
    Asi lo hace el microcodigo del VU1 de las mallas (SLES-53998, tabla de programas en 0x3592b0, p. ej. el de la
    clave 0x10200 en 0x31aae0): por vertice MTIR vi7, y (los 16 bits bajos de la y), IAND vi7, vi7, 2;
    FCAND vi1, 0x3ffff (la CLIP de los tres ultimos vertices: alguno fuera del volumen de recorte);
    IADDIU vi1, vi1, 0x7fff; IADD vi1, vi1, vi7; MFIR.w -> la palabra W del XYZ2 que va al GS, cuyo bit 15 es el
    ADC: el GS no dibuja el triangulo que cierra ese vertice. Los dos primeros de cada tira la llevan siempre."""
    import numpy as np
    return ((np.ascontiguousarray(P[:, 1], dtype='<f4').view('<u4') >> 1) & 1).astype(bool)

def triangulos_dibujados(P):
    """triangulo j (vertices j, j+1, j+2) dibujado por el original: sin la bandera en su ultimo vertice (no_dibuja)"""
    return ~no_dibuja(P)[2:]

def tiras(pk):
    """tiras de un paquete: cada una empieza por su GIFtag (UNPACK 0x6c); dict tipo -> array"""
    res=[]
    for t,a in pk.get('orden',[]):
        if t==0x6c or not res: res.append({})
        res[-1].setdefault(t,a)
    return [r for r in res if 0x68 in r]
