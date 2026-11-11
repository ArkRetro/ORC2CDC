/* Consola serie sin esperas en carrera.
   KallistiOS manda dbglog / printf por el puerto serie (SCIF, 115200 baudios) cuando no hay dc-load: scif_write_buffer
   espera activa a que la cola de 16 bytes tenga sitio y scif_flush a que salga el ultimo byte, haya cable o no (el
   transmisor no tiene control de flujo). Son ~87 us por caracter con el hilo parado: una linea de 150 caracteres
   (la del reparto de la VRAM al subir una etapa o una union) eran 13 ms, y en un cambio de etapa salen varias seguidas
   (VRAM, CURSO, BO, ENTORNO, CHECKPOINT): uno o dos cuadros perdidos cada vez en la consola real. Flycast no lo
   reproduce (su SCIF siempre esta listo).
   Aqui: un manejador de dbgio delante del SCIF. Fuera de la carrera escribe como antes. En carrera (registro_diferido)
   las lineas van a un anillo de 4 KB y registro_paso (tres veces por cuadro) saca solo lo que cabe en la cola del SCIF
   sin esperar (8 bytes cada vez: el aviso TDFE, cola a la mitad o menos). Si el anillo se llena, o si el bucle deja
   de llamar a registro_paso (un error que para la carrera; registro_guarda), se vacia esperando: no se pierde nada. */
#include <kos.h>
#include <string.h>
#include "registro.h"

#define SCFTDR2 (*(volatile uint8_t *)0xffe8000C)
#define SCFSR2  (*(volatile uint16_t *)0xffe80010)
#define ANILLO 4096
static uint8_t anillo[ANILLO]; static volatile uint32_t an_e, an_l;   /* escrito, leido (sin modulo) */
static volatile int diferido; static volatile uint64_t t_paso;
extern dbgio_handler_t dbgio_scif;

/* lo que cabe ahora en la cola del SCIF (sin esperar); devuelve los bytes que quedan en el anillo */
static uint32_t saca(void)
{
    int k; uint32_t q; irq_mask_t m = irq_disable();
    if (an_l != an_e && (SCFSR2 & 0x20)) {
        for (k = 0; k < 8 && an_l != an_e; k++) SCFTDR2 = anillo[an_l++ % ANILLO];
        SCFSR2 &= 0xff9f;
    }
    q = an_e - an_l; irq_restore(m);
    return q;
}
static void vacia(void) { int t = 4000000; while (saca() && --t > 0) ; if (t <= 0) an_l = an_e; }
void registro_paso(void) { t_paso = timer_ms_gettime64(); if (an_l != an_e) saca(); }
void registro_diferido(int si) { if (!si) vacia(); t_paso = timer_ms_gettime64(); diferido = si; }

static int escribe(const uint8_t *d, int len, int xlat)
{
    int i;
    if (!diferido) { if (an_l != an_e) vacia(); return dbgio_scif.write_buffer(d, len, xlat); }
    for (i = 0; i < len; i++) {
        int n = xlat && d[i] == '\n' ? 2 : 1; irq_mask_t m;
        while (ANILLO - (an_e - an_l) < (uint32_t)n) saca();    /* lleno: se espera (no se pierde nada) */
        m = irq_disable();
        if (n == 2) anillo[an_e++ % ANILLO] = '\r';
        anillo[an_e++ % ANILLO] = d[i];
        irq_restore(m);
    }
    return len;
}
static int detectado(void) { return 1; }
static int nada(void) { return 0; }
static int irq_uso(int m) { (void)m; return 0; }
static int vuelca(void) { if (!diferido) { vacia(); return dbgio_scif.flush(); } return 0; }
static int lee_b(uint8_t *d, int len) { return dbgio_scif.read_buffer(d, len); }
static dbgio_handler_t manejador = { .name = "scif_dif", .detected = detectado, .init = nada, .shutdown = nada,
    .set_irq_usage = irq_uso, .flush = vuelca, .write_buffer = escribe, .read_buffer = lee_b };

/* si el bucle no pasa por registro_paso en 300 ms (parado en un error), lo pendiente sale esperando. Lo llama el hilo
   de la musica en cada vuelta (cada 10 ms; un hilo propio serian 32 KB de pila del monton) */
void registro_guarda(void)
{
    if (diferido && an_l != an_e && timer_ms_gettime64() - t_paso > 300) vacia();
}
void registro_inicia(void)
{
    const char *n = dbgio_dev_get();
    if (!n || strcmp(n, "scif")) return;                        /* dc-load u otra salida: como esta */
    dbgio_add_handler(&manejador);
    dbgio_dev_select("scif_dif");
}
