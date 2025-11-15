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
CFLAGS += -w -std=c99 -g  -O2 -DPATH_MAX=1024 -D__m68k__ -m68020-60 -fomit-frame-pointer -I. -Dnsmui -D__AMIGA__ #-w #-finline-functions 
#CFLAGS += -DDEBUG=1
# -Dsmall <-- no SSL bez == -DWITH_SSL
#-ffast-math -fstrength-reduce -finline-functions \

#
#override NETSURF_USE_OPENSSL := YES


CFLAGS += -DWITH_OPENSSL -D__NO_NET_API -D__NO_NETINCLUDE_ERRNO -I$(GCCSDK_INSTALL_ENV)/netinclude
CFLAGS += -I$(GCCSDK_INSTALL_ENV)/include 

CFLAGS += -DENABLE_DOWNLOAD
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
#-Icontent -Icontent/fetchers -Icontent/handlers

LDFLAGS += -Llib -Llibs -L/mnt/e/usr/local/amiga/m68k-amigaos/clib/extra/ns-lib
LDFLAGS += -lmui

NETSURF_USE_FREETYPE2 := YES
NETSURF_FEATURE_FREETYPE2_CFLAGS := -DFB_USE_FREETYPE

# non optional pkg-configed libs

#LDFLAGS += -Wl,--whole-archive

#LDFLAGS +=  -Wl,--no-whole-archive

LDFLAGS += -L$(GCCSDK_INSTALL_ENV)/lib
   LDFLAGS += -lasyncio



  #NETSURF_FEATURE_ROSPRITE_CFLAGS := -DWITH_NSSPRITE
  NETSURF_FEATURE_HUBBUB_CFLAGS := -DWITH_HUBBUB
  NETSURF_FEATURE_BMP_CFLAGS := -DWITH_BMP
  #NETSURF_FEATURE_GIF_CFLAGS := -DWITH_GIF
  NETSURF_FEATURE_PNG_CFLAGS := -DWITH_PNG
  #NETSURF_FEATURE_NSSVG_CFLAGS := -DWITH_NS_SVG

    #$(eval $(call feature_enabled,ROSPRITE,-DWITH_NSSPRITE,-lrosprite,RISC OS Sprite decoder))
    #$(eval $(call feature_enabled,HUBBUB,-DWITH_HUBBUB,-lhubbub -lparserutils,Hubbub HTML parser))
    #$(eval $(call feature_enabled,BMP,-DWITH_BMP,-lnsbmp,NetSurf BMP decoder))
    #$(eval $(call feature_enabled,GIF,-DWITH_GIF,-lnsgif,NetSurf GIF decoder))
  #$(eval $(call feature_enabled,PNG,-DWITH_PNG,-lpng,PNG support))
LDFLAGS += $(shell $(PKG_CONFIG) --libs tre)
 # CFLAGS += -noixemul -std=c99 -I. -Dnsmui -mstring -mmultiple -mcpu=750 -idirafter/usr/local/include -Ifrontends/mui/include -D__MORPHOS_SHAREDLIBS -DAROS_ALMOST_COMPATIBLE -U__STRICT_ANSI__ -DMULTITHREADED
 # LDFLAGS += -Wl,-Map=file.map -noixemul -lxml2 -lcurl -lrxspencer -lssl -lcrypto -liconv -lttengine
  LDFLAGS += -Wl,-Map=file.map  -lcurl  -lamisslstubs  -lamisslauto -liconv -lamiga2 -ldebug # -lttengine -lauto 
  LDFLAGS += -lxml2
   LDFLAGS += -ldom -lhubbub -lparserutils -lwapcaplet -lnslog -lz
  LDFLAGS += -lnsbmp
   LDFLAGS += -lpbl -liconv -lunix -lm -lc -lnet -lft2 -ljpeg-turbo



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
  loadpng.c \
  Prefs/gui_options.c Prefs/gui_optionsInit.c Prefs/gui_locale.c
S_MUI := $(addprefix frontends/mui/,$(S_MUI))

SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_MUI) $(S_IMAGES) $(S_FONTS)

EXETARGET := NetSurf
