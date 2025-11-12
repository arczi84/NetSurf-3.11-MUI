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
#include <time.h>

#include "content/fetch.h"
#include "utils/errors.h"
#include "utils/nsurl.h"
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

void *mui_fetch_setup(struct fetch *parent_fetch, nsurl *url, bool only_2xx, 
                     const char *post_urlenc, 
                     const struct fetch_multipart_data *post_multipart, 
                     const char **headers, struct MinList *list)
{
    struct fetch_info *fetch;
    
    fetch = AllocMem(sizeof (*fetch), MEMF_CLEAR);
    if (fetch) {
        ADDTAIL(list, fetch);
        fetch->fetch_handle = parent_fetch;
        fetch->only_2xx = only_2xx;
        
        /* Store URL string */
        if (url) {
            const char *url_str = nsurl_access(url);
            if (url_str) {
                fetch->url = strdup(url_str);
            }
        }
    }
    
    return fetch;
}

/**
 * Dispatch a single job
 */
bool mui_fetch_start(void *vfetch)
{
    struct fetch_info *fetch = (struct fetch_info *)vfetch;
    
    if (!fetch) {
        return false;
    }
    
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
    
    if (!fetch) {
        return;
    }
    
    if ((fh = fetch->fh)) {
        fetch->fh = NULL;
        CloseAsync(fh);
    }
    
    fetch->aborted = TRUE;
}

void mui_fetch_send_callback(const fetch_msg *msg, struct fetch_info *fetch)
{
    if (!fetch || !msg) {
        return;
    }
    
    fetch->locked = TRUE;
    fetch_send_callback(msg, fetch->fetch_handle);
    fetch->locked = FALSE;
}

/**
 * Free a fetch structure and associated resources.
 */
void mui_fetch_free(void *vf)
{
    struct fetch_info *fetch = (struct fetch_info *)vf;
    
    if (!fetch) {
        return;
    }
    
    REMOVE(fetch);
    
    if (fetch->fh) 
        CloseAsync(fetch->fh);
    if (fetch->path) 
        free(fetch->path);
    if (fetch->url) 
        free(fetch->url);
    if (fetch->mimetype) 
        free(fetch->mimetype);
    
    FreeMem(fetch, sizeof(*fetch));
}

/**
 * Handle a completed fetch of a file.
 */
void mui_fetch_file_finished(struct fetch_info *fetch)
{
    fetch_msg msg;
    
    if (!fetch) {
        return;
    }
    
    /* Send finished message */
    msg.type = FETCH_FINISHED;
    mui_fetch_send_callback(&msg, fetch);
}

/**
 * Handle an error during fetch.
 */
void mui_fetch_file_error(struct fetch_info *fetch, const char *error_msg)
{
    fetch_msg msg;
    
    if (!fetch) {
        return;
    }
    
    /* Send error message */
    msg.type = FETCH_ERROR;
    msg.data.error = error_msg ? error_msg : "Unknown error";
    mui_fetch_send_callback(&msg, fetch);
}

/**
 * Send header data for a fetch.
 */
void mui_fetch_send_header(struct fetch_info *fetch, const char *header_data, size_t len)
{
    fetch_msg msg;
    
    if (!fetch || !header_data) {
        return;
    }
    
    msg.type = FETCH_HEADER;
    msg.data.header_or_data.buf = (const uint8_t *)header_data;
    msg.data.header_or_data.len = len;
    mui_fetch_send_callback(&msg, fetch);
}

/**
 * Send data for a fetch.
 */
void mui_fetch_send_data(struct fetch_info *fetch, const uint8_t *data, size_t len)
{
    fetch_msg msg;
    
    if (!fetch || !data) {
        return;
    }
    
    msg.type = FETCH_DATA;
    msg.data.header_or_data.buf = data;
    msg.data.header_or_data.len = len;
    mui_fetch_send_callback(&msg, fetch);
}

/**
 * Set HTTP response code for a fetch.
 */
void mui_fetch_set_http_code(struct fetch_info *fetch, long http_code)
{
    if (fetch) {
        fetch->httpcode = (int)http_code;
        fetch_set_http_code(fetch->fetch_handle, http_code);
    }
}