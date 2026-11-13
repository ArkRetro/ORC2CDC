/* Misiones de carrera de COAST 2 COAST: los coches de carreras (el rival) como el original.
   Logica del port de PC (outrun2-decomp: platform/racer_setup.cpp 47CF40, platform/race_traffic.cpp 47C8A0 / 47CA40 /
   47BC90 / 477090 / 4763D0 / 477510, platform/mission_manager.cpp 495C20) con las tablas y constantes del ELF de PS2
   (SLES-53998: montaje 0x1ef900, 47C8A0 = 0x1eefe0, 47CA40 = 0x1ef230, 47BC90 = 0x1ecef8, 477090 = 0x1eee38; datos en
   MISION.DAT, tools/conv_mision.py).
   - Cada coche de carreras es un registro (rival_t) que avanza solo por la posicion de carretera del curso entero
     (+40 += +44 x ritmo x arranque cada dos cuadros, 47CA40) hasta que esta entre 10 posiciones detras y 60 delante
     del jugador: entonces la carrera (demo/main.c, rival_abre) le abre un coche del trafico (suceso 0x55 de 46E830)
     con su modelo de Ferrari, que conduce la IA de los coches de carreras (47A7C0) y cuya posicion lleva el registro
     (477090). Cuando ese coche se cierra (77F60: 15 detras o 70 delante) el registro sigue solo (4763D0).
   - Lo que esta: el montaje entero (modelos, colores, ritmos, vaivenes de la velocidad, especiales), 47CA40 / 47BC90
     sin vueltas (c18 = -1 en las 94 misiones), la clasificacion 477510 y la nota del tipo 0 (495C20).
   - SIN PORTAR: los vaivenes 476CA0 / 476DF0 / 476F40 (solo los registros de los tipos 2 y 3 los piden: c30 & 0x18 o
     c44 / c48; los 76 de carrera llevan c30 = 3 sin tablas), 477C90 (orden de las misiones de puntos, tipo != 0), los
     nombres (+78, texto 0x3d6 "%s %d"), +20 de los especiales sin tabla (8514A4[e8], luces del freno). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gd.h"
#include "rival.h"

int (*rival_abre)(int i, int ebx, int delante, int parrilla);

#define REG_BYTES 768
static uint8_t lista_cl[RIVAL_NMIS], lista_sub[RIVAL_NMIS], lista_tipo[RIVAL_NMIS];
static int lista_ok;
/* tablas del ELF (detras de los registros) */
static uint8_t t_listas[8][16], t_fam[64], t_cla[64], t_col[240];
static float t_acel[5], t_curva[12], t_marcha[6]; static int32_t t_millas[8];
static int tab_ok;

static rival_mision_t mis; static int mis_ok;
static int32_t esp_w[8][12]; static int nesp;                   /* especiales: e0..e18, e1c, e20, hay24, e24[2] */

static rival_t rc[RIVAL_MAX]; static int nrc;
static uint32_t semilla;                                        /* 6A4E2C[0] (PS2 [0x5c80b8]) */
static int largo;                                               /* 803710: largo del curso en posiciones */
static int clas_n, lider = -1, ultimo = -1, jug_puesto, jug_puesto_ant, lejos_8037b8, clas_hecha_80fb18;
static int paso_802e08, bulk_80fb09, sin_vaiven_80fb08, cd_delante_800d98, cd_detras_80a5a0;
typedef struct { int32_t pos; float frac, pf; int32_t dif; int idx; } entrada_t;   /* registro de 0x14 de 80FB1C */
static entrada_t clas[RIVAL_MAX + 1];

