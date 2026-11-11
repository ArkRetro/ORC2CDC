/* HUD original (HUD.DAT, ver tools/conv_hud.py): carga y dibujo con la PowerVR a 640x480 */
#include <kos.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include "gd.h"
#include "hud.h"

typedef struct { uint16_t w, h, fmt, lista; uint32_t off, tam; } h_tex_t;
typedef struct { uint16_t tex, w, h, _r; float u0, v0, u1, v1; } h_spr_t;
typedef struct { char nombre[16]; uint16_t n, pri, np, _r; float paso; uint32_t _r2; } h_grp_t;
typedef struct { uint16_t grp, cuadro, spr, flags; float x, y, ax, ay, ang, sx, sy, alfa; } h_pie_t;   /* el del fichero */
/* la pieza en la RAM (24 bytes en vez de 40; hudd_abre la convierte): x, y, ax, ay en 1/8 de pixel, angulo en 1/64 de
   grado, escalas en 1/512 y alfa en 1/255. Error medido sobre MENU.DAT y HUD.DAT: 0,06 px, 0,007 grados, 0,001 de escala
   y alfa (menos de lo que se ve a 640x480). MENU.DAT: 30895 piezas, de 1207 a 724 KB de RAM (docs/DREAMCAST.md) */
typedef struct { uint16_t grp, cuadro, spr, flags; int16_t x, y, ax, ay, ang; uint16_t sx, sy; uint8_t alfa, _r; } h_pc_t;
typedef struct { char nombre[16]; uint16_t grp, alt; int16_t x, y, avance, capa; uint16_t nchar, _r; } h_uso_t;

/* un fichero con formato HUD1 (HUD.DAT, MENU.DAT) */
struct hud_dat {
    uint8_t *dat; uint32_t meta, lba;                       /* meta: bytes en la RAM (parcial) o 0 (fichero entero) */
    uint8_t *tx; uint32_t tam;                              /* parcial: el fichero entero en un bufer temporal (hud_carga) o NULL; bytes del fichero */
    h_tex_t *tex; h_spr_t *spr; h_grp_t *grp; h_pc_t *pie; h_uso_t *uso;
    h_pc_t **gp;                                            /* parcial: las piezas de cada grupo en su bloque (o NULL) */
    uint32_t n_tex, n_grp, n_uso;
    pvr_ptr_t *vtex; uint8_t *usada; uint32_t *dibujada;      /* dibujada: ultimo cuadro en que se uso la textura */
    pvr_poly_hdr_t *cab, *cabf, *caba;                      /* por textura: encima de todo / detras de la escena 3D / aditiva */
    int taco, g_fuera, off_taco;                            /* g_fuera: DIG_PUNTOS_FUERA (estilo 0xD, sin uso propio) */
    int16_t *pal;                                           /* por textura: paleta (pal_reparte) o -1; NULL sin paletas */
    uint8_t *con_pal;                                       /* por textura: subida con su paleta (cabeceras) */
    uint8_t *falta;                                         /* por textura: echada por hud_suelta_quietas y pedida */
    int prestado;                                           /* dat en la RAM de otro (hudd_tablas_en): no se libera */
};

static hud_dat_t *H, *hud_principal;                       /* H: el fichero que se esta dibujando */

/* Bufer de RAM prestado (hudd_bufer) para lo temporal de las subidas: los sectores leidos del GD de un fichero parcial
   (MENU.DAT: 100-400 KB por pantalla) y los indices de las texturas con paleta. Sin el (o si no cabe) se piden al
   monton como antes. demo/main.c presta el hueco de la union mientras no hay carrera (menus): el monton del menu ya no
   tiene esos bloques seguidos (docs/DREAMCAST.md, RAM del menu) */
static uint8_t *pres; static uint32_t pres_tam; static int pres_ocupado;
void hudd_bufer(void *p, uint32_t tam)
{
    pres = p && !((uintptr_t)p & 31) ? (uint8_t *)p : NULL; pres_tam = pres ? tam : 0; pres_ocupado = 0;
}
static uint8_t *toma(uint32_t n)
{
    if (pres && !pres_ocupado && n <= pres_tam) { pres_ocupado = 1; return pres; }
    return memalign(32, n);
}
static void deja(uint8_t *b) { if (b && b == pres) pres_ocupado = 0; else free(b); }
static int ult_tex = -1, modo_fondo;
static uint32_t tinte = 0xFFFFFF;                           /* color que multiplica las piezas (textos) */

/* Liberacion diferida de la VRAM: la PowerVR dibuja el cuadro anterior mientras la CPU prepara el siguiente, asi que
   una textura que se uso hace menos de HUDD_ESPERA cuadros puede estar leyendose todavia; si se libera y en su sitio se
   sube otra, ese cuadro sale con basura (fallos graficos al cambiar de pantalla en el menu). Se apunta y se libera
   cuando ya no la puede leer nadie (hudd_fin_cuadro, una vez por cuadro). */
#define HUDD_ESPERA 3
#define HUDD_PEND 128
static struct { pvr_ptr_t p; uint32_t cuadro; } pend[HUDD_PEND];
static int n_pend;
static uint32_t cuadro_hud = 16;                            /* cuadros dibujados (empieza lejos de 0) */

static void libera_vram(pvr_ptr_t p, uint32_t usada)
{
    if (cuadro_hud - usada >= HUDD_ESPERA || n_pend >= HUDD_PEND) { pvr_mem_free(p); return; }
    pend[n_pend].p = p; pend[n_pend].cuadro = usada; n_pend++;
}
void hudd_fin_cuadro(void)
{
    int i, j; hud_dat_t *h = hud_principal;
    if (h && h->falta)                                      /* las echadas en un apuro que se han querido dibujar */
        for (i = 0; i < (int)h->n_tex; i++) if (h->falta[i]) { h->falta[i] = 0; if (h->usada[i] && !h->vtex[i]) hudd_sube(h, i, 1); }
    cuadro_hud++;
    for (i = j = 0; i < n_pend; i++) {
        if (cuadro_hud - pend[i].cuadro >= HUDD_ESPERA) pvr_mem_free(pend[i].p);
        else pend[j++] = pend[i];
    }
    n_pend = j;
}
int hudd_pendiente(void) { return n_pend; }
/* libera ya lo pendiente: espera a que la PowerVR acabe los cuadros en curso (cargas: carrera, coche nuevo) */
void hudd_vacia(void)
{
    int i;
    if (!n_pend) return;
    thd_sleep(40);                                          /* mas de dos cuadros de dibujo */
    for (i = 0; i < n_pend; i++) pvr_mem_free(pend[i].p);
    n_pend = 0;
}
void hudd_color(uint32_t rgb) { tinte = rgb & 0xFFFFFF; }
static float zcapa, zpaso = 0.001f;

