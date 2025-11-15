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

#include <proto/asyncio.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include "content/fetch.h"
#include "mui/fetch.h"
#include "mui/mui.h"

bool mui_fetch_initialise(const char *scheme)
{
	return true;
}

void mui_fetch_finalise(struct MinList *list)
{
	struct fetch_info *node, *next;

	while ((node = REMHEAD(list)))
	{
		if (node->fh)
			CloseAsync(node->fh);

		FreeMem(node, sizeof(*node));
	}
}

void *mui_fetch_setup(struct fetch *parent_fetch, const char *url, bool only_2xx, const char *post_urlenc, struct form_successful_control *post_multipart, const char **headers, struct MinList *list)
{
	struct fetch_info *fetch;

	fetch = AllocMem(sizeof (*fetch), MEMF_CLEAR);

	if (fetch) {
		ADDTAIL(list, fetch);

		fetch->fetch_handle = parent_fetch;
		fetch->only_2xx = only_2xx;
	}

	return fetch;
}

/**
 * Dispatch a single job
 */
bool mui_fetch_start(void *vfetch)
{
	struct fetch_info *fetch = (struct fetch_info *)vfetch;

	fetch->cachedata.req_time = time(NULL);
	fetch->cachedata.res_time = time(NULL);
	fetch->cachedata.date = 0;
	fetch->cachedata.expires = 0;
	fetch->cachedata.age = INVALID_AGE;
	fetch->cachedata.max_age = 0;
	fetch->cachedata.no_cache = true;
	fetch->cachedata.etag = NULL;
	fetch->cachedata.last_modified = 0;

	return true;
}

void mui_fetch_abort(void *vf)
{
	struct fetch_info *fetch = (struct fetch_info *)vf;
	APTR fh;

	if ((fh = fetch->fh)) {
		fetch->fh = NULL;
		CloseAsync(fh);
	}

	fetch->aborted = TRUE;
}

void mui_fetch_send_callback(fetch_msg msg, struct fetch_info *fetch, const void *data, UQUAD size)
{
	fetch->locked = TRUE;
	fetch_send_callback(msg, fetch->fetch_handle, data, size);
	fetch->locked = FALSE;
}

/**
 * Free a fetch structure and associated resources.
 */

void mui_fetch_free(void *vf)
{
	struct fetch_info *fetch = (struct fetch_info *)vf;

	REMOVE(fetch);

	if (fetch->fh) CloseAsync(fetch->fh);
	if (fetch->path) free(fetch->path);
	if (fetch->url) free(fetch->url);
	if (fetch->mimetype) free(fetch->mimetype);

	FreeMem(fetch, sizeof(*fetch));
}
