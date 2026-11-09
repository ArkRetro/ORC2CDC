/* video.c: peliculas .VQV version 2 (tools/conv_video.py) en la Dreamcast (ver video.h).

   Cada cuadro es una textura VQ de la PowerVR (256x256: libro de 2 KB + 16 KB de indices; la descomprime el chip al
   dibujar) que se dibuja estirada a 640x480, mas su trozo de audio ADPCM de Yamaha (un flujo por canal, 32 kHz). El
   libro va solo en el primer cuadro de cada grupo (GOP); se guarda en la RAM y se sube con los indices de cada cuadro.
   El fichero es una tira de registros de tamano variable (multiplos de 32 bytes) que un hilo lee del GD de 64 en 64
   sectores (128 KB, por DMA, siempre hacia delante) a un anillo de 512 KB-1 MB (lo que quepa); el mismo hilo separa los registros,
   pasa su audio a dos anillos (izquierdo y derecho) y apunta cada cuadro en una cola. El audio de cada cuadro lleva
   1 s de adelanto en el fichero, asi que el lector siempre va por delante de lo que suena.

   Fallo en la consola real (2026-11-08, GDEMU, PAL, RGB): "muestra el nombre del titulo, se congela y no hay audio";
   en Flycast iba bien. Lo que se ha cambiado por eso (ver docs/DREAMCAST.md, "Peliculas"):
   - caudal: de 0,9 MB/s (512x256, libro por cuadro, 48 kHz, cuadros alineados a sector) a ~446 KB/s;
   - precarga de 1,5 s de video (y 2,5 s de audio) antes de empezar, pintando en negro mientras tanto;
   - el audio va por un snd_stream con llamada directa: lo copia el SH4 a la RAM del AICA con las store queues
     (spu_memload_sq), sin el DMA del G2 ni el semaforo comun de snd_stream (stream_sem) que comparte con la musica, y
     nunca devuelve 0 bytes (si falta, rellena con silencio: con 0 snd_stream rellenaria con DMA);
   - el lector no compite con el de la musica (musica_para y musica_pausa_lectura) y va con mas prioridad que el bucle;
   - si no llega un cuadro nuevo en 1 s (o la PowerVR no termina), se para, escribe el diagnostico sobre la imagen
     (cuadros, KB/s del GD, errores, audio) para que el probador haga una foto, espera 12 s o START y sigue al titulo;
   - un hilo vigilante escribe el mismo diagnostico, con el paso en que esta, si el bucle no da senales en 1,5 s (el
     hilo principal bloqueado dentro de KallistiOS); la letra se copia de la BIOS al empezar (la fuente de la BIOS no
     se puede leer mientras el GD trabaja).
   El cuadro que se ensena sale del reloj desde que empieza a sonar (fps del fichero); si el GD se retrasa se repite el
   ultimo y luego se saltan los atrasados. Dos texturas en la memoria de video: se sube el cuadro nuevo a la que no se
   esta dibujando.
   Como el original (0x25e1d8, modo ADVERTISE): la pelicula de titulo y luego la de atraccion, START las corta. */
#include <kos.h>
#include <dc/sound/stream.h>
#include <dc/spu.h>
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#include <dc/g2bus.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include "gd.h"
#include "musica.h"
#include "video.h"

#ifndef PRUEBA_VIDEO_KBS
#define PRUEBA_VIDEO_KBS 0      /* prueba: limita el lector a tantos KB/s (simula un GD lento); 0 sin limite */
#endif
/* PRUEBA_VIDEO_COLGADO n: el bucle se queda dormido en el cuadro n (prueba del vigilante) */
#define BLOQUE_S 64             /* sectores por lectura del GD */
#define BLOQUE (BLOQUE_S * 2048)
#define NCOLA 256               /* cuadros apuntados (el anillo de 1 MB tiene sitio para ~60) */
#define AUDIO_ANILLO (64 * 1024) /* por canal: 4 s a 32 kHz (el anillo de video tiene como mucho 2,3 s, mas 1 de adelanto) */
#define BUF_STREAM 16384        /* bytes por canal del flujo en la RAM del AICA (1 s a 32 kHz) */
#define PRECARGA_MS 1500
#define ATASCO_MS 1000
#define DIAG_MS 12000

