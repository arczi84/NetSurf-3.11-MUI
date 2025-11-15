#ifndef NETSURF_MUI_OPTIONS_H
#define NETSURF_MUI_OPTIONS_H
#endif

#include "utils/nsoption.h"

/*
 * Copyright 2009 Ilkka Lehtoranta <ilkleht@isoveli.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** Enable verbose logging */
NSOPTION_BOOL(verbose_log, false)

/** URL file location */
NSOPTION_STRING(url_file, NULL)

NSOPTION_STRING(cache_dir, "PROGDIR:Resources/Cache")
NSOPTION_BOOL(accept_lang_locale, true)

NSOPTION_INTEGER(redraw_tile_size_x, 100)
NSOPTION_INTEGER(redraw_tile_size_y, 100)

/***** surface options *****/
NSOPTION_INTEGER(fb_depth, 32)
NSOPTION_INTEGER(fb_refresh, 70)
NSOPTION_STRING(fb_device, NULL)
NSOPTION_STRING(fb_input_devpath, NULL)
NSOPTION_STRING(fb_input_glob, NULL)

/***** Amiga-specific options *****/
NSOPTION_INTEGER(scale_aga, 74)
NSOPTION_INTEGER(browser_dpi, 84)
NSOPTION_INTEGER(window_depth, 0)
NSOPTION_INTEGER(mobile_mode, -1)
NSOPTION_INTEGER(gui_font_size, 12)

NSOPTION_BOOL(warp_mode, false)
NSOPTION_BOOL(fullscreen, false)
NSOPTION_BOOL(bitmap_fonts, false)
NSOPTION_BOOL(autodetect_depth, true)
NSOPTION_BOOL(module_autoplay, false)
NSOPTION_BOOL(low_dither_quality, false)
NSOPTION_BOOL(builtinDM, true)
NSOPTION_BOOL(autoupdate, true)
NSOPTION_BOOL(show_conv_status, false)

NSOPTION_STRING(youtube_handler, NULL)
NSOPTION_STRING(screen_modeid, NULL)
NSOPTION_STRING(download_manager, NULL)
NSOPTION_STRING(download_path, NULL)
NSOPTION_STRING(def_search_bar, NULL)
NSOPTION_STRING(text_editor, NULL)
NSOPTION_STRING(net_player, NULL)
NSOPTION_STRING(module_player, NULL)
NSOPTION_STRING(mpeg_player, NULL)
NSOPTION_STRING(theme, NULL)
NSOPTION_STRING(favicon_source, NULL)
NSOPTION_STRING(user_agent, NULL)

NSOPTION_STRING(mpg_width, "640")
NSOPTION_STRING(mpg_height, "360")
NSOPTION_STRING(mpg_bitrate, "700")
NSOPTION_STRING(mpg_audio_rate, "128")
NSOPTION_STRING(mpg_framerate, "24")
NSOPTION_STRING(mpg_audio_frequency, "44100")
NSOPTION_INTEGER(mpg_audio_rate_i, 2)
NSOPTION_STRING(mpg_audio_format, "mp2")
NSOPTION_INTEGER(mpg_audio_format_i, 1)
NSOPTION_INTEGER(mpg_prebuffer, 20)
NSOPTION_INTEGER(youtube_autoplay, 0)

NSOPTION_INTEGER(dither_quality, 0)
NSOPTION_INTEGER(cache_bitmaps, 0)
NSOPTION_INTEGER(mask_alpha, 0)

/***** font options *****/
NSOPTION_INTEGER(fb_font_cachesize, 2048)
NSOPTION_STRING(fb_face_sans_serif, NULL)
NSOPTION_STRING(fb_face_sans_serif_bold, NULL)
NSOPTION_STRING(fb_face_sans_serif_italic, NULL)
NSOPTION_STRING(fb_face_sans_serif_italic_bold, NULL)
NSOPTION_STRING(fb_face_serif, NULL)
NSOPTION_STRING(fb_face_serif_bold, NULL)
NSOPTION_STRING(fb_face_monospace, NULL)
NSOPTION_STRING(fb_face_monospace_bold, NULL)
NSOPTION_STRING(fb_face_cursive, NULL)
NSOPTION_STRING(fb_face_fantasy, NULL)

/***** favourites *****/
NSOPTION_STRING(favourite_1_url, NULL)
NSOPTION_STRING(favourite_2_url, NULL)
NSOPTION_STRING(favourite_3_url, NULL)
NSOPTION_STRING(favourite_4_url, NULL)
NSOPTION_STRING(favourite_5_url, NULL)
NSOPTION_STRING(favourite_6_url, NULL)
NSOPTION_STRING(favourite_7_url, NULL)
NSOPTION_STRING(favourite_8_url, NULL)
NSOPTION_STRING(favourite_9_url, NULL)
NSOPTION_STRING(favourite_10_url, NULL)
NSOPTION_STRING(favourite_11_url, NULL)
NSOPTION_STRING(favourite_12_url, NULL)
NSOPTION_STRING(favourite_1_label, NULL)
NSOPTION_STRING(favourite_2_label, NULL)
NSOPTION_STRING(favourite_3_label, NULL)
NSOPTION_STRING(favourite_4_label, NULL)
NSOPTION_STRING(favourite_5_label, NULL)
NSOPTION_STRING(favourite_6_label, NULL)
NSOPTION_STRING(favourite_7_label, NULL)
NSOPTION_STRING(favourite_8_label, NULL)
NSOPTION_STRING(favourite_9_label, NULL)
NSOPTION_STRING(favourite_10_label, NULL)
NSOPTION_STRING(favourite_11_label, NULL)
NSOPTION_STRING(favourite_12_label, NULL)


