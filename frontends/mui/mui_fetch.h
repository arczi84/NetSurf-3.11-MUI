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

#ifndef MUI_FETCH_H
#define MUI_FETCH_H

#include <exec/types.h>
#include <exec/lists.h>
#include <stdbool.h>
#include <stdint.h>

#include "content/fetch.h"
#include "utils/nsurl.h"
#include "mui/extrasrc.h"

/* Cache constants */
#define INVALID_AGE -1

struct cache_data {
    time_t req_time;     /**< Time of request */
    time_t res_time;     /**< Time of response */
    time_t date;         /**< Date: response header */
    time_t expires;      /**< Expires: response header */
    int age;             /**< Age: response header */
    int max_age;         /**< Max-Age Cache-control parameter */
    bool no_cache;       /**< no-cache Cache-control parameter */
    char *etag;          /**< Etag: response header */
    time_t last_modified; /**< Last-Modified: response header */
};

struct fetch_info
{
    struct MinNode node;
    UQUAD len;
    struct fetch *fetch_handle; /**< The fetch handle we're parented by. */
    APTR fh;
    char *path;
    char *url;                   /**< URL of this fetch. */
    BOOL aborted;
    BOOL locked;
    BOOL only_2xx;              /**< Only HTTP 2xx responses acceptable. */
    int httpcode;
    char *mimetype;
    struct cache_data cachedata; /**< Cache control data */
};

/* Function prototypes */
bool mui_fetch_initialise(const char *scheme);

void *mui_fetch_setup(struct fetch *parent_fetch, nsurl *url, bool only_2xx, 
                      const char *post_urlenc, 
                      const struct fetch_multipart_data *post_multipart, 
                      const char **headers, struct MinList *list);

void mui_fetch_finalise(struct MinList *list);

bool mui_fetch_start(void *vfetch);

void mui_fetch_abort(void *vf);

void mui_fetch_send_callback(const fetch_msg *msg, struct fetch_info *fetch);

void mui_fetch_free(void *vf);

/* Additional helper functions */
void mui_fetch_file_finished(struct fetch_info *fetch);

void mui_fetch_file_error(struct fetch_info *fetch, const char *error_msg);

void mui_fetch_send_header(struct fetch_info *fetch, const char *header_data, size_t len);

void mui_fetch_send_data(struct fetch_info *fetch, const uint8_t *data, size_t len);

void mui_fetch_set_http_code(struct fetch_info *fetch, long http_code);

#endif /* MUI_FETCH_H */