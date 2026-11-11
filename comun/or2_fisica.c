/*
 * or2_fisica.c - manejo de OutRun 2006 C2C (ver or2_fisica.h).
 *
 * Referencias (decompilacion PC, src/driving):
 *   pc_driving_control.cpp  orden de DrivingControl
 *   pc_steering.cpp         steering_operation / toe_angle / tire_direction_angles
 *   pc_transmission.cpp     auto_transmission
 *   pc_driving.cpp          engine_friction / engine_torque / auto_clutch / accel_operation
 *   pc_wheel_dynamics.cpp   neumaticos, ruedas, rodadura, copy_physical_work
 *   pc_chassis.cpp          ActionForce2, calc_game_resist, make_force_work_tire
 *   pc_common_control.cpp   RearGripCtrl, SlipAngleCtrl, CorneringCtrl,
 *                           CalcPlBody_2nd, ass_compulsive_move, CopyCarWork
 *   pc_wall_controller.cpp / pc_wall_response.cpp / pc_wall_rebound.cpp
 *                           muros (CbwColiWall, CalcFrictionStatus, CwReboundStatus)
 *   pc_common_control.cpp   RebPlBody (rebote), pc_chassis.cpp force_work_49fed0 (frenada)
 *   pc_crash.cpp / pc_crash_entry.cpp  estados de choque y su animacion
 *   platform/race_traffic.cpp  choques entre coches (4F9450, 4F6FE0, 4F7BF0, 4F6820)
 *   platform/vehicle_constructor.cpp  umbrales de cambio
 *   platform/vehicle_body_init.cpp    geometria / cargas estaticas
 *
 * Aproximaciones principales (resumen; detalles junto al codigo y en docs/PARIDAD.md, "Fisica: auditoria"):
 *  - Plano de guinada 2D para el movimiento; la suspension (ColiCar, CalcSuspensionForce, CalcTireLoad) va
 *    con el cuerpo reducido a altura, cabeceo y balanceo de angulo pequeno sobre un suelo plano (las
 *    pendientes no cambian el avance: assCancelInclineResistance las anula en el original).
 *  - El firme llega como banderas de material por rueda (GetRoadMu da 1.0 o 0.7).
 *  - ass_compulsive_move usa los rumbos/bordes que pasa el llamador en vez
 *    de muestrear la spline del circuito.
 * Todo en float; sin dobles. Coste: unas 30 funciones trigonometricas por
 * paso interno.
 */
#include "or2_fisica.h"
#include <math.h>
#include <string.h>

#define DT      0.0166112948f   /* 1/60.2 (PC 0x3c881469)          */
#define HZ      60.2f
#define U2R     9.58738019e-05f /* unidad angular -> rad           */
#define R2U     10430.3779f     /* rad -> unidad angular           */
#define G_ACC   9.80665f
#define PI_F    3.14159265f

/* ---------------------------------------------------------------- utiles */
static int16_t s16(int32_t x) { return (int16_t)(uint16_t)(uint32_t)x; }
static int32_t tr(float f)
{
    /* truncamiento hacia cero con saturacion (CVTTSS2SI sin NaN) */
    if (!(f > -2.0e9f)) return -2000000000;
    if (f > 2.0e9f) return 2000000000;
    return (int32_t)f;
}
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float cl01(float v) { return clampf(v, 0.0f, 1.0f); }
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }
static float angw(float a)
{
    while (a > PI_F) a -= 2.0f * PI_F;
    while (a < -PI_F) a += 2.0f * PI_F;
    return a;
}

/* --------------------------------------------------------- carga de tabla */
int or2_carga_param(const float *t, int filas, int col, or2_param_t *p)
{
    int g;
    float k;
    if (!t || !p || col < 0 || col >= OR2_COLUMNAS || filas < OR2_FILAS_MIN) return -1;
    memset(p, 0, sizeof(*p));
#define P(off) (t[(off) / 4 + col])
    p->masa = P(0x0000);
    p->k_inercia[0] = P(0x004c); p->k_inercia[1] = P(0x0098); p->k_inercia[2] = P(0x00e4);
    p->batalla = P(0x0130);
    p->via[0] = P(0x017c); p->via[1] = P(0x01c8);
    p->reparto = P(0x0214);
    p->alt_cg = P(0x0260);
    p->toe[0] = P(0x07b8); p->toe[1] = P(0x0804);
    p->comp_base[0] = P(0x0850); p->comp_base[1] = P(0x089c);
    p->comp_trac[0] = P(0x08e8); p->comp_trac[1] = P(0x0934);
    p->comp_fren[0] = P(0x0980); p->comp_fren[1] = P(0x09cc);
    p->comp_dir[0] = P(0x0a18); p->comp_dir[1] = P(0x0a64);
    p->comp_lev[0] = P(0x0ab0); p->comp_lev[1] = P(0x0afc);
    p->radio[0] = P(0x0b48); p->radio[1] = P(0x0b94);
    p->inercia_rueda[0] = P(0x0c78); p->inercia_rueda[1] = P(0x0cc4);
    p->rod_fija[0] = P(0x0d10); p->rod_fija[1] = P(0x0d5c);
    p->rod_cuad[0] = P(0x0da8); p->rod_cuad[1] = P(0x0df4);
    p->mu_max[0] = P(0x0e40); p->mu_max[1] = P(0x0e8c);
    p->mu_min[0] = P(0x0ed8); p->mu_min[1] = P(0x0f24);
    p->rigidez[0] = P(0x0f70); p->rigidez[1] = P(0x0fbc);
    p->curva_der[0] = P(0x1008); p->curva_der[1] = P(0x1054);
    {   /* 0x10a0 es un entero de 32 bits */
        int32_t n;
        memcpy(&n, &t[0x10a0 / 4 + col], 4);
        p->marchas = n;
    }
    if (p->marchas < 1 || p->marchas > 6) return -2;
    p->rel_extra = P(0x10ec);
    for (g = 1; g <= p->marchas; ++g) p->rel[g] = P(0x1184 + (g - 1) * 0x4c);
    p->rel_final = P(0x134c);
    p->w_baja2 = P(0x1398); p->w_baja = P(0x13e4); p->w_baja_top = P(0x1430);
    p->w_baja_freno = P(0x147c); p->w_sube = P(0x14c8);
    p->cilindros = P(0x1514); p->diametro = P(0x1560); p->carrera = P(0x15ac);
    p->w_ralenti = P(0x15f8); p->w_corte = P(0x1644); p->w_max = P(0x1690);
    p->inercia_motor = P(0x16dc); p->par_mult = P(0x1728); p->embrague_cap = P(0x1774);
    p->freno_g = P(0x17c0); p->freno_rep[0] = P(0x180c); p->freno_rep[1] = P(0x1858);
    p->dir_k1 = P(0x18a4); p->dir_k2 = P(0x18f0); p->radio_giro = P(0x193c);
    p->area = P(0x1988); p->cx = P(0x19d4);
    p->grip_baja = P(0x1c34); p->grip_media = P(0x1c80); p->grip_sube = P(0x1ccc);
    p->grip_amin = P(0x1d18); p->grip_amax = P(0x1d64);
    p->c0_der[0] = P(0x1db0); p->c0_der[1] = P(0x1e48);
    p->c1_der[0] = P(0x1dfc); p->c1_der[1] = P(0x1e94);
    p->c0_nor[0] = P(0x1ee0); p->c0_nor[1] = P(0x1f78);
    p->c1_nor[0] = P(0x1f2c); p->c1_nor[1] = P(0x1fc4);
    p->k_guinada = P(0x2010); p->max_dslip = P(0x205c); p->arrastre_lat = P(0x20a8);
    p->ass_base = P(0x20f4); p->ass_fuera = P(0x2140); p->ass_dentro = P(0x218c);
    p->ass_borde = P(0x21d8); p->ass_sin = P(0x2224); p->ass_tiempo = P(0x2270);
    p->adj_fuera = P(0x22bc); p->adj_pend = P(0x2308); p->adj_opu = P(0x2354);
    p->adj_opu_k = P(0x23a0); p->adj_dentro = P(0x23ec);
    p->hud = P(0x2438);
    /* suspension (vehicle_wheel_positions_4a1e80, car_sus_coli_check, suspension_force) */
    p->sus_anclaje[0] = P(0x0390); p->sus_anclaje[1] = P(0x03dc);
    p->sus_centro[0] = P(0x0428); p->sus_centro[1] = P(0x0474);
    p->sus_inf[0] = P(0x04c0); p->sus_inf[1] = P(0x050c);
    p->sus_sup[0] = P(0x0558); p->sus_sup[1] = P(0x05a4);
    p->sus_muelle[0] = P(0x0688); p->sus_muelle[1] = P(0x06d4);
    p->sus_estab[0] = P(0x0720); p->sus_estab[1] = P(0x076c);
    p->v_tope = P(0x1300);                  /* aqui solo la relacion; se completa abajo */
    /* multiplicador de empuje por marcha: fila (marcha+0x7b) */
    for (g = 0; g <= 6; ++g) p->empuje[g] = P((g + 0x7b) * 0x4c);
#undef P
    if (p->masa <= 0.0f || p->batalla <= 0.0f || p->radio[1] <= 0.0f) return -3;

    /* geometria (vehicle_body_init): CG a L/(1+d) del eje delantero */
    p->cg_del = p->batalla / (p->reparto + 1.0f);
    p->cg_tras = p->batalla - p->cg_del;
    p->carga_est[0] = p->masa / (1.0f / p->reparto + 1.0f) * 0.5f;
    p->carga_est[1] = p->masa / (p->reparto + 1.0f) * 0.5f;

    /* angulo geometrico de direccion (steering_operation):
       atan(L / (sqrt(Rmin^2 - L^2) - via/2)) en unidades de 16 bits */
    {
        float r2 = p->radio_giro * p->radio_giro - p->batalla * p->batalla;
        float den = sqrtf(r2 > 0.0f ? r2 : 0.0f) - p->via[0] * 0.5f;
        if (den <= 0.0f) den = 0.01f;
        p->dir_g = (float)tr(atanf(p->batalla / den) * R2U);
        if (p->dir_g == 0.0f) p->dir_g = 1.0f;
    }

    /* umbrales de cambio (vehicle_gear_thresholds_487570), m/cuadro */
    k = p->radio[1] / (p->rel_final * p->rel_extra * HZ);
    for (g = 0; g < 8; ++g) { p->sube[g] = -10000.0f; p->baja[g] = -10000.0f; p->baja_fr[g] = -10000.0f; }
    p->sube[p->marchas] = 10000.0f;
    for (g = 1; g < p->marchas; ++g) p->sube[g] = p->w_sube / p->rel[g] * k;
    for (g = 2; g <= p->marchas; ++g) {
        float src = (g == 2) ? p->w_baja2 : (g == p->marchas ? p->w_baja_top : p->w_baja);
        p->baja[g] = src / p->rel[g - 1] * k;
        p->baja_fr[g] = (g == 2 ? p->w_baja2 : p->w_baja_freno) / p->rel[g - 1] * k;
    }
    /* e+0xE64 (vehicle_gear_thresholds_487570): corte / fila 0x1300 (fija, aunque haya menos marchas) */
    p->v_tope = p->v_tope > 0.0f ? p->w_corte / p->v_tope * k : 10000.0f;

    /* curvas aproximadas por si no se cargan las reales:
       par plano tipo V12 normalizado y freno lineal con zona muerta */
    p->par_idx = 0.12732396f;
    p->par_gan = 4.3f;
    for (g = 0; g < OR2_PAR_MUESTRAS; ++g) {
        float x = (float)g / 128.0f;
        p->par[g] = g < 128 ? (15.75f + 21.0f * x - 12.0f * x * x) : 0.0f;
    }
    for (g = 0; g < OR2_FRENO_MUESTRAS; ++g)
        p->freno[g] = g < 36 ? 0.0f : (float)(g - 36) / 219.0f;
    p->curvas_reales = 0;
    return 0;
}

void or2_carga_curvas(or2_param_t *p, const float *par, const float *fr)
{
    int i;
    if (!p) return;
    if (par) {
        p->par_idx = par[0];
        p->par_gan = par[1];
        for (i = 0; i < OR2_PAR_MUESTRAS; ++i) p->par[i] = par[2 + i];
    }
    if (fr) for (i = 0; i < OR2_FRENO_MUESTRAS; ++i) p->freno[i] = fr[i];
    if (par && fr) p->curvas_reales = 1;
}

void or2_carga_par_manual(or2_param_t *p, const float *par)
{
    int i;
    if (!p || !par) return;
    p->par_m_idx = par[0];
    p->par_m_gan = par[1];
    for (i = 0; i < OR2_PAR_MUESTRAS; ++i) p->par_m[i] = par[2 + i];
    p->par_m_ok = 1;
}

/* ----------------------------------------------------------- inicio */
static void pos_rueda(const or2_param_t *p, int i, float *x, float *y)
{
    /* x adelante desde el CG, y a la izquierda. i par = izquierda */
    *x = (i < 2) ? p->cg_del : -p->cg_tras;
    *y = p->via[i >> 1] * ((i & 1) ? -0.5f : 0.5f);
}

/* constantes de la suspension (SuspensionTuning de la decompilacion: PC 0x628138, 0x5c36c0, 0x5b0058,
   0x5c36bc, 0x5c36b8, 0x5c36b4) */
#define SUS_APRIETE  5.0f            /* assGetPressDown: el suelo aprieta 5 g de mas (6 con la gravedad) */
#define SUS_RECORR   6.66666651f
#define SUS_FORMA    0.400000006f
#define SUS_AMORT    19228.9883f
#define SUS_REBOTE   0.736847579f
#define SUS_ESCALA   0.101971619f

/* posicion de la rueda i en el cuerpo con los ejes del PC (x derecha, y arriba, z atras; origen en el CG):
   vehicle_wheel_positions_4a1e80 */
static void rueda_pc(const or2_param_t *p, int i, float *x, float *y, float *z)
{
    *x = p->via[i >> 1] * ((i & 1) ? 0.5f : -0.5f);
    *y = p->sus_anclaje[i >> 1] - p->alt_cg;
    *z = (i < 2) ? -p->cg_del : p->cg_tras;
}

/* suspension en reposo con las cargas estaticas (la que deja CalcSuspensionForce en equilibrio) */
static void sus_reposo(or2_coche_t *c, const or2_param_t *p)
{
    float a[2], zf = -p->cg_del, zr = p->cg_tras, x, y, z, ext[2];
    int i;
    for (i = 0; i < 2; ++i) {
        rueda_pc(p, i * 2, &x, &y, &z);
        ext[i] = p->sus_centro[i] - p->carga_est[i] / (p->sus_muelle[i] * SUS_ESCALA);
        a[i] = p->radio[i] - y + ext[i];
    }
    c->cabeceo = (zr - zf) != 0.0f ? (a[0] - a[1]) / (zr - zf) : 0.0f;
    c->alt = a[0] + zf * c->cabeceo;
    c->v_alt = c->w_cab = c->balanceo = c->w_bal = 0.0f;
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        rueda_pc(p, i, &x, &y, &z);
        q->ext = q->ext_ant = ext[i >> 1];
        q->ext_vel = 0.0f;
        q->y28 = y - ext[i >> 1];
        q->f_sus = p->carga_est[i >> 1] * (SUS_APRIETE + 1.0f);
        q->aire = 0;
        c->rueda_y[i] = p->alt_cg + (y - p->sus_inf[i >> 1] < q->y28 ? y - p->sus_inf[i >> 1] : q->y28);
    }
}

void or2_inicia(or2_coche_t *c, const or2_param_t *p)
{
    int i;
    memset(c, 0, sizeof(*c));
    c->d38 = 1.0f;
    c->limitador = 1.0f;
    c->w_motor = p->w_ralenti;
    /* vehicle_construct_4a5830: rpm de ralenti en +20C/+210/+48 */
    c->rpm_i = c->rpm_ant = c->rpm_suave = tr(p->w_ralenti * 9.54929638f);
    c->e98 = 3.40282347e38f;
    for (i = 0; i < 4; ++i) {
        c->r[i].carga_ref = c->r[i].carga = p->carga_est[i >> 1];
        c->r[i].vdx = 1.0f;
        c->r[i].esc = 1.0f;
    }
    sus_reposo(c, p);
    c->marcha = 1;                       /* vehicle_construct_4a5830: event+0x208 = 1 */
    c->da0 = -1;                         /* +DA0 = 0x19A: ningun coche */
}

