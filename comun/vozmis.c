/* vozmis.c: las voces de los bancos de las peticiones del campeonato de COAST 2 COAST (ver vozmis.h).

   Como el original (SLES-53998, 0x22ac00 = el metodo de la etapa del gestor 0x616238, una maquina de estados +0xe8):
   estado 0, por cada peticion de la lista de la etapa el switch 0x415280 por el tipo (y 0x4152e0 / 0x415300 / 0x415320
   por el subtipo) da un banco de la tabla 0x352840 (HAM_MIS\..., 0x80nn) que va al hueco 0x600 / 0x700 / 0x800 de
   0x374180 si no lo lleva ya otro (+0xf4); despues 0x11ed40 los carga; estado 1, +0xf0 = rand % 4; estado 2 el banco
   RANK de 0x415268[+0xf0] (las voces de la nota, 0x2a65a8). La variante 5 tiene ademas COMMON.pak (hueco 1) y PERH.pak
   (hueco 9) desde el principio (0x1b7f74): VOCESH.DAT trae HC.PAK (casi lo mismo que COMMON) y PERH; lo que solo esta en
   COMMON va en el banco 0 de VOCESM.DAT. Aqui los bancos de la etapa se suben juntos a un bloque del AICA (el anterior se
   suelta) por un hilo que lee VOCESM.DAT de 32 KB en 32 KB (en la carrera no hay RAM para leerlos de una vez); mientras
   tanto no suenan (sfx_voz_extra con NULL). */
#include <kos.h>
#include <dc/sound/sound.h>
#include <dc/spu.h>
#include <string.h>
#include <stdlib.h>
#include "gd.h"
#include "sfx.h"
#include "vozmis.h"

#define VM_MAX 6                                        /* COMMON, RANK y 3 de peticion (+1 de margen) */
#define VM_TROZO 16                                     /* sectores por lectura (32 KB) */
typedef struct { char nom[12]; uint32_t o, n, bytes; } vm_banco_t;
typedef struct { uint16_t id, vol; uint32_t desp, bytes, frec, n; } vm_fila_t;
static struct {
    uint8_t *cab; uint32_t lba, tam; int nb; const vm_banco_t *b; const uint8_t *tab;
    vm_fila_t *filas; uint32_t aica;
    int lista[VM_MAX], nl; kthread_t *hilo; volatile int listo;
} V;

int vm_abre(void)
{
    uint32_t h[4], c; uint8_t *s;
    if (V.cab) return 0;
    if (gd_busca("VOCESM.DAT", &V.lba, &V.tam) || !(s = memalign(32, 2048))) return -1;
    if (gd_lee(V.lba, 1, s)) { free(s); return -2; }
    memcpy(h, s, 16); free(s);
    if (memcmp(h, "VZM1", 4) || !h[1] || h[1] > 64 || h[2] + 400 > 4 * 2048) return -2;
    c = (h[2] + 400 + 2047) & ~2047u;
    if (!(V.cab = memalign(32, c)) || gd_lee(V.lba, (int)(c >> 11), V.cab)) { free(V.cab); V.cab = NULL; return -3; }
    V.nb = (int)h[1]; V.b = (const vm_banco_t *)(V.cab + 16); V.tab = V.cab + h[2];
    dbglog(DBG_INFO, "VOZM: VOCESM.DAT, %d bancos\n", V.nb);
    return 0;
}

