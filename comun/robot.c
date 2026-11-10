/* robot.c: el piloto y la acompanante del coche del jugador, como los sucesos ROB01 / ROB02 del original.

   Motor de movimiento (RobMotion): portado de la decompilacion del PC (platform/rob_motion_engine.cpp, rob_motion.cpp)
   y contrastado con SLES-53998 (SetMotion 0x207fa8, SetFrame 0x2081f8, Calc 0x207ed0, SetLoop 0x208418,
   SetMotionConnect 0x2080a0, IsMotionEnd 0x208420):
   - huesos (bone.bin, PC 522DD0): tipo 0/1 claves de traslacion, giro y escala; 2 la raiz de una cadena con IK (51A770:
     2 de dos huesos con la ley del coseno, 1 apuntar); 3 matriz fija (la pone la IK); 4 claves de giro con la
     traslacion de reposo. Arbol 51B420: M = escala Rx Ry Rz T(t) padre (vectores fila; la pila del PC multiplica por
     la izquierda) y "giro unidad" quita el giro acumulado antes de girar.
   - canales (520750 / 522F70 / 522EE0): una tabla de (hueso, mascara de 9 bits) y las claves seguidas de cada canal:
     u16 {tiempo en 1/60 s (13 bits), bit 13 primera/constante, tipo en 14-15: 0 todo 0, 1 valor, 2 valor y tangente,
     3 valor y dos tangentes}, valores en coma flotante de 16 bits (522AA0); Hermite (522C20) leyendo hacia delante y
     volviendo al principio si el tiempo retrocede.
   - mezcla (SetMotionConnect 4F2320 / 51B3F0 / 519DD0): se guarda la postura y durante n cuadros se interpola hacia el
     movimiento nuevo (traslacion y escala lineales; giro: matrices interpoladas y reortonormalizadas, 519970, y
     vueltas a angulos, 519CE0). Las variables del motor 85B2B8.. que el ejecutable no escribe valen 0: el hueso 0
     mezcla su y dos veces (51A3A0), como el original.
   Logica (que movimiento y cuando), solo lo de la variante 1 (el modo OutRun):
   - Piloto (488DD0 / PS2 0x201c40): el movimiento de conducir de su modelo (tabla MODC = PS2 0x40e638) con el cuadro
     sacado del volante: (car+204 + 0x7FFF) / 65535 x 168 (PS2 0x201d00: 0x477fff00 y 0x43280000; el PC usa otras
     constantes); en los coches con la tabla MIRA (PS2 0x40e728: descapotables) tras 121 cuadros con el cuadro entre 83
     y 87 (volante al centro) apoya el codo en la puerta (ida, mano en la puerta guiada por el volante, vuelta); en un
     choque, al recuperarse (car+2F8 > 0) uno de CHQ6 / CHQ7 (PS2 0x40e918 / 0x40e938 por la tabla de estados
     0x40e230); TIME OVER 0x60000. Empieza en el estado 8 (0x201bb8) hasta el final de la cuenta atras.
   - Acompanante (489A90 / PS2 0x202de0): empieza con 0x25000c (FA05) en bucle; quieta, a los 181 cuadros una de la
     lista de aburrida (por turno); en el choque 0x6005f (estados 1, 2 y 5) y al recuperarse una de aterrizar o de
     salto (listas por turno: las mismas cuentas que el piloto usa para elegir el suyo); fuera del asfalto (car+248 &
     0x10F407C) 0x220007 o 0x230003; giro brusco a mas de 100 km/h (|car+4E| entre 0x2000 y 0x4000) solo en los
     abiertos; GOAL 0x60049 (cerrados 0x25000c) y TIME OVER 0x60036 en bucle; al acabar cada reaccion vuelve a
     0x6004b (sentada) en bucle. Listas LISA / LISC (PS2 0x40eb28 / 0x40eb68 segun CERR, 45B380 = PS2 0x1aa3a0).
   - Manos (487940 / 5148D0 / 5148F0): el puno si el movimiento esta en la lista AGAR (PS2 0x36f658), si no la abierta.
   - Meta del piloto (estado 9 con 44FF10; PS2 0x20250c..0x2027cc, PC 488DD0 caso 2): fila del modelo (METR =
     0x40e9b8) y tabla segun 44DC50 de la clave (0x1B 0x40e978, 0x1A / 0x1C 0x40e998, si no 0x40e958): dos registros
     {id, bucle, fin} (METE); de cada uno: id una vez (o nada si es -1), al acabar (o 0xF0 / 0x168 cuadros) el de bucle
     en bucle, y a los 0xF0 / 0x168 cuadros el de fin (si no es -1); tras el primero espera 0x78 cuadros o su fin.
     Sin METR / METE (CHR.DAT antiguo) sigue como antes (sin secuencia).
   No portado (PENDIENTE, sin dato en la demo): el cambio de area (44BCE0: en el modo OutRun las tablas del piloto
   valen -1), asustada y trompo (car+4 & 0x1000 / 0x2000, car+0xC), choque de car+4 & 0x30000, el pelo de 4F1E80 y la
   escala de la luz car+0x58 (487880). */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "robot.h"

#define SEXAG (1.0f / 60.0f)
typedef float mat_t[4][4];

typedef struct { uint8_t tipo, ik, unidad, nh, hijos[6], r[2]; float reposo[9]; } rhueso_t;   /* 48 bytes */
typedef struct { uint32_t id; uint16_t cuadros, banderas; uint32_t ocan, ncan, ocla, ncla; } rmov_t;
typedef struct { uint32_t kind, nh, oh, ninv, oinv, mano_i, mano_d, asiento; } rper_t;

typedef struct { float k0[4], k1[4]; uint16_t keys, left, stream, cursor; float rest; } canal_t;   /* 44 bytes */
typedef struct {
    const rhueso_t *h; int n; const mat_t *inv;
    canal_t can[ROBOT_HUESOS][9];
    mat_t M[ROBOT_HUESOS];
    float T[ROBOT_HUESOS][3], R[ROBOT_HUESOS][3], S[ROBOT_HUESOS][3], ikt[ROBOT_HUESOS][3];
    float sT[ROBOT_HUESOS][3], sR[ROBOT_HUESOS][3], sS[ROBOT_HUESOS][3], sik[ROBOT_HUESOS][3];
    /* objeto RobMotion (82F5F0 + n x 0xA4): +04 id, +08 banderas, +0C, +10 cuadro, +14 fin, +1C, +18 velocidad,
       +20 / +24 ids de la mezcla, +28 / +2C tiempo, +30 cuadro, +38 largo, +9C bucles */
    const rmov_t *mov; const uint8_t *cla; uint32_t id, fl, c_ant, c_id; int32_t bucles; uint16_t bflags;
    float f0c, f10, f14, f1c, vel, c_t28, c_t2c, c_f, c_len;
    int mezcla; float fac;
    int gu;
} motor_t;

static const uint8_t *B; static uint32_t Bn;
static const rper_t *per; static const rmov_t *movs; static uint32_t nper, nmov, npz, nml;
static const rpieza_t *pz; static const rmalla_t *ml; static const rvert_t *vt;
static motor_t mot_c[2], *mot_b;                 /* el piloto y la acompanante; el banderero (31 KB) solo si CHR.DAT lo trae */
#define MOT(p) ((p) < 2 ? mot_c + (p) : mot_b)
static int listo;

/* tablas de la logica (bloque TAB de RBT1) */
static const uint32_t *t_modc, *t_mira, *t_agar, *t_chqe, *t_chq7, *t_chq6, *t_cerr, *t_lisa, *t_lisc, *t_asnt, *t_mate;
static const uint32_t *t_metr, *t_mete; static uint32_t n_metr, n_mete;   /* meta del piloto (estado 9) */
static uint32_t n_agar, n_chqe, n_mate;
static const uint32_t *t_band, *t_btex;                                   /* el banderero (BAND, BTEX) */

