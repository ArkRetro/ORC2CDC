#ifndef META_H
#define META_H
/* La escena de la meta del original (modo 19 GOAL: 49C9C0 / 49CC50 del PC, PS2 0x1b8e98): el guion 486730 y el
   movimiento de camara 486EF0 del suceso 7 (4874A0, PS2 0x1f43d0 / 0x1f4c08), con los datos de META.DAT
   (tools/conv_meta.py). Ver meta.c. */
#include <stdint.h>

typedef struct {
    int      activa;            /* 82E7D8: el guion y el movimiento corren (op 30 lo apaga)                   */
    int      cam;               /* 82E7D0 == 1: el movimiento pone la camara (+E0..+F4, +130, +A4)              */
    int      mueve;             /* 486EF0 ha escrito este paso (82E7EC)                                         */
    int      pasplcar;          /* op 6: 1 = PasPlCar_Ctrl 475720 (sin fisica), 0 = GamePlCar 4A8330           */
    int      coloca;            /* op 7 de este paso: 1 marco de la union (7D3190), 0 el de la etapa (7D2DA0), -1 */
    int      freno;             /* op 11: car+38 (0xFF / 0) y car+1F4 (0x1E0 / 0)                              */
    int      kmh;
    uint32_t humo;              /* op 8: 82E7C8 (bit 0: ruedas 0 y 1, bit 1: 2 y 3; humo y marcas, 41C570)       */
    float    cerca, lejos;      /* op 19 / 20: camara +BC / +C0 (0 = sin cambio este paso)                      */
    float    t[3], r[3];        /* car+2D8 / +2DC / +2E0 y +2E4 / +2E8 / +2EC                                   */
    int16_t  s32;               /* car+32 (82EA7A): la direccion de las ruedas delanteras (rueda 0 +32)         */
    int16_t  dir1;              /* rueda 1 +32 (82EB6E)                                                         */
    int16_t  giro[4];           /* rueda k +30 (82EA78, 82EB6C, 82EC60, 82ED54)                                 */
    float    ojo[3], mira[3];   /* camara +E0 / +EC en el marco del coche (4853D0: por car+280)                  */
    float    balanceo, fov;     /* +130 y +A4 = +AC                                                             */
    int      cuadro;            /* 82E7C4 */
    uint32_t sprite;            /* op 0 de este paso: la animacion de sprites que empieza (428320), 0 = ninguna   */
} meta_t;

int  meta_carga(void);                              /* lee META.DAT; 0 = bien */
int  meta_lista(void);
void meta_suelta(void);                             /* libera META.DAT (fuera de la carrera) */
int  meta_escena(int variante, int preset, int meta);   /* 49C9C0: la escena (meta = 450380(8) - 0xA) */
int  meta_pista(int variante, int meta);            /* 49C9C0: pista de MUSIC.PS2 de la meta (-1: ninguna) */
void meta_empieza(meta_t *m, int escena);           /* 4871A0 */
void meta_paso(meta_t *m);                          /* 4874A0: un paso de logica */
void meta_termina(void);                            /* libera el efecto del AICA */
/* la escena de la salida (modo 13 START, SALIDA.DAT de tools/conv_salida.py; ver meta.c): el mismo guion y
   movimiento, con meta_paso */
int  salida_carga(void);                            /* al arrancar: efectos al AICA; 0 = bien */
int  salida_escena(int jugadores, int modelo, int clave);   /* 49DD40: la escena (10..14) o -1 */
int  salida_empieza(meta_t *m, int escena, int variante);   /* 4871A0; lee el guion; 0 = bien */
void salida_modo_juego(int si);                     /* el modo pasa a GAME (0x10): canal 1 del movimiento */
int  salida_activa(void);
void salida_suelta(void);                           /* fin de la escena: suelta el guion */
void salida_sfx_paso(void);                         /* cada cuadro (segundos trozos de los efectos largos) */
void salida_sfx_calla(void);
#endif
