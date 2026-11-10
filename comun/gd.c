/* Lectura de ficheros de la zona de alta densidad del GD-ROM (pista 3, LBA 45000): el fs_iso9660 de KallistiOS lee la
   TOC de la zona de baja densidad y monta la pista 1. Aqui: el descriptor del volumen en 45016, el directorio raiz y
   los ficheros por sectores con cdrom_read_sectors (direccion en FAD = LBA + 150). Solo el directorio raiz. */
#include <kos.h>
#include <string.h>
#include "gd.h"

static uint8_t sec[2048] __attribute__((aligned(32)));

static kthread_t *fondo;                                /* hilo de la musica */
static volatile uint64_t ult_primer_plano;              /* ms de la ultima lectura de otro hilo */
void gd_hilo_fondo(void) { fondo = thd_get_current(); }
int gd_ocupado(uint32_t ms) { return ult_primer_plano && timer_ms_gettime64() - ult_primer_plano < ms; }

/* por DMA si el destino esta alineado a 32 (la CPU queda libre mientras llegan los datos); si no, PIO. KallistiOS
   serializa los accesos al GD (semaforo del G1) e invalida la cache de datos sobre el destino de un DMA; el destino no
   se debe tocar mientras tanto ni compartir linea de cache (32 bytes) con otros datos: por eso va alineado a 32 y en
   multiplos de 2048 */
/* Dos hilos pueden leer a la vez (la musica y la carga de etapa en segundo plano, demo/main.c): con las ordenes de
   KallistiOS solapadas el GD devuelve a veces ERR_SYS (visto en Flycast). Cada lectura va entera bajo un mutex propio
   y, si falla, se repite (hasta 4 veces). gd_busca tambien lo toma (su sector es comun): recursivo */
/* Cada intento fallido se cuenta (gd_errores: lo ensena el diagnostico del video); con ERR_DISC_CHG (la unidad avisa de
   un cambio de disco, p.ej. tras un fallo de lectura en la consola real) se reinicia la unidad antes de repetir, como
   hace fs_iso9660 de KallistiOS. */
static mutex_t m_gd = RECURSIVE_MUTEX_INITIALIZER;
static volatile uint32_t n_err; static volatile int ult_err;
void gd_errores(uint32_t *n, int *ult) { *n = n_err; *ult = ult_err; }
static int lee(uint32_t lba, int n, void *dst)
{
    int r, pp = thd_get_current() != fondo, k;
    mutex_lock(&m_gd);
    if (pp) ult_primer_plano = timer_ms_gettime64();
    for (k = 0; k < 4; k++) {
        if (!(r = cdrom_read_sectors_ex(dst, lba + 150, n, ((uintptr_t)dst & 31) == 0))) break;
        n_err++; ult_err = r;
        if (r == ERR_DISC_CHG || r == ERR_NO_DISC) cdrom_reinit();
        thd_sleep(20);
    }
    if (pp) ult_primer_plano = timer_ms_gettime64();
    mutex_unlock(&m_gd);
    return r;
}
int gd_lee(uint32_t lba, int n, void *dst) { return lee(lba, n, dst); }

static int busca(const char *nombre, uint32_t *lba, uint32_t *tam);
int gd_busca(const char *nombre, uint32_t *lba, uint32_t *tam)
{
    int r;
    mutex_lock(&m_gd); r = busca(nombre, lba, tam); mutex_unlock(&m_gd);
    return r;
}
static int busca(const char *nombre, uint32_t *lba, uint32_t *tam)
{
    uint32_t rl, rs, o; size_t ln = strlen(nombre);
    if (lee(45016, 1, sec) || memcmp(sec + 1, "CD001", 5)) return -1;
    rl = sec[158] | sec[159] << 8 | sec[160] << 16 | (uint32_t)sec[161] << 24;
    rs = sec[166] | sec[167] << 8 | sec[168] << 16 | (uint32_t)sec[169] << 24;
    for (o = 0; o < rs; o += 2048) {
        int i = 0;
        if (lee(rl + o / 2048, 1, sec)) return -2;
        while (i < 2048 && sec[i]) {
            uint8_t *r = sec + i; int nl = r[32];
            if ((size_t)nl >= ln && !memcmp(r + 33, nombre, ln) && (nl == (int)ln || r[33 + ln] == ';')) {
                *lba = r[2] | r[3] << 8 | r[4] << 16 | (uint32_t)r[5] << 24;
                *tam = r[10] | r[11] << 8 | r[12] << 16 | (uint32_t)r[13] << 24;
                return 0;
            }
            i += r[0];
        }
    }
    return -3;
}

