/* Escritura propia de ficheros en la VMU y pantalla de diagnostico (revision 44; ver vmuesc.h y docs/DREAMCAST.md,
   "Guardado en la VMU").

   Por que no vmufs_write / vmu_block_write de KallistiOS (kernel/arch/dreamcast/hardware/maple/vmu.c):
   - vmu_block_write_internal no mira la respuesta de la confirmacion (MAPLE_COMMAND_BSYNC, "get last error" con la
     fase 4) y, si una fase de 128 bytes falla, sigue mandando las demas y la confirmacion igual (una VMU real ocupada
     grabando la flash del bloque anterior puede no contestar a la primera fase);
   - cada respuesta se espera 100 ms (a 50 Hz son 5 vblank: las tramas solo salen en el vblank) y, si no llega, la
     trama se marca libre sin sacarla de la cola de maple;
   - cuatro intentos por bloque con 100 ms fijos entre medias, sin releer lo escrito; vmufs_write escribe los bloques,
     la FAT y el directorio y solo dice -4 / -5 / -6, sin la respuesta maple ni el bloque.
   Aqui cada orden se espera hasta 1 s (en tramos de 20 ms, sin la carrera entre la cola y genwait), si no vuelve se
   saca de la cola, se miran las respuestas de las 4 fases y de la confirmacion, y cada bloque se relee y se compara;
   un bloque que falla se repite (VE_R_BLOQUE veces, con pausas de 30, 60, 120, 240, 480 ms). Los datos van primero a
   bloques libres (el fichero viejo sigue entero hasta que se graban la FAT y el directorio); solo si no hay sitio se
   reutilizan los del fichero viejo. Todo bajo el mutex de vmufs (fs_vmu / vmufs_read no se cruzan). */
#include <kos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <dc/maple/controller.h>
#include <dc/vmufs.h>
#include <dc/biosfont.h>
#include <dc/pvr.h>
#include <dc/video.h>
#include <kos/genwait.h>
#include "vmuesc.h"

#define BLK 512
#define VE_T_ORDEN 1000     /* ms que se espera cada respuesta */
#define VE_R_BLOQUE 6       /* intentos por bloque */
#define VE_R_LECT 3         /* intentos por lectura */

static uint32_t blkid(int b, int fase) { return ((uint32_t)(b & 0xff) << 24) | ((uint32_t)((b >> 8) & 0xff) << 16) | (fase << 8); }

/* la trama vuelve: se queda tomada (UNSENT) para la orden siguiente, como en KallistiOS */
static void vuelta(maple_state_t *st, maple_frame_t *f) { (void)st; f->state = MAPLE_FRAME_UNSENT; genwait_wake_all(f); }

/* con la trama de la unidad tomada: manda una orden y espera la respuesta; devuelve la respuesta maple o VE_M_TIEMPO */
static int orden(maple_device_t *dev, int cmd, const uint32_t *par, int n, int *ms)
{
    maple_frame_t *f = &dev->frame; uint64_t t0 = timer_ms_gettime64(); int fuera = 0; irq_mask_t s;
    maple_frame_init(f);
    if (n) memcpy(f->send_buf, par, 4 * n);
    f->cmd = cmd; f->dst_port = dev->port; f->dst_unit = dev->unit; f->length = n; f->callback = vuelta;
    maple_queue_frame(f);
    while (f->queued && timer_ms_gettime64() - t0 < VE_T_ORDEN) genwait_wait(f, "vmuesc", 20);
    s = irq_disable();
    if (f->queued || f->state != MAPLE_FRAME_UNSENT) {      /* no volvio: fuera de la cola */
        if (f->queued) maple_queue_remove(f);
        f->state = MAPLE_FRAME_UNSENT; fuera = 1;
    }
    irq_restore(s);
    if (ms) *ms = (int)(timer_ms_gettime64() - t0);
    if (fuera) { thd_sleep(40); return VE_M_TIEMPO; }       /* (por si la DMA en vuelo aun escribe la respuesta) */
    return ((maple_response_t *)f->recv_buf)->response;
}

