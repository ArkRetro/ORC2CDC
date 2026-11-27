/* campeonato.c: los campeonatos de COAST 2 COAST (variante 5 de [780258] = PS2 0x443508), como el original.

   Fuente: SLES-53998 (el PC, 7F9460, no tiene fuente: alli corre el x86 traducido). El gestor es el suceso 0x190
   (descriptor en la tabla 0x3ee900: {0x22db28, 0x22db50, 0x22db70, 0, 0x22dbb0}) sobre el objeto 0x616238 (los
   desplazamientos de abajo son los suyos) con sus metodos 0x229e88 (inicio), 0x22a048 (control), 0x22a2b0 (dibujo),
   0x22a5b8 (HUD), 0x22ac00 (etapa) y 0x22b470 (fin); las peticiones las llevan 24 objetos, uno por tipo (0x22a930: la
   fabrica de 0x4151f0, clases en 0x292788..0x2a4800), todos llamados en cada paso por su vtable +0x24.
   - Guion (0x22ac00 / 0x22b600): por cada etapa, la lista +0x1ff8 (REQUEST_NORMAL / SPECIAL1..3) del Req_<etapa>.bin
     del tipo de su descriptor (CAMP.DAT, tools/conv_camp.py); cada peticion es una seccion de carretera [inicio, fin].
   - Lista de la etapa siguiente (0x22e850, al entrar en la union): con la salida forzada desde el mapa (+0x68) la del
     mapa (+0x70); si no, por el histograma de notas de los niveles anteriores (0x22e808: ninguna nota menor que n).
   - Peticion en curso (0x22b7d0): la primera con inicio - 32 < seccion < fin + 30; fases (0x232988): 1 anuncio (desde
     inicio - 30), 2 espera, 3 empieza (inicio), 4 en curso, 5 acaba (fin: nota 0x22e670 y corazones 0x22e728), 6 la
     nota a la etapa (0x22e498) con su rotulo y su voz, 7 hecha.
   - Medidor +0x28 (0..80) que llena cada tipo con el avance del coche mientras cumple; penalizacion +0x2c (0x22c818:
     choques, golpes, muro, ruedas fuera, roces; tope 100); nota = el mayor k con medidor - penalizacion >= umbral[k]
     (0, 10, 20, 30, 45, 60, 80), tope 4 con un golpe, -3 con uno mas rapido, -4 con un choque (0x22e6d8).
   - Corazones: los del medidor (tope 80) y con nota >= 4 los extra de la lista, de la dificultad y v media x 0,2; con
     nota 6 cada tipo suma su bono (vtable +0x24, fase 5). Fuera de las peticiones un choque quita 50, un golpe de uno
     mas rapido 20 y un golpe 10 (0x22c818). La etapa: media de las notas de sus peticiones (0x22ebb0), a los cuartetos
     de la licencia +0x36E + 2 tipo en la casilla de su lista si mejora (0x22edd8) con millas[nota] x (lista + 1).
   Tipos portados (su control de la vtable +0x24): 1 velocidad (0x29c828), 2 derrape (0x298360), 3 adelantar
   (0x29f038), 4 no chocar (0x29ec18), 12 rebufo (0x2a4198). Los demas (5 colores, 6 conos, 7 monedas, 8 arcos, 11 / 13 /
   14 / 15 / 16 / 19..24: objetos del guion, minijuegos, coches de la peticion) se anuncian y se evaluan con el medidor a
   cero: SIN PORTAR (ver docs/PARIDAD.md, campeonatos). */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "gd.h"
#include "campeonato.h"
#include "haobj.h"
#include "minijuego.h"

int (*camp_graba)(int tipo, int lista, int nota);
void (*camp_voz)(int id);
void (*camp_se)(int id);
void (*camp_millas)(int m);
void (*camp_obj)(int cl, int tipo, float sec, float lat, float esc);
void (*camp_chica)(int que, uint32_t v);
void (*camp_golpe)(void);
void (*camp_punto)(int sec, float *p, float *fx, float *fz);
void (*camp_marco)(const float *l, float *w);
void (*camp_bancos)(int n, const uint8_t *tipo, const uint8_t *sub, int fila, int *rnd);

#define DT 0.016611295f                                 /* +0x180 (0x3c881469) */
#define LLENO 80.0f                                     /* 0x3741a8 */
#define NIV 5
#define NSND 19                                         /* 'SND1': los efectos de los tipos 15 y 19 (abajo) */
typedef struct {                                        /* CAMP.DAT: la peticion */
    int16_t ini, fin; uint8_t tipo, sub, rot, b46; uint16_t voz, _r; int32_t p[10];   /* p[k] = +0x60 + 4 k */
} cpet_t;
typedef struct {
    char m[4]; uint32_t ver, npet, opet;
    float umbral[7]; int32_t tope, extra_lista[4], extra_dif[5], millas[7];
    float penal[17][2][6];
    uint16_t voz_nota[4][7], voz_animo[2][5];
    uint8_t rot_nota[7], marco_nota[2], marco_anuncio, rot_generico, _p;
    uint16_t lista[60][4]; uint8_t arbol[3][15];
} cdat_t;
typedef struct {                                        /* CAMP.DAT 'OBJ1': una fila de objetos de una peticion */
    uint16_t pet; int16_t s0, s1; uint8_t n, clase, banco, _p; float l0, l1, f;
} cobj_t;
typedef struct {                                        /* el registro 0x74 de la peticion (+0x20 los contadores) */
    const cpet_t *d; int fase, ant, nota, cor, extra;   /* +8, +4, +0x24, +0x30, +0x34 */
    float med, pen, v_ini, v_fin, v_med, v_suma, t;     /* +0x28, +0x2c, +0x5c, +0x60, +0x64, +0x68, +0x6c */
    int n, c38, c40, c44, c48, c4c, c50, c54;           /* +0x70, contadores de 0x22c818 */
    const float *P;                                     /* +0xc de los parametros (0x2323e8) */
    int h70, h74, h78, tmr; float h7c;                  /* +0x70.. de los parametros (los usa cada tipo); +0xc del objeto del tipo */
    int rnd;                                            /* +8 de los parametros: el azar del banco de voces (0x22ac00) */
} pet_t;
typedef struct {                                        /* el registro 0x74 del tipo de etapa (+0x1fc + 0x74 tipo) */
    int tipo, cuenta, idx, cor, nota_media, nota[4], pnota[3], pcor[3], hist[7];   /* +0x200, +0x204, +0x208, +0x224.. */
} etapa_t;
typedef struct {
    cdat_t *d; const cpet_t *pets; const cobj_t *objs; uint32_t nobj; const uint8_t *chi;   /* chi: 'CHI1' */
    const uint8_t *cls; int ncls; const uint16_t *snd; int obj_s0;   /* 'CLS1', 'SND1'; la seccion del ultimo objeto puesto */
    int fila, forzada, lista_cfg, dif, lista, nivel;    /* +0xf0, +0x68, +0x70, +0x1ff8, +0x168 */
    etapa_t et[NIV]; pet_t pet[3]; int npet, cur;       /* +0x7c (la lista), +0x80 */
    float pos, pos_ant; int en_union_ant, union_hecha;  /* +0x3e78 / +0x3e7c, +0x3de0 */
    int total, disp, ultima_nota;                       /* +0x1fe4, +0x1fe8 */
    uint32_t reloj, voz_ult; struct { int id, retardo, prio; } vc[8]; int vc_cab, vc_fin;   /* 0x1ac3d8 */
    int ban_on, ban_m4c, ban_txt, ban_mar, ban_n, ban_abre, ban_cierra, ban_ver;          /* rotulo */
    int animo_ult, nota3, nota1;                        /* 0x3b5678, 0x6213b0, 0x6213b8 */
    const uint16_t *se11; int k11_tmr, k11_golpes;      /* 'C111' (0x422e00); +0xc del objeto del tipo 11; golpes del paso */
    uint8_t ver11[24]; int16_t fr11[24]; float p11[24][3];   /* el dibujo de los coches de la peticion (camp_spr3d) */
    const uint16_t *let; int let_k;                     /* 'LET1': los letreros de inicio / fin (0x232e60) y el puesto */
    float xyz[3], fx, fz;                               /* el jugador (del paso anterior: camp_autos va antes) */
    int k20, k21_pl, k21_t;                             /* lineas cruzadas en el paso; el OVNI que lleva al jugador (+0x18) y +0xc */
    const int32_t *t14; const uint8_t *c14; uint32_t n14;   /* 'C141': las tablas de valores y los coches de cada peticion */
    const uint8_t *c14p; int n14c, nx14, objetivo, suma, k14_cambio, k14_toq;   /* la peticion del 14 en curso: sus coches, el siguiente */
    int16_t v14[100];                                   /* el valor de cada coche (+0x10 de su registro de +0x5c) */
    int16_t val11[24];                                  /* el valor del coche de cada plaza (su dibujo) */
    const uint8_t *c24; uint32_t n24;                   /* 'C241': los objetivos de las fotos de cada peticion */
    const uint8_t *c24p; int n24c, rel24, act24, nota24; uint16_t hecho24;   /* la del 24 en curso; el activo (+0x10, -1) */
    const uint8_t *c22; uint32_t n22;                   /* 'C221': las ventanas de la bocina de cada peticion */
    const uint8_t *c22p; int n22c, est22, obj22, cur22; uint32_t hecho22; float tot22, ant22;   /* +0xc, +0x18, +0x2c, +0x24; +0x14, +0x1c */
} camp_t;
static camp_t *C;

int camp_activo(void) { return C != NULL; }
int camp_clases(const uint8_t **v) { *v = C ? C->cls : NULL; return C ? C->ncls : 0; }
/* un efecto de una fila de 'SND1' al azar entre los n primeros (0x11c090 con tabla[rand % n]: 0x422cd0 la pelota, 7;
   0x423820 / 0x423830 / 0x423840 / 0x423850 los meteoritos, 3 cada una) */