/* Uniones empaquetadas (tools/pak_union.py): BKnn.DAT es un descriptor ('PAKD') y sus bytes estan en UNIONES.PAK,
   compartidos con las demas uniones (las de las misiones repiten modelo, texturas, cielo y color de los vertices). Se
   recompone aqui el ORD7 de antes byte a byte: quien lo lee no nota nada. Las operaciones van por orden de origen en el
   paquete y se leen con una ventana (el lector solo avanza; un salto de la cabeza por cada trozo lejano, pocos: el
   paquete guarda lo de cada union seguido). La ventana es lo que sobra del hueco detras del fichero (hasta 64 KB) o,
   si sobran menos de 8 KB, el sector de gd.c (de uno en uno): nada del monton ni de RAM fija (una ventana pedida y soltada mientras el bucle pide
   memoria dejaba un hueco y BKD.DAT ya no cabia: "Out of memory" tras BK135). El descriptor cabe en un sector. */
static int es_union(const char *n) { return n[0] == 'B' && n[1] == 'K' && n[2] >= '0' && n[2] <= '9'; }
static uint8_t desc[2048] __attribute__((aligned(32)));
static char pak_nom[17]; static uint32_t pak_lba, pak_tam;
/* tamano del fichero recompuesto (el del descriptor) o el del fichero; 0 bien */
int gd_tam(const char *nombre, uint32_t *tam)
{
    uint32_t lba; int r = -1;
    mutex_lock(&m_gd);
    if (!busca(nombre, &lba, tam)) {
        r = 0;
        if (es_union(nombre) && !lee(lba, 1, sec) && !memcmp(sec, "PAKD", 4)) memcpy(tam, sec + 4, 4);
    }
    mutex_unlock(&m_gd);
    return r;
}
static void pak_copia(uint8_t *dst, uint32_t pv, uint32_t t, uint32_t q, const uint8_t *s, uint32_t n)
{
    uint32_t el = t == 1 ? 20 : 4, o = t == 1 ? 0 : 20;
    if (!t) { memcpy(dst + q, s, n); return; }
    while (n) {                                             /* flujo de vertices: a su sitio en los de 24 bytes */
        uint32_t r = q / el, e = q % el, m = el - e < n ? el - e : n;
        memcpy(dst + pv + 24 * r + o + e, s, m); s += m; q += m; n -= m;
    }
}
static int pak_lee(uint32_t lba, uint32_t tdesc, uint8_t *dst, uint32_t cap)
{
    uint32_t tam, nops, pv, nv, k, w0 = 0, w1 = 0, vt, ts; uint8_t *d = desc, *v; int r = 0;
    const uint32_t *op;
    mutex_lock(&m_gd);                                      /* (desc es uno: una lectura de union a la vez) */
    if (tdesc > 2048 || lee(lba, 1, d)) { mutex_unlock(&m_gd); return -3; }
    memcpy(&tam, d + 4, 4); memcpy(&nops, d + 8, 4); memcpy(&pv, d + 12, 4); memcpy(&nv, d + 16, 4);
    ts = (tam + 2047) / 2048 * 2048;
    if (ts > cap || 36 + 12 * nops > tdesc || pv + 24 * nv > tam) { mutex_unlock(&m_gd); return -2; }
    if (strncmp(pak_nom, (char *)d + 20, 16)) {             /* el paquete (se recuerda donde esta) */
        memcpy(pak_nom, d + 20, 16); pak_nom[16] = 0;
        if (busca(pak_nom, &pak_lba, &pak_tam)) pak_nom[0] = 0;
    }
    if (!pak_nom[0]) { mutex_unlock(&m_gd); return -1; }
    vt = (cap - ts) & ~2047u; if (vt > 65536) vt = 65536;
    if (vt >= 8192) v = dst + ts; else { vt = 2048; v = sec; }   /* (sec: el mutex es nuestro hasta el final) */
    op = (const uint32_t *)(d + 36);
    for (k = 0; k < nops && !r; k++, op += 3) {
        uint32_t s = op[0], l = op[1], t = op[2] >> 30, q = op[2] & 0x3FFFFFFFu;
        if (s + l > pak_tam || (!t && q + l > tam) || (t && q + l > (t == 1 ? 20 : 4) * nv)) { r = -2; break; }
        while (l) {
            uint32_t n;
            if (s < w0 || s >= w1) {                            /* ventana desde el sector de s */
                uint32_t ns = vt / 2048, fin = (pak_tam + 2047) / 2048;
                w0 = s & ~2047u; if (w0 / 2048 + ns > fin) ns = fin - w0 / 2048;
                if (lee(pak_lba + w0 / 2048, ns, v)) { r = -3; w1 = 0; break; }
                w1 = w0 + ns * 2048;
            }
            n = w1 - s < l ? w1 - s : l;
            pak_copia(dst, pv, t, q, v + (s - w0), n); s += n; q += n; l -= n;
        }
    }
    mutex_unlock(&m_gd);
    if (!r) memset(dst + tam, 0, ts - tam);                 /* (como el relleno del fichero) */
#ifdef DIAG_PAK
    if (!r) {                                               /* medida: huella del ORD7 recompuesto (h = h * 31 + byte; */
        uint32_t h = 0, i;                                  /* la misma que tools/pak_union.py --huella del suelto) */
        for (i = 0; i < tam; i++) h = h * 31 + dst[i];
        printf("PAK: %u bytes, huella %08x\n", (unsigned)tam, (unsigned)h);
    }
#endif
    return r;
}

