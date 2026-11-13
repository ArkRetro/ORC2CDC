/* meta.c: la escena de la meta del original (modo 19 GOAL), con META.DAT (tools/conv_meta.py).

   Del PC (decompilacion, platform/race_end_modes.cpp 49C9C0 / 49CC50 y race_goal_camera.cpp 4871A0 / 486730 / 486EF0 /
   4874A0 / 475720), contrastado con SLES-53998: 49C9C0 = 0x1b8e98, 4871A0 = 0x1f4ef0, 486730 = 0x1f43d0 (tabla de
   saltos de los 31 op en 0x40bf70), 486EF0 = 0x1f4c08, 4874A0 = el final de 0x1f43a0..0x1f43c8.
   - 49C9C0: escena 3 o, en las variantes distintas de 4, 5, 6 y 7 con [0x359d98] == 0x42 (siempre), la del byte
     0x3f99b0 + (meta + preset x 5) x 4; meta = 450380(8) - 0xA (la clave de la etapa de meta - 10). Preset 0 (OutRun 2
     SP, csc_data_cvt): metas A..E -> escenas 3, 33, 3, 35, 34; preset 1 (OutRun 2, csc_data_2): 1..5. Musica 401000(0, pista, 0) (sin
     bucle): variante 1 tabla 0x3f9a18, 2 tabla 0x3f9a30; temporizador del modo 0x4B0 (1200 cuadros; 0x258 en la 2).
   - 4871A0: 82E7D4 = escena, 82E7C4 = 82E7C8 = 82E7DC = 82E7CC = 0, 82E7D8 = 1 (activa), 82E7E0 = 82E7E8 = 0,
     82E7E4 = 2, 82E7D0 = 1, 82E7C0 = 1; marca todos los registros del guion (+0x28 = 1); car+2F0 = (& ~0x46) | 0x38.
   - 4874A0 (suceso 7, antes del coche 8 y de la camara 385): si 82E7D8, 486730 y 486EF0; luego 82E7E0 += 1, 82E7C4 =
     (int) 82E7E0, 82E7E8 = 82E7E0 / 60 (0,016666668).
   - 486730: cada registro marcado con +0x24 <= 82E7C4 se ejecuta una vez (op <= 0x1E). Las de la escena de meta de
     SLES-53998 (las 8 escenas usan el mismo guion 0x36d8a0): 0 op 11(0) car+38 = 0 y car+1F4 = 0; op 6(1) el coche
     pasa a PasPlCar_Ctrl (475720: sin fisica, car+4 |= 0x800000); op 7(1) el coche al origen del marco de la union
     (7D3190) con sus angulos (449640); op 12(2, 0) 82E7E4 = 2 (seccion de curso que ve la camara: 487320 da 0xFFFF, la
     demo elige lo que dibuja a su manera); op 15(1) 82E7C0 = 1 (estilo del humo 41CED0); op 18(1) 2057C0(0x16A / 0x16B,
     1) (RET en PS2 y PC); op 19 / 20 planos cercano 0,5 y lejano 1900 (camara +BC / +C0); cuadro 60 op 5(0x96) el
     efecto 0x96 (4249F0; banco \PAS3.PAK, cargado durante la carrera); 120 op 8(3) 82E7C8 = 3 (humo y marcas de las
     ruedas 0 y 1: bits de 95AFAC en 41C570); 220 op 8(0); 599 cercano 0,1 y lejano 4000; 600 op 30: car+2DC = 0 y
     82E7D8 = 0 (fin: la camara y el coche se quedan como estan).
   - 486EF0 (si la escena tiene movimiento y 82E7E8 <= su duracion, 10 s): 20 canales (513650) en t = 82E7E8:
     car+2D8 c0, +2DC c1 (solo en el modo 16 sin 44FF10; si no 82EA70, que nadie escribe: 0), +2E0 c2, +2E8 c3,
     +2E4 c4, +2EC c5; palabras (x 65536 / 2pi, truncado) rueda 0 +32 c6, rueda 1 +32 c7, ruedas 0..3 +30 c8..c11;
     car+32 = 82E7CC = c6; con 82E7D0 == 1 la camara: +E0..+E8 c12..c14 (ojo), +EC..+F4 c15..c17 (mira), +130 c18
     (balanceo) y +A4 = +AC = 2 atan(0,70865 / (c19 x 0,03937)) (c19: focal en mm; en la variante 4, +E4 += 0,5). */
