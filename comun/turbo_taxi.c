/*
 * turbo_taxi.c - maquina de estados del turbo del taxi (ficha 7 / modelo 0).
 *
 * Secuencia: frenar durante TT_FRENO segundos seguidos y despues acelerar
 * (el gas tiene que cruzar TT_GAS_PISA dentro de TT_ARMADO segundos desde que
 * se armo el turbo).  Un toque rapido de freno no llega a armarlo, asi que no
 * estorba al derrape.  Al encenderse:
 *   - TT_SUBE segundos para llegar al tope de 400,5 km/h de velocimetro, con
 *     una rampa suave que se sigue por realimentacion (cualquier velocidad de
 *     partida llega en ese tiempo);
 *   - TT_EMPUJE segundos manteniendo el tope;
 *   - TT_CAIDA segundos bajando el tope hasta la punta normal;
 *   - TT_ESPERA segundos de espera antes de poder repetir la secuencia;
 *   - TT_CHISPA segundos con chispas en las dos ruedas traseras (el arranque).
 * No se acumula: mientras hay turbo la secuencia no se escucha.
 */
#include "turbo_taxi.h"

#define TT_GAS_PISA     0.50f   /* por encima: acelerador pisado        */
#define TT_FRENO_PISA   0.45f   /* por encima: freno pisado             */
#define TT_FRENO        1.00f   /* freno seguido para armar el turbo (s) */
#define TT_ARMADO       1.20f   /* tras soltar el freno, tiempo para acelerar (s) */

#define TT_SUBE         3.00f   /* del disparo al tope (s)              */
#define TT_EMPUJE       6.00f   /* fase de empuje (s)                   */
#define TT_CAIDA        4.00f   /* fase de caida (s)                    */
#define TT_ESPERA       1.00f   /* espera antes de otro turbo (s)       */
#define TT_CHISPA       1.40f   /* chispas de las ruedas traseras (s)   */

#define TT_ACEL_MAX   110.0f    /* m/s2 (unos 396 km/h fisicos por s)   */
#define TT_ACEL_KP     12.0f    /* realimentacion de la rampa (1/s)     */
#define TT_ARRASTRE     0.0004f /* arrastre cuadratico estimado (1/m)   */
#define TT_DECEL_MAX   22.0f
#define TT_DECEL_BASE   3.0f
#define TT_DECEL_K      2.5f

