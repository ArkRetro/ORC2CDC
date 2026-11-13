/* El final de recreativa de OUTRUN2SP: modo 24 del original tras la meta (19 -> 33 -> 24 -> 25). SLES-53998 con la
   decompilacion del PC (race_ending.cpp, race_autoscene.cpp), comprobado en el MIPS:
   - Modo 24: tabla de modos 0x3f8e50 = 0x1ba4d8 init / 0x1ba5f0 control / 0x1ba6a0 salida (PC 49A660 / 49A710 /
     4527A0). La init (0x19b6d0 = 452B10) pone el temporizador [0x5a2020] = 8 y crea la animacion de carga del banco 0x33
     (330001 / 330000; la Dreamcast lee de un tiron: no sale, como en el modo 25). Al acabar el temporizador, 0x19bae0
     (4527F0) elige el final: columna = (450380(8) - 0xA) % 5 (+ 5 con los presets 0 / 2), id = [0x3e9048 + columna x
     4] en la variante 1, juego de movimientos 1; pide los recursos (0x57, 0xD0, 0xBA, 0xBB, el coche, el sitio de
     [0x3e7780 + columna x 16], 0xCC, el juego de movimientos y los 15 de la fila) y el grupo de movimientos de la fila
     (+0x44, 4F2020).
   - Estados (0x19b740 = 4524E0, [0x556f18]): 0 espera object_db (0x1cd9f8); 1 espera las cargas y que no suene 0xA3
     (0x11ed40); 2 los sucesos de la escena (8 / 0x2D el coche si la fila +0x48, 0x186 / 0x44, 0x187 / 0x45 luces,
     0x18d / 0x1B el sitio (44B890: el modelo END_*_STAGE en el origen), 0x181 / 0xD la camara 485470, 0x17f / 0x17,
     0x183 / 0x34, 6 / 0x19) y la escena 2D [0x35c5c8 + id x 4] en la capa 7, modo 1 (0x114b68: los creditos del banco
     0x43 STAFFROLL_CVT); 3 el guion AUTOSCENE de la fila (0x28d878 = 4B5F60); 4 espera a que corra (0x28d908 = 3): luces
     4523E0 y 4AFBA0; 5 se acaba cuando el guion ha acabado (estado 5 o 0) y la escena 2D esta en 3 (0x115230 = 428880),
     o con A / START (0x210c58); 6 suspende los sucesos y pide el modo 0x19 (0x1d en la variante 5).
   - Salida (0x19ba90): 0x100138(0) para la musica (la de la meta, que sigue sonando desde el modo 19: el 24 no pone
     otra), suelta la escena 2D.
   - AUTOSCENE (PS2 0x28b000..0x28d9xx; PC 4B5150..4B6CD0): reloj 0x28baa8 (4B6BD0): velocidad del canal 0 del registro
     de tipo 5 (1 sin el), acumulado += velocidad, cuadro = (int) acumulado, tiempo = acumulado x 1/60 (0x3c888889),
     fin al pasar de cuadros / ritmo de la cabecera (+4: {1, cuadros, ritmo}); camara 0x28bce0 (4B5180): ojo canales
     0..2, mira 3..5, balanceo 6, FOV = 2 atan(([pista] x 0,5) / (canal 7 x 0,03937)) (0x3f00, 0x3d214270) a 483E10
     (0x17b540: +E0, +EC, +130, +A4); filas 0x28c198 (4B68F0): sucesos y nodos (4B5670: 9 valores de los canales 0..8,
     el 9 la visibilidad > 0,5); dibujo 4B5750: T(+10) Rz Ry Rx(+1C) por nodo (HAKO_HUTA con (-0,045, 0,056, 0) antes),
     BD0000 la matriz de la carroceria (car+B0), BD0001 / 2 las puertas (+164 / +166 del giro y), BD0007 car+32 (giro
     y de la pinza delantera izquierda), BD0008 / C / 10 / 14 car+40..46 (giro x de cada rueda), los objetos (405360)
     con su token; los hijos de las piezas del coche siempre, los demas solo si el nodo se ve.
   SIN PORTAR (docs/PARIDAD.md, OUTRUN2SP: Final de recreativa): la pareja del final (registros de tipo 2: CHR_MAL /
   CHR_FAL con 487C30 / 487D10, los movimientos del grupo de la fila (sucesos 1: 4F2280 / 4F1D30 / 4F1E30 / 4ED890 /
   4F1E60), las caras 514830 (con la mezcla 405450), las manos 514800 y los ojos BD001C..F (514880)), el humo KEM
   (BD0020, 4208A0), los sonidos de los sucesos 2 / 4 (4249F0), el fundido 4AF520 (canal 1 del registro de tipo 5), las
   luces de 4523E0 (5A6AB8; los modelos llevan la luz del conversor en los vertices), el fondo 4522D0 de los presets
   0 / 2 (columnas 5..9: [0x3e7820 + columna x 24]), las luces de freno del coche del final (car+330) y el bloqueo de los
   sucesos 0x10000 del final 6 a partir del cuadro 0x460. */
