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
   - Cada capa (0x11b600 del ELF): volumen = canal/144 * curva_vol(x)/127 * 2^-((15-aten)/2) * factor de la
     definicion (0x352748: motor 1,75, derrape 2, golpe 1) * general/144, de 0 a 0x3fff en la SPU2 (255 en el AICA,
     cuyo volumen es lineal: TL = 16 log2(255 / v) en el controlador de KallistiOS); tono = frecuencia de la muestra *
     2^((curva_pitch(x)/3 - nota*115)/4096) con curva_pitch = semitonos*1024 + fino*16. Las curvas se interpolan
     linealmente en el tramo x0 < x <= x1. Una capa con volumen 0 se apaga y vuelve a empezar la muestra al
     reaparecer (como el original).
   Aqui x es real (interpolando entre umbrales de rpm) para que el tono no vaya a saltos.
   Volumen general (sfx_master, el 0x11ae38 del original = trunc(opcion de efectos / 10 * 144), 144 por defecto): los
   ICS lo usan / 144 y los efectos SE (menus y voces) como el IRX, (vol * general) >> 7 (ver vol_se).
   Menus: sfx_menu(id) toca la muestra "se%03x" una vez (sin bucle) a su frecuencia y con el volumen de la tabla SE del
   IRX (u16 final de la muestra en SFX.DAT), alternando entre dos canales para que un efecto no corte al anterior.
   Voces (VOCES.DAT, tools/conv_sonido.py): las muestras del banco de voz del idioma (ORC.PAK y PERO.pak) y dos efectos
   de carrera (0x4d EXTEND TIME y 0x18d aviso de poco tiempo) a la RAM del AICA al arrancar (214 KB en espanol: caben
   junto a SFX.DAT y los buferes de la musica). sfx_voz(id) toca la del id a su frecuencia (12 kHz) con el volumen de
   la tabla SE del IRX (0x3fff) por el general, como los efectos de menu (vol_se: 0,875 del maximo con las opciones
   por defecto; la musica va al maximo, musica.c); las voces van por un canal propio (una voz nueva corta
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
#define MAX_MUESTRAS 48       /* ICS + menus + golpes entre coches (38 en SFX.DAT) */
#define D_ORIG 0.37f            /* factor de volumen del original tras su rampa inicial */
#define CANAL_MAX 144.0f        /* 0x11b768: el volumen del canal (0..127 de la carrera) se divide por 144 */
#define MASTER_MAX 144.0f       /* 0x11b7b0: volumen general (0x530fb4, 0x11ae38) / 144 */

enum { G_MOTOR_ACEL, G_MOTOR_SUELTA, G_DERRAPE, G_GOLPE, N_GRUPOS };
static const char *nombres_grupo[N_GRUPOS] = { "motor_acel", "motor_suelta", "derrape", "golpe" };
/* factor de cada definicion ICS (tabla de floats del ELF 0x352748, indice = definicion; 0x11b6f8): motor con y sin
   gas (definiciones 18 y 19) 1,75; derrape (0) 2,0; golpe contra muro (3) 1,0 */
static const float F_DEF[N_GRUPOS] = { 1.75f, 1.75f, 2.0f, 1.0f };

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
static int master = 144;        /* volumen general del controlador de sonido (0..144; 144 = opcion de efectos a 10) */
#ifndef PRUEBA_MEZCLA
#define PRUEBA_MEZCLA 0         /* prueba de la mezcla: bit 1 calla los ICS (motor...), bit 2 los SE (menus, voces) */
#endif
static float golpe_x = -1.0f, golpe_vol;
static int listo;
#define N_CANAL_MENU 4          /* un golpe entre coches suena hasta 3 efectos en el mismo cuadro (4F65D0, 476040, 4F67A0) */
static int canal_menu[N_CANAL_MENU] = { -1, -1, -1, -1 }, menu_turno;
#define MAX_VOCES 160                                           /* VOCESH.DAT (INFARTO): 117 */

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

/* volumen de un efecto SE (menus, voces) en el AICA (0..255): SOUNDCD.IRX 0x2118 / 0x2380 lo multiplica por el
   general (0x4b74) y desplaza 7: (vol * master) >> 7. Con la opcion a 10 (master 144) y vol 0x3fff sale 0x47ff, que
   no cabe en los 15 bits con signo del volumen de la SPU2 (sceSdSetParam lo escribe tal cual): suena con la fase
   invertida y |0x47ff - 0x8000| / 0x4000 = 0,875 del maximo. Se pasa la amplitud resultante. */
static int vol_se(int vol)
{
    int s = (vol * master) >> 7, a;
    if (PRUEBA_MEZCLA & 4) return 0;
    s &= 0x7fff;
    a = s < 0x4000 ? s : 0x8000 - s;
    a = (a * 255 + 0x2000) / 0x4000;
    return a > 255 ? 255 : a;
}

/* lleva una capa a (x, volumen del canal 0..1 = canal / 144). 0x11b600: volumen de la SPU2 = 0x3fff * canal / 144 *
   curva / 127 * 2^-((15 - aten) / 2) * factor de la definicion * general / 144 (hasta 0x3fff); en el AICA, 255 */
static void pon_capa(capa_t *k, float x, float vcanal, float fdef)
{
    const muestra_dat_t *m = &mues[k->c->muestra];
    float g = vcanal * curva_vol(k->c->vol, x) * (1.0f / 127.0f) * k->ganancia * fdef * ((float)master / MASTER_MAX) *
              vol_general;
    int vol = (int)(g * 255.0f + 0.5f);
    uint32_t frec;
    if (k->canal < 0) return;
    if (PRUEBA_MEZCLA & 2) vol = 0;
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
    for (i = 0; i < grupos[g].n; i++) pon_capa(&grupos[g].capa[i], x, vcanal, F_DEF[g]);
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
    pon_grupo(G_MOTOR_ACEL, x, va * (1.0f / CANAL_MAX));
    pon_grupo(G_MOTOR_SUELTA, x, vb * (1.0f / CANAL_MAX));
}

void sfx_derrape(float intensidad)
{
    if (!listo) return;
    if (intensidad <= 0.01f) { calla_grupo(G_DERRAPE); return; }
    if (intensidad > 1.0f) intensidad = 1.0f;
    pon_grupo(G_DERRAPE, intensidad * 126.0f, sqrtf(0.4f + 0.6f) * D_ORIG * 151.2f * (1.0f / CANAL_MAX));
}

void sfx_golpe(float fuerza)
{
    if (!listo) return;
    if (fuerza < 0.0f) fuerza = 0.0f;
    if (fuerza > 1.0f) fuerza = 1.0f;
    golpe_x = 1.0f;
    golpe_vol = 0.56f * 127.0f / CANAL_MAX * (0.4f + 0.6f * fuerza);    /* fuerza 1 = el original (canal 0,56 * 127) */
}

void sfx_volumen(float v) { vol_general = v < 0.0f ? 0.0f : v; }
void sfx_master(int m) { master = m < 0 ? 0 : m > 0x7fff ? 0x7fff : m; }

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
    vol = (int)(vol_se(mues[i].vol) * vol_general + 0.5f);
    if (vol > 255) vol = 255;
    if (vol <= 0) return;
    snd_sfx_stop(c);
    manda(c, AICA_CH_CMD_START, &mues[i], base_spu[i], mues[i].frec, vol, 128);
}

