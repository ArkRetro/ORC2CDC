/* OUTRUN2SP: entrada de nombre y rankings (SLES-53998; PC race_name_entry.cpp y sp_rankings.cpp).

   - Modo 26 (tabla de modos 0x3f8e50: 0x1ba968 / 0x1baaf8 / 0x1babe8 = PC 49A960 / 49AA60 / 49AAE0): llega desde los
     resultados (25) con 450240 = 1 (fin del tiempo) o en la variante 0 con un record; temporizador 0 (1b4a10(0)), los
     bancos 0x3D RANKING_CVT, 0x36 NAME_CMN y el del curso (0x37..0x3B NAME5A..E: el del fondo de la meta, tabla de
     saltos 0x3f9800 = el banco de las escenas 0x40d248), y los de sprites 0x12 / 0x14 / 0x15; con todo leido el suceso
     0x189 con la funcion 0x16 (fila 0x15 de la tabla 0x3ee7b4: 0x1fc560 / 0x1fcd30 / 0x1fd1e8 / 0x1fd528) y el 0x17F
     (sonido); cuando acaba (0x1fd1b0: [0x36f4b0] = 1) y vence el temporizador, el modo 27 (GAME OVER). La Dreamcast lee
     los bancos de un tiron (sin la animacion de carga 4B72F0, banco 0x33).
   - 0x1fc560 (PC 4B2370): ruta = 450320 (< 0x10), curso = 44DC50(450560(4)), la escena de fondo [0x40d248 + curso x 4]
     (capa 0, modo 0); el registro de la carrera {4B99D0 | ruta << 24 | 48B140 << 28, 451180(0), corazones (variante 2)
     | 48B180 << 17 | 48B1A0 << 18, nombre 0} se inserta en su tabla (0x1fae90: variante, preset, cuenta = meta de la ruta
     = 0x1ff740(ruta, 4) - 10, cambio, configuracion; sin hacerlo en la variante 0 sin tiempo) y da el puesto (0..9, -1);
     las 10 filas de la tabla (0x2012a8 / 0x2013e8 / 0x201528) y la del jugador (la de su puesto o la de fuera, +0x2D0)
     con el nombre "     " (0x40c838), y la pagina 0 / 5 (puesto / 5 x 5; 5 fuera del ranking).
   - 0x1fcd30 (PC 4B29E0): el primer paso copia el nombre de la licencia (6 bytes, el quinto a 0) a la fila del jugador,
     lo limpia (quita ' ', recorta "0123456789!%&+-./=?@\" por los dos lados; las palabras de 0x36f4b8, sus 4 primeros
     caracteres - 1, sin distinguir mayusculas: "----") y guarda en la tabla el registro del puesto con el nombre de la
     licencia (strncpy 4 en +0xB) y el coche de la licencia (+0x1C & 0x1F en +8 bits 12..16; 0x1fbb20 / 0x1fbbf0 /
     0x1fbd18). Despues: estados 0 -> 2 -> 5, 5: 0xF0 pasos (o START: 1b4548(1)), 7..0xB: guardado de los rankings
     (0x106288 / 0x1063d8: los datos del sistema) y de la licencia (0x1a67e0 / 0x1a6840, si [0x443508] = 0), 0xC: hecho.
     La Dreamcast guarda las licencias y los rankings en el mismo fichero de la VMU (comun/menu.c, comun/rksp.c).
     El sonido 0x158 (4249F0) cuando no suena 0x80A2 solo con el idioma 0 (0x1f6548): con el espanol no suena.
   - 0x1fd1e8 / 0x200398 (PC 4B4F20 / 4B46F0) sin el teclado de letras ([0x424] = 0 en la entrada de nombre; lo pone a 1
     solo la consulta de rankings del frontal de la consola, 0x1ff950): el fondo y el titulo de la tabla (t = 0x40daa0 +
     ([0x40e040 + (cambio + config x 2 + variante x 4) x 4] + preset x 6) x 16: +4 capa 4 modo 1 cuadros 0..0x1E, +0 capa 4
     modo 1); cuando el titulo acaba, la linea de la meta (+0xC [meta], capa 2), las filas 3D0015 / 5 x 3D0016 (cuadro
     0x12A), la de su puesto ([0x40da88 + puesto % 5], capa 3, modo 0) y cada fila con la posicion de 0x40dc80 / 0x40ddb0:
     nombre (fuente 3, amarillo 0xFFFFFF00), numero (pagina 5: estilo 3 y 3D0026), coche 3D0024 ([0x40da10]), cambio
     3D0027, ruta (3D0014 y las metas 0x1ff740 + 0x20 en la capa 4; 3D0011 en los presets 2 / 3), y tiempo (3D0028 por la
     configuracion y el cuadro 3D001A con el tiempo en la fuente 3), puntos ("%7d", "9999999") o corazones ("%d") con el
     tiempo en cifras del estilo 4 (3D0025). Fuera del ranking, la banda 0x150010 (spr_ranking_xst, capa 1) en (0, 0x193),
     el resultado en (0x144, 0x1a3) y el rotulo 0x443..0x445 (TEXT.PS2: "TIEMPO" / "PUNTUACI\xb1N" / "CORAZONES") en la
     fuente 4 en (0x32, 0x1a3).
   - Textos (0x1175a8 -> 0x116fa0 / 0x116e10): la celda entera del glifo en (x, y), avance = ancho + espaciado, espacio =
     ancho; en el PS2 los caracteres >= 0x80 no se dibujan (0x116fa0 los toma con signo: la O acentuada de PUNTUACION
     queda en blanco); DESVIACION pedida por el usuario: la Dreamcast los dibuja con su glifo de la fuente; van en la capa 4 de los sprites (0x117558(4) -> 0x117708). PROBABLE: el color como ARGB
     (el del PC, 42CCA0) y el orden dentro de la capa (los sprites de las escenas antes que los textos).
   SIN PORTAR: el deslizamiento de las filas (0x1fe780 = 4B0970: solo mueve +0x24.. de las filas, que el tablero no lee
   sin el teclado); el banco 0x36 NAME_CMN y los de sprites 0x12 / 0x14 (solo los usa el teclado de letras, que la
   entrada de nombre no ensena); los 15 recorridos continuos (presets 2 / 3). DESVIACION: fuera del ranking (puesto -1)
   el original guarda el nombre en el registro de antes de la tabla (0x1fb970(.., -1)): la Dreamcast no lo hace. */