/* piezas del grupo g (en un fichero parcial cada grupo va en su bloque: lee_tablas) */
static const h_pc_t *pz(const hud_dat_t *h, int g) { return h->gp ? h->gp[g] : h->pie + h->grp[g].pri; }

static int busca_uso(const char *n)
{
    uint32_t i;
    for (i = 0; i < H->n_uso; i++) if (!strncmp(H->uso[i].nombre, n, 16)) return i;
    return -1;
}

/* grupos que se usan: todos menos los tacometros que no son el del coche */
static int grupo_util(int g)
{
    return strncmp(H->grp[g].nombre, "TACO_", 5) || g == H->taco;
}

static int lee_sect(uint32_t lba, uint32_t n, uint8_t *b)
{
    uint32_t hecho = 0;
    while (hecho < n) {                                     /* de 64 en 64 sectores, como gd_carga */
        int k = n - hecho > 64 ? 64 : (int)(n - hecho);
        if (gd_lee(lba + hecho, k, b + hecho * 2048)) return -1;
        hecho += k;
    }
    return 0;
}
/* pieza del fichero (40 bytes) -> la de la RAM (24) */
static void pieza_compacta(const h_pie_t *q, h_pc_t *r)
{
#define Q16(v, k) ((int16_t)lrintf((v) * (k)))
    r->grp = q->grp; r->cuadro = q->cuadro; r->spr = q->spr; r->flags = q->flags;
    r->x = Q16(q->x, 8); r->y = Q16(q->y, 8); r->ax = Q16(q->ax, 8); r->ay = Q16(q->ay, 8); r->ang = Q16(q->ang, 64);
    r->sx = (uint16_t)lrintf(q->sx * 512); r->sy = (uint16_t)lrintf(q->sy * 512);
    r->alfa = (uint8_t)lrintf((q->alfa < 0 ? 0 : q->alfa > 1 ? 1 : q->alfa) * 255); r->_r = 0;
#undef Q16
}
/* tablas de un fichero parcial (los *meta primeros bytes) leidas a trozos de 16 KB y con las piezas ya a 24 bytes: la
   RAM que piden es la del resultado (antes un bloque con las piezas de 40 bytes y luego se encogia: HAHUD.DAT pedia
   208 KB seguidos al empezar la carrera y no los habia, "Out of memory" y el modo sin medidor ni rotulos) */
static uint8_t *tab_en; static uint32_t tab_cap; static int tab_usado;   /* hudd_tablas_en */
void hudd_tablas_en(void *p, uint32_t cap) { tab_en = p; tab_cap = p ? cap : 0; tab_usado = 0; }
static uint8_t *lee_tablas(uint32_t lba, uint32_t *meta, h_pc_t ***gp)
{
    enum { TROZO = 8 };
    uint8_t *t = memalign(32, TROZO * 2048), *d = NULL, pz_[sizeof(h_pie_t)];
    uint32_t c[12], pos = 0, n = *meta, np, ini, finp, quita, sec = 0, k, j, npz = 0, ip = 0, g = 0, ng;
    h_pc_t **G = NULL; const h_grp_t *gr = NULL;
    *gp = NULL;
    if (!t || lee_sect(lba, 1, t)) { free(t); return NULL; }
    memcpy(c, t, sizeof c); np = c[4]; ini = c[9]; finp = ini + np * sizeof(h_pie_t); ng = c[3];
    quita = np * sizeof(h_pie_t);
    if (c[10] < finp || n < finp || c[8] + ng * sizeof(h_grp_t) > ini) quita = 0;   /* otra disposicion: tal cual */
    if (tab_en && n - quita <= tab_cap) { d = tab_en; tab_usado = 1; }
    else d = memalign(32, n - quita);
    tab_en = NULL; tab_cap = 0;
    if (!d || (quita && !(G = calloc(ng, sizeof *G)))) { free(t); if (!tab_usado) free(d); return NULL; }
    while (pos < n) {
        k = (n - pos + 2047) / 2048; if (k > TROZO) k = TROZO;
        if (lee_sect(lba + sec, k, t)) goto mal;
        for (j = 0; j < k * 2048 && pos < n; j++, pos++) {
            uint8_t b = t[j];
            if (!quita || pos < ini) d[pos] = b;
            else if (pos < finp) {
                pz_[npz++] = b;
                if (npz == sizeof(h_pie_t)) {           /* pieza ip: a su grupo (los grupos van en orden de pri) */
                    h_pie_t q; memcpy(&q, pz_, sizeof q); npz = 0;
                    if (!gr) gr = (const h_grp_t *)(d + c[8]);
                    while (g < ng && ip >= (uint32_t)gr[g].pri + gr[g].np) g++;
                    if (g < ng && ip >= gr[g].pri) {
                        if (!G[g] && !(G[g] = malloc(gr[g].np * sizeof(h_pc_t)))) goto mal;
                        pieza_compacta(&q, G[g] + (ip - gr[g].pri));
                    }
                    ip++;
                }
            } else d[pos - quita] = b;
        }
        sec += k;
    }
    free(t);
    if (quita) { uint32_t *cc = (uint32_t *)d; cc[10] -= quita; cc[11] -= quita; }
    *meta = n - quita; *gp = G;
    return d;
mal:
    if (G) { for (k = 0; k < ng; k++) free(G[k]); free(G); }
    free(t); free(d); return NULL;
}

