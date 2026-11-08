/* sfx.c: efectos del coche del jugador para KallistiOS (ver sfx.h).

   Como lo hace el original (decompilacion del port de PC, race_sound.cpp / pc_sound.cpp, y el ELF de PS2):
   - Motor: dos grupos ICS a la vez con el mismo x = indice de rpm (primer umbral de la tabla de 128 rpm mayor que las
     rpm actuales, limitado a 2..127): "con gas" (8 capas) y "sin gas" (7 capas). Volumen del canal:
         A = (x*0.4 + 80)/127 (max 1), B = max(A, 0.8)
         con gas   = A * C^0.3 * D * 51.5      sin gas = B * (1-C)^0.25 * D * 51.5      (0..127)
     con C = acelerador 0..1 y D = 0.37 (rampa de arranque del original ya terminada).
   - Derrape: grupo de 4 capas, x = deslizamiento*126; volumen sqrt(0.1*ruedas + 0.6) * D * 151.2 con las 4 ruedas
     en asfalto (= 56).
   - Golpe contra muro: grupo de 6 capas; al chocar x = 127 y baja 0.05*127 por cuadro (20 cuadros); volumen
     0.56*127 (muro normal).
   - Cada capa: volumen = canal/127 * curva_vol(x)/127 * 2^-((15-aten)/2); tono = frecuencia de la muestra *
     2^((curva_pitch(x)/3 - nota*115)/4096) con curva_pitch = semitonos*1024 + fino*16. Las curvas se interpolan
     linealmente en el tramo x0 < x <= x1. Una capa con volumen 0 se apaga y vuelve a empezar la muestra al
     reaparecer (como el original).
   Aqui x es real (interpolando entre umbrales de rpm) para que el tono no vaya a saltos.
   Menus: sfx_menu(id) toca la muestra "se%03x" una vez (sin bucle) a su frecuencia y con el volumen de la tabla SE del
   IRX (u16 final de la muestra en SFX.DAT), alternando entre dos canales para que un efecto no corte al anterior.
   Voces (VOCES.DAT, tools/conv_sonido.py): las muestras del banco de voz del idioma (ORC.PAK y PERO.pak) y dos efectos
   de carrera (0x4d EXTEND TIME y 0x18d aviso de poco tiempo) a la RAM del AICA al arrancar (214 KB en espanol: caben
   junto a SFX.DAT y los buferes de la musica). sfx_voz(id) toca la del id a su frecuencia (12 kHz) con el volumen de
   la tabla SE del IRX (0x3fff, el maximo de la SPU2 = 255 del AICA: las voces, como en el original, por encima de la
   musica, que va a 200 en musica.c); las voces van por un canal propio (una voz nueva corta
   la anterior: SUPUESTO, el original las manda a la cola de sonido de la carrera) y los efectos por otro. */
#include <kos.h>
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#include <dc/sound/aica_comm.h>
#include <dc/spu.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gd.h"
#include "sfx.h"

#define MAX_CAPAS 8
#define MAX_MUESTRAS 32
#define D_ORIG 0.37f            /* factor de volumen del original tras su rampa inicial */
#define ESCALA 3.0f             /* nivel general frente a la musica (ajustable con sfx_volumen) */

enum { G_MOTOR_ACEL, G_MOTOR_SUELTA, G_DERRAPE, G_GOLPE, N_GRUPOS };
static const char *nombres_grupo[N_GRUPOS] = { "motor_acel", "motor_suelta", "derrape", "golpe" };

typedef struct {                /* como en SFX.DAT */
    char nombre[12];
    uint32_t desp, bytes, frec, ini, fin;
    uint8_t formato, bucle;
    uint16_t vol;               /* efectos de menu: volumen de la tabla SE (0..0x3fff) */
} muestra_dat_t;

typedef struct {
    uint8_t muestra;
    int8_t aten, nota;
    uint8_t nada;
    int8_t pitch[8][4];         /* x, semitonos, fino, 0 */
    int8_t vol[8][2];           /* x, v */
} capa_dat_t;

typedef struct {
    const capa_dat_t *c;
    float ganancia;             /* 2^-((15-aten)/2) */
    int canal;                  /* canal del AICA (-1 = sin canal) */
    int sonando;
    int vol, pan;
    uint32_t frec;
} capa_t;

typedef struct {
    int n;
    capa_t capa[MAX_CAPAS];
} grupo_t;

