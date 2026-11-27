/* minijuego.c: los minijuegos de los campeonatos de COAST 2 COAST, el tipo de peticion 16 (casi todos de Holly).

   Fuente: SLES-53998, la clase 0x29f520 (0x1860 bytes, vtable 0x423a80: +0x24 el control 0x29f7a0, +0x2c el dibujo 3D
   0x29fd50, +0x34 el de los sprites 0x29fe00). Los desplazamientos entre parentesis son los suyos.
   - Fase 1 (0x29f7a0): 0x2a0d08 reparte los registros de 0x2c de +0x7c de los parametros ({+0x20 clase 0 aviso / 1 puerta,
     +0x24 simbolos o carriles, +0x26 / +0x28 secciones (+0x80: relativas al inicio), +0x2a cuantos: con 2 o mas, repartidos
     de +0x26 a +0x28}) en la lista A (+0xc, los avisos, +0x70 cuantos) y la B (+0x8a4, las puertas, +0x72), 50 como mucho
     cada una; las secciones de las puertas a +0x113c (+0x74) y por cada puerta 0x2a3778 pone los conos rojos (clase 6 del
     banco) en los n + 1 bordes de sus n carriles; despues el subtipo (tabla 0x4239f0) los prepara.
   - Fases 4..6: el paso del subtipo (tabla 0x423a10) y cada 600 pasos (0x6213a0, global) una voz de animo (0x1ac3d8 con
     0x3c, 0): 0x4239c0[k % 2] los subtipos 0 y 1, 0x4239c8[k % 3] el 2, 0x4239d8[k % 3] los demas (k = 0x621398 + 1).
     Fases 2..6: +0x1204 = (+0x1204 + 1) % 31 (el vaiven de los sprites).
   - 0x29fb28(n, k): el jugador va por el carril k de n de la puerta (k / n < f < (k + 1) / n, f su fraccion de borde a
     borde); si no, el efecto 0xd4.
   - Dibujo (0x2a0138, por cada registro entre la seccion del jugador - 2 y + 50): los n simbolos +0xc.. en los laterales
     (fila + 0,5) / n, 0,1 de carril hacia fuera del centro, a 2,5 m (+ sin(+0x1204 / 30 x pi) en las puertas y los avisos
     de los subtipos 0, 1, 3 y 4), con el sprite 3D 0x115870 del banco 0x46 de la tabla del subtipo.
   Subtipos (lo que piden los guiones: 2 x 22, 1 x 20, 0 x 16, 6 x 15, 3 x 13, 4 x 9, 5 x 9):
   - 2, el sonido (0x2a1ba0 / 0x2a1e00, banco SOUND.PAK o SOUND2..8): por cada puerta el carril bueno al azar y el animal
     (+8 de los parametros, el de SOUNDn, o rand % 7 con SOUND.PAK) en el, en el aviso k y en los demas carriles otros
     distintos al azar; en la fase 4 al pasar el aviso su voz 0x423c30[animal] (0x3c, 0xb) y 0x232350 (el 2B003D, SIN
     PORTAR); al pasar la puerta, por el carril bueno el medidor + 80 x 1,1 / puertas, +0x3d78 = 1 y el destello 2C0002
     (0x114b68(0x2c0002, 5, 3)); +0x76 + 1. Sprites 0x423b68[animal] a escala 1,5, solo en las puertas. Sin bono de nota 6.
   - 0, la memoria (0x2a1150 / 0x2a13f8, MEMORY.PAK): el aviso k con +0x24 frutas al azar de 6 (0x423ae0) en columnas
     (0x423b88 / 0x423bb0: a 2,5 m) y la puerta k con dos filas de ellas, la buena (rand % 2) las mismas y la otra con al
     menos una distinta; la puerta se mira en 2 (0x29fb28(+0x24 / columnas, buena)); el mismo premio.
   - 1, el distinto (0x2a1618 / 0x2a1948, ODD_ONE.PAK): por puerta una clase al azar (las 9 frutas o los 12 animales de
     0x423b10, cuantos en 0x423ad8): el carril bueno con uno de ella y los demas con otros distintos de la otra; el aviso k
     muestra lo mismo (0x232350 a 20 secciones de el, SIN PORTAR); el mismo premio.
   Los demas subtipos (3 la suma, 4 los regalos, 5 las ovejas, 6 los coches): SIN PORTAR (se anuncian, tienen sus conos y
   sus voces, y acaban con el medidor a cero); ver docs/PARIDAD.md. */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "minijuego.h"
