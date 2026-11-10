/* Medida (DIAG_PERFIL): perfil por muestreo del SH4. El TMU1 interrumpe a PERFIL_HZ y se cuenta el PC interrumpido en
   cubos de 16 bytes del codigo; perfil_vuelca escribe los cubos mas llenos (direcciones para addr2line). Solo para
   medir en Flycast o en la consola: no va en la demo normal. */
#ifdef DIAG_PERFIL
#include <kos.h>
#include <stdlib.h>
#include <string.h>
#include "perfil.h"
#ifndef PERFIL_N
#define PERFIL_N 400                                    /* cubos que se escriben */
#endif
#ifndef PERFIL_HZ
#define PERFIL_HZ 20000
#endif
#define _start (&_executable_start)                     /* _executable_start y _etext: arch.h */
static uint32_t *cubos, ncubos, total; static volatile int activo;
static void muestra(irq_t src, irq_context_t *c, void *d)
{
    uint32_t i; (void)src; (void)d;
    timer_clear(TMU1);
    if (!activo) return;
    i = (c->pc - (uint32_t)_start) >> 4;
    if (i < ncubos) cubos[i]++;
    total++;
}
void perfil_inicia(void)
{
    ncubos = ((uint32_t)&_etext - (uint32_t)_start + 15) >> 4;
    if (!(cubos = calloc(ncubos, 4))) return;
    irq_set_handler(EXC_TMU1_TUNI1, muestra, NULL);
    timer_prime(TMU1, PERFIL_HZ, 1);
    timer_start(TMU1);
}
void perfil_activa(int on) { activo = on && cubos; }
void perfil_vuelca(void)
{
    uint32_t k, i, mx, im, suma = 0;
    if (!cubos || !total) return;
    printf("PERFIL: %u muestras\n", (unsigned)total);
    for (k = 0; k < PERFIL_N; k++) {
        for (i = 0, mx = 0, im = 0; i < ncubos; i++) if (cubos[i] > mx) { mx = cubos[i]; im = i; }
        if (!mx) break;
        suma += mx;
        printf("PERFIL: %08x %u %.2f%%\n", (unsigned)((uint32_t)_start + im * 16), (unsigned)mx, (double)(mx * 100.0f / total));
        cubos[im] = 0;
    }
    memset(cubos, 0, ncubos * 4); total = 0;
}
#endif