#include <kos.h>
#include <string.h>
#include <stdio.h>
#include "nombre.h"
#include "../comun/hud.h"
#include "../comun/sprani.h"
#include "../comun/rksp.h"
#include "../comun/menu.h"
#include "../comun/sfx.h"

#define CUADRO_FILAS 0x12a                              /* 0x2007dc / 0x200830: 429530(3D0015 / 3D0016, .., 0x12A) */
#define T_FIN 0xf0                                      /* 0x1fd0b0: [0x5da42c] = 0xF0 */

/* ---- bloque NOM1 de RANKSP.DAT (tools/conv_sp.py tablas_nombre) */
typedef struct { uint32_t id, w, h, pri, n; int32_t esp, esp_y; } fuente_t;
static const uint8_t *defe, *meta_tab, *filtro, *pal; static int n_pal;
static const uint32_t *fondo, *t34c, *tt, *luz, *f_coche, *f_conf, *lay[2], *estilo, *desc[2];
static const float *y16;
static const char *nom_def, *quita, *recorta, *sust, *rotulo[3];
static fuente_t fu[2];

static spr_banco_t *rb, *fb;
static hud_dat_t *tx; static int u_f[2] = { -1, -1 }, u_banda = -1;
static int activo, presto;

/* ---- el modulo (PS2 0x5da108.., PC 8423C0..) */
typedef struct {
    int32_t score; uint32_t tiempo; int32_t cuenta; char nom[6]; int8_t coche, cambio, conf; int ruta, tipo;
} fila_t;
static fila_t fila[10], fj;                             /* +0 .. (0x48 cada una) y la de fuera (+0x2D0) */
static int puesto, pagina, variante, preset, cuenta, linea, k34c, cambio, conf, primero, est, t_fin, hecho, voz;
static int h_tit = -1, h_tab = -1, h_fondo = -1, h_puesto = -1;
static uint32_t lic_coche; static char lic_nom[6];

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static int lee_nom1(const uint8_t *b, uint32_t tam)
{
    const uint8_t *p = b; int k;
    if (tam < 0x3600 || memcmp(b, "NOM1", 4)) return -1;
    p += 4; defe = p; p += RKSP_TAM;
    meta_tab = p; p += 80; filtro = p; p += 256;
    fondo = (const uint32_t *)p; p += 60 * 4; t34c = (const uint32_t *)p; p += 12 * 4;
    tt = (const uint32_t *)p; p += 24 * 9 * 4; luz = (const uint32_t *)p; p += 5 * 4;
    f_coche = (const uint32_t *)p; p += 10 * 4; f_conf = (const uint32_t *)p; p += 2 * 4;
    y16 = (const float *)p; p += 5 * 4;
    lay[0] = (const uint32_t *)p; p += 75 * 4; lay[1] = (const uint32_t *)p; p += 75 * 4;
    estilo = (const uint32_t *)p; p += 10 * 4;
    nom_def = (const char *)p; p += 8;
    quita = (const char *)p; p += 32; recorta = (const char *)p; p += 32; sust = (const char *)p; p += 32;
    n_pal = (int)rd32(p); k = (int)rd32(p + 4); p += 8; pal = p; p += k;
    for (k = 0; k < 3; k++) { rotulo[k] = (const char *)p; p += 32; }
    for (k = 0; k < 2; k++) { memcpy(&fu[k], p, sizeof fu[k]); p += sizeof fu[k]; }
    desc[0] = (const uint32_t *)p; p += 16 * 4; desc[1] = (const uint32_t *)p; p += 16 * 4;
    return (uint32_t)(p - b) <= tam ? 0 : -2;
}
static float lf(const uint32_t *l, int i) { float f; memcpy(&f, l + i, 4); return f; }

