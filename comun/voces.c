/* voces.c: cuando habla la acompanante (y el locutor) en la carrera del modo OutRun (ver voces.h).

   Como lo hace el original (ELF de PS2 y la decompilacion del port de PC: race_robots.cpp, la acompanante, funcion
   0x39 del coche; race_hud_navi.cpp, el HUD; race_end_modes.cpp, GOAL / TIME OVER). El juego de PS2 es el mismo
   codigo: 0x2059c0..0x205cac (cola y comentarios) y 0x202e60..0x203d94 (reacciones) leen las mismas tablas, y la
   funcion de sonido de la carrera es 0x11C090 (424940 del PC). Las tablas vienen en VOCES.DAT.
   - Cola de comentarios (488130 / 4881C0): 8 entradas {voz, retardo, prioridad, estado}. Una entrada nueva no entra
     si la ultima puesta aun tiene retardo y prioridad mayor o igual, ni si la cola esta llena. Cada cuadro la primera
     entrada suena (estado 1 -> 2) y luego se le resta el retardo (120 cuadros en todas) hasta pasar a la siguiente.
   - Zona (488BD0, solo modo OutRun): al pasar el checkpoint (EXTEND TIME) se arma; cuando el coche lleva mas de 40
     posiciones de carretera en la etapa nueva se pone en la cola el comentario de su zona (nivel 1..4 en el arbol).
   - Derrapes (4882B0 / 488C90 / 488220): un derrape cuenta al llegar a 120 cuadros seguidos; el numero de derrapes de
     la carrera elige el comentario (uno exacto, un rango o, desde el 14, uno al azar entre varios) y un derrape largo
     dice otro a los 360 y a los 600 cuadros. Al dejar de derrapar se rearma.
   - Reacciones (489A90): al empezar un choque de los estados 1, 2 y 5 dice una de "aire" (la del estado); al pasar a
     la recuperacion, una de "aterriza" (estados 2 y 5: choque grande) o de "salto" (el resto), cada lista por turno; y
     al terminar un choque del estado 7, una de "salto". Si ademas en ese cuadro choca con otro coche estando en un
     choque (car+4 & 0x4000000 y +2F0 & 2), una de las 5 de "choque" al azar con una espera de 90 cuadros. La lista de
     "aire" es la de los personajes 9 y 10: en el modo OutRun la acompanante es el 9 (487F50 sin licencia).
     Las de asustada y trompo solo las dice en Coast 2 Coast (variantes 3 y 4): aqui no.
   - Locutor: al empezar el modo GAME (0x1b83e8: cuando no viene de RESTART) la voz 0x82; con el EXTEND TIME el efecto
     0x4d y la voz 0x16c (4B93E0 / 0x1be78c); GOAL 0x16b (49C9C0) y TIME OVER 0x155 (499020); el aviso de poco tiempo
     (4B9CD0 / 0x1c00d8) es el efecto 0x18d cuando quedan 600 cuadros o menos, cada 16, 8 o 4 cuadros (por debajo de
     420 y de 240).
   Todo va por pasos de logica (60 por segundo), como el original. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sfx.h"
#include "voces.h"

typedef struct { uint16_t id, retardo; int16_t prio, nada; int32_t a, b, c; } com_t;   /* como en VOCES.DAT */
typedef struct {
    com_t zona[8], derrapes[12], largo[4];
    uint16_t asustada[2], trompo[2], choque[5], aire9[3], aire[3], aterriza[4], salto[2], n_aterriza, n_salto, nada;
} tablas_t;

enum { R_NADA, R_AIRE, R_ATERRIZA, R_SALTO };

static const tablas_t *T;
static struct { int id, retardo, prio, estado; } cola[8];
static int cabeza, cola_fin, pend_n;
static const com_t *pend[8];
static int zona_arma, zona_pend, checkpoint;
static int vueltas, derr_cuadros, derr_visto, derrapando, com_vuelta, com_l1, com_l2;
static int reaccion, espera, turno_salto, turno_aterriza, listo;

int voces_inicia(void)
{
    uint32_t tam;
    if (listo) return 0;
    if (sfx_voces_inicia()) return -1;
    T = sfx_voces_tablas(&tam);
    if (!T || tam < sizeof(tablas_t)) return -1;
    listo = 1;
    return 0;
}

/* 488130 */
static void pon_cola(const com_t *e)
{
    int ult = (cabeza + 7) & 7;
    if (cola[ult].retardo > 0 && cola[ult].prio >= e->prio) return;
    if (((cabeza + 1) & 7) == cola_fin) return;
    cola[cabeza].id = e->id; cola[cabeza].retardo = e->retardo; cola[cabeza].prio = e->prio; cola[cabeza].estado = 1;
    cabeza = (cabeza + 1) & 7;
}

/* 4881C0 */
static void toca_cola(void)
{
    if (cola_fin == cabeza) return;
    if (cola[cola_fin].estado == 1) { sfx_voz(cola[cola_fin].id); cola[cola_fin].estado = 2; return; }
    if (--cola[cola_fin].retardo) return;
    cola_fin = (cola_fin + 1) & 7;
}