/* lee un bloque: 0 bien, si no la respuesta maple (o VE_M_*) */
static int lee_1(maple_device_t *dev, int b, uint8_t *out, int *ms)
{
    uint32_t par[2] = { MAPLE_FUNC_MEMCARD, blkid(b, 0) }; int r;
    maple_frame_lock(&dev->frame);
    r = orden(dev, MAPLE_COMMAND_BREAD, par, 2, ms);
    if (r == MAPLE_RESPONSE_DATATRF) {
        maple_response_t *resp = (maple_response_t *)dev->frame.recv_buf;
        const uint32_t *d = (const uint32_t *)resp->data;
        if (resp->data_len >= 2 + BLK / 4 && d[0] == MAPLE_FUNC_MEMCARD && d[1] == par[1]) { memcpy(out, d + 2, BLK); r = 0; }
        else r = VE_M_RARO;
    }
    dev->frame.state = MAPLE_FRAME_VACANT;
    return r;
}
static int lee(maple_device_t *dev, int b, uint8_t *out, vmuesc_diag_t *d)
{
    int k, r = 0;
    for (k = 0; k < VE_R_LECT; k++) {
        if (k) { thd_sleep(30 * k); if (d) d->reintentos++; }
        if (!(r = lee_1(dev, b, out, NULL))) return 0;
    }
    if (d) { d->maple = r; d->paso = 5; d->bloque = b; }
    return -1;
}

/* escribe un bloque una vez: 4 fases de 128 bytes y la confirmacion (BSYNC con la fase 4); 0 bien, si no la respuesta
   maple mala y *paso la orden (0..4). resp / ms (6 cada uno, pueden ser NULL): respuesta y tiempo de cada orden */
static int escribe_1(maple_device_t *dev, int b, const uint8_t *buf, int *paso, int *resp, int *ms)
{
    uint32_t par[2 + 128 / 4]; int f, r = 0;
    maple_frame_lock(&dev->frame);
    for (f = 0; f < 5; f++) {
        par[0] = MAPLE_FUNC_MEMCARD; par[1] = blkid(b, f);
        if (f < 4) { memcpy(par + 2, buf + 128 * f, 128); r = orden(dev, MAPLE_COMMAND_BWRITE, par, 2 + 128 / 4, ms ? ms + f : NULL); }
        else r = orden(dev, MAPLE_COMMAND_BSYNC, par, 2, ms ? ms + f : NULL);
        if (resp) resp[f] = r;
        if (r != MAPLE_RESPONSE_OK) { if (paso) *paso = f; break; }
        r = 0;
    }
    dev->frame.state = MAPLE_FRAME_VACANT;
    return r;
}

/* escribe un bloque, lo relee y lo compara; con reintentos. 0 bien */
static uint8_t comp[BLK] __attribute__((aligned(32)));
static int escribe(maple_device_t *dev, int b, const uint8_t *buf, vmuesc_diag_t *d)
{
    int k, j, r = 0, paso = 0;
    for (k = 0; k < VE_R_BLOQUE; k++) {
        if (k) { d->reintentos++; thd_sleep(30 << (k - 1)); }
        paso = 0;
        if ((r = escribe_1(dev, b, buf, &paso, NULL, NULL))) continue;
        for (j = 0; j < VE_R_LECT; j++) { if (j) thd_sleep(30 * j); if (!(r = lee_1(dev, b, comp, NULL))) break; }
        if (r) { paso = 5; continue; }
        if (memcmp(comp, buf, BLK)) { r = VE_M_DISTINTO; paso = 6; continue; }
        return 0;
    }
    d->maple = r; d->paso = paso; d->bloque = b;
    return -1;
}

static int raiz_valida(const vmu_root_t *rz)
{
    int i;
    for (i = 0; i < 16; i++) if (rz->magic[i] != 0x55) return 0;
    return rz->fat_size == 1 && rz->fat_loc < 256 && rz->dir_size >= 1 && rz->dir_size <= 64 &&
           rz->dir_loc < 256 && rz->dir_loc + 1 >= rz->dir_size && rz->blk_cnt >= 1 && rz->blk_cnt <= 256;
}