#include "haobj.h"

void (*mj_mundo)(float sec, float lat, float dx, float p[3]);

#define NMAX 50
typedef struct {                                        /* un registro de la lista A o B (0x2c bytes en el original) */
    int8_t sim[6];                                      /* +0x00.. los simbolos (-1 ninguno) */
    uint8_t pend, buena, n, cl;                         /* +0x18 por pasar, +0x1c el carril bueno, +0x24, +0x20 */
    int16_t sec;                                        /* +0x26 */
} mj_ent_t;
typedef struct {
    const uint8_t *m;                                   /* 'M161' (tools/conv_camp.py minijuegos) */
    uint32_t n; const uint8_t *pets;
    int pet, sub, na, nb, t1204, ok_pet;                /* la peticion preparada (-1 ninguna) */
    uint32_t c13a0, c1398;                              /* 0x6213a0 / 0x621398 (al azar la primera vez) */
    mj_ent_t a[NMAX], b[NMAX];
    int destello, js; float yo[3];                      /* el 2C0002 del acierto (cuadros que lleva), la seccion y el coche */
} mj_t;
static mj_t *M;
/* el bloque 'M161': u16 voz_aviso2[7] (0), u16 animo[10] (14), u8 sprite[34] (34: 6 del subtipo 0, 21 del 1, 7 del 2), u8
   cuantos1[2] (68), 2, f32 columna[9] (72), altura[9] (108), u32 n (144) y las peticiones (148) */
#define CAB 148
#define SPR0 34
#define SPR1 (34 + 6)
#define SPR2 (34 + 27)
static float f32_(int o) { float f; memcpy(&f, M->m + o, 4); return f; }

void mj_libera(void) { free(M); M = NULL; }
int mj_inicia(const uint8_t *d, uint32_t tam)
{
    uint32_t o, l;
    mj_libera();
    for (o = 0; o + 8 + CAB <= tam; o++)                /* (el bloque va detras de los demas, sin alinear: se busca su firma) */
        if (!memcmp(d + o, "M161", 4) && (memcpy(&l, d + o + 4, 4), l >= CAB && o + 8 + l <= tam)) break;
    if (o + 8 + CAB > tam || !(M = calloc(1, sizeof *M))) return -1;
    M->m = d + o + 8; memcpy(&M->n, M->m + CAB - 4, 4); M->pets = M->m + CAB; M->pet = -1;
    M->c13a0 = (uint32_t)rand(); M->c1398 = (uint32_t)rand();
    dbglog(DBG_INFO, "MJ: %u peticiones del tipo 16, %u bytes\n", (unsigned)M->n, (unsigned)sizeof *M);
    return 0;
}
static int u16_(const uint8_t *p) { return p[0] | p[1] << 8; }