/* 0x2012a8 / 0x2013e8 / 0x201528 (PC 4B2080 / 4B2150 / 4B2220): las 10 filas de la tabla */
static void filas(uint32_t t, int tipo)
{
    int i; const uint8_t *r = rksp_tablas() + t;
    for (i = 0; i < 10; i++, r += 16) {
        uint32_t r0 = rd32(r), r8 = rd32(r + 8); fila_t *f = fila + i;
        memset(f, 0, sizeof *f);
        f->score = (int32_t)(r0 & 0xffffff); f->tiempo = rd32(r + 4); f->cuenta = (int32_t)(r8 & 0xfff);
        strncpy(f->nom, (const char *)r + 11, 4); f->nom[4] = 0;     /* 0x2a7d28(+0x14, rec + 0xB, 4); +0x18 = 0 */
        f->coche = (int8_t)(r0 >> 28 & 0xf); f->ruta = (int)(r0 >> 24 & 0xf);
        f->cambio = (int8_t)(r8 >> 17 & 1); f->conf = (int8_t)(r8 >> 18 & 1); f->tipo = tipo;
    }
}

/* la siguiente parte de la RAM fija (nombre_t: ram) tras la ultima apertura en ella (hudd_tablas_ocupa) */
static uint8_t *r_ram; static uint32_t r_cap, r_o;
static void avanza(void) { r_o += (hudd_tablas_ocupa() + 31) & ~31u; if (r_o > r_cap) r_o = r_cap; }