static uint32_t u32le(const uint8_t *p) { return (uint32_t)p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

int rival_inicia(void)
{
    uint32_t sz, n, tam, ot, i; uint8_t *b = gd_carga("MISION.DAT", &sz);
    lista_ok = tab_ok = 0;
    if (!b) return -1;
    if (sz < 16 || memcmp(b, "MIS1", 4)) { free(b); return -2; }
    n = u32le(b + 4); tam = u32le(b + 8); ot = u32le(b + 12);
    if (n != RIVAL_NMIS || tam != REG_BYTES || ot + 128 + 64 + 64 + 240 + 20 + 48 + 24 > sz) { free(b); return -3; }
    for (i = 0; i < n; i++) {
        const uint8_t *r = b + 16 + i * tam;
        lista_cl[i] = (uint8_t)u32le(r); lista_sub[i] = (uint8_t)u32le(r + 4); lista_tipo[i] = (uint8_t)u32le(r + 12);
    }
    {   const uint8_t *t = b + ot; int k;
        for (k = 0; k < 8; k++) memcpy(t_listas[k], t + 16 * k, 16);
        t += 128; memcpy(t_fam, t, 64); t += 64; memcpy(t_cla, t, 64); t += 64; memcpy(t_col, t, 240); t += 240;
        memcpy(t_acel, t, 20); t += 20; memcpy(t_curva, t, 48); t += 48; memcpy(t_marcha, t, 24); t += 24;
        if ((uint32_t)(t - b) + 32 <= sz) memcpy(t_millas, t, 32);
    }
    lista_ok = tab_ok = 1;
    free(b);
    return 0;
}
int rival_lista(int i, int *clave, int *sub, int *tipo)
{
    if (!lista_ok || i < 0 || i >= RIVAL_NMIS) return -1;
    *clave = lista_cl[i]; *sub = lista_sub[i]; *tipo = lista_tipo[i]; return 0;
}
void rival_fuera(void) { mis_ok = 0; nrc = 0; }
const rival_mision_t *rival_mis(void) { return mis_ok ? &mis : NULL; }
const float *rival_acel(void) { return t_acel; }
const float *rival_curva(void) { return t_curva; }
const float *rival_marcha(void) { return t_marcha; }

int rival_carrera(int i)
{
    uint32_t sz; uint8_t *b; const uint8_t *r; int k;
    mis_ok = 0; nrc = 0;
    if (!tab_ok || i < 0 || i >= RIVAL_NMIS) return -1;
    if (!(b = gd_carga("MISION.DAT", &sz))) return -2;
    if (sz < 16 + (uint32_t)(i + 1) * REG_BYTES) { free(b); return -3; }
    r = b + 16 + i * REG_BYTES;
    mis.clave = (int)u32le(r); mis.sub = (int)u32le(r + 4); mis.b14 = (int)u32le(r + 8); mis.tipo = (int)u32le(r + 12);
    memcpy(&mis.segundos, r + 16, 4); memcpy(mis.umbral, r + 20, 12);
    memcpy(mis.cfg, r + 32, 52); memcpy(&mis.c20, r + 32 + 32, 4);
    memcpy(mis.perfil, r + 84, 64); memcpy(mis.c3c, r + 148, 8); memcpy(mis.luces, r + 156, 48);
    nesp = (int)u32le(r + 308); if (nesp > 8) nesp = 8;
    for (k = 0; k < nesp; k++) memcpy(esp_w[k], r + 312 + 48 * k, 48);
    mis.nnodos = u32le(r + 696); if (mis.nnodos > RIVAL_NODOS) mis.nnodos = RIVAL_NODOS;
    memcpy(mis.largo_tramo, r + 700, 32); memcpy(mis.largo_union, r + 732, 32);
    mis.largo = (uint16_t)(r[764] | r[765] << 8);
    free(b);
    mis_ok = 1;
    return 0;
}

/* 46C570: el generador de 16 bits de 6A4E2C (* 0x5E5 + 0x29) */
static uint32_t paso_semilla(void) { semilla = (semilla * 0x5e5u + 0x29u) & 0xffffu; return semilla; }

/* 1d9aa0 (PS2): la familia del modelo (pares {familia, modelo} de 0x40b818) */
static int familia_de(int modelo)
{
    int j;
    for (j = 0; j < 32; j++) { int8_t m = (int8_t)t_fam[2 * j + 1]; if (m >= 0 && m == modelo) return (int8_t)t_fam[2 * j]; }
    return 0;
}
/* 1d9ae8 (PS2): el modelo de la clase de un especial (pares {clase, modelo} de 0x40b858; la clase 0x1E da 0x1B) */
static int modelo_clase(int cl)
{
    int j;
    if (cl == 0x1e) return 0x1b;
    for (j = 0; j < 32; j++) { int8_t c = (int8_t)t_cla[2 * j]; if (c >= 0 && c == cl) return (int8_t)t_cla[2 * j + 1]; }
    return 0;
}

int rival_n(void) { return nrc; }
rival_t *rival_reg(int i) { return i >= 0 && i < nrc ? rc + i : NULL; }
float rival_ritmo(int i) { return i >= 0 && i < nrc ? (float)rc[i].d9c / 600.0f * 0.17f + 1.0f : 1.0f; }   /* 0x4416 0x3e2e147b */
int rival_puesto(void) { return jug_puesto; }
int rival_total(void) { return clas_n; }

int rival_modelo_principal(void)
{
    int i, m = -1; float p = -1e9f;
    for (i = 0; i < nrc; i++) if (rc[i].pos > p) { p = rc[i].pos; m = rc[i].modelo; }
    return m;
}

/* 47CF40 (PS2 0x1ef900) con flag = 0 (el de las misiones: 496A30 estado 3, 47CF40(+0x1C, 0)) */
static void monta_47cf40(void)
{
    const int32_t *c = mis.cfg; int count = c[0], i, k, edi;
    float f28, f14, f2c, f38, f30, f18, f20; const float *pr = mis.perfil;
    semilla = c[11] ? (uint32_t)c[11] & 0xffffu : 0xa5deu;
    if (count < 1) count = 1;
    if (count > RIVAL_MAX) count = RIVAL_MAX;
    nrc = count; memset(rc, 0, sizeof rc);
    sin_vaiven_80fb08 = 0; bulk_80fb09 = 0;
    lider = ultimo = -1; clas_n = 0; lejos_8037b8 = 0; clas_hecha_80fb18 = 0;   /* 4774B0 */
    edi = mis.largo;                                            /* 0x1724e0(c18 = -1): todo el curso */
    largo = edi;
    f28 = (float)c[4] / (float)count; f14 = (float)c[5];
    f2c = count > 1 ? (float)(c[2] - c[1]) / (float)(count - 1) : 0.0f; f38 = (float)c[2];
    f30 = (pr[2] - pr[3]) / (float)count; f18 = f30 + pr[3]; f20 = (float)edi;
    for (i = 0; i < count; i++) {
        rival_t *r = rc + i; const uint8_t *l = t_listas[c[9] & 7]; uint32_t nl = u32le(l), roll; float f1c, x0, x2, x3, x4, x5, x6, x7, x1;
        int32_t q1c; float q20 = pr[8], q24 = pr[9], q28 = pr[10], q2c = pr[11], q30 = pr[12];
        memcpy(&q1c, &pr[7], 4);
        r->modelo = nl ? l[4 + paso_semilla() % nl] : 0;        /* 0x1d99f0: lista c24 (PS2 0x36be70) */
        roll = paso_semilla();
        if ((float)c[10] > (float)roll / 65535.0f * 100.0f) r->color = 1;   /* 0x477fff00, 0x42c8 */
        else {
            int f = familia_de(r->modelo); uint8_t col;
            if (f < 0 || f > 29) f = 0;
            do col = t_col[f * 8 + (paso_semilla() & 7)]; while (col == 1);
            r->color = col;
        }
        r->idx = (uint8_t)i; r->coche = -1; r->delante = 1;
        r->d98 = 0;                                             /* (tipo 6: el tiempo de 4961F0) */
        r->d9c = 0;
        r->pos = f14; r->pos_i = (int32_t)f14 & 0xffff;
        f1c = f20 - f14;
        r->v_ref = f1c / f38 * 17.99198341f / 295.0f;          /* 0x418fef95, 0x43938000 */
        r->ritmo = f1c / (f38 * 60.0f) * 1.043149352f;         /* 0x21d420 = 60, 0x3f8585eb */
        r->v_curva = f18;
        r->f28 = r->f24 = 0; r->per = f38; r->f38 = 1.0f;
        r->vivo = 1; r->d58 = 0; r->d6c = 0; r->especial = 0; r->d64 = 0; r->d5c = 0; r->d68 = 0;
        r->b77 = (uint8_t)i; r->rival = 0xff;
        /* los vaivenes de la velocidad (+08..+1C) con el perfil (+1C entero, +20..+30); 3612 = 60 x 60,2 */
        x5 = q2c; x6 = q30; x3 = q24 * 3612.0f; x4 = 1.0f - x5;
        x2 = (float)paso_semilla() * ((float)q1c * x5);
        x7 = (float)q1c * x4 + x2 * (1.0f / 65536.0f);
        r->amp = x7;
        x1 = 1.0f - x6;
        x0 = (float)paso_semilla() * x3 * x6 * (1.0f / 65536.0f) + x1 * x3;
        r->periodo = x0;
        r->fase = (float)paso_semilla() * x0 * (1.0f / 65536.0f);
        r->amp = r->amp / 295.0f;
        x0 = q20 * x5; x3 = q28 * 3612.0f;
        x5 = (float)paso_semilla() * x0 * (1.0f / 65536.0f) + q20 * x4;
        r->amp_c = x5;
        x0 = (float)paso_semilla() * x3 * x6 * (1.0f / 65536.0f) + x1 * x3;
        r->periodo_c = x0;
        r->fase_c = (float)paso_semilla() * x0 * (1.0f / 65536.0f);
        x0 = (float)paso_semilla() * mis.c3c[1] * (1.0f / 65536.0f) + mis.c3c[0];   /* +20 (+3C, o 0x36bcf8) */
        r->luz = x0 > 1.0f ? 1.0f : x0;
        f14 += f28; f38 -= f2c; f18 += f30;
    }
    for (k = 0; k < nesp; k++) {                                /* los especiales (+34): 0x28 bytes cada uno */
        const int32_t *e = esp_w[k]; float e1c, e20, per = f38; int slot = count - e[3], to71a = 0; rival_t *r;
        memcpy(&e1c, &e[7], 4); memcpy(&e20, &e[8], 4);
        if (slot < 0 || slot >= count) continue;
        r = rc + slot;
        r->modelo = (uint8_t)modelo_clase(e[0]); r->color = (uint8_t)e[1];
        if (e1c != 0.0f) r->f38 = e1c;
#define TIEMPO() do { if (e[6]) { r->pos = (float)e[6]; r->pos_i = (int32_t)r->pos & 0xffff; \
                      r->ritmo = ((float)edi - r->pos) / (r->per * 60.0f) * 1.043149352f; } } while (0)
        if (c[3] != 0) {
            float x1 = count > 1 ? (float)(c[2] - c[3]) / (float)(count - 1) : 0.0f;
            r->per = per = (float)(e[3] - 1) * x1 + (float)c[3]; TIEMPO(); to71a = 1;
        } else if (e[5] != 0) { r->per = per = (float)e[5]; TIEMPO(); to71a = 1; }
        else { if (e[4]) r->v_ref = (float)e[4] / 295.0f; TIEMPO(); }
#undef TIEMPO
        if (to71a) r->v_ref = (float)(edi - r->pos_i) / per * 17.99198341f / 295.0f;
        if (e20 != 0.0f) r->v_curva = e20;
        if (e[2] >= 0 && e[2] < 6) {
            r->rival = (uint8_t)e[2]; r->especial = 1;
            if (e[9]) {
                float a, bb, x; memcpy(&a, &e[10], 4); memcpy(&bb, &e[11], 4);
                x = (float)paso_semilla() * bb * (1.0f / 65536.0f) + a; r->luz = x > 1.0f ? 1.0f : x;
            }
        }
    }
    if (!(c[12] & 8) && !(c[12] & 0x10)) sin_vaiven_80fb08 = 1;
}