typedef struct {                /* cabecera de un .VQV version 2 */
    char marca[4];
    uint32_t version;
    uint16_t ancho, alto;
    uint32_t fps_mil, n, frec, audio_cuadro, adelanto, off_previo, t_previo, off_datos, t_datos, max_reg;
} cab_t;

static cab_t cab;
static const char *nombre_act = "";
static uint32_t lba0, R, t_idx;
static uint8_t *anillo, *aud[2];
static uint8_t *prestado; static uint32_t prestado_tam;   /* video_bufer: anillo y audio ahi, sin pedir RAM */
static int en_prestado;                                   /* los anillos de esta pelicula estan en el prestado */
void video_bufer(void *p, uint32_t tam)
{
    prestado = p && !((uintptr_t)p & 31) ? (uint8_t *)p : NULL; prestado_tam = prestado ? tam : 0;
}
static void suelta_anillos(void)
{
    if (!en_prestado) { free(anillo); free(aud[0]); free(aud[1]); }
    anillo = aud[0] = aud[1] = NULL; en_prestado = 0;
}
static uint8_t libro[2048] __attribute__((aligned(32)));
static struct { uint32_t off, tam, libro; } cola[NCOLA];
/* compartido con el hilo lector: bytes del fichero en el anillo, primer byte que aun hace falta, siguiente registro
   por separar, cuadros separados, cuadros ya usados por el bucle, bytes de audio por canal (escritos y leidos) */
static volatile uint32_t escrito, libre, p_parse, nparse, hecho, a_escrito, a_leido;
static volatile int fin_lector, lector_fin, lector_estado, error_formato;
/* medidas (diagnostico y registro) */
static volatile uint32_t m_lecturas, m_kb, m_ms_gd, m_peor_ms, m_err_lector, f_reloj, au_pedidos, au_faltas, au_kb;
static volatile uint64_t m_t_ini, m_t_ult;
static volatile int audio_en_marcha, pvr_fallos;
static snd_stream_hnd_t hv = SND_STREAM_INVALID;
static int canal[2] = { -1, -1 };          /* canales del AICA del flujo (ver enciende_canales) */
static volatile int audio_muerto;          /* el AICA no pide datos: el audio se tira para no parar el video */
/* vigilante */
static volatile const char *paso = "";
static volatile uint64_t latido;
static volatile int vigila;

/* ---------------------------------------------------------------- letra propia para el diagnostico
   12x24 de la BIOS, copiada al empezar (bfont espera al GD: desde otro hilo, con el GD colgado, se bloquearia) */
static uint16_t fuente[96][24];
static int fuente_ok;
static void fuente_prepara(void)
{
    static uint16_t t[24 * 12] __attribute__((aligned(32)));
    int c, y, x;
    if (fuente_ok) return;
    for (c = 32; c < 128; c++) {
        memset(t, 0, sizeof t);
        bfont_draw(t, 12, 0, c);
        for (y = 0; y < 24; y++) {
            uint16_t m = 0;
            for (x = 0; x < 12; x++) if (t[y * 12 + x]) m |= 0x800 >> x;
            fuente[c - 32][y] = m;
        }
    }
    fuente_ok = 1;
}
static void texto(uint16_t *fb, int x, int y, const char *s)
{
    for (; *s && x <= 640 - 12; s++, x += 12) {
        int c = (unsigned char)*s, f, i;
        if (c < 32 || c > 127) c = '?';
        for (f = 0; f < 24; f++) {
            uint16_t m = fuente[c - 32][f], *p = fb + (y + f) * 640 + x;
            for (i = 0; i < 12; i++) p[i] = (m & (0x800 >> i)) ? 0xFFFF : 0x0000;
        }
    }
}