hud_dat_t *hudd_abre(const char *fich, int modelo, int parcial)
{
    uint32_t sz, *c, i, j; hud_dat_t *h = calloc(1, sizeof *h);
    if (!h) return NULL;
    if (!parcial) {
        if (!(h->dat = gd_carga(fich, &sz))) { free(h); return NULL; }
    } else {                                                /* cabecera y tabla de texturas; luego hasta la 1a textura */
        uint8_t *b = memalign(32, 2048); uint32_t fin, ns, m = 0xffffffffu;
        if (!b || gd_busca(fich, &h->lba, &sz) || lee_sect(h->lba, 1, b)) { free(b); free(h); return NULL; }
        h->tam = sz;
        c = (uint32_t *)b; fin = c[6] + 16 * c[1]; ns = (fin + 2047) / 2048;
        if (ns > 1) { free(b); b = memalign(32, ns * 2048); if (!b || lee_sect(h->lba, ns, b)) { free(b); free(h); return NULL; } }
        c = (uint32_t *)b;
        for (i = 0; i < c[1]; i++) { h_tex_t *t = (h_tex_t *)(b + c[6]) + i; if (t->off < m) m = t->off; }
        free(b);
        h->meta = m;
        if (!(h->dat = lee_tablas(h->lba, &h->meta, &h->gp))) { free(h); return NULL; }
        h->prestado = tab_usado; tab_usado = 0;
    }
    if (memcmp(h->dat, "HUD1", 4)) { if (h->gp) { for (i = 0; i < ((uint32_t *)h->dat)[3]; i++) free(h->gp[i]); free(h->gp); } if (!h->prestado) free(h->dat); free(h); return NULL; }
    c = (uint32_t *)h->dat;
    h->n_tex = c[1]; h->n_grp = c[3]; h->n_uso = c[5];
    h->tex = (h_tex_t *)(h->dat + c[6]); h->spr = (h_spr_t *)(h->dat + c[7]); h->grp = (h_grp_t *)(h->dat + c[8]);
    {   /* piezas a 24 bytes en su sitio; en un fichero parcial (solo tablas en la RAM) lo de detras se baja y el bloque
           se encoge */
        const h_pie_t *f = (const h_pie_t *)(h->dat + c[9]); h_pc_t *o = (h_pc_t *)(h->dat + c[9]);
        uint32_t np = c[4];
        if (!h->gp) for (i = 0; i < np; i++) { h_pie_t q = f[i]; pieza_compacta(&q, o + i); }   /* o[i] no pisa f[i + 1] */
    }
    h->pie = h->gp ? NULL : (h_pc_t *)(h->dat + c[9]); h->uso = (h_uso_t *)(h->dat + c[10]); h->off_taco = c[11];
    h->taco = h->dat[c[11] + (modelo & 15)]; if (h->taco == 0xff) h->taco = -1;
    H = h;
    for (i = 0, h->g_fuera = -1; i < h->n_grp; i++) if (!strncmp(h->grp[i].nombre, "DIG_PUNTOS_FUERA", 16)) h->g_fuera = i;
    h->vtex = calloc(h->n_tex, sizeof *h->vtex); h->usada = calloc(h->n_tex, 1); h->dibujada = calloc(h->n_tex, 4);
    h->cab = memalign(32, h->n_tex * sizeof *h->cab); h->cabf = memalign(32, h->n_tex * sizeof *h->cabf);
    h->caba = memalign(32, h->n_tex * sizeof *h->caba);
    for (i = 0; i < h->n_grp; i++)
        if (grupo_util(i)) for (j = 0; j < h->grp[i].np; j++) h->usada[h->spr[pz(h, i)[j].spr].tex] = 1;
    if (!h->n_grp) memset(h->usada, 1, h->n_tex);           /* sin grupos (bancos SPRANI): todas se pueden subir */
    return h;
}

/* ---- paletas (solo el HUD de carrera, HUD.DAT; docs/DREAMCAST.md, Memoria de video)
   Casi todas las texturas ARGB4444 del HUD vienen de texturas de PS2 con paleta (CLUT de 16 o 256 colores) y tienen
   16 colores distintos o menos (paneles, cifras, marchas, mapa de ruta) o 256 o menos (cifras del tiempo, tacometros,
   rebufo, avisos de bifurcacion y tiempo extra). Esas se suben con indices de 4 u 8 bits y una paleta ARGB4444 de la
   PowerVR con exactamente sus colores: cada texel da el mismo valor de 16 bits que antes (sin perdida; la imagen es la
   misma) y ocupan 1/4 o 1/2. Las demas (la de los avisos de la salida y la meta, 576 colores; las ARGB1555 de la
   velocidad, que no caben exactas en una paleta 4444; las VQ) se quedan como estaban.
   La PowerVR tiene una tabla de 1024 entradas y un formato para todas (aqui ARGB4444; nadie mas usa paletas): las de
   8 bits usan un banco de 256 (4 bancos) y las de 4 bits uno de 16 (64). pal_reparte llena los bancos de 8 bits por
   orden de colores (las que caben juntas en 256 comparten banco: los tacometros, las cifras del tiempo) y pone las de
   4 bits en las entradas que sobran al final de los bancos (o con otra de 4 bits si caben juntas en 16); la que no
   cabe se queda en 16 bits. Los colores de cada banco van ordenados: el indice de un texel es su posicion (pal_idx). */
static uint16_t pal_col[1024];                              /* la tabla de la PowerVR (colores ordenados por banco) */
static uint16_t pal_n8[4];                                  /* entradas usadas al principio de cada banco de 8 bits */
static uint8_t pal_n4[64];                                  /* entradas usadas de cada banco de 4 bits (0: libre) */
#define PAL_4 0x4000                                        /* pal[]: banco de 4 bits (si no, de 8); bits 0-9: entrada */

