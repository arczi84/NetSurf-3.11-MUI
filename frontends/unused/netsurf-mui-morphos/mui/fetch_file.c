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

/** \file
 * Fetching of data from a file (implementation).
 */

#include <malloc.h>

#include <proto/asyncio.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>

#include "content/content.h"
#include "content/fetch.h"
#include "mui/fetch.h"
#include "mui/mui.h"
#include "mui/utils.h"
#include "utils/log.h"
#include "utils/url.h"

STATIC struct MinList file_fetcher_list = { (APTR)&file_fetcher_list.mlh_Tail, NULL, (APTR)&file_fetcher_list };
STATIC UBYTE fetch_buffer[16384];

/**
 * Finalise a cURL fetcher
 */

void mui_fetch_file_finalise(const char *scheme)
{
	mui_fetch_finalise(&file_fetcher_list);
}


/**
 * Start fetching data for the given URL.
 *
 * The function returns immediately. The fetch may be queued for later
 * processing.
 *
 * A pointer to an opaque struct curl_fetch_info is returned, which can be passed to
 * fetch_abort() to abort the fetch at any time. Returns 0 if memory is
 * exhausted (or some other fatal error occurred).
 *
 * The caller must supply a callback function which is called when anything
 * interesting happens. The callback function is first called with msg
 * FETCH_TYPE, with the Content-Type header in data, then one or more times
 * with FETCH_DATA with some data for the url, and finally with
 * FETCH_FINISHED. Alternatively, FETCH_ERROR indicates an error occurred:
 * data contains an error message. FETCH_REDIRECT may replace the FETCH_TYPE,
 * FETCH_DATA, FETCH_FINISHED sequence if the server sends a replacement URL.
 *
 * Some private data can be passed as the last parameter to fetch_start, and
 * callbacks will contain this.
 */

static void *mui_fetch_file_setup(struct fetch *parent_fetch, const char *url, bool only_2xx, const char *post_urlenc, struct form_successful_control *post_multipart, const char **headers)
{
	struct fetch_info *fetch;

	fetch = mui_fetch_setup(parent_fetch, url, only_2xx, post_urlenc, post_multipart, headers, &file_fetcher_list);

	if (fetch) {
		LONG ok;

		fetch->path = url_to_path(url);
		ok = 0;

		if (fetch->path) {
			fetch->fh = OpenAsync(fetch->path, MODE_READ, sizeof(fetch_buffer));

			if (fetch->fh) {
				BPTR lock;

				lock = Lock(fetch->path, ACCESS_READ);

				if (lock) {
					D_S(struct FileInfoBlock, fib)

					#if defined(__MORPHOS__)
					if (DOSBase->dl_lib.lib_Version >= 51) {
						if (Examine64(lock, fib, NULL))
							fetch->len = fib->fib_Size64;
					}
					else
					#endif
					{
						if (Examine(lock, fib))
							fetch->len = fib->fib_Size;
					}

					UnLock(lock);
				}

				fetch_set_http_code(fetch->fetch_handle, 200);
				fetch->mimetype = fetch_mimetype(fetch->path);
				LOG(("mimetype %s len %ld", fetch->mimetype, fetch->len));

				mui_fetch_send_callback(FETCH_TYPE, fetch, fetch->mimetype, fetch->len);
				ok = 1;
			} else {
				STRPTR errorstring;
				ULONG length;

				length = sizeof("Could not open file\n%s") + strlen(fetch->path);

				errorstring = AllocTaskPooled(length);

				if (errorstring) {
					NewRawDoFmt("Could not open file\n%s", NULL, errorstring, fetch->path);
					fetch_set_http_code(fetch->fetch_handle, 404);
					mui_fetch_send_callback(FETCH_ERROR, fetch, errorstring, 0);
					fetch->aborted = true;
					FreeTaskPooled(errorstring, length);
				}
			}
		}

		if (!ok) {
			mui_fetch_free(fetch);
			fetch = NULL;
		}
	}

	return fetch;
}

/**
 * Do some work on current fetches.
 *
 * Must be called regularly to make progress on fetches.
 */

static void mui_fetch_file_poll(const char *scheme_ignored)
{
	struct fetch_info *next, *fetch;

	ITERATELISTSAFE(fetch, next, &file_fetcher_list) {
		if (fetch->locked) continue;

		if (!fetch->aborted) {
			ULONG len;

			len = ReadAsync(fetch->fh, fetch_buffer, sizeof(fetch_buffer));

			LOG(("fetch %lx read %ld",fetch,len));

			mui_fetch_send_callback(FETCH_DATA, fetch, fetch_buffer, len);

			if (len < sizeof(fetch_buffer) && !fetch->aborted) {
				mui_fetch_send_callback(FETCH_FINISHED, fetch, &fetch->cachedata, 0);
				fetch->aborted = TRUE;
			}
		}

		if (fetch->aborted) {
			fetch_remove_from_queues(fetch->fetch_handle);
			fetch_free(fetch->fetch_handle);
		}
	}
}

/**
 * Initialise the fetcher.
 *
 * Must be called once before any other function.
 */

void mui_fetch_file_register(void)
{
	if (!fetch_add_fetcher("file",
				       mui_fetch_initialise,
				       mui_fetch_file_setup,
				       mui_fetch_start,
				       mui_fetch_abort,
				       mui_fetch_free,
				       mui_fetch_file_poll,
				       mui_fetch_file_finalise)) {
			LOG(("Unable to register Amiga fetcher for file:"));
	}
}