void or2_coloca(or2_coche_t *c, const or2_param_t *p, float rumbo, float v)
{
    int i, g;
    float mf;
    or2_inicia(c, p);
    c->psi = -rumbo;
    c->vx = -sinf(c->psi) * v;
    c->vy = cosf(c->psi) * v;
    for (i = 0; i < 4; ++i) {
        c->r[i].vel = v;
        c->r[i].omega = v / p->radio[i >> 1];
    }
    mf = v * DT;
    c->vel_mf = mf;
    c->kmh = tr(mf * 216.72f);
    c->rumbo_v16 = s16(tr(c->psi * R2U));
    g = 1;
    if (v > 0.5f) {
        while (g < p->marchas && mf > p->sube[g]) ++g;
        c->embrague = 255;
        c->emb_frac = 1.0f;
        c->emb_par = p->embrague_cap;
        c->w_motor = clampf(v / p->radio[1] * p->rel[g] * p->rel_final * p->rel_extra,
                            p->w_ralenti, p->w_max);
    }
    c->marcha = g;
}

/* ------------------------------------------------------- motor (pc_driving) */
static float friccion_motor(const or2_param_t *p, float rpm, float gan)
{
    float r = 0.0f, base, geo;
    if (!(0.0f > rpm)) r = rpm > p->w_corte ? p->w_corte : rpm;
    base = ((r * 0.455945313f + 143.239456f) * r + 97000.0f) * 0.102040812f;
    geo = p->diametro * p->diametro * p->carrera * p->cilindros * 0.785398185f;
    return geo * (gan * base) * 0.0795774683f;
}

static float vel_motor_prevista(const or2_coche_t *c, const or2_param_t *p, int g)
{
    float s, rel;
    if (g == 0) return c->w_motor;
    s = cosf((float)c->r[3].desliz * U2R) * c->r[3].vel +
        cosf((float)c->r[2].desliz * U2R) * c->r[2].vel;
    s = fabsf(s) * 0.5f;
    rel = p->rel[g] * p->rel_final * p->rel_extra / p->radio[1];
    return rel * s;
}

static void cambio_auto(or2_coche_t *c, const or2_param_t *p)
{
    int g = c->marcha;
    const float *baja = c->freno_op < 128 ? p->baja : p->baja_fr;
    if (c->vel_mf > p->sube[g] && c->freno_op < 128) { ++g; c->cambio = 1; }
    else if (baja[g] > c->vel_mf && c->c282 == 0) { --g; c->cambio = 3; }
    if (g > p->marchas) g = p->marchas;
    if (g < 0) g = 0;
    c->marcha = g;
}

/* manual_transmission (pc_transmission.cpp; PS2 igual): con +E84 (el aviso de cambio llego a 480 cuadros) hace
   de automatico hasta que se vuelve a tocar la palanca. Reducir con el motor al corte (rpm previstas >= 0x1644),
   sin freno y con gas: no hay golpe de gas (+296 = 0) y arranca el derrape (+D36 = 80, +D94/+D98/+D9C) */
static float vel_motor_prevista(const or2_coche_t *c, const or2_param_t *p, int g);
static void cambio_manual(or2_coche_t *c, const or2_param_t *p, int pide)
{
    if (c->e84) cambio_auto(c, p);
    if (pide > 0) {
        c->e84 = 0;
        if (c->marcha < p->marchas) { c->marcha++; c->cambio = 1; }
        return;
    }
    if (pide < 0) {
        c->e84 = 0;
        if (c->marcha <= 1) return;
        c->marcha--;
        if (vel_motor_prevista(c, p, c->marcha) >= p->w_corte && c->freno_op < 128 && c->ac_motor > 16) {
            if (c->d36 < 12) { c->d94 = 180; c->d98 = 60; }
            c->d9c = 60; c->d36 = 80; c->cambio = 0;
        } else c->cambio = 3;
    }
}

/* CheckShiftWarning (4A50F0): en manual, 480 cuadros pidiendo subir (por encima del umbral, embrague cogido,
   gas >= 175, sin freno ni derrape) pasan al cambio automatico (+E84); +E80 enciende el aviso a los 180 */
static void aviso_cambio(or2_coche_t *c, const or2_param_t *p)
{
    int act, n;
    if (c->c282 > 0 || c->c283 > 0 || c->t_choque > 0.0f || c->t_recup > 0.0f || c->e84) return;
    act = c->marcha < p->marchas && c->vel_mf > p->sube[c->marcha] && c->embrague >= 255 &&
          c->pedal_ac >= 175 && c->pedal_fr <= 0 && c->d36 <= 0;
    n = c->e7c;
    if (act) n++; else { n--; if (n > 180) n = 180; }
    c->e7c = n;
    if (c->cambio == 1) { n = 0; c->e7c = 0; }
    if (n >= 480) { c->e7c = 0; c->e84 = 1; c->e80 = 0; }
    else { c->e84 = 0; c->e80 = n >= 180; }
    if (c->e7c < 0) c->e7c = 0;
    if (c->e7c > 480) c->e7c = 480;
}

static void acelerador_op(or2_coche_t *c, const or2_param_t *p)
{
    float pred = vel_motor_prevista(c, p, c->marcha);
    switch (c->cambio) {
    case 1: {
        int v;
        if (c->embrague == 0 && (c->kmh == 0 || c->ac_guardado == 0 || p->w_ralenti > c->w_motor))
            c->cambio = 2;
        v = c->ac_guardado - 64; if (v < 0) v = 0;
        c->ac_motor = v; c->ac_guardado = v;
        break; }
    case 2:
        if (c->embrague < 255) { c->ac_motor = 0; c->ac_guardado = 0; }
        else c->cambio = 0;
        if (p->w_ralenti > c->w_motor || c->kmh == 0) c->cambio = 0;
        break;
    case 3:
        if (!(pred > c->w_motor)) { c->cambio = 0; break; }
        c->ac_motor = 237;
        if (c->w_motor >= p->w_corte) c->cambio = 4;
        break;
    case 4:
        c->ac_motor = 237;
        if (p->w_corte > pred) c->cambio = 0;
        break;
    default:
        c->ac_guardado = c->ac_motor;
        break;
    }
}

static void fin_embrague(or2_coche_t *c, const or2_param_t *p)
{
    c->emb_frac = (float)c->embrague * 0.00392156886f;
    c->emb_par = p->embrague_cap * c->emb_frac;
}

static void embrague_auto(or2_coche_t *c, const or2_param_t *p)
{
    int sh = c->cambio;
    if (c->t_recup > 0.0f) {                     /* recuperandose de un choque: embrague suelto */
        c->embrague = 0; c->emb_estado = 1;
        fin_embrague(c, p);
        return;
    }
    if ((c->kmh == 0 || iabs(c->s4e) > 0x7000) && (c->freno_op > 50 || c->rpm_i < 4500)) {
        c->embrague = c->kmh < 255 ? c->kmh : 255;
        c->emb_estado = 1;
        fin_embrague(c, p);
        return;
    }
    if (sh == 4) {
        /* media de la escala del circulo de friccion de las traseras (+E4): patinando suelta el embrague */
        float m = (c->r[3].esc + c->r[2].esc) * 0.5f;
        c->embrague += 1.0f > m ? -4 : 4;
        if (c->embrague > 255) c->embrague = 255;
        if (c->embrague < 0) c->embrague = 0;
        c->emb_estado = 4;
    } else if (sh == 1 || sh == 3) {
        c->embrague = 0; c->emb_estado = (sh == 1) ? 2 : 3;
    } else {
        int d = 10, e;
        switch (c->emb_estado) {
        case 1: d = c->rpm_ant < c->rpm_i ? 16 : 1; break;
        case 2: case 3: d = 32; break;
        case 4: d = 16; break;
        default: break;
        }
        e = c->embrague + d;
        if (c->emb_estado == 1 && e > 255) c->emb_estado = 0;
        c->embrague = e < 255 ? e : 255;
    }
    fin_embrague(c, p);
}

/* torque_table: la curva 0 (automatico) o la 1 (manual, event+0x13) salvo con +E84 */
static int curva_manual(const or2_coche_t *c, const or2_param_t *p) { return c->manual && !c->e84 && p->par_m_ok; }

static void par_motor(or2_coche_t *c, const or2_param_t *p)
{
    float rpm = clampf(c->w_motor, 0.0f, p->w_max);
    int m = curva_manual(c, p);
    float gan = m ? p->par_m_gan : p->par_gan, ix = m ? p->par_m_idx : p->par_idx;
    const float *curva = m ? p->par_m : p->par;
    float fr = friccion_motor(p, rpm, gan), par, frac, seno, obj, paso, umbral, ral;
    int32_t idx = tr(ix * rpm), ac;
    if (idx < 0) idx = 0;
    if (idx > OR2_PAR_MUESTRAS - 1) idx = OR2_PAR_MUESTRAS - 1;
    par = p->par_mult * gan * curva[idx] + fr;
    ac = c->ac_motor; if (ac < 0) ac = 0; if (ac > 255) ac = 255;
    frac = (float)ac * 0.00392156886f;
    seno = sinf((float)((ac * 16384) / 255) * U2R);
    obj = seno;
    if (c->marcha) obj = frac * frac * c->emb_frac + (1.0f - c->emb_frac) * seno;
    if (obj > c->limitador) {
        float m = (c->limitador + obj) * 0.5f;
        c->ac_motor = tr(sqrtf(m) * 255.0f);
        obj = c->limitador;
    }
    par *= obj;
    paso = c->embrague < 128 ? 0.8f : (c->marcha == p->marchas ? 0.2f : 0.4f);
    umbral = p->w_corte;
    if (c->marcha == p->marchas) umbral *= 1.0f + c->rebufo_k * 0.0649999976f;   /* rebufo (pc_driving 158) */
    if (rpm >= umbral) {
        if (c->marcha == p->marchas && p->modelo >= 15) paso *= 0.5f;   /* preparados (PS2 0x18ce5c) */
        c->limitador -= paso;
    } else c->limitador += paso;
    c->limitador = cl01(c->limitador);
    ral = p->w_ralenti >= rpm ? friccion_motor(p, p->w_ralenti, gan) * 2.0f : 0.0f;
    c->par_neto = ral + par - fr;
}

static void reparte_freno(or2_coche_t *c, const or2_param_t *p)
{
    float pr = p->freno[c->freno_op < 0 ? 0 : (c->freno_op > 255 ? 255 : c->freno_op)];
    float a = 1.0f / (p->freno_rep[0] + 1.0f), b = 1.0f / (p->freno_rep[1] + 1.0f);
    float sesgo = (b - a) * pr + a;
    float total = p->freno_g * p->masa;
    float tras = p->radio[1] * total * sesgo * pr * 0.5f;
    float del = (1.0f - sesgo) * p->radio[0] * total * pr * 0.5f;
    c->r[0].freno = c->r[1].freno = del;
    c->r[2].freno = c->r[3].freno = tras;
}

/* -------------------------------------------- neumaticos (pc_wheel_dynamics) */
static float limite_suave(float mag, float grip)
{
    float ratio, f = 1.0f;
    if (grip == 0.0f) return grip;
    ratio = mag / grip;
    if (ratio > 0.25f && 4.0f > ratio) {
        float u = 1.0f - (ratio - 0.25f) * 0.266666681f;
        f = (1.0f - u * u * u) * 0.75f + 0.25f;
    }
    return f * grip;
}

static float omega_proy(const or2_rueda_t *q, float radio)
{
    return (q->vel / radio) * cosf((float)q->desliz * U2R);
}

static float freno_con_signo(float om, float fr) { return om >= 0.0f ? fr : -fr; }
static float aplica_freno(float om, float d) { return fabsf(om) > fabsf(d) ? om - d : 0.0f; }

static float rel_total(const or2_param_t *p, int g)
{
    return p->rel[g] * p->rel_final * p->rel_extra;
}

static void fuerza_trasera(or2_coche_t *c, const or2_param_t *p)
{
    or2_rueda_t *l = &c->r[2], *r = &c->r[3];
    float radio = p->radio[1], in = p->inercia_rueda[1], ie = p->inercia_motor;
    float rs = radio * radio;
    float ml = l->carga_ref * rs * 0.101971619f, mr = r->carga_ref * rs * 0.101971619f;
    float pl = omega_proy(l, radio), pr = omega_proy(r, radio);
    float motor = c->par_neto / ie * DT + c->w_motor;
    float rel = 0.0f, ef = 0.0f, il, ir, ol, orr, bl, br, med, dif, lim, el, er, wl, wr;
    if (c->marcha) { rel = rel_total(p, c->marcha); ef = rel * rel * ie; motor /= rel; }
    il = ml + in; ir = mr + in;
    ol = (pl * ml + l->omega * in) / il;
    orr = (pr * mr + r->omega * in) / ir;
    bl = freno_con_signo(ol, l->freno); br = freno_con_signo(orr, r->freno);
    l->freno = bl; r->freno = br;
    ol = aplica_freno(ol, bl / il * DT);
    orr = aplica_freno(orr, br / ir * DT);
    med = (orr * ir + ol * il) / (ir + il);
    dif = motor - med;
    lim = 0.0f;
    if (ef != 0.0f) lim = c->emb_par * rel / ef * DT;
    if (dif > lim) dif = lim; else if (-lim > dif) dif = -lim;
    el = ef * 0.5f; er = ef - el;     /* event+0x2a4 (reparto diferencial) = 0 */
    wl = ((dif + ol) * el + il * ol) / (el + il);
    wr = ((dif + orr) * er + ir * orr) / (er + ir);
    l->omega_t = wl; r->omega_t = wr;
    l->f_lon = (wl - pl) / radio * ml * HZ;
    r->f_lon = (wr - pr) / radio * mr * HZ;
}

static void fuerza_delantera(or2_coche_t *c, const or2_param_t *p)
{
    int i;
    float radio = p->radio[0], in = p->inercia_rueda[0];
    for (i = 0; i < 2; ++i) {
        or2_rueda_t *q = &c->r[i];
        float m = radio * radio * q->carga_ref * 0.101971619f;
        float inv = 1.0f / (m + in);
        float pj = omega_proy(q, radio);
        float om = (pj * m + q->omega * in) * inv;
        float b = freno_con_signo(om, q->freno);
        q->freno = b;
        om = aplica_freno(om, inv * b * DT);
        q->omega_t = om;
        q->f_lon = (om - pj) / radio * m * HZ;
    }
}

static void circulo_friccion(or2_coche_t *c)
{
    int i;
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        float fx = q->f_lon, fy = q->f_lat, g = q->agarre;
        float mag = sqrtf(fx * fx + fy * fy);
        float lim = c->marcha > 1 ? limite_suave(mag, g) : g;
        float esc = mag > lim ? lim / mag : 1.0f, sm;
        q->esc = esc;                                    /* +E4 (lo usa el embrague en el estado 4) */
        if (fx >= 0.0f) fx = esc * fx;
        else { float u = 1.0f - esc; fx = (1.0f - u * u * u) * fx; }
        q->f_lon = fx;
        sm = c->freno_op > 250 ? mag : fabsf(fy);
        lim = limite_suave(sm, g);
        esc = sm > lim ? lim / sm : 1.0f;
        q->f_lat = esc * fy;
    }
}

static void giro_delantero(or2_coche_t *c, const or2_param_t *p)
{
    int i;
    float inv = 1.0f / p->inercia_rueda[0];
    for (i = 0; i < 2; ++i) {
        or2_rueda_t *q = &c->r[i];
        float antes = q->omega - p->radio[0] * q->f_lon * inv * DT;
        float om = antes - q->freno * inv * DT;
        if (0.0f >= antes * om) om = 0.0f;
        q->omega = om;
    }
}

