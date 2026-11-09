#ifndef MODO_VIDEO_H
#define MODO_VIDEO_H
/* Pantalla del arranque del original (PAL) para elegir 50 / 60 Hz, con MODOV.DAT (tools/conv_modo_video.py).
   Se llama con el modo PAL ya puesto; cambia ella misma a 60 Hz (NTSC entrelazado, DM_640x480) durante la prueba y
   vuelve a PAL si no se confirma. Devuelve 1 si se queda en 60 Hz, 0 en 50 Hz (o sin MODOV.DAT). */
int modo_video_pantalla(void);
#endif
