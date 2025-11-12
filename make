export PATH=/bin/:/usr/bin/:/usr/:/opt/netsurf/m68*/cross/m68*/:/opt/netsurf/m68*/env/lib/
export PKG_CONFIG_PATH=/mnt/d/opt/netsurfy/netsurf-all-3.11/inst-amigaos3/lib/pkgconfig:/opt/netsurf/m68k-unknown-amigaos/env/lib/pkgconfig
clear;make -j16 HOST=clib2 TARGET=mui PREFIX=/opt/netsurf/ #package