/* colores distintos de la textura i (ordenados) en c[] si son <= 256; devuelve cuantos (257: mas) */
static int pal_colores(const hud_dat_t *h, int i, uint16_t *c)
{
    static uint32_t visto[2048];                            /* 65536 bits */
    const uint16_t *t = (const uint16_t *)((h->meta ? h->tx : h->dat) + h->tex[i].off); uint32_t k, n = 0, m = h->tex[i].w * h->tex[i].h;
    for (k = 0; k < m && n <= 256; k++) {
        uint16_t v = t[k];
        if (!(visto[v >> 5] & (1u << (v & 31)))) { visto[v >> 5] |= 1u << (v & 31); if (n < 256) c[n] = v; n++; }
    }
    for (k = 0; k < n && k < 256; k++) visto[c[k] >> 5] &= ~(1u << (c[k] & 31));
    if (n > 256) { memset(visto, 0, sizeof visto); return 257; }
    for (k = 1; k < n; k++) { uint16_t v = c[k]; int j = k; while (j > 0 && c[j - 1] > v) { c[j] = c[j - 1]; j--; } c[j] = v; }
    return n;
}
/* union ordenada de a (na) y b (nb) en o (hasta max; devuelve el tamano, max + 1 si no cabe) */
static int pal_une(const uint16_t *a, int na, const uint16_t *b, int nb, uint16_t *o, int max)
{
    int i = 0, j = 0, n = 0;
    while (i < na || j < nb) {
        uint16_t v = j >= nb || (i < na && a[i] <= b[j]) ? a[i] : b[j];
        if (i < na && a[i] == v) i++;
        if (j < nb && b[j] == v) j++;
        if (n == max) return max + 1;
        o[n++] = v;
    }
    return n;
}
static uint32_t tam_vram(const hud_dat_t *h, int i);
static void pal_reparte(hud_dat_t *h)
{
    uint16_t *col, *nc, tmp[256]; int16_t *orden; uint32_t i, j, n = h->n_tex; int k, b, s;
    h->pal = malloc(n * sizeof *h->pal); h->con_pal = calloc(n, 1);
    col = malloc(n * 256 * sizeof *col); nc = malloc(n * sizeof *nc); orden = malloc(n * sizeof *orden);
    if (!h->pal || !h->con_pal || !col || !nc || !orden) {
        free(h->pal); free(h->con_pal); h->pal = NULL; h->con_pal = NULL; free(col); free(nc); free(orden); return;
    }
    memset(pal_n8, 0, sizeof pal_n8); memset(pal_n4, 0, sizeof pal_n4);
    for (i = 0; i < n; i++) {                               /* solo ARGB4444 sin VQ (las 1555 no caben exactas) */
        h->pal[i] = -1;
        nc[i] = h->tex[i].fmt == 2 && h->tex[i].w >= 8 && h->tex[i].h >= 8 ? pal_colores(h, i, col + 256 * i) : 257;
        orden[i] = i;
    }
    for (i = 1; i < n; i++) { int16_t v = orden[i]; j = i; while (j > 0 && nc[orden[j - 1]] < nc[v]) { orden[j] = orden[j - 1]; j--; } orden[j] = v; }
    for (j = 0; j < n; j++) {                               /* 8 bits, de mas a menos colores: el primer banco que vale */
        i = orden[j];
        if (nc[i] <= 16 || nc[i] > 256) continue;
        for (b = 0; b < 4; b++) {
            k = pal_une(pal_col + 256 * b, pal_n8[b], col + 256 * i, nc[i], tmp, 256);
            if (k <= 256) { memcpy(pal_col + 256 * b, tmp, k * 2); pal_n8[b] = k; h->pal[i] = 256 * b; break; }
        }
    }
    for (j = 0; j < n; j++) {                               /* 4 bits: con otra si caben, si no en un banco libre */
        i = orden[j];
        if (nc[i] > 16) continue;
        for (s = 63; s >= 0; s--) {
            if (!pal_n4[s]) continue;
            k = pal_une(pal_col + 16 * s, pal_n4[s], col + 256 * i, nc[i], tmp, 16);
            if (k <= 16) { memcpy(pal_col + 16 * s, tmp, k * 2); pal_n4[s] = k; h->pal[i] = PAL_4 | 16 * s; break; }
        }
        if (h->pal[i] >= 0) continue;
        for (s = 63; s >= 0; s--)                           /* libre: ni de 4 bits ni dentro de lo usado de su banco */
            if (!pal_n4[s] && 16 * s >= 256 * (s / 16) + pal_n8[s / 16]) {
                memcpy(pal_col + 16 * s, col + 256 * i, nc[i] * 2); pal_n4[s] = nc[i] ? nc[i] : 1; h->pal[i] = PAL_4 | 16 * s; break;
            }
        if (h->pal[i] >= 0) continue;
        for (b = 0; b < 4; b++) {                           /* sin banco de 4 bits: en uno de 8 que no pise los de 4 */
            k = pal_une(pal_col + 256 * b, pal_n8[b], col + 256 * i, nc[i], tmp, 256);
            for (s = 16 * b; k <= 256 && s < 16 * b + 16; s++) if (pal_n4[s] && 16 * s < 256 * b + k) k = 257;
            if (k <= 256) { memcpy(pal_col + 256 * b, tmp, k * 2); pal_n8[b] = k; h->pal[i] = 256 * b; break; }
        }
    }
    pvr_set_pal_format(PVR_PAL_ARGB4444);
    for (i = 0; i < 1024; i++) pvr_set_pal_entry(i, pal_col[i]);
    {
        uint32_t ahorro = 0; int n8 = 0, n4 = 0;
        for (i = 0; i < n; i++) if (h->pal[i] >= 0 && h->usada[i]) ahorro += h->tex[i].tam - tam_vram(h, i);
        for (b = 0; b < 4; b++) n8 += pal_n8[b] > 0;
        for (s = 0; s < 64; s++) n4 += pal_n4[s] > 0;
        dbglog(DBG_INFO, "HUD: paletas en %d bancos de 8 bits y %d de 4, %uK menos de VRAM\n", n8, n4, (unsigned)(ahorro >> 10));
    }
    free(col); free(nc); free(orden);
}
/* indice del texel v en el banco de la textura (busqueda en sus colores ordenados) */
static inline uint32_t pal_idx(int16_t p, uint16_t v)
{
    const uint16_t *c = pal_col + (p & 1023); int lo = 0, hi = (p & PAL_4) ? pal_n4[(p & 1023) >> 4] : pal_n8[(p & 1023) >> 8];
    while (lo < hi) { int m = (lo + hi) >> 1; if (c[m] < v) lo = m + 1; else hi = m; }
    return lo;
}
/* bytes de la textura i en la VRAM (con su paleta: 1/2 o 1/4) */
static uint32_t tam_vram(const hud_dat_t *h, int i)
{
    if (!h->pal || h->pal[i] < 0) return h->tex[i].tam;
    return ((h->tex[i].w * h->tex[i].h >> ((h->pal[i] & PAL_4) ? 1 : 0)) + 31) & ~31u;
}
/* sube la textura i (los texeles en d) a v: con su paleta si la tiene y hay RAM para los indices; 1 con paleta.
   temporal: d es un bufer que se tira despues (los sectores leidos del GD): los indices se escriben encima de los
   texeles (1 byte o medio por cada 2 leidos, hacia delante) y no piden RAM */
