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
   El 6 (los coches): SIN PORTAR (se anuncia, tiene sus conos y sus voces, y acaba con el medidor a cero; necesita el
   trafico de la peticion 0x1f1a88); ver docs/PARIDAD.md. */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "minijuego.h"
#include "haobj.h"

void (*mj_mundo)(float sec, float lat, float dx, float p[3]);

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
static float rnd15(void) { return (float)(rand() & 0x7fff) * 3.0517578e-5f; }

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
    for (o += 8 + l; o + 8 + 64 <= tam; o++)            /* 'M162' detras (tools/conv_camp.py; sin el, los 3..5 sin portar) */
        if (!memcmp(d + o, "M162", 4) && (memcpy(&l, d + o + 4, 4), l >= 64 && o + 8 + l <= tam)) { M->m2 = d + o + 8; break; }
    memcpy(&M->ncpet, d + 8, 4); memcpy(&l, d + 12, 4); /* (cdat_t: npet, opet; cpet_t de 52 bytes, p[6] = +0x78 en +36) */
    if (l + M->ncpet * 52 <= tam) M->cpets = d + l;
    M->icono = M->zzz = -1; M->n_ov = 100;
    dbglog(DBG_INFO, "MJ: %u peticiones del tipo 16, %u bytes%s\n", (unsigned)M->n, (unsigned)sizeof *M, M->m2 ? ", M162" : "");
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
        if (k < M->na) { M->a[k].sim[0] = (int16_t)v; M->a[k].n = 1; }
        for (s = 0; s < n; s++) {
            if (s == b->buena) { b->sim[s] = (int16_t)v; continue; }
            do {                                        /* uno al azar distinto del bueno y de los de antes */
                int i; b->sim[s] = (int16_t)(rand() % 7); malo = b->sim[s] == v;
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
    int k, s, i, c0 = M->m[68], c1 = M->m[69];
    for (k = 0; k < M->nb; k++) {
        mj_ent_t *b = &M->b[k]; int n = b->n > 6 ? 6 : b->n, cl = rand() % 2, v, malo;
        if (!n || !c0 || !c1) continue;
        b->buena = (uint8_t)(rand() % n);
        v = cl ? c0 + rand() % c1 : rand() % c0;
        for (s = 0; s < n; s++) {
            if (s == b->buena) { b->sim[s] = (int16_t)v; continue; }
            do {
                b->sim[s] = (int16_t)(cl ? rand() % c0 : c0 + rand() % c1); malo = b->sim[s] == v;
                for (i = 0; i < s && !malo; i++) malo = b->sim[i] == b->sim[s];
            } while (malo);
        }
    }
    for (k = 0; k < M->na && k < M->nb; k++) { M->a[k].n = M->b[k].n; memcpy(M->a[k].sim, M->b[k].sim, sizeof M->a[k].sim); }
}
/* la puerta k cuenta lo de los avisos entre la puerta k - 1 y la k (estrictamente) */
static int entre(int k, int sec) { return (k == 0 || M->b[k - 1].sec < sec) && sec < M->b[k].sec; }
/* los demas carriles de la puerta b: bueno + d0 + rand % dn, distintos del bueno y (dist) de los de antes, y (pos) > 0 */
static void otros(mj_ent_t *b, int d0, int dn, int dist, int pos)
{
    int s, i, v, g = b->sim[b->buena], malo, n = b->n > 6 ? 6 : b->n;
    for (s = 0; s < n; s++) {
        if (s == b->buena) continue;
        do {
            v = g + d0 + rand() % dn; malo = v == g || (pos && v <= 0);
            for (i = 0; i < s && dist && !malo; i++) malo = b->sim[i] == v;
            b->sim[s] = (int16_t)v;
        } while (malo);
    }
}
/* 0x2a2080: el subtipo 3 (la suma) */
static void prepara3(void)
{
    int k, j, nv, de, ha, v;
    for (k = 0; k < M->na; k++) {
        mj_ent_t *a = &M->a[k];
        nv = a->n > 2 ? 2 : a->n; a->n = 1;
        memcpy(&de, M->m2 + 8 * nv, 4); memcpy(&ha, M->m2 + 8 * nv + 4, 4);
        if ((!nv || rnd15() < 0.5f) && ha > de) v = rand() % (ha - de) + de;
        else if (ha > 0) { v = rand() % ha; if (!v) v = 1; }
        else v = 1;
        if ((nv == 1 && rand() % 5 == 0) || (nv == 2 && (rand() & 3) == 0)) v = -v;
        a->sim[0] = (int16_t)v;
    }
    for (k = 0; k < M->nb; k++) {
        mj_ent_t *b = &M->b[k]; int n = b->n > 6 ? 6 : b->n;
        if (!n) continue;
        b->buena = (uint8_t)(rand() % n);
        for (j = 0; j < n; j++) b->sim[j] = 0;
        for (j = 0; j < M->na; j++) if (entre(k, M->a[j].sec)) b->sim[b->buena] += M->a[j].sim[0];
        otros(b, -5, 10, 1, 0);
    }
}
/* 0x2a26d0: el subtipo 4 (los regalos); nk: +0x78 de los parametros (las clases en juego) */
static void prepara4(int nk)
{
    int perm[5] = { 0, 1, 2, 3, 4 }, k, j, s, t, cuantos;
    t = rand() % 5; s = perm[0]; perm[0] = perm[t]; perm[t] = s;
    for (k = 1; k < nk && k < 5; k++) { t = 1 + rand() % 4; s = perm[k]; perm[k] = perm[t]; perm[t] = s; }
    do {
        for (cuantos = k = 0; k < M->na; k++) {
            mj_ent_t *a = &M->a[k]; int n = a->n > 6 ? 6 : a->n;
            for (s = 0; s < n; s++) {
                float r = rnd15();
                if (r < 0.4f) { a->sim[s] = (int16_t)perm[0]; cuantos++; }
                else a->sim[s] = (int16_t)(r > 0.8f && nk >= 2 ? perm[1 + rand() % (nk - 1)] : 5);
            }
        }
    } while (!cuantos && M->na);
    for (k = 0; k < M->nb; k++) {
        mj_ent_t *b = &M->b[k]; int n = b->n > 6 ? 6 : b->n;
        if (!n) continue;
        b->buena = (uint8_t)(rand() % n);
        for (j = 0; j < n; j++) b->sim[j] = 0;
        for (j = 0; j < M->na; j++)
            if (entre(k, M->a[j].sec)) for (s = 0; s < M->a[j].n && s < 6; s++) b->sim[b->buena] += M->a[j].sim[s] == perm[0];
        otros(b, -4, 8, 0, 1);
    }
    M->icono = perm[0]; M->t_ico = 0;                   /* 0x231fe0: el regalo a contar (0x423af8[perm[0]]) en el HUD */
}
/* 0x2a2de8: el subtipo 5 (las ovejas): la puerta 0 con su carril bueno y a cero */
static void prepara5(void)
{
    int j;
    memset(M->ov, 0, sizeof M->ov); M->t_ov = 0;
    if (!M->nb || !M->b[0].n) return;
    M->b[0].buena = (uint8_t)(rand() % M->b[0].n);
    for (j = 0; j < M->b[0].n && j < 6; j++) M->b[0].sim[j] = 0;
}
/* 0x2a2e80 (fases 1..4): las ovejas entre el aviso 0 y la puerta - 40; devuelve 1 si sale una */
static int ovejas(int js)
{
    mj_ent_t *b = &M->b[0]; int k, sale = 0;
    for (k = 0; k < 10; k++)                            /* 0x229120: avanzan y se van al acabar su escena */
        if (M->ov[k].on && (M->ov[k].cu += M->ov[k].v) >= (float)M->n_ov) M->ov[k].on = 0;
    if (!M->na || !M->nb || !b->n || js < M->a[0].sec || js >= b->sec - 40) return 0;
    if (b->sim[b->buena] < 200) M->t_ov--;
    if (M->t_ov > 0) return 0;
    for (k = 0; k < 10; k++) if (!M->ov[k].on) {
        M->ov[k].on = 1; M->ov[k].cu = 0; M->ov[k].v = (float)(rand() % 10) * 0.1f + 1.0f;   /* 0x228e60 / 0x229080 */
        M->t_ov = rand() % 40 + 30;
        b->sim[b->buena]++; otros(b, -4, 8, 0, 1);
        sale = 1; break;
    }
    return sale;
}
/* 0x29fb28: el jugador por el carril k de n (lat: la fraccion de camp_obj) */
static int carril_ok(float lat, int n, int k) { return n > 0 && (float)k / (float)n < lat && lat < (float)(k + 1) / (float)n; }

float mj_paso(int pet, int sub, int fase, int rnd, const camp_coche_t *c, void (*voz)(int id, int retardo, int prio))
{
    float dm = 0.0f, lat = 1.0f - c->lat;               /* (c->lat: 0 el borde derecho; la de camp_obj, 0 el izquierdo) */
    int k, fase_ant;
    if (!M) return 0.0f;
    memcpy(M->yo, c->xyz, sizeof M->yo); M->js = c->seccion;
    if (M->destello > 0) M->destello++;
    k = M->ant; M->ant = fase; fase_ant = k;            /* (+4: la fase del paso anterior) */
    if (fase == 1) {
        M->sub = sub; reparte(pet); M->icono = M->zzz = -1;
        if (sub == 0) prepara0(); else if (sub == 1) prepara1(); else if (sub == 2) prepara2(rnd);
        else if (M->m2 && sub == 3) prepara3();
        else if (M->m2 && sub == 4) prepara4(M->cpets && (uint32_t)pet < M->ncpet ? (int)M->cpets[52 * pet + 36] : 1);
        else if (M->m2 && sub == 5) prepara5();
        for (k = 0; k < M->nb && sub >= 3; k++)
            dbglog(DBG_INFO, "MJ: puerta %d en la seccion %d: %d carriles, el bueno %d: %d %d %d%s\n", k, M->b[k].sec, M->b[k].n, M->b[k].buena,
                   M->b[k].sim[0], M->b[k].sim[1], M->b[k].sim[2], sub == 4 ? " (los regalos de los avisos)" : "");
        for (k = 0; k < M->na && (sub == 3 || sub == 4); k++)
            dbglog(DBG_INFO, "MJ: aviso %d en la seccion %d: %d %d %d\n", k, M->a[k].sec, M->a[k].sim[0], M->a[k].sim[1], M->a[k].sim[2]);
        return 0.0f;
    }
    if (M->pet != pet) return 0.0f;
    if (fase >= 7) { M->pet = -1; M->icono = M->zzz = -1; return 0.0f; }
    if (M->icono >= 0) M->t_ico++;
    if (M->zzz >= 0) M->t_zzz++;
    if (M->m2 && sub == 5) {                            /* 0x2a2e80 */
        if (fase_ant == 3) { M->zzz = 0; M->t_zzz = 0; }   /* 0x2322a0 (y +0x160 del gestor = 1: SIN averiguar) */
        else if (fase == 5) { memset(M->ov, 0, sizeof M->ov); M->zzz = -1; }   /* 0x229708 y 0x232330 */
        else if (fase < 5 && ovejas(c->seccion) && camp_se) {
            camp_se(u16_(M->m2 + M2_SE)); camp_se(u16_(M->m2 + M2_SE + 2 + 2 * (rand() % 3)));
        }
    }
    if (fase >= 5) M->icono = -1;                       /* (SUPUESTO: el regalo del HUD se va al acabar la peticion) */
    if (fase >= 4 && fase <= 6) {
        if ((sub <= 2 || (M->m2 && sub <= 5)) && fase == 4) {   /* 0x2a13f8 / 0x2a1948 / 0x2a1e00 / 0x2a24d8 / 0x2a2be0 / 0x2a2e80 */
            for (k = 0; k < M->na && (sub == 1 || sub == 2); k++) {   /* (los avisos: el 1 a 20 secciones, solo 0x232350; el 2 su voz) */
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
                dbglog(DBG_INFO, "MJ: puerta %d en la seccion %d: carril bueno %d de %d (%d %d %d), el jugador en %.2f: %s\n", k, b->sec, b->buena, b->n,
                       b->sim[0], b->sim[1], b->sim[2], (double)lat, dm > 0.0f ? "bien" : "mal");
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
    int a = val < 0 ? -val : val, c[3], nc, k; float p[3], q[3], d, f = 0.0f, ancho, sp = esc * 1.25f, o;
    if (a >= 100) { nc = 3; c[0] = a / 100; c[1] = (a - c[0] * 100) / 10; c[2] = a % 10; }
    else if (a >= 10) { nc = 2; c[0] = a / 10; c[1] = a % 10; }
    else { nc = 1; c[0] = a; }
    if (c[0] > 9) c[0] = 9;
    memcpy(p, p0, sizeof p);
    d = sqrtf((p[0] - M->yo[0]) * (p[0] - M->yo[0]) + (p[1] - M->yo[1]) * (p[1] - M->yo[1]) + (p[2] - M->yo[2]) * (p[2] - M->yo[2]));
    if (d > 125.0f) {                                   /* de lejos: las dos filas se apartan, suben y se separan las cifras */
        f = (d - 125.0f) / 100.0f; if (f > 1.0f) f = 1.0f;
        if (fila == 1 || fila == 2) for (k = 0, o = f * (fila == 1 ? -1.5f : 1.5f) * (float)(nc + (signo ? 1 : 0)); k < 3; k++) p[k] += r[k] * o;
        p[1] += 2.0f * f;
        if (esc < 2.6f) f *= 2.0f;
        sp += f * sp;
    }
    ancho = (float)(nc + (signo ? 1 : 0)) * sp; o = ancho * 0.25f;
    if (signo) {
        for (k = 0; k < 3; k++) q[k] = p[k] - r[k] * o;
        q[1] += 0.5f; m = pon(v, m, max, M->m2[M2_SIG + (val < 0)], q, esc, 0);
    }
    for (k = 0; k < nc; k++) {
        float x = sp * (float)(k + (signo ? 1 : 0)) - o; int j;
        for (j = 0; j < 3; j++) q[j] = p[j] + r[j] * x;
        q[1] += 0.5f; m = pon(v, m, max, M->m2[M2_CIF + (val < 0 ? 0 : 10) + c[k]], q, esc, 0);
    }
    return m;
}

int mj_spr3d(haobj_s3d_t *v, int max, int (*cuadros)(const char *g))
{
    int k, i, m = 0, sub;
    if (!M || M->pet < 0) return 0;
#ifdef DIAG_SIN_MJ
    return 0;                                           /* medida: sin los sprites */
#endif
    if (M->destello > 0 && m < max) {                   /* 0x114b68(0x2c0002, 5, 3): el destello del acierto (SUPUESTO: sobre el coche) */
        int nc = cuadros ? cuadros("HA_S02") : 1;
        if (M->destello > nc) M->destello = 0;
        else { strcpy(v[m].g, "HA_S02"); v[m].cuadro = M->destello - 1; memcpy(v[m].p, M->yo, sizeof v[m].p); v[m].p[1] += 1.5f; v[m].esc = 0.7f; m++; }   /* (la de los arcos) */
    }
    sub = M->sub;
    if (sub > 5 || (sub > 2 && !M->m2) || !mj_mundo) return m;
    if (sub == 5) {                                     /* las ovejas (0x228e60: capa 9, en (0, -50) a 0,55) y zzz */
        float p[3] = { 0.0f, -50.0f, 0.0f };
        if (cuadros) { char g[8]; snprintf(g, sizeof g, "MJ_%02d", M->m2[M2_OVE]); if ((k = cuadros(g)) > 1) M->n_ov = k; }
        for (k = 0; k < 10; k++) if (M->ov[k].on) m = pon(v, m, max, M->m2[M2_OVE], p, -0.55f, (int)M->ov[k].cu);
        if (M->zzz >= 0) { p[0] = 176.0f; p[1] = 78.0f; m = pon(v, m, max, M->m2[M2_ZZZ], p, -1.0f, M->t_zzz % 120); }   /* (en el panel: SUPUESTO (176, 78)) */
    }
    if (sub == 4 && M->icono >= 0 && M->icono < 5) {    /* 0x231fe0 / 0x2320d8: crece en el panel (0,35 s) y se va arriba (180, -110) a 0,75 */
        float p[3] = { 176.0f, 78.0f, 0.0f }, e, t;     /* (SUPUESTO: el panel en (176, 78) y 2 s en el) */
        if (M->t_ico < 120) { t = (float)M->t_ico / 21.0f; e = 0.5f * (t < 1.0f ? t : 1.0f); }
        else { t = (float)(M->t_ico - 120) / 21.0f; if (t > 1.0f) t = 1.0f; p[0] += (180.0f - 176.0f) * t; p[1] += (-110.0f - 78.0f) * t; e = 0.5f + 0.25f * t; }
        if (e > 0.01f) m = pon(v, m, max, M->m2[M2_REG + M->icono], p, -e, 0);
    }
    {   float bob = sinf((float)M->t1204 / 30.0f * 3.14159265f); int lista;
        for (lista = 0; lista < 2; lista++)             /* 0x2a1548 / 0x2a1ae8 / 0x2a1fc8 / 0x2a2618 / 0x2a2d30 / 0x2a3280 (0x2a0138) */
            for (k = 0; k < (lista ? M->nb : M->na); k++) {
                mj_ent_t *e = lista ? &M->b[k] : &M->a[k];
                int col = sub == 0 && k < M->na ? M->a[k].n : 1, filas, f, j;
                if ((sub == 2 || sub == 5) && e->cl != 1) continue;   /* (el 2 y el 5: solo las puertas) */
                if (e->sec < M->js - 2 || e->sec >= M->js + 50 || col < 1 || col > 3) continue;
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
                        p[1] += 2.5f + f32_(108 + 4 * o) + (e->cl == 1 || sub == 0 || sub == 1 || sub == 3 || sub == 4 ? bob : 0.0f);
                        if (sub <= 2) { m = pon(v, m, max, M->m[(sub == 0 ? SPR0 : sub == 1 ? SPR1 : SPR2) + sm], p, 1.5f, 0); continue; }
                        if (sub == 4 && e->cl != 1) {   /* los regalos del aviso (0x423af8 a 1,3; 5: nada) */
                            if (sm >= 0 && sm < 5) m = pon(v, m, max, M->m2[M2_REG + sm], p, 1.3f, 0);
                            continue;
                        }
                        mj_mundo((float)e->sec, l, 1.0f, r);   /* (el lateral de la carretera) */
                        for (i = 0; i < 3; i++) r[i] -= p[i];
                        r[1] = 0.0f;
                        {   float n_ = sqrtf(r[0] * r[0] + r[2] * r[2]), esc = sub == 4 ? 3.5f : 3.0f; int s0 = e->sim[0], s1 = e->sim[1];
                            if (n_ > 0.0f) { r[0] /= n_; r[2] /= n_; }
                            if (e->cl != 1) p[1] += 3.0f;   /* (el aviso del 3: 3 m mas arriba y siempre con signo) */
                            else if (sub == 3 && (s0 < -9 || s0 >= 100 || s0 <= -100 || s1 < -9 || s1 >= 100 || s1 <= -100)) esc = 2.5f;
                            else if (sub == 5 && (s0 >= 100 || s1 >= 100)) esc = 2.5f;
                            m = numero(v, m, max, sm, p, r, esc, e->cl == 1 ? f + 1 : 0, e->cl != 1 || sm < 0);
                        }
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