/* 477510: la clasificacion (de mas a menos avance) con el jugador */
static void clasifica_477510(const rival_jug_t *j)
{
    int i, n = 0, a, b;
    for (i = nrc - 1; i >= 0; i--) {
        rival_t *r = rc + i; entrada_t *e;
        if (!r->vivo) continue;
        e = clas + n++;
        e->pos = r->pos_i; e->frac = r->pos < 0 ? 0 : r->pos - floorf(r->pos); e->pf = (float)e->pos + e->frac;
        e->dif = r->pos_i - j->pos; e->idx = i;
    }
    {   entrada_t *e = clas + n++;
        e->pos = j->pos; e->frac = j->frac < 0 ? 0 : j->frac - floorf(j->frac); e->pf = (float)j->pos + e->frac; e->dif = 0; e->idx = -1; }
    clas_hecha_80fb18 = 1;
    for (a = 1; a < n; a++)                                     /* 4772D0: posicion y fraccion de mayor a menor */
        for (b = a; b > 0; b--) {
            entrada_t *x = clas + b - 1, *y = clas + b, t;
            if (x->pos > y->pos || (x->pos == y->pos && x->frac >= y->frac)) break;
            t = *x; *x = *y; *y = t;
        }
    lider = clas[0].idx; ultimo = clas[n - 1].idx; clas_n = n;
    for (i = 0; i < n; i++) {
        if (clas[i].idx < 0) { jug_puesto_ant = jug_puesto; jug_puesto = i; continue; }
        rc[clas[i].idx].puesto = (uint8_t)i; rc[clas[i].idx].dif = clas[i].dif;
    }
    lejos_8037b8 = 0;
    if (lider >= 0) return;
    if (n >= 2 && clas[1].dif < -0x19) lejos_8037b8 = 1;      /* el jugador lidera por mas de 25 posiciones */
}