int nombre_entra(const nombre_t *d)
{
    uint32_t tam, curso, t; const uint8_t *x; uint8_t rec[16]; char n[16]; int ruta, sel_t; fila_t *e;
    nombre_sale();
    /* las tablas de los tres bancos, una detras de otra en la RAM fija (nunca en el monton: 86 + 110..113 + 6 KB, que
       quedarian bajo lo que se pida hasta volver al frontal); lo que sobra, para los sectores de las subidas de texturas */
    r_ram = d->ram; r_cap = d->ram ? d->ram_cap : 0; r_o = 0;
    rb = r_ram ? spr_abre_en("RANKSP.DAT", r_ram, r_cap) : NULL;
    if (rb) avanza();
    x = rb ? spr_extra(rb, &tam) : NULL;
    if (!x || lee_nom1(x, tam)) {
        dbglog(DBG_ERROR, "NOMBRE: sin RANKSP.DAT (RAM fija %uK): sin el modo 26\n", (unsigned)(r_cap >> 10));
        nombre_sale(); return -1;
    }
    rksp_defecto(defe);                                 /* 0x1fa3b0: sin datos guardados, las de por defecto */
    variante = d->variante; preset = d->preset & 3;
    ruta = d->ruta; if (!(ruta < 0x10) || ruta < 0) ruta = 0;
    /* 44DC50(450560(4)): el descriptor de la etapa del nivel 4; sin llegar a ella 450560(4) = -1 y 44DC50 da el de
       [0x548f6c] (en la BSS: sin localizar quien lo pone; PROBABLE el del primer registro: el mismo fondo, 0x37000C /
       0x37000D, para cualquier etapa que no sea una meta) */
    /* desc[0] del arbol 0 (csc_data_2), desc[1] del 1 (csc_data_cvt): el del preset (0x1760c0: 1 -> csc_data_2) */
    curso = desc[(preset & 1) ? 0 : 1][d->registro4 >= 0 && d->registro4 < 16 ? d->registro4 : 0];
    if (curso >= 60) curso = 0;
    sprintf(n, "NOM5%c.DAT", 'A' + (int)((fondo[curso] >> 16) - 0x37));
    if ((fb = spr_abre_en(n, r_ram + r_o, r_cap - r_o))) avanza();
    if (fb) { hudd_tablas_fijas(r_ram + r_o, r_cap - r_o); if ((tx = hudd_abre_gd("NOMTX.DAT"))) avanza(); }
    if (!fb || !tx) {
        dbglog(DBG_ERROR, "NOMBRE: sin %s o NOMTX.DAT (RAM fija %uK, %u bytes usados): sin el modo 26\n", n, (unsigned)(r_cap >> 10), (unsigned)r_o);
        nombre_sale(); return -1;
    }
    if (r_o < r_cap) { hudd_bufer(r_ram + r_o, r_cap - r_o); presto = 1; }
    u_f[0] = hudd_uso(tx, "F3"); u_f[1] = hudd_uso(tx, "F4"); u_banda = hudd_uso(tx, "BANDA");
    hudd_sube_uso(tx, u_f[0]); hudd_sube_uso(tx, u_f[1]); hudd_sube_uso(tx, u_banda);
    spr_suelta_todas();
    h_fondo = spr_crea_entera(fondo[curso], 0, 0);     /* 0x114b68([0x40d248 + curso x 4], 0, 0) */
    cuenta = meta_tab[ruta * 5 + 4] - 10;               /* [0x420] = 0x1ff740(ruta, 4) - 10 */
    linea = cuenta;                                     /* [0x360] */
    cambio = d->manual & 1 ? 1 : 0; conf = d->config < 0 ? 0 : d->config;
    /* el registro de la carrera (0x1fc69c..0x1fc7a8) */
    {
        uint32_t r0 = ((uint32_t)d->puntos & 0xffffff) | ((uint32_t)(d->modelo & 0xf) << 28) | ((uint32_t)ruta << 24);
        uint32_t r8 = variante == 2 ? (uint32_t)d->corazones & 0xfff : 0;
        r8 = (r8 & 0xfff9ffffu) | (uint32_t)(cambio & 1) << 17 | (uint32_t)(conf & 1) << 18;
        memcpy(rec, &r0, 4); memcpy(rec + 4, &d->tiempo, 4); memcpy(rec + 8, &r8, 4); memset(rec + 11, 0, 5);
    }
    puesto = variante == 0 && d->tiempo == 0 ? 10 : rksp_inserta(rec, variante, preset, cuenta, cambio, conf);
    if (!(puesto < 10)) puesto = -1;
    k34c = (int)t34c[(cambio + conf * 2 + variante * 4) & 15];
    t = rksp_tabla(variante, preset, cuenta, cambio, conf);
    if (variante >= 0 && variante <= 2) filas(t, variante);
    else memset(fila, 0, sizeof fila);
    e = puesto >= 0 ? fila + puesto : &fj;              /* la fila del jugador */
    memset(e, 0, sizeof *e);
    memcpy(e->nom, nom_def, 6);                         /* lw + lh de 0x40c838 */
    e->coche = (int8_t)d->modelo; e->score = d->puntos; e->cuenta = variante == 2 ? (int16_t)d->corazones : 0;
    e->ruta = ruta; e->tipo = variante; e->tiempo = d->tiempo; e->cambio = (int8_t)cambio; e->conf = (int8_t)conf;
    pagina = puesto < 0 ? 5 : puesto / 5 * 5;
    h_tit = h_tab = h_puesto = -1;
    primero = 1; est = 0; hecho = 0; voz = 1;
    sel_t = k34c + preset * 6;
    dbglog(DBG_INFO, "NOMBRE: modo 26, variante %d preset %d meta %d ruta %d, puesto %d (fondo %06x, tabla %d), VRAM libre %uK, "
           "tablas %u bytes en la RAM fija (%uK)\n", variante, preset, cuenta, ruta, puesto, (unsigned)fondo[curso], sel_t,
           (unsigned)(pvr_mem_available() >> 10), (unsigned)r_o, (unsigned)(r_cap >> 10));
    activo = 1;
    return 0;
}