static uint8_t *dat;
static muestra_dat_t *mues;
static int n_mues;
static uint32_t base_spu[MAX_MUESTRAS];
static uint32_t ram_spu;
static grupo_t grupos[N_GRUPOS];
static float umbral_rpm[128];
static float vol_general = 1.0f;
static float golpe_x = -1.0f, golpe_vol;
static int listo;
#define N_CANAL_MENU 2
static int canal_menu[N_CANAL_MENU] = { -1, -1 }, menu_turno;
#define VOL_MENU 0.8f           /* nivel de los efectos de menu (volumen 0x3fff del IRX) */
#define MAX_VOCES 64

typedef struct {                /* como en VOCES.DAT */
    uint16_t id, vol;
    uint32_t desp, bytes, frec, n;
} voz_dat_t;

static voz_dat_t voz[MAX_VOCES];
static uint32_t voz_spu, voz_tablas_tam;
static uint8_t *voz_tablas;
static int n_voz, canal_voz = -1, canal_voz_se = -1;
static char voz_idioma[9];

/* curva de volumen (0..127) en x */
static float curva_vol(const int8_t p[8][2], float x)
{
    int i;
    for (i = 0; i < 7; i++) {
        float x0 = p[i][0], x1 = p[i + 1][0];
        if (x0 < x && x <= x1)
            return p[i][1] + (p[i + 1][1] - p[i][1]) * (x - x0) / (x1 - x0);
    }
    return 0.0f;
}

/* curva de tono en x, en unidades del original (semitonos*1024 + fino*16) */
static float curva_pitch(const int8_t p[8][4], float x)
{
    int i;
    for (i = 0; i < 7; i++) {
        float x0 = p[i][0], x1 = p[i + 1][0];
        if (x0 < x && x <= x1) {
            float v0 = p[i][1] * 1024 + p[i][2] * 16, v1 = p[i + 1][1] * 1024 + p[i + 1][2] * 16;
            return v0 + (v1 - v0) * (x - x0) / (x1 - x0);
        }
    }
    return 0.0f;
}

static void manda(int canal, uint32_t orden, const muestra_dat_t *m, uint32_t base, uint32_t frec, int vol, int pan)
{
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    memset(tmp, 0, sizeof(tmp));
    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = canal;
    chan->cmd = orden;
    if (m) {
        chan->base = base;
        chan->type = m->formato == 2 ? AICA_SM_ADPCM : AICA_SM_16BIT;
        chan->length = m->fin;
        chan->loop = m->bucle;
        chan->loopstart = m->ini;
        chan->loopend = m->fin;
    }
    chan->freq = frec;
    chan->vol = vol;
    chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}

/* lleva una capa a (x, volumen del canal 0..1) */
static void pon_capa(capa_t *k, float x, float vcanal)
{
    const muestra_dat_t *m = &mues[k->c->muestra];
    float g = vcanal * curva_vol(k->c->vol, x) * (1.0f / 127.0f) * k->ganancia * vol_general * ESCALA;
    int vol = (int)(g * 255.0f + 0.5f);
    uint32_t frec;
    if (k->canal < 0) return;
    if (vol > 255) vol = 255;
    if (vol <= 0) {
        if (k->sonando) { snd_sfx_stop(k->canal); k->sonando = 0; }
        return;
    }
    frec = (uint32_t)(m->frec * exp2f((curva_pitch(k->c->pitch, x) * (1.0f / 3.0f) - k->c->nota * 115.0f) *
                                      (1.0f / 4096.0f)) + 0.5f);
    if (frec < 100) frec = 100;
    if (frec > 176400) frec = 176400;
    if (!k->sonando) {
        manda(k->canal, AICA_CH_CMD_START, m, base_spu[k->c->muestra], frec, vol, 128);
        k->sonando = 1; k->vol = vol; k->frec = frec; k->pan = 128;
        return;
    }
    {
        uint32_t o = 0;
        uint32_t df = frec > k->frec ? frec - k->frec : k->frec - frec;
        if (df * 512 > k->frec) o |= AICA_CH_UPDATE_SET_FREQ;     /* cambios de menos de ~0.2 %: no se mandan */
        if (vol != k->vol) o |= AICA_CH_UPDATE_SET_VOL;
        if (!o) return;
        if (o & AICA_CH_UPDATE_SET_FREQ) k->frec = frec;
        k->vol = vol;
        manda(k->canal, AICA_CH_CMD_UPDATE | o, NULL, 0, k->frec, k->vol, k->pan);
    }
}

static void pon_grupo(int g, float x, float vcanal)
{
    int i;
    if (x < 0.0f) x = 0.0f;
    if (x > 127.0f) x = 127.0f;
    for (i = 0; i < grupos[g].n; i++) pon_capa(&grupos[g].capa[i], x, vcanal);
}

static void calla_grupo(int g)
{
    int i;
    for (i = 0; i < grupos[g].n; i++) {
        capa_t *k = &grupos[g].capa[i];
        if (k->sonando) { snd_sfx_stop(k->canal); k->sonando = 0; }
    }
}