int vmuesc_escribe(maple_device_t *dev, const char *nombre, const uint8_t *buf, int nb, vmuesc_diag_t *d)
{
    static vmu_root_t rz __attribute__((aligned(32)));
    static uint16_t fat[BLK / 2] __attribute__((aligned(32)));
    vmu_dir_t *dir = NULL, nd; int nbl = nb / BLK, i, n = 0, idx = -1, hueco = -1, ndir, k, r = 0, b, g, *lista = NULL;
    uint8_t *lei = NULL; uint64_t t0 = timer_ms_gettime64();
    d->cod = 0; d->fase = '-'; d->libres = -1; d->viejo = 0; d->necesarios = nbl; d->maple = 0; d->paso = -1;
    d->bloque = -1; d->reintentos = 0; d->puerto = dev ? dev->port : -1; d->unidad = dev ? dev->unit : -1; d->ms = 0;
    if (irq_inside_int()) return d->cod = VE_IRQ;
    if (!dev || !(dev->info.functions & MAPLE_FUNC_MEMCARD)) return d->cod = VE_SIN_VMU;
    if (nbl <= 0 || nb % BLK) return d->cod = VE_MEMORIA;
    vmufs_mutex_lock();
    d->fase = 'R';
    if (lee(dev, 255, (uint8_t *)&rz, d)) { r = VE_RAIZ; goto fin; }
    if (!raiz_valida(&rz)) { r = VE_FORMATO; goto fin; }
    ndir = rz.dir_size * (BLK / sizeof(vmu_dir_t));
    dir = malloc(rz.dir_size * BLK); lista = malloc(sizeof(int) * nbl); lei = malloc(BLK);
    if (!dir || !lista || !lei) { r = VE_MEMORIA; goto fin; }
    d->fase = 'T';
    if (lee(dev, rz.fat_loc, (uint8_t *)fat, d)) { r = VE_FAT; goto fin; }
    d->fase = 'D';
    for (i = 0; i < rz.dir_size; i++)                       /* el directorio va hacia atras desde dir_loc */
        if (lee(dev, rz.dir_loc - i, (uint8_t *)dir + i * BLK, d)) { r = VE_DIR; goto fin; }
    for (i = 0; i < ndir; i++) {
        if (dir[i].filetype == VMU_FILE_NONE) { if (hueco < 0) hueco = i; }
        else if (idx < 0 && !strncmp(dir[i].filename, nombre, VMU_FILENAME_SIZE)) idx = i;
    }
    for (d->libres = 0, i = 0; i < rz.blk_cnt; i++) if (fat[i] == VMU_FAT_UNALLOCATED) d->libres++;
    d->fase = 'S';
    if (idx >= 0) d->viejo = dir[idx].filesize;
    if (d->libres + d->viejo < nbl) { r = VE_SITIO; goto fin; }
    if (idx < 0 && hueco < 0) { r = VE_DIR_LLENO; goto fin; }
    /* bloques: los libres de arriba abajo (como vmufs para los ficheros de datos); si no bastan, los del viejo */
    for (i = rz.blk_cnt - 1; i >= 0 && n < nbl; i--) if (fat[i] == VMU_FAT_UNALLOCATED) lista[n++] = i;
    for (b = idx >= 0 ? dir[idx].firstblk : VMU_FAT_LAST_IN_FILE, g = 0; n < nbl && b != VMU_FAT_LAST_IN_FILE; g++) {
        if (b >= rz.blk_cnt || g > 255) { r = VE_FORMATO; goto fin; }
        lista[n++] = b; b = fat[b];
    }
    if (n < nbl) { r = VE_SITIO; goto fin; }
    d->fase = 'B';
    for (i = 0; i < nbl; i++) if (escribe(dev, lista[i], buf + i * BLK, d)) { r = VE_BLOQUE; goto fin; }
    /* la FAT: fuera la cadena vieja y dentro la nueva */
    for (b = idx >= 0 ? dir[idx].firstblk : VMU_FAT_LAST_IN_FILE, g = 0; b != VMU_FAT_LAST_IN_FILE && b < rz.blk_cnt && g < 256; g++) {
        int s = fat[b]; fat[b] = VMU_FAT_UNALLOCATED; b = s;
    }
    for (i = 0; i < nbl; i++) fat[lista[i]] = i + 1 < nbl ? lista[i + 1] : VMU_FAT_LAST_IN_FILE;
    memset(&nd, 0, sizeof nd);
    nd.filetype = VMU_FILE_DATA; nd.copyprotect = VMU_FILE_COPYABLE; nd.firstblk = lista[0];
    strncpy(nd.filename, nombre, VMU_FILENAME_SIZE);
    vmufs_dir_fill_time(&nd);
    nd.filesize = nbl; nd.hdroff = 0; nd.dirty = 0;
    k = idx >= 0 ? idx : hueco;
    dir[k] = nd;
    d->fase = 'F';
    if (escribe(dev, rz.fat_loc, (const uint8_t *)fat, d)) { r = VE_GRABA_FAT; goto fin; }
    d->fase = 'E';
    k /= BLK / sizeof(vmu_dir_t);
    if (escribe(dev, rz.dir_loc - k, (const uint8_t *)dir + k * BLK, d)) { r = VE_GRABA_DIR; goto fin; }
    /* relectura final: FAT, directorio y el fichero por su cadena */
    d->fase = 'V';
    if (lee(dev, rz.fat_loc, (uint8_t *)fat, d) || lee(dev, rz.dir_loc - k, (uint8_t *)dir + k * BLK, d)) { r = VE_RELEE; goto fin; }
    for (idx = -1, i = k * 16; i < k * 16 + 16; i++)
        if (dir[i].filetype == VMU_FILE_DATA && !strncmp(dir[i].filename, nombre, VMU_FILENAME_SIZE)) idx = i;
    if (idx < 0 || dir[idx].filesize != nbl) { r = VE_DISTINTO; goto fin; }
    for (b = dir[idx].firstblk, i = 0; i < nbl; i++) {
        if (b >= rz.blk_cnt) { r = VE_DISTINTO; d->bloque = b; goto fin; }
        if (lee(dev, b, lei, d)) { r = VE_RELEE; goto fin; }
        if (memcmp(lei, buf + i * BLK, BLK)) { r = VE_DISTINTO; d->maple = VE_M_DISTINTO; d->paso = 6; d->bloque = b; goto fin; }
        b = fat[b];
    }
    if (b != VMU_FAT_LAST_IN_FILE) r = VE_DISTINTO;
fin:
    vmufs_mutex_unlock();
    free(dir); free(lista); free(lei);
    d->cod = r; d->ms = (int)(timer_ms_gettime64() - t0);
    if (!r) d->fase = '-';
    dbglog(r ? DBG_ERROR : DBG_INFO, "vmuesc: %s en %c%d: %d (fase %c, libres %d, viejo %d, necesarios %d, maple %d, "
           "orden %d, bloque %d, reintentos %d, %d ms)\n", nombre, d->puerto >= 0 ? 'a' + d->puerto : '-', d->unidad, r,
           d->fase, d->libres, d->viejo, nbl, d->maple, d->paso, d->bloque, d->reintentos, d->ms);
    return r;
}