#include <kos.h>
#include <string.h>
#include <math.h>
#include "final.h"
#include "../comun/gd.h"
#include "../comun/sprani.h"
#include "../comun/or2_fisica.h"

#define N_IDS 0x26
#define N_AS 0x38
#define K_ANG 10430.3779f                                     /* 0x6282c0: 65536 / 2 pi (cvttss2si) */

typedef float mat_t[4][4];
static const uint8_t *F, *G;                                  /* FINAL.DAT y el guion de la escena */
static uint32_t Fn, Gn;
static spr_banco_t *sb;
static int activo, estado, temp, id, col, h2d = -1, coche, en_curso;
static float acc, ahora, fin_t;                              /* +C, +4, +8 del trabajo de AUTOSCENE */
static int cuadro;                                           /* +10 */
static uint32_t cam_e, tipo5;                                /* +64 (pista de la camara) y +74 (registro de tipo 5) */
static uint32_t nfil;                                        /* filas: una por entrada del guion */
#define MAX_FIL 64
static struct { uint32_t fl, tipo, p1, p2, suc; } fil[MAX_FIL];
static final_vista_t V;

static uint32_t U(uint32_t o) { uint32_t v; if (o + 4 > Gn) return 0; memcpy(&v, G + o, 4); return v; }
static float Fl(uint32_t o) { float v; if (o + 4 > Gn) return 0; memcpy(&v, G + o, 4); return v; }
static void Pf(uint32_t o, float v) { if (o + 4 <= Gn) memcpy((uint8_t *)G + o, &v, 4); }
static void Pu(uint32_t o, uint32_t v) { if (o + 4 <= Gn) memcpy((uint8_t *)G + o, &v, 4); }
static uint32_t FU(uint32_t o) { uint32_t v; if (o + 4 > Fn) return 0; memcpy(&v, F + o, 4); return v; }

/* 513650 sobre el par {cuenta (16 bits), claves} del guion */
static int canal(uint32_t ch, float t, float *v)
{
    int n = (int16_t)(U(ch) & 0xffff); uint32_t k = U(ch + 4);
    if (!U(ch) || n <= 0 || k + 16u * n > Gn || (k & 3)) return 0;
    *v = or2_canal((const float *)(G + k), n, t);
    return 1;
}

/* pila de matrices del PC (filas: los ejes; las operaciones van en el marco local, como robot.c) */
static void m_id(mat_t m) { memset(m, 0, sizeof(mat_t)); m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1; }
static void op_tras(mat_t m, const float *v) { int j; for (j = 0; j < 4; j++) m[3][j] += v[0] * m[0][j] + v[1] * m[1][j] + v[2] * m[2][j]; }
static void op_rot(mat_t m, int a, int b, float ang)
{
    float c = cosf(ang), s = sinf(ang), x; int j;
    for (j = 0; j < 4; j++) { x = m[a][j]; m[a][j] = c * x + s * m[b][j]; m[b][j] = -s * x + c * m[b][j]; }
}
static void op_zyx(mat_t m, const float *r) { op_rot(m, 0, 1, r[2]); op_rot(m, 2, 0, r[1]); op_rot(m, 1, 2, r[0]); }   /* 40A4E0 */