#include <kos.h>
#include <dc/sound/sound.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "meta.h"
#include "../comun/gd.h"
#include "../comun/sfx.h"
#include "../comun/or2_fisica.h"

#define K_ANG 10430.3779296875f            /* 6282C0: 65536 / 2pi */
typedef struct { uint32_t op, w[4]; float f14, f18; int16_t cuadro; uint16_t r; } reg_t;   /* 32 bytes */
typedef struct { uint32_t og, n, om; float dur; } esc_t;

typedef struct { uint8_t *D; uint32_t Dn; const esc_t *esc; uint32_t nesc; } dat_t;
static dat_t M, Sa;                        /* META.DAT y la parte residente de SALIDA.DAT */
static dat_t *A = &M;                      /* el de la escena que corre */
static const uint8_t *tab;                 /* escena[20], pista_v1[5], pista_v2[5] */
static uint32_t ef_id, ef_vol, ef_hz, ef_n, ef_o, ef_b, ef_spu;
static uint8_t hecho[64];                  /* +0x28 de cada registro del guion de la escena */
static int escena = -1;
static float reloj;                        /* 82E7E0 */
static float t_mov;                        /* 82E7E8 */

static volatile int lista;                 /* META.DAT leido y comprobado (meta_carga puede ir en el hilo de carga) */
int meta_carga(void)
{
    uint32_t h[4], i;
    if (M.D) return 0;
    if (!(M.D = gd_carga("META.DAT", &M.Dn))) return -1;
    memcpy(h, M.D, 16);
    if (M.Dn < 16 + 32 + 20 || memcmp(M.D, "MET1", 4) || h[2] + 16 * h[1] > M.Dn || h[1] > 64) { free(M.D); M.D = NULL; return -2; }
    M.nesc = h[1]; M.esc = (const esc_t *)(M.D + h[2]); tab = M.D + 16;
    { const uint8_t *e = M.D + 16 + 32; uint16_t a, b; memcpy(&a, e, 2); memcpy(&b, e + 2, 2); ef_id = a; ef_vol = b;
      memcpy(&ef_hz, e + 4, 4); memcpy(&ef_n, e + 8, 4); memcpy(&ef_o, e + 12, 4); memcpy(&ef_b, e + 16, 4); }
    for (i = 0; i < M.nesc; i++)
        if ((M.esc[i].n && (M.esc[i].og + 32 * M.esc[i].n > M.Dn || M.esc[i].n > 64)) || (M.esc[i].om && M.esc[i].om + 160 > M.Dn)) {
            free(M.D); M.D = NULL; return -3;
        }
    if (ef_o + ef_b > M.Dn) ef_b = 0;
    dbglog(DBG_INFO, "META: META.DAT %u KB, %u escenas, efecto 0x%x\n", (unsigned)(M.Dn >> 10), (unsigned)M.nesc, (unsigned)ef_id);
    lista = 1;
    return 0;
}
int meta_lista(void) { return lista; }
void meta_suelta(void) { lista = 0; free(M.D); M.D = NULL; M.esc = NULL; tab = NULL; M.nesc = 0; if (A == &M) escena = -1; }

int meta_escena(int variante, int preset, int meta)
{
    if (!M.D || variante == 4 || variante == 5 || variante == 6 || variante == 7) return 3;
    if (meta < 0 || meta > 4 || preset < 0 || preset > 3) return 3;
    return tab[meta + preset * 5];
}
int meta_pista(int variante, int meta)
{
    if (!M.D || meta < 0 || meta > 4) return -1;
    if (variante == 1) return tab[20 + meta];
    if (variante == 2) return tab[25 + meta];
    return -1;                             /* 0 (Time Attack del PC) y las demas: otra cosa o nada (PENDIENTE) */
}

void meta_empieza(meta_t *m, int e)
{
    memset(m, 0, sizeof *m);
    m->coloca = -1; m->cam = 1; m->activa = 1; m->pasplcar = 0;
    A = &M; escena = e; reloj = 0; t_mov = 0; m->cuadro = 0;
    memset(hecho, 1, sizeof hecho);
    if (M.D && ef_b && !ef_spu) ef_spu = sfx_muestra_sube(M.D + ef_o, ef_b);
}
void meta_termina(void) { if (ef_spu) { sfx_muestra_baja(ef_spu); ef_spu = 0; } if (A == &M) escena = -1; }

static int salida_juego;                   /* 78026C == 0x10 (modo GAME) */
static void salida_efecto(uint32_t id);

