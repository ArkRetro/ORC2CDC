/* Rankings de OUTRUN2SP (ver rksp.h). Como el PS2 (SLES-53998):
   - Tablas en el orden de la RAM del PS2 (0x5d7548..0x5da108): variante 1 presets 0 / 1 (+0, preset x 0x320 + cuenta x
     0xA0), variante 2 (+0x640, igual), variante 0 presets 0 / 1 (+0xC80, preset x 0xC80 + cambio x 0x640 + config x 0x320
     + cuenta x 0xA0), variante 1 presets 2 / 3 (+0x2440 + preset x 0xA0) y variante 0 presets 2 / 3 (+0x21C0 + preset x
     0x280 + cambio x 0x140 + config x 0xA0): los accesos 0x1fb970 / 0x1fb9f0 / 0x1fbad0 y la insercion 0x1fae90 (PC
     4F3810 / 4F3870 / 4F3910 / 4F3010).
   - Por defecto (0x1fa3b0, al arrancar sin datos guardados): las que da esa rutina del PS2 ejecutada en la conversion
     (tools/conv_sp.py tablas_nombre: nombres AM2 / OR2 / R&D, 100000 puntos, 4'50"000 y 14'40"000).
   - Se guardan en la VMU con las licencias (comun/menu.c: el fichero de la consola guarda los datos del sistema con
     ellas, 0x106288 / 0x1063d8 en el estado 7.. de la entrada de nombre). */
#include <string.h>
#include "rksp.h"

static uint8_t tab[RKSP_TAM], tab_uj[RKSP_TAM];
static int hay, hay_uj;

uint8_t *rksp_tablas(void) { return tab; }
int rksp_hay(void) { return hay; }
void rksp_defecto(const uint8_t *def) { if (!hay && def) { memcpy(tab, def, RKSP_TAM); hay = 1; } }
void rksp_pon(const uint8_t *t) { memcpy(tab, t, RKSP_TAM); hay = 1; }

uint32_t rksp_tabla(int v, int p, int c, int a, int b)
{
    if (v == 1) return p == 2 || p == 3 ? 0x2440u + p * 0xa0 : (uint32_t)(p * 0x320 + c * 0xa0);
    if (v == 2) return 0x640u + p * 0x320 + c * 0xa0;
    if (p == 2 || p == 3) return 0x21c0u + p * 0x280 + a * 0x140 + b * 0xa0;
    return 0xc80u + p * 0xc80 + a * 0x640 + b * 0x320 + c * 0xa0;
}