void vmuesc_codigo(const vmuesc_diag_t *d, char *s, int n)
{
    char m[8];
    if (d->maple == VE_M_TIEMPO) strcpy(m, "T"); else snprintf(m, sizeof m, "%d", d->maple);
    snprintf(s, n, "E%d F%c I%d L%d N%d U%d P%c%d M%s W%d K%d R%d", d->cod, d->fase == '-' ? 'X' : d->fase, d->intento,
             d->libres, d->necesarios, d->n_vmu, d->puerto >= 0 ? 'A' + d->puerto : 'X', d->unidad < 0 ? 0 : d->unidad,
             m, d->paso, d->bloque, d->reintentos);
}

/* ---------------------------------------------------------------- pantalla de diagnostico */
/* letra 12x24 de la BIOS copiada (bfont espera al GD: se copia al arrancar, vmu_inicia) */
static uint16_t letra[96][24];
static int letra_ok;
void vmuesc_letra(void)
{
    static uint16_t t[24 * 12] __attribute__((aligned(32)));
    int c, y, x;
    if (letra_ok) return;
    for (c = 32; c < 128; c++) {
        memset(t, 0, sizeof t);
        bfont_draw(t, 12, 0, c);
        for (y = 0; y < 24; y++) {
            uint16_t m = 0;
            for (x = 0; x < 12; x++) if (t[y * 12 + x]) m |= 0x800 >> x;
            letra[c - 32][y] = m;
        }
    }
    letra_ok = 1;
}