static float canal(const uint8_t *mv, int k, float t)
{
    uint32_t n, o;
    memcpy(&n, mv + 8 * k, 4); memcpy(&o, mv + 8 * k + 4, 4);
    if (!n || o + 16 * n > A->Dn) return 0.0f;
    return or2_canal((const float *)(A->D + o), (int)n, t);
}
static int16_t palabra(float v) { float x = v * K_ANG; return (int16_t)(int32_t)x; }   /* cvttss2si (16 bits bajos) */

static void guion(meta_t *m)                /* 486730 */
{
    const esc_t *E; const reg_t *r; uint32_t i;
    if (!A->D || escena < 0 || (uint32_t)escena >= A->nesc) return;
    E = A->esc + escena; if (!E->n) return;
    r = (const reg_t *)(A->D + E->og);
    for (i = 0; i < E->n; i++, r++) {
        if (!hecho[i] || r->cuadro > m->cuadro) continue;
        hecho[i] = 0;
        switch (r->op) {
        case 0: m->sprite = r->w[0]; break;   /* 428320(w4, w8, wC): la animacion de sprites (2C000A: la cuenta) */
        case 3: case 4: break;              /* la musica: 401050 / 401030 y 401000(w4, 48B1E0, 1) (la pone main.c) */
        case 5:                             /* 4249F0(w4), salvo 0x16A con 4493C0 y las variantes 6 / 7 */
            if (A == &Sa) { salida_efecto(r->w[0]); break; }
            if (r->w[0] == ef_id && ef_spu) sfx_muestra_toca(ef_spu, ef_n, ef_hz, (int)ef_vol);
            else dbglog(DBG_INFO, "META: efecto 0x%x sin muestra\n", (unsigned)r->w[0]);
            break;
        case 6: if (r->w[0] <= 1) m->pasplcar = (int)r->w[0]; break;   /* 486942 SelectPlCarCtrl */
        case 7: m->coloca = (int)r->w[0]; break;
        case 8: m->humo = r->w[0]; break;
        case 11: m->freno = r->w[0] == 1 ? 0xff : 0; m->kmh = r->w[0] == 1 ? 0x1e0 : 0; break;
        case 12: case 15: case 18: break;   /* ver arriba */
        case 14: m->cam = r->w[0] == 1; break;
        case 19: m->cerca = r->f14; break;
        case 20: m->lejos = r->f14; break;
        case 30: m->t[1] = 0; m->activa = 0; break;
        default: dbglog(DBG_INFO, "META: op %u sin portar (cuadro %d)\n", (unsigned)r->op, r->cuadro); break;
        }
    }
}

static int movimiento(meta_t *m)            /* 486EF0 */
{
    const esc_t *E; const uint8_t *mv; float t = t_mov, c19;
    if (!A->D || escena < 0 || (uint32_t)escena >= A->nesc) return 0;
    E = A->esc + escena; if (!E->om) return 0;
    if (t > E->dur) return 0;
    mv = A->D + E->om;
    m->t[0] = canal(mv, 0, t);
    m->t[1] = A == &Sa && salida_juego ? canal(mv, 1, t) : 0.0f;   /* modo 16 sin 44FF10; si no 82EA70 (0) */
    m->t[2] = canal(mv, 2, t);
    m->r[1] = canal(mv, 3, t);
    m->r[0] = canal(mv, 4, t);
    m->r[2] = canal(mv, 5, t);
    m->s32 = palabra(canal(mv, 6, t));
    m->dir1 = palabra(canal(mv, 7, t));
    m->giro[0] = palabra(canal(mv, 8, t));
    m->giro[1] = palabra(canal(mv, 9, t));
    m->giro[2] = palabra(canal(mv, 10, t));
    m->giro[3] = palabra(canal(mv, 11, t));
    if (!m->cam) return 1;
    m->ojo[0] = canal(mv, 12, t); m->ojo[1] = canal(mv, 13, t); m->ojo[2] = canal(mv, 14, t);
    m->mira[0] = canal(mv, 15, t); m->mira[1] = canal(mv, 16, t); m->mira[2] = canal(mv, 17, t);
    m->balanceo = canal(mv, 18, t);
    c19 = canal(mv, 19, t);
    m->fov = 2.0f * atanf(0.7086499929428101f / (c19 * 0.03937000036239624f));
    return 1;
}