/* el diagnostico sobre la imagen que se esta viendo (como pvr_colgado de demo/main.c) */
static void diag_pinta(const char *motivo)
{
    char l[10][96]; int i, k = 0, ult; uint32_t ne; uint64_t ahora = timer_ms_gettime64();
    uint16_t *fb = (uint16_t *)(0xa5000000 + (PVR_GET(0x0050) & 0x7ffffc));
    uint32_t t_rel = ahora > m_t_ini && m_t_ini ? (uint32_t)(ahora - m_t_ini) : 0;
    static const char *est[] = { "sin empezar", "leyendo", "anillo lleno", "fin", "error" };
    if (!fuente_ok) return;
    gd_errores(&ne, &ult);
    /* 52 letras por linea como mucho (640 / 12) */
    snprintf(l[k++], 96, "VIDEO %s: %s", nombre_act, motivo);
    snprintf(l[k++], 96, "cuadros: leidos %u vistos %u de %u reloj %u", (unsigned)nparse, (unsigned)hecho,
             (unsigned)cab.n, (unsigned)f_reloj);
    snprintf(l[k++], 96, "GD: %u KB %u lect. %u KB/s (media %u)", (unsigned)m_kb, (unsigned)m_lecturas,
             m_ms_gd ? (unsigned)(m_kb * 1000 / m_ms_gd) : 0, t_rel ? (unsigned)((uint64_t)m_kb * 1000 / t_rel) : 0);
    snprintf(l[k++], 96, "GD: peor %u ms, ultima hace %u ms", (unsigned)m_peor_ms,
             m_t_ult ? (unsigned)(ahora - m_t_ult) : 0);
    snprintf(l[k++], 96, "GD: errores %u (ult. %d), lector %u, %s", (unsigned)ne, ult, (unsigned)m_err_lector,
             est[lector_estado >= 0 && lector_estado < 5 ? lector_estado : 4]);
    snprintf(l[k++], 96, "fichero %s, anillo %u KB, pvr fallos %d", error_formato ? "MAL" : "bien", (unsigned)(R >> 10),
             pvr_fallos);
    snprintf(l[k++], 96, "audio: %s, canales %d y %d", !audio_en_marcha ? "parado" : audio_muerto ? "MUERTO" : "sonando",
             canal[0], canal[1]);
    snprintf(l[k++], 96, "audio: anillo %u KB, dado %u KB, faltas %u/%u", (unsigned)((a_escrito - a_leido) >> 10),
             (unsigned)au_kb, (unsigned)au_faltas, (unsigned)au_pedidos);
    snprintf(l[k++], 96, "Haz una foto y mandala");
    for (i = 0; i < k; i++) texto(fb, 8, 8 + i * 26, l[i]);
}

static void diag_registra(const char *motivo)
{
    int ult; uint32_t ne; uint64_t t_rel = m_t_ini ? timer_ms_gettime64() - m_t_ini : 0;
    gd_errores(&ne, &ult);
    dbglog(DBG_INFO, "VIDEO %s: %s; cuadros %u/%u de %u; GD %u KB en %u lecturas, %u KB/s leyendo, %u KB/s de media, "
           "peor %u ms; errores %u (%d), lector %u; audio %u KB, faltas %u/%u; anillo %u KB\n", nombre_act, motivo,
           (unsigned)nparse, (unsigned)hecho, (unsigned)cab.n, (unsigned)m_kb, (unsigned)m_lecturas,
           m_ms_gd ? (unsigned)(m_kb * 1000 / m_ms_gd) : 0, t_rel ? (unsigned)(m_kb * 1000 / t_rel) : 0,
           (unsigned)m_peor_ms, (unsigned)ne, ult, (unsigned)m_err_lector, (unsigned)au_kb, (unsigned)au_faltas,
           (unsigned)au_pedidos, (unsigned)(R >> 10));
}

/* si el hilo principal no da senales en 1,5 s (bloqueado dentro de KallistiOS), el diagnostico con el paso */
static void *vigilante(void *arg)
{
    char s[64]; int dicho = 0;
    (void)arg;
    while (vigila) {
        thd_sleep(200);
        if (vigila && timer_ms_gettime64() - latido > 1500) {
            snprintf(s, sizeof s, "COLGADO en '%s'", (const char *)paso);
            diag_pinta(s);
            if (!dicho) { dicho = 1; dbglog(DBG_ERROR, "VIDEO %s: %s\n", nombre_act, s); }
        }
    }
    return NULL;
}
static void vivo(const char *p) { paso = p; latido = timer_ms_gettime64(); }

/* ---------------------------------------------------------------- lector */
static void desde_anillo(uint8_t *dst, uint32_t off, uint32_t n)
{
    while (n) {
        uint32_t o = off % R, k = R - o < n ? R - o : n;
        memcpy(dst, anillo + o, k); dst += k; off += k; n -= k;
    }
}
static void audio_mete(int c, uint32_t off, uint32_t n)
{
    uint32_t i = 0;
    while (i < n) {
        uint32_t o = (a_escrito + i) % AUDIO_ANILLO, k = n - i;
        if (k > AUDIO_ANILLO - o) k = AUDIO_ANILLO - o;
        desde_anillo(aud[c] + o, off + i, k); i += k;
    }
}