/* 476480 / 4764D0 (sin vueltas): el registro sin coche que cruza al jugador salta fuera de la vista */
static void tira_476480(rival_t *r, int x)
{
    float v = (float)x + 61.0f;                                 /* (puesto del jugador - +74) x 0 + x + 61 */
    if (v > r->pos) { r->pos = v; r->pos_i = (int32_t)v; }
}
static void tira_4764d0(rival_t *r, int x)
{
    float v = (float)x - 10.0f - 1.0f;
    if (!(v > 0.0f) || !(r->pos > v)) return;
    r->pos = v; r->pos_i = (int32_t)v & 0xffff;
}

/* 47BC90 (sin vueltas): avance del registro sin coche y apertura de su coche cerca del jugador */
static void avanza_47bc90(int i, const rival_jug_t *j, float ritmo, float arranque, int modo16)
{
    rival_t *r = rc + i; int esi = j->pos, edi, ebx;
    if (modo16 && j->corre) { float v = r->ritmo * ritmo * arranque + r->pos; r->pos = v; r->pos_i = (int32_t)v & 0xffff; }
    else { cd_delante_800d98 = 0; cd_detras_80a5a0 = 0; }
    if (r->d6c) return;
    edi = r->pos_i; ebx = edi - esi;
    if (ebx > 0) {
        if (!r->delante) { tira_476480(r, esi); return; }
        if (ebx >= 0x3c) return;
        if (cd_delante_800d98 || !rival_abre || rival_abre(i, ebx, 1, !modo16) < 0) { tira_476480(r, esi); return; }
        cd_delante_800d98 = 0x5a;                               /* (4B00D0 = 0 fuera de la variante 9) */
        return;
    }
    if (r->delante) { tira_4764d0(r, esi); return; }
    if (ebx < -10) return;
    if (cd_detras_80a5a0 || !rival_abre || rival_abre(i, ebx, 0, !modo16) < 0) { tira_4764d0(r, esi); return; }
    cd_detras_80a5a0 = 0x5a;
}