static int sube_tex(hud_dat_t *h, int i, uint8_t *d, pvr_ptr_t v, int temporal)
{
    int16_t p = h->pal ? h->pal[i] : -1; uint32_t k, m = h->tex[i].w * h->tex[i].h, tam = tam_vram(h, i); uint8_t *o;
    const uint16_t *t = (const uint16_t *)d;
    if (p < 0 || !(o = temporal ? d : toma(tam))) { pvr_txr_load(d, v, h->tex[i].tam); return 0; }
    if (p & PAL_4) for (k = 0; k < m; k += 2) o[k >> 1] = pal_idx(p, t[k]) | pal_idx(p, t[k + 1]) << 4;   /* par: bajo */
    else for (k = 0; k < m; k++) o[k] = pal_idx(p, t[k]);  /* (twiddled: el mismo orden de texeles que la de 16 bits) */
    pvr_txr_load(o, v, tam);
    if (!temporal) deja(o);
    return 1;
}

static void cabeceras(hud_dat_t *h, int i)
{
    static const uint32_t FMT[3] = { PVR_TXRFMT_ARGB1555, PVR_TXRFMT_RGB565, PVR_TXRFMT_ARGB4444 };
    pvr_poly_cxt_t cx;
    /* formato: bits 0-7 el de 16 bits; bit 8 VQ (libro de 256 entradas al principio de los datos) */
    uint32_t f = FMT[(h->tex[i].fmt & 0xff) < 3 ? (h->tex[i].fmt & 0xff) : 2] | PVR_TXRFMT_TWIDDLED;
    if (h->tex[i].fmt & 0x100) f |= PVR_TXRFMT_VQ_ENABLE;
    if (h->con_pal && h->con_pal[i]) {                      /* con paleta (pal_reparte): siempre twiddled */
        int16_t p = h->pal[i];
        f = (p & PAL_4) ? PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL((p & 1023) >> 4) : PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL((p & 1023) >> 8);
    }
    pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, f, h->tex[i].w, h->tex[i].h, h->vtex[i], PVR_FILTER_BILINEAR);
    cx.txr.env = PVR_TXRENV_MODULATEALPHA; cx.depth.write = PVR_DEPTHWRITE_DISABLE;
    cx.depth.comparison = PVR_DEPTHCMP_GREATER; pvr_poly_compile(h->cabf + i, &cx);
    cx.depth.comparison = PVR_DEPTHCMP_ALWAYS; pvr_poly_compile(h->cab + i, &cx);
    /* capas con el bit 4 de las banderas de la capa SPRANI (+0x48: los resplandores de fondo negro de GAME_CVT; la pieza
       lo lleva en flags & 4): se suman a lo de detras */
    cx.blend.src = PVR_BLEND_SRCALPHA; cx.blend.dst = PVR_BLEND_ONE; pvr_poly_compile(h->caba + i, &cx);
}

int hudd_sube(hud_dat_t *h, int t0, int nt)
{
    int i, fin; uint32_t lo = 0xffffffffu, hi = 0, s0 = 0, ns; uint8_t *b = NULL;
    if (!h) return -1;
    if (nt < 0) nt = h->n_tex - t0;
    fin = t0 + nt; if (fin > (int)h->n_tex) fin = h->n_tex;
    for (i = t0; i < fin; i++) if (h->usada[i] && !h->vtex[i]) {
        if (h->tex[i].off < lo) lo = h->tex[i].off;
        if (h->tex[i].off + h->tex[i].tam > hi) hi = h->tex[i].off + h->tex[i].tam;
    }
    if (hi <= lo) return 0;
    if (h->meta && !h->tx) {                                /* del GD: los sectores que cubren esas texturas */
        s0 = lo / 2048; ns = (hi + 2047) / 2048 - s0;
        if (!(b = toma(ns * 2048))) return -3;
        if (lee_sect(h->lba + s0, ns, b)) { deja(b); return -4; }
    }
    for (i = t0; i < fin; i++) {
        if (!h->usada[i] || h->vtex[i]) continue;
        if (!(h->vtex[i] = pvr_mem_malloc(tam_vram(h, i)))) { deja(b); hudd_baja(h, t0, nt); return -2; }
        {   uint8_t *d = h->tx ? h->tx + h->tex[i].off : h->meta ? b + (h->tex[i].off - s0 * 2048) : h->dat + h->tex[i].off;
            if (h->con_pal) h->con_pal[i] = sube_tex(h, i, d, h->vtex[i], h->meta != 0);
            else pvr_txr_load(d, h->vtex[i], h->tex[i].tam);
        }
        cabeceras(h, i);
    }
    deja(b);
    return 0;
}

void hudd_baja(hud_dat_t *h, int t0, int nt)
{
    int i, fin;
    if (!h) return;
    if (nt < 0) nt = h->n_tex - t0;
    fin = t0 + nt; if (fin > (int)h->n_tex) fin = h->n_tex;
    for (i = t0; i < fin; i++) if (h->vtex[i]) { libera_vram(h->vtex[i], h->dibujada[i]); h->vtex[i] = NULL; }
}

/* Piezas por grupo de un fichero parcial (MENU.DAT: 724 KB en 57 bloques). hudd_grupos_suelta libera las de los grupos
   que no dibujan ninguna textura de [t0, t0 + nt) (en la carrera el menu solo dibuja la pausa: su pantalla) y
   devuelve los bytes; hudd_grupos_carga vuelve a leer del GD las que faltan (al volver al menu; 0 bien y sin leer
   nada si no falta ninguna). Un grupo sin sus piezas no se dibuja (hudd_dibuja) */