/* separa los registros completos que hay en el anillo: audio a sus anillos, cuadro a la cola */
static void parsea(void)
{
    uint32_t lim = escrito < cab.t_datos ? escrito : cab.t_datos;
    while (!error_formato && nparse < cab.n && p_parse + 32 <= lim) {
        const uint32_t *h = (const uint32_t *)(anillo + p_parse % R);   /* 32 bytes alineados: no dan la vuelta */
        uint32_t tam = h[0], li = h[1] & 1, na = h[2], t_img = (li ? 2048 : 0) + t_idx;
        if (tam < 32 + t_img + 2 * na || tam > cab.max_reg || (tam & 31) || na != cab.audio_cuadro || h[3] != nparse ||
            (nparse == 0 && !li)) {
            error_formato = 1;
            break;
        }
        if (p_parse + tam > lim || nparse - hecho >= NCOLA || AUDIO_ANILLO - (a_escrito - a_leido) < na) break;
        audio_mete(0, p_parse + 32 + t_img, na);
        audio_mete(1, p_parse + 32 + t_img + na, na);
        a_escrito += na;
        cola[nparse % NCOLA].off = p_parse; cola[nparse % NCOLA].tam = tam; cola[nparse % NCOLA].libro = li;
        p_parse += tam;
        nparse++;
    }
}

static void *lector(void *arg)
{
    uint32_t total = (cab.t_datos + 2047) / 2048, s = 0;
    (void)arg;
    m_t_ini = timer_ms_gettime64();
    while (!fin_lector) {
        uint32_t n; int r; uint64_t t;
        parsea();
        if (error_formato) { lector_estado = 4; break; }
        if (s >= total) {                                       /* todo leido: queda separar lo que falte */
            lector_estado = 3;
            if (nparse >= cab.n) break;
            thd_sleep(10); continue;
        }
        n = total - s < BLOQUE_S ? total - s : BLOQUE_S;
        if (escrito + n * 2048 - libre > R) { lector_estado = 2; thd_sleep(5); continue; }
        lector_estado = 1;
        t = timer_ms_gettime64();
        /* destino alineado a 32 (R es multiplo de BLOQUE y cada lectura empieza en un multiplo de BLOQUE: nunca da
           la vuelta); la CPU no toca esa parte del anillo mientras llega */
        r = gd_lee(lba0 + cab.off_datos / 2048 + s, n, anillo + escrito % R);
        m_t_ult = timer_ms_gettime64();
        if (r) { m_err_lector++; thd_sleep(50); continue; }
        if (m_t_ult - t > m_peor_ms) m_peor_ms = m_t_ult - t;
        m_lecturas++; m_kb += n * 2; m_ms_gd += m_t_ult - t;
        escrito += n * 2048; s += n;
#if PRUEBA_VIDEO_KBS
        while (!fin_lector && timer_ms_gettime64() - m_t_ini < (uint64_t)m_kb * 1000 / PRUEBA_VIDEO_KBS) thd_sleep(5);
#endif
    }
    lector_fin = 1;
    return NULL;
}

/* ---------------------------------------------------------------- audio: llamada directa de snd_stream
   pide = bytes en total (los dos canales); left/right: direcciones en la RAM del AICA (alineadas a 32) */
static size_t audio_directo(snd_stream_hnd_t h, uintptr_t left, uintptr_t right, size_t pide)
{
    uint32_t por = pide / 2, hay = (a_escrito - a_leido) & ~31u, n = por < hay ? por : hay, c;
    (void)h;
    au_pedidos++;
    for (c = 0; c < 2; c++) {
        uintptr_t d = c ? right : left; uint32_t i = 0;
        while (i < n) {
            uint32_t o = (a_leido + i) % AUDIO_ANILLO, k = n - i;
            if (k > AUDIO_ANILLO - o) k = AUDIO_ANILLO - o;
            spu_memload_sq(d + i, aud[c] + o, k); i += k;
        }
        if (n < por) spu_memset_sq(d + n, 0x80808080, por - n);   /* silencio en ADPCM de Yamaha: +1 y -1 paso */
    }
    if (n < por) au_faltas++;
    a_leido += n; au_kb += n >> 9;
    return por * 2;
}