static void se_de(int base, int n) { int id; if (!C->snd || !camp_se) return; id = C->snd[base + rand() % n]; if (id) camp_se(id); }
int camp_tipo(int g, int nodo) { return C && g >= 0 && g < 3 && nodo >= 0 && nodo < 15 ? C->d->arbol[g][nodo] : -1; }
void camp_libera(void)
{
    if (!C) return;
    if (camp_obj) camp_obj(-2, 0, 0, 0, 0);
    if (camp_bancos) camp_bancos(-1, NULL, NULL, 0, NULL);
    mj_libera();
    free(C->d); free(C); C = NULL;
}
int camp_inicia(int grupo, int fila, int forzada, int lista, int dificultad)
{
    uint32_t tam; cdat_t *d;
    camp_libera();
    if (!(d = gd_carga("CAMP.DAT", &tam))) { dbglog(DBG_ERROR, "CAMP: sin CAMP.DAT\n"); return -1; }
    if (tam < sizeof(cdat_t) || memcmp(d->m, "CMP1", 4) || d->opet != sizeof(cdat_t) || d->opet + d->npet * sizeof(cpet_t) > tam) {
        dbglog(DBG_ERROR, "CAMP: CAMP.DAT no vale\n"); free(d); return -2;
    }
    if (!(C = calloc(1, sizeof *C))) { free(d); return -3; }
    (void)grupo; C->d = d; C->pets = (const cpet_t *)((const uint8_t *)d + d->opet);
    {   uint32_t o = d->opet + d->npet * sizeof(cpet_t), n;   /* los objetos (detras; los CAMP.DAT de antes no los tienen) */
        if (o + 8 <= tam && !memcmp((const uint8_t *)d + o, "OBJ1", 4) && (memcpy(&n, (const uint8_t *)d + o + 4, 4), o + 8 + n * sizeof(cobj_t) <= tam)) {
            C->objs = (const cobj_t *)((const uint8_t *)d + o + 8); C->nobj = n;
            o += 8 + n * sizeof(cobj_t);
            if (o + 4 + 56 + 8 <= tam && !memcmp((const uint8_t *)d + o, "CHI1", 4)) { C->chi = (const uint8_t *)d + o + 4; o += 4 + 56 + 8; }
            if (C->chi && o + 8 <= tam && !memcmp((const uint8_t *)d + o, "CLS1", 4) && (memcpy(&n, (const uint8_t *)d + o + 4, 4), o + 8 + 8 * n <= tam)) {
                C->cls = (const uint8_t *)d + o + 8; C->ncls = (int)n; o += 8 + 8 * n; }
            if (C->cls && o + 4 + 2 * NSND <= tam && !memcmp((const uint8_t *)d + o, "SND1", 4)) {
                C->snd = (const uint16_t *)((const uint8_t *)d + o + 4); o += 4 + 2 * NSND;
                if (o + 8 <= tam && !memcmp((const uint8_t *)d + o, "ZON1", 4)) { memcpy(&n, (const uint8_t *)d + o + 4, 4); o += 8 + 20 * n; }
                if (o + 12 <= tam && !memcmp((const uint8_t *)d + o, "C111", 4)) {
                    C->se11 = (const uint16_t *)((const uint8_t *)d + o + 4); o += 12;
                    if (o + 8 <= tam && !memcmp((const uint8_t *)d + o, "LET1", 4)) {
                        C->let = (const uint16_t *)((const uint8_t *)d + o + 4); o += 8;
                        if (o + 8 + 264 <= tam && !memcmp((const uint8_t *)d + o, "C141", 4)) {
                            memcpy(&C->n14, (const uint8_t *)d + o + 4, 4); C->t14 = (const int32_t *)((const uint8_t *)d + o + 8); C->c14 = (const uint8_t *)d + o + 8 + 264;
                            {   const uint8_t *e = C->c14; uint32_t k;   /* detras, 'C241' (los objetivos del 24) */
                                for (k = 0; k < C->n14 && e + 4 <= (const uint8_t *)d + tam; k++) e += 4 + 4 * e[2];
                                if (e + 8 <= (const uint8_t *)d + tam && !memcmp(e, "C241", 4)) { memcpy(&C->n24, e + 4, 4); C->c24 = e + 8;
                                    for (e += 8, k = 0; k < C->n24 && e + 4 <= (const uint8_t *)d + tam; k++) e += 4 + 16 * e[2];
                                    if (e + 8 <= (const uint8_t *)d + tam && !memcmp(e, "C221", 4)) { memcpy(&C->n22, e + 4, 4); C->c22 = e + 8; } }
                            }
                        }
                    }
                }
            } }
    }
    mj_inicia((const uint8_t *)d, tam);               /* (el tipo 16: comun/minijuego.c) */
    C->fila = fila >= 0 && fila < 4 ? fila : 0; C->forzada = forzada; C->lista_cfg = lista & 3;
    C->dif = dificultad < 0 ? 0 : dificultad > 4 ? 4 : dificultad;
    C->lista = 0; C->nivel = -1; C->cur = -1; C->reloj = 0x10000; C->animo_ult = -1; C->let_k = -1; C->k21_pl = -1;
    dbglog(DBG_INFO, "CAMP: fila %d, forzada %d lista %d, %u bytes\n", C->fila, forzada, C->lista_cfg, (unsigned)(tam + sizeof *C));
    return 0;
}

/* ---- voces: la cola 0x1ac3d8 / 0x1ac330 (la del INFARTO, PS2 0x1ac3d8 = 45BD30 del PC) */
static void suena(int id) { if ((id & 0x1ff) && (id & 0x1ff) != 0x1ff && camp_voz) camp_voz(id & 0x1ff); }
static void voz(int id, int retardo, int prio)
{
    int k;
    if (!(C->reloj - C->voz_ult <= (uint32_t)C->vc[C->vc_cab].retardo)) {
        C->voz_ult = C->reloj; C->vc[C->vc_cab].retardo = retardo; C->vc[C->vc_cab].prio = prio; C->vc[C->vc_cab].id = id;
        suena(id); return;
    }
    if (C->vc[C->vc_fin].prio >= prio) return;
    C->vc_fin = (C->vc_fin + 1) & 7; k = C->vc_fin;
    C->vc[k].prio = prio; C->vc[k].id = id; C->vc[k].retardo = retardo;
}
static void voz_bomba(void)
{
    if (C->vc_cab == C->vc_fin || C->reloj - C->voz_ult <= (uint32_t)C->vc[C->vc_cab].retardo) return;
    C->voz_ult = C->reloj; C->vc_cab = (C->vc_cab + 1) & 7; suena(C->vc[C->vc_cab].id);
}
static void voz_calla(void) { C->vc_cab = C->vc_fin; }   /* 0x1ac318 */
/* 0x2a6240: la de animo (al azar de 5 sin repetir la ultima; 0x423ee0 los tipos 1 y 2, 0x423ef8 los demas) */
static void animo(int tipo, int k)
{
    if (k < 0) k = rand() % 5;
    if (k == C->animo_ult && ++k >= 5) k = 0;
    C->animo_ult = k; voz(C->d->voz_animo[tipo == 1 || tipo == 2 ? 0 : 1][k], 0x3c, 5);
}

/* ---- rotulo (0x22f9d0 el del anuncio, 0x22f7a0 el de la nota; los de HAHUD.DAT del INFARTO: marco 2B0020 y el texto
   generico 2B003D que a los 30 cuadros pasa al de la peticion; SIN la deformacion de entrada / salida) */
static void rotulo_abre(int modo, int texto, int marco, int abre, int cierra)
{
    if (texto == 0xff) return;
    C->ban_on = 1; C->ban_m4c = modo; C->ban_txt = texto; C->ban_mar = marco; C->ban_n = 0; C->ban_abre = abre;
    C->ban_cierra = cierra; C->ban_ver = modo ? texto : C->d->rot_generico;
}
static void rotulo_paso(void)
{
    if (!C->ban_on) return;
    if (C->ban_n <= 10) C->ban_n++;
    else if (!C->ban_m4c) {
        if (C->ban_abre < 0 || C->ban_n - 20 < C->ban_abre) C->ban_n++;
        else { C->ban_ver = C->ban_txt; C->ban_m4c = 1; C->ban_n = 0; }
    } else if (C->ban_cierra < 0 || C->ban_n - 10 < C->ban_cierra) C->ban_n++;
    else C->ban_on = 0;
}
int camp_rotulo(int *texto, int *marco, int *cuadro)
{
    if (!C || !C->ban_on || C->ban_ver == 0xff) return 0;
    *texto = C->ban_ver; *marco = C->ban_mar; *cuadro = C->ban_n;
    return 1;
}
uint32_t camp_tinte(void) { return 0xffffffffu; }

/* ---- la etapa */
static etapa_t *et_cur(void) { return C->nivel >= 0 && C->nivel < NIV ? &C->et[C->nivel] : NULL; }
/* 0x22f030: los corazones de la carrera (+0x1fe4) */
static void suma_total(void)
{
    int k, t = 0;
    for (k = 0; k <= C->nivel && k < NIV; k++) t += C->et[k].cor;
    C->total = t;
}
/* 0x2323e8: la fila de penalizaciones del tipo (0x374240: tipo 5 -> +0xf0, 2 -> +0x60, 16 -> +0x300, si no tipo x
   0x30 + 0x18 x (subtipo < 2 ? subtipo : 1)) */