void meta_paso(meta_t *m)                   /* 4874A0 */
{
    m->coloca = -1; m->cerca = m->lejos = 0; m->sprite = 0;
    if (!m->activa) { m->mueve = 0; return; }
    guion(m);
    m->mueve = movimiento(m);               /* tambien en el paso del op 30: 4874A0 ya miro 82E7D8 */
#ifdef DIAG_META_CUADRO
    if (m->cuadro >= DIAG_META_CUADRO) return;  /* prueba: la escena quieta en ese cuadro */
#endif
    reloj += 1.0f;
    m->cuadro = (int)reloj;
    t_mov = reloj * 0.01666666753590107f;
}

/* ---------------------------------------------------------------- la escena de la salida (SALIDA.DAT)
   START (49DB20 / 49DD40, PS2 0x1b6824..0x1b69ac) llama a 4871A0 con la escena de la ruta: con un jugador el byte de
   la tabla 0x3f9338 por el modelo (12 / 13 solo con la palabra 0 del descriptor de la etapa 0 o 15; si no 11), con
   mas, 0x3f9328[n - 1]. El guion y el movimiento son los de la meta (486730 / 486EF0, suceso 7). Los efectos (op 5,
   banco PAS1.PAK) se suben al AICA al arrancar y se quedan; el guion (16 KB) se lee al empezar cada salida y se suelta
   al acabar (op 30). */
typedef struct { uint16_t id, vol; uint32_t hz, n, o, b, corte, n2, o2, b2; } sef_t;    /* 36 bytes */
#define SAL_EF 12
#define SAL_CANALES 6                      /* a la vez en la salida: 0x97, 0x98, 0x9c, 0x9a, 0x9d, 0x99 */
static sef_t sef[SAL_EF]; static uint32_t sef_n, sef_a[SAL_EF], sef_a2[SAL_EF];
static uint32_t sal_lba, sal_res, sal_hdr[7];
static uint8_t sal_rutas[34];
static int sal_canal[SAL_CANALES], sal_sig, sal_ingles, sal_variante;
static struct { int ef; uint64_t t0; } sal_pend[SAL_CANALES];

int salida_carga(void)
{
    uint32_t tam, i; uint8_t *b;
    if (gd_busca("SALIDA.DAT", &sal_lba, &tam)) return -1;
    if (!(b = memalign(32, 36 * 1024))) return -2;
    if (gd_lee(sal_lba, 1, b) || memcmp(b, "SAL1", 4)) { free(b); return -3; }
    memcpy(sal_hdr, b + 4, sizeof sal_hdr);                 /* nesc, o_esc, o_dat, n_ef, o_ef, residente */
    memcpy(sal_rutas, b + 28, sizeof sal_rutas);
    sal_res = sal_hdr[5];
    sef_n = sal_hdr[3] > SAL_EF ? SAL_EF : sal_hdr[3];
    if (sal_hdr[0] > 64 || sal_res > tam || sal_hdr[4] + 36 * sef_n > sal_res || sal_res > 64 * 1024) { free(b); return -4; }
    {   /* la tabla de efectos (en la parte residente) */
        uint32_t s0 = sal_hdr[4] / 2048, s1 = (sal_hdr[4] + 36 * sef_n + 2047) / 2048;
        if (gd_lee(sal_lba + s0, (int)(s1 - s0), b)) { free(b); return -5; }
        memcpy(sef, b + (sal_hdr[4] - s0 * 2048), 36 * sef_n);
    }
    for (i = 0; i < sef_n; i++) {                           /* las muestras, trozo a trozo, al AICA */
        int k;
        for (k = 0; k < 2; k++) {
            uint32_t o = k ? sef[i].o2 : sef[i].o, n = k ? sef[i].b2 : sef[i].b, s0 = o / 2048, s1 = (o + n + 2047) / 2048;
            if (!n) continue;
            if ((s1 - s0) * 2048 > 36 * 1024 || o + n > tam || gd_lee(sal_lba + s0, (int)(s1 - s0), b)) { free(b); return -6; }
            if (k) sef_a2[i] = sfx_muestra_sube(b + (o - s0 * 2048), n); else sef_a[i] = sfx_muestra_sube(b + (o - s0 * 2048), n);
            if (!(k ? sef_a2[i] : sef_a[i])) dbglog(DBG_ERROR, "SALIDA: sin RAM de sonido para el efecto 0x%x\n", sef[i].id);
        }
    }
    free(b);
    for (i = 0; i < SAL_CANALES; i++) sal_canal[i] = sfx_canal_nuevo();
    /* 4493C0 / 0x1f6548: 0x16A solo con el idioma 0 (ingles: 0x1071d8); el de la demo es el de las voces */
    sal_ingles = sfx_voces_idioma() && !strcmp(sfx_voces_idioma(), "ENGLISHC");
    dbglog(DBG_INFO, "SALIDA: %u efectos en el AICA (RAM de sonido libre %u KB), guion %u bytes\n", (unsigned)sef_n,
           (unsigned)(snd_mem_available() >> 10), (unsigned)sal_res);
    return 0;
}