/* 0x2a0d08 (y 0x2a3778: los conos de las puertas) */
static void reparte(int pet)
{
    const uint8_t *e = M->pets; uint32_t k; int i, j, nr = 0;
    M->na = M->nb = 0; M->pet = pet; M->t1204 = 0;
    for (k = 0; k < M->n; k++, e += 4 + 8 * e[3]) if (u16_(e) == pet) { nr = e[3]; break; }
    if (k == M->n) return;
    if (camp_obj) camp_obj(-1, 0, 0, 0, 0);
    for (i = 0; i < nr; i++) {
        const uint8_t *r = e + 4 + 8 * i; int cl = r[0], c = r[2], s0 = (int16_t)u16_(r + 4), s1 = (int16_t)u16_(r + 6);
        int m = c < 2 || s0 >= s1 ? 1 : c; float paso = m > 1 ? (float)(s1 - s0) / (float)(c - 1) : 0.0f;
        for (j = 0; j < m; j++) {
            mj_ent_t *x;
            if (cl == 0 && M->na < NMAX) x = &M->a[M->na++];
            else if (cl == 1 && M->nb < NMAX) x = &M->b[M->nb++];
            else continue;
            memset(x->sim, 0xff, sizeof x->sim); x->pend = 1; x->buena = 0; x->n = r[1]; x->cl = (uint8_t)cl;
            x->sec = (int16_t)(s0 + (int)(paso * (float)j));
        }
    }
#ifndef DIAG_SIN_MJ                                     /* (medida: sin los conos) */
    for (i = 0; i < M->nb && camp_obj; i++)             /* 0x2a3778: conos rojos (6) en los bordes de los carriles */
        for (j = 0; j <= M->b[i].n && M->b[i].n; j++) camp_obj(6, 2, (float)M->b[i].sec, (float)j / (float)M->b[i].n, 1.0f);
#endif
    dbglog(DBG_INFO, "MJ: peticion %d subtipo %d: %d avisos, %d puertas\n", pet, M->sub, M->na, M->nb);
}
/* 0x2a1ba0: el subtipo 2 (el sonido) */
static void prepara2(int rnd)
{
    int k, s, v;
    for (k = 0; k < M->nb; k++) {
        mj_ent_t *b = &M->b[k]; int n = b->n > 6 ? 6 : b->n, malo;
        if (!n) continue;
        b->buena = (uint8_t)(rand() % n);
        v = rnd >= 0 ? rnd % 7 : rand() % 7;
        if (k < M->na) { M->a[k].sim[0] = (int8_t)v; M->a[k].n = 1; }
        for (s = 0; s < n; s++) {
            if (s == b->buena) { b->sim[s] = (int8_t)v; continue; }
            do {                                        /* uno al azar distinto del bueno y de los de antes */
                int i; b->sim[s] = (int8_t)(rand() % 7); malo = b->sim[s] == v;
                for (i = 0; i < s && !malo; i++) malo = b->sim[i] == b->sim[s];
            } while (malo);
        }
    }
}
/* 0x2a1150: el subtipo 0 (la memoria): el aviso k muestra a->n frutas al azar (de 6) y la puerta k dos filas de a->n: la
   buena (al azar) con las mismas y la otra con otras (al menos una distinta en su sitio) */
static void prepara0(void)
{
    int k, s, n;
    for (k = 0; k < M->na && k < M->nb; k++) {
        mj_ent_t *a = &M->a[k], *b = &M->b[k]; int iguales;
        n = a->n > 3 ? 3 : a->n;
        b->n = (uint8_t)(2 * n);
        for (s = 0; s < n; s++) a->sim[s] = (int8_t)(rand() % 6);
        b->buena = (uint8_t)(rand() % 2);
        for (s = 0; s < n; s++) b->sim[b->buena * n + s] = a->sim[s];
        do {
            for (iguales = s = 0; s < n; s++) { b->sim[(1 - b->buena) * n + s] = (int8_t)(rand() % 6); iguales += b->sim[(1 - b->buena) * n + s] == a->sim[s]; }
        } while (iguales == n);
    }
}
/* 0x2a1618: el subtipo 1 (el distinto): por puerta una clase al azar (0 las frutas, cuantos1[0], 1 los animales, cuantos1[1]);
   el carril bueno con uno de esa clase y los demas con otros distintos de la otra; el aviso k igual que la puerta */
