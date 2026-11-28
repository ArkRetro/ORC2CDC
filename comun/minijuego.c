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
   - 3, la suma (0x2a2080 / 0x2a24d8, MATH.PAK): el aviso con un numero por su nivel (+0x24, 0x3b5648 {de, hasta}: con el
     nivel 0 o rand < 0,5 de + rand % (hasta - de), si no rand % hasta o 1; negativo el nivel 1 con rand % 5 == 0 y el 2
     con rand & 3 == 0); en la puerta k el carril bueno (rand % n) con la suma de los avisos entre la puerta k - 1 y la k
     y los demas bueno - 5 + rand % 10 distintos de el y de los de antes. Numeros: 0x2a0838 con las cifras del banco 0x46.
   - 4, los regalos (0x2a26d0 / 0x2a2be0, ITEMS.PAK): perm[5] (0..4) al azar y N = +0x78 de los parametros de clases en
     juego; en cada hueco del aviso perm[0] (rand < 0,4), otro de perm[1..N-1] (> 0,8) o nada (5), hasta que haya uno de
     perm[0]; en la puerta el bueno cuenta los perm[0] de los avisos de antes y los demas bueno - 4 + rand % 8 (> 0 y
     distintos del bueno); el regalo a contar en el HUD (0x231fe0). Los regalos 0x423af8 a 1,3 y los numeros a 3,5.
   - 5, las ovejas (0x2a2de8 / 0x2a2e80, SHEEP.PAK): una puerta; desde el aviso 0 hasta la puerta - 40 secciones cada 30 +
     rand % 40 pasos (hasta 200) una oveja 0x460028 cruza la pantalla (0x228e60: (0, -50) a 0,55, velocidad 1 + 0,1 x rand
     % 10) con los efectos 0x188 y 0x423a70[rand % 3]; el carril bueno cuenta las ovejas, los demas bueno - 4 + rand % 8;
     zzz (0x460027, 0x2322a0) en el panel de la peticion desde el primer paso de la fase 4 hasta la 5.
   - 6, los coches (0x2a3300 / 0x2a3510, CARS: agente c2c9): la puerta 0 con su carril bueno al azar; 0x2a3bd0 da a cada
     coche de la lista +0x5c la clase +0x78 + 3 (+0x78 = +8, el azar del banco) con rand < 0,5 o una de las otras dos
     (3..5); 0x1f1a88 / 0x1f20e8 el trafico de la peticion con los modelos +0x64 o 0x104. 0x22d0a0, al abrir cada coche de
     la lista, cuenta en +0x7a los de la clase pedida y les da el modelo 0x415500 (los demas 0x415510 / 0x415530);
     0x19c6f0 los tine por su clase (3 rojo, 4 verde, 5 azul). En la fase 4 el carril bueno de la puerta 0 lleva la
     cuenta y los demas cuenta - 1 + rand % 3 (> 0, distintos de ella), cada vez que cambia; la puerta, como los demas.
     Numeros en la puerta a escala 3 (0x2a0788). Aqui (bloque 'M163'): main.c abre los coches de la lista con
     mj_coche y los tine (DESVIACION: color_mul, el multiplicador de los vertices). */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "minijuego.h"
#include "haobj.h"

void (*mj_mundo)(float sec, float lat, float dx, float p[3]);
#ifdef PRUEBA_AUTO                                      /* el registro de cada aviso y puerta, solo en las pruebas (el codigo */
#define MJLOG dbglog                                    /* de mas hacia saltar .init al siguiente bloque de 8 KB: -8K de RAM) */
#else
#define MJLOG(...) ((void)0)
#endif

#define NMAX 50
typedef struct {                                        /* un registro de la lista A o B (0x2c bytes en el original) */
    int16_t sim[6];                                     /* +0x00.. los simbolos (-1 ninguno; 3..5: los numeros) */
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
    const uint8_t *m2, *cpets; uint32_t ncpet;          /* 'M162' (3..5) y las peticiones de CAMP.DAT (sus parametros) */
    int ant, icono, t_ico, zzz, t_zzz, t_ov, n_ov;      /* la fase anterior (+4), el regalo del HUD (0x231fe0) y zzz */
    struct { int on; float cu, v; } ov[10];             /* las ovejas (+0x1208, 10 x 0xa0): cuadro y velocidad */
    const uint8_t *m3, *c6p; uint32_t n6;               /* (c2c9) 'M163' (el 6) y los coches de la peticion en curso */
    int n6c, nx6, cl6, cuenta, ini6;                    /* sus coches, el siguiente, la clase pedida (+0x78 + 3), +0x7a, inicio */
    int8_t c6[100];                                     /* la clase de cada coche (+0xb de su registro de +0x5c) */
} mj_t;
static mj_t *M;
/* el bloque 'M161': u16 voz_aviso2[7] (0), u16 animo[10] (14), u8 sprite[34] (34: 6 del subtipo 0, 21 del 1, 7 del 2), u8
   cuantos1[2] (68), 2, f32 columna[9] (72), altura[9] (108), u32 n (144) y las peticiones (148) */