int final_activo(void) { return activo; }
const final_vista_t *final_vista(void) { return &V; }

int final_columna(int variante, int preset, int meta)
{
    int c;
    if (variante != 1 || meta < 0) return -1;                /* 0 / 2 (Contrarreloj, INFARTO) y las demas: SIN PORTAR */
    c = meta % 5;
    if (preset == 0 || preset == 2) c += 5;
    return c;
}

/* 4B5180 */
static void camara(void)
{
    float v, a, e;
    int k;
    if (!cam_e) return;
    for (k = 0; k < 6; k++) if (canal(cam_e + 4 + 8 * k, ahora, &v)) { if (k < 3) V.ojo[k] = v; else V.mira[k - 3] = v; }
    if (canal(cam_e + 0x3c, ahora, &a) && a != 0) {
        e = Fl(cam_e) * 0.5f;
        V.fov = 2.0f * atan2f(e / (a * 0.0393700004f), 1.0f);
    }
    if (canal(cam_e + 0x34, ahora, &v)) V.balanceo = v;
}

/* 4B5670: canales de los nodos (sin las ranuras de matriz: solo las piden los robots, SIN PORTAR) */
static void nodo_cuadro(uint32_t n, int prof)
{
    uint32_t ch = U(n + 0x38), i, nk = U(n + 0x3c), hj = U(n + 0x40); float v; int k;
    if (prof > 16) return;
    if (ch) for (k = 0; k < 10; k++) {
        if (!canal(ch + 8 * k, ahora, &v)) continue;
        if (k == 9) Pu(n, v > 0.5f ? U(n) | 0x100 : U(n) & ~0x100u);
        else Pf(n + 0x10 + 4 * k, v);
    }
    if (!(U(n) & 0x100)) return;
    for (i = 0; i < nk && i < 256; i++) nodo_cuadro(hj + 0x44 * i, prof + 1);
}

static int16_t palabra(float v) { return (int16_t)(int32_t)(v * K_ANG); }

/* 4B5750 */
static void nodo_dibuja(uint32_t n, const mat_t P, int prof)
{
    mat_t M; float r[3], t[3]; uint32_t tok = U(n + 8), nm = U(n + 0xc), i, nk = U(n + 0x3c), hj = U(n + 0x40);
    int vis = (U(n) & 0x100) != 0, hijos = 0;
    if (prof > 16) return;
    memcpy(M, P, sizeof M);
    if (tok != 0xffffffffu && vis && nm && nm + 10 <= Gn && !memcmp(G + nm, "HAKO_HUTA", 10)) {
        static const float lid[3] = { -0.045f, 0.056f, 0.0f }; op_tras(M, lid);   /* 0xbd3851ec, 0x3d65c91d */
    }
    for (i = 0; i < 3; i++) { t[i] = Fl(n + 0x10 + 4 * i); r[i] = Fl(n + 0x1c + 4 * i); }
    op_tras(M, t); op_zyx(M, r);
    if (tok >= 0xbd0000u && tok <= 0xbd001bu) {
        switch (tok - 0xbd0000u) {
        case 0: memcpy(V.B0, M, sizeof M); break;
        case 7: V.s32 = palabra(Fl(n + 0x20)); break;
        case 8: V.giro[0] = palabra(Fl(n + 0x1c)); break;
        case 0xc: V.giro[1] = palabra(Fl(n + 0x1c)); break;
        case 0x10: V.giro[2] = palabra(Fl(n + 0x1c)); break;
        case 0x14: V.giro[3] = palabra(Fl(n + 0x1c)); break;
        default: break;                                      /* puertas +164 / +166, luces car+330: SIN PORTAR */
        }
        hijos = 1;
    } else if (tok >= 0xbd001cu && tok <= 0xbd0020u) {
        /* ojos (514880) y humo (4208A0): SIN PORTAR */
    } else if (vis) {
        if (tok >> 16 == 0xd0 && V.nprop < FINAL_PROPS) { V.prop[V.nprop].reg = (uint16_t)(tok & 0xffff); memcpy(V.prop[V.nprop].M, M, sizeof M); V.nprop++; }
        hijos = 1;
    }
    if (hijos) for (i = 0; i < nk && i < 256; i++) nodo_dibuja(hj + 0x44 * i, (const float (*)[4])M, prof + 1);
}