uint32_t hudd_grupos_suelta(hud_dat_t *h, int t0, int nt)
{
    uint32_t g, j, b = 0;
    if (!h || !h->gp) return 0;
    for (g = 0; g < h->n_grp; g++) {
        int vale = 0;
        if (!h->gp[g]) continue;
        for (j = 0; j < h->grp[g].np && !vale; j++) { int t = h->spr[h->gp[g][j].spr].tex; vale = t >= t0 && t < t0 + nt; }
        if (!vale) { free(h->gp[g]); h->gp[g] = NULL; b += h->grp[g].np * sizeof(h_pc_t); }
    }
    return b;
}
int hudd_grupos_carga(hud_dat_t *h)
{
    enum { TROZO = 8 };
    uint32_t ini, fin, pos, sec, k, j, g = 0, ip = 0, npz = 0, falta = 0; uint8_t *t, *nuevo, pz_[sizeof(h_pie_t)];
    if (!h || !h->gp) return 0;
    for (k = 0; k < h->n_grp; k++) if (!h->gp[k] && h->grp[k].np) falta++;
    if (!falta) return 0;
    if (!(nuevo = calloc(h->n_grp, 1))) return -1;
    for (k = 0; k < h->n_grp; k++)
        if (!h->gp[k] && h->grp[k].np) {
            if (!(h->gp[k] = malloc(h->grp[k].np * sizeof(h_pc_t)))) {
                for (j = 0; j < k; j++) if (nuevo[j]) { free(h->gp[j]); h->gp[j] = NULL; }
                free(nuevo); return -1;
            }
            nuevo[k] = 1;
        }
    ini = ((const uint32_t *)h->dat)[9]; fin = ini + ((const uint32_t *)h->dat)[4] * sizeof(h_pie_t);
    sec = ini / 2048; pos = sec * 2048;
    if (!(t = memalign(32, TROZO * 2048))) goto mal;
    while (pos < fin) {
        k = (fin - pos + 2047) / 2048; if (k > TROZO) k = TROZO;
        if (lee_sect(h->lba + sec, k, t)) { free(t); goto mal; }
        for (j = 0; j < k * 2048 && pos < fin; j++, pos++) {
            if (pos < ini) continue;
            pz_[npz++] = t[j];
            if (npz == sizeof(h_pie_t)) {                   /* pieza ip: a su grupo (en orden de pri, como lee_tablas) */
                h_pie_t q; memcpy(&q, pz_, sizeof q); npz = 0;
                while (g < h->n_grp && ip >= (uint32_t)h->grp[g].pri + h->grp[g].np) g++;
                if (g < h->n_grp && ip >= h->grp[g].pri && nuevo[g]) pieza_compacta(&q, h->gp[g] + (ip - h->grp[g].pri));
                ip++;
            }
        }
        sec += k;
    }
    free(t); free(nuevo);
    return 0;
mal:
    for (k = 0; k < h->n_grp; k++) if (nuevo[k]) { free(h->gp[k]); h->gp[k] = NULL; }
    free(nuevo);
    return -2;
}

/* cierra un fichero de hudd_abre: su VRAM (con la espera de hudd_baja) y su RAM */
void hudd_cierra(hud_dat_t *h)
{
    if (!h || h == hud_principal) return;
    hudd_baja(h, 0, -1); hudd_vacia();
    if (H == h) H = NULL;
    if (h->gp) { uint32_t k; for (k = 0; k < h->n_grp; k++) free(h->gp[k]); free(h->gp); }
    if (!h->prestado) free(h->dat);
    free(h->vtex); free(h->usada); free(h->dibujada); free(h->cab); free(h->cabf); free(h->caba);
    free(h->pal); free(h->con_pal); free(h->falta); free(h);
}

int hud_carga(const char *fich, int modelo)
{
    /* En la RAM solo las tablas (48 KB de los 1182 de HUD.DAT): los texeles se leen del GD al subirlos (hudd_sube).
       Aqui, una vez, el fichero entero en el bufer prestado (hudd_bufer; si no, del monton y se libera) para contar
       los colores de cada textura (pal_reparte) y subirlas */
    hud_dat_t *h; uint32_t ns; int r;
    if (!(h = hud_principal = hudd_abre(fich, modelo, 1))) return -1;
    ns = (h->tam + 2047) / 2048;
    if (!(h->tx = toma(ns * 2048))) return -1;
    if (lee_sect(h->lba, ns, h->tx)) { deja(h->tx); h->tx = NULL; return -1; }
    pal_reparte(h);
    h->falta = calloc(h->n_tex, 1);
    r = hudd_sube(h, 0, -1);
    deja(h->tx); h->tx = NULL;
    return r ? -2 : 0;
}
void hud_baja_tex(void) { hudd_baja(hud_principal, 0, -1); }
/* en un apuro de VRAM (demo/main.c vram_apuro): echa ya las texturas del HUD de carrera que no se han dibujado en los
   ultimos `cuadros` cuadros (los avisos de la salida, la meta, el tiempo extra...; la PowerVR ya no las lee) y vuelven
   a subirse solas el cuadro en que se quieran dibujar (si caben; si no, ese aviso no se ve). Devuelve los bytes */
uint32_t hud_suelta_quietas(uint32_t cuadros)
{
    hud_dat_t *h = hud_principal; uint32_t i, b = 0;
    if (!h) return 0;
    for (i = 0; i < h->n_tex; i++)
        if (h->vtex[i] && cuadro_hud - h->dibujada[i] > cuadros) { pvr_mem_free(h->vtex[i]); h->vtex[i] = NULL; b += tam_vram(h, i); }
    return b;
}
void hud_pon_modelo(int modelo)
{
    hud_dat_t *h = hud_principal; uint32_t i, j;
    if (!h) return;
    H = h; h->taco = h->dat[h->off_taco + (modelo & 15)]; if (h->taco == 0xff) h->taco = -1;
    memset(h->usada, 0, h->n_tex);                          /* texturas que se suben: las del tacometro nuevo */
    for (i = 0; i < h->n_grp; i++)
        if (grupo_util(i)) for (j = 0; j < h->grp[i].np; j++) h->usada[h->spr[pz(h, i)[j].spr].tex] = 1;
}
int hud_sube_tex(void) { return hud_principal ? hudd_sube(hud_principal, 0, -1) : -1; }

static void pieza(const h_pc_t *p, const h_grp_t *g, int cuadro, float X, float Y, float alfa)
{
    const h_spr_t *s = H->spr + p->spr;
    float ang = p->ang * (1.0f / 64), c, si, px[4], py[4], a = p->alfa * (1.0f / 255) * alfa; int k; pvr_vertex_t v;
    const float sx = p->sx * (1.0f / 512), sy = p->sy * (1.0f / 512), x0 = p->x * 0.125f, y0 = p->y * 0.125f;
    if (a <= 0.004f) return;
    if (!H->vtex[s->tex]) { if (H->falta && H->usada[s->tex]) H->falta[s->tex] = 1; return; }   /* hud_suelta_quietas */
    if (p->flags & 1) ang += g->paso * cuadro;
    ang *= (float)M_PI / 180.0f; c = cosf(ang); si = sinf(ang);
    for (k = 0; k < 4; k++) {
        float qx = ((k & 1) ? s->w : 0) - p->ax * 0.125f, qy = ((k & 2) ? s->h : 0) - p->ay * 0.125f;
        qx *= sx; qy *= sy;
        px[k] = qx * c - qy * si + x0 + X; py[k] = qx * si + qy * c + y0 + Y;
    }
    {   int clave = s->tex | ((p->flags & 4) << 14);
        if (clave != ult_tex) {
            pvr_prim(((p->flags & 4) ? H->caba : modo_fondo ? H->cabf : H->cab) + s->tex, sizeof(pvr_poly_hdr_t)); ult_tex = clave;
        }
    }
    H->dibujada[s->tex] = cuadro_hud;
    if (a > 1) a = 1;
    v.argb = ((uint32_t)(a * 255) << 24) | tinte; v.oargb = 0; v.z = zcapa; zcapa += zpaso;
    for (k = 0; k < 4; k++) {
        v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = px[k]; v.y = py[k]; v.u = (k & 1) ? s->u1 : s->u0; v.v = (k & 2) ? s->v1 : s->v0;
        pvr_prim(&v, sizeof v);
    }
}

