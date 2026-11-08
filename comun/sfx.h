#ifndef SFX_H
#define SFX_H
/* Efectos del coche del jugador (motor, derrape, golpe) con el sistema "ICS" del original: cada efecto es un grupo de
   capas en bucle (una muestra cada una) que se mezclan y cambian de tono segun un parametro x (0..127): las rpm en el
   motor, el deslizamiento en el derrape y el tiempo desde el impacto en el golpe. Los datos (muestras ADPCM y curvas)
   vienen de SFX.DAT (tools/conv_sonido.py). Usa canales del AICA propios (snd_sfx_chn_alloc), sin tocar los de
   snd_stream. */

int  sfx_inicia(void);                     /* carga SFX.DAT y lo sube a la RAM de sonido; 0 = bien */
void sfx_motor(float rpm, float acelerador, int marcha);   /* una vez por cuadro (tambien hace avanzar el golpe) */
void sfx_derrape(float intensidad);        /* 0..1 (0 = calla), una vez por cuadro */
void sfx_golpe(float fuerza);              /* 0..1: dispara el golpe (se apaga solo en ~1/3 s) */
void sfx_volumen(float v);                 /* volumen general de los efectos (1 = por defecto) */
void sfx_para(void);                       /* calla todo (pausa, fin de carrera); se reanuda con la siguiente llamada */
/* efectos de los menus (bancos SE del original, muestras "se%03x" de SFX.DAT), disparo simple sin bucle por dos canales
   propios (sfx_para no los calla): */
enum { SFX_CANCELA = 0x00, SFX_CURSOR = 0x01, SFX_BLOQUEADO = 0x03, SFX_ACEPTA = 0x40 };
void sfx_menu(int id);

#endif