/* ---------------------------------------------------------------- matrices de fila: M = op M (pila del PC) */
static void m_mul(mat_t o, const mat_t a, const mat_t b)       /* o = a b */
{
    mat_t r; int i, j;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) r[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
    memcpy(o, r, sizeof r);
}
static void m_id(mat_t m) { memset(m, 0, sizeof(mat_t)); m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1; }
static void op_tras(mat_t m, const float *v) { int j; for (j = 0; j < 4; j++) m[3][j] += v[0] * m[0][j] + v[1] * m[1][j] + v[2] * m[2][j]; }
static void op_rot(mat_t m, int a, int b, float ang)            /* fila a' = c a + s b, fila b' = -s a + c b */
{
    float c = cosf(ang), s = sinf(ang), t; int j;
    for (j = 0; j < 4; j++) { t = c * m[a][j] + s * m[b][j]; m[b][j] = -s * m[a][j] + c * m[b][j]; m[a][j] = t; }
}
static void op_zyx(mat_t m, const float *r) { op_rot(m, 0, 1, r[2]); op_rot(m, 2, 0, r[1]); op_rot(m, 1, 2, r[0]); }   /* 40A470 */
static void op_esc(mat_t m, const float *s)                     /* 40A360, solo si algun eje se aparta mas de 0,01 */
{
    int j, k;
    if (!(fabsf(1.0f - s[0]) > 0.01f || fabsf(1.0f - s[1]) > 0.01f || fabsf(1.0f - s[2]) > 0.01f)) return;
    for (k = 0; k < 3; k++) for (j = 0; j < 4; j++) m[k][j] *= s[k];
}
static void op_izq(mat_t m, const mat_t L) { m_mul(m, L, m); }
static void m_inv(mat_t o, const mat_t a)                       /* 40A240: D3DXMatrixInverse (afin) */
{
    float d = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
              a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]), k; mat_t r; int i, j;
    if (d == 0) { m_id(o); return; }
    k = 1.0f / d;
    r[0][0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) * k; r[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) * k;
    r[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) * k; r[1][0] = (a[1][2] * a[2][0] - a[1][0] * a[2][2]) * k;
    r[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) * k; r[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) * k;
    r[2][0] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) * k; r[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) * k;
    r[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) * k;
    for (j = 0; j < 3; j++) { r[j][3] = 0; r[3][j] = -(a[3][0] * r[0][j] + a[3][1] * r[1][j] + a[3][2] * r[2][j]); }
    r[3][3] = 1; for (i = 0; i < 4; i++) memcpy(o[i], r[i], 16);
}
static void punto(const mat_t m, const float *v, float *o)
{
    float x = v[0], y = v[1], z = v[2]; int j;
    for (j = 0; j < 3; j++) o[j] = x * m[0][j] + y * m[1][j] + z * m[2][j] + m[3][j];
}

/* ---------------------------------------------------------------- claves */
static float half(uint16_t h)                                   /* 522AA0 */
{
    union { uint32_t u; float f; } v;
    if (!h) return 0.0f;
    v.u = ((uint32_t)((h & 0x7fff) + 0x1c000) << 13) | ((uint32_t)(h & 0x8000) << 16);
    return v.f;
}
static uint16_t rd16(const motor_t *m, uint32_t o) { return (uint16_t)(m->cla[o] | m->cla[o + 1] << 8); }
static void lee_clave(const motor_t *m, float *k, uint16_t *cur)  /* 522AE0 */
{
    uint16_t w = rd16(m, *cur); *cur += 2;
    k[0] = (float)(w & 0x1fff) * SEXAG; k[1] = k[2] = k[3] = 0;
    switch (w >> 14) {
    case 0: return;
    case 1: k[3] = half(rd16(m, *cur)); *cur += 2; return;
    case 2: k[3] = half(rd16(m, *cur)); k[1] = k[2] = half(rd16(m, *cur + 2)); *cur += 4; return;
    default: k[3] = half(rd16(m, *cur)); k[1] = half(rd16(m, *cur + 2)); k[2] = half(rd16(m, *cur + 4)); *cur += 6; return;
    }
}
static void rebobina(const motor_t *m, canal_t *c)              /* 522BE0 */
{
    if (!c->keys) return;
    c->left = (uint16_t)(c->keys - 2); c->cursor = c->stream;
    lee_clave(m, c->k0, &c->cursor); c->k0[0] = 0; lee_clave(m, c->k1, &c->cursor);
}
static float evalua(const motor_t *m, canal_t *c, float t)      /* 522C20 */
{
    float u, dt, inv, u2, s2, ha, s2u, u2inv, a, b;
    if (c->keys == 1) return c->k0[3];
    if (c->k0[0] > t) {
        if (c->left == (uint16_t)(c->keys - 2)) return c->k0[3];
        rebobina(m, c);
        if (c->k0[0] > t) return c->k0[3];
    } else if (fabsf(c->k0[0] - t) < 1.1920929e-07f) return c->k0[3];
    while (!(c->k1[0] > t)) {
        if (fabsf(c->k1[0] - t) < 1.1920929e-07f) return c->k1[3];
        if (!c->left) return c->k1[3];
        c->left--; memcpy(c->k0, c->k1, sizeof c->k0); lee_clave(m, c->k1, &c->cursor);
    }
    u = t - c->k0[0]; dt = c->k1[0] - c->k0[0]; inv = 1.0f / dt;
    u2 = u * u; s2 = u2 * (inv * inv); ha = s2 * 3.0f; s2u = s2 * u; u2inv = u2 * inv;
    a = s2u - u2inv; b = (s2u * inv) * 2.0f;
    return ((a - u2inv) + u) * c->k0[2] + ((b - ha) + 1.0f) * c->k0[3] + (ha - b) * c->k1[3] + a * c->k1[1];
}
static void evalua3(const motor_t *m, int i, int k0, float t, float *o)   /* 519920 */
{
    int k;
    for (k = 0; k < 3; k++) { canal_t *c = &((motor_t *)m)->can[i][k0 + k]; o[k] = c->keys ? evalua(m, c, t) : c->rest; }
}

