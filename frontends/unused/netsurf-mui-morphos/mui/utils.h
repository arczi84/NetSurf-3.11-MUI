#ifndef MUI_UTILS_H
#define MUI_UTILS_H

/*
 * Copyright 2009 Ilkka Lehtoranta <ilkleht@isoveli.org>
 *
 * This file is part of NetSurf.
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

/** \file
 * Various support functions.
 */

#include "extrasrc.h"

STRPTR GetNameFromLock(BPTR lock);
STRPTR BuildFileName(CONST_STRPTR path, CONST_STRPTR file);
STRPTR DupStr(CONST_STRPTR string);
WCHAR utf8_at_index(CONST_STRPTR str, LONG idx, size_t *bytelength);
ULONG utf8_octets(CONST_STRPTR str, LONG maxlen);
ULONG utf8_codepoints(CONST_STRPTR str, LONG maxlen);
STRPTR utf8_to_amiga(CONST_STRPTR src);

#endif /* MUI_UTILS_H */
