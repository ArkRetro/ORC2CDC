/* Musica por streaming desde el GD: ficheros MUSn.ADP (ADPCM de Yamaha estereo intercalado por nibbles, 48 kHz) o
   MUSn.PCM (16 bits estereo intercalado a 44,1 kHz; tools/c/musica.c con PCM=1). Un hilo lee del GD por DMA en trozos de 32 KB a un anillo de 1 MB (unos 21 s de ADPCM a 48 KB/s; 6 s de PCM);
   snd_stream de KallistiOS lo vacia desde musica_poll (una vez por cuadro). La pista vuelve a empezar al acabar. */
#include <kos.h>
#include <dc/sound/stream.h>
#include "gd.h"
#include "musica.h"

#define ANILLO (1024 * 1024)
#define TROZO (32 * 1024)
static uint8_t anillo[ANILLO] __attribute__((aligned(32)));
static uint8_t salida[2 * TROZO] __attribute__((aligned(32)));
static uint8_t lectura[TROZO] __attribute__((aligned(32)));
static volatile uint32_t escrito, leido;                     /* bytes totales escritos y leidos del anillo */
static volatile int activa, pausa_lectura, fin_hilo;
static uint32_t lba0, tam, pos;
static snd_stream_hnd_t hs = SND_STREAM_INVALID;
static kthread_t *hilo;
static int volumen = 200, adpcm;
static mutex_t m_poll = MUTEX_INITIALIZER;                   /* snd_stream_poll desde el bucle o desde el lector */

static void *lector(void *arg)
{
    (void)arg;
    gd_hilo_fondo();
    while (!fin_hilo) {
        /* el anillo tiene ~21 s de ADPCM (48 KB/s): durante una carga (pausa_lectura o lecturas de otro hilo hace
           menos de 300 ms) no se lee; con el anillo casi vacio se lee igualmente */
        int vacio = escrito - leido < 128 * 1024;
        if (activa && pausa_lectura) {                          /* carga larga: el bucle no llama a musica_poll */
            mutex_lock(&m_poll); if (activa) snd_stream_poll(hs); mutex_unlock(&m_poll);
        }
        if (!activa || ANILLO - (escrito - leido) < TROZO || (!vacio && (pausa_lectura || gd_ocupado(300)))) { thd_sleep(10); continue; }
        {
            uint32_t n = TROZO, i, w;
            if (pos + n > tam) n = (tam - pos + 2047) & ~2047u;
            if (gd_lee(lba0 + pos / 2048, n / 2048, lectura)) { thd_sleep(50); continue; }
            if (pos + n > tam) n = tam - pos;
            for (i = 0; i < n; ) {                              /* al anillo (puede dar la vuelta) */
                uint32_t o = (escrito + i) % ANILLO, k = n - i;
                if (k > ANILLO - o) k = ANILLO - o;
                memcpy(anillo + o, lectura + i, k); i += k;
            }
            w = escrito + n; escrito = w;
            pos += n; if (pos >= tam) pos = 0;
        }
    }
    return NULL;
}

static void *datos(snd_stream_hnd_t h, int pide, int *dados)
{
    uint32_t hay = escrito - leido, n = pide, i;
    (void)h;
    if (n > 2 * TROZO) n = 2 * TROZO;
    if (n > hay) n = hay & ~31u;
    if (!n) { *dados = 0; return NULL; }
    for (i = 0; i < n; ) {
        uint32_t o = (leido + i) % ANILLO, k = n - i;
        if (k > ANILLO - o) k = ANILLO - o;
        memcpy(salida + i, anillo + o, k); i += k;
    }
    leido += n; *dados = n;
    return salida;
}

void musica_inicia(void)
{
    snd_stream_init();
    hs = snd_stream_alloc(datos, SND_STREAM_BUFFER_MAX);
    hilo = thd_create(0, lector, NULL);
}

int musica_pon(int n)
{
    char nom[16];
    musica_para();
    thd_sleep(30);
    sprintf(nom, "MUS%d.ADP", n); adpcm = 1;                     /* ADPCM de Yamaha a 48 kHz o, si no, PCM */
    if (gd_busca(nom, &lba0, &tam)) {
        sprintf(nom, "MUS%d.PCM", n); adpcm = 0;
        if (gd_busca(nom, &lba0, &tam)) return -1;
    }
    pos = 0; escrito = leido = 0; activa = 1;
    {   /* colchon antes de sonar (con tope: si el GD falla no se queda aqui) */
        int t;
        for (t = 0; t < 300 && escrito < (adpcm ? 64 : 256) * 1024 && activa; t++) thd_sleep(10);
    }
    if (adpcm) snd_stream_start_adpcm(hs, 48000, 1);
    else snd_stream_start(hs, 44100, 1);
    snd_stream_volume(hs, volumen);
    return 0;
}

void musica_para(void) { if (activa) { mutex_lock(&m_poll); snd_stream_stop(hs); activa = 0; mutex_unlock(&m_poll); } }
void musica_poll(void) { if (activa) { mutex_lock(&m_poll); snd_stream_poll(hs); mutex_unlock(&m_poll); } }
void musica_pausa_lectura(int p) { pausa_lectura = p; }