static const rmov_t *busca_mov(uint32_t id)
{
    uint32_t a = 0, b = nmov;
    while (a < b) { uint32_t c = (a + b) / 2; if (movs[c].id < id) a = c + 1; else b = c; }
    return a < nmov && movs[a].id == id ? movs + a : NULL;
}
/* 520750 / 522F70 / 522EE0: los canales del movimiento (sin tocar el cuadro) */
static int pon_claves(motor_t *m, uint32_t id)
{
    static const uint16_t MASK[9] = { 0x20, 0x10, 0x8, 0x4, 0x2, 0x1, 0x100, 0x80, 0x40 };
    const rmov_t *v = busca_mov(id); const uint8_t *can; int i, k, ancho; uint32_t c = 0, p = 0, nc;
    if (!v) return 0;
    m->mov = v; m->cla = B + v->ocla; can = B + v->ocan; nc = v->ncan; m->bflags = v->banderas;
    ancho = (v->banderas & 2) != 0;
    for (i = 0; i < m->n; i++) {
        uint32_t mask = 0;
        if (ancho) { if (c + 4 <= nc && i == (can[c] | can[c + 1] << 8)) { mask = can[c + 2] | can[c + 3] << 8; c += 4; } }
        else if (c + 2 <= nc && i == can[c]) { mask = can[c + 1]; c += 2; }
        for (k = 0; k < 9; k++) {
            canal_t *ch = &m->can[i][k]; uint32_t n = 0;
            if (!(mask & MASK[k])) { ch->keys = 0; continue; }
            ch->stream = (uint16_t)p;
            for (;;) {
                uint16_t w; int ini;
                if (p + 2 > v->ncla) break;
                w = rd16(m, p); ini = (w & 0x2000) != 0;
                if (ini && (w & 0x1fff) == 1) break;
                if (n && (w & 0x1fff) == 0) break;
                p += 2; n++;
                if (ini && (w & 0x1fff) == 0) { ch->k0[3] = half(rd16(m, p)); p += 2; break; }
                p += (w >> 14) * 2;
            }
            ch->keys = (uint16_t)n;
            if (n >= 2) rebobina(m, ch);
        }
    }
    return 1;
}
static int pon_mov(motor_t *m, uint32_t id)                     /* 4F2280 SetMotion */
{
    if (!pon_claves(m, id)) return 0;
    m->id = id; m->fl &= 0xffffffc2u; m->f0c = m->f10 = 0; m->f14 = m->f1c = (float)m->mov->cuadros; m->bucles = 0;
    return 1;
}
static void pon_cuadro(motor_t *m, float fr)                    /* 4F1D30 SetFrame */
{
    uint32_t fl = m->fl & 0xfffffff7u; int32_t lp = m->bucles; uint32_t bucle = lp != 0 && lp != 1;
    m->f10 = fr; fl = ((fl ^ bucle) & 1) ^ fl; m->fl = fl;
    if (fl & 1) {
        if (fr > m->f14) do { m->fl |= 8; m->f10 -= m->f14; } while (m->f10 > m->f14);
        if (0.0f > m->f10) { float v = m->f10; do { v += m->f14; m->fl |= 8; } while (0.0f > v); m->f10 = v; }
    } else {
        if (fr > m->f14) { m->f10 = m->f14; m->fl |= 0xc; }
        if (0.0f > m->f10) { m->f10 = 0; m->fl |= 0xc; }
    }
    if ((m->fl & 8) && lp > 0) m->bucles = lp - 1;
}
static int fin_mov(const motor_t *m) { return (m->fl >> 2) & 1; }   /* 4F1E70 IsMotionEnd */

