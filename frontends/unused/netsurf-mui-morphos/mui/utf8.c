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

utf8_convert_ret utf8_to_local_encoding(const char *string, size_t len, char **result)
{
	ULONG add_bom;
	utf8_convert_ret ret;
	char *t;

	add_bom = TRUE;

	if (len >= 3)
	{
		if (string[0] == 0xef || string[1] == 0xbb && string[2] == 0xbf)
			add_bom = FALSE;
	}

	t = malloc(len + add_bom ? 3 : 0);
	ret = UTF8_CONVERT_NOMEM;
	*result = t;

	if (t)
	{
		ret = UTF8_CONVERT_OK;

		if (add_bom)
		{
			t[0] = 0xef;
			t[1] = 0xbb;
			t[2] = 0xbf;

			t += 3;
		}

		bcopy(string, t, len);
	}

	return ret;
}