#define CAB 148
#define SPR0 34
#define SPR1 (34 + 6)
#define SPR2 (34 + 27)
static float f32_(int o) { float f; memcpy(&f, M->m + o, 4); return f; }
/* 'M162' (detras de 'M161'): i32 nivel[3][2] (0), u8 signo[2] (24: + y -), cifra[2][10] (26: de los negativos, 36: de los
   demas), regalo[5] (46), oveja (51), zzz (52), 3, u16 efecto_oveja (56) y salto[3] (58) */
#define M2_SIG 24
#define M2_CIF 26
#define M2_REG 46
#define M2_OVE 51
#define M2_ZZZ 52
#define M2_SE 56
static int rnd15(void) { return rand() & 0x7fff; }   /* (x 2^-15: rand < 0,5 es < 16384, < 0,4 < 13108, > 0,8 > 26214) */

void mj_libera(void) { if (M && M->c6p && camp_trafpet) camp_trafpet(NULL); free(M); M = NULL; }
int mj_inicia(const uint8_t *d, uint32_t tam)
{
    uint32_t o, l; mj_t *q;
    mj_libera();
    for (o = 0; o + 8 + CAB <= tam; o++)                /* (el bloque va detras de los demas, sin alinear: se busca su firma) */
        if (!memcmp(d + o, "M161", 4) && (memcpy(&l, d + o + 4, 4), l >= CAB && o + 8 + l <= tam)) break;
    if (o + 8 + CAB > tam || !(q = calloc(1, sizeof *q))) return -1;
    q->m = d + o + 8; memcpy(&q->n, q->m + CAB - 4, 4); q->pets = q->m + CAB; q->pet = -1;
    q->c13a0 = (uint32_t)rand(); q->c1398 = (uint32_t)rand();
    o += 8 + l;                                         /* 'M162' justo detras (tools/conv_camp.py; sin el, los 3..5 sin portar) */
    if (o + 8 + 64 <= tam && !memcmp(d + o, "M162", 4)) q->m2 = d + o + 8;
    q->ncpet = ((const uint32_t *)d)[2]; l = ((const uint32_t *)d)[3];   /* (cdat_t: npet, opet; cpet_t de 52 bytes, p[6] en +36) */
    if (l + q->ncpet * 52 <= tam) q->cpets = d + l;
    for (o = 0; o + 8 + 24 <= tam; o++)                 /* (c2c9) 'M163' detras de 'C131' */
        if (!memcmp(d + o, "M163", 4) && (memcpy(&l, d + o + 4, 4), l >= 24 && o + 8 + l <= tam)) {
            q->m3 = d + o + 8; memcpy(&q->n6, q->m3 + 20, 4); break; }
    q->icono = q->zzz = -1; q->n_ov = 100; M = q;
    MJLOG(DBG_INFO, "MJ: %u peticiones del tipo 16, %u bytes%s\n", (unsigned)q->n, (unsigned)sizeof *q, q->m2 ? ", M162" : "");
    return 0;
}
static int u16_(const uint8_t *p) { return p[0] | p[1] << 8; }