static float limita(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

void turbo_taxi_inicia(turbo_taxi_t *t, float punta, float objetivo)
{
    int disparos = t ? t->disparos : 0;
    int secuencias = t ? t->secuencias : 0;
    if (!t) return;
    t->estado = 0;
    t->paso = 0;
    t->t = 0.0f;
    t->t_freno = 0.0f;
    t->gas_ant = 0.0f;
    t->v0 = 0.0f;
    t->punta = punta > 1.0f ? punta : 1.0f;
    t->objetivo = objetivo > punta ? objetivo : punta * 1.10f;
    t->tope = t->punta;
    t->acel = 0.0f;
    t->activo = 0;
    t->disparos = disparos;
    t->secuencias = secuencias;
    t->chispa_t = 0.0f;
}

void turbo_taxi_corta(turbo_taxi_t *t)
{
    if (!t) return;
    t->estado = 0;
    t->paso = 0;
    t->t = 0.0f;
    t->t_freno = 0.0f;
    t->gas_ant = 0.0f;
    t->v0 = 0.0f;
    t->tope = t->punta;
    t->acel = 0.0f;
    t->activo = 0;
    t->chispa_t = 0.0f;
}

void turbo_taxi_paso(turbo_taxi_t *t, float gas, float freno, float velocidad, float dt)
{
    if (!t) return;
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > 0.25f) dt = 0.25f;             /* un salto de cuadros no rompe la cuenta */
    t->acel = 0.0f;
    t->tope = t->punta;
    if (t->chispa_t > 0.0f) {               /* el arranque se queda sin chispas al estabilizarse */
        t->chispa_t -= dt;
        if (t->chispa_t < 0.0f) t->chispa_t = 0.0f;
    }

    if (t->estado == 2) {                   /* empuje */
        t->t += dt;
        if (t->t < TT_EMPUJE) {
            float u = limita(t->t / TT_SUBE, 0.0f, 1.0f);
            float s = u * u * (3.0f - 2.0f * u);          /* rampa suave de la velocidad */
            float vr = t->v0 + (t->objetivo - t->v0) * s; /* velocidad pedida en este instante */
            float dr = (t->objetivo - t->v0) * 6.0f * u * (1.0f - u) / TT_SUBE;   /* su derivada */
            float a = dr + (vr - velocidad) * TT_ACEL_KP + TT_ARRASTRE * velocidad * velocidad;
            t->tope = t->objetivo;
            t->acel = limita(a, 0.0f, TT_ACEL_MAX);
            t->activo = 1;
            t->gas_ant = gas;
            return;
        }
        t->estado = 3;                      /* pasa a la caida en este mismo paso */
        t->t = 0.0f;
    }

    if (t->estado == 3) {                   /* caida progresiva */
        float u, s, a;
        t->t += dt;
        u = limita(t->t / TT_CAIDA, 0.0f, 1.0f);
        s = u * u * (3.0f - 2.0f * u);      /* suave al principio y al final */
        t->tope = t->objetivo + (t->punta - t->objetivo) * s;
        if (velocidad > t->tope) {
            a = TT_DECEL_BASE + (velocidad - t->tope) * TT_DECEL_K;
            t->acel = -limita(a, 0.0f, TT_DECEL_MAX);
        }
        t->activo = 1;
        if (t->t >= TT_CAIDA) {
            t->estado = 4;
            t->t = 0.0f;
            t->tope = t->punta;
        }
        t->gas_ant = gas;
        return;
    }

    if (t->estado == 4) {                   /* cola: acaba de bajar hasta la punta */
        float a;
        t->t += dt;
        t->tope = t->punta;
        if (velocidad > t->punta + 0.05f) {
            a = TT_DECEL_BASE + (velocidad - t->punta) * TT_DECEL_K;
            t->acel = -limita(a, 0.0f, TT_DECEL_MAX);
            t->activo = 1;
        } else {
            t->activo = 0;
        }
        if (t->t >= TT_ESPERA) t->estado = 0;
        t->gas_ant = gas;
        return;
    }

    /* estados 0 (reposo) y 1 (armado): escuchando la secuencia */
    t->activo = 0;
    t->tope = t->punta;
    if (t->estado == 0) {
        t->paso = 0;
        if (freno >= TT_FRENO_PISA) {       /* 1,00 s de freno seguidos */
            t->t_freno += dt;
            t->paso = 1;
            if (t->t_freno + 0.0001f >= TT_FRENO) {
                t->estado = 1;
                t->paso = 2;
                t->t = 0.0f;
            }
        } else {
            t->t_freno = 0.0f;              /* un toque rapido no arma nada */
        }
    } else {
        t->t += dt;
        if (gas >= TT_GAS_PISA && t->gas_ant < TT_GAS_PISA) {   /* acelera tras el freno: turbo */
            t->estado = 2;
            t->paso = 3;
            t->t = 0.0f;
            t->v0 = limita(velocidad, 0.0f, t->objetivo);
            t->activo = 1;
            t->disparos++;
            t->secuencias++;
            t->tope = t->objetivo;
            t->acel = TT_ACEL_MAX;
            t->chispa_t = TT_CHISPA;
            t->t_freno = 0.0f;
        } else if (t->t > TT_ARMADO) {
            t->estado = 0;
            t->paso = 0;
            t->t_freno = 0.0f;
        }
    }
    t->gas_ant = gas;
}