static const float *penal(const cpet_t *d)
{
    const float *b = &C->d->penal[0][0][0]; int o;
    if (d->tipo == 5) o = 0xf0; else if (d->tipo == 2) o = 0x60; else if (d->tipo == 16) o = 0x300;
    else o = d->tipo * 0x30 + 0x18 * (d->sub < 2 ? d->sub : 1);
    o /= 4; return o + 6 <= 204 ? b + o : b;
}
void camp_etapa(int tipo)
{
    etapa_t *e; unsigned w; int i, n;
    if (!C) return;
    if (++C->nivel >= NIV) C->nivel = NIV - 1;
    if (C->forzada) C->lista = C->lista_cfg;            /* 0x22ac00: +0x1ff8 = +0x70 con +0x68 */
    e = et_cur(); memset(e, 0, sizeof *e); e->tipo = tipo; e->idx = -1;
    for (i = 0; i < 4; i++) e->nota[i] = 7;
    C->npet = 0; C->cur = -1; C->union_hecha = 0; C->let_k = -1; C->k21_pl = -1;
    if (camp_obj) camp_obj(-1, 0, 0, 0, 0);
    if (camp_chica) camp_chica(-1, 0);                  /* (CHRH.DAT ya leido al empezar la etapa) */
    w = tipo >= 0 && tipo < 60 ? C->d->lista[tipo][C->lista] : 0xffff;
    n = w == 0xffff ? 0 : (int)(w & 15);
    for (i = 0; i < n && i < 3; i++) {                  /* 0x22b600: 0x22e630 (a cero) y 0x2323e8 */
        pet_t *p = &C->pet[i]; memset(p, 0, sizeof *p);
        p->d = &C->pets[(w >> 4) + i]; p->P = penal(p->d); p->rnd = -1; C->npet++; e->cuenta++;
    }
    /* 0x22ac00, estados 0..2: los bancos de voces de las peticiones (y su azar en +8), +0xf0 = rand % 4 (la fila de las
       voces de la nota, 0x2a65a8) y su banco RANK (comun/vozmis.c) */
    C->fila = rand() & 3;
    if (camp_bancos) {
        uint8_t t[3], b[3]; int r[3];
        for (i = 0; i < C->npet; i++) { t[i] = C->pet[i].d->tipo; b[i] = C->pet[i].d->sub; }
        camp_bancos(C->npet, t, b, C->fila, r);
        for (i = 0; i < C->npet; i++) C->pet[i].rnd = r[i];
    }
    dbglog(DBG_INFO, "CAMP: nivel %d etapa tipo %d lista %d, %d peticiones:", C->nivel, tipo, C->lista, C->npet);
    for (i = 0; i < C->npet; i++) dbglog(DBG_INFO, " %d/%d (%d..%d)", C->pet[i].d->tipo, C->pet[i].d->sub, C->pet[i].d->ini, C->pet[i].d->fin);
    dbglog(DBG_INFO, "\n");
}
/* 0x22e808: ninguna nota menor que n en el nivel k */
static int sin_menores(int k, int n)
{
    int i;
    if (k < 0 || k >= NIV) return 1;
    for (i = 0; i < n && i < 7; i++) if (C->et[k].hist[i]) return 0;
    return 1;
}
/* 0x22e850: la lista de la etapa siguiente, al entrar en la union */
static void elige_lista(void)
{
    int ant = C->lista, v = C->nivel;
    C->lista = 0;
    if (C->forzada) { C->lista = C->lista_cfg; return; }
    if (v == 1) C->lista = sin_menores(0, 4) && sin_menores(1, 4) ? 1 : 0;
    else if (v == 2) {
        C->lista = sin_menores(2, 3) ? 1 : 0;
        if (ant == 1 && sin_menores(2, 4)) C->lista = 2;
    } else if (v == 3) {
        C->lista = sin_menores(3, 3) ? 1 : 0;
        if (ant > 0 && sin_menores(3, 4)) C->lista = 2;
        if (ant == 2 && sin_menores(3, 5)) C->lista = 3;
    }
}
void camp_fin_etapa(void)                               /* 0x22c000 con 0x1cec50: 0x22edd8 y las millas */
{
    etapa_t *e;
    if (!C || !(e = et_cur())) return;
    if (camp_graba && e->nota[C->lista] != 7) {         /* (si mejora: millas[nota] x (lista + 1) a la licencia, 0x1f41c0) */
        int m = camp_graba(e->tipo, C->lista, e->nota[C->lista]) ? C->d->millas[e->nota[C->lista]] * (C->lista + 1) : 0;
        if (m && camp_millas) camp_millas(m);
        dbglog(DBG_INFO, "CAMP: etapa %d lista %d nota %d, %d corazones (%d)\n", e->tipo, C->lista, e->nota[C->lista], e->cor, m);
    } else dbglog(DBG_INFO, "CAMP: etapa %d sin nota (%d/%d)\n", e->tipo, e->idx + 1, e->cuenta);
}

/* ---- la peticion: nota y corazones */
static void nota_de(pet_t *p)                           /* 0x22e670 + 0x22e6d8 */
{
    float g = p->med - p->pen; int k, t = 6;
    p->nota = 0;
    for (k = 0; k < 7; k++) if (C->d->umbral[k] <= g) p->nota = k;
    if (p->c40) t = 4;
    if (p->c44) t -= 3;
    if (p->c48) t -= 4;
    if (t < p->nota) p->nota = t;
    if (p->nota < 0) p->nota = 0;
}
static void corazones_de(pet_t *p)                      /* 0x22e728 */
{
    int c = (int)(p->med - p->pen);
    if (c < 0) c = 0;
    if (c > C->d->tope) c = C->d->tope;
    p->cor = c;
    if (p->nota >= 4) p->extra += C->d->extra_lista[C->lista] + C->d->extra_dif[C->dif] + (int)(p->v_med * 0.2f);
}
static void a_la_etapa(pet_t *p)                        /* 0x22e498 + 0x22ebb0 + 0x22ed30 */
{
    etapa_t *e = et_cur(); int i, s = 0, h = p->cor + p->extra;
    if (!e || e->idx < 0 || e->idx > 2) return;
    e->cor += h; e->pnota[e->idx] = p->nota; e->pcor[e->idx] = h;
    memset(e->hist, 0, sizeof e->hist);
    for (i = 0; i <= e->idx; i++) { e->hist[e->pnota[i]]++; s += e->pnota[i]; }
    e->nota_media = s / (e->idx + 1);
    if (e->idx >= e->cuenta - 1 && (e->nota[C->lista] == 7 || e->nota[C->lista] < e->nota_media)) e->nota[C->lista] = e->nota_media;
}
/* 0x22f090: el bono por la velocidad: 0 si v_ini - v_fin < 10 o >= 30; (30 - d) / 20 entre medias */
static float frena(float a, float b) { float d = a - b; return d < 10.0f || d >= 30.0f ? 0.0f : fminf(1.0f, (30.0f - d) / 20.0f); }
static int bono(const pet_t *p, float k)                /* 30 x el exceso del medidor sobre 80 hasta 80 / k, + 40 x 0x22f090 */
{
    float x = 80.0f / k - LLENO, r = x > 0.0f ? (p->med - p->pen - LLENO) / x : 1.0f;
    if (r > 1.0f) r = 1.0f;
    if (r < 0.0f) r = 0.0f;
    return (int)(r * 30.0f) + (int)(40.0f * frena(p->v_ini, p->v_fin));
}

/* ---- los objetos de la peticion (fase 1 de los tipos 6, 7 y 8): 0x293968 reparte cuantos de la seccion inicial a la
   final (sin llegar) con el lateral de una punta a otra; 0x2938d0 uno suelto (cuantos 1); 0x297e20 su tipo de control
   (el switch por la clase: 0..1 y 5, 8, 11..13 conos, 2..3 monedas, 4 arcos) y la clase del banco (CAMP.DAT). Devuelve
   cuantos hay de la clase de objeto k (0x295fa8 / 0x2958f8 / 0x29a890: los que cuentan para el medidor) */
static int pon_objetos(const pet_t *p, int k)
{
    uint32_t i; int j, n = 0, pi = (int)(p->d - C->pets);
    if (!camp_obj) return 0;
    camp_obj(-1, 0, 0, 0, 0);
    for (i = 0; i < C->nobj; i++) {
        const cobj_t *o = C->objs + i;
        int t = o->clase == 2 || o->clase == 3 ? 6 : o->clase == 4 ? 7 : o->clase == 8 ? 0x10 : o->clase == 9 ? 0x0d : 2;   /* (0x10 / 0x0d: haobj.h) */
        if (o->pet != pi) continue;
        for (j = 0; j < o->n; j++) {
            camp_obj(o->banco, t, o->s0 + (float)j * (float)(o->s1 - o->s0) / (float)o->n,
                     o->l0 + (float)j * (o->l1 - o->l0) / (float)o->n, o->clase == 4 ? o->f : 1.0f);
            if (o->clase == k && !n++) C->obj_s0 = o->s0;
        }
    }
    dbglog(DBG_INFO, "CAMP: objetos de la peticion %d, %d de la clase %d\n", pi, n, k);
    return n;
}
/* 0x2323e8 con el tipo 5: la zona lateral (fraccion del ancho de la carretera, de borde a borde) y su color (+0xc): subtipo 0
   0..0,5 (2), 1 0,5..1 (3), 2 0,25..0,75 (4); 0x232768 la mira entre el inicio y el final de la peticion */
static int zona_de(const cpet_t *d, float *lo, float *hi)
{
    if (d->tipo != 5 || d->sub > 2) return 0;
    *lo = d->sub == 1 ? 0.5f : d->sub == 2 ? 0.25f : 0.0f; *hi = *lo + 0.5f;
    return 2 + d->sub;
}
int camp_zona(float *lo, float *hi, int *s0, int *s1)
{
    const pet_t *p; int c;
    if (!C || C->cur < 0) return 0;
    p = &C->pet[C->cur];
    if (p->fase < 1 || p->fase > 5 || !(c = zona_de(p->d, lo, hi))) return 0;
    *s0 = p->d->ini; *s1 = p->d->fin;
    return c;
}
/* cuantos de la clase del banco cl en la lista de este paso */
static int cuantos(const int *v, int n, int cl) { int i, k = 0; for (i = 0; i < n; i++) k += v[i] == cl; return k; }