static void pon_pendiente(void)
{
    if (pend_n <= 0) return;
    pon_cola(pend[rand() % pend_n]);
    pend_n = 0;
}

void voces_carrera(void)
{
    memset(cola, 0, sizeof cola);
    cabeza = cola_fin = pend_n = 0;
    zona_arma = zona_pend = checkpoint = 0;
    vueltas = derr_cuadros = derr_visto = derrapando = com_vuelta = com_l1 = com_l2 = 0;
    reaccion = R_NADA; espera = 0;
    if (listo) sfx_voz_calla();
}

void voces_salida(void) { if (listo) sfx_voz(0x82); }

void voces_checkpoint(void)
{
    if (!listo) return;
    sfx_voz(0x4d);
    sfx_voz(0x16c);
    checkpoint = 1;
}

void voces_meta(void) { if (listo) sfx_voz(0x16b); }
void voces_tiempo_fuera(void) { if (listo) sfx_voz(0x155); }
void voces_para(void) { if (listo) sfx_voz_calla(); }

void voces_tiempo(int t)
{
    int mascara, objetivo;
    if (!listo) return;
    if (t < 0) t = 0;
    if (t > 600) return;
    if (t <= 240) { mascara = 3; objetivo = 2; }
    else if (t <= 420) { mascara = 7; objetivo = 5; }
    else { mascara = 15; objetivo = 12; }
    if ((t & mascara) == objetivo) sfx_voz(0x18d);
}

static int turno(int *k, int n) { int r = *k; if (++*k >= n) *k = 0; return r; }

/* las reacciones de 489A90 que tienen voz en el modo OutRun */
static void reacciona(const voces_coche_t *c)
{
    static const uint8_t grande[7] = { 0, 1, 0, 0, 1, 0, 0 };
    int k = c->choque - 1;
    if (c->t_choque > 0.0f) {                                   /* choque en curso */
        if (reaccion != R_AIRE) {
            if (k == 0 || k == 1 || k == 4) { reaccion = R_AIRE; sfx_voz(T->aire9[k == 0 ? 0 : k == 1 ? 1 : 2]); }
            else if (k == 6) reaccion = R_AIRE;
        }
    } else if (c->t_recup > 0.0f) {                             /* recuperacion */
        if (reaccion != R_ATERRIZA && reaccion != R_SALTO) {
            if (k >= 0 && k <= 6 && !grande[k]) { reaccion = R_SALTO; sfx_voz(T->salto[turno(&turno_salto, T->n_salto)]); }
            else { reaccion = R_ATERRIZA; sfx_voz(T->aterriza[turno(&turno_aterriza, T->n_aterriza)]); }
        }
    } else if (reaccion == R_AIRE && c->choque == 7) {          /* fin del estado 7 (animacion de 48A6BC) */
        reaccion = R_SALTO; sfx_voz(T->salto[turno(&turno_salto, T->n_salto)]);
    } else reaccion = R_NADA;
    if (c->contacto && c->t_choque > 0.0f && espera <= 0) { sfx_voz(T->choque[rand() % 5]); espera = 90; }
}

void voces_paso(const voces_coche_t *c)
{
    const com_t *e;
    if (!listo) return;
    reacciona(c);
    /* 488BD0 zona */
    if (checkpoint) zona_arma = 1;
    else if (zona_arma && c->en_etapa > 40.0f) {
        for (e = T->zona; e < T->zona + 8 && e->prio >= 0; e++)
            if (e->a == c->zona) { zona_arma = 0; pon_cola(e); break; }
    }
    pon_pendiente();
    /* 488C90 numero de derrapes */
    if (derr_visto && !com_vuelta && T->derrapes[0].prio >= 0) {
        for (e = T->derrapes; e < T->derrapes + 12 && e->prio >= 0; e++) {
            if (e->a != -1) {
                if (e->a == vueltas) { pon_cola(e); com_vuelta = 1; break; }
            } else if (e->b != -1 && e->c != -1) {
                if (e->b <= vueltas && e->c >= vueltas) { pon_cola(e); com_vuelta = 1; break; }
            } else if (e->b <= vueltas) {
                com_vuelta = 1;
                if (pend_n < 8) pend[pend_n++] = e;
            }
        }
    }
    pon_pendiente();
    /* 488220 derrape largo */
    if (T->largo[0].prio >= 0)
        for (e = T->largo; e < T->largo + 4 && e->prio >= 0; e++) {
            if (!com_l1 && e->a == derr_cuadros) { pon_cola(e); com_l1 = 1; }
            else if (!com_l2 && e->a == derr_cuadros) { pon_cola(e); com_l2 = 1; }
        }
    toca_cola();
    /* 4882B0 cuenta de derrapes */
    if (c->derrape && !derrapando) derrapando = 1;
    if (derrapando) {
        derr_cuadros++;
        if (derr_cuadros >= 120 && !derr_visto) { derr_visto = 1; vueltas++; }
    }
    if (!c->derrape && derrapando) derr_visto = derrapando = derr_cuadros = com_vuelta = com_l1 = com_l2 = 0;
    if (espera > 0) espera--;
    checkpoint = 0;
}