static void giro_trasero(or2_coche_t *c, const or2_param_t *p)
{
    or2_rueda_t *l = &c->r[2], *r = &c->r[3];
    float ie = p->inercia_motor, in = p->inercia_rueda[1], radio = p->radio[1];
    float libre = c->par_neto / ie * DT + c->w_motor;
    float motor = libre, rel = 1.0f, ef = 0.0f, emb = 0.0f, inv = 1.0f / in;
    float ol, orr, par2, media, eq, comb, lim, cambio, corr, cand, dm, res;
    int32_t rpm, dif, fra, resid;
    if (c->marcha) {
        rel = rel_total(p, c->marcha); ef = rel * rel * ie;
        emb = c->emb_par * rel; motor = motor / rel;
    }
    ol = l->omega - l->f_lon * inv * radio * DT - l->freno * inv * DT;
    orr = r->omega - r->f_lon * inv * radio * DT - r->freno * inv * DT;
    par2 = in * 2.0f;
    media = (orr + ol) * in / par2;
    comb = par2 + ef;
    eq = (media * par2 + motor * ef) / comb;
    lim = 0.0f;
    if (emb != 0.0f) lim = emb / ef * DT;
    cambio = eq - motor;
    if (cambio > lim) cambio = lim; else if (-lim > cambio) cambio = -lim;
    motor += cambio;
    corr = (eq * comb - motor * ef) / par2 - media;
    l->omega = corr + ol;
    r->omega = corr + orr;
    cand = c->w_motor;
    if (c->marcha) { ef = ef / (rel * rel); cand = rel * motor; }
    dm = (cand - libre) / ie * ef;
    res = clampf(libre + dm, 0.0f, p->w_max);
    c->w_motor = res;
    c->rpm_ant = c->rpm_i;
    rpm = tr(res * 9.54929638f);
    c->rpm_i = rpm;
    dif = rpm - c->rpm_suave;
    fra = dif / (p->w_corte > res ? 8 : 4);
    resid = dif - fra;
    if (resid > 1500) resid = 1500; else if (resid < -1500) resid = -1500;
    c->rpm_suave = c->rpm_suave - resid + dif;
}

static void rodadura(or2_coche_t *c, const or2_param_t *p)
{
    int i;
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        int ax = i >> 1;
        float inv = 1.0f / p->rod_cuad[ax];
        float fija = p->rod_fija[ax] * q->carga;
        float d = q->f_lon;
        float per = d * d * inv + fija;
        if (c->marcha == 1) {
            /* correccion de primera del original */
            float cand = p->rel[2] / p->rel[1] * d;
            float cper = cand * cand * inv + fija;
            cand = cand - cper;
            if (cand > d - per) per = d - cand;
        }
        q->resist = -per;
    }
}

/* SetRunningResistance (PC 502120, pc_wheel_dynamics.cpp running_resistance): fuera del asfalto (materiales
   0x801c bajo la rueda, o bajo el centro del coche para las cuatro) una fuerza contra la velocidad de la rueda de
   |cos(deriva de la rueda)| * 0,175 * kmh^2 * 0,0225 kgf. Constantes 6AE4E4..6AE4F8; sin ella durante el rebote
   (+283). La parte del handicap (+DF8 / +DB4) es nula para el jugador. */
static void resistencia_tierra(or2_coche_t *c, const or2_entrada_t *e)
{
    int i, centro = (e->sup_centro & OR2_SUP_TIERRA) != 0;
    float base = 0.0f;
    if (c->c283 <= 0) { float k = (float)c->kmh; base = -(k * k * 0.0225f); }
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        float esc = 0.0f;
        if (centro || (e->sup[i] & OR2_SUP_TIERRA)) esc = 0.175f;
        q->r_tierra = esc != 0.0f ? fabsf(cosf((float)q->desliz * U2R)) * esc * base : 0.0f;
    }
}

/* --------------------------------------------- mando (PS2 0x1b3fb8 / 0x1b42e0, PC 453860 / 4537C0) */
/* filas de la velocidad de la direccion (PS2 0x3f8800, 9 x 0x18 bytes; PC 5A7C30): paso y divisor alejandose
   del centro, paso y divisor volviendo, rampa de la cruceta al pulsar y al soltar. Fila 8 fuera de la carrera. */
static const struct { int32_t paso, div, paso2, div2; float in, out; } fila_vol[9] = {
    { 11, 4, 16, 3, 0.0599999987f, 0.0710000023f }, { 12, 4, 17, 3, 0.0700000003f, 0.0825000033f },
    { 14, 4, 18, 3, 0.0799999982f, 0.0939999968f }, { 15, 4, 19, 3, 0.0900000036f, 0.105499998f },
    { 17, 4, 20, 3, 0.100000001f, 0.116999999f },   { 18, 4, 21, 3, 0.109999999f, 0.129000008f },
    { 19, 4, 22, 3, 0.120000005f, 0.141000003f },   { 20, 4, 23, 3, 0.129999995f, 0.152999997f },
    { 21, 4, 25, 3, 0.140000001f, 0.165000007f } };

int or2_volante(or2_coche_t *c, int eje, int izq, int der, int opcion, int en_carrera, int es_volante)
{
    /* zona muerta: 0,25 con el mando (PS2 0x3e800000; el PC usa 0,2) y 1/128 con volante */
    float dz = es_volante ? 0.0078125f : 0.25f, v, a, x, st;
    int f = (opcion < 0 || opcion > 7) ? 4 : opcion, vol, prev = c->vol_ant, d;
    int t = en_carrera ? f : 8;
    v = (float)(eje * 256) * 3.05175781e-05f;
    if (dz > fabsf(v)) a = 0.0f;
    else a = (v > 0.0f ? v - dz : v + dz) / (1.0f - dz);
    /* cruceta: rampa (bits 0x80 / 0x100 del mando) que vuelve sola al soltar */
    x = c->vol_dig;
    if (der) { x = fila_vol[t].in + x; if (x > 1.0f) x = 1.0f; }
    else if (izq) { x = x - fila_vol[t].in; if (-1.0f > x) x = -1.0f; }
    else if (x > 0.0f) { x = x - fila_vol[t].out; if (0.0f > x) x = 0.0f; }
    else if (0.0f > x) { x = fila_vol[t].out + x; if (x > 0.0f) x = 0.0f; }
    c->vol_dig = x;
    st = fabsf(x) > fabsf(a) ? x : a;
    vol = tr(st * 127.0f);
    /* 4537C0 / 0x1b42e0: sin volante, el volumen se acerca al pedido a pasos (fila de la opcion) */
    d = vol - prev;
    if (!es_volante && d != 0) {
        if ((int32_t)((uint32_t)vol * (uint32_t)prev) >= 0 && iabs(vol) > iabs(prev)) {
            int32_t p = d < -fila_vol[f].paso ? -fila_vol[f].paso : (d > fila_vol[f].paso ? fila_vol[f].paso : d);
            if (prev != 0) vol = p + prev;
            else { int32_t q = p / fila_vol[f].div; vol = (q != 0 ? q : (d > 0 ? 1 : -1)) + prev; }
        } else {
            int32_t p = d / fila_vol[f].div2;
            if (p == 0) p = d > 0 ? 1 : -1;
            if (p < -fila_vol[f].paso2) p = -fila_vol[f].paso2; else if (p > fila_vol[f].paso2) p = fila_vol[f].paso2;
            vol = p + prev;
        }
    }
    c->vol_ant = vol;
    return vol;
}

float or2_mu_superficie(uint32_t sup)
{
    if (sup == 1u) return 0.0f;
    if (sup == 4u || sup == 8u || sup == 16u || sup == 0x8000u) return 0.699999988f;   /* PC 5C403C */
    return 1.0f;
}

/* -------------------------------------------- RearGripCtrl (pcc:60-110) */
static float curva_agarre(const or2_coche_t *c, const or2_param_t *p, int idx)
{
    float base = p->mu_min[idx] + 2.0f, obj, mezcla, t;
    if (c->d36 <= 0) return base;
    /* event+0xd94: reduccion con el motor al corte en manual (180 cuadros), t = d94 * 0,1107 hasta 20 */
    t = clampf((float)c->d94 * 0.110741965f, 0.0f, 20.0f);
    obj = t + p->curva_der[idx];
    mezcla = fabsf(sinf((float)c->d46 * U2R));
    return mezcla * (obj - base) + base;
}

static void ctrl_agarre_trasero(or2_coche_t *c, const or2_param_t *p, float ofs)
{
    int32_t st, as;
    if (c->d36 > 0) c->d36--;
    if (c->pedal_fr > 10 && c->pedal_fr_ant <= 10 && c->pedal_ac < 250) c->d36 = 40;
    if (c->pedal_ac < 250 && c->pedal_ac_ant >= 250 && c->pedal_fr > 10) c->d36 = 40;
    if (c->manual) {
        if (c->d94 > 0) c->d94--;
        if (c->d98 > 0) c->d98--;
        if (c->d9c > 0) c->d9c--;
    }
    c->d38 = 1.0f;
    st = c->s4e; as = iabs(st);
    if (c->kmh != 0 && as >= 0x300 && c->d36 > 0) {
        int32_t prod;
        float x, q, amt;
        if (as > 0x600) c->d36 = 12;
        prod = (int32_t)c->s16a * st;
        c->d38 = prod < 0 ? p->grip_baja : (prod > 0 ? p->grip_sube : p->grip_media);
        if (st > 0) x = (float)(c->stick16 - 0x7f00) * -1.53789370e-05f;
        else x = (float)(c->stick16 + 0x7f00) * 1.53789370e-05f;
        x = cl01(x);
        q = 1.0f - (float)c->pedal_ac * 0.00392156886f;
        amt = (1.0f - q * x) * (p->grip_amax - p->grip_amin) + p->grip_amin;
        c->d38 -= amt;
    }
    /* work+0x244 bit 3: contacto con el muro en el cuadro anterior */
    if (c->roce > 0) {
        float cand = fabsf(sinf((float)st * U2R)) * 0.25f + 0.75f;
        if (cand > c->d38) c->d38 = cand;
    }
    if (as > 0x3800) {
        /* event+0x26c: desplazamiento lateral (+ = lado derecho aqui) */
        if ((st > 0 && ofs > 0.0f) || (st < 0 && ofs < 0.0f)) c->d38 = 1.0f;
    }
    c->d38 = cl01(c->d38);
}

/* -------------------------------------------- SlipAngleCtrl (pcc:173-260) */
static void ctrl_deriva(or2_coche_t *c, const or2_param_t *p)
{
    float v = c->vel_mf, cl, t, a0, a1, b0, b1, coef0, coef1, curva1, ajuste, grip;
    float fuerza, q, sp, ratio, curva0, shz, op, fade, ninv, mezcla, mez;
    int32_t dot_i, edi, d, negold, oldi, odel, delta, nd, maxd, ang, inp, idif, a, sa;
    int32_t esc, ebx, e, edi2, d48;
    int16_t objetivo;
    cl = clampf(v, 0.922849774f, 1.84569955f);
    t = (cl - 0.922849774f) * 1.08359993f;
    if (c->d36 >= 12) { a0 = p->c0_der[0]; b0 = p->c0_der[1]; a1 = p->c1_der[0]; b1 = p->c1_der[1]; }
    else { a0 = p->c0_nor[0]; b0 = p->c0_nor[1]; a1 = p->c1_nor[0]; b1 = p->c1_nor[1]; }
    coef0 = (b0 - a0) * t + a0;
    coef1 = (b1 - a1) * t + a1;
    /* giro del cuadro en unidades (work+0x50 . normal) */
    dot_i = s16(tr(c->w * DT * R2U));
    edi = -(int32_t)s16(tr(cosf((float)c->d46 * U2R) * p->k_guinada * (float)dot_i));
    curva1 = curva_agarre(c, p, 1);
    /* ajuste por la deriva que pide la curvatura de la carretera (d4a) */
    if (c->d4a < 0) {
        if (c->d46 < c->d4a)
            ajuste = (float)(c->d46 - c->d4a) * p->adj_pend / (float)(-0x4000 - c->d4a) + p->adj_fuera;
        else if (c->d46 > 0)
            ajuste = (float)c->d46 * p->adj_opu_k * 6.10351562e-05f + p->adj_opu;
        else ajuste = p->adj_dentro;
    } else {
        if (c->d46 > c->d4a)
            ajuste = (float)(c->d46 - c->d4a) * p->adj_pend / (float)(0x4000 - c->d4a) + p->adj_fuera;
        else if (c->d46 < 0)
            ajuste = p->adj_opu - (float)c->d46 * p->adj_opu_k * 6.10351562e-05f;
        else ajuste = p->adj_dentro;
    }
    grip = cl01(c->d38 + ajuste);
    fuerza = grip * curva1 * p->masa * 0.5f;
    q = p->masa * 0.0509858094f;
    sp = v * HZ; if (sp < 0.01f) sp = 0.01f;
    fuerza = fuerza / q / (sp * sp) * p->batalla;
    /* objetivo = atan(L*mu*g/v^2): angulo de giro que permite el agarre */
    objetivo = s16(tr(atanf(fuerza) * R2U));

    oldi = c->d46;
    d = tr((float)objetivo * coef1);
    negold = -oldi;
    if (negold < 0) { d = -d; if (d < negold) d = negold; }
    else if (d > negold) d = negold;
    odel = c->d48;
    delta = tr((float)d / coef1);
    delta = (delta - odel + edi) / 4;
    nd = odel + delta;
    maxd = tr(p->max_dslip);
    if (nd < -maxd) nd = -maxd; else if (nd > maxd) nd = maxd;
    nd = s16(nd);
    if (oldi + nd < -0x3400) nd = -0x3400 - oldi;
    if (oldi + nd > 0x3400) nd = 0x3400 - oldi;
    c->d48 = s16(nd);
    c->d46 = s16(oldi + c->d48);

    /* angulo delantero maximo para el radio minimo con deriva d46 */
    ratio = p->masa / (1.0f / p->reparto + 1.0f);
    curva0 = curva_agarre(c, p, 0);
    shz = v * HZ;
    ratio = ratio * 0.101971619f / (curva0 * ratio) * (shz * shz);
    {
        float af = (float)c->d46 * U2R, cf = cosf(af), base = ratio / p->batalla;
        float den = base - sinf(af);
        float arg = den != 0.0f ? cf / den : 1.0e6f;
        edi = s16(tr(atanf(arg) * R2U));
    }
    inp = c->s32; ang = c->d46;
    op = 0.0f;
    if ((inp - ang) * inp < 0) op = cl01((float)iabs(c->stick16) * 3.07578739e-05f);
    c->d3c = c->d3c + (op - c->d3c) * 0.1f;
    fade = 1.0f - c->d3c;
    a = tr(fade * (float)edi);
    idif = inp - ang;
    sa = a; if (idif < 0) sa = -sa;
    esc = tr((float)sa * coef0);
    if (idif < 0) { if (esc < idif) esc = idif; } else if (esc > idif) esc = idif;
    ninv = -1.0f / coef0;
    ebx = -(idif + tr((float)esc * ninv));
    if (ang * inp > 0 && iabs(ang) > iabs(inp)) a = 0;
    if (inp < 0) a = -a;
    e = tr((float)a * coef0);
    if (inp < 0) { if (e < inp) e = inp; } else if (e > inp) e = inp;
    e = tr((float)e * ninv);
    edi2 = ang - e - inp;
    d48 = c->d48;
    mez = 0.0f;
    if (d48 * ang >= 0) mez = cl01((float)iabs(d48) * 0.00390625f);
    mezcla = (1.0f - mez) * (float)ebx + (float)edi2 * mez;
    c->d44 = s16(tr(mezcla));
    if (c->c283 > 0) { c->d44 = 0; c->d46 = 0; }     /* durante el rebote */
}