int sfx_inicia(void)
{
    uint32_t tam, h[8], off, total, *cab;
    int g, i;
    if (listo) return 0;
    cab = gd_carga("SFX.DAT", &tam);
    if (!cab) return -1;
    memcpy(h, cab, sizeof(h));      /* 'SFX1', muestras, grupos, off_m, off_g, off_d, off_rpm, total */
    if (tam < 32 || memcmp(h, "SFX1", 4) || h[1] > MAX_MUESTRAS || h[7] > tam || h[5] > tam) { free(cab); return -1; }
    /* muestras a la RAM del AICA (un solo bloque); en la RAM principal solo quedan las tablas */
    snd_init();
    total = (tam - h[5] + 31) & ~31u;
    ram_spu = snd_mem_malloc(total);
    if (!ram_spu) { free(cab); return -1; }
    spu_memload(ram_spu, (uint8_t *)cab + h[5], (tam - h[5] + 3) & ~3u);
    dat = malloc(h[5]);
    if (!dat) { free(cab); return -1; }
    memcpy(dat, cab, h[5]);
    free(cab);
    n_mues = h[1];
    mues = (muestra_dat_t *)(dat + h[3]);
    memcpy(umbral_rpm, dat + h[6], sizeof(umbral_rpm));
    for (i = 0; i < n_mues; i++) base_spu[i] = ram_spu + mues[i].desp;
    /* grupos por nombre; un canal del AICA reservado por capa */
    memset(grupos, 0, sizeof(grupos));
    off = h[4];
    for (i = 0; i < (int)h[2]; i++) {
        const char *nm = (const char *)(dat + off);
        uint32_t nc;
        const capa_dat_t *c = (const capa_dat_t *)(dat + off + 16);
        memcpy(&nc, dat + off + 12, 4);
        for (g = 0; g < N_GRUPOS; g++) {
            int j;
            if (strncmp(nm, nombres_grupo[g], 12)) continue;
            grupos[g].n = nc > MAX_CAPAS ? MAX_CAPAS : (int)nc;
            for (j = 0; j < grupos[g].n; j++) {
                capa_t *k = &grupos[g].capa[j];
                k->c = &c[j];
                k->ganancia = exp2f(-(15 - c[j].aten) * 0.5f);
                k->canal = c[j].muestra < n_mues ? snd_sfx_chn_alloc() : -1;
            }
        }
        off += 16 + nc * sizeof(capa_dat_t);
    }
    for (i = 0; i < N_CANAL_MENU; i++) canal_menu[i] = snd_sfx_chn_alloc();
    listo = 1;
    return 0;
}

void sfx_motor(float rpm, float acelerador, int marcha)
{
    float x, a, b, c, va, vb;
    int i;
    (void)marcha;
    if (!listo) return;
    /* golpe: se apaga en 20 cuadros */
    if (golpe_x >= 0.0f) {
        pon_grupo(G_GOLPE, golpe_x * 127.0f, golpe_vol);
        golpe_x -= 0.05f;
        if (golpe_x < 0.0f) calla_grupo(G_GOLPE);
    }
    /* indice de rpm con parte fraccionaria */
    for (i = 0; i < 128 && !(rpm < umbral_rpm[i]); i++) ;
    if (i <= 0) x = 0.0f;
    else if (i >= 128) x = 127.0f;
    else x = i - 1 + (rpm - umbral_rpm[i - 1]) / (umbral_rpm[i] - umbral_rpm[i - 1]);
    if (x < 2.0f) x = 2.0f;
    c = acelerador < 0.0f ? 0.0f : acelerador > 1.0f ? 1.0f : acelerador;
    a = (x * 0.4f + 80.0f) * (1.0f / 127.0f);
    b = a;
    if (a > 1.0f) a = b = 1.0f;
    else if (b < 0.8f) b = 0.8f;
    va = a * powf(c, 0.3f) * D_ORIG * 51.5f;
    vb = b * powf(1.0f - c, 0.25f) * D_ORIG * 51.5f;
    if (va > 127.0f) va = 127.0f;
    if (vb > 127.0f) vb = 127.0f;
    pon_grupo(G_MOTOR_ACEL, x, va * (1.0f / 127.0f));
    pon_grupo(G_MOTOR_SUELTA, x, vb * (1.0f / 127.0f));
}

void sfx_derrape(float intensidad)
{
    if (!listo) return;
    if (intensidad <= 0.01f) { calla_grupo(G_DERRAPE); return; }
    if (intensidad > 1.0f) intensidad = 1.0f;
    pon_grupo(G_DERRAPE, intensidad * 126.0f, sqrtf(0.4f + 0.6f) * D_ORIG * 151.2f * (1.0f / 127.0f));
}

