# Prueba 0: KOS_BASE y el entorno de KallistiOS (source environ.sh)
TARGET = demo.elf
OBJS = main.o camara.o coli.o perfil.o meta.o fin.o final.o sp.o pcarga.o nombre.o ../comun/gd.o ../comun/musica.o ../comun/or2_fisica.o ../comun/hud.o ../comun/sfx.o ../comun/voces.o ../comun/video.o ../comun/menu.o ../comun/vmu.o ../comun/vmuesc.o ../comun/robot.o ../comun/infarto.o ../comun/haobj.o ../comun/modo_video.o ../comun/registro.o ../comun/sprani.o ../comun/fsp.o ../comun/rksp.o ../comun/rival.o
all: rm-elf $(TARGET)
# el HUD y los menus dibujan con pvr_prim: pasan por ta_prim (demo/main.c) para contar sus vertices en el presupuesto
# del TA (docs/DREAMCAST.md)
../comun/hud.o ../comun/menu.o: CFLAGS += -Dpvr_prim=ta_prim
# Rev 2.9.9.c: los menus compilados por tamano (-Os gana a -O2, que va antes): 19 KB menos de codigo, que son RAM del
# monton; no son codigo de la carrera. Quitar esta linea vuelve a -O2
../comun/menu.o: CFLAGS += -Os
# (INFARTO, Rev 1.1) lo mismo para el modo y sus objetos y para el frontal de OUTRUN2SP, su ranking y la pantalla de la
# VMU (fuera de la carrera o sin peso en ella): compensan el codigo nuevo del modo y la RAM del arranque no baja
# (INFARTO 2: estrellas, carriles, marcas y postes) y el fin de la carrera, la escena de la meta, los sprites de los menus,
# los rivales de C2C, la cola de voces, la lectura del GD y la musica: poco calculo por cuadro (INFARTO 3: el HUD, los
# efectos y la camara vuelven a -O2, corren en cada cuadro de la carrera; la RAM sale de las tablas del modo al monton y
# de FRIO en main.c)
../comun/infarto.o ../comun/haobj.o ../comun/fsp.o ../comun/rksp.o ../comun/vmuesc.o ../comun/vmu.o ../comun/registro.o ../comun/modo_video.o final.o nombre.o pcarga.o sp.o fin.o meta.o ../comun/sprani.o ../comun/rival.o ../comun/voces.o ../comun/gd.o ../comun/musica.o: CFLAGS += -Os
# pvr_state de KallistiOS (la presentacion a FPS_OBJETIVO esconde el render acabado hasta su campo: demo/main.c)
main.o: CFLAGS += -I$(KOS_BASE)/kernel/arch/dreamcast/hardware/pvr
include $(KOS_BASE)/Makefile.rules
clean: rm-elf
	-rm -f $(OBJS)
rm-elf:
	-rm -f $(TARGET)
$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -Wl,--wrap=pvr_scene_finish -Wl,--wrap=pvr_poly_compile