/* ---- los tipos: el control de su vtable +0x24 (solo con la peticion en curso de su tipo) */
static int k6;                                         /* 0x3b5528 */
/* 0x205288 con la fila de la nota de ahora (el switch 0x4230f0 / 0x4236f0: 0..1 -> 0, 2..3 -> 5, 4 -> 3, 5..6 -> 4) */
static void cara_nota(const pet_t *p)
{
    float g = p->med - p->pen; int k, n = 0;
    for (k = 0; k < 7; k++) if (C->d->umbral[k] <= g) n = k;
    if (camp_chica) camp_chica(0, n < 2 ? 0 : n < 4 ? 5 : n == 4 ? 3 : 4);
}
static void chica_para(void) { if (camp_chica) camp_chica(1, 0); }
static void ventana22(int i, float *a, float *b)       /* (tipo 22) la ventana i en secciones de la etapa (+0 + 8 / +4 + 0xc) */
{
    const uint8_t *o = C->c22p + 16 * i; float f8, fc; int ini = C->pet[C->cur].d->ini;
    memcpy(&f8, o + 4, 4); memcpy(&fc, o + 8, 4);
    *a = (float)(ini + (int16_t)(o[0] | o[1] << 8)) + f8; *b = (float)(ini + (int16_t)(o[2] | o[3] << 8)) + fc;
}
static void tipo_paso(pet_t *p, const camp_coche_t *c, float dpos)
{
    const cpet_t *d = p->d; float largo = (float)(d->fin - d->ini);
    switch (d->tipo) {
    case 1:                                             /* 0x29c828: pasar de p[7] km/h */
        if (p->fase == 4) {
            if (c->kmh < (float)d->p[7]) {
                if (p->h78) { p->h78 = 0; p->h74 = 0; chica_para(); }
                if (p->h74 == 0x3c) voz(0xf1, 0x3c, 0xa);
                p->h74++;
            } else if (!p->h78) { p->h78 = 1; voz(0xf0, 0x3c, 0xb); cara_nota(p); }
            if (p->h78) {
                p->med += dpos * (LLENO / largo);
                if (++p->h70 % 0x6f == 0) animo(1, rand() % 5);
            }
        } else if (p->fase == 5 && p->nota == 6) p->extra += bono(p, 1.0f);
        break;
    case 2: case 9:                                     /* 0x298360: derrapar (el 9 es otra clase; SIN PORTAR su diferencia) */
        if (p->fase == 4) {
            if (c->derrapa) { if (!p->h74) { p->h74 = 1; voz(0x8c6, 0x3c, 0); cara_nota(p); } }
            else if (p->h74) { p->h74 = 0; chica_para(); }
            if (p->h74) {
                int v = (int)c->kmh, k = ++p->h70;
                if ((v > 0xf0 && k % 100 == 0) || (v > 0xc8 && k % 0x8c == 0) || (v > 0x96 && k % 0xb4 == 0)) animo(2, k % 5);
                p->med += dpos * (LLENO / (largo * 0.9f));
            }
        } else if (p->fase == 5 && p->nota == 6) p->extra += bono(p, 0.85f);
        break;
    case 3:                                             /* 0x29f038: adelantar p[8] coches (hasta p[9]) */
        if (p->fase == 3) { p->h70 = p->h74 = p->h78 = c->pasados; p->h7c = d->p[8] > 0 ? LLENO / (float)d->p[8] : 0.0f; }
        else if (p->fase == 4) {
            int n = c->pasados, s = 0;
            p->h78 = n;
            if (n > p->h74) { s = n - p->h74; p->h74 = n; if (d->sub == 1 && !p->tmr) { if (camp_se) camp_se(0xd8); p->tmr = 0x258; } }
            else if (n < p->h74) { s = n - p->h74; p->h74 = n; }
            if (p->tmr > 0) p->tmr--;
            if (s > 0) { if (n - p->h70 <= d->p[9]) p->med += s * p->h7c; animo(3, -1); }
            else if (s < 0 && n - p->h70 >= 0) p->med += s * p->h7c;
        } else if (p->fase == 5 && p->nota == 6) {
            int b = 0, n = p->h78 - p->h70;
            if (!p->c4c && !p->c54) b = 25;
            if (!p->c50 && !p->c40) b += 25;
            if (n >= d->p[9]) b += 20;
            else if (d->p[9] > d->p[8]) b += (int)((float)(n - d->p[8]) / (float)(d->p[9] - d->p[8]) * 20.0f);
            p->extra += b;
        }
        break;
    case 4:                                             /* 0x29ec18: no chocar (los sucesos penalizan, 0x22c818) */
        if (p->fase == 4) p->med += dpos * (LLENO / (largo * 0.9f));
        else if (p->fase == 5 && p->nota == 6) p->extra += (!p->c50 && !p->c4c && !p->c54 ? 30 : 0) + (int)(40.0f * frena(p->v_ini, p->v_fin));
        break;
    case 12:                                            /* 0x2a4198: ir al rebufo el p[7] % de la peticion */
        {   float k; memcpy(&k, &d->p[7], 4); k *= 0.01f;   /* (+0x7c en flotante: lwc1, 0x2a41f8) */
            if (k <= 0.0f) k = 1.0f;
            if (p->fase == 4) {
                if (c->rebufo > 0.18f && c->rebufo_ang < 0x5dc) { if (!p->h74) { p->h74 = 1; p->h70 = 0; } }
                else if (p->h74) { p->h70 = 0; p->h74 = 0; }
                if (p->h74) {
                    int v = (int)c->kmh, m = ++p->n;
                    p->med += dpos * (LLENO / (largo * k));
                    if ((v > 0xf0 && m % 0x78 == 0) || (v > 0xc8 && v <= 0xf0 && m % 0xb4 == 0) || (v > 0x96 && v <= 0xc8 && m % 0xdc == 0)) {
                        if (rand() & 3) animo(12, -1);
                    }
                }
            } else if (p->fase == 5 && p->nota == 6) p->extra += bono(p, k);
        }
        break;
    case 6:                                             /* 0x295a28: los conos azules (clase 1 = 5 del banco), no los rojos */
        if (p->fase == 1) { int n = pon_objetos(p, 1); p->h7c = n > 0 ? LLENO / ((float)n * 0.99f) : 0.0f; p->h70 = 0; }
        else if (p->fase >= 2 && p->fase <= 4) {        /* 0x295d48: rojo +0x2c += 3 x; azul (derrapando si p70 == 1) */
            int az = cuantos(c->toq, c->ntoq, 5), ro = cuantos(c->toq, c->ntoq, 6), v = (int)c->kmh;
            p->pen += 3.0f * p->h7c * (float)ro;
            if (az && (d->p[4] != 1 || c->derrapa)) {
                p->med += p->h7c * (float)az;            /* (la voz: 0x3b5528 cada 6 / 7 / 8 por la velocidad, SIMPLIFICADO) */
                k6++; if ((v > 0xf0 && k6 % 6 == 0) || (v > 0xc8 && k6 % 7 == 0) || (v > 0x96 && (k6 & 7) == 0)) animo(6, k6 % 5);
            }
        } else if (p->fase == 5 && p->nota == 6) p->extra += (int)(70.0f * frena(p->v_ini, p->v_fin));
        break;
    case 7:                                             /* 0x2953b8: monedas (2 = 0x0a) y corazones (3 = 0x39), p70 de ellas */
        if (p->fase == 1) { p->h78 = pon_objetos(p, 2); p->h70 = p->h74 = p->tmr = 0; p->h7c = d->p[4] > 0 ? LLENO / (float)d->p[4] : 0.0f; }
        else if (p->fase >= 2 && p->fase <= 4) {        /* 0x295740 (derrapando si p74 == 1) */
            int mo = cuantos(c->toq, c->ntoq, 0x0a), co = cuantos(c->toq, c->ntoq, 0x39);
            if (p->fase == 4) p->tmr++;
            if ((mo || co) && (d->p[5] != 1 || c->derrapa)) {
                p->med += p->h7c * (float)(mo + co); p->h70 += mo; p->h74 += co;
                if (mo) animo(7, -1);
                if (co && p->tmr >= 0xb5) { p->tmr = 0; animo(7, -1); }
            }
        } else if (p->fase == 5 && p->nota == 6) {      /* 70 x cogidas / todas (70 todas) + la mitad de lo que pasa de p70 */
            int b = p->h70 == p->h78 ? 70 : p->h78 ? (int)((float)p->h70 / (float)p->h78 * 70.0f) : 0, t = p->h70 + p->h74;
            if (d->p[4] < t) b += (int)((float)(t - d->p[4]) * 0.5f);
            p->extra += b;
        }
        break;
    case 8:                                             /* 0x29a330: cruzar p70 arcos (4 = 0x0b) */
        if (p->fase == 1) { pon_objetos(p, 4); p->h7c = d->p[4] > 0 ? LLENO / ((float)d->p[4] * 0.99f) : 0.0f; p->tmr = 0; }
        else if (p->fase >= 2 && p->fase <= 4) {        /* 0x29a670 (derrapando si p74 == 1) */
            int a = cuantos(c->cru, c->ncru, 0x0b);
            if (p->fase == 4) p->tmr++;
            if (a && (d->p[5] != 1 || c->derrapa)) {
                p->med += p->h7c * (float)a;
                if (p->tmr >= 0x78) { p->tmr = 0; animo(8, -1); }
            }
        } else if (p->fase == 5 && p->nota == 6) p->extra += (int)(70.0f * frena(p->v_ini, p->v_fin));
        break;
    case 5:                                             /* 0x2a3db0: ir por la zona del subtipo (0x2323e8 / 0x232768) */
        if (p->fase == 4) {
            float lo, hi; int dentro = zona_de(d, &lo, &hi) && c->seccion >= d->ini && c->seccion < d->fin && c->lat > lo && c->lat < hi;
            if (dentro && (d->p[6] != 1 || c->derrapa)) { if (!p->h74) { p->h74 = 1; p->h7c = 0.0f; } }   /* (+0x78 == 1: derrapando) */
            else if (p->h74) { p->h7c = 0.0f; p->h74 = 0; }
            p->h7c += dpos;
            if (p->h74) {
                int v = (int)c->kmh, k = ++p->tmr;
                p->med += dpos * (LLENO / (largo * 0.9f));
                if ((v > 0xf0 && k % 100 == 0) || (v > 0xc8 && k % 0x8c == 0) || (v > 0x96 && k % 0xb4 == 0)) animo(5, k % 5);
            }
        } else if (p->fase == 5 && p->nota == 6) p->extra += bono(p, 0.9f);
        break;
    case 15:                                            /* 0x293190: empujar la pelota (clase 8 = 0x11 del banco) */
        if (p->fase == 1) { pon_objetos(p, 8); p->h7c = LLENO / ((float)(d->fin - d->ini) * 0.85f); p->h70 = p->h78 = 0; p->h74 = C->obj_s0; }
        else if (p->fase >= 2) {                        /* 0x2934e8 (fases 2..5) */
            int mid = d->ini + (d->fin - d->ini) / 2;
            if (!p->h70 && c->bola_sec >= 0 && c->bola_sec + 5 < c->seccion && c->seccion < mid - 10 && camp_obj) {
                camp_obj(0x11, 0x10, (float)mid, 0.5f, 1.0f); p->h70 = 1; p->h74 = mid;   /* la que se ha dejado atras: otra en medio */
            }
            if (p->fase < 5 && !p->h78) {
                if (c->bola_toca) {                     /* +3FC == 8: lo que ha avanzado desde el toque anterior (+4 si nada) */
                    float g = (float)(c->bola_sec - p->h74) * p->h7c;
                    p->med += g > 0.0f ? g : 4.0f; p->h74 = c->bola_sec; se_de(0, 7);
                    dbglog(DBG_INFO, "CAMP: pelota tocada en la seccion %d (jugador %d), medidor %d\n", c->bola_sec, c->seccion, (int)p->med);
                } else if (p->fase == 4 && c->bola_sec > d->fin) {   /* pasa del final: lo ultimo y hecha (+400) */
                    p->med += (float)(c->bola_sec - p->h74) * p->h7c; p->h74 = c->bola_sec; p->h78 = 1;
                }
            } else p->h78 = 1;
            if (p->fase == 5 && p->nota == 6) p->extra += (int)(70.0f * frena(p->v_ini, p->v_fin));
        }
        break;
    case 19:                                            /* 0x29da88: los meteoritos (clase 9 = 0x3d): no dejar que te den */
        if (p->fase == 1) { p->h70 = pon_objetos(p, 9); voz(0x7f, 0x3c, 0xb); }
        else if (p->fase >= 2 && p->fase <= 4) {        /* 0x29e018 (fases 2..4; en la 4 el medidor con el avance) */
            int k = cuantos(c->toq, c->ntoq, 0x3d); float pm;
            if (p->fase == 4) p->med += dpos * (LLENO / (largo * 0.9f));
            if (c->met_caen) se_de(7, 3);               /* 0x423820: empieza a caer */
            if (c->met_llegan) se_de(10, 3);            /* 0x423830: en el suelo (SIN 0x22df58 ni el efecto 0x22c6c8) */
            memcpy(&pm, &d->p[5], 4);                   /* +0x74 de los parametros, flotante */
            if (k) dbglog(DBG_INFO, "CAMP: meteorito: %d golpes en la seccion %d (penalizacion %d + %d x %d)\n", k, c->seccion, (int)p->pen, k, (int)pm);
            for (; k > 0; k--) {                        /* golpe: +0x2c += p74; sin choque (+2F0 & 2), 0x182d38 y v x 0,9 */
                p->pen += pm; if (p->pen > 100.0f) p->pen = 100.0f;
                if (!c->choque && camp_golpe) camp_golpe();
                se_de(13, 3); se_de(16, 3);             /* 0x423840 y 0x423850 */
            }
        }
        break;
    case 11:                                            /* 0x294a90: chocar con los coches de la peticion (subtipos 0 y 1) */
        if (p->fase == 1) {                             /* 0x1f20e8(+0x10 de los parametros): los coches nuevos son de la peticion */
            p->h70 = 0; p->h7c = d->p[6] > 0 ? LLENO / (float)d->p[6] : 0.0f; C->k11_tmr = 0x1a;   /* +0x74 = 80 / p78; +0x18 = 0x1a */
        } else if (p->fase == 4 && C->k11_golpes > 0) { /* +0xb6c y +0x2f4 == 0: el medidor + p74 (y +0x10 + 1) */
            p->med += p->h7c * (float)C->k11_golpes; p->h70 += C->k11_golpes;
            dbglog(DBG_INFO, "CAMP: coche de la peticion golpeado (%d de %d), medidor %d\n", p->h70, d->p[6], (int)p->med);
        } else if (p->fase == 5 && p->nota == 6) {      /* +0x10 > p78: < p7c ? (n - p78) / (p7c - p78) x 70 : (n - p7c) x 5 + 70 */
            int n = p->h70, a = d->p[6], b = d->p[7];
            p->extra += n <= a ? 0 : n < b ? (int)((float)(n - a) / (float)(b - a) * 70.0f) : (n - b) * 5 + 70;
        }
        C->k11_golpes = 0;
        break;
    case 14:                                            /* 0x29ccb8: la suma (tocar los coches con numero hasta el objetivo) */
        if (p->fase == 1) {                             /* 0x29d410: el valor de cada coche y el objetivo (+0x70; la suma +0x72) */
            uint32_t k; const uint8_t *e = C->c14; int pi = (int)(d - C->pets), i, L, sum = 0, r8, ronda;
            C->c14p = NULL; C->n14c = C->nx14 = C->objetivo = C->suma = 0;
            for (k = 0; e && k < C->n14; k++, e += 4 + 4 * e[2]) if ((e[0] | e[1] << 8) == pi) { C->c14p = e + 4; C->n14c = e[2] > 100 ? 100 : e[2]; break; }
            if (!C->c14p) break;
            L = e[3] > 2 ? 2 : e[3];                    /* (el nivel, lbu +0x74: algun guion lleva basura ahi; DESVIACION: tope 2) */
            for (i = 0; i < C->n14c; i++) {             /* rand < 0,2: un negativo de la fila del nivel; si no un positivo */
                const int32_t *t = C->t14 + ((float)(rand() & 0x7fff) * 3.0517578e-5f < 0.2f ? 0 : 33);
                int n = t[L] > 0 && t[L] <= 10 ? (int)t[L] : 1;
                C->v14[i] = (int16_t)t[3 + 10 * L + (rand() & 0x7fff) % n]; sum += C->v14[i];
            }
            r8 = (sum > -1 ? sum : sum + 7) >> 3;
            for (ronda = 1; ronda <= 10; ronda++) {     /* rondas: cada valor con 0,7 (positivo) / 0,4; hasta pasar de suma / 8 */
                for (i = 0; i < C->n14c; i++)
                    if ((float)(rand() & 0x7fff) * 3.0517578e-5f < (C->v14[i] > 0 ? 0.7f : 0.4f)) C->objetivo += C->v14[i];
                if (C->objetivo >= r8) break;
            }
            if (C->objetivo <= 0) C->objetivo = 1;
            dbglog(DBG_INFO, "CAMP: suma: %d coches, nivel %d, objetivo %d\n", C->n14c, L, C->objetivo);
        } else if (p->fase >= 2 && p->fase <= 4 && C->k14_toq) {   /* un coche tocado: el medidor (0x29d074) */
            int a = abs(C->objetivo - C->suma), b = abs(C->objetivo - (C->suma - C->k14_cambio));
            p->med = (float)(C->objetivo - (a < C->objetivo ? a : C->objetivo)) / (float)C->objetivo * C->d->umbral[6] * 1.15f;
            if (a < b) animo(14, -1);                   /* (+0x3d78 / +0x3d74: mas cerca / mas lejos; SIMPLIFICADO) */
            dbglog(DBG_INFO, "CAMP: suma %d (%+d) de %d, medidor %d\n", C->suma, C->k14_cambio, C->objetivo, (int)p->med);
        } else if (p->fase == 5) {
            C->c14p = NULL;
            if (p->nota == 6) p->extra += (!p->c4c && !p->c54 ? 30 : 0) + (int)(40.0f * frena(p->v_ini, p->v_fin));
        }
        C->k14_cambio = C->k14_toq = 0;
        break;
    case 20:                                            /* 0x296120: cruzar la linea entre los dos coches de un par (los de la */
        if (p->fase == 1) { p->h70 = 0; C->k20 = 0; haobj_pares_vacia(); }   /* peticion, 0x296560; el +0x70 de los parametros) */
        else if (p->fase == 4 && C->k20 > 0) {          /* 0x296f58: par cruzado, medidor + 80 / p74 (0x374190 + 0x18) */
            p->med += (float)C->k20 * (d->p[5] > 0 ? C->d->umbral[6] / (float)d->p[5] : 0.0f); p->h70 += C->k20;
            if (camp_chica) camp_chica(2, c->modelo == 9 ? 0x6003d : (float)rand() * 4.656613e-10f >= 0.5f ? 0x6003e : 0x6003f);
            dbglog(DBG_INFO, "CAMP: linea cruzada (%d de %d), medidor %d\n", p->h70, (int)d->p[5], (int)p->med);
        } else if (p->fase == 5) {
            haobj_pares_vacia();                        /* 0x296cc0 x 20 y 0x1f20e8(0) */
            if (p->nota == 6) {                         /* (+0x50 == 0: 40) + min(1, (n - p74) / (p78 - p74)) x 30 + (n - p78) x 5 */
                int n = p->h70, a = d->p[5], b = d->p[6], x = p->c50 ? 0 : 0x28;
                if (a < n) { float f = b > a ? (float)(n - a) / (float)(b - a) : 1.0f; x += (int)(fminf(1.0f, f) * 30.0f); if (b < n) x += (n - b) * 5; }
                p->extra += x;
            }
        }
        C->k20 = 0;
        break;
    case 21:                                            /* 0x292840: los OVNIs (los coches de la peticion): no dejar que te lleven */
        if (p->fase == 1) { p->h70 = p->tmr = 0; C->k21_pl = -1; C->k21_t = 0; }
        else if (p->fase == 2 || p->fase == 4) {        /* (0x11c090(0x1d8) en la 3 y 0x81d8 en la 5: SIN, el sonido continuo) */
            if (p->fase == 4 && c->kmh > 100.0f && c->lat >= 0.0f && c->lat <= 1.0f && C->k21_pl < 0) {   /* 0x2325a0(0, 1) y +0x18 == 0 */
                p->med += dpos * (LLENO / (largo * 0.85f));
                if (++p->tmr % 0xb4 == 0) animo(21, -1);   /* (SIMPLIFICADO: sin la alternancia 0x3b5520 / 0x422c70) */
            }
            if (C->k21_pl >= 0) {                       /* 0x292d28: el OVNI se lleva al jugador (+0xc: 50 pasos tras 0x1f de +0xd18) */
                int k = ++C->k21_t;
                if (k == 1) { if (camp_se) camp_se(0x23f); chica_para(); if (camp_chica) camp_chica(2, 0x60048); if (camp_golpe && !c->choque) camp_golpe(); }
                if (k > 0x1f && k <= 0x1f + 30 && (0x1f + 50 - k) % 10 == 0) { p->pen += 3.0f; if (p->pen > 100.0f) p->pen = 100.0f; }
                if (k >= 0x1f + 50) { C->k21_pl = -1; C->k21_t = 0; }
                if (k == 1) dbglog(DBG_INFO, "CAMP: el OVNI se lleva al jugador en la seccion %d\n", c->seccion);
            }
        } else if (p->fase == 5) {
            C->k21_pl = -1;
            if (p->nota == 6) p->extra += (!p->c50 && !p->c4c && !p->c54 ? 30 : 0) + (int)(40.0f * frena(p->v_ini, p->v_fin));
        }
        break;
    case 16: p->med += mj_paso((int)(d - C->pets), d->sub, p->fase, p->rnd, c, voz); break;   /* los minijuegos (minijuego.c) */
    case 22:                                            /* 0x29b3d0: la bocina (pulsarla en las ventanas de la lista +0x7c) */
        if (p->fase == 1) {                             /* 0x29bfc0: las ventanas y su largo total (+0x14; PROBABLE la suma) */
            uint32_t k; const uint8_t *e = C->c22; int pi = (int)(d - C->pets), i;
            C->c22p = NULL; C->n22c = 0; C->est22 = 0; C->obj22 = -2; C->hecho22 = 0; C->tot22 = 0;
            for (k = 0; e && k < C->n22; k++, e += 4 + 16 * e[2]) if ((e[0] | e[1] << 8) == pi) { C->c22p = e + 4; C->n22c = e[2] > 32 ? 32 : e[2]; break; }
            for (i = 0; i < C->n22c; i++) { float a, b; ventana22(i, &a, &b); C->tot22 += b - a; }
            if (C->tot22 <= 0) C->tot22 = 1;
            dbglog(DBG_INFO, "CAMP: bocina: %d ventanas, %.0f secciones\n", C->n22c, (double)C->tot22);
        } else if (p->fase == 4 && C->c22p) {           /* 0x29c058: la ventana (+0x24) o la siguiente (+0x28) */
            int i, nx = -1; float a, b, f = c->pos;
            C->cur22 = -1;
            for (i = 0; i < C->n22c; i++) {
                if (C->hecho22 >> i & 1) continue;
                ventana22(i, &a, &b);
                if (a <= f && f <= b) { C->cur22 = i; break; }
                if (b < f) C->hecho22 |= 1u << i;
            }
            if (C->cur22 < 0) for (i = 0; i < C->n22c && nx < 0; i++) { ventana22(i, &a, &b); if (!(C->hecho22 >> i & 1) && f <= a) nx = i; }
            if (C->est22 == 0) {                        /* (0x1b4518: el boton de la bocina) */
                if (c->bocina) { C->est22 = 1; if (camp_se) camp_se(0x1bc); C->obj22 = nx >= 0 ? nx : C->cur22 >= 0 ? C->cur22 : -2; }
            } else if (!c->bocina) C->est22 = 0;        /* (el 0x81bc que la para: SIN, el efecto es corto) */
            else if (C->obj22 == C->cur22) {            /* sonando en su ventana: medidor + 80 x avance / largo x 1,25 en la carretera */
                if (c->lat >= 0.0f && c->lat <= 1.0f) p->med += LLENO * (f - C->ant22) / C->tot22 * 1.25f;   /* (0x2325a0(0, 1)) */
            } else { p->pen += LLENO * (f - C->ant22) / C->tot22 * 0.25f; if (p->pen > 100.0f) p->pen = 100.0f; }   /* fuera: x 0,25 */
            C->ant22 = f;
        } else if (p->fase == 5) {
            C->c22p = NULL;
            if (p->nota == 6) p->extra += bono(p, 0.9f);
        }
        if (p->fase != 4) C->ant22 = c->pos;
        break;
    case 24:                                            /* 0x2a48a0: las fotos (los objetivos de la lista +0x70 del guion) */
        if (p->fase == 1) {                             /* 0x2a5fd8: los objetivos (+0x14 cuantos) */
            uint32_t k; const uint8_t *e = C->c24; int pi = (int)(d - C->pets);
            C->c24p = NULL; C->n24c = 0; C->act24 = -1; C->hecho24 = 0; C->nota24 = -1;
            for (k = 0; e && k < C->n24; k++, e += 4 + 16 * e[2]) if ((e[0] | e[1] << 8) == pi) { C->c24p = e + 4; C->n24c = e[2] > 16 ? 16 : e[2]; C->rel24 = e[3] ? d->ini : 0; break; }
            dbglog(DBG_INFO, "CAMP: fotos: %d objetivos\n", C->n24c);
        } else if (p->fase >= 2 && p->fase <= 4 && C->c24p) {
            int i;                                      /* 0x2a60a0: el objetivo de la seccion; pasado su final, perdido */
            for (i = 0; i < C->n24c; i++) {
                const uint8_t *o = C->c24p + 16 * i; int s0 = (int16_t)(o[12] | o[13] << 8) + C->rel24, s1 = (int16_t)(o[14] | o[15] << 8) + C->rel24;
                if (C->hecho24 >> i & 1) continue;
                if (C->act24 == i) { if (s1 < c->seccion) { C->hecho24 |= (uint16_t)(1u << i); C->act24 = -1; } }
                else if (C->act24 < 0 && s0 < c->seccion) { C->act24 = i; voz(rand() & 1 ? 0xf4 : 0xf3, 0x3c, 4);
                    dbglog(DBG_INFO, "CAMP: foto %d: el objetivo (secciones %d..%d)\n", i, s0, s1); }
            }
            if (p->fase == 4 && C->act24 >= 0 && c->foto && camp_marco) {   /* 0x2a4fa8: el disparo (+0x2c la nota de la foto) */
                const uint8_t *o = C->c24p + 16 * C->act24; float l[3], w[3], a, b, df; int nf = 0;
                memcpy(l, o, 12); camp_marco(l, w);
                a = atan2f(w[0] - c->xyz[0], w[2] - c->xyz[2]); b = atan2f(c->fx, c->fz); df = a - b;   /* (0x2bb0a0 x 2) */
                if (df < -3.14159265f) df += 6.28318531f;
                if (df > 3.14159265f) df -= 6.28318531f;
                if (fabsf(df) < 0.50265f) nf = (int)((1.0f - fabsf(df) / 0.50265f) * 10.0f + 0.5f);   /* (+0x24: el 0,5; SIMPLIFICADO) */
                p->med += (float)nf / 10.0f * (LLENO / ((float)C->n24c * 0.9f));   /* (0x2a5074, al acabar el disparo) */
                if (camp_se) camp_se(0x174);
                C->nota24 = nf; C->hecho24 |= (uint16_t)(1u << C->act24); C->act24 = -1;
                dbglog(DBG_INFO, "CAMP: foto: %.0f grados del objetivo, nota %d, medidor %d\n", (double)(df * 57.29578f), nf, (int)p->med);
            }
        } else if (p->fase == 5) {
            C->c24p = NULL; C->act24 = -1;
            if (p->nota == 6) p->extra += bono(p, 0.9f);
        }
        break;
    default: break;                                     /* SIN PORTAR */
    }
}
/* (tipo 22) 1 con una ventana de la bocina en curso (las pruebas) */
int camp_bocina22(void) { return C && C->c22p && C->cur22 >= 0; }
/* (tipo 24) el objetivo de la foto activo, en el mundo (camp_marco); 0 ninguno */
int camp_foto(float *w)
{
    const pet_t *p = C && C->cur >= 0 ? &C->pet[C->cur] : NULL; float l[3];
    if (!p || p->d->tipo != 24 || !C->c24p || C->act24 < 0 || !camp_marco) return 0;
    memcpy(l, C->c24p + 16 * C->act24, 12); camp_marco(l, w); return 1;
}