/* ---------------------------------------------------------------- arbol de huesos (51B420) */
static void mezcla3(float *v, const float *s, float f) { int k; for (k = 0; k < 3; k++) v[k] = (v[k] - s[k]) * f + s[k]; }
static void zyx_mat(const float *a, mat_t o) { m_id(o); op_zyx(o, a); }
static void angulos(const mat_t mx, float *o)                   /* 519CE0 */
{
    if (fabsf(mx[2][2]) > 1e-8f) {
        float ax = atan2f((1.0f / mx[2][2]) * mx[1][2], 1.0f), y = atan2f(-mx[0][2], (1.0f / cosf(ax)) * mx[2][2]);
        o[0] = ax; o[1] = y; o[2] = fabsf(y) < 1.5707964f ? atan2f(mx[0][1], mx[0][0]) : atan2f(-mx[0][1], -mx[0][0]);
        return;
    }
    if (fabsf(mx[1][2]) > 1e-8f) { o[0] = 1.5707964f; o[1] = atan2f(-mx[0][2], mx[1][2]); o[2] = atan2f(mx[2][0], -mx[2][1]); return; }
    o[0] = 0; o[2] = atan2f(-mx[1][0], mx[1][1]); o[1] = mx[0][2] > 0 ? -1.5707964f : 1.5707964f;
}
static void mezcla_giro(float *ang, const float *sav, float t)  /* 519DD0 / 519970 */
{
    mat_t a, b, o; float s = 1.0f - t, x0, x1, x2, y0, y1, y2, z0, z1, z2, w0, w1, w2, l;
    zyx_mat(ang, a); zyx_mat(sav, b); m_id(o);
    x0 = a[0][0] * t + b[0][0] * s; x1 = a[1][0] * t + b[1][0] * s; x2 = a[2][0] * t + b[2][0] * s;
    o[0][0] = x0; o[1][0] = x1; o[2][0] = x2;
    l = sqrtf(x2 * x2 + x1 * x1 + x0 * x0); if (l > 0.01f) { o[0][0] = x0 / l; o[1][0] = x1 / l; o[2][0] = x2 / l; }
    y0 = b[0][1] * s + a[0][1] * t; y1 = b[1][1] * s + a[1][1] * t; y2 = b[2][1] * s + a[2][1] * t;
    z0 = y2 * x1 - y1 * x2; z1 = y0 * x2 - y2 * x0; z2 = y1 * x0 - y0 * x1;
    w0 = z1 * x2 - z2 * x1; w1 = z2 * x0 - z0 * x2; w2 = z0 * x1 - z1 * x0;
    l = sqrtf(w2 * w2 + w1 * w1 + w0 * w0); if (l > 0.01f) { o[0][1] = w0 / l; o[1][1] = w1 / l; o[2][1] = w2 / l; } else { o[0][1] = o[1][1] = o[2][1] = 0; }
    l = sqrtf(z2 * z2 + z1 * z1 + z0 * z0); if (l > 0.01f) { o[0][2] = z0 / l; o[1][2] = z1 / l; o[2][2] = z2 / l; } else { o[0][2] = o[1][2] = o[2][2] = 0; }
    angulos(o, ang);
}
static void mezcla_t(motor_t *m, int i)                         /* 51A3A0: el hueso 0 mezcla la y dos veces */
{
    float *v = m->T[i], *s = m->sT[i], f = m->fac;
    if (i != 0) { mezcla3(v, s, f); return; }
    v[1] = (v[1] - s[1]) * f + s[1]; v[0] = (v[0] - s[0]) * f + s[0]; v[1] = (v[1] - s[1]) * f + s[1]; v[2] = (v[2] - s[2]) * f + s[2];
}
static void trs(motor_t *m, int i, float t, mat_t M)            /* 51A5F0 */
{
    evalua3(m, i, 0, t, m->T[i]); evalua3(m, i, 3, t, m->R[i]); evalua3(m, i, 6, t, m->S[i]);
    if (m->mezcla) { mezcla_t(m, i); mezcla_giro(m->R[i], m->sR[i], m->fac); mezcla3(m->S[i], m->sS[i], m->fac); }
    op_tras(M, m->T[i]);
    if (m->h[i].unidad) { int a, b; for (a = 0; a < 3; a++) for (b = 0; b < 3; b++) M[a][b] = a == b; }
    op_zyx(M, m->R[i]); op_esc(M, m->S[i]);
}
static int sin_tras(const motor_t *m, int i) { return !m->can[i][0].keys && !m->can[i][1].keys && !m->can[i][2].keys; }
static void tras_reposo(motor_t *m, int i, mat_t M)
{
    float v[3] = { m->can[i][0].rest, m->can[i][1].rest, m->can[i][2].rest };
    memcpy(m->T[i], v, sizeof v); op_tras(M, v);
}
static void escala_j(motor_t *m, int j, float t, mat_t M)
{
    evalua3(m, j, 6, t, m->S[j]);
    if (m->mezcla) mezcla3(m->S[j], m->sS[j], m->fac);
    op_esc(M, m->S[j]);
}
static void apunta(mat_t M, const float *v)                      /* 519870 */
{
    float a = v[0], b = v[1], c = v[2], s = b * b + a * a, inv = 1.0f / sqrtf(s), inv2 = 1.0f / sqrtf(c * c + s);
    float co = (inv * s) * inv2, sn = 0.0f - inv2 * c; mat_t Z, Y;
    m_id(Z); Z[0][0] = inv * a; Z[0][1] = inv * b; Z[1][0] = -inv * b; Z[1][1] = inv * a; op_izq(M, Z);
    m_id(Y); Y[0][0] = co; Y[0][2] = -sn; Y[2][0] = sn; Y[2][2] = co; op_izq(M, Y);
}
static void objetivo(motor_t *m, int e, const float *keys, int fb, const mat_t M, float *v)
{
    mat_t I;
    if (fb & 1) punto(M, keys, m->ikt[e]); else memcpy(m->ikt[e], keys, 12);
    if (m->mezcla) mezcla3(m->ikt[e], m->sik[e], m->fac);
    m_inv(I, M); punto(I, m->ikt[e], v);
}
static void ik(motor_t *m, int i, float t, int fb, mat_t P)     /* 51A770 */
{
    const rhueso_t *H = m->h; mat_t M; float keys[3], v[3], ln, l1, l2, ca, sa, cb, sb; int j0, j1, e, bend;
    memcpy(M, P, sizeof M);
    if (H[i].ik == 2) {
        j0 = H[i].hijos[0]; j1 = H[j0].hijos[0]; e = H[j1].hijos[0];
        if (sin_tras(m, e)) { memcpy(m->M[j0], M, sizeof M); trs(m, j0, t, m->M[j0]); memcpy(m->M[j1], m->M[j0], sizeof M); trs(m, j1, t, m->M[j1]); return; }
        tras_reposo(m, j0, M);
        evalua3(m, e, 0, t, keys);
        objetivo(m, e, keys, fb, M, v);
        ln = sqrtf(v[2] * v[2] + v[1] * v[1] + v[0] * v[0]);
        { mat_t A; m_id(A); apunta(A, v);
          l1 = m->can[j1][0].rest; l2 = m->can[e][0].rest; bend = H[j1].ik;
          ca = ((l1 * l1 + ln * ln) - l2 * l2) / ((l1 * ln) * 2.0f); if (ca > 1) ca = 1; else if (ca < -1) ca = -1;
          sa = sqrtf(1 - ca * ca); if (bend == 0) sa = -sa;
          { mat_t Z; m_id(Z); Z[0][0] = ca; Z[0][1] = sa; Z[1][0] = -sa; Z[1][1] = ca; op_izq(A, Z); }
          op_izq(M, A); }
        escala_j(m, j0, t, M); memcpy(m->M[j0], M, sizeof M);
        tras_reposo(m, j1, M);
        cb = ((l1 * l1 + l2 * l2) - ln * ln) / ((l1 * l2) * 2.0f); if (cb > 1) cb = 1; else if (cb < -1) cb = -1;
        sb = sqrtf(1 - cb * cb); if (1 - bend == 0) sb = -sb;
        { mat_t Z; m_id(Z); Z[0][0] = -cb; Z[0][1] = sb; Z[1][0] = -sb; Z[1][1] = -cb; op_izq(M, Z); }
        escala_j(m, j1, t, M); memcpy(m->M[j1], M, sizeof M);
    } else if (H[i].ik == 1) {
        j0 = H[i].hijos[0]; e = H[j0].hijos[0];
        if (sin_tras(m, e)) { memcpy(m->M[j0], M, sizeof M); trs(m, j0, t, m->M[j0]); return; }
        tras_reposo(m, j0, M);
        evalua3(m, e, 0, t, keys); objetivo(m, e, keys, fb, M, v);
        apunta(M, v); escala_j(m, j0, t, M); memcpy(m->M[j0], M, sizeof M);
    }
}
static void hueso(motor_t *m, int i, float t, int fb, const mat_t P)
{
    const rhueso_t *h = m->h + i; mat_t M; int k;
    memcpy(M, P, sizeof M);
    switch (h->tipo) {
    case 0: case 1: trs(m, i, t, M); break;
    case 2: trs(m, i, t, M); ik(m, i, t, fb, M); break;
    case 3: memcpy(M, m->M[i], sizeof M); break;
    case 4:
        tras_reposo(m, i, M);
        if (h->unidad) { int a, b; for (a = 0; a < 3; a++) for (b = 0; b < 3; b++) M[a][b] = a == b; }
        evalua3(m, i, 3, t, m->R[i]);
        if (m->mezcla) mezcla_giro(m->R[i], m->sR[i], m->fac);
        op_zyx(M, m->R[i]); escala_j(m, i, t, M);
        break;
    default: break;
    }
    memcpy(m->M[i], M, sizeof M);
    for (k = 0; k < h->nh; k++) hueso(m, h->hijos[k], t, fb, M);
}
static void calcula(motor_t *m, float cuadro, int mezcla, float fac)   /* 51B6D0 / 51B650 */
{
    mat_t I; m_id(I);
    m->mezcla = mezcla; m->fac = fac;
    hueso(m, 0, cuadro * SEXAG, m->bflags, I);
    m->mezcla = 0;
}
/* 51B260 con 85B2E8 = 4: la postura actual a las guardadas (las variables 85B2xx valen 0: copias) */
static void guarda(motor_t *m, int i)
{
    const rhueso_t *h = m->h + i; int k;
    switch (h->tipo) {
    case 0: case 1: case 2:
        memcpy(m->sT[i], m->T[i], 12); memcpy(m->sR[i], m->R[i], 12); memcpy(m->sS[i], m->S[i], 12);
        if (h->tipo == 2 && (h->ik == 2 || h->ik == 1)) {
            int j0 = h->hijos[0], j1 = h->ik == 2 ? m->h[j0].hijos[0] : j0, e = m->h[j1].hijos[0];
            if (sin_tras(m, e)) {
                memcpy(m->sT[j0], m->T[j0], 12); memcpy(m->sR[j0], m->R[j0], 12); memcpy(m->sS[j0], m->S[j0], 12);
                if (h->ik == 2) { memcpy(m->sT[j1], m->T[j1], 12); memcpy(m->sR[j1], m->R[j1], 12); memcpy(m->sS[j1], m->S[j1], 12); }
            } else {
                memcpy(m->sik[e], m->ikt[e], 12); memcpy(m->sS[j0], m->S[j0], 12);
                if (h->ik == 2) memcpy(m->sS[j1], m->S[j1], 12);
            }
        }
        break;
    case 4: memcpy(m->sR[i], m->R[i], 12); memcpy(m->sS[i], m->S[i], 12); break;
    default: break;
    }
    for (k = 0; k < h->nh; k++) guarda(m, h->hijos[k]);
}
static void conecta(motor_t *m, uint32_t id, float fr, float largo)   /* 4F2320 SetMotionConnect */
{
    if (largo < 1.0f) { pon_mov(m, id); return; }
    if (!busca_mov(id)) return;
    m->c_ant = m->id; m->c_id = id;
    guarda(m, 0);
    pon_claves(m, id);
    m->id = id; m->fl = (m->fl & 0xffffffd2u) | 0x10; m->f0c = m->f10 = m->c_f = fr; m->c_t28 = m->c_t2c = 0; m->c_len = largo;
}
static void paso_mezcla(motor_t *m, float t)                    /* 4F2410 */
{
    int32_t lp = m->bucles;
    m->fl = (m->fl & 0xfffffff6u) | (lp != 0); m->c_t2c = t;
    if (t >= m->c_len) { m->fl &= 0xffffffefu; pon_mov(m, m->c_id); pon_cuadro(m, m->c_f); m->bucles = lp; }
}
static void calc(motor_t *m)                                    /* 4F2520 Calc (un paso: 1 cuadro, velocidad 1) */
{
    float d = 1.0f * m->vel;
    if (!m->mov) return;
    if (!(m->fl & 0x10)) {
        if (m->f10 > m->f1c) m->f10 = m->f1c;
        m->f0c = m->f10; calcula(m, m->f0c, 0, 0); pon_cuadro(m, m->f0c + d);
    } else {
        float t = m->c_t2c; m->c_t28 = t;
        calcula(m, m->f0c, 1, t / m->c_len); paso_mezcla(m, m->c_t28 + d);
    }
    m->fl |= 0x20;
}
static void pon_mezcla(motor_t *m, uint32_t id, int32_t bucles)    /* 487B70: SetMotionConnect(id, 0, 10), SetLoop */
{
    conecta(m, id, 0.0f, 10.0f); m->bucles = bucles;
}
static int agarre(uint32_t id)                                  /* 487940 */
{
    uint32_t a = 0, b = n_agar;
    while (a < b) { uint32_t c = (a + b) / 2; if (t_agar[c] < id) a = c + 1; else b = c; }
    return a < n_agar && t_agar[a] == id;
}
static void manos(motor_t *m) { if (!(m->fl & 0x20)) m->gu = agarre(m->id); }

