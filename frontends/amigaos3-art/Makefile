#
# Makefile for NetSurf AmigaOS3 frontend
#
# This file is part of NetSurf 
#
# ----------------------------------------------------------------------------
# Amigaos3 target setup
# ----------------------------------------------------------------------------
VERSION := 3.11
# ----------------------------------------------------------------------------
CFLAGS += -std=c99 -s -O3 -DPATH_MAX=1024 -D__m68k__ -m68020-60 -fomit-frame-pointer -ffast-math -fstrength-reduce -finline-functions \
	      -Dnsframebuffer -Dsmall  -Dnsamigaos3 \
          -D__AMIGA__ -D__BIG_ENDIAN_BGRA__
#
#override NETSURF_USE_OPENSSL := YES

ifeq (NETSURF_USE_AMISSL,YES) 

$(eval $(call feature_enabled,AMISSL,-DWITH_AMISSL -DWITH_OPENSSL -D__NO_NET_API -D__NO_NETINCLUDE_ERRNO -I$(GCCSDK_INSTALL_ENV)/netinclude,-lamisslauto,AmiSSL))
else
#ifeq ($(NETSURF_USE_OPENSSL),YES)
#LDFLAGS += $(shell $(PKG_CONFIG) --static --libs openssl)
#endif
CFLAGS += -DWITH_OPENSSL -D__NO_NET_API -D__NO_NETINCLUDE_ERRNO -I$(GCCSDK_INSTALL_ENV)/netinclude
endif

CFLAGS += -I$(GCCSDK_INSTALL_ENV)/include

CFLAGS += -DENABLE_DOWNLOAD
CFLAGS += -DNO_INLINE_STDARG
CFLAGS += -DWITH_NSLOG

override  NETSURF_FB_FONTPATH="PROGDIR:Fonts"

NETSURF_FS_BACKING_STORE := YES

#resource path
CFLAGS += '-DNETSURF_FB_RESPATH="$(NETSURF_FB_RESPATH)"'

CFLAGS += -DTURBOJPEG

CFLAGS +=	-I/mnt/e/usr/local/amiga/m68k-amigaos/clib/extra/ns-include
#CFLAGS +=	-I/mnt/e/usr/local/amiga/m68k-amigaos/clib/extra/ns-include/turbojpeg
CFLAGS +=	-Ifrontends/amigaos3-art/libnsfb/include
CFLAGS +=	-Iinclude

LDFLAGS += -Llib -Llibs -L/mnt/e/usr/local/amiga/m68k-amigaos/clib/extra/ns-lib
LDFLAGS += -lmui

NETSURF_USE_FREETYPE2 := YES
NETSURF_FEATURE_FREETYPE2_CFLAGS := -DFB_USE_FREETYPE

# non optional pkg-configed libs
#LDFLAGS += -Wl,--whole-archive
#$(eval $(call pkg_config_find_and_add,libnsfb,libnsfb))
#LDFLAGS +=  -Wl,--no-whole-archive
LDFLAGS += -lsdl
LDFLAGS += $(shell $(PKG_CONFIG) --libs tre)
LDFLAGS += -L$(GCCSDK_INSTALL_ENV)/lib

LDFLAGS += -lpbl -liconv -lunix -ltheme -lm -lc -lamiga2 -lnet -lft2 -ljpeg-turbo

#LDFLAGS += -Wl,--whole-archive
#LDFLAGS += -L/mnt/e/usr/local/amiga/m68k-amigaos/clib/extra/ns-lib/jmemnobs.o 
#LDFLAGS += -L/mnt/e/usr/local/amiga/m68k-amigaos/clib/extra/ns-lib/jerror.o
#LDFLAGS +=  -Wl,--no-whole-archive

# ---------------------------------------------------------------------------
# Target setup
# ---------------------------------------------------------------------------

