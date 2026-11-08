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
static int lee(uint32_t lba, int n, void *dst)
{
    int r, pp = thd_get_current() != fondo;
    if (pp) ult_primer_plano = timer_ms_gettime64();
    r = cdrom_read_sectors_ex(dst, lba + 150, n, ((uintptr_t)dst & 31) == 0);
    if (pp) ult_primer_plano = timer_ms_gettime64();
    return r;
}
int gd_lee(uint32_t lba, int n, void *dst) { return lee(lba, n, dst); }

int gd_busca(const char *nombre, uint32_t *lba, uint32_t *tam)
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

/* el fichero entero en memoria (alineada a 32); NULL si no esta */
void *gd_carga(const char *nombre, uint32_t *tam)
{
    uint32_t lba, n, hecho = 0; uint8_t *b;
    if (gd_busca(nombre, &lba, tam)) return NULL;
    n = (*tam + 2047) / 2048;
    b = memalign(32, n * 2048);
    if (!b) return NULL;
    while (hecho < n) {                                     /* de 64 en 64 sectores */
        int k = n - hecho > 64 ? 64 : (int)(n - hecho);
        if (lee(lba + hecho, k, b + hecho * 2048)) { free(b); return NULL; }
        hecho += k;
    }
    return b;
}