/* 47C8A0 / 47CA40: escala y arranque comunes */
static void escalas(const rival_jug_t *j, float *escala, float *arranque)
{
    *escala = 2.0f;
    if (lejos_8037b8) *escala = mis.perfil[5] * 2.0f;           /* perfil +0x14 */
    else if (0.7383999825f > j->v && j->cuadros >= 0x5e1) *escala = mis.perfil[4] * 2.0f;   /* 0x3f3cffe8; +0x10 */
    *arranque = 1.0f;
    if (j->cuadros < 0x5e1) *arranque = (float)j->cuadros / 1505.0f;   /* 0x44bc2000 */
}
float rival_escala_v(const rival_jug_t *j)
{
    if (lejos_8037b8) return mis.perfil[5];
    if (0.7383999825f > j->v && j->cuadros >= 0x5e1) return mis.perfil[4];
    return 1.0f;
}

/* 47C8A0 (el del montaje, 47D8B0) */
static void aparecen_47c8a0(const rival_jug_t *j, int modo16)
{
    float esc, arr; int i;
    if (cd_delante_800d98) cd_delante_800d98--;
    if (cd_detras_80a5a0) cd_detras_80a5a0--;
    escalas(j, &esc, &arr);
    for (i = 0; i < nrc; i++) {
        if (rc[i].coche >= 0 || !rc[i].vivo) continue;          /* (los de coche: 477090 en la carrera) */
        avanza_47bc90(i, j, rival_ritmo(i) * esc, arr, modo16);
    }
}