/* ---------------------------------------------------------------- datos */
static const uint32_t *tabla(const char *k, uint32_t *n)
{
    const uint32_t *w = (const uint32_t *)(B + ((const uint32_t *)B)[8]), *fin = (const uint32_t *)(B + Bn);
    while (w + 2 <= fin && memcmp(w, "FIN ", 4)) {
        if (!memcmp(w, k, 4)) { if (n) *n = w[1]; return w + 2; }
        w += 2 + w[1];
    }
    if (n) *n = 0;
    return NULL;
}
int robot_inicia(const void *rbt, uint32_t bytes)
{
    const uint32_t *w = rbt; uint32_t i, n;
    listo = 0;
    if (!rbt || bytes < 48 || memcmp(w, "RBT1", 4)) return -1;
    B = rbt; Bn = bytes;
    nper = w[1]; nmov = w[2]; npz = w[3]; nml = w[4];
    if (nper < 2 || nper > ROBOT_N) return -2;            /* 2: CHR.DAT sin el banderero */
    per = (const rper_t *)(w + 9); movs = (const rmov_t *)(per + nper);
    pz = (const rpieza_t *)(B + w[5]); ml = (const rmalla_t *)(B + w[6]); vt = (const rvert_t *)(B + w[7]);
    for (i = 0; i < nper; i++) if (per[i].nh > ROBOT_HUESOS) return -3;
    t_modc = tabla("MODC", &n); if (n < 30) return -4;
    t_mira = tabla("MIRA", &n); if (n < 120) return -4;
    t_agar = tabla("AGAR", &n_agar);
    t_chqe = tabla("CHQE", &n_chqe); t_chq7 = tabla("CHQ7", NULL); t_chq6 = tabla("CHQ6", NULL);
    t_cerr = tabla("CERR", &n); if (n < 30) return -4;
    t_mate = tabla("MATE", &n_mate); n_mate /= 9;
    t_lisa = tabla("LISA", NULL); t_lisc = tabla("LISC", NULL); t_asnt = tabla("ASNT", &n);
    t_metr = tabla("METR", &n_metr); t_mete = tabla("METE", &n_mete);
    if (n_metr < 30 || n_mete < 3 * 8 * 3) t_metr = t_mete = NULL;
    if (!t_mate || !t_agar || !t_chqe || !t_chq7 || !t_chq6 || !t_lisa || !t_lisc || !t_asnt || n < 30 * 6) return -4;
    t_band = tabla("BAND", &n); if (nper < 3 || n < 5 * 18 + 9) t_band = NULL;
    t_btex = tabla("BTEX", &n); if (!n) t_btex = NULL;
    if (nper >= 3 && !mot_b && !(mot_b = malloc(sizeof *mot_b))) return -5;
    for (i = 0; i < nper; i++) {
        motor_t *m = MOT(i); int j, k;
        memset(m, 0, sizeof *m);
        m->h = (const rhueso_t *)(B + per[i].oh); m->n = per[i].nh; m->inv = (const mat_t *)(B + per[i].oinv); m->vel = 1;
        for (j = 0; j < m->n; j++) {
            for (k = 0; k < 9; k++) m->can[j][k].rest = m->h[j].reposo[k];
            memcpy(m->T[j], m->h[j].reposo, 12); memcpy(m->R[j], m->h[j].reposo + 3, 12); memcpy(m->S[j], m->h[j].reposo + 6, 12);
            m_id(m->M[j]);
        }
    }
    listo = 1;
    return 0;
}
int robot_listo(void) { return listo; }

/* ---------------------------------------------------------------- logica */
enum { R_IDLE, R_BORED, R_SCARED, R_SPIN, R_CRASH, R_TURN, R_LAND, R_JUMP, R_AIR, R_POSE, R_START, R_GOAL, R_OVER,
       R_CHEER, R_HIT, R_RANK, R_AREA };
static struct {
    int modelo, cerr;
    int d_est, d_cuenta;                            /* 82F588, 82F58C */
    int m_sub, m_t; const uint32_t *m_ent;          /* 82F590, 82F594, 82F598 (la meta del piloto) */
    int a_est, a_tiempo, a_fin;                     /* 82F5A4, 82F5A8, 82F5B0 */
    int8_t pick_idle, pick_turn, pick_jump, pick_land;   /* 82F5AA.., 82F2F8 / 82F2F9 */
} lg;

static const uint32_t *lista(int off, int *n)       /* {lista, cuenta} de +off: 8 x {cuenta, 4 ids} en LISA / LISC */
{
    const uint32_t *L = lg.cerr ? t_lisc : t_lisa, *e = L + 5 * (off / 8);
    *n = (int)e[0];
    return e + 1;
}
static uint32_t elige(int8_t *pick, int off) { int n; const uint32_t *l = lista(off, &n); int i = *pick; return i >= 0 && i < 4 ? l[i] : 0xffffffffu; }
static void avanza(int8_t *pick, int off) { int n; lista(off, &n); (*pick)++; if (*pick >= n) *pick = 0; }

void robot_carrera(int modelo)
{
    motor_t *d = MOT(0), *a = MOT(1);
    if (!listo) return;
    if (modelo < 0 || modelo >= 30) modelo = 6;
    memset(&lg, 0, sizeof lg);
    lg.modelo = modelo; lg.cerr = t_cerr[modelo] != 0;
    /* 4884D0 */
    d->fl = 0; d->mov = NULL;
    pon_mov(d, t_modc[modelo]); pon_cuadro(d, 0); calc(d); pon_cuadro(d, 85.0f); d->bucles = 0;
    lg.d_est = 8; lg.d_cuenta = 0; lg.pick_jump = lg.pick_land = 0;
    d->gu = agarre(d->id);
    /* 4885A0 (variante 1: 0x25000c) */
    a->fl = 0; a->mov = NULL;
    pon_mov(a, 0x25000c); pon_cuadro(a, 0); calc(a); pon_cuadro(a, 0); a->bucles = -1;
    lg.a_est = R_START; lg.a_tiempo = 0; lg.pick_idle = lg.pick_turn = 0;
    a->gu = agarre(a->id);
}

