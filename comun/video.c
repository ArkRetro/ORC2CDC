/* video.c: peliculas .VQV (tools/conv_video.py) en la Dreamcast (ver video.h).

   Cada cuadro es una textura VQ de la PowerVR de 512x256 (libro de 2 KB + 32 KB de indices: la descomprime el chip al
   dibujar) que se dibuja estirada a 640x480, mas su trozo de audio ADPCM de Yamaha estereo a 48 kHz (intercalado por
   nibbles, como la musica). Los cuadros tienen tamano fijo (36 KB, 18 sectores) y se leen de 4 en 4 del GD por DMA
   (147 KB por lectura, ~0,9 MB/s a 25 fps) a un anillo de 12 cuadros desde un hilo; el audio de cada cuadro lleva 1 s
   de adelanto en el fichero, asi que el lector siempre va por delante de lo que suena. El audio va por un snd_stream
   propio; el cuadro que se ensena sale del reloj desde que empieza a sonar (fps del fichero), y si el GD se retrasa se
   repite el ultimo y luego se saltan los atrasados. Dos texturas en la memoria de video (68 KB): se sube el cuadro
   nuevo a la que no se esta dibujando.
   Como el original (0x25e1d8, modo ADVERTISE): la pelicula de titulo y luego la de atraccion, START las corta. */
#include <kos.h>
#include <dc/sound/stream.h>
#include <malloc.h>
#include <string.h>
#include "gd.h"
#include "musica.h"
#include "video.h"

#define N_ANILLO 12             /* cuadros en el anillo */
#define POR_LECTURA 4           /* cuadros por lectura del GD */
#define AUDIO_ANILLO (128 * 1024)
#define SALIDA_AUDIO (64 * 1024)

typedef struct {                /* cabecera de un .VQV */
    char marca[4];
    uint32_t version;
    uint16_t ancho, alto;
    uint32_t fps_mil, n, frec, audio_cuadro, adelanto, t_cuadro, off_previo, t_previo, off_cuadros;
} cab_t;

static cab_t cab;
static uint32_t lba0;
static uint8_t *anillo, *aud, *salida;
static volatile uint32_t leidos, usados, a_escrito, a_leido;
static volatile int fin_lector, error_lector;
static snd_stream_hnd_t hv = SND_STREAM_INVALID;

static void audio_mete(const uint8_t *p, uint32_t n)
{
    uint32_t i = 0;
    while (i < n) {
        uint32_t o = (a_escrito + i) % AUDIO_ANILLO, k = n - i;
        if (k > AUDIO_ANILLO - o) k = AUDIO_ANILLO - o;
        memcpy(aud + o, p + i, k); i += k;
    }
    a_escrito += n;
}

static void *lector(void *arg)
{
    uint32_t sect = cab.t_cuadro / 2048;
    (void)arg;
    while (!fin_lector && leidos < cab.n) {
        uint32_t k = leidos, n = POR_LECTURA, i;
        if (k + n > cab.n) n = cab.n - k;
        if ((k % N_ANILLO) + n > N_ANILLO) n = N_ANILLO - k % N_ANILLO;   /* sin dar la vuelta en una lectura */
        if (k + n > usados + N_ANILLO || AUDIO_ANILLO - (a_escrito - a_leido) < n * cab.audio_cuadro) { thd_sleep(5); continue; }
        if (gd_lee(lba0 + (cab.off_cuadros + k * cab.t_cuadro) / 2048, n * sect, anillo + (k % N_ANILLO) * cab.t_cuadro)) {
            error_lector = 1; thd_sleep(20); continue;
        }
        for (i = 0; i < n; i++)                                 /* el audio de cada cuadro (el de L cuadros despues) */
            audio_mete(anillo + ((k + i) % N_ANILLO) * cab.t_cuadro + 2048 + cab.ancho * cab.alto / 4, cab.audio_cuadro);
        leidos = k + n;
    }
    return NULL;
}

static void *datos_audio(snd_stream_hnd_t h, int pide, int *dados)
{
    uint32_t hay = a_escrito - a_leido, n = pide, i;
    (void)h;
    if (n > SALIDA_AUDIO) n = SALIDA_AUDIO;
    if (n > hay) n = hay & ~31u;
    if (!n) { *dados = 0; return NULL; }
    for (i = 0; i < n; ) {
        uint32_t o = (a_leido + i) % AUDIO_ANILLO, k = n - i;
        if (k > AUDIO_ANILLO - o) k = AUDIO_ANILLO - o;
        memcpy(salida + i, aud + o, k); i += k;
    }
    a_leido += n; *dados = n;
    return salida;
}

static int boton_start(void)
{
    maple_device_t *mc = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = mc ? (cont_state_t *)maple_dev_status(mc) : NULL;
    return st && (st->buttons & CONT_START);
}

static void dibuja(pvr_ptr_t tex)
{
    pvr_poly_cxt_t cxt; pvr_poly_hdr_t hdr; pvr_vertex_t v;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED | PVR_TXRFMT_VQ_ENABLE,
                     cab.ancho, cab.alto, tex, PVR_FILTER_BILINEAR);
    cxt.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof hdr);
    v.argb = 0xFFFFFFFF; v.oargb = 0; v.z = 1.0f;
    v.flags = PVR_CMD_VERTEX; v.x = 0; v.y = 480; v.u = 0; v.v = 1; pvr_prim(&v, sizeof v);
    v.y = 0; v.v = 0; pvr_prim(&v, sizeof v);
    v.x = 640; v.y = 480; v.u = 1; v.v = 1; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL; v.y = 0; v.v = 0; pvr_prim(&v, sizeof v);
}