int salida_escena(int jugadores, int modelo, int clave)
{
    int e;
    if (!sal_res) return -1;
    if (jugadores > 1) return jugadores <= 4 ? sal_rutas[30 + jugadores - 1] : -1;
    if (modelo < 0 || modelo >= 30) return -1;
    e = sal_rutas[modelo];
    if ((e == 12 || e == 13) && clave != 0 && clave != 15) e = 11;
    return e;
}

int salida_empieza(meta_t *m, int e, int variante)
{
    uint32_t i;
    salida_suelta();
    if (!sal_res || e < 0) return -1;
    if (!(Sa.D = memalign(32, (sal_res + 2047) & ~2047u))) return -2;
    if (gd_lee(sal_lba, (int)((sal_res + 2047) / 2048), Sa.D)) { salida_suelta(); return -3; }
    Sa.Dn = sal_res; Sa.nesc = sal_hdr[0]; Sa.esc = (const esc_t *)(Sa.D + sal_hdr[1]);
    if ((uint32_t)e >= Sa.nesc) { salida_suelta(); return -4; }
    for (i = 0; i < Sa.nesc; i++)
        if ((Sa.esc[i].n && (Sa.esc[i].og + 32 * Sa.esc[i].n > Sa.Dn || Sa.esc[i].n > 64)) || (Sa.esc[i].om && Sa.esc[i].om + 160 > Sa.Dn)) {
            salida_suelta(); return -5;
        }
    memset(m, 0, sizeof *m);
    m->coloca = -1; m->cam = 1; m->activa = 1;
    A = &Sa; escena = e; reloj = 0; t_mov = 0; salida_juego = 0; sal_variante = variante;
    memset(hecho, 1, sizeof hecho);
    memset(sal_pend, 0, sizeof sal_pend);
    return 0;
}
void salida_modo_juego(int si) { salida_juego = si; }
void salida_suelta(void)
{
    if (A == &Sa) { A = &M; escena = -1; }
    free(Sa.D); memset(&Sa, 0, sizeof Sa);
}
int salida_activa(void) { return A == &Sa && Sa.D != NULL; }

static void salida_efecto(uint32_t id)    /* 4249F0 (op 5) */
{
    uint32_t i; int c;
    if (id == 0x16a && !sal_ingles) return;                 /* 4493C0 */
    if (sal_variante == 6 || sal_variante == 7) return;     /* 450240 */
    for (i = 0; i < sef_n && sef[i].id != id; i++) ;
    if (i == sef_n || !sef_a[i]) { dbglog(DBG_INFO, "SALIDA: efecto 0x%x sin muestra\n", (unsigned)id); return; }
    c = sal_sig; sal_sig = (sal_sig + 1) % SAL_CANALES;     /* cada efecto en su canal (los del IRX no se cortan) */
    if (sal_canal[c] < 0) return;
    sfx_muestra_toca_en(sal_canal[c], sef_a[i], sef[i].n, sef[i].hz, sef[i].vol);
    sal_pend[c].ef = sef[i].n2 && sef_a2[i] ? (int)i + 1 : 0; sal_pend[c].t0 = timer_us_gettime64();
}
/* cada cuadro: el segundo trozo de las muestras de mas de 65535 (ver tools/conv_salida.py) */
void salida_sfx_paso(void)
{
    int c; uint64_t t = timer_us_gettime64();
    for (c = 0; c < SAL_CANALES; c++) {
        int i = sal_pend[c].ef - 1;
        if (i < 0 || t - sal_pend[c].t0 < (uint64_t)sef[i].corte * 1000000u / sef[i].hz) continue;
        sfx_muestra_toca_en(sal_canal[c], sef_a2[i], sef[i].n2, sef[i].hz, sef[i].vol);
        sal_pend[c].ef = 0;
    }
}
void salida_sfx_calla(void)
{
    int c;
    for (c = 0; c < SAL_CANALES; c++) { sal_pend[c].ef = 0; if (sal_canal[c] >= 0) sfx_canal_calla(sal_canal[c]); }
}