/* 488DD0 estado 9 (PS2 0x20250c): la secuencia de la meta; registros de 3 (0xFFFFFFFE: no hay registro) */
static void meta_piloto(motor_t *m, const robot_coche_t *c)
{
    const uint32_t *r;
    int s = lg.m_sub, b = s >= 5;                   /* 1..4 el primer registro, 5..8 el segundo */
    switch (s) {
    case 0: {
        int fila = (int)t_metr[lg.modelo], tb = c->clase_meta == 0x1b ? 1 : c->clase_meta == 0x1a || c->clase_meta == 0x1c ? 2 : 0;
        lg.m_ent = t_mete + (tb * 8 + (fila & 3) * 2) * 3; lg.m_sub = 1;
        break; }
    case 1: case 5:
        r = lg.m_ent + 3 * b; lg.m_t = b ? 0x168 : 0xf0;
        if (r[0] == 0xfffffffeu) { lg.m_sub = b ? 8 : 4; break; }
        if ((int32_t)r[0] < 0) { lg.m_t = -1; lg.m_sub = b ? 6 : 2; break; }
        pon_mezcla(m, r[0], 0); lg.m_sub = b ? 6 : 2;
        break;
    case 2: case 6:
        r = lg.m_ent + 3 * b;
        if (!fin_mov(m) && lg.m_t >= 0) {
            if (lg.m_t != 0) { lg.m_t--; break; }
            lg.m_sub = (b ? 7 : 3) + ((int32_t)r[2] < 0 ? 1 : 0);
            break;
        }
        pon_mezcla(m, r[1], -1); lg.m_t = b ? 0x168 : 0xf0;
        break;
    case 3: pon_mezcla(m, lg.m_ent[2], 0); lg.m_sub = 4; lg.m_t = 0x78; break;
    case 4: { int t = lg.m_t; lg.m_t = t - 1; if (t >= 0 && !fin_mov(m)) break; lg.m_sub = 5; break; }
    case 7: pon_mezcla(m, lg.m_ent[3 + 2], 0); lg.m_sub = 8; break;
    default: break;
    }
}
static void piloto(const robot_coche_t *c)          /* 488DD0 (variante 1) */
{
    motor_t *m = MOT(0); const uint32_t *mira = t_mira + 4 * lg.modelo;
    float fr = (float)(c->s204 + 0x7fff) / 65535.0f * 168.0f;
    int k;
    if (lg.d_est == 8) { if (c->fase != 0) lg.d_est = 1; }
    else if (c->fase == 3) { if (lg.d_est != 0xa) { pon_mezcla(m, 0x60000, -1); lg.d_est = 0xa; } }
    else if (c->fase == 2) { if (t_metr && lg.d_est != 9) { lg.d_est = 9; lg.m_sub = 0; } }   /* 44FF10 */
    else if (c->t_recup > 0.0f) {
        if (lg.d_est != 6 && lg.d_est != 7) {
            k = c->choque - 1;
            lg.d_est = k >= 0 && (uint32_t)k < n_chqe ? (int)t_chqe[k] : 6;
            if (lg.d_est == 7) pon_mezcla(m, t_chq7[(lg.pick_jump + 2 * lg.cerr) & 3], 0);
            else pon_mezcla(m, t_chq6[(lg.pick_land + 2 * lg.cerr) & 3], 0);
        }
    } else if (lg.d_est == 1) {
        if (lg.d_cuenta > 0x78 && (int32_t)mira[0] >= 0) { lg.d_est = 2; pon_mezcla(m, mira[0], 0); lg.d_cuenta = 0; }
        else if (fr > 83.0f && fr < 87.0f) lg.d_cuenta++;
    } else if (lg.d_est == 2) {
        if (fin_mov(m)) { lg.d_est = 4; conecta(m, mira[2], fr, 10.0f); m->bucles = 0; }
    } else if (lg.d_est == 4) {
        if (lg.d_cuenta > 0x78) { lg.d_est = 5; pon_mezcla(m, mira[3], 0); lg.d_cuenta = 0; }
        else if (fr > 83.0f && fr < 87.0f) lg.d_cuenta++;
    } else if (lg.d_est == 5 || lg.d_est == 6 || lg.d_est == 7 || lg.d_est == 0xc) {
        if (fin_mov(m)) { lg.d_est = 1; conecta(m, t_modc[lg.modelo], fr, 10.0f); m->bucles = 0; }
    }
    if (lg.d_est == 1 || lg.d_est == 4 || lg.d_est == 8) pon_cuadro(m, fr);
    if (lg.d_est == 9) meta_piloto(m, c);
    manos(m);
    calc(m);
}

static void a_reposo(motor_t *m)                    /* 4886B0 */
{
    if (!fin_mov(m)) return;
    conecta(m, 0x6004b, 0, 10.0f); m->bucles = -1; lg.a_est = R_IDLE; lg.a_tiempo = 0;
}
static void acompanante(const robot_coche_t *c)     /* 489A90 (variante 1) */
{
    motor_t *m = MOT(1); uint32_t slide = c->sup & 0x10f407cu; int st = lg.a_est, k, a;
    static const uint8_t GRANDE[7] = { 0, 1, 0, 0, 1, 0, 0 };
    /* reacciones */
    do {
        if (st == R_START) { lg.a_est = R_IDLE; break; }
        if (c->fase == 2) { if (st != R_GOAL) { lg.a_est = R_GOAL; lg.a_fin = 0; } break; }
        if (c->fase == 3) { if (st != R_OVER) { pon_mezcla(m, 0x60036, -1); lg.a_est = R_OVER; } break; }
        if (c->t_choque > 0.0f) {                   /* en el aire (choque) */
            if (st == R_AIR) break;
            k = c->choque - 1;
            if (k < 0 || k > 6) break;
            if (k == 0 || k == 1 || k == 4) { lg.a_est = R_AIR; pon_mezcla(m, 0x6005f, 0); }
            else if (k == 6) lg.a_est = R_AIR;
            break;
        }
        if (c->t_recup > 0.0f) {                    /* recuperandose */
            if (st == R_LAND || st == R_JUMP) break;
            k = c->choque - 1;
            if (k >= 0 && k <= 6 && !GRANDE[k]) { lg.a_est = R_JUMP; pon_mezcla(m, elige(&lg.pick_jump, 0x38), 0); avanza(&lg.pick_jump, 0x38); }
            else { lg.a_est = R_LAND; pon_mezcla(m, elige(&lg.pick_land, 0x30), 0); avanza(&lg.pick_land, 0x30); }
            break;
        }
        if (st == R_LAND || st == R_JUMP) break;
        if (slide && st != R_HIT) { pon_mezcla(m, !(slide & 4) ? 0x220007 : 0x230003, 0); lg.a_est = R_HIT; break; }
        if (c->kmh >= 100) {                        /* giro brusco */
            a = c->s4e < 0 ? -c->s4e : c->s4e;
            if (a > 0x2000 && a < 0x4000) {
                if (st == R_TURN || lg.cerr) break;
                lg.a_est = R_TURN; pon_mezcla(m, elige(&lg.pick_turn, 0x28), 0); avanza(&lg.pick_turn, 0x28);
                break;
            }
        }
        if (lg.a_tiempo > 0xb4) {                   /* aburrida */
            lg.a_est = R_BORED; pon_mezcla(m, elige(&lg.pick_idle, 8), 0); avanza(&lg.pick_idle, 8); lg.a_tiempo = 0;
        }
    } while (0);
    /* 48A6BC: la reaccion en curso */
    switch (lg.a_est) {
    case R_IDLE: lg.a_tiempo++; break;
    case R_BORED: case R_SCARED: case R_SPIN: case R_LAND: case R_JUMP: case R_AREA: a_reposo(m); break;
    case R_TURN:
        if (!fin_mov(m)) break;
        a = c->s4e < 0 ? -c->s4e : c->s4e;
        if (a > 0x2000 && a < 0x4000) { pon_mezcla(m, elige(&lg.pick_turn, 0x28), 0); break; }
        lg.a_est = R_IDLE; pon_mezcla(m, 0x6004b, -1); avanza(&lg.pick_turn, 0x28); lg.a_tiempo = 0;
        break;
    case R_AIR:
        if (!(c->t_choque == 0.0f) || c->choque != 7) break;
        lg.a_est = R_JUMP;
        { int n; const uint32_t *l = lista(0x20, &n); pon_mezcla(m, l[1], 0); }
        avanza(&lg.pick_jump, 0x20);
        break;
    case R_GOAL:
        if (lg.a_fin == 0) { pon_mezcla(m, lg.cerr ? 0x25000c : 0x60049, -1); lg.a_fin = 8; }
        break;
    case R_HIT:
        if (!fin_mov(m)) break;
        if (!slide) { a_reposo(m); break; }
        pon_mezcla(m, !(slide & 4) ? 0x220007 : 0x230003, 0); lg.a_est = R_HIT;
        break;
    default: break;
    }
    manos(m);
    calc(m);
}