/* ---------------------------------------------------------------- voces */
/* lee un VOZ1 (VOCES.DAT o VOCESH.DAT) en el AICA; buf/cap: RAM prestada para el fichero (NULL: del monton). Las
   tablas de cuando habla se quedan con las del primero que las traiga (VOCESH.DAT no lleva) */
static int voces_lee(const char *fich, void *buf, uint32_t cap)
{
    uint32_t tam = 0, h[8], i, n;
    uint8_t *d;
    if (buf) { if (gd_carga_en(fich, buf, cap, &tam)) return -1; d = buf; }
    else if (!(d = gd_carga(fich, &tam))) return -1;
    memcpy(h, d, sizeof(h));    /* 'VOZ1', n, off_muestras, off_tablas, off_datos, total, idioma[8] */
    if (tam < 32 || memcmp(h, "VOZ1", 4) || h[1] > MAX_VOCES || h[5] > tam || h[4] > tam || h[3] > h[4] ||
        h[2] + 20 * h[1] > h[3]) { if (!buf) free(d); return -1; }
    if (voz_spu) { sfx_voz_calla(); snd_mem_free(voz_spu); voz_spu = 0; n_voz = 0; }
    voz_spu = snd_mem_malloc((h[5] - h[4] + 31) & ~31u);
    if (!voz_spu) { if (!buf) free(d); dbglog(DBG_ERROR, "VOZ: %s no cabe en el AICA\n", fich); return -1; }
    if (!voz_tablas && h[4] > h[3] + 32) {
        voz_tablas_tam = h[4] - h[3];
        if (!(voz_tablas = malloc(voz_tablas_tam))) { if (!buf) free(d); return -1; }
        memcpy(voz_tablas, d + h[3], voz_tablas_tam);
    }
    spu_memload(voz_spu, d + h[4], (h[5] - h[4] + 3) & ~3u);
    n = h[1];
    for (i = 0; i < n; i++) memcpy(&voz[i], d + h[2] + 20 * i, 20);
    memcpy(voz_idioma, d + 24, 8); voz_idioma[8] = 0;
    n_voz = n;
    if (!buf) free(d);
    dbglog(DBG_INFO, "VOZ: %s, %u muestras, %u KB en el AICA\n", fich, (unsigned)n, (unsigned)((h[5] - h[4]) >> 10));
    return 0;
}
static const char *voz_fich;
int sfx_voces_inicia(void)
{
    if (!listo || n_voz) return n_voz ? 0 : -1;
    if (voces_lee("VOCES.DAT", NULL, 0)) return -1;
    voz_fich = "VOCES.DAT";
    canal_voz = snd_sfx_chn_alloc();
    canal_voz_se = snd_sfx_chn_alloc();
    return 0;
}
/* el juego de voces del modo (el original pide otros bancos por variante: ORC/PERO en OutRun, HC/PERH/HAM en el
   INFARTO); solo relee si cambia */