static uint32_t rd(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static int inserta_en(uint8_t *tab, const uint8_t rec[16], int v, int p, int c, int a, int b)
{
    uint32_t t = rksp_tabla(v, p, c, a, b), at;
    if (t + 160 > RKSP_TAM) return -1;
    for (at = 0; at < 10; at++) {                       /* 0x1fb0..: el puesto del registro */
        const uint8_t *e = tab + t + at * 16;
        if (v == 0) { if (rd(rec + 4) < rd(e + 4)) break; }
        else if (v == 1) { if ((rd(rec) & 0xffffff) > (rd(e) & 0xffffff)) break; }
        else if ((rd(rec + 8) & 0xfff) > (rd(e + 8) & 0xfff)) break;
    }
    if (at == 10) return -1;
    memmove(tab + t + (at + 1) * 16, tab + t + at * 16, (9 - at) * 16);   /* los de detras bajan, el decimo se pierde */
    memcpy(tab + t + at * 16, rec, 16);
    return (int)at;
}
int rksp_inserta(const uint8_t rec[16], int v, int p, int c, int a, int b) { return inserta_en(tab, rec, v, p, c, a, b); }

/* Rev 2.9.3: los records de UN JUGADOR de la consola (OUTRUN, HEART ATTACK y TIME ATTACK del frontal). DESVIACION: la
   Dreamcast los guarda con el formato y el orden de las tablas de OUTRUN2SP (las mismas cuentas de rksp_tabla), para
   ensenarlos con el mismo tablero (0x200398), y empiezan vacias (sin nombre; tiempo 0xFFFFFFFF), no con los registros
   por defecto de OUTRUN2SP.
   Rev 2.9.4 (correccion): 0x5f8598 / 0x2270a8 / 0x226df8 / 0x227310 NO son la base local de records sino el cliente de
   los rankings en red (DNAS: "User: %I64u / rating: ..." y DNAS_Init junto a su codigo; 0x2270a8 mete una peticion en
   una cola de 16 entradas de 0x14 bytes que despacha 0x227128; 0x226fc0 acepta los id 0x21..0x74, uno por codigo de la
   Contrarreloj 0x00..0x53): sin portar. Las tablas locales del PS2 son las de \Common\def_orhiscores.bin (cadena
   0x3ef798), que 0x1a18e8 / 0x1a1910 carga en 0x577d44 (PROBABLE: su formato no esta leido). El formato de la DC sigue
   siendo una DESVIACION. */
uint8_t *rkuj_tablas(void) { return tab_uj; }
int rkuj_hay(void) { return hay_uj; }
void rkuj_pon(const uint8_t *t) { memcpy(tab_uj, t, RKSP_TAM); hay_uj = 1; }
static void rkuj_vacia(void)
{
    uint32_t i, z = 0, f = 0xffffffffu;
    for (i = 0; i + 16 <= RKSP_TAM; i += 16) { memcpy(tab_uj + i, &z, 4); memcpy(tab_uj + i + 4, &f, 4); memset(tab_uj + i + 8, 0, 8); }
    hay_uj = 1;
}
int rkuj_inserta(const uint8_t rec[16], int v, int p, int c, int a, int b)
{
    if (!hay_uj) rkuj_vacia();
    return inserta_en(tab_uj, rec, v, p, c, a, b);
}

/* Rev 2.9.4: RKU2. Las 42 tablas que rk_un_jugador (demo/main.c) llega a usar, en este orden: variante 1 presets 0 / 1
   x metas 0..4, variante 2 igual, variante 0 presets 0 / 1 x cambio 0 / 1 x metas 0..4 (configuracion 0) y variante 1
   presets 2 / 3 (los cursos seguidos: rksp_tabla no usa la cuenta). DESVIACION de formato (la VMU: 6720 bytes en lugar
   de las 0x2bc0 de RKUJ) */
static uint32_t rkuj2_tabla(int i)
{
    if (i < 10) return rksp_tabla(1, i / 5, i % 5, 0, 0);
    if (i < 20) return rksp_tabla(2, (i - 10) / 5, i % 5, 0, 0);
    if (i < 40) { i -= 20; return rksp_tabla(0, i / 10, i % 5, (i / 5) & 1, 0); }
    return rksp_tabla(1, 2 + (i - 40), 0, 0, 0);
}
void rkuj_compacta(uint8_t *out)
{
    int i;
    if (!hay_uj) rkuj_vacia();
    for (i = 0; i < RKUJ2_N; i++) memcpy(out + i * 160, tab_uj + rkuj2_tabla(i), 160);
}
int rkuj_expande(const uint8_t *in, uint32_t tam)
{
    int i;
    if (tam < RKUJ2_TAM) return -1;
    rkuj_vacia();
    for (i = 0; i < RKUJ2_N; i++) memcpy(tab_uj + rkuj2_tabla(i), in + i * 160, 160);
    return 0;
}

/* Rev 2.9.4: RKTA, los records de la CONTRARRELOJ por codigo que no son las metas A..E (0x3c..0x45, en rkuj). El PS2
   los tiene en la red (0x226df8: un tablero por codigo, 0x226fc0) y el PC en sus tablas locales de una etapa (0x4d84 de
   frontend_rankings 51D0F0: 10 por codigo, sin separar por cambio; el cambio va en cada fila), metas (0x2804) y cursos
   seguidos (0x9154). DESVIACION: formato propio de 8 bytes por fila (rksp.h) y nombre de 4 caracteres (como rkuj).
   Indice: codigos 0x00..0x3b en el indice = codigo (15 etapas x 2 juegos x 2 sentidos), 0x46..0x4f en 0x3c + (codigo -
   0x46) y 0x50..0x53 en 0x46 + (codigo - 0x50). Insercion con < estricto, como inserta_en de la variante 0 */
#define RKTA_VACIA 0x7fffffu
static uint8_t tab_ta[RKTA_TAM];
static int hay_ta;
int rkta_indice(int c)
{
    if (c >= 0 && c < 0x3c) return c;
    if (c >= 0x46 && c < 0x50) return 0x3c + c - 0x46;
    if (c >= 0x50 && c < 0x54) return 0x46 + c - 0x50;
    return -1;
}
uint8_t *rkta_tablas(void) { return tab_ta; }
int rkta_hay(void) { return hay_ta; }
static void rkta_vacia(void)
{
    uint32_t i, f = RKTA_VACIA;
    for (i = 0; i < RKTA_TAM; i += 8) { memcpy(tab_ta + i, &f, 4); memset(tab_ta + i + 4, 0, 4); }
    hay_ta = 1;
}
int rkta_pon(const uint8_t *t, uint32_t tam)
{
    uint16_t n, f; uint32_t i, j;
    if (tam < 4) return -1;
    memcpy(&n, t, 2); memcpy(&f, t + 2, 2);
    if ((uint64_t)tam < 4u + (uint64_t)n * f * 8) return -1;   /* (en 64 bits: n x f x 8 de un RKTA corrupto daba la
                                                           vuelta en 32 y pasaba, y se leia fuera del bufer) */
    rkta_vacia();                                       /* (otro numero de tablas o de filas: lo que cabe) */
    for (i = 0; i < n && i < RKTA_N; i++)
        for (j = 0; j < f && j < RKTA_FILAS; j++) memcpy(tab_ta + (i * RKTA_FILAS + j) * 8, t + 4 + (i * f + j) * 8, 8);
    return 0;
}
int rkta_bloque(uint8_t *out)
{
    uint16_t n = RKTA_N, f = RKTA_FILAS;
    if (!hay_ta) rkta_vacia();
    memcpy(out, &n, 2); memcpy(out + 2, &f, 2); memcpy(out + 4, tab_ta, RKTA_TAM);
    return RKTA_BLOQUE;
}
int rkta_inserta(int codigo, uint32_t ms, int modelo, int cambio, int bandera, const char *nombre)
{
    int k = rkta_indice(codigo), at, i; uint8_t *t, r[8]; uint32_t w;
    if (k < 0 || !ms) return -1;
    if (ms >= RKTA_VACIA) ms = RKTA_VACIA - 1;          /* (el crono topa en 0x57E3F cuadros: cabe) */
    if (!hay_ta) rkta_vacia();
    t = tab_ta + k * RKTA_FILAS * 8;
    for (at = 0; at < RKTA_FILAS; at++) { memcpy(&w, t + at * 8, 4); if (ms < (w & RKTA_VACIA)) break; }
    if (at == RKTA_FILAS) return -1;
    w = ms | (uint32_t)(modelo & 0xf) << 23 | (uint32_t)(bandera & 0x1f) << 27;
    memcpy(r, &w, 4); memset(r + 4, 0, 4);
    for (i = 0; i < 4 && nombre && nombre[i]; i++) r[4 + i] = (uint8_t)nombre[i] & 0x7f;   /* strncpy 4 */
    r[4] |= (uint8_t)((cambio & 1) << 7);               /* (los nombres son ASCII: el bit 7 queda libre) */
    memmove(t + (at + 1) * 8, t + at * 8, (RKTA_FILAS - 1 - at) * 8);
    memcpy(t + at * 8, r, 8);
    return at;
}
int rkta_fila16(int codigo, uint8_t out[160])
{
    int k = rkta_indice(codigo), i, j; const uint8_t *t;
    if (k < 0) return -1;
    for (i = 0; i < 10; i++) {                          /* vacias: tiempo 0xFFFFFFFF y sin nombre (como rkuj) */
        uint32_t z = 0, f = 0xffffffffu; uint8_t *o = out + 16 * i;
        memcpy(o, &z, 4); memcpy(o + 4, &f, 4); memset(o + 8, 0, 8);
    }
    if (!hay_ta) return 0;
    t = tab_ta + k * RKTA_FILAS * 8;
    for (i = 0; i < RKTA_FILAS && i < 10; i++) {
        uint32_t w, r0, r8; uint8_t *o = out + 16 * i; const uint8_t *q = t + i * 8;
        memcpy(&w, q, 4);
        if ((w & RKTA_VACIA) == RKTA_VACIA || !(q[4] & 0x7f)) continue;
        r0 = (w >> 23 & 0xf) << 28; r8 = (uint32_t)(q[4] >> 7) << 17 | (w >> 27 & 0x1f) << 12;
        w &= RKTA_VACIA;
        memcpy(o, &r0, 4); memcpy(o + 4, &w, 4); memcpy(o + 8, &r8, 4);
        for (j = 0; j < 4 && (q[4 + j] & 0x7f); j++) o[11 + j] = q[4 + j] & 0x7f;
    }
    return 0;
}
