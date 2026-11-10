#!/usr/bin/env bash
# instala_entorno.sh: el entorno de trabajo del port en un contenedor nuevo (Debian / Ubuntu, como root).
#  - paquetes del sistema (compilador del host, Xvfb y xdotool para las pruebas en Flycast, genisoimage para mkgdi.py)
#    y de Python (numpy, scipy, Pillow, capstone, gdown) que usan los conversores de tools/
#  - KallistiOS en /opt/toolchains/dc/kos (commit d81aa3f) con su kos-chain (perfil stable: GCC 15.2, makejobs=4,
#    sin Objective C) en /opt/toolchains/dc/sh-elf, environ.sh de doc/environ.sh.sample, la libreria y makeip
#  - Flycast en /opt/flycast/build/flycast (cmake Release, USE_VULKAN=OFF: en Xvfb dibuja con el OpenGL de Mesa)
# Cada parte se salta si ya esta. Despues: tools/regenera_datos.sh <iso> <datos/> rehace los datos desde tu ISO.
# Tiempo: la cadena de GCC es lo que mas tarda (~1 h en 4 nucleos).
set -euo pipefail
DC=/opt/toolchains/dc; KOS=$DC/kos; FLYCAST=/opt/flycast
KOS_COMMIT=d81aa3f

echo "== paquetes"
if command -v apt-get > /dev/null; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -q
    apt-get install -y -q build-essential git cmake ninja-build pkg-config texinfo libjpeg-dev libpng-dev libelf-dev \
        libgmp-dev libmpfr-dev libmpc-dev flex bison wget curl bzip2 xz-utils python3 python3-pip bc \
        xvfb xdotool imagemagick genisoimage ffmpeg \
        libsdl2-dev libgl1-mesa-dev libudev-dev libcurl4-openssl-dev libzip-dev libpulse-dev libasound2-dev
fi
pip3 install -q numpy scipy pillow capstone gdown 2> /dev/null || pip3 install -q --break-system-packages numpy scipy pillow capstone gdown

echo "== KallistiOS ($KOS_COMMIT) y kos-chain"
mkdir -p $DC
[ -d $KOS/.git ] || git clone -q https://github.com/KallistiOS/KallistiOS.git $KOS
git -C $KOS checkout -q $KOS_COMMIT
if [ ! -x $DC/sh-elf/bin/sh-elf-gcc ]; then
    cd $KOS/utils/kos-chain
    cfg=Makefile.dreamcast.cfg                            # (el de la Dreamcast; kos-chain lee Makefile.cfg si existe)
    sed -i -e 's/^toolchain_profile=.*/toolchain_profile=stable/' -e 's/^makejobs=.*/makejobs=4/' \
           -e 's/^verbose=.*/verbose=0/' -e 's/^enable_objc=.*/enable_objc=0/' -e 's/^enable_objcpp=.*/enable_objcpp=0/' $cfg
    [ -f Makefile.cfg ] && sed -i -e 's/^makejobs=.*/makejobs=4/' -e 's/^enable_objc=.*/enable_objc=0/' \
           -e 's/^enable_objcpp=.*/enable_objcpp=0/' -e 's/^toolchain_profile=.*/toolchain_profile=stable/' Makefile.cfg
    make                                                  # GCC 15.2, binutils y newlib en /opt/toolchains/dc/sh-elf
fi
[ -f $KOS/environ.sh ] || cp $KOS/doc/environ.sh.sample $KOS/environ.sh
set +u; . $KOS/environ.sh; set -u
[ -f $KOS/lib/dreamcast/libkallisti.a ] || make -C $KOS
[ -x $KOS/utils/makeip/makeip ] || make -C $KOS/utils/makeip
$DC/sh-elf/bin/sh-elf-gcc --version | head -1

echo "== Flycast"
if [ ! -x $FLYCAST/build/flycast ]; then
    [ -d $FLYCAST/.git ] || git clone -q --recursive https://github.com/flyinghead/flycast.git $FLYCAST
    git -C $FLYCAST submodule update -q --init --recursive
    cmake -S $FLYCAST -B $FLYCAST/build -DCMAKE_BUILD_TYPE=Release -DUSE_VULKAN=OFF
    cmake --build $FLYCAST/build -j"$(nproc)"
fi
ls -l $FLYCAST/build/flycast

echo "== listo: source $KOS/environ.sh && make -C demo"