# The filter and target for split messages
MESSAGES_FILTER=ami
MESSAGES_TARGET=$(FRONTEND_RESOURCES_DIR)

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------
# Amiga sources 
ifeq ($(DEBUG_BUILD),YES)
S_AMIGAOS3 :=  gui-fb.c
else
S_AMIGAOS3 :=  gui.c
endif

S_AMIGAOS3 +=  framebuffer.c misc.c  utf8.c  loadpng.c
S_AMIGAOS3 +=  ami_clipboard.c clipboard.c version.c
S_AMIGAOS3 +=  fetch.c findfile.c conv.c check_version.c

ifeq ($(NO_MUI),)
S_AMIGAOS3 += mui/gui_optionsInit.c mui/gui_options.c mui/gui_locale.c mui/gui_url.c mui/gui_urlInit.c
S_AMIGAOS3 += mui/downloadInit.c mui/download.c
endif

# FRAMEBUFFER sources 
S_FRAMEBUFFER :=   local_history.c bitmap.c  corewindow.c schedule.c 
S_FRAMEBUFFER_FBTK +=  fbtk.c fill.c user.c window.c

ifeq ($(DEBUG_BUILD),YES)
	S_FRAMEBUFFER_FBTK += event.c  text.c bitmap.c ../../amigaos3-art/fbtk/scroll.c
else
	S_AMIGAOS3_FBTK := event.c text.c bitmap.c scroll.c 
endif

S_LIBNSFB_PLOT := api.c util.c generic.c 32bpp-xbgr8888.c 32bpp-xrgb8888.c 16bpp.c 8bpp.c #24bpp.c 
S_LIBNSFB_SURFACE := surface.c ram.c sdl.c
S_LIBNSFB := libnsfb.c dump.c cursor.c palette.c

S_AMIGAOS3  += font_freetype.c font_internal.c #os3support.c

S_AMIGA +=  #schedule.c #iff_dr2d.c

ifeq ($(RTG),YES)
S_LIBNSFB_SURFACE +=  getpixel.c
endif

S_AMIGA :=	$(addprefix frontends/amiga/,$(S_AMIGA))
S_FRAMEBUFFER := $(addprefix frontends/framebuffer/,$(S_FRAMEBUFFER)) $(addprefix frontends/framebuffer/fbtk/,$(S_FRAMEBUFFER_FBTK)) 
S_AMIGAOS3 :=	$(addprefix frontends/amigaos3-art/,$(S_AMIGAOS3)) $(addprefix frontends/amigaos3-art/fbtk/,$(S_AMIGAOS3_FBTK))
S_LIBNSFB :=  $(addprefix frontends/amigaos3-art/libnsfb/,$(S_LIBNSFB))  $(addprefix frontends/amigaos3-art/libnsfb/plot/,$(S_LIBNSFB_PLOT))  \
							$(addprefix frontends/amigaos3-art/libnsfb/surface/,$(S_LIBNSFB_SURFACE))  

S_AMIGA_FB :=  $(S_FRAMEBUFFER) $(S_AMIGAOS3) $(S_LIBNSFB) $(S_AMIGA)

# This is the final source build list
# Note this is deliberately *not* expanded here as common and image
#   are not yet available
SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_AMIGA_FB) $(S_IMAGES) $(S_FONTS)

EXETARGET := NetSurf-SDL

# ----------------------------------------------------------------------------
# Install target
# ----------------------------------------------------------------------------

NETSURF_FRAMEBUFFER_RESOURCE_LIST := adblock.css credits.html	\
	default.css internal.css licence.html			\
	netsurf.png quirks.css welcome.html