void sfx_golpe(float fuerza)
{
    if (!listo) return;
    if (fuerza < 0.0f) fuerza = 0.0f;
    if (fuerza > 1.0f) fuerza = 1.0f;
    golpe_x = 1.0f;
    golpe_vol = 0.56f * (0.4f + 0.6f * fuerza);    /* fuerza 1 = el original */
}

void sfx_volumen(float v) { vol_general = v < 0.0f ? 0.0f : v; }

void sfx_para(void)
{
    int g;
    if (!listo) return;
    for (g = 0; g < N_GRUPOS; g++) calla_grupo(g);
    golpe_x = -1.0f;
}

void sfx_menu(int id)
{
    char nm[12];
    int i, c, vol;
    if (!listo) return;
    sprintf(nm, "se%03x", id & 0xfff);
    for (i = 0; i < n_mues && strncmp(mues[i].nombre, nm, 12); i++) ;
    if (i >= n_mues) return;
    c = canal_menu[menu_turno];
    menu_turno = (menu_turno + 1) % N_CANAL_MENU;
    if (c < 0) return;
    vol = (int)(mues[i].vol * (1.0f / 0x3fff) * VOL_MENU * vol_general * 255.0f + 0.5f);
    if (vol > 255) vol = 255;
    if (vol <= 0) return;
    snd_sfx_stop(c);
    manda(c, AICA_CH_CMD_START, &mues[i], base_spu[i], mues[i].frec, vol, 128);
}

/* ---------------------------------------------------------------- voces */
int sfx_voces_inicia(void)
{
    uint32_t tam, h[8], i;
    uint8_t *d;
    if (!listo || n_voz) return n_voz ? 0 : -1;
    d = gd_carga("VOCES.DAT", &tam);
    if (!d) return -1;
    memcpy(h, d, sizeof(h));    /* 'VOZ1', n, off_muestras, off_tablas, off_datos, total, idioma[8] */
    if (tam < 32 || memcmp(h, "VOZ1", 4) || h[1] > MAX_VOCES || h[5] > tam || h[4] > tam || h[3] > h[4] ||
        h[2] + 20 * h[1] > h[3]) { free(d); return -1; }
    voz_spu = snd_mem_malloc((h[5] - h[4] + 31) & ~31u);
    voz_tablas_tam = h[4] - h[3];
    voz_tablas = malloc(voz_tablas_tam);
    if (!voz_spu || !voz_tablas) { free(d); return -1; }
    spu_memload(voz_spu, d + h[4], (h[5] - h[4] + 3) & ~3u);
    memcpy(voz_tablas, d + h[3], voz_tablas_tam);
    for (i = 0; i < h[1]; i++) memcpy(&voz[i], d + h[2] + 20 * i, 20);
    memcpy(voz_idioma, d + 24, 8); voz_idioma[8] = 0;
    n_voz = h[1];
    free(d);
    canal_voz = snd_sfx_chn_alloc();
    canal_voz_se = snd_sfx_chn_alloc();
    return 0;
}

const void *sfx_voces_tablas(uint32_t *tam) { if (tam) *tam = voz_tablas_tam; return voz_tablas; }
const char *sfx_voces_idioma(void) { return voz_idioma; }

static void toca_voz(int id, int canal)
{
    int i, vol;
    muestra_dat_t m;
    if (canal < 0) return;
    for (i = 0; i < n_voz && voz[i].id != (id & 0x1ff); i++) ;
    if (i >= n_voz) return;
    vol = (int)(voz[i].vol * (1.0f / 0x3fff) * vol_general * 255.0f + 0.5f);   /* 0x3fff = el maximo del AICA */
    if (vol > 255) vol = 255;
    if (vol <= 0) return;
    if ((id & 0x1ff) != 0x18d) dbglog(DBG_INFO, "VOZ 0x%x\n", id & 0x1ff);   /* para las pruebas (consola) */
    memset(&m, 0, sizeof m);
    m.formato = 2; m.bucle = 0; m.ini = 0; m.fin = voz[i].n;
    snd_sfx_stop(canal);
    manda(canal, AICA_CH_CMD_START, &m, voz_spu + voz[i].desp, voz[i].frec, vol, 128);
}

void sfx_voz(int id) { if (n_voz) toca_voz(id, (id & 0x1ff) == 0x4d || (id & 0x1ff) == 0x18d ? canal_voz_se : canal_voz); }

void sfx_voz_calla(void)
{
    if (canal_voz >= 0) snd_sfx_stop(canal_voz);
    if (canal_voz_se >= 0) snd_sfx_stop(canal_voz_se);
}