/* 0x2a0d08 (y 0x2a3778: los conos de las puertas) */
static void reparte(int pet)
{
    mj_t *const Q = M;
    const uint8_t *e = Q->pets; uint32_t k; int i, j, nr = 0;
    Q->na = Q->nb = 0; Q->pet = pet; Q->t1204 = 0;
    for (k = 0; k < Q->n; k++, e += 4 + 8 * e[3]) if (u16_(e) == pet) { nr = e[3]; break; }
    if (k == Q->n) return;
    if (camp_obj) camp_obj(-1, 0, 0, 0, 0);
    for (i = 0; i < nr; i++) {
        const uint8_t *r = e + 4 + 8 * i; int cl = r[0], c = r[2], s0 = (int16_t)u16_(r + 4), s1 = (int16_t)u16_(r + 6);
        int m = c < 2 || s0 >= s1 ? 1 : c; float paso = m > 1 ? (float)(s1 - s0) / (float)(c - 1) : 0.0f;
        for (j = 0; j < m; j++) {
            mj_ent_t *x;
            if (cl == 0 && Q->na < NMAX) x = &Q->a[Q->na++];
            else if (cl == 1 && Q->nb < NMAX) x = &Q->b[Q->nb++];
            else continue;
            memset(x->sim, 0xff, sizeof x->sim); x->pend = 1; x->buena = 0; x->n = r[1]; x->cl = (uint8_t)cl;
            x->sec = (int16_t)(s0 + (int)(paso * (float)j));
        }
    }
#ifndef DIAG_SIN_MJ                                     /* (medida: sin los conos) */
    for (i = 0; i < Q->nb && camp_obj; i++)             /* 0x2a3778: conos rojos (6) en los bordes de los carriles */
        for (j = 0; j <= Q->b[i].n && Q->b[i].n; j++) camp_obj(6, 2, (float)Q->b[i].sec, (float)j / (float)Q->b[i].n, 1.0f);
#endif
    MJLOG(DBG_INFO, "MJ: peticion %d subtipo %d: %d avisos, %d puertas\n", pet, Q->sub, Q->na, Q->nb);
}
/* los demas carriles de la puerta b: base + rand % dn, distintos del bueno y (dist) de los de antes, y (pos) > 0 */
static void otros(mj_ent_t *b, int base, int dn, int dist, int pos)
{
    int s, i, v, g = b->sim[b->buena], malo, n = b->n > 6 ? 6 : b->n;
    for (s = 0; s < n; s++) {
        if (s == b->buena) continue;
        do {
            v = base + rand() % dn; malo = v == g || (pos && v <= 0);
            for (i = 0; i < s && dist && !malo; i++) malo = b->sim[i] == v;
            b->sim[s] = (int16_t)v;
        } while (malo);
    }
}
/* 0x2a1ba0: el subtipo 2 (el sonido) */
static void prepara2(int rnd)
{
    mj_t *const Q = M;
    int k, v;
    for (k = 0; k < Q->nb; k++) {
        mj_ent_t *b = &Q->b[k]; int n = b->n > 6 ? 6 : b->n;
        if (!n) continue;
        b->buena = (uint8_t)(rand() % n);
        v = rnd >= 0 ? rnd % 7 : rand() % 7;
        if (k < Q->na) { Q->a[k].sim[0] = (int16_t)v; Q->a[k].n = 1; }
        b->sim[b->buena] = (int16_t)v; otros(b, 0, 7, 1, 0);   /* (los demas al azar distintos del bueno y de los de antes) */
    }
}
/* 0x2a1150: el subtipo 0 (la memoria): el aviso k muestra a->n frutas al azar (de 6) y la puerta k dos filas de a->n: la
   buena (al azar) con las mismas y la otra con otras (al menos una distinta en su sitio) */
static void prepara0(void)
{
    mj_t *const Q = M;
    int k, s, n;
    for (k = 0; k < Q->na && k < Q->nb; k++) {
        mj_ent_t *a = &Q->a[k], *b = &Q->b[k]; int iguales;
        n = a->n > 3 ? 3 : a->n;
        b->n = (uint8_t)(2 * n);
        for (s = 0; s < n; s++) a->sim[s] = (int16_t)(rand() % 6);
        b->buena = (uint8_t)(rand() % 2);
        for (s = 0; s < n; s++) b->sim[b->buena * n + s] = a->sim[s];
        do {
            for (iguales = s = 0; s < n; s++) { b->sim[(1 - b->buena) * n + s] = (int16_t)(rand() % 6); iguales += b->sim[(1 - b->buena) * n + s] == a->sim[s]; }
        } while (iguales == n);
    }
}
/* 0x2a1618: el subtipo 1 (el distinto): por puerta una clase al azar (0 las frutas, cuantos1[0], 1 los animales, cuantos1[1]);
   el carril bueno con uno de esa clase y los demas con otros distintos de la otra; el aviso k igual que la puerta */