static void grupo(int g, int cuadro, float X, float Y, float alfa)
{
    const h_grp_t *G = H->grp + g; const h_pc_t *P = pz(H, g); int j;
    if (!P) return;                                         /* sus piezas, sueltas (hudd_grupos_suelta) */
    for (j = 0; j < G->np; j++)
        if (G->n <= 1 || P[j].cuadro == cuadro ||
            (cuadro > P[j].cuadro && cuadro < P[j].cuadro + (P[j].flags >> 4)))   /* MENU.DAT: vale flags >> 4 cuadros */
            pieza(P + j, G, cuadro, X, Y, alfa);
}

static void dibuja(const char *nombre, int alt, int cuadro, float alfa)
{
    int u = busca_uso(nombre); int g;
    if (u < 0) return;
    g = alt && H->uso[u].alt != 0xffff ? H->uso[u].alt : H->uso[u].grp;
    grupo(g, cuadro, H->uso[u].x, H->uso[u].y, alfa);
}

/* cifras de un uso con el grupo g en (x, y): un caracter por cifra (' ' solo avanza) */
static void cifras(int u, int g, const char *txt, float x, float y, float alfa)
{
    int i;
    for (i = 0; txt[i]; i++)
        if (txt[i] >= '0' && txt[i] <= '9') grupo(g, txt[i] - '0', x + i * H->uso[u].avance, y, alfa);
}
/* numero: en x (o en la del uso si x < -9000) */
static void numero(const char *nombre, int alt, const char *txt, float x)
{
    int u = busca_uso(nombre), g;
    if (u < 0) return;
    g = alt && H->uso[u].alt != 0xffff ? H->uso[u].alt : H->uso[u].grp;
    if (x < -9000) x = H->uso[u].x;
    cifras(u, g, txt, x, H->uso[u].y, 1);
}
/* grupo de las cifras de la puntuacion por estilo (0 normal, 1 rebufo = alternativo, 2 fuera) */
static int grupo_puntos(int u, int estilo)
{
    if (estilo == 2 && H->g_fuera >= 0) return H->g_fuera;
    if (estilo == 1 && H->uso[u].alt != 0xffff) return H->uso[u].alt;
    return H->uso[u].grp;
}

/* ---- acceso generico */
int hudd_uso(const hud_dat_t *h, const char *nombre)
{
    uint32_t i;
    if (!h) return -1;
    for (i = 0; i < h->n_uso; i++) if (!strncmp(h->uso[i].nombre, nombre, 16)) return i;
    return -1;
}
int hudd_cuadros(const hud_dat_t *h, int u) { return h && u >= 0 ? h->grp[h->uso[u].grp].n : 0; }
/* texturas que usa el grupo de un uso: primera y cuantas (las de un atlas propio van seguidas) */
int hudd_tex_uso(const hud_dat_t *h, int u, int *t0)
{
    int j, lo = 1 << 30, hi = -1; const h_grp_t *g;
    if (!h || u < 0 || u >= (int)h->n_uso) return 0;
    g = h->grp + h->uso[u].grp;
    if (!pz(h, h->uso[u].grp)) return 0;
    for (j = 0; j < g->np; j++) { int t = h->spr[pz(h, h->uso[u].grp)[j].spr].tex; if (t < lo) lo = t; if (t > hi) hi = t; }
    if (hi < 0) return 0;
    *t0 = lo; return hi - lo + 1;
}
const uint8_t *hudd_extra(const hud_dat_t *h) { return h ? h->dat + h->off_taco + 16 : NULL; }

void hudd_dibuja(hud_dat_t *h, int u, int cuadro, float dx, float dy, float alfa, float z, int fondo)
{
    int n;
    if (!h || u < 0 || u >= (int)h->n_uso || alfa <= 0.004f) return;
    H = h; ult_tex = -1; modo_fondo = fondo;
    zcapa = z; zpaso = fondo ? 0.000001f : 0.001f;
    n = h->grp[h->uso[u].grp].n;
    if (cuadro < 0) cuadro = 0;
    if (n > 1 && cuadro >= n) cuadro = n - 1;
    grupo(h->uso[u].grp, cuadro, h->uso[u].x + dx, h->uso[u].y + dy, alfa);
    modo_fondo = 0; zpaso = 0.001f;
}

/* Sprites sueltos (comun/sprani.c): hudd_capa fija el fichero, la z de partida y el modo (fondo = 1: detras de la
   escena 3D, como hudd_dibuja); hudd_sprite dibuja el sprite s en el rectangulo (q0, q1)-(q2, q3) de su espacio con
   la matriz afin m (por filas: x' = x m0 + y m2 + m4, y' = x m1 + y m3 + m5), alfa y suma (aditiva) */
void hudd_capa(hud_dat_t *h, float z, int fondo)
{
    H = h; ult_tex = -1; modo_fondo = fondo; zcapa = z; zpaso = fondo ? 0.000001f : 0.001f;
}
float hudd_z(void) { return zcapa; }
int hudd_sprite_tex(const hud_dat_t *h, int s) { return h && s >= 0 ? h->spr[s].tex : -1; }
void hudd_sprite(int s, const float *m, const float *q, float alfa, int aditiva)
{
    const h_spr_t *sp; int k; pvr_vertex_t v;
    if (!H || alfa <= 0.004f) return;
    sp = H->spr + s;
    if (!H->vtex[sp->tex]) return;
    {   int clave = sp->tex | (aditiva ? 0x10000 : 0);
        if (clave != ult_tex) {
            pvr_prim((aditiva ? H->caba : modo_fondo ? H->cabf : H->cab) + sp->tex, sizeof(pvr_poly_hdr_t)); ult_tex = clave;
        }
    }
    H->dibujada[sp->tex] = cuadro_hud;
    if (alfa > 1) alfa = 1;
    v.argb = ((uint32_t)(alfa * 255) << 24) | tinte; v.oargb = 0; v.z = zcapa; zcapa += zpaso;
    for (k = 0; k < 4; k++) {
        float x = (k & 1) ? q[2] : q[0], y = (k & 2) ? q[3] : q[1];
        v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = x * m[0] + y * m[2] + m[4]; v.y = x * m[1] + y * m[3] + m[5];
        v.u = (k & 1) ? sp->u1 : sp->u0; v.v = (k & 2) ? sp->v1 : sp->v0;
        pvr_prim(&v, sizeof v);
    }
}
int hudd_residente(const hud_dat_t *h, int t) { return h && h->vtex[t] != NULL; }