/* Fallo de la consola (y de Flycast) que congelaba la pelicula sin sonido: KallistiOS arranca los dos canales de un
   flujo estereo a la vez con una orden AICA_CH_START_SYNC cuyo mapa de canales es de 32 bits (aica_cmd_t.cmd_id y
   aica_sync_play del ARM): `(1ULL << ch0) | (1ULL << ch1)` se queda en 0 con canales >= 32. La musica pide su flujo al
   arrancar (canales 0 y 1), pero luego sfx.c reserva canales para los efectos, el motor y las voces, y el flujo de la
   pelicula (pedido despues) recibe los canales 33 y 34: quedan preparados con KEYONB a 0 y nunca suenan. snd_stream
   no pide datos, el anillo de audio se llena, el lector se para con el y el video se congela (con video.c de antes,
   a los ~3 s: leidos 76 cuadros, el anillo de audio de 128 KB lleno, posicion del AICA 0; visto en Flycast el
   2026-11-09). Aqui: si los canales son >= 32 se encienden a mano, como aica_sync_play (KEYONB en los dos y KEYONEX),
   cuando el ARM ya ha procesado las ordenes de arranque (cola de ordenes vacia). */
#define AICA_COLA (SPU_RAM_UNCACHED_BASE + 0x10000)    /* aica_queue_t de las ordenes: head, tail */
#define AICA_CANAL_REG(c) (0xa0700000 + (c) * 0x80)   /* registro 0 del canal c del AICA */
static void enciende_canales(void)
{
    uint64_t t = timer_ms_gettime64();
    if (canal[0] < 32 && canal[1] < 32) return;                 /* KallistiOS ya los ha encendido */
    if (canal[0] < 0 || canal[1] < 0) return;
    while (g2_read_32(AICA_COLA) != g2_read_32(AICA_COLA + 4) && timer_ms_gettime64() - t < 200) thd_sleep(1);
    thd_sleep(2);                                               /* el ARM termina la ultima orden */
    g2_write_32(AICA_CANAL_REG(canal[0]), g2_read_32(AICA_CANAL_REG(canal[0])) | 0x4000);
    g2_write_32(AICA_CANAL_REG(canal[1]), g2_read_32(AICA_CANAL_REG(canal[1])) | 0xc000);
    dbglog(DBG_INFO, "VIDEO: canales %d y %d encendidos a mano (registro %04x)\n", canal[0], canal[1],
           (unsigned)(g2_read_32(AICA_CANAL_REG(canal[0])) & 0xffff));
}

/* ---------------------------------------------------------------- reproduccion */
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

/* un cuadro (negro si tex es NULL); 0 bien, -1 la PowerVR no ha terminado el anterior */
static int cuadro(pvr_ptr_t tex)
{
    vivo("pvr_wait_ready");
    if (pvr_wait_ready() < 0) { pvr_fallos++; return -1; }
    vivo("escena");
    pvr_set_bg_color(0, 0, 0);
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    if (tex) dibuja(tex);
    pvr_list_finish();
    pvr_scene_finish();
    return 0;
}

/* el video no avanza: diagnostico sobre la ultima imagen, 12 s o START, y al titulo */
static void atasco(const char *motivo)
{
    uint64_t t = timer_ms_gettime64(); int st, ant = boton_start();
    if (audio_en_marcha) { snd_stream_stop(hv); audio_en_marcha = 0; }
    diag_registra(motivo);
    pvr_wait_ready();                                           /* que la PowerVR no cambie la imagen despues */
    while (timer_ms_gettime64() - t < DIAG_MS) {
        vivo("diagnostico");
        diag_pinta(motivo);
        st = boton_start();
        if (st && !ant) break;
        ant = st;
        thd_sleep(100);
    }
}