static void prepara1(void)
{
    mj_t *const Q = M;
    int k, c0 = Q->m[68], c1 = Q->m[69];
    for (k = 0; k < Q->nb; k++) {
        mj_ent_t *b = &Q->b[k]; int n = b->n > 6 ? 6 : b->n, cl = rand() % 2, v;
        if (!n || !c0 || !c1) continue;
        b->buena = (uint8_t)(rand() % n);
        v = cl ? c0 + rand() % c1 : rand() % c0;
        b->sim[b->buena] = (int16_t)v; otros(b, cl ? 0 : c0, cl ? c0 : c1, 1, 0);
    }
    for (k = 0; k < Q->na && k < Q->nb; k++) { Q->a[k].n = Q->b[k].n; memcpy(Q->a[k].sim, Q->b[k].sim, sizeof Q->a[k].sim); }
}
/* la puerta k cuenta lo de los avisos entre la puerta k - 1 y la k (estrictamente) */
static int entre(int k, int sec) { return (k == 0 || M->b[k - 1].sec < sec) && sec < M->b[k].sec; }
/* 0x2a2080: el subtipo 3 (la suma) */
static void prepara3(void)
{
    mj_t *const Q = M;
    int k, j, nv, de, ha, v;
    for (k = 0; k < Q->na; k++) {
        mj_ent_t *a = &Q->a[k];
        nv = a->n > 2 ? 2 : a->n; a->n = 1;
        memcpy(&de, Q->m2 + 8 * nv, 4); memcpy(&ha, Q->m2 + 8 * nv + 4, 4);
        if ((!nv || rnd15() < 16384) && ha > de) v = rand() % (ha - de) + de;
        else if (ha > 0) { v = rand() % ha; if (!v) v = 1; }
        else v = 1;
        if ((nv == 1 && rand() % 5 == 0) || (nv == 2 && (rand() & 3) == 0)) v = -v;
        a->sim[0] = (int16_t)v;
    }
    for (k = 0; k < Q->nb; k++) {
        mj_ent_t *b = &Q->b[k]; int n = b->n > 6 ? 6 : b->n;
        if (!n) continue;
        b->buena = (uint8_t)(rand() % n);
        for (j = 0; j < n; j++) b->sim[j] = 0;
        for (j = 0; j < Q->na; j++) if (entre(k, Q->a[j].sec)) b->sim[b->buena] += Q->a[j].sim[0];
        otros(b, b->sim[b->buena] - 5, 10, 1, 0);
    }
}
/* 0x2a26d0: el subtipo 4 (los regalos); nk: +0x78 de los parametros (las clases en juego) */
static void prepara4(int nk)
{
    mj_t *const Q = M;
    int perm[5] = { 0, 1, 2, 3, 4 }, k, j, s, t, cuantos;
    t = rand() % 5; s = perm[0]; perm[0] = perm[t]; perm[t] = s;
    for (k = 1; k < nk && k < 5; k++) { t = 1 + rand() % 4; s = perm[k]; perm[k] = perm[t]; perm[t] = s; }
    do {
        for (cuantos = k = 0; k < Q->na; k++) {
            mj_ent_t *a = &Q->a[k]; int n = a->n > 6 ? 6 : a->n;
            for (s = 0; s < n; s++) {
                int r = rnd15();
                if (r < 13108) { a->sim[s] = (int16_t)perm[0]; cuantos++; }
                else a->sim[s] = (int16_t)(r > 26214 && nk >= 2 ? perm[1 + rand() % (nk - 1)] : 5);
            }
        }
    } while (!cuantos && Q->na);
    for (k = 0; k < Q->nb; k++) {
        mj_ent_t *b = &Q->b[k]; int n = b->n > 6 ? 6 : b->n;
        if (!n) continue;
        b->buena = (uint8_t)(rand() % n);
        for (j = 0; j < n; j++) b->sim[j] = 0;
        for (j = 0; j < Q->na; j++)
            if (entre(k, Q->a[j].sec)) for (s = 0; s < Q->a[j].n && s < 6; s++) b->sim[b->buena] += Q->a[j].sim[s] == perm[0];
        otros(b, b->sim[b->buena] - 4, 8, 0, 1);
    }
    Q->icono = perm[0]; Q->t_ico = 0;                   /* 0x231fe0: el regalo a contar (0x423af8[perm[0]]) en el HUD */
}
/* 0x2a2de8: el subtipo 5 (las ovejas): la puerta 0 con su carril bueno y a cero */
static void prepara5(void)
{
    mj_t *const Q = M;
    int j;
    memset(Q->ov, 0, sizeof Q->ov); Q->t_ov = 0;
    if (!Q->nb || !Q->b[0].n) return;
    Q->b[0].buena = (uint8_t)(rand() % Q->b[0].n);
    for (j = 0; j < Q->b[0].n && j < 6; j++) Q->b[0].sim[j] = 0;
}
/* 0x2a2e80 (fases 1..4): las ovejas entre el aviso 0 y la puerta - 40; devuelve 1 si sale una */
static int ovejas(int js)
{
    mj_t *const Q = M;
    mj_ent_t *b = &Q->b[0]; int k, sale = 0;
    for (k = 0; k < 10; k++)                            /* 0x229120: avanzan y se van al acabar su escena */
        if (Q->ov[k].on && (Q->ov[k].cu += Q->ov[k].v) >= (float)Q->n_ov) Q->ov[k].on = 0;
    if (!Q->na || !Q->nb || !b->n || js < Q->a[0].sec || js >= b->sec - 40) return 0;
    if (b->sim[b->buena] < 200) Q->t_ov--;
    if (Q->t_ov > 0) return 0;
    for (k = 0; k < 10; k++) if (!Q->ov[k].on) {
        Q->ov[k].on = 1; Q->ov[k].cu = 0; Q->ov[k].v = (float)(rand() % 10) * 0.1f + 1.0f;   /* 0x228e60 / 0x229080 */
        Q->t_ov = rand() % 40 + 30;
        b->sim[b->buena]++; otros(b, b->sim[b->buena] - 4, 8, 0, 1);
        sale = 1; break;
    }
    return sale;
}
/* 0x2a3300 / 0x2a3bd0: el subtipo 6 (los coches); rnd: +8 de los parametros (el azar del banco CARS) */
static void prepara6(int pet, int rnd)
{
    mj_t *const Q = M;
    const uint8_t *e = Q->m3 ? Q->m3 + 24 : NULL; uint32_t k; int i, o0, o1;
    Q->c6p = NULL; Q->n6c = Q->nx6 = Q->cuenta = 0;
    for (k = 0; e && k < Q->n6; k++, e += 8 + 4 * e[2]) if (u16_(e) == pet) { Q->c6p = e + 8; Q->n6c = e[2] > 100 ? 100 : e[2]; break; }
    if (!Q->c6p) return;
    Q->ini6 = Q->cpets && (uint32_t)pet < Q->ncpet ? (int16_t)u16_(Q->cpets + 52 * pet) : 0;
    Q->cl6 = 3 + (rnd >= 0 ? rnd : rand()) % 3;          /* (+0x78 + 3; el banco tiene 3: PROBABLE) */
    o0 = Q->cl6 == 3 ? 4 : 3; o1 = Q->cl6 == 5 ? 4 : 5;   /* (las otras dos; con el 5 al reves que 0x2a3bd0: da igual, rand & 1) */
    for (i = 0; i < Q->n6c; i++) Q->c6[i] = (int8_t)(rnd15() < 16384 ? Q->cl6 : rand() & 1 ? o0 : o1);
    if (Q->nb && Q->b[0].n) { Q->b[0].buena = (uint8_t)(rand() % Q->b[0].n); memset(Q->b[0].sim, 0xff, sizeof Q->b[0].sim); }
    if (camp_trafpet) {                                 /* 0x1f20e8(+0x10): el trafico con los modelos +0x64 (o 0x104) */
        camp_trafpet_t r; int16_t m64 = (int16_t)u16_(e + 4);
        for (i = 0; i < 7; i++) r.modelo[i] = m64 != -1 ? m64 : 0x104;
        r.tope = 4; r.lod = 1; r.marca = 0; camp_trafpet(&r);   /* (los suyos son los de la lista: mj_coche) */
    }
    MJLOG(DBG_INFO, "MJ: coches: %d en la lista, la clase %d (azar %d), +0x64 %d\n", Q->n6c, Q->cl6, rnd, (int16_t)u16_(e + 4));
}
/* (main.c) el siguiente coche de la lista del 6 que abrir, como camp_coche14: seccion, carril y marca (HA_VISTO |
   HA_CAMP | indice + 1 | el modelo del juego << 8 | la clase - 2 << 16); 0x22d0a0 cuenta los de la clase pedida */