/* ----------------------------------------------- vectores 2D en el mundo */
typedef struct { float x, y; } v2;
static v2 mk(float x, float y) { v2 r; r.x = x; r.y = y; return r; }
static v2 dir_ang(float a) { return mk(-sinf(a), cosf(a)); }   /* + = izquierda */
static float lon2(v2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
static v2 unidad(v2 a)
{
    float l = lon2(a);
    if (l > 0.0001f) { a.x /= l; a.y /= l; }
    return a;
}
static float cruz(v2 a, v2 b) { return a.x * b.y - a.y * b.x; } /* >0: b a la izq. de a */

static void avanza_choque(or2_coche_t *c);
static const or2_choques_t *choques_act;   /* datos de choques (para las animaciones), los da or2_choque_coche */

/* CheckWanderer (4A5260): con las cuatro ruedas fuera del asfalto (+244 sin otro material que tierra, arena o
   hierba; en el nivel 0 tambien el 7) +E90 = 1 y el contador +E88 sube de 2 en 2 hasta 120; al volver, +E98 =
   cuadros que faltan para el borde al ritmo de la velocidad lateral, y el contador baja de 1 en 1. Se anula en
   el rebote, en choque, recuperandose y en derrape */
static void ayuda_tierra(or2_coche_t *c, const or2_entrada_t *e, int bordes)
{
    uint32_t sup = e->sup[0] | e->sup[1] | e->sup[2] | e->sup[3], ok = e->nivel != 0 ? 0x801cu : 0x809cu;
    int ant = c->e90;
    if (e->sin_ayuda_tierra || !sup) { c->e88 = c->e90 = 0; return; }   /* sin datos de superficie */
    if (!(sup & ~ok)) { c->e90 = 1; c->e94 = c->ofs; }
    else {
        c->e90 = 0;
        if (ant) {
            v2 h = dir_ang((float)c->d4c * U2R);
            float vl = cruz(h, mk(c->vx, c->vy));           /* velocidad lateral (m/s), + hacia la izquierda */
            c->e98 = 3.40282347e38f;
            if (!(1.1920929e-7f > fabsf(vl)) && bordes) {
                /* cross(lateral, rumbo).y >= 0 (hacia la derecha): +264 / |v| * -60,2; si no, +268 / |v| * 60,2 */
                if (vl <= 0.0f) c->e98 = e->dist_der / -vl * 60.2f;
                else c->e98 = e->dist_izq / vl * 60.2f;
            }
        }
    }
    if (c->e90) { c->e88 += 2; if (c->e88 > 120) c->e88 = 120; c->e8c = c->e88; }
    else { c->e88--; if (c->e88 < 0) c->e88 = 0; }
    if (c->c283 > 0 || c->t_choque > 0.0f || c->t_recup > 0.0f || c->d36 > 0) c->e88 = 0;
}

/* AssistWanderer (4A5470): al volver al asfalto gira la velocidad y la carroceria hacia el rumbo de la
   carretera (o media curva mas alla si va hacia el lado en que se salio), con una rampa triangular de +E8C
   cuadros escalada por +E98 * 0,1333 */
static void asiste_tierra(or2_coche_t *c, v2 *vel)
{
    int32_t mitad, dif;
    float r;
    int16_t obj;
    if (c->e90 || c->e88 <= 0 || c->e8c < 30) return;
    mitad = (c->e8c - (c->e8c >> 31)) / 2;
    if (c->e88 >= mitad) r = 2.0f - (float)c->e88 / (float)mitad;
    else r = (float)c->e88 / (float)mitad;
    r = cl01(r / (c->e98 * 0.13333334f));
    obj = c->d4c;
    dif = s16((int32_t)c->d4e - c->d4c);
    if (iabs(dif) > 0x400 && (float)dif * c->e94 >= 0.0f) obj = s16(obj + ((dif - (dif >> 31)) >> 1));
    {
        float sp = lon2(*vel);
        int16_t d1 = s16((int32_t)obj - c->rumbo_v16), d2 = s16((int32_t)obj - s16(tr(c->psi * R2U)));
        float a1 = (float)s16(tr((float)d1 * r)) * U2R, a2 = (float)s16(tr((float)d2 * r)) * U2R;
        v2 d = dir_ang((float)c->rumbo_v16 * U2R + a1);
        if (sp > 0.0001f) *vel = mk(d.x * sp, d.y * sp);
        c->psi = angw(c->psi + a2);
    }
}

/* ColiCar (car_sus_coli_check + car_sus_bump_push, sobre un suelo plano a la altura 0), CalcSuspensionForce
   (4A1B70) y CalcTireLoad (4A1A90 con assSpecificAmountOfTireLoad 518120 y assDiagonalTireLoad 518240).
   Alturas en el suelo con angulos pequenos: h + y - z * cabeceo + x * balanceo. */
static void suspension(or2_coche_t *c, const or2_param_t *p, int suelo)
{
    const float apr = SUS_APRIETE + 1.0f;
    float push = 0.0f, real = 0.0f, ref = 0.0f, inv, dif, izq, der;
    int i;
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        int ax = i >> 1;
        float x, y, z, hc, pb;
        rueda_pc(p, i, &x, &y, &z);
        /* CommonPlCar: +28 = +08 - extension maxima; la rueda baja hasta el suelo */
        q->y28 = y - p->sus_sup[ax];
        hc = c->alt + (q->y28 - p->radio[ax]) - z * c->cabeceo + x * c->balanceo;
        if (suelo && 0.0f >= hc) { q->y28 += -hc; q->aire = 0; }
        else q->aire = 1;
        /* car_sus_bump_push: con la suspension a tope la carroceria sube lo que haga falta */
        pb = -(c->alt + y - (p->radio[ax] + p->sus_inf[ax]) - z * c->cabeceo + x * c->balanceo);
        if (suelo && pb > push) push = pb;
    }
    c->alt += push;
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        float x, y, z, cmp, ant;
        rueda_pc(p, i, &x, &y, &z);
        cmp = y - q->y28;
        if (0.0f > cmp) cmp = 0.0f;
        ant = q->ext;
        q->ext = cmp;
        q->ext_vel = (cmp - ant) * HZ;
        q->ext_ant = ant;
    }
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        int ax = i >> 1;
        float cmp = q->ext, k = p->sus_muelle[ax], f, d, av, rt, am;
        f = 0.0f - (cmp - p->sus_centro[ax]) * k * apr;
        f -= (cmp - c->r[i ^ 1].ext) * p->sus_estab[ax] * apr;
        if (0.0f > cmp - p->sus_inf[ax]) f -= k * (cmp - p->sus_inf[ax]) * apr;
        if (cmp - p->sus_sup[ax] > 0.0f) f -= k * (cmp - p->sus_sup[ax]) * apr;
        av = fabsf(q->ext_vel);
        d = (av * av * av + 1.0f) * av * 0.5f;
        if (d > 1.0f) d = (sqrtf(av) + av) * 0.5f;
        rt = cl01(fabsf((cmp - p->sus_centro[ax]) * SUS_RECORR)) * SUS_FORMA;
        am = (1.0f - rt) * SUS_AMORT * d;
        am = q->ext_vel >= 0.0f ? -am : am * SUS_REBOTE;
        q->f_sus = (am + f) * SUS_ESCALA;
    }
    /* CalcTireLoad: la fuerza del muelle (>= 0) sobre la normal del suelo / 6, nada en el aire o en el rebote */
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        float y = 0.0f;
        if (!q->aire && c->c283 <= 0) {
            y = q->f_sus > 0.0f ? q->f_sus : 0.0f;
            y *= cosf(c->cabeceo) * cosf(c->balanceo);
        }
        q->carga = y / apr;
        real += q->carga; ref += q->carga_ref;
    }
    inv = 1.0f / ref; dif = real - ref;
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        q->carga -= q->carga_ref * inv * dif;
        if (0.0f > q->carga) q->carga = 0.0f;
    }
    izq = c->r[0].carga + c->r[3].carga; der = c->r[1].carga + c->r[2].carga; dif = izq - der;
    c->r[0].carga -= c->r[0].carga_ref / p->masa * dif;
    c->r[1].carga += c->r[1].carga_ref / p->masa * dif;
    c->r[2].carga += c->r[2].carga_ref / p->masa * dif;
    c->r[3].carga -= c->r[3].carga_ref / p->masa * dif;
    for (i = 0; i < 4; ++i) if (0.0f > c->r[i].carga) c->r[i].carga = 0.0f;
}