int sfx_voces_juego(const char *fich, void *buf, uint32_t cap)
{
    if (!listo || !voz_fich) return -1;
    if (!strcmp(voz_fich, fich)) return 0;
    if (voces_lee(fich, buf, cap)) { voces_lee(voz_fich, buf, cap); return -1; }
    voz_fich = fich;
    return 0;
}
static int voz_grupo = -1;
void sfx_voz_grupo(int g) { voz_grupo = g; }

const void *sfx_voces_tablas(uint32_t *tam) { if (tam) *tam = voz_tablas_tam; return voz_tablas; }
const char *sfx_voces_idioma(void) { return voz_idioma; }

static void toca_voz(int id, int canal)
{
    int i, vol;
    muestra_dat_t m;
    if (canal < 0) return;
    i = n_voz;
    if (voz_grupo >= 0) {                                       /* la del banco de la etapa (HAM: id + 0x200 (g + 1)) */
        int k = (id & 0x1ff) + 0x200 * (voz_grupo + 1);
        for (i = 0; i < n_voz && voz[i].id != k; i++) ;
    }
    if (i >= n_voz) for (i = 0; i < n_voz && voz[i].id != (id & 0x1ff); i++) ;
    if (i >= n_voz) return;
    vol = (int)(vol_se(voz[i].vol) * vol_general + 0.5f);   /* como los efectos SE (vol_se) */
    if (vol > 255) vol = 255;
    if (vol <= 0) return;
    if ((id & 0x1ff) != 0x18d) dbglog(DBG_INFO, "VOZ 0x%x\n", id & 0x1ff);   /* para las pruebas (consola) */
    memset(&m, 0, sizeof m);
    m.formato = 2; m.bucle = 0; m.ini = 0; m.fin = voz[i].n;
    snd_sfx_stop(canal);
    manda(canal, AICA_CH_CMD_START, &m, voz_spu + voz[i].desp, voz[i].frec, vol, 128);
}

void sfx_voz(int id) { if (n_voz) toca_voz(id, (id & 0x1ff) == 0x4d || (id & 0x1ff) == 0x18d ? canal_voz_se : canal_voz); }
void sfx_voz_se(int id) { if (n_voz) toca_voz(id, canal_voz_se); }   /* efectos de los bancos de voz (424940 directo) */

/* una muestra suelta (ADPCM de Yamaha) por el canal de efectos de las voces: el efecto 0x96 de la escena de la meta
   (demo/meta.c). sfx_muestra_sube la deja en el AICA (0 si no cabe); vol: el de la tabla SE del IRX */
uint32_t sfx_muestra_sube(const void *adpcm, uint32_t bytes)
{
    uint32_t a;
    if (!listo || !bytes) return 0;
    if (!(a = snd_mem_malloc((bytes + 31) & ~31u))) return 0;
    spu_memload(a, (void *)adpcm, (bytes + 3) & ~3u);
    return a;
}
void sfx_muestra_baja(uint32_t a) { if (a) { if (canal_voz_se >= 0) snd_sfx_stop(canal_voz_se); snd_mem_free(a); } }
void sfx_muestra_toca(uint32_t a, uint32_t muestras, uint32_t hz, int vol_irx)
{
    muestra_dat_t m; int vol;
    if (!listo || !a || canal_voz_se < 0) return;
    vol = (int)(vol_se(vol_irx) * vol_general + 0.5f);
    if (vol > 255) vol = 255;
    if (vol <= 0) return;
    memset(&m, 0, sizeof m);
    m.formato = 2; m.bucle = 0; m.ini = 0; m.fin = muestras;
    snd_sfx_stop(canal_voz_se);
    manda(canal_voz_se, AICA_CH_CMD_START, &m, a, hz, vol, 128);
}

void sfx_voz_calla(void)
{
    if (canal_voz >= 0) snd_sfx_stop(canal_voz);
    if (canal_voz_se >= 0) snd_sfx_stop(canal_voz_se);
}