int mj_coche(int *sec, int *carril, int *marca)
{
    mj_t *const Q = M; const uint8_t *e; int c, mo;
#ifdef DIAG_SIN_MJ6
    return 0;                                           /* (medida: el 16/6 sin sus coches) */
#endif
    if (!M || Q->pet < 0 || Q->sub != 6 || !Q->c6p || Q->nx6 >= Q->n6c || Q->ant < 1 || Q->ant > 4) return 0;
    for (;;) {
        e = Q->c6p + 4 * Q->nx6;
        *sec = Q->ini6 + (int16_t)u16_(e); *carril = (int8_t)e[2];
        if (*sec >= Q->js + 5 || ++Q->nx6 >= Q->n6c) break;
    }
    if (Q->nx6 >= Q->n6c || *sec - Q->js > 40) return 0;
    c = Q->c6[Q->nx6];
    if (c == Q->cl6) Q->cuenta++;                       /* 0x22d278: +0x7a + 1 */
    mo = c == Q->cl6 ? Q->m3[rand() % 3] : Q->m3[3 + rand() % 7];   /* 0x415500 / 0x415510 (SIMPLIFICADO: al azar, sin 0x415530) */
    *marca = (int)(HA_VISTO | HA_CAMP) | (Q->nx6 + 1) | mo << 8 | (c - 2) << 16; Q->nx6++;
    MJLOG(DBG_INFO, "MJ: coche %d en la seccion %d carril %d: clase %d, modelo %d, cuenta %d\n", Q->nx6, *sec, *carril, c, mo, Q->cuenta);
    return 1;
}
/* 0x29fb28: el jugador por el carril k de n (lat: la fraccion de camp_obj) */
static int carril_ok(float lat, int n, int k) { return n > 0 && (float)k / (float)n < lat && lat < (float)(k + 1) / (float)n; }