/* 0x1fc340 / 0x1fc3e8 / 0x1fc4a8 (PC 4B07B0 / 4B0830 / 4B08C0) */
static int en(const char *set, char c) { return c == 0 || strchr(set, c) != NULL; }
static void limpia(char *s)
{
    char o[8]; int i, j = 0, n;
    for (i = 0; s[i]; i++) if (!en(quita, s[i])) o[j++] = s[i];
    o[j] = 0;
    for (i = 0; o[i] && en(recorta, o[i]); i++) ;
    memmove(o, o + i, strlen(o + i) + 1);
    for (n = strlen(o); n > 0 && en(recorta, o[n - 1]); n--) ;
    o[n] = 0; strcpy(s, o);
}
static int igual(const char *a, const char *b)         /* 0x2a9fe8 (stricmp) */
{
    for (;; a++, b++) {
        int x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a, y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if (x != y) return 0;
        if (!x) return 1;
    }
}
static void confirma(void)                              /* 0x1fcda4..0x1fd058 */
{
    fila_t *e = puesto >= 0 ? fila + puesto : &fj; char b[8], w[64]; const char *q = (const char *)pal; int i, k;
    memcpy(e->nom, lic_nom, 6); e->nom[4] = 0;          /* 0x1f6c80(+0x14, licencia, 6) */
    memcpy(b, e->nom, 6); b[6] = 0; limpia(b);
    if (b[0]) {
        for (i = 0; i < n_pal; i++, q += strlen(q) + 1) {
            strncpy(w, q, sizeof w - 1); w[sizeof w - 1] = 0;
            for (k = 0; k < 4 && w[k]; k++) w[k]--;      /* 68694C: los 4 primeros caracteres + 1 */
            if (igual(b, w)) break;
        }
        if (i < n_pal) { memcpy(e->nom, sust, 4); e->nom[4] = sust[4]; }
    }
    if (puesto >= 0) {                                  /* 0x1fbb20 / 0x1fbbf0 / 0x1fbd18 */
        uint8_t *r = rksp_tablas() + rksp_tabla(variante, preset, cuenta, cambio, conf) + puesto * 16;
        uint32_t r8;
        for (i = 0; i < 4; i++) r[11 + i] = (uint8_t)lic_nom[i];   /* 0x2a7d28(rec + 0xB, licencia, 4): strncpy */
        for (i = 0; i < 4; i++) if (!lic_nom[i]) { memset(r + 11 + i, 0, 4 - i); break; }
        r8 = rd32(r + 8); r8 = (r8 & 0xfffe0fffu) | (lic_coche & 0x1f) << 12; memcpy(r + 8, &r8, 4);
    }
}

