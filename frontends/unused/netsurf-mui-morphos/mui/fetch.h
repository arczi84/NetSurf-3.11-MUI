#ifndef MUI_FETCH_H
#define MUI_FETCH_H

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

#include "mui/extrasrc.h"

#include "content/content.h"

struct fetch_info
{
	struct MinNode node;
	UQUAD len;
	struct fetch *fetch_handle; /**< The fetch handle we're parented by. */
	APTR fh;
	char *path;
	char *url;		/**< URL of this fetch. */
	BOOL aborted;
	BOOL locked;
	BOOL only_2xx;		/**< Only HTTP 2xx responses acceptable. */
	int httpcode;
	char *mimetype;
	struct cache_data cachedata;
};

bool mui_fetch_initialise(const char *scheme);
void *mui_fetch_setup(struct fetch *parent_fetch, const char *url, bool only_2xx, const char *post_urlenc, struct form_successful_control *post_multipart, const char **headers, struct MinList *list);
void mui_fetch_finalise(struct MinList *list);

bool mui_fetch_start(void *vfetch);
void mui_fetch_abort(void *vf);
void mui_fetch_send_callback(fetch_msg msg, struct fetch_info *fetch, const void *data, UQUAD size);
void mui_fetch_free(void *vf);

#endif /* MUI_FETCH_H */
