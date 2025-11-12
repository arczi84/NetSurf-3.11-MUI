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

#include <proto/icon.h>
#include <proto/dos.h>
#include <proto/datatypes.h>

#include "content/fetch.h"
#include "utils/log.h"
#include "utils/utils.h"

/**
 * filetype -- determine the MIME type of a local file
 */

struct filetype
{
	CONST_STRPTR pattern;
	CONST_STRPTR mimetype;
};

static const struct filetype mimetypes[] =
{
	// text
	{ "#?.css", "text/css" },
	{ "#?.(html|htm|shtml)", "text/html" },
	{ "#?.(txt|readme|c|cc|cpp|h|i|s|asm|cue|dok|notes)", "text/plain" },

	// image
	{ "#?.(jpg|jpeg)", "image/jpeg" },
	{ "#?.gif", "image/gif" },
	{ "#?.png", "image/png" },
	{ "#?.bmp", "image/bmp" },

	// application
	{ "#?.(lha|zip|lhx|gz|bz2|tar)", "application/octet-stream" },

	NULL, NULL
};

const char *fetch_filetype(const char *unix_path)
{
	CONST struct filetype *types;

	types = mimetypes;

	do
	{
		TEXT pat[64];

		ParsePatternNoCase(types->pattern, pat, sizeof(pat));

		if (MatchPatternNoCase(pat, (STRPTR)unix_path))
		{
			return types->mimetype;
		}

		types++;
	}
	while (types->pattern);

	return "text/plain";
}

char *fetch_mimetype(const char *ro_path)
{
	return strdup(fetch_filetype(ro_path));
}