int nombre_paso(uint32_t nuevo)
{
    if (!activo) return 0;
    spr_paso();
    if (voz) {                                          /* 0x1fcd64: 0x80A2 sin sonar y el idioma 0 -> 0x158 */
        voz = 0;
        if (sfx_voces_idioma() && !strcmp(sfx_voces_idioma(), "ENGLISHC")) sfx_menu(0x158);
    }
    if (primero) {                                      /* [0x36f4a8] */
        const uint8_t *l = menu_licencia();
        memset(lic_nom, 0, sizeof lic_nom); lic_coche = 0;
        if (l) { memcpy(lic_nom, l, 6); lic_coche = rd32(l + 0x1c); }
        confirma(); primero = 0;
        return 0;
    }
    switch (est) {                                      /* tabla de saltos 0x40c870 */
    case 0: case 1: est = 2; break;
    case 2: est = 5; break;
    case 5: t_fin = T_FIN; est = 6;                     /* (sigue en el 6) */
    /* fall through */
    case 6:
        if (--t_fin <= 0 || (nuevo & CONT_START)) est = 7;   /* 1b4548(1) */
        break;
    case 7:                                             /* 0x106288..: los rankings y la licencia a la VMU */
        menu_guarda_vmu(); est = 0xc; break;
    case 0xc: hecho = 1; break;                          /* [0x36f4b0] = 1 */
    default: break;
    }
    /* 49AA60: hecho -> (temporizador 0 vencido) modo 27 */
    return hecho;
}