/* -------------------------------------------------------- paso de 1/60.2 */
static void tick(or2_coche_t *c, const or2_param_t *p, const or2_entrada_t *e)
{
    int i;
    float mu = (e->mu > 0.0f) ? e->mu : 1.0f;
    float sp = sinf(c->psi), cp = cosf(c->psi);
    v2 fwd = mk(-sp, cp), izq = mk(-cp, -sp);
    float u = c->vx * fwd.x + c->vy * fwd.y;      /* velocidad en el cuerpo */
    float wl = c->vx * izq.x + c->vy * izq.y;
    float ofs, fbx = 0.0f, fby = 0.0f, tau = 0.0f, blend, ax, ay, alpha, tau_cab = 0.0f, tau_bal = 0.0f;
    v2 v0 = mk(c->vx, c->vy), vp, cl = mk(0.0f, 0.0f);
    float w_p, psi_p, cang = 0.0f;
    int tiene_bordes, sin_ruedas, sin_arcade;

    /* --- entradas (OperationInput 49FAD0 = PS2 0x17ef10): +34 = volumen 1, +38 = volumen 2 (255 mientras se
       recupera de un choque, +2F8 > 0), +202 = volumen 0 << 8 --- */
    c->pedal_ac_ant = c->pedal_ac; c->pedal_fr_ant = c->pedal_fr;
    c->pedal_ac = tr(cl01(e->acelerador) * 255.0f + 0.5f);
    c->pedal_fr = tr(cl01(e->freno) * 255.0f + 0.5f);
    if (e->volumen_ok) c->stick16 = (e->volumen < -128 ? -128 : (e->volumen > 127 ? 127 : e->volumen)) * 256;
    else c->stick16 = tr(clampf(e->stick, -1.0f, 1.0f) * 127.0f) * 256;
    c->ac_motor = c->pedal_ac;
    c->freno_op = c->t_recup > 0.0f ? 255 : c->pedal_fr;
    c->manual = e->manual != 0;
    tiene_bordes = (e->dist_izq + e->dist_der) > 0.01f;
    /* event+0x26c con signo: <0 lado izquierdo, >0 lado derecho */
    ofs = tiene_bordes ? (e->dist_izq - e->dist_der) * 0.5f : 0.0f;
    c->ofs = ofs;
    if (c->manual) aviso_cambio(c, p);                  /* CheckShiftWarning 4A50F0 */
    ayuda_tierra(c, e, tiene_bordes);                   /* CheckWanderer 4A5260 */
    /* estado de choque y su animacion (4A2400 / pc_advance_crash_state) */
    avanza_choque(c);
    if (c->roce > 0) c->roce--;
    if (c->d22 > 0) c->d22--;                           /* 0x17bfa8: +D22 y +DA4 bajan uno por cuadro */
    if (c->da4 > 0) c->da4--;
    /* 4F9450: derrapando (deriva trasera de 5,6 a 90 grados a 100 km/h del modelo) se atraviesa el trafico
       sin reaccion durante 60 cuadros */
    {
        int32_t a = iabs(c->s4e);
        if (c->kmh >= 100 && a > 0x400 && a < 0x4000) c->paso = 60;
        else if (c->paso > 0) c->paso--;
    }

    /* --- velocidades de rueda (tire_velocity) --- */
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        float x, y, vu, vv, l;
        pos_rueda(p, i, &x, &y);
        vu = u - c->w * y;
        vv = wl + c->w * x;
        l = sqrtf(vu * vu + vv * vv);
        q->vel = l;
        if (l >= 0.0001f) {
            q->vdx = vu / l; q->vdy = vv / l;
            q->ang_vel = s16(tr(atan2f(vv, vu) * R2U));
        } else {
            q->vdx = cosf((float)q->ang * U2R); q->vdy = sinf((float)q->ang * U2R);
            q->ang_vel = q->ang;
        }
    }

    /* --- direccion (steering_operation, pc_steering.cpp:29-93) --- */
    {
        float b = p->dir_k2 / p->dir_g * 32768.0f;
        int32_t base = tr(-1.0f / b * (float)c->stick16 * p->dir_k1);
        c->s32 = s16(base);
        if (c->kmh > 20) {
            int32_t road = ((int32_t)c->r[2].ang_vel + (int32_t)c->r[3].ang_vel) / 2;
            int16_t r16 = s16(road + (int32_t)c->s16a * 8), b16 = s16(base);
            int done = 0;
            if (r16 < -0x2000) r16 = -0x2000; else if (r16 > 0x2000) r16 = 0x2000;
            if ((int32_t)b16 * r16 > 0) {
                int32_t dif = (int32_t)r16 - b16, in16 = c->stick16, corr;
                if (in16 < 0) in16 = -in16;
                if (in16 > 0x7000) in16 = 0x7000;
                corr = in16 * dif / 0x7000;
                if ((int32_t)r16 * corr > 0) { c->s32 = s16(base + corr); done = 1; }
            }
            if (!done && c->d36 > 0) c->s32 = s16(base + c->s16a);
        }
    }

    /* --- toe_angle --- */
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        int ax = i >> 1;
        float sg = (i & 1) ? -1.0f : 1.0f, d = q->f_lon;
        float pos = d > 0.0f ? d : 0.0f, neg = d > 0.0f ? 0.0f : d;
        float nrm = cl01((float)iabs(c->stick16) * 3.07578739e-05f);
        float rem = 1.0f - (float)c->pedal_ac * 0.00392156886f;
        q->toe_ang = p->toe[ax] * sg;
        q->toe_comp = p->comp_trac[ax] * pos + p->comp_base[ax] - p->comp_fren[ax] * neg +
                      p->comp_lev[ax] * rem + p->comp_dir[ax] * nrm;
    }

    /* --- tire_direction_angles --- */
    {
        int16_t of = c->s4c, orr = c->s4e;
        for (i = 0; i < 4; ++i) {
            or2_rueda_t *q = &c->r[i];
            int32_t a = tr(q->toe_ang * R2U);
            if (i < 2) a += c->s32;
            q->ang = s16(a);
            q->desliz = s16((int32_t)q->ang_vel - q->ang);
        }
        c->s4c = s16(((int32_t)c->r[0].desliz + c->r[1].desliz) / 2);
        c->s4e = s16(((int32_t)c->r[2].desliz + c->r[3].desliz) / 2);
        c->s168 = s16(c->s4c - of);
        c->s16a = s16(c->s4e - orr);
    }

    /* --- transmision, motor y frenos --- */
    if (e->manual) cambio_manual(c, p, e->cambio_pide);
    else cambio_auto(c, p);
    acelerador_op(c, p);
    reparte_freno(c, p);
    embrague_auto(c, p);
    par_motor(c, p);

    /* --- cargas: las de la suspension del paso anterior (CalcTireLoad 4A1A90, al final de este paso) --- */

    /* --- mu del firme bajo cada rueda (GetRoadMu 500B30 + assMuControl 518420, pc_road_mu.cpp) --- */
    for (i = 0; i < 4; ++i) c->r[i].mu = e->sup[i] ? or2_mu_superficie(e->sup[i]) : mu;
    {
        or2_rueda_t *q = c->r;
        if (q[2].desliz < 0 && q[1].mu > q[0].mu) q[1].mu = q[0].mu;
        if (q[3].desliz > 0 && q[0].mu > q[1].mu) q[0].mu = q[1].mu;
        if (q[0].mu > q[2].mu) q[2].mu = q[0].mu;
        if (q[1].mu > q[3].mu) q[3].mu = q[1].mu;
    }

    /* --- agarre, rigidez y fuerza lateral --- */
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        int ax = i >> 1;
        float rat = q->carga / q->carga_ref, mx = p->mu_max[ax];
        float coef = clampf(mx - (mx - p->mu_min[ax]) * rat * rat, 0.0f, mx);
        float x, load, ang, f, esc;
        int32_t a;
        /* tire_grip: sin agarre durante el rebote (+283) */
        q->agarre = (e->en_suelo && c->c283 <= 0) ? q->carga * q->mu * coef : 0.0f;
        /* cornering_power: por debajo de 20 km/h del modelo y con +D90 (60 cuadros tras el rebote o el choque)
           la rigidez de deriva baja a la centesima */
        x = p->rigidez[ax];
        if (c->kmh < 20 && c->d90 > 0) x *= 0.00999999978f;
        x = 1.0f / x + q->toe_comp;
        q->comp = x > 0.0f ? x : 0.0f;
        a = s16((int32_t)q->ang - q->ang_vel);
        if (a > 0x4000 || a < -0x4000) a = s16(-32768 - a);
        load = q->vel * q->carga_ref * 6.13869143f;
        ang = (float)a * U2R;
        if (0.0001f > load) { q->f_lat = 0.0f; continue; }
        f = (ang - q->comp * ang / (1.0f / load + q->comp)) * load;
        esc = load / fabsf(f);
        if (1.0f > esc) f *= esc;
        q->f_lat = f;
    }
    fuerza_delantera(c, p);
    fuerza_trasera(c, p);
    circulo_friccion(c);
    giro_delantero(c, p);
    giro_trasero(c, p);
    rodadura(c, p);
    resistencia_tierra(c, e);

    /* --- copy_physical_work + make_force_work_tire: fuerzas al cuerpo ---
       (cuerpo: x adelante, y izquierda; kgf). La correccion 0xa0 quita
       la componente de arrastre de la fuerza lateral durante el derrape
       y en coches con 0x20a8 = 0; se aplica sin par, como el original. */
    blend = cl01(1.0f - (float)c->d36 * 0.0833333358f) * p->arrastre_lat;
    /* make_force_work_tire: en choque solo empujan las ruedas en los estados 3, 6 y 7 */
    sin_ruedas = c->t_choque > 0.0f && !(c->choque == 3 || c->choque == 6 || c->choque == 7);
    /* CalcPlBody_2nd: en choque (salvo 3 y 6) se anulan las aceleraciones de las capas arcade */
    sin_arcade = c->t_choque > 9.99999975e-05f && !(c->choque == 3 || c->choque == 6);
    for (i = 0; i < 4; ++i) {
        or2_rueda_t *q = &c->r[i];
        float th = (float)q->ang * U2R, hx = cosf(th), hy = sinf(th);
        float lx = -hy, ly = hx, vx = q->vdx, vy = q->vdy;
        float appx = q->f_lat * lx, appy = q->f_lat * ly;
        float sx = appx + q->f_lon * hx, sy = appy + q->f_lon * hy;
        float along = sx * vx + sy * vy + q->resist;    /* 0x7c (escalar) */
        float lat = sx * (-vy) + sy * vx;               /* 0x88 (escalar) */
        float f7x = vx * along, f7y = vy * along;
        float f8x = -vy * lat, f8y = vx * lat;
        float pj = appx * vx + appy * vy, ajx, ajy, la, x, y;
        ajx = appx - pj * vx + pj * vx * blend;
        ajy = appy - pj * vy + pj * vy * blend;
        la = sqrtf(ajx * ajx + ajy * ajy);
        if (la > 0.0001f) { la = fabsf(q->f_lat) / la; ajx *= la; ajy *= la; }
        /* make_force_work_tire: ni las ruedas en el aire ni las delanteras si una trasera lo esta */
        if (!e->en_suelo || sin_ruedas || q->aire || (i < 2 && (c->r[2].aire || c->r[3].aire))) {
            q->fx = q->fy = 0.0f; continue;
        }
        pos_rueda(p, i, &x, &y);
        q->fx = f7x + f8x + (ajx - appx);
        q->fy = f7y + f8y + (ajy - appy);
        tau += x * (f7y + f8y) - y * (f7x + f8x);
        /* cabeceo y balanceo de 0x7C y 0x88 aplicadas en el punto de contacto (+2C bajo el CG) */
        tau_cab += -(q->y28 - p->radio[i >> 1]) * (f7x + f8x);
        tau_bal += (q->y28 - p->radio[i >> 1]) * (f7y + f8y);
        /* empuje por marcha (filas 124..129) si 0x7c va a favor de la marcha; en los preparados (modelo >= 15)
           en la ultima marcha con el limitador por debajo de 0,975 no hay (fuente = +DC0 = 1) */
        if (c->marcha > 0 && (f7x * u + f7y * wl) > 0.0f) {
            float fac = p->empuje[c->marcha] - 1.0f, v2l = u * u + wl * wl;
            if (c->marcha == p->marchas && p->modelo >= 15 && c->limitador < 0.975000024f) fac = 0.0f;
            if (fac < 0.0f) fac = 0.0f;
            if (v2l >= 0.0001f) {
                float k = (f7x * fac * u + f7y * fac * wl) / v2l;
                q->fx += k * u; q->fy += k * wl;
            }
        }
        fbx += q->fx; fby += q->fy;
        /* +94: resistencia de tierra, en el centro de la rueda sin par (set_force_obsolete con flags 1) */
        fbx += q->r_tierra * vx; fby += q->r_tierra * vy;
    }
    /* arrastre aerodinamico (calc_game_resist), kgf */
    {
        float v2l = u * u + wl * wl, vm = sqrtf(v2l);
        if (vm > 0.0001f) {
            float f = v2l * p->cx * p->area * 0.0600224324f * (1.0f - c->rebufo_k);   /* menos con rebufo */
            fbx -= u / vm * f; fby -= wl / vm * f;
        }
        /* force_work_49fed0: +282 cuadros de frenada tras un golpe, masa * 4 * (0, .25, .5, .75, 1) kgf contra
           la velocidad, menos por debajo de 0.1384 m/cuadro */
        if (c->c282 > 0) {
            static const float tabla[5] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
            int k = c->c282 > 4 ? 4 : c->c282;
            float am = tabla[k] * p->masa * 4.0f, d = vm * DT;
            float r = am / (0.138427466f > d ? 0.138427466f : d);
            c->c282--;
            fbx -= r * u * DT; fby -= r * wl * DT;
        }
    }
    /* RebPlBody (4A62E0): durante el rebote la velocidad sigue el rumbo +286 (sin perder modulo), el cuerpo
       gira hacia la carretera y no hay capas arcade */
    if (c->c283 > 0) {
        int cnt = c->c283;
        float sp0 = lon2(v0);
        int16_t yaw = s16(tr(c->psi * R2U)), obj;
        int32_t q1, q2, raw;
        v2 d;
        if (c->rebote == 1) {
            int16_t dl = s16((int32_t)c->d4e - c->d4c), mid;
            obj = c->d4e;
            if (iabs(dl) > 0x400 && (float)dl * ofs < 0.0f) obj = c->d4c;
            mid = s16((int32_t)obj - (int32_t)s16((int32_t)obj - c->d4c) / 2);
            c->h286 = s16((int32_t)c->h286 + (int32_t)s16((int32_t)mid - c->h286) / cnt);
            raw = s16((int32_t)obj - yaw);
            q1 = raw / cnt;
            q2 = (int32_t)s16(raw - q1) / cnt;
            c->psi = angw(c->psi + (float)s16(q1 + q2) * U2R);
        } else {
            c->psi = angw(c->psi + (float)((int32_t)c->h288 / 30) * U2R);
        }
        d = dir_ang((float)c->h286 * U2R);
        v0 = mk(d.x * sp0, d.y * sp0);
        /* RebPlBody_Sub2 (4A0320): correccion lateral de la posicion hacia el centro de la calzada, o a 2 m
           dentro del borde mas cercano si queda mas cerca; se resta lo que ya mueve la velocidad lateral
           (cada vez menos) y se reparte con un perfil coseno de 3 grados por cuadro que acaba en el ultimo
           cuadro del rebote. Si pide mas que la velocidad de un cuadro no se aplica. Sin los tramos de las
           etapas 0x12 y 0x1C en que el original se la salta. */
        if (c->rebote == 1 && tiene_bordes) {
            v2 h = dir_ang((float)c->d4c * U2R), der = mk(h.y, -h.x);
            float dl2 = -ofs, a1, b1, am, fn, fp, corr;
            if (!(1.1920929e-7f > fabsf(ofs))) {
                a1 = e->dist_der - 2.0f;             /* (+264 + 2) / +26C: a 2 m del borde derecho      */
                b1 = 2.0f - e->dist_izq;             /* (+268 - 2) / +26C: a 2 m del borde izquierdo    */
                if (fabsf(a1) > fabsf(b1)) a1 = b1;
                if (fabsf(dl2) > fabsf(a1)) dl2 = a1;
            }
            am = (v0.x * der.x + v0.y * der.y) * DT * (1.0f - (float)cnt * 0.0333333351f);
            dl2 -= am;
            fn = 1.0f - cosf((float)cnt * 0.0523598790f);
            fp = 1.0f - cosf((float)(cnt - 1) * 0.0523598790f);
            corr = (1.0f - fp / fn) * fabsf(dl2);
            if (!(corr > sp0 * DT)) c->corr_lat += dl2 < 0.0f ? -corr : corr;
        }
        tau = 0.0f;                              /* se quita el par alrededor de la normal (guinada) */
        sin_arcade = 1;
    }
    /* WrecPlBody (4A0C70): la grua del choque fuerte lleva el coche en linea recta al punto de la carretera
       (el que da el juego en e->grua_*) en los cuadros que quedan, sin fuerzas, y gira el cuerpo hacia el rumbo
       de la carretera; en el ultimo, PlWrecker (504900 fase 1) lo deja alli con el rumbo de la carretera, parado
       (estado 1) o con 0,67 de la velocidad y como mucho 0,7383 m/cuadro (160 km/h del modelo; estados 2 y 5) */
    if (c->grua > 0) {
        int cnt = --c->grua, k;
        int16_t yaw = s16(tr(c->psi * R2U));
        int16_t obj = e->grua_ok ? s16(tr(angw(-e->grua_rumbo) * R2U)) : c->d4c;
        c->d44 = 0; c->d46 = 0; c->s4e = 0; c->s4c = 0;
        if (cnt <= 0) {
            float vf = (c->choque == 2 || c->choque == 5) ? c->vel_mf * 0.670000017f : 0.0f;
            v2 d;
            if (vf > 0.738279819f) vf = 0.738279819f;
            c->psi = angw((float)obj * U2R);
            d = dir_ang(c->psi);
            v0 = mk(d.x * vf * HZ, d.y * vf * HZ);
            c->w = 0.0f;
            c->w_motor = p->w_ralenti;
            for (k = 0; k < 4; ++k) { c->r[k].vel = vf * HZ; c->r[k].omega = vf * HZ / p->radio[k >> 1]; }
            c->recolocar = 1;
        } else {
            v0 = e->grua_ok ? mk(e->grua_dx / (float)cnt * HZ, e->grua_dy / (float)cnt * HZ) : mk(0.0f, 0.0f);
            c->w = (float)((int32_t)s16((int32_t)obj - yaw) / cnt) * U2R * HZ;
        }
        fbx = fby = tau = 0.0f;
        sin_arcade = 1;
    }

    /* ActionForce2 sobre la copia: velocidad y guinada previstas */
    ax = (fbx * fwd.x + fby * izq.x) * G_ACC / p->masa;
    ay = (fbx * fwd.y + fby * izq.y) * G_ACC / p->masa;
    alpha = tau * G_ACC / (p->masa * p->k_inercia[1]);
    vp = mk(v0.x + ax * DT, v0.y + ay * DT);
    w_p = c->w + alpha * DT;
    psi_p = c->psi + w_p * DT;

    /* --- capas arcade --- */
    ctrl_agarre_trasero(c, p, ofs);
    ctrl_deriva(c, p);

    if (e->en_suelo && !sin_arcade) {
        float speed = lon2(vp);
        /* CorneringCtrl (4A3950): bicicleta cinematica con el angulo delantero d44+s32 y el trasero d46; el
           centro instantaneo sale de la ley de senos como en el original */
        if (!(c->s4e < -0x4000 || c->s4e > 0x4000) && c->kmh != 0) {
            float th0 = (float)((int32_t)c->d44 + c->s32) * U2R, th1 = (float)c->d46 * U2R;
            float sg = th0 > th1 ? 1.0f : -1.0f, off, A, B, ang, om, rate;
            v2 obj;
            if (fabsf(th1) > 1.57079637f) sg = -sg;
            off = sg * 1.57079637f; A = off + th0; B = off + th1;
            if (A == B) { ang = A - off; om = 0.0f; }
            else {
                float q = fabsf(sinf(B) * p->batalla / sinf(A - B));
                float x = cosf(A) * q + p->cg_del, y = sinf(A) * q, phi = atan2f(y, x);
                float quo = sinf(A) * q / sinf(phi);
                om = speed / (fabsf(quo) * sg);
                ang = phi - off;
            }
            rate = (ang - c->d40) * HZ;
            om -= rate;
            cang = (om - w_p) * HZ;
            c->d40 = ang;
            obj = dir_ang(psi_p + ang);
            cl = mk((obj.x * speed - vp.x) * HZ, (obj.y * speed - vp.y) * HZ);
        }
        /* CalcPlBody_2nd (pcc:886-925): mezcla por |sin deriva trasera| */
        {
            float tg = c->kmh != 0 ? fabsf(sinf((float)c->s4e * U2R)) : 0.0f;
            c->d54 = cl01((tg - c->d54) * 0.5f + c->d54);
            cl.x *= c->d54; cl.y *= c->d54; cang *= c->d54;
        }
        /* limitador de fuerza lateral total fuera de derrape */
        if (c->d36 <= 0) {
            float thr = ((p->mu_min[1] + p->mu_min[0]) * 0.5f + 2.0f) * G_ACC;
            v2 d = mk((vp.x + cl.x * DT - v0.x) * HZ, (vp.y + cl.y * DT - v0.y) * HZ);
            v2 dv = unidad(v0);
            float pr = d.x * dv.x + d.y * dv.y, ln, ex;
            if (lon2(v0) > 0.0001f) { d.x -= pr * dv.x; d.y -= pr * dv.y; }
            ln = lon2(d);
            d = unidad(d);
            ex = ln - thr;
            if (ex > 0.0f) { cl.x -= ex * d.x; cl.y -= ex * d.y; }
        }
        /* ass_compulsive_move (pcc:815-883) */
        {
            int32_t dh = iabs(s16((int32_t)c->rumbo_v16 - c->d4c));
            int16_t rd;
            int32_t obj, cur, qd;
            if (s16(dh) < 0x4000 && tiene_bordes) {
                v2 h = dir_ang((float)c->d4c * U2R), pr;
                float cy, s, coef;
                /* 1) en derrape, alinear la velocidad con la carretera */
                pr = unidad(mk(vp.x + cl.x * DT, vp.y + cl.y * DT));
                cy = cruz(h, pr);
                s = sinf((float)c->d46 * U2R);
                /* cross(rumbo, prevista).y * sin(d46) < 0 -> 0x2140 (PC 5184B0, PS2 0x187db4) */
                coef = (cy * s < 0.0f) ? p->ass_fuera : p->ass_dentro;
                coef *= fabsf(s);
                if (c->d36 <= 0) coef = s * s * p->ass_base;
                {
                    v2 n = unidad(mk(pr.x + (h.x - pr.x) * coef, pr.y + (h.y - pr.y) * coef));
                    cl = mk((n.x * speed - vp.x) * HZ, (n.y * speed - vp.y) * HZ);
                }
                /* 2) ayuda junto al borde hacia el que va el coche */
                pr = unidad(mk(vp.x + cl.x * DT, vp.y + cl.y * DT));
                cy = cruz(h, pr);
                {
                    int32_t dl = (int32_t)c->d4e - c->d4c;
                    int salta = 0;
                    if (iabs(dl) > 0x100) {
                        if (cy > 0.0f && dl < 0 && c->s4e < 0) salta = 1;
                        if (cy < 0.0f && dl > 0 && c->s4e > 0) salta = 1;
                    }
                    if (!salta) {
                        float lado = 0.0f, tmr, p1, p2, fz, s2;
                        int32_t st = c->d46, ob = (int32_t)c->s4e * 2;
                        if (ofs < 0.0f && cy > 0.0f) {
                            float den = ofs - e->dist_izq;
                            if (-den > 0.0001f) lado = ofs / den;
                        } else if (ofs > 0.0f && cy < 0.0f) {
                            float den = ofs + e->dist_der;
                            if (den > 0.0001f) lado = ofs / den;
                        }
                        if (ob > 0) { if (st > ob) st = ob; } else if (st < ob) st = ob;
                        s2 = sinf((float)st * U2R); s2 = s2 * s2;
                        tmr = cl01((float)c->d36 * 0.0833333358f);
                        p1 = p->ass_sin * s2;
                        p2 = p->ass_borde * lado;
                        fz = cl01(p->ass_tiempo * tmr * p1 * p2);
                        if (p2 > 1.0f && c->d36 > 0) {
                            float ds = clampf(fz - c->d58, -0.00999999978f, 1.0f);
                            fz = c->d58 + ds;
                        }
                        c->d58 = fz;
                        {
                            v2 n = unidad(mk(pr.x + (h.x - pr.x) * fz, pr.y + (h.y - pr.y) * fz));
                            cl = mk((n.x * speed - vp.x) * HZ, (n.y * speed - vp.y) * HZ);
                        }
                    }
                }
            }
            /* deriva pedida por la curvatura: d4a -> -5*(d4e-d4c) suavizado */
            rd = s16((int32_t)c->d4e - c->d4c);
            obj = -5 * (int32_t)rd;
            if (obj < -16383) obj = -16383; else if (obj > 16383) obj = 16383;
            cur = c->d4a;
            qd = (obj - cur) / 10;
            if (qd < -256) qd = -256; else if (qd > 256) qd = 256;
            c->d4a = s16(cur + qd);
        }
        /* atenuacion a baja velocidad (<150 km/h fisicos) */
        if (0.692137301f > c->vel_mf) {
            float k = cl01((c->vel_mf - 0.230712444f) * 2.16719985f);
            cl.x *= k; cl.y *= k; cang *= k;
        }
    }

    /* --- ActionForce2 final (con AssistWanderer, que gira la velocidad y la carroceria antes) --- */
    {
        v2 vw = v0;
        float nx, ny, l, wn = w_p + cang * DT, fs = 0.0f, tc = tau_cab, tb = tau_bal;
        asiste_tierra(c, &vw);
        nx = vp.x + cl.x * DT + (vw.x - v0.x); ny = vp.y + cl.y * DT + (vw.y - v0.y);
        l = sqrtf(nx * nx + ny * ny);
        if (l > 166.666672f) { nx *= 166.666672f / l; ny *= 166.666672f / l; }
        wn = clampf(wn, -60.2f, 60.2f);
        c->vx = nx; c->vy = ny; c->w = wn;
        c->psi = angw(c->psi + wn * DT);
        /* make_force_work_sus + gravedad + ass_press_down_against_road: altura, cabeceo y balanceo (los mismos
           pasos de ActionForce2: velocidad y despues posicion; inercias masa * 0x4c / 0x0e4) */
        for (i = 0; i < 4; ++i) {
            float x, y, z, f = c->r[i].f_sus;
            if (c->c283 > 0 && f < 0.0f) f = 0.0f;
            rueda_pc(p, i, &x, &y, &z);
            fs += f; tc += -z * f; tb += x * f;
        }
        c->v_alt += (fs - p->masa * (SUS_APRIETE + 1.0f)) * G_ACC / p->masa * DT;
        c->alt += c->v_alt * DT;
        c->w_cab += tc * G_ACC / (p->masa * p->k_inercia[0]) * DT;
        c->w_bal += tb * G_ACC / (p->masa * p->k_inercia[2]) * DT;
        c->cabeceo += c->w_cab * DT;
        c->balanceo += c->w_bal * DT;
    }
    /* RebPlBody: fin del rebote (+283) y espera (+284) */
    if (c->c283 > 0) {
        c->s4e = 0; c->s4c = 0;
        if (--c->c283 == 0) c->d90 = 60;
    } else if (c->c284 > 0) c->c284--;

    /* --- ColiCar (ruedas contra un suelo plano), CalcSuspensionForce y CalcTireLoad --- */
    suspension(c, p, e->en_suelo);

    /* --- CopyCarWork --- */
    {
        float spd = sqrtf(c->vx * c->vx + c->vy * c->vy);
        c->vel_mf = spd * DT;
        c->kmh = tr(c->vel_mf * 216.720001f);
        if (c->vel_mf > 0.0184569955f) c->rumbo_v16 = s16(tr(atan2f(-c->vx, c->vy) * R2U));
        else c->rumbo_v16 = s16(tr(c->psi * R2U));
        /* +134 + i*0xC: altura de la rueda para el dibujo (anclaje - tope de compresion, como mucho +28) */
        for (i = 0; i < 4; ++i) {
            float x, y, z, h;
            rueda_pc(p, i, &x, &y, &z);
            h = y - p->sus_inf[i >> 1];
            if (h > c->r[i].y28) h = c->r[i].y28;
            c->rueda_y[i] = p->alt_cg + h;
        }
    }
    if (c->d90 > 0) c->d90--;                            /* cola de CommonPlCar (4A82C4) */
}

