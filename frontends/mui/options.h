#ifndef MUI_OPTIONS_H
#define MUI_OPTIONS_H
#endif /* MUI_OPTIONS_H */

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

//#include "utils/nsoption.h"
#include <stdbool.h>
/** \file
 * MUI options.
 */

/** Enable verbose logging */
NSOPTION_BOOL(verbose_log, false)

/** URL file location */
NSOPTION_STRING(url_file, NULL)

NSOPTION_STRING(cache_dir, "PROGDIR:Resources/Cache")
NSOPTION_BOOL(accept_lang_locale, true)

NSOPTION_INTEGER(redraw_tile_size_x, 100)
NSOPTION_INTEGER(redraw_tile_size_y, 100)