static int reproduce(const char *nombre, pvr_ptr_t tex[2], kthread_t **hilo)
{
    uint32_t objetivo, sp; uint64_t t0, t_nuevo, t_audio, t;
    int saltado = 0, start_ant = 1, cual = 0, hay = 0, seguidos = 0;
    uint8_t *p;

    musica_para();
    musica_pausa_lectura(1);                                    /* el GD para la pelicula sola */
    vivo("audio previo");
    sp = (2 * cab.t_previo + 2047) / 2048;                      /* 32 KB */
    p = memalign(32, sp * 2048);
    if (p && !gd_lee(lba0 + cab.off_previo / 2048, sp, p)) {
        memcpy(aud[0], p, cab.t_previo); memcpy(aud[1], p + cab.t_previo, cab.t_previo);
        a_escrito = cab.t_previo;
    }
    free(p);
    *hilo = thd_create(0, lector, NULL);
    if (*hilo) thd_set_prio(*hilo, PRIO_DEFAULT - 1);           /* que rellene en cuanto llega el DMA */
    else return 2;

    /* precarga: 1,5 s de video (o el anillo lleno), en negro; con tope por si el GD no responde */
    objetivo = (uint32_t)((uint64_t)cab.fps_mil * PRECARGA_MS / 1000000);
    if (objetivo > cab.n) objetivo = cab.n;
    t = timer_ms_gettime64();
    while (nparse < objetivo && lector_estado != 2 && !lector_fin && !error_formato) {
        int st = boton_start();
        if (st && !start_ant) return 1;
        start_ant = st;
        if (timer_ms_gettime64() - t > 10000) break;
        if (cuadro(NULL) < 0) thd_sleep(10);
    }
    dbglog(DBG_INFO, "VIDEO %s: precarga de %u cuadros en %u ms (%u KB, %u KB/s leyendo)\n", nombre, (unsigned)nparse,
           (unsigned)(timer_ms_gettime64() - t), (unsigned)m_kb, m_ms_gd ? (unsigned)(m_kb * 1000 / m_ms_gd) : 0);
    if (!nparse) { atasco(error_formato ? "formato del fichero" : "no llega nada del GD"); return 2; }

    vivo("snd_stream");
    if (hv == SND_STREAM_INVALID) {
        /* snd_stream_alloc toma los dos canales libres mas bajos: se averiguan antes (nadie mas pide canales despues
           del arranque) */
        int c0 = snd_sfx_chn_alloc(), c1 = snd_sfx_chn_alloc();
        if (c1 >= 0) snd_sfx_chn_free(c1);
        if (c0 >= 0) snd_sfx_chn_free(c0);
        hv = snd_stream_alloc(NULL, BUF_STREAM);
        if (hv != SND_STREAM_INVALID) { snd_stream_set_callback_direct(hv, audio_directo); canal[0] = c0; canal[1] = c1; }
        dbglog(DBG_INFO, "VIDEO: flujo de audio %d en los canales %d y %d del AICA\n", hv, c0, c1);
    }
    /* el audio de la pelicula sin atenuar (el PC lo mezcla tal cual: MenuAudio::mix, movie_), como la musica */
    if (hv != SND_STREAM_INVALID) {
        snd_stream_start_adpcm(hv, cab.frec, 1);
        snd_stream_volume(hv, 255);
        enciende_canales();
        audio_en_marcha = 1;
    }
    t0 = t_nuevo = t_audio = timer_ms_gettime64();

    for (;;) {
        uint64_t ahora = timer_ms_gettime64();
        uint32_t f = (uint32_t)((ahora - t0) * cab.fps_mil / 1000000), np = nparse;
        int st = boton_start();
        vivo("bucle");
        f_reloj = f;
        if (st && !start_ant) { saltado = 1; break; }
        start_ant = st;
        if (audio_en_marcha) {
            uint32_t p = au_pedidos;
            vivo("snd_stream_poll");
            snd_stream_poll(hv);
            if (p != au_pedidos) t_audio = ahora;
            else if (!audio_muerto && ahora - t_audio > 2500) {    /* el AICA no pide datos (en 2,5 s pide 5 veces) */
                audio_muerto = 1;
                dbglog(DBG_ERROR, "VIDEO %s: el AICA no pide audio (canales %d y %d); se tira\n", nombre, canal[0], canal[1]);
            }
        }
        if (!audio_en_marcha || audio_muerto) a_leido = a_escrito;  /* sin audio el lector no se para por el */
        if (f >= cab.n) break;
#ifdef PRUEBA_VIDEO_COLGADO
        if (f >= PRUEBA_VIDEO_COLGADO) { vivo("prueba: colgado"); for (;;) thd_sleep(1000); }   /* prueba del vigilante */
#endif
        if (np > hecho && f >= hecho) {                         /* hay un cuadro nuevo que ensenar */
            uint32_t k = f < np ? f : np - 1, j = k, o, n, i;
            while (!cola[j % NCOLA].libro && j > hecho) j--;     /* el libro mas reciente hasta k */
            if (cola[j % NCOLA].libro) desde_anillo(libro, cola[j % NCOLA].off + 32, 2048);
            vivo("pvr_txr_load");
            cual ^= 1;
            pvr_txr_load(libro, tex[cual], 2048);
            o = cola[k % NCOLA].off + 32 + (cola[k % NCOLA].libro ? 2048 : 0);
            for (i = 0; i < t_idx; i += n) {                     /* los indices (pueden dar la vuelta al anillo) */
                n = R - (o + i) % R; if (n > t_idx - i) n = t_idx - i;
                pvr_txr_load(anillo + (o + i) % R, (uint8_t *)tex[cual] + 2048 + i, n);
            }
            hecho = k + 1; hay = 1; t_nuevo = ahora;
            libre = cola[k % NCOLA].off + cola[k % NCOLA].tam;  /* lo anterior ya se puede pisar */
        }
        if (f >= hecho && ahora - t_nuevo > ATASCO_MS) { atasco("sin cuadros nuevos en 1 s"); return 2; }
        if (cuadro(hay ? tex[cual] : NULL) < 0) {
            if (++seguidos >= 20) { atasco("la PowerVR no termina el cuadro"); return 2; }
        } else seguidos = 0;
    }
    if (saltado) {                                              /* que el START no lo vea tambien el titulo */
        int k;
        for (k = 0; k < 200 && boton_start(); k++) { vivo("soltar START"); thd_sleep(10); }
    }
    return saltado;
}

