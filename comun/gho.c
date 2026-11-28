/* Rev 2.9.9.c (modos "sp"): el coche fantasma de la CONTRARRELOJ con el coche de la grabacion.

   El original reproduce el fantasma con el modelo que se grabo (467E30 / 4AE0F0: alfa +B68), no con el del jugador. La
   Dreamcast lo dibuja con la forma simplificada GHO1 (tools/conv_coche.py fantasma: las envolventes de los volumenes de
   sombra, 182-566 triangulos sin textura; DESVIACION pedida en la Rev 2.9.2.a), asi que del coche de la grabacion solo
   hace falta su bloque GHO1 (hasta ~11 KB), no el COCHEnn.DAT entero (420-620 KB de RAM y 150-200 KB de VRAM, que no
   caben con el del jugador). gho_lee lo saca del COCHEnnL.DAT (o COCHEnn.DAT) del GD sin leer el fichero entero: la
   cabecera ORD7 da donde empieza la ruta del modelo (en los coches, el bloque CEX2: 'CEX2' 8 colores ... ; detras SVL1,
   ANI1 y GHO1), y se recorre sector a sector (un sector de 2 KB de cache) hasta GHO1, que se copia a dst.
   Formato (tools/modelo.py escribe / demo/main.c lee_modelo_en): 'ORD7' + 13 u32 (mallas, vertices, ruta, paso, texturas,
   ...), texturas de 24 bytes, mallas de 32, vertices de 24 y la ruta (ruta x paso floats) */
#include <kos.h>
#include <string.h>
#include "gd.h"
#include "gho.h"

static uint32_t g_lba, g_tam, g_sec = 0xffffffffu;
static uint8_t *g_cache;
static int palabra(uint32_t off, uint32_t *w)          /* el u32 del byte off del fichero (alineado a 4) */
{
    uint32_t s = off >> 11;
    if (off + 4 > g_tam) return -1;
    if (s != g_sec) { if (gd_lee(g_lba + s, 1, g_cache)) return -1; g_sec = s; }
    memcpy(w, g_cache + (off & 2047), 4);
    return 0;
}
/* (c2c9) frio, solo al cargar: -Os (el codigo cuenta en pasos de 8 KB de RAM) */
__attribute__((optimize("Os"))) int gho_lee(const char *fich, void *dst, uint32_t cap, uint8_t *sector, uint32_t *n)
{
    uint32_t h[13], i, r, w[6], q, fin, k, nv, a, b, t;
    *n = 0;
    if (gd_busca(fich, &g_lba, &g_tam) || g_tam < 64) return -1;
    g_cache = sector; g_sec = 0xffffffffu;
    if (palabra(0, w) || memcmp(w, "ORD7", 4)) return -2;
    for (i = 0; i < 13; i++) if (palabra(4 + 4 * i, h + i)) return -2;
    if (h[3] != 1 || h[2] < 6) return -3;                   /* (los coches: la ruta es CEX2, un u32 por punto) */
    r = 56 + h[4] * 24 + h[0] * 32 + h[1] * 24;             /* la ruta (CEX2) */
    fin = r + h[2] * 4;
    for (i = 0; i < 6; i++) if (palabra(r + 4 * i, w + i)) return -4;
    if (memcmp(w, "CEX2", 4) || w[1] != 8) return -4;
    q = r + 4 * (6 + 8 * w[2] + 5 * w[3] + w[4] + 2 * w[5]);
    if (palabra(q, &t)) return -5;
    if (!memcmp(&t, "SVL1", 4)) {                           /* volumenes de sombra: se saltan */
        if (palabra(q + 4, &nv)) return -5;
        q += 8;
        for (k = 0; k < nv && q + 8 <= fin; k++) {
            if (palabra(q, &a) || palabra(q + 4, &b)) return -5;
            q += 4 * (2 + 3 * a + 3 * b);
        }
    }
    if (palabra(q, &t) || memcmp(&t, "ANI1", 4) || palabra(q + 4, &a)) return -6;
    q += 4 * a;                                             /* GHO1, detras de ANI1 */
    if (palabra(q, &t) || memcmp(&t, "GHO1", 4) || palabra(q + 8, &b)) return -7;
    if (q + 12 + 20 * b > fin || 20 * b > cap) return -8;
    for (k = 0; k < 5 * b; k++) if (palabra(q + 12 + 4 * k, (uint32_t *)dst + k)) return -9;
    *n = b;
    return 0;
}