/* ---- dibujo: los textos van a la capa 4 detras de las escenas de esa capa */
typedef struct { int f; float x, y; uint32_t col; char s[16]; } txt_t;
static txt_t tq[24]; static int n_tq;
static void texto(int f, int x, int y, uint32_t col, const char *s)
{
    if (n_tq < 24) { tq[n_tq].f = f; tq[n_tq].x = x; tq[n_tq].y = y; tq[n_tq].col = col; snprintf(tq[n_tq].s, sizeof tq[n_tq].s, "%s", s); n_tq++; }
}
/* 0x1f6d08 (449AC0): minutos, segundos y milesimas */
static void palabras(uint32_t t, int *mi, int *se, int *ms)
{
    uint32_t s = t / 1000, h = s / 3600, m = s / 60 - h * 3600;
    *ms = (uint16_t)(t - s * 1000); *mi = (uint16_t)m;
    *se = (uint16_t)(m * 0xffffffc4u - (uint16_t)((uint16_t)h * 0xe10u) + s);
}
/* 0x2016d0 (4B22F0): una escena del estilo por cifra decimal */
static void cifras(int est_, const char *s, int x, int y, int capa)
{
    uint32_t tok = estilo[est_ * 2]; int paso = (int)estilo[est_ * 2 + 1], at = 0;
    for (; *s; s++, at += paso) if (*s >= '0' && *s <= '9') spr_inmediata(tok, (float)at + (float)x, (float)y, capa, *s - '0');
}
static void t_cifras(int x, int y, uint32_t t)          /* 0x200ee0..0x200fe0 */
{
    char b[16]; int mi, se, ms, ty = y + 0x1c;
    palabras(t, &mi, &se, &ms);
    sprintf(b, "%2d", mi); cifras(4, b, x + 0x33, ty, 4);
    sprintf(b, "%02d", se); cifras(4, b, x + 0x55, ty, 4);
    sprintf(b, "%03d", ms); cifras(4, b, x + 0x7c, ty, 4);
    spr_inmediata(estilo[8], (float)(x + 0x4c), (float)ty, 4, 10);   /* 3D0025 cuadros 0xA / 0xB */
    spr_inmediata(estilo[8], (float)(x + 0x70), (float)ty, 4, 11);
}
static void puntos(const fila_t *f, int x, int y)       /* 0x200e50 (4B4010) */
{
    char b[16];
    if (f->score > 9999999) strcpy(b, "9999999"); else sprintf(b, "%7d", (int)f->score);
    texto(0, x, y, 0xffffff, b); t_cifras(x, y, f->tiempo);
}
static void corazones(const fila_t *f, int x, int y)    /* 0x201000 (4B4170) */
{
    char b[16]; sprintf(b, "%d", (int)f->cuenta); texto(0, x, y, 0xffffff, b); t_cifras(x, y, f->tiempo);
}
static void tiempo(const fila_t *f, int x, int y)       /* 0x201198 (4B1FA0) */
{
    char b[16]; int mi, se, ms;
    spr_inmediata(0x3d001a, 0, (float)y - 240.0f + 24.0f, 4, 0);
    palabras(f->tiempo, &mi, &se, &ms);
    sprintf(b, "%2d", mi); texto(0, x, y, 0xffffff, b);
    sprintf(b, "%02d", se); texto(0, x + 0x32, y, 0xffffff, b);
    sprintf(b, "%03d", ms); texto(0, x + 0x6a, y, 0xffffff, b);
}
static void marcas(int x, int y, int capa, int ruta, uint32_t tok)   /* 0x1ff878 (4B17E0) */
{
    int n;
    spr_inmediata(tok, (float)x, (float)y, capa, 0);
    if (tok == 0x360009 || tok == 0x3d0011) return;
    for (n = 0; n < 5; n++) {
        int g = ruta >= 0 && ruta < 16 ? meta_tab[ruta * 5 + n] : 0xf;
        if (g < 0xf) spr_inmediata(tok, (float)x, (float)y, capa + 1, g + 0x20);
    }
}
static int tablero(void)                                /* 0x200398 con [0x424] = 0: 1 si dibuja la banda */
{
    const uint32_t *t = tt + (k34c + preset * 6) * 9; const uint32_t *L; int i, base;
    /* presets 0 / 1 (0x2004d8..): los presets 2 / 3 (0x200400..: 3D001D / 3D001E, linea 6) no salen */
    if (h_tit < 0) h_tit = spr_crea(t[1], 4, 1, 0, 0x1e);   /* 0x114d00(t + 4, 4, 1, 0, 0x1E) */
    if (h_tab < 0) h_tab = spr_crea_entera(t[0], 4, 1);  /* 0x114b68(t + 0, 4, 1) */
    if (spr_estado(h_tit) != 3) return 0;
    if (linea >= 0 && linea < 7 && linea != 6) spr_inmediata(t[2 + linea], 0, 0, 2, 0);
    if (pagina < 5) spr_inmediata(0x3d0015, 0, 0, 2, CUADRO_FILAS);
    else for (i = 4; i >= 0; i--) spr_inmediata(0x3d0016, 0, y16[4 - i], 2, CUADRO_FILAS);
    if (h_puesto < 0 && puesto >= 0) h_puesto = spr_crea_entera(luz[puesto % 5], 3, 0);
    L = lay[pagina < 5 ? 0 : 1]; base = pagina / 5 * 5;
    for (i = 0; i < 5; i++) {
        const fila_t *f = fila + base + (4 - i); const uint32_t *a = L + i * 15; char b[8]; int k;
        for (k = 0; k < 6 && f->nom[k]; k++) b[k] = (char)filtro[(uint8_t)f->nom[k]];   /* 0x200308(.., 6) */
        b[k] = 0;
        texto(0, (int)lf(a, 1), (int)lf(a, 2), 0xffff00, b);
        if (pagina >= 5) {
            const uint32_t *m = L + (4 - i) * 15; char nb[16];
            sprintf(nb, "%2d", pagina + i + 1); cifras(3, nb, (int)lf(m, 11), (int)lf(m, 12), 3);
            spr_inmediata(0x3d0026, lf(m, 11) + 46.0f, lf(m, 12), 3, 10);
        }
        spr_inmediata(0x3d0024, lf(a, 5), lf(a, 6), 3, f->coche >= 0 && f->coche < 10 ? (int)f_coche[(int)f->coche] : 0);
        spr_inmediata(0x3d0027, lf(a, 3), lf(a, 4), 3, f->cambio);
        marcas((int)lf(a, 7), (int)lf(a, 8), 3, f->ruta, preset == 2 || preset == 3 ? 0x3d0011 : 0x3d0014);
        if (f->tipo == 0) {
            spr_inmediata(0x3d0028, lf(a, 13), lf(a, 14), 3, (int)f_conf[f->conf & 1]);
            tiempo(f, (int)lf(a, 9), (int)lf(a, 10));
        } else if (f->tipo == 1) puntos(f, (int)lf(a, 9), (int)lf(a, 10));
        else if (f->tipo == 2) corazones(f, (int)lf(a, 9), (int)lf(a, 10));
    }
    if (puesto >= 0) return 0;
    if (fj.tipo == 0) tiempo(&fj, 0x144, 0x1a3);         /* 0x200d9c.. */
    else if (fj.tipo == 1) puntos(&fj, 0x144, 0x1a3);
    else if (fj.tipo == 2) corazones(&fj, 0x144, 0x1a3);
    if (fj.tipo >= 0 && fj.tipo <= 2) texto(1, 0x32, 0x1a3, 0xffffff, rotulo[fj.tipo]);
    return 1;
}
static void escribe(const txt_t *q, float z)            /* 0x1175a8: 0x116fa0 / 0x116e10 por caracter */
{
    const fuente_t *f = fu + q->f; float x = q->x; const char *s;
    hudd_color(q->col);
    for (s = q->s; *s; s++) {
        int c = (uint8_t)*s;                            /* DESVIACION (el usuario, revision 41: "PUNTUACI N"): 0x116fa0
                                                           toma el caracter con signo y no dibuja los >= 0x80; aqui sin
                                                           signo, con el glifo de la fuente (la 4 trae 0x20..0xDF: Ó = 0xB1) */
        if (c != ' ' && c != 9 && c != 10 && c >= (int)f->pri && c < (int)(f->pri + f->n))
            { hudd_dibuja(tx, u_f[q->f], c - (int)f->pri, x, q->y, 1, z, 0); z = hudd_z(); }
        x += *s == ' ' ? (int)f->w : (int)f->w + f->esp;
    }
    hudd_color(0xffffff);
}
void nombre_dibuja(float z)
{
    int i, banda;
    if (!activo) return;
    n_tq = 0;
    banda = tablero();
    spr_dibuja(0, 1, z, 0);
    if (banda) hudd_dibuja(tx, u_banda, 0, 0, 0x193, 1, hudd_z(), 0);   /* 0x117d28(0x150010, 0, 0x193, capa 1) */
    spr_dibuja(1, 5, hudd_z(), 0);
    for (i = 0; i < n_tq; i++) escribe(tq + i, hudd_z());
    spr_dibuja(5, SPR_CAPAS, hudd_z(), 0);
}

void nombre_sale(void)                                  /* 0x1fd528 (4B0950: 428600) y 49AAE0 */
{
    spr_suelta_todas();
    h_tit = h_tab = h_fondo = h_puesto = -1;
    if (rb || fb || tx) pvr_wait_ready();
    if (fb) { spr_cierra(fb); fb = NULL; }
    if (rb) { spr_cierra(rb); rb = NULL; }
    if (tx) { hudd_cierra(tx); tx = NULL; }
    if (presto) { hudd_bufer(NULL, 0); presto = 0; }    /* (la RAM fija, para el modo siguiente) */
    activo = 0;
}