#define D_LIN 18
#define D_COL 53
static char lin[D_LIN][D_COL + 1];
static uint16_t col_lin[D_LIN];
static int n_lin;
static void dice(uint16_t col, const char *f, ...) __attribute__((format(printf, 2, 3)));
static void dice(uint16_t col, const char *f, ...)
{
    va_list a;
    if (n_lin >= D_LIN) { memmove(lin[1], lin[2], (D_LIN - 2) * sizeof lin[0]); memmove(col_lin + 1, col_lin + 2, (D_LIN - 2) * 2); n_lin = D_LIN - 1; }
    va_start(a, f); vsnprintf(lin[n_lin], sizeof lin[0], f, a); va_end(a);
    col_lin[n_lin++] = col;
    dbglog(DBG_INFO, "vmudiag: %s\n", lin[n_lin - 1]);
}
#define BLANCO 0xFFFF
#define VERDE 0x87F0
#define ROJO 0xF8A3
#define AMARILLO 0xFFE0
#define FONDO 0x0008

/* sobre la imagen que se ve (FB_R_SOF1; 640 de ancho, RGB565 como pone main.c): se repinta entera cada vez */
static void pinta(void)
{
    uint16_t *fb; int i, y, x, f;
    if (!vid_mode || vid_mode->width != 640) return;
    fb = (uint16_t *)(0xa5000000 + (PVR_GET(0x0050) & 0x7ffffc));
    for (i = 0; i < 640 * 480; i++) fb[i] = FONDO;
    if (!letra_ok) return;
    for (i = 0; i < n_lin; i++)
        for (x = 0; lin[i][x] && x < D_COL; x++) {
            int c = (unsigned char)lin[i][x];
            if (c < 32 || c > 127) c = '?';
            for (f = 0; f < 24; f++) {
                uint16_t m = letra[c - 32][f], *p = fb + (8 + i * 26 + f) * 640 + 4 + x * 12;
                for (y = 0; y < 12; y++) if (m & (0x800 >> y)) p[y] = col_lin[i];
            }
        }
}

static const char *nom_maple(int r)
{
    static char s[8];
    switch (r) {
    case MAPLE_RESPONSE_OK: return "BIEN";
    case MAPLE_RESPONSE_DATATRF: return "DATOS";
    case MAPLE_RESPONSE_NONE: return "NADA(-1)";
    case MAPLE_RESPONSE_BADFUNC: return "FUNC(-2)";
    case MAPLE_RESPONSE_BADCMD: return "ORDEN(-3)";
    case MAPLE_RESPONSE_AGAIN: return "OTRAVEZ(-4)";
    case MAPLE_RESPONSE_FILEERR: return "FICH(-5)";
    case VE_M_TIEMPO: return "TIEMPO";
    case VE_M_RARO: return "RARO";
    case VE_M_DISTINTO: return "DISTINTO";
    }
    snprintf(s, sizeof s, "%d", r); return s;
}

static uint32_t botones(void)
{
    maple_device_t *c = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = c ? (cont_state_t *)maple_dev_status(c) : NULL;
    return st ? st->buttons : 0;
}