/* ---- los coches de la peticion del tipo 11 (0x1f20e8 / 0x1f1a88: el trafico sale con la tabla de 0x46 bytes de
   +0x10 de los parametros; [0x55cf38 + 8 + plaza] & 3 == 2 los marca). SIMPLIFICADO (DESVIACION): aqui los coches del
   trafico que aparecen por delante del jugador desde el anuncio (fase 1) hasta el final (fase 4) son los de la peticion,
   con los modelos del trafico (los de la tabla, 0x96 / 0xa5 o +0x64, no estan en la DC) */
static const pet_t *pet11(void)                         /* (y los tipos 20 y 21: los mismos coches de la peticion) */
{
    const pet_t *p = C && C->cur >= 0 ? &C->pet[C->cur] : NULL;
    return p && ((p->d->tipo == 11 && p->d->sub < 2) || p->d->tipo == 14 || p->d->tipo == 20 || p->d->tipo == 21) && p->fase >= 1 && p->fase <= 4 ? p : NULL;
}
static float d2(const float *a, const float *b) { float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2]; return x * x + y * y + z * z; }
void camp_autos(haobj_auto_t *v, int n)
{
    const pet_t *p = pet11(); int i, js, t = p ? p->d->tipo : 0;
    if (!C) return;
    js = (int)floorf(C->pos);
    for (i = 0; i < n && i < 24; i++) {
        C->ver11[i] = 0;
        if (!v[i].pos) continue;
        if (!(v[i].marca & HA_VISTO)) {                 /* (una plaza nueva: el coche acaba de aparecer) */
            v[i].marca |= HA_VISTO; C->fr11[i] = 0;
            if (p && v[i].sec > js && t != 14) v[i].marca |= HA_CAMP | (t == 11 && p->d->sub == 1 ? HA_FANT : 0);   /* (14: camp_coche14) */
        }
        if (!(v[i].marca & HA_CAMP)) continue;
        if (!p) { v[i].marca &= ~(HA_CAMP | HA_FANT | HA_PAR | HA_SUELTO | 0xff); continue; }   /* (0x1f20e8(0) en la fase 5) */
        memcpy(C->p11[i], v[i].pos, sizeof C->p11[i]);
        if (t == 21) {                                  /* 0x292d28: el OVNI toca al jugador (0x1d3a10 con radio 4: aqui 5 m, PROBABLE) */
            #ifdef PRUEBA_C21
            if (abs(v[i].sec - js) <= 8) v[i].golpe = 1;   /* prueba: a 8 secciones cuenta como toque (el piloto no los alcanza) */
#endif
            if (!(v[i].marca & HA_SUELTO) && C->k21_pl < 0 && p->fase >= 2 && (d2(v[i].pos, C->xyz) < 25.0f || v[i].golpe)) {
                v[i].marca |= HA_SUELTO; C->k21_pl = i; C->k21_t = 0;   /* (despues, +C5C 0x8000 y +B64 = 0x3c: se va) */
            }
            if (!(v[i].marca & HA_SUELTO) && v[i].sec - js < 10) C->ver11[i] = 1;
            continue;
        }
        if (t == 20) continue;
        if (t == 14) {                                  /* 0x29cf18: los coches con numero (+0xd14 == 3; el indice + 1 en la marca) */
            int k = (v[i].marca & 0xff) - 1;
            if (k < 0 || k >= C->n14c || (v[i].marca & HA_SUELTO)) continue;
            {   float r = abs(C->v14[k]) < 10 ? 3.0f : 4.0f; r += 1.5f;   /* 0x1d3a10 con radio 3 / 4 (+ el coche: PROBABLE) */
#ifdef PRUEBA_C14
                if (abs(v[i].sec - js) <= 1 && (rand() & 1)) v[i].golpe = 1;   /* prueba: la mitad de los que pasa a su lado */
#endif
#ifdef PRUEBA_C14V
                {   static int t_; if (abs(v[i].sec - js) <= 2 && ++t_ % 10 == 0) dbglog(DBG_INFO, "C14V: coche %d a %.1f m (radio %.1f), seccion %d / %d\n", k + 1, (double)sqrtf(d2(v[i].pos, C->xyz)), (double)r, v[i].sec, js); }
#endif
                if (d2(v[i].pos, C->xyz) < r * r || v[i].golpe) {   /* +0x72 += valor (no baja de 0) */
                    int a = C->suma; C->suma += C->v14[k]; if (C->suma < 0) C->suma = 0;
                    C->k14_cambio += C->suma - a; C->k14_toq = 1;
                    v[i].marca |= HA_SUELTO; if (camp_se) camp_se(C->v14[k] > 0 ? 0xd6 : 0xd4);   /* (efectos: SUPUESTO) */
                } else if (v[i].sec < js - 1) v[i].marca |= HA_SUELTO;   /* 0x19c990: el que queda atras se va */
            }
            if (!(v[i].marca & HA_SUELTO) && v[i].sec - js < 10) { C->ver11[i] = 1; C->val11[i] = C->v14[k]; }
            continue;
        }
        if (v[i].sec - 7 == js && C->k11_tmr >= 0x3c) { /* 0x294e00: un coche 7 secciones por delante: 0x422e00[rand % 3] */
            if (C->se11 && camp_se) camp_se(C->se11[rand() % 3]);
            C->k11_tmr = 0;
        }
#ifdef PRUEBA_C11
        if (v[i].sec == js && !(v[i].marca & HA_SUELTO)) v[i].golpe = 1;   /* prueba: pasar a su lado cuenta como golpe */
#endif
        if (v[i].golpe && !(v[i].marca & HA_SUELTO)) { v[i].marca |= HA_SUELTO; C->k11_golpes++; }   /* (una vez por coche: PROBABLE) */
        if (!(v[i].marca & HA_SUELTO) && v[i].sec - js < 10) C->ver11[i] = 1;
    }
    if (t == 20 && p->fase >= 2) {                      /* 0x296560 (fases 2..4) y 0x296f58 (la 4) */
        haobj_pares_mascara(HA_CAMP); haobj_pares(v, n, p->d->ini, p->d->fin, C->xyz); haobj_pares_mascara(0);
#ifdef PRUEBA_C20
        {   int a, b, k;                                /* prueba: el par a 3 secciones del jugador cuenta como cruzado (no apunta) */
            for (k = 0; k < 10; k++) if (haobj_par(k, &a, &b) && a < n && b < n && abs(v[a].sec - js) <= 3 && abs(v[b].sec - js) <= 3 && p->fase == 4) {
                v[a].marca = (v[a].marca & ~HA_PAR) | HA_SUELTO; v[b].marca = (v[b].marca & ~HA_PAR) | HA_SUELTO; C->k20++; } }
#endif
        if (p->fase == 4) C->k20 += haobj_pares_cruza(v, n, C->xyz, C->fx, C->fz);
#if defined(PRUEBA_C20) || defined(DIAG_HA)
        if (C->reloj % 60 == 0) {
            int a, b, k, np = 0, nm = 0; for (k = 0; k < 10; k++) np += haobj_par(k, &a, &b);
            for (k = 0; k < n; k++) nm += v[k].pos && (v[k].marca & HA_CAMP) && !(v[k].marca & HA_SUELTO);
            dbglog(DBG_INFO, "C20: fase %d seccion %d: %d coches de la peticion, %d pares\n", p->fase, js, nm, np);
        }
#endif
    }
    if (p) C->k11_tmr++;
}
/* (tipo 14) el siguiente coche de la cuenta que hay que poner: su seccion de la etapa (inicio + la del guion) y carril,
   cuando el jugador esta a menos de 40 secciones; *marca: la marca de su plaza. 0 si no hay */