/* 4B5C40: los sucesos de una fila hasta el cuadro (solo el 10, quitar la fila, en las de nodos) */
static void sucesos(int f)
{
    uint32_t s = fil[f].suc;
    if (!fil[f].p2 || !s) return;
    while (s + 12 <= Gn && U(s) != 0xffffffffu && !((int32_t)U(s) > cuadro)) {
        if (U(s + 4) == 10) fil[f].fl &= ~1u;
        if (!U(s + 8)) { s = 0; break; }
        s += U(s + 8) * 4;
    }
    fil[f].suc = s;
}

static void reloj(void)                                       /* 0x28baa8 (4B6BD0) */
{
    float vel = 1.0f, v; uint32_t f;
    if (tipo5 && canal(tipo5, ahora, &v)) vel = v;           /* +7C (el fundido, +78 = canal 1 -> 4AF520: SIN PORTAR) */
    acc += vel; cuadro = (int)acc; ahora = acc * 0.0166666675f;
    if (ahora > fin_t) { ahora = fin_t; en_curso = 0; }
    camara();
    for (f = 0; f < nfil; f++) {
        if (!(fil[f].fl & 1)) continue;
        sucesos((int)f);
        if (fil[f].tipo == 1) nodo_cuadro(fil[f].p1, 0);
    }
    /* dibujo (4B69C0): las filas de nodos sobre la identidad */
    {   mat_t I; m_id(I); V.nprop = 0;
        for (f = 0; f < nfil; f++) if ((fil[f].fl & 1) && fil[f].tipo == 1) nodo_dibuja(fil[f].p1, (const float (*)[4])I, 0); }
}

static int guion_empieza(uint32_t esc)                        /* 4B6A70 / 4B66B0 */
{
    uint32_t o = FU(4 + 4 + 16 * N_IDS + 40 + 80 + 4 + 8 * esc), n = FU(4 + 4 + 16 * N_IDS + 40 + 80 + 4 + 8 * esc + 4), i, t;
    if (esc >= N_AS || !o || !n || o + n > Fn) return -1;
    G = F + o; Gn = n;
    nfil = U(8); if (nfil > MAX_FIL) nfil = MAX_FIL;
    cam_e = tipo5 = 0;
    for (i = 0; i < nfil; i++) {
        uint32_t e = U(0xc) + 12 * i;
        fil[i].fl = 1; fil[i].tipo = U(e); fil[i].p1 = U(e + 4); fil[i].p2 = fil[i].suc = U(e + 8);
        if (fil[i].tipo == 0) cam_e = fil[i].p1;
        if (fil[i].tipo == 5) tipo5 = fil[i].p1;
        if (fil[i].tipo == 6) fil[i].fl = 0x81;
    }
    /* 4B6510: +0 |= 0x300 en todos los nodos (visibles) */
    {   uint32_t pila[512], np = 0;
        for (i = 0; i < nfil; i++) if (fil[i].tipo == 1 && np < 512) pila[np++] = fil[i].p1;
        while (np) {
            uint32_t q = pila[--np], k;
            Pu(q, U(q) | 0x300);
            for (k = 0; k < U(q + 0x3c) && np < 512; k++) pila[np++] = U(q + 0x40) + 0x44 * k;
        }
    }
    t = U(4);
    fin_t = (float)(int32_t)U(t + 4) / Fl(t + 8);
    acc = ahora = 0; cuadro = 0; en_curso = 1;
    m_id(V.B0); V.s32 = 0; memset(V.giro, 0, sizeof V.giro);
    V.fov = 1.0f; V.balanceo = 0;
    camara();                                                 /* (4B66B0: la camara al empezar) */
    return 0;
}