void vmuesc_pantalla(maple_device_t *dev, const vmuesc_diag_t *ult)
{
    static vmu_root_t rz __attribute__((aligned(32)));
    static uint16_t fat[BLK / 2] __attribute__((aligned(32)));
    static uint8_t orig[BLK] __attribute__((aligned(32))), prueba[BLK] __attribute__((aligned(32)));
    maple_device_t *v; int i, r, ms, libres = 0, prb = -1, resp[6], tms[6], paso, malo = 0, sitio = 0, solte = 0;
    uint64_t t0, t_pinta = 0;
    n_lin = 0;
    dice(AMARILLO, "DIAGNOSTICO DE LA VMU (revision 44)");
    if (ult && ult->cod) { char c[64]; vmuesc_codigo(ult, c, sizeof c); dice(ROJO, "ultimo: %s", c); }
    for (i = 0; (v = maple_enum_type(i, MAPLE_FUNC_MEMCARD)); i++)
        dice(BLANCO, "VMU %c%d func %08lx dat %08lx v%d", 'A' + v->port, v->unit, (unsigned long)v->info.functions,
             (unsigned long)v->info.function_data[0], v->valid);
    if (!i) dice(ROJO, "no hay ninguna VMU");
    pinta();
    if (!dev) { malo = 1; goto espera; }
    vmufs_mutex_lock();
    /* 1: raiz */
    r = lee_1(dev, 255, (uint8_t *)&rz, &ms);
    dice(r ? ROJO : VERDE, "1 raiz (255) de %c%d: %s %d ms", 'A' + dev->port, dev->unit, r ? nom_maple(r) : "BIEN", ms); pinta();
    if (r) { malo = 1; goto suelta; }
    if (!raiz_valida(&rz)) { dice(ROJO, "  sin formato: %02x fat %d x%d dir %d x%d", rz.magic[0], rz.fat_loc, rz.fat_size, rz.dir_loc, rz.dir_size); malo = 1; goto suelta; }
    dice(BLANCO, "  formato bien: FAT %d dir %d x%d, %d bloques", rz.fat_loc, rz.dir_loc, rz.dir_size, rz.blk_cnt);
    /* 2: FAT */
    r = lee_1(dev, rz.fat_loc, (uint8_t *)fat, &ms);
    if (!r) for (i = 0; i < rz.blk_cnt; i++) if (fat[i] == VMU_FAT_UNALLOCATED) { libres++; prb = i; }
    dice(r ? ROJO : VERDE, "2 FAT: %s %d ms, %d bloques libres", r ? nom_maple(r) : "BIEN", ms, libres); pinta();
    if (r) { malo = 1; goto suelta; }
    /* 3: directorio */
    {
        int nf = 0, nl = 0, tam = -1, mt = 0, j;
        for (i = 0; i < rz.dir_size; i++) {
            vmu_dir_t *e = (vmu_dir_t *)prueba;
            r = lee_1(dev, rz.dir_loc - i, prueba, &ms); mt += ms;
            if (r) break;
            for (j = 0; j < 16; j++) {
                if (e[j].filetype == VMU_FILE_NONE) nl++;
                else { nf++; if (!strncmp(e[j].filename, "OR2C2C", VMU_FILENAME_SIZE)) tam = e[j].filesize; }
            }
        }
        dice(r ? ROJO : VERDE, "3 directorio: %s %d ms, %d ficheros, %d huecos", r ? nom_maple(r) : "BIEN", mt, nf, nl);
        if (r) { malo = 1; goto suelta; }
        if (tam >= 0) dice(BLANCO, "  OR2C2C: %d bloques", tam); else dice(BLANCO, "  sin OR2C2C");
        int nec = ult && ult->necesarios > 0 ? ult->necesarios : 20, hay = libres + (tam > 0 ? tam : 0);
        if (hay < nec) { dice(ROJO, "  NO HAY SITIO: libres %d, hacen falta %d", hay, nec); sitio = 1; }
        else dice(VERDE, "  sitio: libres %d, hacen falta %d", hay, nec);
        pinta();
    }
    /* 4: prueba de escritura en un bloque libre (el mas alto: donde iria el fichero), restaurado despues */
    if (prb < 0) { dice(AMARILLO, "4 sin bloque libre: sin prueba de escritura"); goto suelta; }
    r = lee_1(dev, prb, orig, &ms);
    dice(r ? ROJO : VERDE, "4 bloque %d libre, leer: %s %d ms", prb, r ? nom_maple(r) : "BIEN", ms); pinta();
    if (r) { malo = 1; goto suelta; }
    for (i = 0; i < BLK; i++) prueba[i] = (uint8_t)(orig[i] ^ (0xA5 + i * 7));
    for (i = 0; i < 6; i++) resp[i] = tms[i] = 0;
    paso = -1;
    r = escribe_1(dev, prb, prueba, &paso, resp, tms);
    dice(r ? ROJO : VERDE, "  escribir %d/%d %d/%d %d/%d %d/%d conf %d/%d ms", resp[0], tms[0], resp[1], tms[1],
         resp[2], tms[2], resp[3], tms[3], resp[4], tms[4]);
    if (r) { dice(ROJO, "  fallo la orden %d: %s", paso, nom_maple(r)); malo = 1; }
    pinta();
    {
        int k, rr = 0;
        for (k = 0; k < 3; k++) { if (k) thd_sleep(30 * k); if (!(rr = lee_1(dev, prb, comp, &ms))) break; }
        if (rr) { dice(ROJO, "  releer: %s %d ms (%d veces)", nom_maple(rr), ms, k); malo = 1; }
        else if (memcmp(comp, prueba, BLK)) {
            int nd = 0; for (i = 0; i < BLK; i++) nd += comp[i] != prueba[i];
            dice(ROJO, "  releer: DISTINTO (%d bytes) %d ms", nd, ms); malo = 1;
        } else dice(VERDE, "  releer: BIEN %d ms (%d veces)", ms, k + 1);
    }
    pinta();
    /* 5: lo mismo con vmu_block_write de KallistiOS (restaura el bloque) */
    t0 = timer_ms_gettime64();
    r = vmu_block_write(dev, prb, orig);
    ms = (int)(timer_ms_gettime64() - t0);
    dice(r ? ROJO : VERDE, "5 KOS vmu_block_write (restaura): %d, %d ms", r, ms);
    if (r) {                                                /* si KOS no pudo, se restaura con la propia */
        malo = 1; r = escribe_1(dev, prb, orig, &paso, NULL, NULL);
        dice(r ? ROJO : AMARILLO, "  restaurado con la propia: %s", r ? nom_maple(r) : "BIEN");
    }
    r = lee_1(dev, prb, comp, &ms);
    if (r) { dice(ROJO, "  releer: %s %d ms", nom_maple(r), ms); malo = 1; }
    else if (memcmp(comp, orig, BLK)) { dice(ROJO, "  releer: DISTINTO del original"); malo = 1; }
    else dice(VERDE, "  releer: BIEN, bloque como estaba (%d ms)", ms);
suelta:
    vmufs_mutex_unlock();
espera:
    dice(malo ? ROJO : sitio ? AMARILLO : VERDE, "%s - A / B / START: salir", malo ? "HAY FALLOS: haz una foto" :
         sitio ? "VMU BIEN, SIN SITIO" : "TODO BIEN");
    pinta();
    for (t0 = timer_ms_gettime64(); timer_ms_gettime64() - t0 < 300000; ) {   /* (5 min como mucho) */
        uint32_t b = botones() & (CONT_A | CONT_B | CONT_START);
        if (!b) solte = 1; else if (solte) break;
        if (timer_ms_gettime64() - t_pinta > 250) { t_pinta = timer_ms_gettime64(); pinta(); }   /* (si el PVR cambia de imagen) */
        thd_sleep(20);
    }
    while (botones() & (CONT_A | CONT_B | CONT_START)) thd_sleep(20);   /* que no lo vea el menu */
}