int camp_coche14(int *sec, int *carril, int *marca)
{
    const pet_t *p = pet11(); const uint8_t *e;
    if (!p || p->d->tipo != 14 || !C->c14p || C->nx14 >= C->n14c) return 0;
    for (;;) {                                          /* (c2c7: los que el tope de main.c deja pasar sin abrir, fuera) */
        e = C->c14p + 4 * C->nx14;
        *sec = p->d->ini + (int16_t)(e[0] | e[1] << 8); *carril = (int8_t)e[2];
        if (*sec >= (int)floorf(C->pos) + 5 || ++C->nx14 >= C->n14c) break;
    }
    if (C->nx14 >= C->n14c) return 0;
    if (*sec - (int)floorf(C->pos) > 40) return 0;
    *marca = (int)(HA_VISTO | HA_CAMP) | (C->nx14 + 1); C->nx14++;
    return 1;
}
int camp_cuenta14(int *objetivo, int *suma)
{
    const pet_t *p = pet11();
    if (!p || p->d->tipo != 14 || !C->c14p) return 0;
    *objetivo = C->objetivo; *suma = C->suma; return 1;
}
int camp_pares_pide(void) { const pet_t *p = pet11(); return p && p->d->tipo == 20; }
int camp_lineas(float (*a)[3], float (*b)[3], int max)
{
    const pet_t *p = pet11(); int i, k = 0, x, y;
    if (!p || p->d->tipo != 20 || p->fase < 2) return 0;
    for (i = 0; i < 10 && k < max; i++) {
        if (!haobj_par(i, &x, &y) || x >= 24 || y >= 24) continue;
        memcpy(a[k], C->p11[x], sizeof a[k]); memcpy(b[k], C->p11[y], sizeof b[k]); a[k][1] += 1.0f; b[k][1] += 1.0f; k++;
    }
    return k;
}
/* el dibujo: 11/1 el 2C0006 de los fantasmas (0x294b18 lo prepara; como el 0x1b2190 del INFARTO: escala 1,68 hasta el
   cuadro 25, luego - 0,2 por cuadro); 11/0 DESVIACION: sin el modelo de los coches de la peticion, el icono del
   corazon del medidor (HA_C0, 2B000F) sobre ellos */