float mj_paso(int pet, int sub, int fase, int rnd, const camp_coche_t *c, void (*voz)(int id, int retardo, int prio))
{
    mj_t *const Q = M;
    float dm = 0.0f, lat = 1.0f - c->lat;               /* (c->lat: 0 el borde derecho; la de camp_obj, 0 el izquierdo) */
    int k, fase_ant;
    if (!M) return 0.0f;
    memcpy(Q->yo, c->xyz, sizeof Q->yo); Q->js = c->seccion;
    if (Q->destello > 0) Q->destello++;
    k = Q->ant; Q->ant = fase; fase_ant = k;            /* (+4: la fase del paso anterior) */
    if (fase == 1) {
        Q->sub = sub; reparte(pet);
        if (sub == 0) prepara0(); else if (sub == 1) prepara1(); else if (sub == 2) prepara2(rnd);
        else if (Q->m2) {
            if (sub == 3) prepara3();
            else if (sub == 4) prepara4(Q->cpets && (uint32_t)pet < Q->ncpet ? (int)Q->cpets[52 * pet + 36] : 1);
            else if (sub == 5) prepara5();
        }
        if (sub == 6) prepara6(pet, rnd);
#ifdef PRUEBA_MJ                                        /* (prueba: lo preparado) */
        for (k = 0; k < Q->nb && sub >= 3; k++)
            dbglog(DBG_INFO, "MJ: puerta %d en la seccion %d: %d carriles, el bueno %d: %d %d %d%s\n", k, Q->b[k].sec, Q->b[k].n, Q->b[k].buena,
                   Q->b[k].sim[0], Q->b[k].sim[1], Q->b[k].sim[2], sub == 4 ? " (los regalos de los avisos)" : "");
        for (k = 0; k < Q->na && (sub == 3 || sub == 4); k++)
            dbglog(DBG_INFO, "MJ: aviso %d en la seccion %d: %d %d %d\n", k, Q->a[k].sec, Q->a[k].sim[0], Q->a[k].sim[1], Q->a[k].sim[2]);
#endif
        return 0.0f;
    }
    if (Q->pet != pet) return 0.0f;
    if (fase >= 7) { Q->pet = -1; Q->icono = Q->zzz = -1; return 0.0f; }
    Q->t_ico++; Q->t_zzz++;                             /* (los tiempos del regalo y de zzz: se ponen a 0 al salir) */
    if (Q->m2 && sub == 5) {                            /* 0x2a2e80 */
        if (fase_ant == 3) { Q->zzz = 0; Q->t_zzz = 0; }   /* 0x2322a0 (y +0x160 del gestor = 1: SIN averiguar) */
        else if (fase == 5) { memset(Q->ov, 0, sizeof Q->ov); Q->zzz = -1; }   /* 0x229708 y 0x232330 */
        else if (fase < 5 && ovejas(c->seccion) && camp_se) {
            camp_se(u16_(Q->m2 + M2_SE)); camp_se(u16_(Q->m2 + M2_SE + 2 + 2 * (rand() % 3)));
        }
    }
    if (fase >= 5) Q->icono = -1;                       /* (SUPUESTO: el regalo del HUD se va al acabar la peticion) */
    if (sub == 6 && fase >= 5 && fase_ant < 5 && Q->c6p && camp_trafpet) camp_trafpet(NULL);   /* 0x1f20e8(0) */
    if (sub == 6 && fase == 4 && Q->c6p && Q->nb && Q->b[0].n && Q->b[0].sim[Q->b[0].buena] != Q->cuenta) {   /* 0x2a3510 */
        mj_ent_t *b = &Q->b[0]; int s, v, n = b->n > 6 ? 6 : b->n;
        b->sim[b->buena] = (int16_t)Q->cuenta;
        for (s = 0; s < n; s++) if (s != b->buena) { do v = Q->cuenta + rand() % 3 - 1; while (v <= 0 || v == Q->cuenta); b->sim[s] = (int16_t)v; }
    }
    if (fase >= 4 && fase <= 6) {
        if ((sub <= 2 || (Q->m2 && sub <= 5) || (sub == 6 && Q->c6p)) && fase == 4) {   /* 0x2a13f8 / 0x2a1948 / 0x2a1e00 / 0x2a24d8 / 0x2a2be0 / 0x2a2e80 */
            for (k = 0; k < Q->na && (sub == 1 || sub == 2); k++) {   /* (los avisos: el 1 a 20 secciones, solo 0x232350; el 2 su voz) */
                mj_ent_t *a = &Q->a[k];
                if (!a->pend || c->seccion < a->sec - (sub == 1 ? 20 : 0)) continue;
                if (sub == 2 && a->sim[0] >= 0 && voz) voz(u16_(Q->m + 2 * a->sim[0]), 0x3c, 0xb);
                a->pend = 0;
                MJLOG(DBG_INFO, "MJ: aviso %d en la seccion %d: animal %d\n", k, a->sec, a->sim[0]);
            }
            for (k = 0; k < Q->nb; k++) {
                mj_ent_t *b = &Q->b[k];
                if (!b->pend || c->seccion < b->sec) continue;
#ifdef PRUEBA_MJ
                lat = ((float)b->buena + 0.5f) / (float)(sub == 0 ? 2 : b->n ? b->n : 1);   /* prueba: por el carril bueno (el piloto no elige) */
#endif
                if (carril_ok(lat, sub ? b->n : 2, b->buena)) { dm += 80.0f * 1.1f / (float)Q->nb; Q->destello = 1; }
                else if (camp_se) camp_se(0xd4);
                b->pend = 0;
                MJLOG(DBG_INFO, "MJ: puerta %d en la seccion %d: carril bueno %d de %d (%d %d %d), el jugador en %.2f: %s\n", k, b->sec, b->buena, b->n,
                       b->sim[0], b->sim[1], b->sim[2], (double)lat, dm > 0.0f ? "bien" : "mal");
            }
        }
        if (++Q->c13a0 % 600 == 0 && voz) {            /* el animo de los minijuegos (0x29f980..) */
            int i = (int)(++Q->c1398);                  /* (0x4239c0[k % 2], 0x4239c8[k % 3] el 2, 0x4239d8[k % 3] los demas) */
            voz(u16_(Q->m + 14 + (sub < 2 ? 2 * (i % 2) : (sub == 2 ? 4 : 12) + 2 * (i % 3))), 0x3c, 0);
        }
    }
    if (fase >= 2 && fase <= 6 && ++Q->t1204 >= 0x1f) Q->t1204 = 0;
    return dm;
}