static void *vm_hilo(void *p)
{
    uint8_t *t = memalign(32, VM_TROZO * 2048); uint32_t off = 0; int j, k = 0;
    (void)p;
    if (!t) { V.listo = -1; return NULL; }
    for (j = 0; j < V.nl; j++) {
        const vm_banco_t *b = &V.b[V.lista[j]];
        uint32_t s0 = b->o >> 11, fin = b->o + 20 * b->n, od = (fin + 2047) & ~2047u, i, q;
        if (!b->n) continue;
        if (gd_lee(V.lba + s0, (int)(((fin + 2047) >> 11) - s0), t)) break;   /* (las filas: menos de 2 sectores) */
        memcpy(&V.filas[k], t + (b->o & 2047), 20 * b->n);
        for (i = 0; i < b->n; i++) V.filas[k + i].desp += off;
        for (q = 0; q < b->bytes; q += VM_TROZO * 2048) {
            uint32_t c = b->bytes - q > VM_TROZO * 2048 ? VM_TROZO * 2048 : b->bytes - q;
            if (gd_lee(V.lba + ((od + q) >> 11), (int)((c + 2047) >> 11), t)) break;
            spu_memload(V.aica + off + q, t, (c + 3) & ~3u);
        }
        if (q < b->bytes) break;
        k += (int)b->n; off += (b->bytes + 31) & ~31u;
    }
    free(t);
    if (j < V.nl) { V.listo = -1; dbglog(DBG_ERROR, "VOZM: no se pudo leer el banco %d\n", j); return NULL; }
    sfx_voz_extra(V.filas, k, V.aica); V.listo = 1;
    dbglog(DBG_INFO, "VOZM: %d voces de la etapa en el AICA\n", k);
    return NULL;
}
static void espera(void) { if (V.hilo) { thd_join(V.hilo, NULL); V.hilo = NULL; } }
static void suelta(void)
{
    espera(); sfx_voz_extra(NULL, 0, 0);
    if (V.aica) { snd_mem_free(V.aica); V.aica = 0; }
    free(V.filas); V.filas = NULL; V.nl = 0;
}
static void pon(int b)
{
    int j;
    if (b < 0 || b >= V.nb || V.nl >= VM_MAX) return;
    for (j = 0; j < V.nl && V.lista[j] != b; j++) ;     /* (+0xf4: sin repetir) */
    if (j == V.nl) V.lista[V.nl++] = b;
}

void vm_etapa(int n, const uint8_t *tipo, const uint8_t *sub, int fila, int *rnd)
{
    int i, j, nf = 0; uint32_t tam = 0;
    suelta();                                           /* (V.cab: vm_abre ya ha dicho que si; n <= 3, camp_etapa) */
    pon(0); pon(1 + (fila & 3));                        /* COMMON y RANK1..4 */
    for (i = 0; i < n && i < 3; i++) {                  /* 0x22ac00, estado 0: el de cada peticion (0x415280) */
        const uint8_t *e = V.tab + 2 * (8 * tipo[i] + sub[i]); int b = -1, k = -1;
        if (tipo[i] >= 25 || sub[i] >= 8 || e[0] == 0xff) ;
        else if (e[0] == 0xfe) {                        /* 16/2: SOUND con una sola peticion (0x22bd40 == 0x22bd60) */
            b = e[1]; if (n > 1) { k = rand() % 7; b += 1 + k; }
        } else {                                        /* 13 FOOD, 16/6 CARS: +8 = rand % n */
            b = e[0]; if (e[1] > 1) { k = rand() % e[1]; if (e[1] == 6) b += k; }
            /* FOOD1..6 son bancos seguidos (0x4152e0[rnd], n = 6); CARS (n = 3) es uno solo (0x22afec: el azar elige la
               voz y los coches, no el banco; con + rnd salian UFO.pak o HOLDLINE.pak, vacio, y el minijuego sin voces) */
        }
        if (b >= 0) pon(b);
        rnd[i] = k;
    }
    for (j = 0; j < V.nl; j++) { nf += (int)V.b[V.lista[j]].n; tam += (V.b[V.lista[j]].bytes + 31) & ~31u; }
    if (!nf || !(V.filas = malloc(20 * nf)) || !(V.aica = snd_mem_malloc(tam))) {
        dbglog(DBG_ERROR, "VOZM: sin sitio para %u KB de voces (%d)\n", (unsigned)(tam >> 10), nf);
        free(V.filas); V.filas = NULL; V.nl = 0; return;
    }
    V.listo = 0;
    dbglog(DBG_INFO, "VOZM: etapa con %d bancos (", V.nl);
    for (j = 0; j < V.nl; j++) dbglog(DBG_INFO, "%s%.12s", j ? " " : "", V.b[V.lista[j]].nom);
    dbglog(DBG_INFO, "), %d voces, %u KB en el AICA\n", nf, (unsigned)(tam >> 10));
    if (!(V.hilo = thd_create(0, vm_hilo, NULL))) vm_hilo(NULL);
}

void vm_cierra(void)
{
    suelta(); free(V.cab); memset(&V, 0, sizeof V);
}