/* -------------------------------------------------------------- API */
static void salidas(or2_coche_t *c, const or2_param_t *p)
{
    float spd = sqrtf(c->vx * c->vx + c->vy * c->vy), hud, rv;
    c->velocidad = spd;
    hud = p->hud * c->vel_mf * 216.720001f;
    if (!(0.0f < p->arrastre_lat) && 300.0f > hud) hud = clampf(hud * 1.01010096f, 0.0f, 300.0f);
    c->velocidad_hud = hud;
    rv = spd > 0.05f ? atan2f(-c->vx, c->vy) : c->psi;
    c->rumbo_vel = -rv;
    c->rumbo_cuerpo = -c->psi;
    c->beta = angw(c->rumbo_cuerpo - c->rumbo_vel);
    c->guinada = -c->w;
    c->angulo_rueda = -(float)c->s32 * U2R;
    c->temporizador = c->d36;
    c->derrape = c->d36 >= 12;
    c->desliz_grados = -(float)c->s4e * (360.0f / 65536.0f);
    c->rpm = c->w_motor * 9.54929638f;
}

void or2_paso(or2_coche_t *c, const or2_param_t *p, const or2_entrada_t *e, float dt)
{
    int n = 0;
    c->nivel_etapa = e->nivel;
#if defined(__sh__)
    /* la FPU del EE de la PS2 redondea hacia cero (no es IEEE): la SH4 hace lo mismo con FPSCR.RM = 1 mientras
       dura el paso (en el banco, REDONDEO=cero); el resto del programa sigue con el redondeo al mas cercano */
    unsigned int fpscr = __builtin_sh_get_fpscr();
    __builtin_sh_set_fpscr((fpscr & ~3u) | 1u);
#endif
    /* rumbos de la carretera en unidades del PC (+ izquierda) */
    c->d4c = s16(tr(angw(-e->rumbo_carretera) * R2U));
    c->d4e = s16(tr(angw(-e->rumbo_adelante) * R2U));
    if (dt <= 0.0f) {
        tick(c, p, e);
    } else {
        c->acum += dt;
        while (c->acum >= DT && n < 8) { tick(c, p, e); c->acum -= DT; ++n; }
        if (n == 8) c->acum = 0.0f;
    }
    salidas(c, p);
#if defined(__sh__)
    __builtin_sh_set_fpscr(fpscr);
#endif
}

/* ======================================================= choques */
/* PC 62812C: paso de los temporizadores de choque (bits 3C881469, no 1/60) */
#define DT_CHOQUE 0.0166112948f

/* pc_find_crash_segment + pc_interpolate_crash_keys: Hermite entre claves (t, valor, tangente de salida,
   tangente de entrada) */
static float muestra_canal(const or2_canal_t *ch, float t)
{
    int lo = 0, hi = 0, resto;
    const float *a, *b;
    float d, span, s;
    if (!ch->k || ch->n <= 0) return 0.0f;
    resto = ch->n - 1;
    while (resto && !(ch->k[hi * 4] >= t)) { lo = hi; ++hi; --resto; }
    if (!resto && t > ch->k[hi * 4]) lo = hi;
    a = ch->k + lo * 4; b = ch->k + hi * 4;
    if (lo == hi) return a[1];
    d = t - a[0]; span = b[0] - a[0];
    if (!(span > 0.0f)) return a[1];
    s = d / span;
    return d * (s - 1.0f) * (s - 1.0f) * a[3] + (2.0f * s * s * s - 3.0f * s * s + 1.0f) * a[1] +
           (3.0f * s * s - 2.0f * s * s * s) * b[1] + d * s * (s - 1.0f) * b[2];
}

/* 513650 sobre claves sueltas (las trayectorias de los objetos de las uniones, BO.DAT): n claves de 4 floats */
float or2_canal(const float *k, int n, float t)
{
    or2_canal_t ch;
    ch.k = k; ch.n = n;
    return muestra_canal(&ch, t);
}

int or2_pose(const or2_choques_t *q, int estado, float t, int lado, float tr3[3], float gr[3])
{
    int i;
    for (i = 0; i < 3; ++i) tr3[i] = gr[i] = 0.0f;
    if (!q || !q->ok || estado < 0 || estado >= OR2_ESTADOS_CHOQUE) return 0;
    if (t >= q->dur[estado]) return 0;
    for (i = 0; i < 3; ++i) {
        tr3[i] = muestra_canal(&q->pose[estado][i], t);
        gr[i] = muestra_canal(&q->pose[estado][3 + i], t);
    }
    /* +2F0 >> 7: 1 refleja guinada y balanceo, 2 cabeceo y guinada */
    if (lado == 1) { gr[1] = -gr[1]; gr[2] = -gr[2]; }
    else if (lado == 2) { gr[0] = -gr[0]; gr[1] = -gr[1]; }
    return 1;
}

/* pc_enter_crash (4A6EA0) para el coche del jugador */
static void entra_choque(or2_coche_t *c, int estado, int lado)
{
    const or2_choques_t *q = choques_act;
    if (!q || !q->ok || estado < 0 || estado >= OR2_ESTADOS_CHOQUE) return;
    c->choque = estado;
    c->lado = lado & 7;
    c->t_choque = q->dur[estado];
    c->t_pose = 0.0f;
    c->t_recup = 0.0f;
    c->golpe_sonido = 1;
}

/* pc_advance_crash_state (4A2400) */
static void avanza_choque(or2_coche_t *c)
{
    const or2_choques_t *q = choques_act;
    float sig;
    if (c->t_choque > 0.0f) {
        sig = c->t_choque - DT_CHOQUE;
        c->t_choque = sig;
        c->t_pose += DT_CHOQUE;
        if (!(0.0f < sig)) {
            c->t_choque = 0.0f;
            c->t_recup = (q && q->ok) ? q->recup[c->choque] : 0.0f;
            c->d90 = 60;
            memset(c->pose_t, 0, sizeof c->pose_t);
            memset(c->pose_r, 0, sizeof c->pose_r);
            return;
        }
        or2_pose(q, c->choque, c->t_pose, c->lado, c->pose_t, c->pose_r);
    } else if (c->t_recup > 0.0f) c->t_recup -= DT_CHOQUE;
}

static uint32_t lee32(const uint8_t *b) { uint32_t v; memcpy(&v, b, 4); return v; }

int or2_carga_choques(const void *datos, uint32_t tam, or2_choques_t *q)
{
    const uint8_t *d = (const uint8_t *)datos;
    uint32_t o = (200 * OR2_COLUMNAS + 162 + 256) * 4, n, i, k;
    if (!q) return -1;
    memset(q, 0, sizeof(*q));
    if (!d || tam < o + 4 + 256 + 4 + 300 + 8 || memcmp(d + o, "MAP1", 4)) return -2;
    memcpy(q->mapa, d + o + 4, sizeof q->mapa);
    o += 4 + 256;
    if (memcmp(d + o, "CHQ1", 4)) return -3;
    o += 4;
    q->umbral[0] = (float)lee32(d + o) * 0.00461420696f;      /* km/h del modelo -> m/cuadro (1/216.72) */
    q->umbral[1] = (float)lee32(d + o + 4) * 0.00461420696f;
    memcpy(q->rest, d + o + 8, 12);
    memcpy(q->estado_traf, d + o + 20, 32);
    memcpy(q->masa, d + o + 52, 12);
    memcpy(q->reparto, d + o + 64, sizeof q->reparto);
    o += 64 + sizeof q->reparto;
    if (o + 8 > tam || memcmp(d + o, "POS1", 4)) return -4;
    n = lee32(d + o + 4);
    o += 8;
    if (n > OR2_ESTADOS_CHOQUE) return -5;
    for (i = 0; i < n; ++i) {
        if (o + 8 > tam) return -6;
        memcpy(&q->dur[i], d + o, 4); memcpy(&q->recup[i], d + o + 4, 4);
        o += 8;
        for (k = 0; k < 6; ++k) {
            uint32_t m;
            if (o + 4 > tam) return -6;
            m = lee32(d + o); o += 4;
            if (o + m * 16 > tam) return -6;
            q->pose[i][k].n = (int)m;
            q->pose[i][k].k = (const float *)(const void *)(d + o);   /* PARAM.DAT esta alineado a 4 */
            o += m * 16;
        }
    }
    q->ok = 1;
    choques_act = q;
    return 0;
}

int or2_columna_modelo(const or2_choques_t *q, int modelo)
{
    /* 5051D0 / PS2 18D7B8: modelo >= 15 = preparado (tablas 0/3); partida empezada (35B9B8 != 0) = 1 o 0 */
    if (!q || !q->ok || modelo < 0 || modelo >= 30) return -1;
    return (int)q->mapa[modelo >= 15 ? 0 : 1][modelo % 15];
}

/* ---------------------------------------------- choques entre coches (4F9450, modulo 4F65A0..4F9E94 del PC; SLES-53998
   0x191fa0 (pasada), 0x196850 (4F9000), 0x196c40 (4F8E70), 0x196e00 (4F86C0), 0x1942d8 (4F6FE0), 0x194f48 (4F76E0),
   0x195708 (4F7BF0), 0x1960e8 (4F6820), 0x194cf8 / 0x195628 (4F65D0 / 4F67A0), 0x1ec540 / 0x1ec620 (475F70 / 476040)) */
#define EPS_CHOQUE 9.99999975e-05f   /* 5A29E0 (PS2 0x38d1b717): "esta en choque" (+2C8 / +2F8 > esto) */
#define EPS_CAMBIO 1.1920929e-07f    /* 6281F0 (PS2 0x34000000): +2C8 del jugador para el intercambio */
#define CIERRE_7   0.369139910f      /* 5B440C (PS2 0x3ebcffe8): 80 km/h del modelo, umbral del estado 7 */
#define CIERRE_G   0.138427630f      /* 5A923C (PS2 0x3e0dbfee): 30 km/h del modelo, umbral del golpe al otro */
#define TOPE_7     1.38427508f       /* 4F6820 (PS2 0x3fb12fea): 300 km/h del modelo */

/* esquina i de una caja (4F8E70): 0 (x0, z0), 1 (x0, z1), 2 (x1, z1), 3 (x1, z0) */
static void esquina(const float *cj, int i, float *x, float *z)
{
    *x = i < 2 ? cj[0] : cj[3];
    *z = (i == 0 || i == 3) ? cj[2] : cj[5];
}
/* ejes del coche en el mundo: derecha (cos r, -sin r), atras -(sin r, cos r) */
static void a_mundo(float px, float py, float r, float x, float z, float *wx, float *wy)
{
    float c = cosf(r), s = sinf(r);
    *wx = px + x * c - z * s;
    *wy = py - x * s - z * c;
}
static void a_local(float px, float py, float r, float wx, float wy, float *x, float *z)
{
    float c = cosf(r), s = sinf(r), dx = wx - px, dy = wy - py;
    *x = dx * c - dy * s;
    *z = -dx * s - dy * c;
}
/* 4F86C0: el recorrido de la esquina P1 -> P2 (ejes de la caja A) contra sus cuatro bordes; dir = diagonal hacia fuera
   de la esquina; dentro = P2 dentro de la caja (entonces se acepta el cruce hacia atras y se pega al borde). Devuelve
   el borde (-1 ninguno), su sitio s y el punto de cruce. */