int video_reproduce(const char *nombre)
{
    uint32_t tam, i;
    uint8_t *c0;
    pvr_ptr_t tex[2] = { NULL, NULL };
    kthread_t *hilo = NULL, *vig;
    int r;

    nombre_act = nombre;
    fuente_prepara();
    if (gd_busca(nombre, &lba0, &tam)) return -1;
    c0 = memalign(32, 2048);
    if (!c0) return -1;
    if (gd_lee(lba0, 1, c0)) { free(c0); return -1; }
    memcpy(&cab, c0, sizeof cab);
    free(c0);
    t_idx = cab.ancho * cab.alto / 4;
    if (memcmp(cab.marca, "OVQ2", 4) || cab.version != 2 || !cab.n || !cab.fps_mil || cab.ancho > 1024 ||
        cab.alto > 1024 || (cab.ancho & (cab.ancho - 1)) || (cab.alto & (cab.alto - 1)) || (t_idx & 31) ||
        cab.off_datos % 2048 || cab.off_previo % 2048 || (cab.audio_cuadro & 31) || (cab.t_previo & 31) ||
        cab.t_previo > AUDIO_ANILLO / 2 || cab.max_reg > 2 * BLOQUE ||
        (uint64_t)cab.off_datos + cab.t_datos > tam) {
        dbglog(DBG_ERROR, "VIDEO %s: no es un .VQV version 2\n", nombre);
        return -1;
    }
    /* el anillo: en el bufer que preste quien llama (video_bufer; demo/main.c presta el hueco de la union, que fuera
       de la carrera no se usa: 1 MB, ~2,3 s, y el audio detras) o, sin el, lo que quepa en el monton dejando 192 KB
       libres (pilas de los hilos, KallistiOS), de 512 KB a 1 MB. Desde los personajes (CHR.DAT) el monton del
       arranque ya no tiene 512 KB seguidos: sin el prestamo no hay pelicula (docs/DREAMCAST.md, RAM del menu) */
    if (prestado && prestado_tam >= 4 * BLOQUE + 2 * AUDIO_ANILLO) {   /* bufer prestado (video_bufer): todo en el */
        uint32_t cabe = (prestado_tam - 2 * AUDIO_ANILLO) / BLOQUE;
        R = (cabe > 8 ? 8 : cabe) * BLOQUE;
        anillo = prestado; aud[0] = prestado + R; aud[1] = aud[0] + AUDIO_ANILLO; en_prestado = 1;
    } else {
        aud[0] = memalign(32, AUDIO_ANILLO); aud[1] = memalign(32, AUDIO_ANILLO);
        struct mallinfo mi = mallinfo();
        uint32_t cabe = mi.fordblks > 192 * 1024 ? (mi.fordblks - 192 * 1024) / BLOQUE : 0;
        R = (cabe > 8 ? 8 : cabe < 4 ? 4 : cabe) * BLOQUE;
        for (anillo = NULL; R >= 4 * BLOQUE && !(anillo = memalign(32, R)); R -= BLOQUE) ;
    }
    if (pvr_mem_available() > 2 * (2048 + t_idx) + 4096) { tex[0] = pvr_mem_malloc(2048 + t_idx); tex[1] = pvr_mem_malloc(2048 + t_idx); }
    if (!anillo || !aud[0] || !aud[1] || !tex[0] || !tex[1]) {
        struct mallinfo mi = mallinfo();
        dbglog(DBG_ERROR, "VIDEO %s: sin memoria (anillo %p de %u KB, audio %p %p, VRAM %p %p; RAM libre %u KB, "
               "mayor hueco ~%u KB; VRAM libre %u KB)\n", nombre, anillo, (unsigned)(R >> 10), aud[0], aud[1], tex[0],
               tex[1], (unsigned)(mi.fordblks >> 10), (unsigned)(mi.fordblks >> 10), (unsigned)(pvr_mem_available() >> 10));
        if (tex[0]) pvr_mem_free(tex[0]);
        if (tex[1]) pvr_mem_free(tex[1]);
        suelta_anillos();
        return -1;
    }
    for (i = 0; i < 2; i++) memset(aud[i], 0x80, AUDIO_ANILLO);
    escrito = libre = p_parse = nparse = hecho = a_escrito = a_leido = 0;
    fin_lector = lector_fin = lector_estado = error_formato = 0;
    m_lecturas = m_kb = m_ms_gd = m_peor_ms = m_err_lector = f_reloj = au_pedidos = au_faltas = au_kb = 0;
    m_t_ini = m_t_ult = 0; audio_en_marcha = pvr_fallos = audio_muerto = 0;
    dbglog(DBG_INFO, "VIDEO %s: %u cuadros %ux%u a %u.%03u fps, audio %u Hz, %u KB/s, anillo %u KB\n", nombre,
           (unsigned)cab.n, cab.ancho, cab.alto, (unsigned)(cab.fps_mil / 1000), (unsigned)(cab.fps_mil % 1000),
           (unsigned)cab.frec, (unsigned)((uint64_t)cab.t_datos * cab.fps_mil / cab.n / 1024000), (unsigned)(R >> 10));

    vivo("inicio");
    vigila = 1;
    vig = thd_create(0, vigilante, NULL);
    if (vig) thd_set_prio(vig, PRIO_DEFAULT - 2);

    r = reproduce(nombre, tex, &hilo);

    vivo("fin: esperando al lector");
    fin_lector = 1;
    if (hilo) thd_join(hilo, NULL);
    vivo("fin: audio");
    if (audio_en_marcha) { snd_stream_stop(hv); audio_en_marcha = 0; }
    musica_pausa_lectura(0);
    vivo("fin: pvr");
    pvr_wait_ready();                                           /* la PowerVR ya no lee las texturas */
    vigila = 0;
    if (vig) thd_join(vig, NULL);
    pvr_mem_free(tex[0]); pvr_mem_free(tex[1]);
    suelta_anillos();
    diag_registra(r == 1 ? "saltado" : r == 2 ? "atascado" : "fin");
    return r;
}

int video_atraccion(void)
{
    int r = video_reproduce("MVTS.VQV"), s;
    if (r == 1 || r == 2) return 1;                             /* START o atascado: al titulo */
    s = video_reproduce("MVATTR.VQV");
    if (s == 1 || s == 2) return 1;
    return r < 0 && s < 0 ? -1 : 0;
}

/* al arrancar: primero la animacion del titulo (el logo, MV.PS2 ts50) y despues la pelicula de atraccion (los
   coches, attr50), el mismo orden que video_atraccion; START (o un atasco, tras el diagnostico) pasa al titulo. Por
   inactividad en el titulo, video_atraccion repite titulo y atraccion, como el modo de atraccion del original. */
void video_titulo(void)
{
    int r = video_reproduce("MVTS.VQV");
    if (r == 1 || r == 2) return;
    video_reproduce("MVATTR.VQV");
}