static void prepara1(void)
{
    int k, s, i, c0 = M->m[68], c1 = M->m[69];
    for (k = 0; k < M->nb; k++) {
        mj_ent_t *b = &M->b[k]; int n = b->n > 6 ? 6 : b->n, cl = rand() % 2, v, malo;
        if (!n || !c0 || !c1) continue;
        b->buena = (uint8_t)(rand() % n);
        v = cl ? c0 + rand() % c1 : rand() % c0;
        for (s = 0; s < n; s++) {
            if (s == b->buena) { b->sim[s] = (int8_t)v; continue; }
            do {
                b->sim[s] = (int8_t)(cl ? rand() % c0 : c0 + rand() % c1); malo = b->sim[s] == v;
                for (i = 0; i < s && !malo; i++) malo = b->sim[i] == b->sim[s];
            } while (malo);
        }
    }
    for (k = 0; k < M->na && k < M->nb; k++) { M->a[k].n = M->b[k].n; memcpy(M->a[k].sim, M->b[k].sim, sizeof M->a[k].sim); }
}
/* 0x29fb28: el jugador por el carril k de n (lat: la fraccion de camp_obj) */
static int carril_ok(float lat, int n, int k) { return n > 0 && (float)k / (float)n < lat && lat < (float)(k + 1) / (float)n; }

float mj_paso(int pet, int sub, int fase, int rnd, const camp_coche_t *c, void (*voz)(int id, int retardo, int prio))
{
    float dm = 0.0f, lat = 1.0f - c->lat;               /* (c->lat: 0 el borde derecho; la de camp_obj, 0 el izquierdo) */
    int k;
    if (!M) return 0.0f;
    memcpy(M->yo, c->xyz, sizeof M->yo); M->js = c->seccion;
    if (M->destello > 0) M->destello++;
    if (fase == 1) {
        M->sub = sub; reparte(pet);
        if (sub == 0) prepara0(); else if (sub == 1) prepara1(); else if (sub == 2) prepara2(rnd);
        return 0.0f;
    }
    if (M->pet != pet) return 0.0f;
    if (fase >= 7) { M->pet = -1; return 0.0f; }
    if (fase >= 4 && fase <= 6) {
        if (sub <= 2 && fase == 4) {                    /* 0x2a13f8 / 0x2a1948 / 0x2a1e00 */
            for (k = 0; k < M->na && sub; k++) {        /* (los avisos: el 1 a 20 secciones, solo 0x232350; el 2 su voz) */
                mj_ent_t *a = &M->a[k];
                if (!a->pend || c->seccion < a->sec - (sub == 1 ? 20 : 0)) continue;
                if (sub == 2 && a->sim[0] >= 0 && voz) voz(u16_(M->m + 2 * a->sim[0]), 0x3c, 0xb);
                a->pend = 0;
                dbglog(DBG_INFO, "MJ: aviso %d en la seccion %d: animal %d\n", k, a->sec, a->sim[0]);
            }
            for (k = 0; k < M->nb; k++) {
                mj_ent_t *b = &M->b[k];
                if (!b->pend || c->seccion < b->sec) continue;
#ifdef PRUEBA_MJ
                lat = ((float)b->buena + 0.5f) / (float)(sub == 0 ? 2 : b->n ? b->n : 1);   /* prueba: por el carril bueno (el piloto no elige) */
#endif
                if (carril_ok(lat, sub ? b->n : 2, b->buena)) { dm += 80.0f * 1.1f / (float)M->nb; M->destello = 1; }
                else if (camp_se) camp_se(0xd4);
                b->pend = 0;
                dbglog(DBG_INFO, "MJ: puerta %d en la seccion %d: carril bueno %d de %d, el jugador en %.2f: %s\n", k, b->sec, b->buena, b->n,
                       (double)lat, dm > 0.0f ? "bien" : "mal");
            }
        }
        if (++M->c13a0 % 600 == 0 && voz) {            /* el animo de los minijuegos (0x29f980..) */
            int i = (int)(++M->c1398), id = sub < 2 ? u16_(M->m + 14 + 2 * (i % 2)) : sub == 2 ? u16_(M->m + 14 + 4 + 2 * (i % 3))
                                                                                               : u16_(M->m + 14 + 12 + 2 * (i % 3));
            voz(id, 0x3c, 0);
        }
    }
    if (fase >= 2 && fase <= 6 && ++M->t1204 >= 0x1f) M->t1204 = 0;
    return dm;
}