static int borde_cruza(const float *cj, float p1x, float p1z, float p2x, float p2z, float dx, float dz, int dentro,
                       float *s_out, float *qx, float *qz)
{
    /* hueco: id, eje (0 x, 1 z), plano, sentido (P1 >= P2 en el eje), signo exigido a dir en el eje */
    static const int id_[4] = { 0, 2, 3, 1 }, eje_[4] = { 0, 0, 1, 1 }, pl_[4] = { 0, 3, 2, 5 };
    static const int ge_[4] = { 1, 0, 1, 0 }, sg_[4] = { 1, -1, 1, -1 };
    int k, mejor = -1; float dmin = 3.40282347e38f, sm = 0.0f, mx = 0.0f, mz = 0.0f;
    for (k = 0; k < 4; ++k) {
        float a = eje_[k] ? p1z : p1x, b = eje_[k] ? p2z : p2x, av = eje_[k] ? p1x : p1z, bv = eje_[k] ? p2x : p2z;
        float plano = cj[pl_[k]], d = b - a, t, dv, pv, lo, hi, cx, cz, dist;
        int ebx = ge_[k] ? !(a < b) : !(b < a);
        if (fabsf(d) < 1e-8f) continue;                        /* 0x197600: 0x322bcc77 */
        t = (plano - a) / d;
        if (ebx) { if (!dentro && t < 0.0f) continue; if (1.0f < t) continue; }
        else if (dentro) { if (t < 0.0f) continue; }
        else { if (t < 0.0f || 1.0f < t) continue; }
        dv = eje_[k] ? dz : dx;
        if (sg_[k] > 0 ? !(dv < 0.0f) : !(0.0f < dv)) continue;
        pv = (1.0f - t) * av + bv * t;
        if (eje_[k] == 0) { cx = plano; cz = pv; } else { cx = pv; cz = plano; }
        dist = sqrtf((cx - p1x) * (cx - p1x) + (cz - p1z) * (cz - p1z));   /* 40F140: desde P1 al punto sin pegar */
        lo = eje_[k] == 0 ? cj[5] : cj[3]; hi = eje_[k] == 0 ? cj[2] : cj[0];
        if (dentro) { if (lo > pv) pv = lo; if (pv > hi) pv = hi; }
        else if (lo > pv || pv > hi) continue;
        if (eje_[k] == 0) cz = pv; else cx = pv;
        if (!(dmin > dist)) continue;
        dmin = dist; mejor = id_[k]; sm = (hi - pv) / (hi - lo); mx = cx; mz = cz;
    }
    *s_out = sm; *qx = mx; *qz = mz;
    return mejor;
}
/* 4F9000 (caja ib de B vista desde la caja ia de A) + 4F8E70: registros de A (las esquinas de B en A); devuelve la
   separacion mas larga (punto de cruce - esquina, mundo) y la direccion de su borde (4F6AC0) */
static int esquinas_en(const or2_forma_t *a, const or2_forma_t *b, int *borde, float *s, int *esq_b,
                       float *vx, float *vy, float *wx, float *wy)
{
    int i, alguna = 0; float lmax = -1.0f;
    *vx = *vy = *wx = *wy = 0.0f;
    for (i = 0; i < 4; ++i) {
        float cx, cz, w1x, w1y, w2x, w2y, p1x, p1z, p2x, p2z, ox, oz, d1x, d1y, dx, dz, qx, qz, sq, ex, ey, fx, fy;
        int id, dentro;
        borde[i] = -1; s[i] = 0.0f;
        esquina(b->caja, i, &cx, &cz);
        a_mundo(b->xa, b->ya, b->rumbo_a, cx, cz, &w1x, &w1y);   /* M1: los dos en el cuadro anterior */
        a_local(a->xa, a->ya, a->rumbo_a, w1x, w1y, &p1x, &p1z);
        a_mundo(b->x, b->y, b->rumbo, cx, cz, &w2x, &w2y);       /* M2: ahora */
        a_local(a->x, a->y, a->rumbo, w2x, w2y, &p2x, &p2z);
        ox = i >= 2 ? -1.0f : 1.0f; oz = (i == 0 || i == 3) ? 1.0f : -1.0f;
        a_mundo(0.0f, 0.0f, b->rumbo, ox, oz, &d1x, &d1y);       /* la diagonal con la rotacion de M2 */
        a_local(0.0f, 0.0f, a->rumbo, d1x, d1y, &dx, &dz);
        dentro = !(p2x > a->caja[0]) && !(a->caja[3] > p2x) && !(p2z > a->caja[2]) && !(a->caja[5] > p2z);
        id = borde_cruza(a->caja, p1x, p1z, p2x, p2z, dx, dz, dentro, &sq, &qx, &qz);
        if (id < 0) continue;
        borde[i] = id; s[i] = sq; esq_b[i] = 1; alguna = 1;
        /* 4F6AC0: el punto de cruce menos la esquina, en el mundo, y la "normal" del borde (es su direccion) */
        a_mundo(a->x, a->y, a->rumbo, qx, qz, &ex, &ey);
        ex -= w2x; ey -= w2y;
        {
            float l = ex * ex + ey * ey;
            if (lmax < 0.0f || l > lmax) {
                float nx = 0.0f, nz = 0.0f;
                if (id == 0) nz = -1.0f; else if (id == 1) nx = -1.0f; else if (id == 2) nz = 1.0f; else nx = 1.0f;
                a_mundo(0.0f, 0.0f, a->rumbo, nx, nz, &fx, &fy);
                lmax = l; *vx = ex; *vy = ey; *wx = fx; *wy = fy;
            }
        }
    }
    return alguna;
}

int or2_contacto(const or2_forma_t *a, const or2_forma_t *b, or2_contacto_t *k)
{
    float dx = b->x - a->x, dy = b->y - a->y, ra = a->radio, rb = b->radio;
    float v1x, v1y, w1x, w1y, v2x, v2y, w2x, w2y, vx, vy, wx, wy, pr;
    int r1, r2, i;
    memset(k, 0, sizeof *k);
    for (i = 0; i < 4; ++i) k->borde_a[i] = k->borde_b[i] = -1;
    /* 4F9450: cercania (40F140 en 3D; aqui en el plano) y altura de la animacion de cada uno contra la caja del otro */
    if (!(sqrtf(dx * dx + dy * dy) < ra + rb)) return 0;
    if (b->pose_y - rb * 0.5f > a->caja[1]) return 0;
    if (a->pose_y - ra * 0.5f > b->caja[1]) return 0;
    r1 = esquinas_en(a, b, k->borde_a, k->s_a, k->esq_b, &v1x, &v1y, &w1x, &w1y);   /* recA: B en A */
    r2 = esquinas_en(b, a, k->borde_b, k->s_b, k->esq_a, &v2x, &v2y, &w2x, &w2y);   /* recB: A en B */
    if (!r1 && !r2) return 0;
    k->toca = 1;
    /* 4F8490: la separacion mas larga (la de B en A, o la de A en B cambiada de signo), sin su parte a lo largo del
       borde (40F1F0) */
    if (!r1 || (r2 && !(v1x * v1x + v1y * v1y > v2x * v2x + v2y * v2y))) { vx = -v2x; vy = -v2y; wx = w2x; wy = w2y; }
    else { vx = v1x; vy = v1y; wx = w1x; wy = w1y; }
    pr = vx * wx + vy * wy;
    k->sep[0] = vx - wx * pr; k->sep[1] = vy - wy * pr;
    return 1;
}

/* 4F6640: el lado del golpe por las esquinas y bordes de la caja 0 de A (0xff = derecha) */
static int lado_golpe(const or2_contacto_t *k)
{
    int j;
    if (k->esq_a[0] || k->esq_a[1]) return 0;                 /* +514 / +518: esquinas de la derecha */
    if (k->esq_a[2] || k->esq_a[3]) return 1;
    for (j = 0; j < 4; ++j) {
        int id = k->borde_a[j];
        if (id < 0) continue;
        if (id == 0) return 0;
        if (id == 2) return 1;
        return !(0.5f >= k->s_a[j]) ? 1 : 0;                   /* borde de morro o cola: s <= 0,5 en el lado x0 */
    }
    return 1;
}
/* 4F65D0: familia del efecto por el modelo del otro (0x1f autobus 2, 0x27..0x2b camiones 1) */
static int familia_modelo(int m) { return m == 0x1f ? 2 : (m > 0x26 && m <= 0x2b ? 1 : 0); }
/* 4F67A0: familia por +31C (1 -> 2, 2 -> 1) */
static int familia_se(int se) { return se == 1 ? 2 : (se == 2 ? 1 : 0); }
static void suena(or2_otro_t *o, uint32_t id) { if (id && o->se_n < 3) o->se_id[o->se_n++] = (int)id; }
/* 475F70: estado del golpeado por el cierre (16/17 coche, 19/18 pesado: clases 0..2) */
static int tipo_golpe(float cierre, int clase)
{
    int peq = clase >= 0 && clase <= 2;
    if (!(cierre >= CIERRE_7)) return peq ? 0x12 : 0x11;
    return peq ? 0x13 : 0x10;
}
static int recorta_clase(int c) { return c < 0 ? 0 : (c > 6 ? 6 : c); }
/* 4F68F0: |proyeccion de vA - vB sobre la recta de A a B| */
static float cierre(v2 va, v2 vb, v2 dp)
{
    float l2 = dp.x * dp.x + dp.y * dp.y, pr;
    if (!(l2 > 0.0f)) return 0.0f;
    pr = ((va.x - vb.x) * dp.x + (va.y - vb.y) * dp.y) / sqrtf(l2);
    return fabsf(pr);
}
/* 4F7BF0: el intercambio de un coche S con el otro O (copias de antes, Sv / Ov); m = factor de masa (6AC8E0) */
static v2 empuja(v2 sv, v2 ov, float s_vel, float lj, float lo, float m, float e)
{
    if (!(lj > lo)) {
        v2 n = unidad(mk(ov.x - sv.x, ov.y - sv.y));
        float s = lo > s_vel ? s_vel : lo;
        return mk(sv.x + n.x * s, sv.y + n.y * s);
    }
    return mk(sv.x + (ov.x - sv.x) * m * e, sv.y + (ov.y - sv.y) * m * e);
}
static float restitucion(const or2_choques_t *q, float lj)
{
    return lj > q->umbral[1] ? q->rest[2] : (lj > q->umbral[0] ? q->rest[1] : q->rest[0]);
}
static float masa_clase(const or2_choques_t *q, int clase) { return q->masa[clase <= 2 ? 2 : 1]; }
static float reparto_de(const or2_choques_t *q, int ca, int cb, float sa, float sb)
{
    float r = (sa + sb) > 0.0f ? sb / (sa + sb) : 0.0f, tb = q->reparto[recorta_clase(ca) * OR2_CLASES + recorta_clase(cb)];
    if (!(tb < r)) r = tb;                                      /* el minimo de la tabla [A * 7 + B] */
    return r;
}
/* 4F76E0 de un coche de trafico X contra Y (con los registros de X, rx, y los de Y, ry): 1 si queda en roce (+4
   0x400000: sin intercambio). y_estado7 = Y es el jugador y acaba de entrar en el estado 7 (+4 0x80000). Sin +4 bit 0
   (el trafico) solo suena (4F67A0 tipo 0). */
static int reacciona_trafico(const or2_choques_t *q, or2_otro_t *x, const int *rx, const int *ry, int y_estado7)
{
    int j;
    if (!(x->t_choque > EPS_CHOQUE || x->t_recup > EPS_CHOQUE || y_estado7)) {   /* +B5C == 0x12: no lo hay */
        for (j = 0; j < 4; ++j) {
            if (rx[j] == 0 || rx[j] == 2 || ((j == 1 || j == 2) && rx[j] == 1)) return 1;
            if (ry[j] == 0 || ry[j] == 2 || ((j == 1 || j == 2) && ry[j] == 1)) return 1;
        }
    }
    if (q->se_ok && x->d22 <= 0) {                              /* 4F67A0 (+300 >= -100: siempre en la demo) */
        suena(x, q->se[familia_se(x->se)]);
        x->d22 = 60;
    }
    return 0;
}

float or2_reparto(const or2_choques_t *q, int clase_a, int clase_b, float va, float vb)
{
    if (!q || !q->ok) return 0.5f;
    return reparto_de(q, clase_a, clase_b, fabsf(va), fabsf(vb));
}

void or2_choque_coche(or2_coche_t *c, const or2_param_t *p, const or2_choques_t *q, or2_otro_t *o)
{
    const or2_contacto_t *k = o->k;
    v2 va, vb, dp, a0, b0;
    float sa, sb, a2c8, b2c8, flag = 0.0f, lj, e;
    int clase, j, roce_a = 0, roce_b = 0, e7 = 0;
    (void)p;
    o->golpe = -1; o->cambia_vel = 0; o->reaccion = 0; o->impulso = 0; o->roce = 0; o->roce_a = 0; o->se_n = 0;
    o->reparto = 1.0f;
    if (!q || !q->ok || !k || !k->toca) return;
    choques_act = q;
    clase = recorta_clase(o->clase);
    va = mk(c->vx * DT, c->vy * DT);                            /* m/cuadro, como +20 */
    vb = mk(o->vx * DT, o->vy * DT);
    sa = lon2(va); sb = lon2(vb);
    dp = mk(o->dx, o->dy);
    /* 4F9450: reparto con la tabla [jugador (clase 5) * 7 + otro]; +2C8 de los dos antes de las reacciones */
    o->reparto = reparto_de(q, 5, clase, sa, sb);
    a2c8 = c->t_choque; b2c8 = o->t_choque;
    /* ---- 4F6FE0 (A = el jugador, B = el otro) */
    do {
        int mismo, cola, cl, lado;
        if (c->paso > 0) break;                                 /* +1030: derrapando, nada (4F65D0 tipo 0 abajo) */
        mismo = c->da0 == o->id && c->da4 > 0;
        cola = o->b58 == 0x12 || o->t_choque > EPS_CHOQUE || o->t_recup > EPS_CHOQUE;
        if (!cola) {
            int sale = 0;
            roce_a = 1; c->da0 = o->id; c->da4 = 120;
            if (mismo) break;
            if (k->esq_a[0] || k->esq_a[3]) break;              /* +514 / +520: mis esquinas de cola dentro del otro */
            for (j = 0; j < 4 && !sale; ++j) {
                int id = k->borde_a[j];
                if (id == 0 || id == 2 || ((j == 1 || j == 2) && id == 1)) sale = 1;
            }
            for (j = 0; j < 4 && !sale; ++j) {
                int id = k->borde_b[j];
                if ((id == 0 || id == 2) && !(k->s_b[j] < 0.05f)) sale = 1;   /* 628128 */
                if ((j == 1 || j == 2) && id == 1) sale = 1;
            }
            if (!sale && o->k320 == 1)
                for (j = 0; j < 4 && !sale; ++j) {
                    if (k->borde_a[j] == 1) sale = 1;
                    if (k->borde_b[j] == 3 || k->borde_b[j] == 0 || k->borde_b[j] == 2) sale = 1;
                }
            if (sale) break;
        }
        /* L4F7237 */
        c->da0 = -1; c->da4 = 0; roce_a = 0;
        if (c->t_choque > EPS_CHOQUE || c->t_recup > EPS_CHOQUE) break;
        for (j = 0; j < 4; ++j) if (k->borde_a[j] == 3) break;  /* me han dado por detras: sin reaccion */
        if (j < 4) break;
        /* (sin handicap +DB4 ni partida en red: no hay solo efecto) */
        if (o->t_choque > EPS_CHOQUE || o->t_recup > EPS_CHOQUE || o->t_choque > 0.0f) {
            o->golpe = tipo_golpe(cierre(va, vb, dp), clase);   /* el otro ya va en choque: 476260 otra vez */
            break;
        }
        lado = lado_golpe(k);
        /* 4F6960: si mi velocidad se aleja del otro, el cierre cuenta 0 */
        {
            v2 u = unidad(va), d = unidad(dp);
            flag = (u.x * d.x + u.y * d.y) < 0.0f ? 0.0f : cierre(va, vb, dp);
        }
        if (flag > CIERRE_7) cl = 1;
        else if (o->k320 != 3 && !(sa < CIERRE_7)) cl = 1;      /* autobus y camiones: basta con ir a 80 km/h */
        else cl = o->b58 == 0x12 ? 3 : 2;
        if (cl == 1 || cl == 3) {
            if (cl == 3) lado = o->lado_26c < 0.0f;
            entra_choque(c, 7, lado);
            e7 = 1; o->reaccion = 1;
        }
        if ((cl == 1 || cl == 2) && !(flag < CIERRE_G)) o->golpe = tipo_golpe(flag, clase);
        if (q->se_ok && c->d22 <= 0) { suena(o, q->se[3 * 4 + familia_modelo(o->modelo)]); c->d22 = 60; }
    } while (0);
    /* ---- 4F76E0 (A = el otro, B = el jugador) */
    if (o->golpe >= 0) {                                        /* 476040 ya le ha puesto el estado (+2C8 > 0) */
        if (o->t_choque <= EPS_CHOQUE) o->t_choque = q->dur[o->golpe];
    }
    roce_b = reacciona_trafico(q, o, k->borde_b, k->borde_a, e7);
    /* ---- intercambio (4F9450 tras las reacciones) */
    if (!(a2c8 > EPS_CAMBIO)) {
        if (c->paso > 0 || b2c8 > 0.0f) {
            if (q->se_ok && c->d22 <= 0) { suena(o, q->se[familia_modelo(o->modelo)]); c->d22 = 60; }
        } else {
            a0 = va; b0 = vb;
            lj = cierre(a0, b0, dp);
            e = restitucion(q, lj);
            if (!roce_a) {
                va = empuja(a0, b0, sa, lj, q->umbral[0], q->masa[0], e);
                o->impulso = 1;
                /* 4F6820: en el estado 7 no pasa de 300 km/h del modelo */
                if (c->t_choque > 0.0f && c->choque == 7) {
                    float s = lon2(va);
                    if (s > TOPE_7) { va.x *= TOPE_7 / s; va.y *= TOPE_7 / s; }
                }
                c->vx = va.x * HZ; c->vy = va.y * HZ;
                c->vel_mf = lon2(va);
                c->kmh = tr(c->vel_mf * 216.720001f);
            }
            if (!roce_b) {
                vb = empuja(b0, a0, sb, lj, q->umbral[0], masa_clase(q, clase), e);
                o->vx = vb.x * HZ; o->vy = vb.y * HZ;
                o->cambia_vel = 1;
            }
        }
    }
    o->roce = roce_b; o->roce_a = roce_a;
}