int camp_spr3d(haobj_s3d_t *v, int max, int (*cuadros)(const char *g))
{
    const pet_t *p = pet11(); int i, k = 0, f, nc;
    if (!p) return 0;
    if (p->d->tipo == 20) return 0;
    if (p->d->tipo == 14) {                             /* (DESVIACION: el numero con las cifras del HUD, rojo el negativo) */
        for (i = 0; i < 24 && k < max; i++) {
            if (!C->ver11[i]) continue;
            snprintf(v[k].g, sizeof v[k].g, "#%d", abs(C->val11[i]) % 1000); v[k].cuadro = C->val11[i] < 0;
            memcpy(v[k].p, C->p11[i], sizeof v[k].p); v[k].p[1] += 2.0f; v[k].esc = 0; k++;
        }
        return k;
    }
    f = p->d->sub == 1 || p->d->tipo == 21; nc = cuadros ? cuadros(f ? "HA_S06" : "HA_C0") : 1;   /* (21: DESVIACION, sin el modelo del OVNI) */
    for (i = 0; i < 24 && k < max; i++) {
        if (!C->ver11[i]) continue;
        strcpy(v[k].g, f ? "HA_S06" : "HA_C0");   /* (g: 8 bytes) */ memcpy(v[k].p, C->p11[i], sizeof v[k].p); v[k].p[1] += 1.6f;
        if (f) {
            v[k].cuadro = C->fr11[i]; v[k].esc = C->fr11[i] <= 0x19 ? 1.68f : 1.68f - (float)(C->fr11[i] - 0x19) * 0.2f;
            if (++C->fr11[i] >= nc) C->fr11[i] = (int16_t)(nc - 1);
        } else { v[k].cuadro = (int)(C->reloj % (uint32_t)(nc > 0 ? nc : 1)); v[k].esc = 2.0f; }
        k++;
    }
    return k;
}