int video_reproduce(const char *nombre)
{
    uint32_t tam, t_vq, hecho = 0;
    uint8_t *c0;
    pvr_ptr_t tex[2] = { NULL, NULL };
    kthread_t *hilo;
    int saltado = 0, start_ant = 1, cual = 0, hay = 0;
    uint64_t t0;

    if (gd_busca(nombre, &lba0, &tam)) return -1;
    c0 = memalign(32, 2048);
    if (!c0) return -1;
    if (gd_lee(lba0, 1, c0)) { free(c0); return -1; }
    memcpy(&cab, c0, sizeof cab);
    free(c0);
    if (memcmp(cab.marca, "OVQ1", 4) || cab.version != 1 || !cab.n || !cab.fps_mil || cab.t_cuadro % 2048 ||
        cab.off_cuadros % 2048 || cab.off_previo % 2048 || cab.t_previo > AUDIO_ANILLO / 2 ||
        (uint64_t)cab.off_cuadros + (uint64_t)cab.n * cab.t_cuadro > tam) return -1;
    t_vq = 2048 + cab.ancho * cab.alto / 4;
    if (t_vq + cab.audio_cuadro > cab.t_cuadro) return -1;

    anillo = memalign(32, N_ANILLO * cab.t_cuadro);
    aud = memalign(32, AUDIO_ANILLO);
    salida = memalign(32, SALIDA_AUDIO);
    if (pvr_mem_available() > 2 * t_vq + 4096) { tex[0] = pvr_mem_malloc(t_vq); tex[1] = pvr_mem_malloc(t_vq); }
    if (!anillo || !aud || !salida || !tex[0] || !tex[1]) {
        dbglog(DBG_ERROR, "VIDEO %s: sin memoria\n", nombre);
        if (tex[0]) pvr_mem_free(tex[0]);
        if (tex[1]) pvr_mem_free(tex[1]);
        free(anillo); free(aud); free(salida); anillo = aud = salida = NULL;
        return -1;
    }
    musica_para();
    leidos = usados = a_escrito = a_leido = 0; fin_lector = error_lector = 0;
    {   /* audio previo (el primer segundo) */
        uint32_t s = (cab.t_previo + 2047) / 2048;
        uint8_t *p = memalign(32, s * 2048);
        if (p && !gd_lee(lba0 + cab.off_previo / 2048, s, p)) audio_mete(p, cab.t_previo);
        free(p);
    }
    hilo = thd_create(0, lector, NULL);
    {   /* colchon: unos cuantos cuadros antes de empezar (con tope por si el GD falla) */
        int t;
        for (t = 0; t < 300 && leidos < (cab.n < 8 ? cab.n : 8); t++) thd_sleep(10);
    }
    if (hv == SND_STREAM_INVALID) hv = snd_stream_alloc(datos_audio, SND_STREAM_BUFFER_MAX);
    /* el audio de la pelicula sin atenuar (el PC lo mezcla tal cual: MenuAudio::mix, movie_), como la musica */
    if (hv != SND_STREAM_INVALID) { snd_stream_start_adpcm(hv, cab.frec, 1); snd_stream_volume(hv, 255); }
    t0 = timer_ms_gettime64();
    dbglog(DBG_INFO, "VIDEO %s: %u cuadros %ux%u a %u.%03u fps\n", nombre, (unsigned)cab.n, cab.ancho, cab.alto,
           (unsigned)(cab.fps_mil / 1000), (unsigned)(cab.fps_mil % 1000));

    for (;;) {
        uint32_t f = (uint32_t)((timer_ms_gettime64() - t0) * cab.fps_mil / 1000000);
        int st = boton_start();
        if (st && !start_ant) { saltado = 1; break; }
        start_ant = st;
        if (hv != SND_STREAM_INVALID) snd_stream_poll(hv);
        if (f >= cab.n) break;
        if (f < leidos && (!hay || f + 1 > hecho)) {            /* el cuadro de ahora ya esta leido: a la VRAM */
            cual ^= 1;
            pvr_txr_load(anillo + (f % N_ANILLO) * cab.t_cuadro, tex[cual], t_vq);
            hecho = f + 1; hay = 1;
            usados = f + 1;                                     /* los anteriores ya no hacen falta */
        } else if (leidos > f + 1 && usados < f) usados = f;   /* atrasado: se saltan */
        if (pvr_wait_ready() < 0) continue;
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        if (hay) dibuja(tex[cual]);
        pvr_list_finish();
        pvr_scene_finish();
    }
    fin_lector = 1;
    thd_join(hilo, NULL);
    if (saltado) {                                              /* que el START no lo vea tambien el titulo */
        int t;
        for (t = 0; t < 200 && boton_start(); t++) thd_sleep(10);
    }
    if (hv != SND_STREAM_INVALID) snd_stream_stop(hv);
    pvr_wait_ready();                                           /* la PowerVR ya no lee las texturas */
    pvr_mem_free(tex[0]); pvr_mem_free(tex[1]);
    free(anillo); free(aud); free(salida); anillo = aud = salida = NULL;
    dbglog(DBG_INFO, "VIDEO %s: %s en el cuadro %u (leidos %u)%s\n", nombre, saltado ? "saltado" : "fin",
           (unsigned)hecho, (unsigned)leidos, error_lector ? ", con errores del GD" : "");
    return saltado;
}

int video_atraccion(void)
{
    int r = video_reproduce("MVTS.VQV"), s;
    if (r == 1) return 1;
    s = video_reproduce("MVATTR.VQV");
    if (s == 1) return 1;
    return r < 0 && s < 0 ? -1 : 0;
}

void video_titulo(void) { video_atraccion(); }