void robot_paso(const robot_coche_t *c)
{
    if (!listo) return;
    if (c->modelo != lg.modelo) robot_carrera(c->modelo);
    piloto(c);
    acompanante(c);
}

const float (*robot_huesos(int p))[4][4] { return (const float (*)[4][4])MOT(p)->M; }
const float (*robot_inversa(int p, int i))[4] { return MOT(p)->inv[i]; }
int robot_mano_gu(int p) { return MOT(p)->gu; }
const float *robot_asiento(int modelo, int p)
{
    if (modelo < 0 || modelo >= 30) modelo = 6;
    return (const float *)(t_asnt + 6 * modelo + 3 * p);
}
int robot_npiezas(void) { return (int)npz; }
const rpieza_t *robot_pieza(int i) { return pz + i; }
const rmalla_t *robot_malla(int i) { return ml + i; }
const rvert_t *robot_vert(int i) { return vt + i; }
int robot_nmat(void) { return (int)n_mate; }
const float *robot_material(int i) { return (const float *)(t_mate + 9 * i); }
uint32_t robot_movimiento(int p) { return MOT(p)->id; }
float robot_cuadro(int p) { return MOT(p)->f0c; }
int robot_estado(int p) { return p ? lg.a_est : lg.d_est; }

/* ---------------------------------------------------------------- el banderero de la salida (ROB03)
   PS2 (SLES-53998): init 0x2046f0 (PC 488710), control 0x204888 (PC 488820), huida 0x204ca0 (PC 487A30), dibujo
   0x204c28 (PC 4879D0); la tela de la bandera 0x100a68 / 0x100d20 / 0x100d40 / 0x101070 (PC 402100 / 402140 / 402170 /
   4021A0), con las mismas constantes que el PC (0x328700..0x32872c: 1, 0,0027222, 0xFFC0C0C0, los puntos de la asta
   (0,146, 0,18, 0,007) y (0,175, 0,68, 0,015)). Tablas BAND de CHR.DAT (tools/conv_chr.py). El mundo de la demo:
   la posicion del original (marco de la etapa de salida) pasa por el marco de la etapa (como la escena de la meta). */