/* dentro de la lista TR, despues de todo lo demas (z por encima de la escena) */
void hud_dibuja(const hud_estado_t *e)
{
    char b[16]; int t, cs, m, cuadro;
    if (!hud_principal) return;
    H = hud_principal; ult_tex = -1; zcapa = 20.0f;
    if (e->solo_puntos) {                                       /* 0x1bd4ac: 2C0029 (capa 3) y 1bf3e8 */
        zcapa = 30.0f; dibuja("PANEL_PUNTOS", 0, 0, 1);
        t = busca_uso("PUNTOS");
        if (t >= 0) { sprintf(b, "%07d", e->puntos > 9999999 ? 9999999 : e->puntos < 0 ? 0 : e->puntos);
                      zcapa = 90.0f; cifras(t, grupo_puntos(t, e->puntos_estilo), b, H->uso[t].x, H->uso[t].y, 1); }
        return;
    }
    /* capa 2: mapa de ruta y tacometro */
    dibuja("RUTA5_FONDO", 0, 0, 1);
    if (H->taco >= 0) {
        cuadro = (int)(e->rpm * 180.0f / 10000.0f); if (cuadro < 0) cuadro = 0; if (cuadro > 179) cuadro = 179;
        grupo(H->taco, cuadro, -219, 127, 1);
    }
    zcapa = 30.0f;
    /* capa 3: paneles y etapas */
    if (!e->infarto) { dibuja("PANEL_TIEMPO", 0, 0, 1); dibuja("PANEL_PUNTOS", 0, 0, 1); }
    for (t = 0; t < e->etapa; t++) dibuja("RUTA5_HECHA", 0, t, 1);
    dibuja("RUTA5_ACTUAL", 0, e->etapa, (e->cuadro & 32) ? 1.0f : 0.4f);
    zcapa = 40.0f;
    /* capa 4: REV (solo con cambio manual) */
    if (e->manual) { dibuja("REV_TEXTO", 0, 0, 1); dibuja("REV_LUZ", 0, 0, 1); if (e->rev_on && (e->cuadro & 8)) dibuja("REV_ON", 0, 0, 1); }
    zcapa = 50.0f;
    /* capa 5: unidades y rebufo */
    dibuja("KMH", e->rebufo, 0, 1);
    if (e->rebufo && (e->cuadro % 30) <= 20) dibuja("REBUFO", 0, 0, 1);
    zcapa = 80.0f;
    dibuja("CAJA", e->seis, 0, 1);
    zcapa = 90.0f;
    m = e->marcha; if (m < 1) m = 1; if (m > 6) m = 6;
    dibuja("MARCHA", 0, m, 1);
    /* capa 9: numeros */
    t = e->tiempo < 0 ? 0 : e->tiempo; if (t > 999) t = 999;
    if (t >= 100) { sprintf(b, "%03d", t); numero("TIEMPO", 0, b, 276); }
    else { sprintf(b, "%02d", t); numero("TIEMPO", 0, b, -9999); }
    cs = e->crono_ms; if (cs > 99 * 60000 + 59999) cs = 99 * 60000 + 59999;
    sprintf(b, "%2d %02d %03d", cs / 60000, (cs / 1000) % 60, cs % 1000); numero("CRONO", 0, b, -9999);
    t = e->infarto ? -1 : busca_uso("PUNTOS");                  /* 4BD480: "%07d", 9999999 como mucho */
    if (t >= 0) { sprintf(b, "%07d", e->puntos > 9999999 ? 9999999 : e->puntos < 0 ? 0 : e->puntos);
                  cifras(t, grupo_puntos(t, e->puntos_estilo), b, H->uso[t].x, H->uso[t].y, 1); }
    t = e->kmh < 0 ? 0 : e->kmh > 999 ? 999 : e->kmh;
    sprintf(b, "%03d", t); numero("VELOCIDAD", e->rebufo, b, -9999);
}

/* avisos animados (cuenta atras, GOAL, EXTEND TIME...): llamar despues de hud_dibuja; dibuja el cuadro dado del uso */
void hud_aviso(const char *nombre, int cuadro, float alfa)
{
    if (!hud_principal) return;
    H = hud_principal;
    if (zcapa < 100.0f) zcapa = 100.0f;
    ult_tex = -1;
    dibuja(nombre, 0, cuadro, alfa);
}

/* 4BA9D0: cifras del estilo del uso en (x, y), avance del uso por caracter (los que no son cifras no se dibujan) */
void hud_cifras(const char *uso, const char *txt, float x, float y, uint32_t rgb)
{
    int u;
    if (!hud_principal) return;
    H = hud_principal;
    if ((u = busca_uso(uso)) < 0) return;
    if (zcapa < 100.0f) zcapa = 100.0f;
    ult_tex = -1; hudd_color(rgb);
    cifras(u, H->uso[u].grp, txt, x, y, 1);
    hudd_color(0xFFFFFF);
}

/* 4BD480: cada mensaje de la cola en (0x1E0, y + 0x49) con "%7d" */
void hud_puntos_msg(int puntos, int estilo, float dy, float alfa)
{
    char b[16]; int u;
    if (!hud_principal) return;
    H = hud_principal;
    if ((u = busca_uso("PUNTOS")) < 0 || alfa <= 0.004f) return;
    if (zcapa < 100.0f) zcapa = 100.0f;
    ult_tex = -1;
    sprintf(b, "%7d", puntos > 9999999 ? 9999999 : puntos < 0 ? 0 : puntos);
    cifras(u, grupo_puntos(u, estilo), b, H->uso[u].x - 1, H->uso[u].y + dy, alfa);
}