install-framebuffer:
	$(VQ)echo " INSTALL: $(DESTDIR)/$(PREFIX)"
	$(Q)$(INSTALL) -d $(DESTDIR)/$(NETSURF_FRAMEBUFFER_BIN)
	$(Q)$(INSTALL) -T $(EXETARGET) $(DESTDIR)/$(NETSURF_FRAMEBUFFER_BIN)/netsurf
	$(Q)$(INSTALL) -d $(DESTDIR)/$(NETSURF_FRAMEBUFFER_RESOURCES)
	$(Q)for F in $(NETSURF_FRAMEBUFFER_RESOURCE_LIST); do $(INSTALL) -m 644 $(FRONTEND_RESOURCES_DIR)/$$F $(DESTDIR)/$(NETSURF_FRAMEBUFFER_RESOURCES); done
	$(Q)$(INSTALL) -m 644 -T $(MESSAGES_TARGET)/en/Messages $(DESTDIR)/$(NETSURF_FRAMEBUFFER_RESOURCES)/Messages

# ----------------------------------------------------------------------------
# Package target
# ----------------------------------------------------------------------------

package: netsurf-m68k.lha
package-amikit: netsurf-amikit.lha
package-aga: netsurf-m68k-aga.lha
AMIGA_LANGUAGES := de en it ja nl pl
AMIGA_PLATFORM_RESOURCES := Themes Icons default.css default.css.info favicon.png LangNames Resource.map splash.png bookmarks.htm Options
AMIGA_PLATFORM_RESOURCES_AK := Themes Icons default.css default.css.info favicon.png LangNames Resource.map splash.png bookmarks.htm Options-AK
AMIGA_GENERIC_RESOURCES := $(AMIGA_LANGUAGES) ca-bundle 
AMIGA_RESOURCES := $(addprefix frontends/amigaos3-art/resources/,$(AMIGA_PLATFORM_RESOURCES)) $(addprefix frontends/amigaos3-art/resources/,$(AMIGA_GENERIC_RESOURCES))
AMIGA_RESOURCES_AK := $(addprefix frontends/amigaos3-art/resources/,$(AMIGA_PLATFORM_RESOURCES_AK)) $(addprefix frontends/amigaos3-art/resources/,$(AMIGA_GENERIC_RESOURCES))
AMIGA_DISTRIBUTION_FILES := frontends/amigaos3-art/dist/*
AMIGA_INSTALL_TARGET_DIR := NetSurf_Amiga
netsurf-m68k.lha: $(EXETARGET)
	$(VQ)echo Creating netsurf-m68k.lha
	$(Q)rm -rf $(AMIGA_INSTALL_TARGET_DIR)
	$(Q)mkdir -p $(AMIGA_INSTALL_TARGET_DIR)/NetSurf
	$(Q)cp -p NetSurf $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/NetSurf
#	$(Q)cp -p NetSurf-AGA $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/NetSurf-AGA	
	$(Q)mkdir $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources
	$(Q)cp -r $(AMIGA_RESOURCES) $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources
	$(Q)cp -r $(AMIGA_DISTRIBUTION_FILES) $(AMIGA_INSTALL_TARGET_DIR)/NetSurf
	$(Q)cp frontends/framebuffer/res/ca-bundle $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/ca-bundle
	$(Q)cp frontends/framebuffer/res/adBlock.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/adblock.css
	$(Q)cp frontends/framebuffer/res/default.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/default.css
	$(Q)cp frontends/framebuffer/res/internal.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/internal.css
	$(Q)cp frontends/framebuffer/res/quirks.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/quirks.css
	$(Q)cp frontends/framebuffer/res/netsurf.png $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/netsurf.png
	$(Q)cp frontends/amigaos3-art/pkg/drawer.info $(AMIGA_INSTALL_TARGET_DIR)/NetSurf.info
	chmod 777 NetSurf_Amiga *.* -R
	$(Q)cd $(AMIGA_INSTALL_TARGET_DIR); \
	  lha a netsurf-m68k-$(VERSION).lha NetSurf NetSurf.info
upload:
	$(VQ)echo Uploading...
	#$(Q)cp $(AMIGA_INSTALL_TARGET_DIR)/netsurf-m68k.lha "b:\dropbox\netsurf\3.6\"
	#$(Q)date > "b:\dropbox\netsurf\wip\version.txt"
	
netsurf-m68k-aga.lha: $(EXETARGET)
	$(VQ)echo Creating netsurf-m68k-aga.lha
	$(Q)rm -rf $(AMIGA_INSTALL_TARGET_DIR)
	$(Q)mkdir -p $(AMIGA_INSTALL_TARGET_DIR)/NetSurf
	$(Q)cp -p NetSurf-AGA-nojs $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/NetSurf-fpu
	$(Q)cp -p NetSurf-AGA $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/NetSurf
	$(Q)mkdir $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources
	$(Q)cp -r $(AMIGA_RESOURCES) $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources
	$(Q)cp -r $(AMIGA_DISTRIBUTION_FILES) $(AMIGA_INSTALL_TARGET_DIR)/NetSurf
	$(Q)cp frontends/framebuffer/res/AdBlock.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/adblock.css
	$(Q)cp frontends/framebuffer/res/default.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/default.css
	$(Q)cp frontends/framebuffer/res/internal.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/internal.css
	$(Q)cp frontends/framebuffer/res/Quirks.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/quirks.css
	$(Q)cp frontends/framebuffer/res/netsurf.png $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/netsurf.png
	$(Q)cp frontends/amigaos3-art/pkg/drawer.info $(AMIGA_INSTALL_TARGET_DIR)/NetSurf.info
	chmod 777 NetSurf_Amiga *.* -R
	$(Q)cd $(AMIGA_INSTALL_TARGET_DIR); \
	  lha a netsurf-m68k-aga.lha NetSurf NetSurf.info
upload-aga:
	$(VQ)echo Uploading...
	$(Q)cp $(AMIGA_INSTALL_TARGET_DIR)/netsurf-m68k-aga.lha "b:\dropbox\netsurf\3.6\wip"
	$(Q)date > "b:\dropbox\netsurf\wip\version.txt"
netsurf-amikit.lha: $(EXETARGET)
	$(VQ)echo Creating netsurf-amikit.lha
	$(Q)rm -rf $(AMIGA_INSTALL_TARGET_DIR)
	$(Q)mkdir -p $(AMIGA_INSTALL_TARGET_DIR)/NetSurf
	$(Q)cp -p NetSurf $(AMIGA_INSTALL_TARGET_DIR)/NetSurf
	$(Q)mkdir $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources
	$(Q)cp -r $(AMIGA_RESOURCES_AK) $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources
	$(Q)cp -r $(AMIGA_DISTRIBUTION_FILES) $(AMIGA_INSTALL_TARGET_DIR)/NetSurf
	$(Q)cp frontends/framebuffer/res/AdBlock.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/adblock.css
	$(Q)cp frontends/framebuffer/res/default.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/default.css
	$(Q)cp frontends/framebuffer/res/internal.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/internal.css
	$(Q)cp frontends/framebuffer/res/Quirks.css $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/quirks.css
	$(Q)cp frontends/framebuffer/res/netsurf.png $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/netsurf.png
	$(Q)cp frontends/amigaos3-art/pkg/drawer.info $(AMIGA_INSTALL_TARGET_DIR)/NetSurf.info
	$(Q)cp -f frontends/amigaos3-art/pkg/ak/*.info $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/
	chmod 777 Netsurf_Amiga *.* -R
	mv $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/Options-AK $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Resources/Options
	rm -R $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/C
	rm $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/NetSurf-AGA.info
	rm -R $(AMIGA_INSTALL_TARGET_DIR)/NetSurf/Libs
	$(Q)cd $(AMIGA_INSTALL_TARGET_DIR); \
	  lha a netsurf-amikit.lha NetSurf NetSurf.info
upload-amikit:
	$(VQ)echo Uploading...
	$(Q)cp $(AMIGA_INSTALL_TARGET_DIR)/netsurf-amikit.lha "b:\dropbox\netsurf\amikit"
	$(Q)date > "b:\dropbox\netsurf\wip\version.txt"