/* un sprite del grupo MJ_kk en p (escala esc; esc < 0: en la pantalla, en (p[0], p[1]) desde el centro a escala -esc) */
static int pon(haobj_s3d_t *v, int m, int max, int kk, const float *p, float esc, int cuadro)
{
    if (m >= max) return m;
    snprintf(v[m].g, sizeof v[m].g, "MJ_%02d", kk); v[m].cuadro = cuadro; memcpy(v[m].p, p, sizeof v[m].p); v[m].esc = esc;
    return m + 1;
}
/* 0x2a0838: el numero val en p con la escala esc, las cifras a lo largo de r (el lateral de la carretera; DESVIACION: el
   original usa el eje x de la camara); fila: 1 / 2 se separan de lejos (la fila del carril de la puerta + 1, 0 el aviso);
   signo: el + o el - delante (el aviso, y los negativos) */
static int numero(haobj_s3d_t *v, int m, int max, int val, const float *p0, const float *r, float esc, int fila, int signo)
{
    mj_t *const Q = M;
    int a = val < 0 ? -val : val, c[3], nc = 0, k, j; float p[3], d = 0.0f, f, sp = esc * 1.25f, o;
    do c[nc++] = a % 10; while ((a /= 10) && nc < 3);  /* (las cifras al reves: c[nc - 1] la de mas peso) */
    for (j = 0; j < 3; j++) { p[j] = p0[j]; d += (p[j] - Q->yo[j]) * (p[j] - Q->yo[j]); }
    d = sqrtf(d);                                       /* (signo: 0 o 1) */
    if (d > 125.0f) {                                   /* de lejos: las dos filas se apartan, suben y se separan las cifras */
        f = (d - 125.0f) / 100.0f; if (f > 1.0f) f = 1.0f;
        o = fila == 1 ? -1.5f : fila == 2 ? 1.5f : 0.0f;
        for (j = 0; j < 3; j++) p[j] += r[j] * o * f * (float)(nc + signo);
        p[1] += 2.0f * f;
        if (esc < 2.6f) f *= 2.0f;
        sp += f * sp;
    }
    o = (float)(nc + signo) * sp * 0.25f;               /* (ancho / 4) */
    p[1] += 0.5f;
    for (k = -signo; k < nc; k++) {                     /* el signo en -ancho / 4, la cifra k en paso x (k + signo) - ancho / 4 */
        float q[3], x = k < 0 ? -o : sp * (float)(k + signo) - o;
        for (j = 0; j < 3; j++) q[j] = p[j] + r[j] * x;
        m = pon(v, m, max, k < 0 ? Q->m2[M2_SIG + (val < 0)] : Q->m2[M2_CIF + (val < 0 ? 0 : 10) + c[nc - 1 - k]], q, esc, 0);
    }
    return m;
}

