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

#include <stdlib.h>
#include <string.h>

#include <proto/keymap.h>

#include "utils/utf8.h"
#include "utils/errors.h"

#include "netsurf/utf8.h"

#include "amiga/utf8.h"

/* Updated function signature to match newer API */
nserror utf8_to_local_encoding1(const char *string, size_t len, char **result)
{
	ULONG add_bom;
	nserror ret;
	char *t;

	add_bom = TRUE;

	if (len >= 3)
	{
		if (string[0] == 0xef && string[1] == 0xbb && string[2] == 0xbf)
			add_bom = FALSE;
	}

	t = malloc(len + (add_bom ? 3 : 0));
	ret = NSERROR_NOMEM;
	*result = t;

	if (t)
	{
		ret = NSERROR_OK;

		if (add_bom)
		{
			t[0] = 0xef;
			t[1] = 0xbb;
			t[2] = 0xbf;

			t += 3;
		}

		memcpy(t, string, len);
	}

	return ret;
}

/* Add the missing utf8_from_local_encoding function */
nserror utf8_from_local_encoding1(const char *string, size_t len, char **result)
{
	char *t;
	size_t offset = 0;

	/* Check for BOM and skip it if present */
	if (len >= 3 && string[0] == 0xef && string[1] == 0xbb && string[2] == 0xbf) {
		offset = 3;
	}

	t = malloc(len - offset + 1);
	if (!t) {
		*result = NULL;
		return NSERROR_NOMEM;
	}

	memcpy(t, string + offset, len - offset);
	t[len - offset] = '\0';
	*result = t;

	return NSERROR_OK;
}


static struct gui_utf8_table utf8_table = {
	.utf8_to_local = utf8_to_local_encoding1,
	.local_to_utf8 = utf8_from_local_encoding1,
};

struct gui_utf8_table *amiga_utf8_table = &utf8_table;