/* ---- 0x22c818: los sucesos (con la peticion en curso entre sus fases 3 y 5, o sin ella) */
static const uint32_t MASCARA = 0x2f03f82u;             /* superficies de asfalto (la del INFARTO, 45E2F0) */
static int sucesos(pet_t *p, const camp_coche_t *c)
{
    static const uint8_t B[14] = { 0, 0, 1, 1, 0, 0, 1, 2, 2, 2, 2, 0, 0, 0 };   /* (los del INFARTO: PROBABLE la tabla 0x4154c0) */
    etapa_t *e = et_cur(); int r = 0, k, en = p && p->fase >= 3 && p->fase <= 5;
    if (!e) return 0;
    if (c->choque_ini) {
        int i = c->choque - 1 >= 0 && c->choque - 1 <= 13 ? B[c->choque - 1] : 2;
        if (i == 0) {                                   /* choque */
            voz_calla();
            if (en) {
                voz(c->choque == 1 ? 0x94d : 0x14e, 0x3c, 5); p->pen += p->P[0]; p->c38++; p->c48++; suma_total();
                if ((int)p->P[0] < C->total) e->cor -= (int)p->P[0]; else e->cor = 0;
            } else { suma_total(); if (C->total > 50) e->cor -= 50; else e->cor = 0; voz(0x950, 0x3c, 5); }
            r = 1;
        } else if (i == 1 && c->impulso_rapido) {       /* golpe de uno mas rapido */
            voz_calla();
            if (en) {
                p->pen += p->P[1]; voz(0x149, 0x3c, 5); p->c44++; p->c38++; suma_total();
                if ((int)p->P[1] < C->total) e->cor -= (int)p->P[1]; else e->cor = 0;
            } else { suma_total(); if (C->total > 20) e->cor -= 20; else e->cor = 0; voz(0x14c, 0x3c, 5); }
            r = 1;
        }
    } else if (c->impulso) {                            /* golpe */
        if (en) {
            p->pen += p->P[2]; p->c40++; p->c38++; suma_total();
            if ((int)p->P[2] < C->total) e->cor -= (int)p->P[2]; else e->cor = 0;
        } else { suma_total(); if (C->total > 10) e->cor -= 10; else e->cor = 0; }
        r = 1;
    } else if (c->roce && en) { p->pen += p->P[5] * DT; p->c50++; r = 1; }
    if (en) {
        for (k = 0; k < 4; k++) if (!(c->sup[k] & MASCARA)) { p->pen += p->P[4] * DT; p->c4c++; r = 1; }
        if (c->muro) { p->pen += p->P[3] * DT; p->c54++; r = 1; }
        if (p->pen > 100.0f) p->pen = 100.0f;
    }
    return r;
}

/* ---- 0x232988: la maquina de fases de la peticion en curso */
static void fases(pet_t *p, const camp_coche_t *c)
{
    const cpet_t *d = p->d; int a = p->fase, s = c->seccion;
    etapa_t *e = et_cur();
    p->ant = a;
    if (s < d->ini - 30) p->fase = 0;
    else if (s < d->ini) p->fase = a == 0 ? 1 : 2;
    else if (s <= d->fin) p->fase = a == 2 ? 3 : 4;
    else p->fase = a == 4 ? 5 : a == 5 ? 6 : 7;
    switch (p->fase) {
    case 1:                                             /* el anuncio: 0x22f9d0 y la voz 0x2a6358 */
        if (e) e->idx++;
        if (d->rot != 0xff) rotulo_abre(0, d->rot, C->d->marco_anuncio, 30, -1);
        else rotulo_abre(1, C->d->rot_generico, C->d->marco_anuncio, -1, -1);   /* (el tipo 20: sin rotulo propio) */
        voz_calla(); if (d->voz && d->voz != 0xffff) voz(d->voz, 0x3c, 9);
        dbglog(DBG_INFO, "CAMP: peticion %d/%d (%d..%d)\n", d->tipo, d->sub, d->ini, d->fin);
        break;
    case 3:                                             /* empieza: la voz 0x8e8 (0x2a6580) */
        p->v_ini = p->v_med = c->kmh; p->v_suma = p->t = 0.0f; p->n = 0;
        if (!(d->tipo == 16 && d->sub == 5)) voz(0x8e8, 0x3c, 9);
        break;
    case 4:
        p->t += DT; p->n++; p->v_suma += c->kmh; if (p->n > 0) p->v_med = p->v_suma / (float)p->n;
        break;
    case 5:                                             /* acaba: la nota y los corazones */
        p->v_fin = c->kmh; nota_de(p); corazones_de(p);
        break;
    case 6:                                             /* a la etapa, el rotulo de la nota (0x22f7a0) y su voz (0x2a65a8) */
        a_la_etapa(p); C->ultima_nota = p->nota;
        rotulo_abre(1, C->d->rot_nota[p->nota], C->d->marco_nota[p->nota >= 4], -1, 0x78);
        {   int k;
            if (p->nota >= 4) k = (C->nota3++ % 3) + 4;
            else if (p->nota >= 2) k = p->nota;
            else k = C->nota1++ & 1;
            voz(C->d->voz_nota[C->fila][k], 0x3c, 0xa);
        }
        if (camp_chica && C->chi && p->nota >= 0 && p->nota < 7) {   /* 0x2a66a8: para y el movimiento de la nota */
            int m = c->modelo - 2, t = m >= 0 && m < 8 ? C->chi[56 + m] : 0; uint32_t id;
            memcpy(&id, C->chi + 28 * t + 4 * p->nota, 4);
            camp_chica(1, 0); camp_chica(2, id);
        }
        dbglog(DBG_INFO, "CAMP: peticion %d hecha: %d - %d, nota %d, %d + %d corazones\n", d->tipo, (int)p->med, (int)p->pen, p->nota, p->cor, p->extra);
        break;
    default: break;
    }
}

/* 0x22a2b0 -> 0x232e60: el letrero de inicio (0x1a7c80) de cada peticion de la etapa en su inicio y el de fin (0x1a7cb0,
   salvo los minijuegos, +0 == 0x10) en su fin, mientras la seccion del jugador esta entre la del letrero - 50 y + 2; en
   el centro de la seccion + 4 m (los del INFARTO: haobj_marcas, con su vaiven; el de 0x232e60, [+0x18c] con 0x22b880, SIN) */
static void letrero(int s)
{
    int i, n = 0, reg[2], sec[2], k;
#ifdef DIAG_SIN_LET
    return;                                             /* medida: sin los letreros */
#endif
    if (!C->let) return;
    for (i = 0; i < C->npet; i++) {
        const cpet_t *d = C->pet[i].d;
        if (n < 2 && d->ini - 50 < s && s < d->ini + 2) { reg[n] = C->let[0]; sec[n++] = d->ini; }
        if (n < 2 && d->tipo != 16 && d->fin - 50 < s && s < d->fin + 2) { reg[n] = C->let[1]; sec[n++] = d->fin; }
    }
    k = n ? n | sec[0] << 2 | (n > 1 ? sec[1] : 0) << 14 : 0;
    if (k == C->let_k) return;
    C->let_k = k;
    if (n && camp_punto) {
        float P[2][3], fx[2], fz[2];
        for (i = 0; i < n; i++) camp_punto(sec[i], P[i], fx + i, fz + i);
        haobj_marcas(n, reg, (const float (*)[3])P, fx, fz);
    } else haobj_marcas(0, reg, NULL, NULL, NULL);
}
void camp_paso(const camp_coche_t *c)
{
    pet_t *p; int i;
    if (!C) return;
    C->reloj++;
    C->pos_ant = C->pos; C->pos = c->pos; memcpy(C->xyz, c->xyz, sizeof C->xyz); C->fx = c->fx; C->fz = c->fz;
    if (c->en_union) {                                  /* 0x22c000: la lista de la siguiente al entrar en la union */
        if (!C->union_hecha) { elige_lista(); C->union_hecha = 1; }
        C->cur = -1;                                    /* 0x22b7d0: sin peticion en la union */
        letrero(-1000);
    } else {
        float dpos = C->pos - C->pos_ant;
        letrero(c->seccion);
        if (dpos < 0.0f || dpos > 50.0f) dpos = 0.0f;
        if (C->cur >= 0 && C->pet[C->cur].fase >= 7) C->cur = -1;
        if (C->cur < 0)                                 /* 0x22b7d0 */
            for (i = 0; i < C->npet; i++) if (C->pet[i].d->ini - 32 < c->seccion && c->seccion < C->pet[i].d->fin + 30 && C->pet[i].fase < 7) { C->cur = i; break; }
        p = C->cur >= 0 ? &C->pet[C->cur] : NULL;
        if (p) fases(p, c);
        sucesos(p, c);                                  /* (con el tipo 4, 0x22e9a0: el HUD de la nota, SIN PORTAR) */
        if (p) tipo_paso(p, c, dpos);
    }
    suma_total();
    if (C->disp < C->total) C->disp += (C->total - C->disp + 2) / 3;   /* (el contador se acerca; 22e118: PROBABLE) */
    else if (C->disp > C->total) C->disp -= (C->disp - C->total + 2) / 3;
    rotulo_paso(); voz_bomba();
}
int camp_total(void) { return C ? C->total : 0; }
int camp_corazones(void) { return C ? C->disp : 0; }
int camp_filas(int32_t *v)                              /* 0x22a5b8: 0x1bd920(k, R[k] +0x20c) por cada nivel hasta el actual */
{
    int k, n = C ? C->nivel : -1;
    if (n < 0) { v[0] = 0; return 0; }
    for (k = 0; k <= n && k < NIV; k++) v[k] = C->et[k].cor;
    return n < NIV ? n : NIV - 1;
}
