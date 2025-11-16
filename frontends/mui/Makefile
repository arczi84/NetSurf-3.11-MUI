#
# Makefile for NetSurf AmigaOS3 frontend
#
# This file is part of NetSurf 
#
# ----------------------------------------------------------------------------
# AmigaOS target setup
# ----------------------------------------------------------------------------
VERSION := 3.11
# ----------------------------------------------------------------------------
CFLAGS += -w -std=c99 -s -O3 -DPATH_MAX=1024 -D__m68k__ -m68020-60 -fomit-frame-pointer -I. -Dnsmui
CFLAGS += -ffast-math -fstrength-reduce -finline-functions
#
#override NETSURF_USE_OPENSSL := YES

GCCSDK_INSTALL_ENV ?= /opt/netsurf/m68k-unknown-amigaos/env
GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/m68k-unknown-amigaos/cross/bin

PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config
PKG_CONFIG_PATH=../inst-amigaos3/lib/pkgconfig:/opt/netsurf/m68k-unknown-amigaos/env/lib/pkgconfig

CFLAGS += -DWITH_OPENSSL -D__NO_NET_API -D__NO_NETINCLUDE_ERRNO -I$(GCCSDK_INSTALL_ENV)/netinclude
CFLAGS += -I$(GCCSDK_INSTALL_ENV)/include 

CFLAGS += -DNO_INLINE_STDARG
#CFLAGS += -DWITH_NSLOG
#CFLAGS += -DIMAGE_CACHE_VERBOSE
CFLAGS += -DNDEBUG

NETSURF_FS_BACKING_STORE := YES

#resource path
CFLAGS += '-DNETSURF_FB_RESPATH="$(NETSURF_FB_RESPATH)"'

CFLAGS += -DTURBOJPEG

CFLAGS +=	-I/mnt/e/usr/local/amiga/m68k-amigaos/clib/extra/ns-include
#CFLAGS +=	-I/mnt/e/usr/local/amiga/m68k-amigaos/clib/extra/ns-include/turbojpeg
CFLAGS +=	-Ifrontends -Ifrontends/mui/include -Ifrontends/mui/include-ns -Ifrontends/mui/Prefs #-Inetsurf/include
CFLAGS +=	-Iinclude 

LDFLAGS += -Llib -Llibs -L/mnt/e/usr/local/amiga/m68k-amigaos/clib/extra/ns-lib
LDFLAGS += -lmui-3

NETSURF_USE_FREETYPE2 := YES
NETSURF_FEATURE_FREETYPE2_CFLAGS := -DFB_USE_FREETYPE

# non optional pkg-configed libs
LDFLAGS += -L$(GCCSDK_INSTALL_ENV)/lib
LDFLAGS += -lasyncio

# optional features
NETSURF_FEATURE_HUBBUB_CFLAGS := -DWITH_HUBBUB
NETSURF_FEATURE_BMP_CFLAGS := -DWITH_BMP
#NETSURF_FEATURE_GIF_CFLAGS := -DWITH_GIF
NETSURF_FEATURE_PNG_CFLAGS := -DWITH_PNG
#NETSURF_FEATURE_NSSVG_CFLAGS := -DWITH_NS_SVG

LDFLAGS += $(shell $(PKG_CONFIG) --libs tre)
LDFLAGS += -Wl,-Map=file.map  -lcurl  -lamisslstubs  -lamisslauto -liconv -lamiga2 -ldebug # -lttengine -lauto 
LDFLAGS += -ldom -lhubbub -lparserutils -lwapcaplet -lnslog -lz
LDFLAGS += -lnsbmp
LDFLAGS += -lpbl -liconv -lunix -lm -lc -lnet -ljpeg-turbo 
#-lft2


# S_MUI are sources purely for the MUI build cookies.c tree.c  hotlistwindowclass.c used amiga/fetch.c fetch_file.c  #filetype_mui.c 
S_MUI := netsurf.c methodstack.c applicationclass.c browserclass.c historylistclass.c historypopstringclass.c windowclass.c \
	transferanimclass.c navigationbarclass.c addressbarclass.c titleclass.c titlelabelclass.c toolbuttonclass.c \
	quicklinkgroupclass.c quicklinkbuttongroupclass.c quicklinkparentgroupclass.c \
	searchbarclass.c gui.c findtextclass.c popstringclass.c loginwindowclass.c \
  hotlistwindowclass.c downloadwindowclass.c  history.c \
	schedule.c thumbnail.c bitmap.c font.c utf8.c login.c	\
  plotters.c save_pdf.c pdfclass.c print.c \
	clipboard.c save_complete.c fetch.c findfile.c\
  search.c utils.c mui.c throbber.c extrasrc.c \
	mui_delay.c \
  loadpng.c \
  Prefs/gui_options.c Prefs/gui_optionsInit.c Prefs/gui_locale.c
S_MUI := $(addprefix frontends/mui/,$(S_MUI))

SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_MUI) $(S_IMAGES) $(S_FONTS)

EXETARGET := NetSurf
