#ifndef VIDEO_H
#define VIDEO_H
/* Peliculas del original (MV.PS2) convertidas a texturas VQ de la PowerVR con audio ADPCM (tools/conv_video.py,
   ficheros .VQV). Ver video.c. Necesita la PowerVR y snd_stream ya iniciados (pvr_init, musica_inicia). */

/* Reproduce un .VQV a pantalla completa con su audio. Devuelve 1 si se ha saltado con START, 0 si ha acabado, 2 si se
   ha atascado (sin cuadros nuevos en 1 s: deja 12 s el diagnostico escrito sobre la imagen) y -1 si no se ha podido
   (sin fichero, sin memoria): entonces no dibuja nada. Para la musica (y su lectura del GD) mientras tanto. */
int video_reproduce(const char *nombre);

/* Lo que hace el modo ADVERTISE del original (0x25e1d8): la pelicula de titulo (MVTS.VQV) y luego la de atraccion
   (MVATTR.VQV); START salta las dos. Devuelve 1 si se ha pulsado START (o una se ha atascado), 0 si han acabado, -1 si
   no hay peliculas. */
int video_atraccion(void);

/* Gancho de demo/main.c antes del primer titulo: MVATTR.VQV y luego MVTS.VQV (START o un atasco: al titulo). */
void video_titulo(void);

#endif