#define TELA_C 5
#define TELA_F 7
typedef struct {                                   /* nodo de la tela (0x5C del PC: 95B27C + (f x 5 + c) x 0x5C) */
    uint32_t bordes; float pos[3], vel[3], rep[3], prev[3], viento[3], l_arr, l_aba, l_izq, l_der;
} nodo_t;
static struct {
    int hay, vis, huida, sig, preset;              /* work+4 & 0x8000, 82F5E4 (5DB544..), 82F5E8 */
    float pos[3], rot[3], marco[4];                /* 5DB558 / 5DB568 */
    mat_t W;                                       /* work+10 */
    float w40[3];                                  /* work+40 (la traslacion de W del paso anterior) */
    nodo_t n[TELA_F][TELA_C];
    float escala;                                  /* +CAC: nadie la escribe (0) */
} bd;
static const uint32_t *bd_fila(void) { return t_band + 18 * bd.preset; }
static float f32u(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

/* 401080: la tela de c x f nodos entre a y b (espacio de la mano) */
static void tela_inicia(const float *a, const float *b)
{
    float d[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, len, seg, s[3], l2, p[3];
    int r, c;
    len = sqrtf(d[2] * d[2] + d[0] * d[0] + d[1] * d[1]);
    seg = len / (float)(TELA_C - 1);
    memcpy(s, d, 12);
    l2 = sqrtf(d[2] * d[2] + d[1] * d[1] + d[0] * d[0]);
    if (l2 > 0.0001f) { float k = seg / l2; s[0] = d[0] * k; s[1] = d[1] * k; s[2] = d[2] * k; }
    for (r = 0; r < TELA_F; r++)
        for (c = 0; c < TELA_C; c++) {
            nodo_t *n = &bd.n[r][c];
            memset(n, 0, sizeof *n);
            n->bordes = (r ? 1u : 0u) | (r != TELA_F - 1 ? 2u : 0u) | (c ? 4u : 0u) | (c != TELA_C - 1 ? 8u : 0u);
            memcpy(n->pos, a, 12); memcpy(n->rep, a, 12);
            n->l_arr = n->l_aba = n->l_izq = n->l_der = seg;
        }
    memcpy(p, a, 12);
    for (c = 0; c < TELA_C; c++) {
        memcpy(bd.n[0][c].pos, p, 12); memcpy(bd.n[0][c].rep, p, 12);
        p[0] += s[0]; p[1] += s[1]; p[2] += s[2];
    }
}
static void limita(float *a, const float *b, float largo)      /* 401790 */
{
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2], d2 = dz * dz + dy * dy + dx * dx, k;
    if (!(d2 > largo * largo)) return;
    k = largo / sqrtf(d2);
    a[0] = k * dx + b[0]; a[1] = dy * k + b[1]; a[2] = k * dz + b[2];
}
static void muelle(float *p, const float *q, const float *l, float largo)   /* 401720 */
{
    float dx = q[0] - l[0], dy = q[1] - l[1], dz = q[2] - l[2], k = -((0.0027222000062465668f / largo) * dy);
    p[0] += dx * k; p[1] += dy * k; p[2] += dz * k;
    limita(p, q, largo);
}
/* 402140: M = el hueso de la mano por work+10 (vectores fila) */
static void tela_paso(const mat_t M)
{
    int r, c;
    for (c = 0; c < TELA_C; c++) punto(M, bd.n[0][c].rep, bd.n[0][c].pos);   /* 401490 */
    for (r = 1; r < TELA_F; r++)
        for (c = 0; c < TELA_C; c++) {
            nodo_t *n = &bd.n[r][c]; float o[3], t[3], P[3]; int k;
            memcpy(n->prev, n->pos, 12); memcpy(o, n->pos, 12);
            for (k = 0; k < 3; k++) t[k] = n->viento[0] * M[0][k] + n->viento[1] * M[1][k] + n->viento[2] * M[2][k];   /* normal */
            for (k = 0; k < 3; k++) P[k] = t[k] * bd.escala + o[k] + n->vel[k];   /* + 95AEB8 (0) */
            if (n->bordes & 2) muelle(P, bd.n[r + 1][c].pos, o, n->l_aba);
            if (n->bordes & 4) muelle(P, bd.n[r][c - 1].pos, o, n->l_izq);    /* + 95AEAC (0) */
            if (n->bordes & 8) muelle(P, bd.n[r][c + 1].pos, o, n->l_der);
            muelle(P, bd.n[r - 1][c].pos, o, n->l_arr);
            n->vel[0] = (P[0] - o[0]) * 1.0f;
            memcpy(n->pos, P, 12);
            n->vel[1] = (P[1] - o[1]) * 1.0f - 1.0f * 0.0027222000062465668f;
            n->vel[2] = (P[2] - o[2]) * 1.0f;
        }
}
static void bd_matriz(void)                        /* push unit, T(pos), Rz, Ry, Rx; y el marco de la etapa */
{
    mat_t A, Mk; float c = cosf(bd.marco[3]), sn = sinf(bd.marco[3]);
    m_id(A); op_tras(A, bd.pos); op_zyx(A, bd.rot);
    m_id(Mk); Mk[0][0] = c; Mk[0][2] = -sn; Mk[2][0] = sn; Mk[2][2] = c;
    Mk[3][0] = bd.marco[0]; Mk[3][1] = bd.marco[1]; Mk[3][2] = bd.marco[2];
    m_mul(bd.W, A, Mk);
}
int robot_band_hay(void) { return listo && t_band != NULL && nper >= 3; }
void robot_band_inicia(int preset, const float *marco)  /* 0x2046f0 */
{
    static const float A[3] = { 0.14599999785f, 0.18000000715f, 0.00700000022f }, Bp[3] = { 0.17499999702f, 0.68000000715f, 0.01499999966f };
    motor_t *m = MOT(2); const uint32_t *f; int k;
    bd.hay = 0;
    if (!robot_band_hay() || preset < 0 || preset > 4) return;
    bd.preset = preset; f = bd_fila();
    memcpy(bd.marco, marco, sizeof bd.marco);
    for (k = 0; k < 3; k++) { bd.pos[k] = f32u(f[9 + k]); bd.rot[k] = f32u(f[15 + k]); }
    bd.huida = bd.sig = 0; bd.vis = 0; bd.escala = 0;
    m->fl = 0; m->mov = NULL;
    if (!pon_mov(m, f[0])) return;                 /* SetMotion(0x36f778[preset]) */
    m->bucles = 0;                                 /* SetLoop(0) */
    bd_matriz(); memcpy(bd.w40, bd.W[3], 12);
    tela_inicia(A, Bp);                            /* 0x100a68 (402100) */
    bd.hay = 1;
}
void robot_band_quita(void) { bd.hay = 0; bd.vis = 0; }
static float dist3(const float *a, const float *b) { float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2]; return sqrtf(x * x + y * y + z * z); }
static int huye(const robot_band_t *e)            /* 0x204ca0 (487A30): solo el coche del jugador (CAR01) */
{
    const uint32_t *T = t_band + 90;               /* distancias {dibujo, huida, minima}, eje, punto */
    float lim = f32u(T[2]), lejos = f32u(T[1]), d, eje[3], pt[3], dir[3], l, cs, ratio; int k;
    float pp[3] = { f32u(T[6]), f32u(T[7]), f32u(T[8]) }, ax[3] = { f32u(T[3]), f32u(T[4]), f32u(T[5]) }, fp[3];
    {   float c = cosf(bd.marco[3]), sn = sinf(bd.marco[3]);   /* 5DB558 en el mundo de la demo */
        fp[0] = bd.pos[0] * c + bd.pos[2] * sn + bd.marco[0]; fp[1] = bd.pos[1] + bd.marco[1]; fp[2] = -bd.pos[0] * sn + bd.pos[2] * c + bd.marco[2]; }
    d = dist3(e->coche, fp);
    if (!(d > lim)) return 0;
    for (k = 0; k < 3; k++) eje[k] = ax[0] * e->B0[0][k] + ax[1] * e->B0[1][k] + ax[2] * e->B0[2][k];   /* 40A820 */
    for (k = 0; k < 3; k++) pt[k] = pp[0] * e->B0[0][k] + pp[1] * e->B0[1][k] + pp[2] * e->B0[2][k] + e->B0[3][k];   /* 40A7D0 */
    for (k = 0; k < 3; k++) dir[k] = fp[k] - pt[k];
    l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (l > 0) for (k = 0; k < 3; k++) dir[k] /= l;
    cs = dir[0] * eje[0] + dir[1] * eje[1] + dir[2] * eje[2];
    ratio = sqrtf(d * d - lim * lim) / d;
    return cs > ratio && lejos > d;
}
void robot_band_paso(const robot_band_t *e)       /* 0x204888 (488820) */
{
    const uint32_t *f, *T; motor_t *m = MOT(2); float v[3], to[3]; int k, misma;
    if (!bd.hay) return;
    f = bd_fila(); T = t_band + 90;
    if (dist3(e->ojo, bd.w40) > f32u(T[0])) { bd.vis = 0; return; }
    bd.vis = 1;
    for (k = 0; k < 3; k++) { v[k] = e->mira[k] - e->ojo[k]; to[k] = bd.w40[k] - e->ojo[k]; }
    if (v[0] * to[0] + v[1] * to[1] + v[2] * to[2] < 0.0f) return;
    if (e->t8367bc >= 0x78) pon_cuadro(m, 0.0f);   /* SetFrame(0) */
    misma = m->id == f[0];                          /* el nombre del movimiento puesto = el del inicial */
    if (fin_mov(m) && !bd.huida) {
        int i = bd.sig;
        conecta(m, f[3 + i], 0.0f, i == 0 && misma ? 0.0f : 20.0f);
        m->bucles = 0;
        for (k = 0; k < 3; k++) bd.pos[k] = f32u(f[12 + k]);   /* 0x40efc8 */
        if (++bd.sig >= 6) bd.sig = 0;
    }
    if (!bd.huida && e->t8367bc < 1 && huye(e)) {
        conecta(m, misma ? f[1] : f[2], 0.0f, 4.0f);
        bd.huida = 1;
    }
    bd_matriz();
    calc(m);                                       /* 0x201868 (488450): Calc */
    memcpy(bd.w40, bd.W[3], 12);                   /* work+40 (de work+10) */
    {   mat_t H; int b = bd.preset == 1 || bd.preset == 3 ? 0x11 : 0x1a;   /* 0x208470: la matriz del hueso */
        if (b < m->n) { m_mul(H, m->M[b], bd.W); tela_paso(H); } }
}
int robot_band_visible(void) { return bd.hay && bd.vis; }
int robot_band_activo(void) { return bd.hay; }
const float (*robot_band_mundo(void))[4] { return (const float (*)[4])bd.W; }
uint32_t robot_band_textura(void) { return t_btex ? t_btex[0] : 0xffffffffu; }
/* 401990: posiciones y normales de los 35 vertices (fila x 5 + columna) */
void robot_band_tela(float (*pos)[3], float (*nor)[3])
{
    int r, c, k;
    for (r = 0; r < TELA_F; r++) for (c = 0; c < TELA_C; c++) memcpy(pos[r * TELA_C + c], bd.n[r][c].pos, 12);
    for (r = 0; r + 1 < TELA_F; r++) {
        for (c = 0; c + 1 < TELA_C; c++) {          /* 40F2C0(p0, p1, p2): (p0 - p1) x (p2 - p0) normalizado */
            const float *p0 = pos[r * TELA_C + c], *p1 = pos[r * TELA_C + c + 1], *p2 = pos[(r + 1) * TELA_C + c];
            float a[3] = { p0[0] - p1[0], p0[1] - p1[1], p0[2] - p1[2] }, b[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
            float *o = nor[r * TELA_C + c], l2;
            o[0] = b[2] * a[1] - b[1] * a[2]; o[1] = b[0] * a[2] - b[2] * a[0]; o[2] = b[1] * a[0] - b[0] * a[1];
            l2 = o[0] * o[0] + o[1] * o[1] + o[2] * o[2];
            if (l2 != 0.0f) { float kk = 1.0f / sqrtf(l2); for (k = 0; k < 3; k++) o[k] *= kk; }
        }
        memcpy(nor[r * TELA_C + TELA_C - 1], nor[r * TELA_C + TELA_C - 2], 12);
    }
    for (c = 0; c + 1 < TELA_C; c++) memcpy(nor[(TELA_F - 1) * TELA_C + c], nor[(TELA_F - 2) * TELA_C + c], 12);
    memcpy(nor[TELA_F * TELA_C - 1], nor[TELA_F * TELA_C - 2], 12);
}