/* 47CA40: desde el puesto del jugador hacia atras y hacia delante */
static void aparecen_47ca40(const rival_jug_t *j)
{
    float esc, arr; int k, c36 = jug_puesto;
    if (cd_delante_800d98) cd_delante_800d98--;
    if (cd_detras_80a5a0) cd_detras_80a5a0--;
    escalas(j, &esc, &arr);
    for (k = c36 + 1; k < clas_n; k++) {                        /* los de detras del jugador */
        int i = clas[k].idx; rival_t *r;
        if (i < 0) continue;
        r = rc + i;
        if (r->coche >= 0 || !r->vivo) continue;
        if (r->dif < -0x14 && bulk_80fb09) {                    /* los demas, de una vez */
            float comb = arr * esc;
            for (; k < clas_n; k++) {
                int q = clas[k].idx; float x;
                if (q < 0) continue;
                x = rival_ritmo(q) * rc[q].ritmo * comb + rc[q].pos; rc[q].pos = x; rc[q].pos_i = (int32_t)x & 0xffff;
            }
            break;
        }
        avanza_47bc90(i, j, rival_ritmo(i) * esc, arr, 1);
    }
    for (k = c36 - 1; k >= 0; k--) {                            /* los de delante */
        int i = clas[k].idx; rival_t *r;
        if (i < 0) continue;
        r = rc + i;
        if (r->coche >= 0 || !r->vivo) continue;
        if (r->dif > 0x50 && bulk_80fb09) {
            float comb = arr * esc;
            for (; k >= 0; k--) {
                int q = clas[k].idx; float x;
                if (q < 0) continue;
                x = rival_ritmo(q) * rc[q].ritmo * comb + rc[q].pos; rc[q].pos = x; rc[q].pos_i = (int32_t)x & 0xffff;
            }
            return;
        }
        avanza_47bc90(i, j, rival_ritmo(i) * esc, arr, 1);
    }
}

/* 496A30 estado 3: 47CF40 (sin 47CF40 con +0x14 == 4) */
void rival_prepara(void)
{
    paso_802e08 = 0; cd_delante_800d98 = cd_detras_80a5a0 = 0;
    if (!mis_ok || mis.b14 == 4) { nrc = 0; return; }
    monta_47cf40();
}
/* 47DAC0 / 47D8B0 al empezar la carrera (fuera del modo 16: sin avance, los de delante en los carriles 2 / 3) */
void rival_salida(const rival_jug_t *j)
{
    if (!nrc) return;
    clasifica_477510(j); aparecen_47c8a0(j, 0);
    clasifica_477510(j); clasifica_477510(j);
}

/* 47EC00 (los coches de carreras): cuadros impares 47CF00 (47CA40), pares la clasificacion */
void rival_paso(const rival_jug_t *j)
{
    if (!nrc) return;
    paso_802e08++;
    if (paso_802e08 & 1) {
        if (j->corre) bulk_80fb09 = 1;                          /* 47CF00: modo 16 y 49B2D0 < 0x3C */
        if (clas_hecha_80fb18) aparecen_47ca40(j); else aparecen_47c8a0(j, 1);
    } else clasifica_477510(j);
}

/* 477090: el registro sigue a su coche (la posicion no baja nunca) */
void rival_sigue(int i, float pos, int pos_i)
{
    rival_t *r = rival_reg(i);
    if (!r) return;
    if (!(pos < r->pos)) { r->pos = pos; r->pos_i = pos_i; }
}
/* 4763D0: el coche se cierra; el registro sigue solo */
void rival_suelta(int i, int jug_pos)
{
    rival_t *r = rival_reg(i);
    if (!r || r->coche < 0) return;
    r->delante = r->pos_i - jug_pos > 0;
    r->coche = -1;
    cd_delante_800d98 = 0; cd_detras_80a5a0 = 0;
}

/* 495C20, tipo 0: 6 - puesto (0 el ultimo) */
int rival_nota(void)
{
    int v = 6 - jug_puesto;
    if (v <= 0) v = 0;
    if (jug_puesto == clas_n - 1) v = 0;
    return v;
}

/* 47DA80 (PS2 0x1f0c60): el coche de carreras de referencia del HUD: el lider ([64E190]) o, si lidera el jugador, el
   segundo de la clasificacion; -1 sin coches */
int rival_47da80(void)
{
    if (!nrc || clas_n <= 1) return -1;
    return lider >= 0 ? lider : clas[1].idx;
}
int rival_largo(void) { return largo; }
int rival_millas(int nota) { return nota >= 0 && nota < 8 ? (int)t_millas[nota] : 0; }
/* 495A60: la carrera siguiente de la clave (subclave + 1) si la hay; -1 si no */
int rival_siguiente(int i)
{
    int k;
    if (!lista_ok || i < 0 || i >= RIVAL_NMIS) return -1;
    for (k = 0; k < RIVAL_NMIS; k++) if (lista_cl[k] == lista_cl[i] && lista_sub[k] == lista_sub[i] + 1) return k;
    return -1;
}