int mj_spr3d(haobj_s3d_t *v, int max, int (*cuadros)(const char *g))
{
    mj_t *const Q = M;
    int k, i, m = 0, sub;
    if (!M || Q->pet < 0) return 0;
#ifdef DIAG_SIN_MJ
    return 0;                                           /* medida: sin los sprites */
#endif
    if (Q->destello > 0 && m < max) {                   /* 0x114b68(0x2c0002, 5, 3): el destello del acierto (SUPUESTO: sobre el coche) */
        int nc = cuadros ? cuadros("HA_S02") : 1;
        if (Q->destello > nc) Q->destello = 0;
        else { strcpy(v[m].g, "HA_S02"); v[m].cuadro = Q->destello - 1; memcpy(v[m].p, Q->yo, sizeof v[m].p); v[m].p[1] += 1.5f; v[m].esc = 0.7f; m++; }   /* (la de los arcos) */
    }
    sub = Q->sub;
    if (sub > 6 || (sub > 2 && !Q->m2) || (sub == 6 && !Q->c6p) || !mj_mundo) return m;
    if (sub == 5) {                                     /* las ovejas (0x228e60: capa 9, en (0, -50) a 0,55) y zzz */
        float p[3] = { 0.0f, -50.0f, 0.0f };
        for (k = 0; k < 10; k++) if (Q->ov[k].on) {
            m = pon(v, m, max, Q->m2[M2_OVE], p, -0.55f, (int)Q->ov[k].cu);
            if (cuadros && (i = cuadros(v[m - 1].g)) > 1) Q->n_ov = i;   /* (los cuadros de la escena: 100) */
        }
        if (Q->zzz >= 0) { p[0] = 176.0f; p[1] = 78.0f; m = pon(v, m, max, Q->m2[M2_ZZZ], p, -1.0f, Q->t_zzz % 120); }   /* (en el panel: SUPUESTO (176, 78)) */
    }
    if (sub == 4 && Q->icono >= 0) {    /* 0x231fe0 / 0x2320d8: crece en el panel (0,35 s) y se va arriba (180, -110) a 0,75 */
        float p[3] = { 176.0f, 78.0f, 0.0f }, t = (float)(Q->t_ico < 120 ? Q->t_ico : Q->t_ico - 120) / 21.0f;   /* (SUPUESTO: el */
        if (t > 1.0f) t = 1.0f;                         /* panel en (176, 78) y 2 s en el) */
        if (Q->t_ico >= 120) { p[0] += 4.0f * t; p[1] -= 188.0f * t; t = 2.0f + t; } else t *= 2.0f;
        m = pon(v, m, max, Q->m2[M2_REG + Q->icono], p, -0.25f * t - 0.001f, 0);   /* (0..0,5 crece; 0,5..0,75 va arriba) */
    }
    {   float bob = sinf((float)Q->t1204 / 30.0f * 3.14159265f); int lista;
        for (lista = 0; lista < 2; lista++)             /* 0x2a1548 / 0x2a1ae8 / 0x2a1fc8 / 0x2a2618 / 0x2a2d30 / 0x2a3280 (0x2a0138) */
            for (k = 0; k < (lista ? Q->nb : Q->na); k++) {
                mj_ent_t *e = lista ? &Q->b[k] : &Q->a[k];
                int col = sub == 0 && k < Q->na ? Q->a[k].n : 1, filas, f, j;
                if ((sub == 2 || sub >= 5) && e->cl != 1) continue;   /* (el 2, el 5 y el 6: solo las puertas) */
                if (e->sec < Q->js - 2 || e->sec >= Q->js + 50 || col < 1 || col > 3) continue;
                filas = e->n / col;
                for (f = 0; f < filas; f++) {
                    float w = 1.0f / (float)filas, l = w * (float)f + w * 0.5f, p[3], r[3];
                    if (l < 0.5f) l -= w * 0.1f; else if (l > 0.5f) l += w * 0.1f;
                    if (l < 0.0f) l = 0.0f;
                    if (l > 1.0f) l = 1.0f;
                    for (j = 0; j < col && m < max; j++) {
                        int sm = f * col + j < 6 ? e->sim[f * col + j] : -1, o = (col - 1) * 3 + j;
                        if (sub <= 2 && sm < 0) continue;
                        mj_mundo((float)e->sec, l, f32_(72 + 4 * o), p);
                        p[1] += 2.5f + f32_(108 + 4 * o) + bob;   /* (el vaiven: puertas y avisos de 0, 1, 3 y 4, los unicos que se ven) */
                        if (sub <= 2) { m = pon(v, m, max, Q->m[(sub == 0 ? SPR0 : sub == 1 ? SPR1 : SPR2) + sm], p, 1.5f, 0); continue; }
                        if (sub == 4 && e->cl != 1) {   /* los regalos del aviso (0x423af8 a 1,3; 5: nada) */
                            if (sm >= 0 && sm < 5) m = pon(v, m, max, Q->m2[M2_REG + sm], p, 1.3f, 0);
                            continue;
                        }
                        mj_mundo((float)e->sec, l, f32_(72 + 4 * o) + 1.0f, r);   /* (el lateral de la carretera, 1 m en xz) */
                        r[0] -= p[0]; r[1] = 0.0f; r[2] -= p[2];
                        {   float esc = sub == 4 ? 3.5f : 3.0f; int s0 = e->sim[0], s1 = e->sim[1];   /* (r: ya de 1 m en xz) */
                            if (e->cl != 1) p[1] += 3.0f;   /* (el aviso del 3: 3 m mas arriba y siempre con signo) */
                            else if (sub == 3 && (s0 < -9 || s0 >= 100 || s1 < -9 || s1 >= 100)) esc = 2.5f;
                            else if (sub == 5 && (s0 >= 100 || s1 >= 100)) esc = 2.5f;
                            m = numero(v, m, max, sm, p, r, esc, e->cl == 1 ? f + 1 : 0, e->cl != 1 || sm < 0);
                        }
                    }
                }
            }
    }
#ifdef PRUEBA_MJ
    {   static int t_; if (m && ++t_ % 60 == 0) dbglog(DBG_INFO, "MJ: %d sprites (%s) en %.0f %.0f %.0f, jugador en %.0f %.0f %.0f\n", m, v[m - 1].g,
                                                     (double)v[m - 1].p[0], (double)v[m - 1].p[1], (double)v[m - 1].p[2], (double)Q->yo[0], (double)Q->yo[1], (double)Q->yo[2]); }
#endif
    return m;
}
