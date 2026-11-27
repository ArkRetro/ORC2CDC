# Prueba 0: KOS_BASE y el entorno de KallistiOS (source environ.sh)
TARGET = demo.elf
OBJS = main.o camara.o coli.o perfil.o meta.o fin.o final.o sp.o pcarga.o nombre.o ../comun/gd.o ../comun/musica.o ../comun/or2_fisica.o ../comun/hud.o ../comun/sfx.o ../comun/voces.o ../comun/video.o ../comun/menu.o ../comun/vmu.o ../comun/vmuesc.o ../comun/robot.o ../comun/infarto.o ../comun/haobj.o ../comun/modo_video.o ../comun/registro.o ../comun/sprani.o ../comun/fsp.o ../comun/rksp.o ../comun/rival.o ../comun/mision.o ../comun/campeonato.o ../comun/vozmis.o
all: rm-elf $(TARGET)
# el HUD y los menus dibujan con pvr_prim: pasan por ta_prim (demo/main.c) para contar sus vertices en el presupuesto
# del TA (docs/DREAMCAST.md)
../comun/hud.o ../comun/menu.o: CFLAGS += -Dpvr_prim=ta_prim
# Rev 2.9.9.c: los menus compilados por tamano (-Os gana a -O2, que va antes): 19 KB menos de codigo, que son RAM del
# monton; no son codigo de la carrera. Quitar esta linea vuelve a -O2
# (C2C e INFARTO) lo mismo para lo que no corre en cada cuadro de la carrera o pesa poco en ella: el modo, sus objetos,
# las misiones y los campeonatos de C2C, el frontal de OUTRUN2SP, su ranking, la VMU, el fin, la meta, la cola de voces,
# la lectura del GD y la musica. NO el dibujo, la fisica, el HUD, los efectos de sonido ni la camara (-O2)
../comun/menu.o ../comun/mision.o ../comun/rival.o ../comun/campeonato.o ../comun/vozmis.o ../comun/infarto.o ../comun/haobj.o ../comun/fsp.o ../comun/vmuesc.o ../comun/sprani.o ../comun/rksp.o ../comun/modo_video.o ../comun/registro.o ../comun/voces.o ../comun/vmu.o ../comun/gd.o ../comun/musica.o pcarga.o meta.o sp.o fin.o final.o nombre.o: CFLAGS += -Os
# pvr_state de KallistiOS (la presentacion a FPS_OBJETIVO esconde el render acabado hasta su campo: demo/main.c)
main.o: CFLAGS += -I$(KOS_BASE)/kernel/arch/dreamcast/hardware/pvr
include $(KOS_BASE)/Makefile.rules
clean: rm-elf
	-rm -f $(OBJS)
rm-elf:
	-rm -f $(TARGET)
$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -Wl,--wrap=pvr_scene_finish -Wl,--wrap=pvr_poly_compile