int final_entra(int c, uint8_t *ram, uint32_t cap, uint8_t *staff, uint32_t scap)
{
    uint32_t sz, n;
    activo = 0; sb = NULL; h2d = -1;
    if (c < 0 || c > 9 || !ram) return -1;
    if (gd_carga_en("FINAL.DAT", ram, cap, &sz) || sz < 8 || memcmp(ram, "FNL1", 4)) {
        dbglog(DBG_ERROR, "FINAL: sin FINAL.DAT (%uK de RAM)\n", (unsigned)(cap >> 10)); return -2;
    }
    F = ram; Fn = sz; n = FU(4);
    if (n != N_IDS) return -3;
    col = c; id = (int)FU(8 + 16 * N_IDS + 4 * c);          /* [0x3e9048 + columna x 4] */
    if (id < 0 || id >= N_IDS) return -4;
    coche = FU(8 + 16 * id + 8) != 0;
    sb = staff ? spr_abre_en("STAFF.DAT", staff, scap) : NULL;
    if (!sb) dbglog(DBG_ERROR, "FINAL: sin STAFF.DAT (%uK de RAM)\n", (unsigned)(scap >> 10));
    spr_suelta_todas();
    estado = 0; temp = 8;                                     /* 0x19b6d0: [0x5a2020] = 8 */
    memset(&V, 0, sizeof V); m_id(V.B0); V.fov = 1.0f; V.coche = coche;
    activo = 1;
    dbglog(DBG_INFO, "FINAL: columna %d, final %d, escena AUTOSCENE 0x%x, grupo 0x%x, coche %d, 2D 0x%x\n", col, id,
           (unsigned)FU(8 + 16 * id), (unsigned)FU(8 + 16 * id + 4), coche, (unsigned)FU(8 + 16 * id + 12));
    return 0;
}

int final_paso(uint32_t nuevo)
{
    if (!activo) return 0;
    if (temp) {                                               /* 0x1ba5f0: el temporizador y despues 0x19bae0 */
        if (--temp == 0) estado = 2;                          /* (estados 0 / 1: object_db y las cargas, ya hechas) */
        return 1;
    }
    switch (estado) {
    case 2:
        if (sb) h2d = spr_crea_entera(FU(8 + 16 * id + 12), 7, 1);   /* 0x114b68(token, 7, 1) */
        estado = 3;
        break;
    case 3:
        if (guion_empieza(FU(8 + 16 * id))) { dbglog(DBG_ERROR, "FINAL: sin el guion de la escena\n"); en_curso = 0; }
        estado = 4;
        /* fallthrough */
    case 4:                                                   /* 4B5FD0 == 3 (el guion corre): luces 4523E0 SIN PORTAR */
        estado = 5;
        break;
    case 5:
        if (en_curso) reloj();
        if ((!en_curso && (h2d < 0 || spr_estado(h2d) == 3)) || (nuevo & (CONT_A | CONT_START))) estado = 6;
        break;
    default:
        return 0;
    }
    if (sb) spr_paso();
    return estado != 6;
}

void final_dibuja2d(void)
{
    if (activo && sb) spr_dibuja(7, 8, 120.0f, 0);
}

void final_sale(void)
{
    if (sb) { spr_suelta_todas(); pvr_wait_ready(); spr_cierra(sb); sb = NULL; }
    h2d = -1; activo = 0; G = F = NULL; Gn = Fn = 0;
}