void or2_choque_trafico(const or2_choques_t *q, or2_otro_t *a, or2_otro_t *b)
{
    const or2_contacto_t *k = a->k;
    v2 va, vb, dp, a0, b0;
    float sa, sb, lj, e, a2c8;
    int ra, rb;
    a->golpe = b->golpe = -1; a->cambia_vel = b->cambia_vel = 0; a->se_n = b->se_n = 0;
    a->reaccion = b->reaccion = a->impulso = b->impulso = 0;
    if (!q || !q->ok || !k || !k->toca) return;
    va = mk(a->vx * DT, a->vy * DT); vb = mk(b->vx * DT, b->vy * DT);
    sa = lon2(va); sb = lon2(vb);
    dp = mk(b->dx - a->dx, b->dy - a->dy);
    a->reparto = reparto_de(q, a->clase, b->clase, sa, sb);
    b->reparto = 1.0f - a->reparto;
    a2c8 = a->t_choque;
    ra = reacciona_trafico(q, a, k->borde_a, k->borde_b, 0);
    rb = reacciona_trafico(q, b, k->borde_b, k->borde_a, 0);
    a->roce = ra; b->roce = rb;
    if (a2c8 > EPS_CAMBIO) return;
    a0 = va; b0 = vb;
    lj = cierre(a0, b0, dp);
    e = restitucion(q, lj);
    if (!ra) {
        va = empuja(a0, b0, sa, lj, q->umbral[0], masa_clase(q, recorta_clase(a->clase)), e);
        a->vx = va.x * HZ; a->vy = va.y * HZ; a->cambia_vel = 1;
    }
    if (!rb) {
        vb = empuja(b0, a0, sb, lj, q->umbral[0], masa_clase(q, recorta_clase(b->clase)), e);
        b->vx = vb.x * HZ; b->vy = vb.y * HZ; b->cambia_vel = 1;
    }
}

/* CwCrushStatus (5038D0): choque fuerte contra el muro si el angulo de impacto llega al de aplastamiento.
   Sin handicap (+DB4 = 0) ni asistencia (+DF8 = 0), y sin los tramos de atrapamiento por etapa (5C2570,
   check_crush_entrapment_length) ni el desvio del estado 1 al 5 (resolve_entry): son posiciones de la malla de
   la carretera del original, que la demo no tiene. Velocidades en km/h del modelo (+1F4); n = punta al corte:
   n - 70 o menos, nada (solo el temporizador de colision); por encima de n - 20 el estado 1 (vuelta de campana);
   dentro de un tramo de atrapamiento de la etapa (c->atrapa & 1) los limites son 180 y 350; en uno de desvio
   (c->atrapa & 4) el estado 1 pasa al 5;
   si no, 5 (a 10 grados o mas) o 2. Entra con StartCrash (4A2270) y la grua (PlWrecker fase > 1: cuadros =
   duracion * 60.2 + 1). lado: +670 & 0xA0000000 (puntos 0 y 2 de la forma del coche contra el muro), aqui el
   costado izquierdo del coche (supuesto: los puntos pares son los del lado izquierdo). */
static int aplasta(or2_coche_t *c, const or2_param_t *p, float imp, int lado)
{
    const or2_choques_t *q = choques_act;
    int n, bajo, alto, estado, fase;
    if (!q || !q->ok) return 0;
    if (c->t_choque > 0.0f || c->t_recup > 0.0f || c->grua > 0) return 0;
    n = tr(p->v_tope * 216.720001f);
    bajo = n - 70; alto = n - 20;
    if (c->atrapa & 1) { bajo = 180; alto = 350; }      /* check_crush_entrapment_length (PS2 0x199c04..0x199c48) */
    if (c->kmh <= bajo) return 0;
    estado = c->kmh > alto ? 1 : (imp >= 0.174532920f ? 5 : 2);
    if (estado == 1 && (c->atrapa & 4)) estado = 5;     /* resolve_entry (PS2 0x182d6c..0x182dc4) */
    entra_choque(c, estado, lado);
    fase = tr(q->dur[estado] * 60.2000008f);
    if (fase > 1) { c->grua = fase + 1; c->grua_ini = fase + 1; }
    return 1;
}

/* CbwColiWall (5041B0) para el jugador: roce, rebote y choque fuerte (CwCrushStatus) */
/* crush_threshold (PC pc_wall_controller.cpp, PS2 0x1994c4..0x19954c): 45 grados en la etapa de nivel 0 (44C940 =
   PS2 0x177270 da 0); en las demas 25 grados si la curva que viene (+D4E - +D4C, x 2 pi / 65536) no pasa de 20
   grados y 18 si pasa */
static float ang_aplasta(const or2_coche_t *c, int16_t dl)
{
    if (c->nivel_etapa == 0) return 0.785398185f;                       /* 3F490FDB */
    return (float)iabs(dl) * 9.58738019e-05f <= 0.349065840f ? 0.436332315f : 0.314159274f;   /* 38C90FDB, 3EB2B8C2, 3EDF66F3, 3EA0D97C */
}
int or2_choque_muro(or2_coche_t *c, const or2_param_t *p, float nx, float ny, float pen)
{
    v2 v = mk(c->vx, c->vy), n = mk(nx, ny), f;
    float sp = lon2(v), vn, imp, lim;
    int16_t dl;
    int lado, fuerte = 0, muro;
    float ANG_APLASTA;
    if (sp < 0.0001f) return 0;
    vn = v.x * n.x + v.y * n.y;
    /* calc_wall_impact_angle: 90 grados - acos(|v . n|) */
    imp = 1.57079637f - acosf(clampf(fabsf(vn) / sp, 0.0f, 1.0f));
    dl = s16((int32_t)c->d4e - c->d4c);
    ANG_APLASTA = ang_aplasta(c, dl);
    /* rebound_threshold: 5 grados (10 si la curva va hacia el otro lado), 25 rozando o tras un golpe */
    if (c->kmh >= 50 && c->roce == 0 && c->c282 == 0) {
        lim = 0.0872664601f;
        if (iabs(dl) > 0x400 && (float)dl * c->ofs < 0.0f) lim = 0.174532920f;
    } else lim = 0.436332315f;
    /* lado del coche que da contra el muro: el izquierdo si la normal (hacia la calzada) apunta a su derecha */
    {
        v2 izq = mk(-cosf(c->psi), -sinf(c->psi));
        lado = (n.x * izq.x + n.y * izq.y) < 0.0f ? 1 : 0;
    }
    /* sin clase de muro bajo la forma (+2A8 & 0x10F0020 = 0: los limites 0x4000 y 0x40) solo hay roce (PS2 0x199430
       .., PC pc_wall_controller.cpp 150) */
    muro = c->muro_clase == 0 || (c->muro_clase & 0x010f0020u) != 0;
    /* impacto entre el aplastamiento y el rebote: solo si se acepta el choque fuerte hay rebote */
    if (muro && vn < 0.0f && !(imp >= lim) && imp >= ANG_APLASTA) fuerte = aplasta(c, p, imp, lado);
    if (muro && vn < 0.0f && (imp >= lim || fuerte)) {
        /* cw_rebound_status: rumbo del muro (el sentido de la carretera) y rumbo de rebote */
        int16_t muro = s16(tr(atan2f(ny, nx) * R2U)), dif;
        if (iabs(s16((int32_t)muro - c->d4c)) >= 0x4000) muro = s16((int32_t)muro - 0x8000);
        dif = s16((int32_t)c->rumbo_v16 - muro);
        if (c->c284 == 0 && c->c282 < 24) c->c282 = 24;
        if (c->kmh >= 100 && c->c284 == 0) {
            /* calc_rebound_heading: un cuarto de reflexion mas la mitad de la curva que viene */
            int32_t cuarto = -(int32_t)dif / 4, mitad = (int32_t)dl / 2;
            if ((mitad + cuarto) * cuarto < 0) mitad = -(cuarto / 2);
            c->rebote = 1;
            c->h286 = s16((int32_t)c->rumbo_v16 + mitad + cuarto - dif);
        } else {
            c->rebote = 0;
            c->h286 = s16((int32_t)c->rumbo_v16 - ((int32_t)dif * 5) / 4);
        }
        c->v28c = c->vel_mf;
        c->h288 = s16((int32_t)c->h286 - (int32_t)s16(tr(c->psi * R2U)));
        c->c283 = 30;
        c->c284 = 36;
        c->golpe_sonido = 1;
        /* despues del rebote, el choque fuerte si el impacto llega al angulo de aplastamiento */
        if (!fuerte && imp >= ANG_APLASTA) fuerte = aplasta(c, p, imp, lado);
        return fuerte ? 2 : 1;
    }
    /* calc_friction_status: frenada de kmh >> 7 cuadros, el cuerpo gira hacia el muro 2608 unidades por metro
       de penetracion y la velocidad sigue el muro sin perder modulo */
    {
        /* PS2 0x19a200..0x19a308: cuadros de frenada por la velocidad; mas en los tramos de atrapamiento del choque
           (kmh >> 6, o >> 3 si ademas lo es del roce) y del roce (>> 6); sin clase de muro, un escalon menos (sin
           tramos, kmh >> 8). Sin handicap (+DB4 = 0) ni asistencia (+DF8) */
        int ba, bb, b;
        float giro = pen * 2608.0f * U2R, a;
        if (c->atrapa & 1) { ba = (c->kmh >> ((c->atrapa & 2) ? 3 : 6)) & 255; bb = (c->kmh >> 7) & 255; }
        else if (c->atrapa & 2) { ba = (c->kmh >> 6) & 255; bb = (c->kmh >> 7) & 255; }
        else { ba = (c->kmh >> 7) & 255; bb = (c->kmh >> 8) & 255; }
        b = muro ? ba : bb;
        v2 t;
        if (c->c282 < b) c->c282 = b;
        c->roce = 2;
        t = mk(-n.y, n.x);                       /* tangente del muro en el sentido de la marcha */
        if (t.x * v.x + t.y * v.y < 0.0f) t = mk(-t.x, -t.y);
        f = dir_ang(c->psi);
        a = atan2f(cruz(f, t), f.x * t.x + f.y * t.y);   /* >0: la tangente queda a la izquierda */
        if (fabsf(a) < giro) giro = fabsf(a);
        c->psi = angw(c->psi + (a > 0.0f ? giro : -giro));
        if (vn < 0.0f) {
            v2 w = mk(v.x - n.x * vn, v.y - n.y * vn);
            float l = lon2(w);
            if (l > 0.0001f) { c->vx = w.x * sp / l; c->vy = w.y * sp / l; }
        }
    }
    (void)p;
    return 0;
}

/* CheckSlipStream (4A4D20) */
void or2_rebufo(or2_coche_t *c, const or2_otro_t *otros, int n)
{
    float best = 0.0f, base, obj, cur;
    v2 va = unidad(mk(c->vx, c->vy));
    int i, cnt;
    /* +162 = rumbo de la velocidad (+160) menos el del cuerpo (+2E) */
    int32_t ang = iabs(s16((int32_t)c->rumbo_v16 - s16(tr(c->psi * R2U))));
    c->rebufo_marca = 0;
    for (i = 0; i < n; ++i) {
        const or2_otro_t *o = otros + i;
        float d = sqrtf(o->dx * o->dx + o->dy * o->dy), fd, sv, fv, al, fa, sc, vo;
        v2 vb, to;
        if (d < 10.0f) d = 10.0f; else if (d > 50.0f) d = 50.0f;
        fd = 1.0f - (d - 10.0f) * 0.0250000004f;
        vo = sqrtf(o->vx * o->vx + o->vy * o->vy);
        sv = vo * DT * 216.720001f;
        fv = 300.0f > sv ? sv * 0.00333333341f : 1.0f;
        vb = unidad(mk(o->vx, o->vy));
        al = cl01(vb.x * va.x + vb.y * va.y);
        to = unidad(mk(o->dx, o->dy));
        /* el otro tiene que ir delante, a menos de 20 grados (todo a menos de 5) de su propia velocidad */
        fa = cl01(((vb.x * to.x + vb.y * to.y) - 0.939692616f) / (0.996194720f - 0.939692616f));
        sc = fa * al * fv * fd;
        if (sc > best) {
            best = sc;
            if (sc > 0.180000007f && ang < 1500 && i < 32) c->rebufo_marca |= 1u << i;   /* +B72 = 180 */
        }
    }
    c->rebufo = best;
    /* objetivo = best * acelerador * (1 - freno) * cos(d46), rampa de 180 cuadros */
    base = (1.0f - (float)c->freno_op * 0.00392156886f) * best * (float)c->pedal_ac * 0.00392156886f;
    obj = cosf((float)c->d46 * U2R) * base;
    cnt = c->rebufo_n;
    cur = c->rebufo_k;
    if ((float)(cnt + 1) * obj * 0.00555555569f > cur) {
        if (cnt < 180) ++cnt;
        c->rebufo_n = cnt;
        c->rebufo_k = (float)cnt * obj * 0.00555555569f;
    } else if (cnt > 0) {
        c->rebufo_k = cur - cur / (float)cnt;
        c->rebufo_n = cnt - 1;
    } else { c->rebufo_n = 0; c->rebufo_k = 0.0f; }
}