int mj_spr3d(haobj_s3d_t *v, int max, int (*cuadros)(const char *g))
{
    int k, i, m = 0;
    if (!M || M->pet < 0) return 0;
#ifdef DIAG_SIN_MJ
    return 0;                                           /* medida: sin los sprites */
#endif
    if (M->destello > 0 && m < max) {                   /* 0x114b68(0x2c0002, 5, 3): el destello del acierto (SUPUESTO: sobre el coche) */
        int nc = cuadros ? cuadros("HA_S02") : 1;
        if (M->destello > nc) M->destello = 0;
        else { strcpy(v[m].g, "HA_S02"); v[m].cuadro = M->destello - 1; memcpy(v[m].p, M->yo, sizeof v[m].p); v[m].p[1] += 1.5f; v[m].esc = 0.7f; m++; }   /* (la de los arcos) */
    }
    if (M->sub > 2 || !mj_mundo) return m;
    {   float bob = sinf((float)M->t1204 / 30.0f * 3.14159265f); int lista;
        for (lista = 0; lista < 2; lista++)             /* 0x2a1548 / 0x2a1ae8 / 0x2a1fc8: los avisos y las puertas (0x2a0138) */
            for (k = 0; k < (lista ? M->nb : M->na); k++) {
                mj_ent_t *e = lista ? &M->b[k] : &M->a[k];
                int col = M->sub == 0 && k < M->na ? M->a[k].n : 1, filas, f, j;
                if (M->sub == 2 && e->cl != 1) continue;   /* (el 2: solo las puertas) */
                if (e->sec < M->js - 2 || e->sec >= M->js + 50 || col < 1 || col > 3) continue;
                filas = e->n / col;
                for (f = 0; f < filas; f++) {
                    float w = 1.0f / (float)filas, l = w * (float)f + w * 0.5f;
                    if (l < 0.5f) l -= w * 0.1f; else if (l > 0.5f) l += w * 0.1f;
                    if (l < 0.0f) l = 0.0f;
                    if (l > 1.0f) l = 1.0f;
                    for (j = 0; j < col && m < max; j++) {
                        int sm = f * col + j < 6 ? e->sim[f * col + j] : -1, o = (col - 1) * 3 + j;
                        if (sm < 0) continue;
                        mj_mundo((float)e->sec, l, f32_(72 + 4 * o), v[m].p);
                        v[m].p[1] += 2.5f + f32_(108 + 4 * o) + (e->cl == 1 || M->sub != 2 ? bob : 0.0f);
                        snprintf(v[m].g, sizeof v[m].g, "MJ_%02d", M->m[(M->sub == 0 ? SPR0 : M->sub == 1 ? SPR1 : SPR2) + sm]);
                        v[m].cuadro = 0; v[m].esc = 1.5f; m++;
                    }
                }
            }
    }
#ifdef PRUEBA_MJ
    {   static int t_; if (m && ++t_ % 60 == 0) dbglog(DBG_INFO, "MJ: %d sprites (%s) en %.0f %.0f %.0f, jugador en %.0f %.0f %.0f\n", m, v[m - 1].g,
                                                     (double)v[m - 1].p[0], (double)v[m - 1].p[1], (double)v[m - 1].p[2], (double)M->yo[0], (double)M->yo[1], (double)M->yo[2]); }
#endif
    return m;
}
