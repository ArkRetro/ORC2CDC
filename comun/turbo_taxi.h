/*
 * turbo_taxi.h - turbo del taxi de Crazy Taxi (ficha 7 / modelo 0).
 *
 * Mecanica: frenar durante 1 segundo seguido y despues acelerar.  Un toque
 * rapido de freno no lo enciende (no estorba al derrape).  Mientras dura, el
 * coche empuja hasta 400 km/h de velocimetro (400,5 internos) en unos 3 s y
 * despues la punta baja poco a poco hasta la velocidad maxima normal.  Durante
 * el arranque pide chispas en las dos ruedas traseras (1,40 s) con el mismo
 * efecto de las chispas del choque.  Es una capa arcade: no toca el motor ni
 * los neumaticos, solo pide una aceleracion longitudinal, un tope de velocidad
 * y las chispas.
 *
 * Todo en unidades del mundo de la fisica (m/s y m/s2).  El que llama
 * convierte los km/h del velocimetro con el factor hud del coche.
 */
#ifndef TURBO_TAXI_H
#define TURBO_TAXI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   estado;       /* 0 inactivo, 1 armado, 2 empuje, 3 caida, 4 espera */
    int   paso;         /* 0 reposo, 1 frenando, 2 armado, 3 turbo            */
    float t;            /* tiempo de la fase (s)                               */
    float t_freno;      /* tiempo con el freno pisado (s)                      */
    float v0;           /* velocidad al disparar el turbo (m/s)                */
    float gas_ant;      /* acelerador del paso anterior (0..1)                 */
    float punta;        /* velocidad punta normal (m/s)                        */
    float objetivo;     /* tope del turbo: 400 km/h de velocimetro (m/s)       */
    float tope;         /* tope que se pide a la fisica ahora (m/s)            */
    float acel;         /* aceleracion longitudinal pedida (m/s2, + empuja)    */
    int   activo;       /* 1 mientras el turbo hace algo (empuje, caida o espera) */
    int   disparos;     /* contador de turbos (diagnostico)                    */
    int   secuencias;   /* secuencias validas (diagnostico)                    */
    float chispa_t;     /* segundos que quedan de chispas traseras (0 = no)    */
} turbo_taxi_t;

/* punta: velocidad maxima normal en m/s; objetivo: tope del turbo en m/s. */
void turbo_taxi_inicia(turbo_taxi_t *t, float punta, float objetivo);

/* Un paso de la mecanica (dt en segundos, gas/freno 0..1, velocidad en m/s). */
void turbo_taxi_paso(turbo_taxi_t *t, float gas, float freno, float velocidad, float dt);

/* Corta el turbo y vuelve al reposo (choque, grua, cambio de coche, etc.). */
void turbo_taxi_corta(turbo_taxi_t *t);

#ifdef __cplusplus
}
#endif

#endif