/* el fichero entero en dst (alineado a 32, cap bytes: el tamano redondeado a sectores); 0 bien */
int gd_carga_en(const char *nombre, void *dst, uint32_t cap, uint32_t *tam)
{
    uint32_t lba, n, hecho = 0; uint8_t *b = dst;
    if (gd_busca(nombre, &lba, tam)) return -1;
    if (es_union(nombre)) {                                 /* descriptor de union empaquetada? */
        int pk;
        mutex_lock(&m_gd); pk = !lee(lba, 1, sec) && !memcmp(sec, "PAKD", 4); mutex_unlock(&m_gd);
        if (pk) { uint32_t td = *tam; memcpy(tam, sec + 4, 4); return pak_lee(lba, td, b, cap); }
    }
    n = (*tam + 2047) / 2048;
    if (n * 2048 > cap) return -2;
    while (hecho < n) {                                     /* de 64 en 64 sectores */
        int k = n - hecho > 64 ? 64 : (int)(n - hecho);
        if (lee(lba + hecho, k, b + hecho * 2048)) return -3;
        hecho += k;
    }
    return 0;
}

/* el fichero entero en memoria (alineada a 32); NULL si no esta */
void *gd_carga(const char *nombre, uint32_t *tam)
{
    uint32_t n; uint8_t *b;
    if (gd_tam(nombre, tam)) return NULL;
    n = (*tam + 2047) / 2048;
    b = memalign(32, n * 2048);
    if (!b) return NULL;
    if (gd_carga_en(nombre, b, n * 2048, tam)) { free(b); return NULL; }
    return b;
}
