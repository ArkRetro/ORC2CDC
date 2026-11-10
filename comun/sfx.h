#ifndef SFX_H
#define SFX_H
#include <stdint.h>
/* Efectos del coche del jugador (motor, derrape, golpe) con el sistema "ICS" del original: cada efecto es un grupo de
   capas en bucle (una muestra cada una) que se mezclan y cambian de tono segun un parametro x (0..127): las rpm en el
   motor, el deslizamiento en el derrape y el tiempo desde el impacto en el golpe. Los datos (muestras ADPCM y curvas)
   vienen de SFX.DAT (tools/conv_sonido.py). Usa canales del AICA propios (snd_sfx_chn_alloc), sin tocar los de
   snd_stream. */

int  sfx_inicia(void);                     /* carga SFX.DAT y lo sube a la RAM de sonido; 0 = bien */
void sfx_motor(float rpm, float acelerador, int marcha);   /* una vez por cuadro (tambien hace avanzar el golpe) */
void sfx_derrape(float intensidad);        /* 0..1 (0 = calla), una vez por cuadro */
void sfx_golpe(float fuerza);              /* 0..1: dispara el golpe (se apaga solo en ~1/3 s) */
void sfx_volumen(float v);                 /* factor de los efectos del port (1 = el del original) */
void sfx_master(int m);                    /* volumen general del original (0x11ae38): opcion de efectos / 10 * 144 */
void sfx_para(void);                       /* calla todo (pausa, fin de carrera); se reanuda con la siguiente llamada */
/* efectos de los menus (bancos SE del original, muestras "se%03x" de SFX.DAT), disparo simple sin bucle por dos canales
   propios (sfx_para no los calla): */
enum { SFX_CANCELA = 0x00, SFX_CURSOR = 0x01, SFX_BLOQUEADO = 0x03, SFX_ACEPTA = 0x40 };
void sfx_menu(int id);
/* voces (VOCES.DAT, despues de sfx_inicia): sfx_voz(id) toca la voz del id del original (los 9 bits bajos; tambien los
   efectos de carrera 0x4d y 0x18d, por otro canal); sfx_voz_calla las corta. Las tablas de cuando habla (voces.c) y el
   idioma del banco vienen en el mismo fichero. */
int  sfx_voces_inicia(void);               /* 0 = bien */
void sfx_voz(int id);
void sfx_voz_calla(void);
const void *sfx_voces_tablas(uint32_t *tam);
const char *sfx_voces_idioma(void);
/* otro juego de voces (VOCESH.DAT del INFARTO / VOCES.DAT); buf, cap: RAM prestada para leerlo. 0 = bien */
int  sfx_voces_juego(const char *fich, void *buf, uint32_t cap);
void sfx_voz_grupo(int g);                 /* banco HAM de la etapa (-1: ninguno) */
void sfx_voz_se(int id);                   /* un efecto del banco de voces por el canal de efectos */
/* una muestra suelta (ADPCM de Yamaha) por el canal de efectos de las voces (la escena de la meta, demo/meta.c) */
uint32_t sfx_muestra_sube(const void *adpcm, uint32_t bytes);   /* direccion en el AICA o 0 */
void sfx_muestra_baja(uint32_t a);
void sfx_muestra_toca(uint32_t a, uint32_t muestras, uint32_t hz, int vol_irx);

#endif
