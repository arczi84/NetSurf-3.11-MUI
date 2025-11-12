/*
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
 * Copyright 2010 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2010-2020 Vincent Sanders <vince@netsurf-browser.org>
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
/**
 * \file
 *
 * Browser window creation and manipulation implementation.
 */
#include "utils/config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <nsutils/time.h>
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/corestrings.h"
#include "utils/messages.h"
#include "utils/nsoption.h"
#include "netsurf/types.h"
#include "netsurf/browser_window.h"
#include "netsurf/window.h"
#include "netsurf/misc.h"
#include "netsurf/content.h"
#include "netsurf/search.h"
#include "netsurf/plotters.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "content/content_debug.h"
#include "html/html.h"
#include "html/form_internal.h"
#include "javascript/js.h"
#include "desktop/browser_private.h"
#include "desktop/scrollbar.h"
#include "desktop/gui_internal.h"
#include "desktop/download.h"
#include "desktop/frames.h"
#include "desktop/global_history.h"
#include "desktop/textinput.h"
#include "desktop/hotlist.h"
#include "desktop/knockout.h"
#include "desktop/browser_history.h"
#include "desktop/theme.h"
#ifdef WITH_THEME_INSTALL
#include "desktop/theme.h"
#endif
/**
 * smallest scale that can be applied to a browser window
 */
#define SCALE_MINIMUM 0.2
/**
 * largest scale that can be applied to a browser window
 */
#define SCALE_MAXIMUM 10.0
/**
 * maximum frame depth
 */
#define FRAME_DEPTH 8
/* Forward declare internal navigation function */
static nserror browser_window__navigate_internal(
    struct browser_window *bw, struct browser_fetch_parameters *params);

/**
 * Close and destroy all child browser window.
 *
 * \param bw browser window
 */
static void browser_window_destroy_children(struct browser_window *bw)
{
    int i;
    LOG(("browser_window_destroy_children: bw = %p", bw));
    if (bw->children) {
        for (i = 0; i < (bw->rows * bw->cols); i++) {
            browser_window_destroy_internal(&bw->children[i]);
        }
        free(bw->children);
        bw->children = NULL;
        bw->rows = 0;
        bw->cols = 0;
    }
}

/**
 * Free the stored fetch parameters
 *
 * \param bw The browser window
 */
static void
browser_window__free_fetch_parameters(struct browser_fetch_parameters *params)
{
    LOG(("browser_window__free_fetch_parameters: params = %p", params));
    if (params->url != NULL) {
        nsurl_unref(params->url);
        params->url = NULL;
    }
    if (params->referrer != NULL) {
        nsurl_unref(params->referrer);
        params->referrer = NULL;
    }
    if (params->post_urlenc != NULL) {
        free(params->post_urlenc);
        params->post_urlenc = NULL;
    }
    if (params->post_multipart != NULL) {
        fetch_multipart_data_destroy(params->post_multipart);
        params->post_multipart = NULL;
    }
    if (params->parent_charset != NULL) {
        free(params->parent_charset);
        params->parent_charset = NULL;
    }
}

/**
 * Get position of scrollbar widget within browser window.
 *
 * \param bw The browser window
 * \param horizontal Whether to get position of horizontal scrollbar
 * \param x Updated to x-coord of top left of scrollbar widget
 * \param y Updated to y-coord of top left of scrollbar widget
 */
static inline void
browser_window_get_scrollbar_pos(struct browser_window *bw,
                bool horizontal,
                int *x, int *y)
{
    LOG(("browser_window_get_scrollbar_pos: bw = %p, horizontal = %d", bw, horizontal));
    if (horizontal) {
        *x = 0;
        *y = bw->height - SCROLLBAR_WIDTH;
    } else {
        *x = bw->width - SCROLLBAR_WIDTH;
        *y = 0;
    }
}

/**
 * Get browser window horizontal scrollbar widget length
 *
 * \param bw The browser window
 * \return the scrollbar's length
 */
static inline int get_horz_scrollbar_len(struct browser_window *bw)
{
    LOG(("get_horz_scrollbar_len: bw = %p", bw));
    if (bw->scroll_y == NULL) {
        return bw->width;
    }
    return bw->width - SCROLLBAR_WIDTH;
}

/**
 * Get browser window vertical scrollbar widget length
 *
 * \param bw The browser window
 * \return the scrollbar's length
 */
static inline int get_vert_scrollbar_len(struct browser_window *bw)
{
    LOG(("get_vert_scrollbar_len: bw = %p", bw));
    return bw->height;
}

/**
 * Set or remove a selection.
 *
 * \param bw browser window with selection
 * \param selection true if bw has a selection, false if removing selection
 * \param read_only true iff selection is read only (e.g. can't cut it)
 */
static void
browser_window_set_selection(struct browser_window *bw,
                bool selection,
                bool read_only)
{
    struct browser_window *top;
    LOG(("browser_window_set_selection: bw = %p, selection = %d, read_only = %d", bw, selection, read_only));
    assert(bw != NULL);
    top = browser_window_get_root(bw);
    assert(top != NULL);
    if (bw != top->selection.bw &&
        top->selection.bw != NULL &&
        top->selection.bw->current_content != NULL) {
        /* clear old selection */
        content_clear_selection(top->selection.bw->current_content);
    }
    if (selection) {
        top->selection.bw = bw;
    } else {
        top->selection.bw = NULL;
    }
    top->selection.read_only = read_only;
}

/**
 * Set the scroll position of a browser window.
 *
 * scrolls the viewport to ensure the specified rectangle of the
 *   content is shown.
 *
 * \param bw window to scroll
 * \param rect The rectangle to ensure is shown.
 * \return NSERROR_OK on success or apropriate error code.
 */
static nserror
browser_window_set_scroll(struct browser_window *bw, const struct rect *rect)
{
    LOG(("browser_window_set_scroll: bw = %p, rect = (%d, %d, %d, %d)", bw, rect->x0, rect->y0, rect->x1, rect->y1));
    if (bw->window != NULL) {
        return guit->window->set_scroll(bw->window, rect);
    }
    if (bw->scroll_x != NULL) {
        scrollbar_set(bw->scroll_x, rect->x0, false);
    }
    if (bw->scroll_y != NULL) {
        scrollbar_set(bw->scroll_y, rect->y0, false);
    }
    return NSERROR_OK;
}

/**
 * Internal helper for getting the positional features
 *
 * \param[in] bw browser window to examine.
 * \param[in] x x-coordinate of point of interest
 * \param[in] y y-coordinate of point of interest
 * \param[out] data Feature structure to update.
 * \return NSERROR_OK or appropriate error code on faliure.
 */
static nserror
browser_window__get_contextual_content(struct browser_window *bw,
                      int x, int y,
                      struct browser_window_features *data)
{
    nserror ret = NSERROR_OK;
    LOG(("browser_window__get_contextual_content: bw = %p, x = %d, y = %d", bw, x, y));
    /* Handle (i)frame scroll offset (core-managed browser windows only) */
    x += scrollbar_get_offset(bw->scroll_x);
    y += scrollbar_get_offset(bw->scroll_y);
    if (bw->children) {
        /* Browser window has children, so pass request on to
         * appropriate child.
         */
        struct browser_window *bwc;
        int cur_child;
        int children = bw->rows * bw->cols;
        /* Loop through all children of bw */
        for (cur_child = 0; cur_child < children; cur_child++) {
            /* Set current child */
            bwc = &bw->children[cur_child];
            /* Skip this frame if (x, y) coord lies outside */
            if ((x < bwc->x) ||
                (bwc->x + bwc->width < x) ||
                (y < bwc->y) ||
                (bwc->y + bwc->height < y)) {
                continue;
            }
            /* Pass request into this child */
            return browser_window__get_contextual_content(bwc,
                         (x - bwc->x), (y - bwc->y), data);
        }
        /* Coordinate not contained by any frame */
    } else if (bw->current_content != NULL) {
        /* Pass request to content */
        ret = content_get_contextual_content(bw->current_content,
                            x, y, data);
        data->main = bw->current_content;
    }
    return ret;
}

/**
 * implements the download operation of a window navigate
 */
static nserror
browser_window_download(struct browser_window *bw,
           nsurl *url,
           nsurl *nsref,
           uint32_t fetch_flags,
           bool fetch_is_post,
           llcache_post_data *post)
{
    llcache_handle *l;
    struct browser_window *root;
    nserror error;
    LOG(("browser_window_download: bw = %p, url = %s", bw, nsurl_access(url)));
    root = browser_window_get_root(bw);
    assert(root != NULL);
    fetch_flags |= LLCACHE_RETRIEVE_FORCE_FETCH;
    fetch_flags |= LLCACHE_RETRIEVE_STREAM_DATA;
    error = llcache_handle_retrieve(url, fetch_flags, nsref,
                   fetch_is_post ? post : NULL,
                   NULL, NULL, &l);
    if (error == NSERROR_NO_FETCH_HANDLER) {
        /* no internal handler for this type, call out to frontend */
        error = guit->misc->launch_url(url);
    } else if (error != NSERROR_OK) {
        NSLOG(netsurf, INFO, "Failed to fetch download: %d", error);
    } else {
        error = download_context_create(l, root->window);
        if (error != NSERROR_OK) {
            NSLOG(netsurf, INFO,
                  "Failed creating download context: %d", error);
            llcache_handle_abort(l);
            llcache_handle_release(l);
        }
    }
    return error;
}

/**
 * recursively check browser windows for activity
 *
 * \param bw browser window to start checking from.
 */
static bool browser_window_check_throbber(struct browser_window *bw)
{
    int children, index;
    LOG(("browser_window_check_throbber: bw = %p", bw));
    if (bw->throbbing)
        return true;
    if (bw->children) {
        children = bw->rows * bw->cols;
        for (index = 0; index < children; index++) {
            if (browser_window_check_throbber(&bw->children[index]))
                return true;
        }
    }
    if (bw->iframes) {
        for (index = 0; index < bw->iframe_count; index++) {
            if (browser_window_check_throbber(&bw->iframes[index]))
                return true;
        }
    }
    return false;
}

/**
 * Start the busy indicator.
 *
 * \param bw browser window
 */
static nserror browser_window_start_throbber(struct browser_window *bw)
{
    LOG(("browser_window_start_throbber: bw = %p", bw));
    bw->throbbing = true;
    while (bw->parent)
        bw = bw->parent;
    return guit->window->event(bw->window, GW_EVENT_START_THROBBER);
}

/**
 * Stop the busy indicator.
 *
 * \param bw browser window
 */
static nserror browser_window_stop_throbber(struct browser_window *bw)
{
    nserror res = NSERROR_OK;
    LOG(("browser_window_stop_throbber: bw = %p", bw));
    bw->throbbing = false;
    while (bw->parent) {
        bw = bw->parent;
    }
    if (!browser_window_check_throbber(bw)) {
        res = guit->window->event(bw->window, GW_EVENT_STOP_THROBBER);
    }
    return res;
}

/**
 * Callback for fetchcache() for browser window favicon fetches.
 *
 * \param c content handle of favicon
 * \param event The event to process
 * \param pw a context containing the browser window
 * \return NSERROR_OK on success else appropriate error code.
 */
static nserror
browser_window_favicon_callback(hlcache_handle *c,
               const hlcache_event *event,
               void *pw)
{
    struct browser_window *bw = pw;
    LOG(("browser_window_favicon_callback: c = %p, event->type = %d", c, event->type));
    switch (event->type) {
    case CONTENT_MSG_DONE:
        if (bw->favicon.current != NULL) {
            content_close(bw->favicon.current);
            hlcache_handle_release(bw->favicon.current);
        }
        bw->favicon.current = c;
        bw->favicon.loading = NULL;
        /* content_get_bitmap on the hlcache_handle should give
         *   the favicon bitmap at this point
         */
        guit->window->set_icon(bw->window, c);
        break;
    case CONTENT_MSG_ERROR:
        /* clean up after ourselves */
        if (c == bw->favicon.loading) {
            bw->favicon.loading = NULL;
        } else if (c == bw->favicon.current) {
            bw->favicon.current = NULL;
        }
        hlcache_handle_release(c);
        if (bw->favicon.failed == false) {
            nsurl *nsref = NULL;
            nsurl *nsurl;
            nserror error;
            bw->favicon.failed = true;
            error = nsurl_create("resource:favicon.ico", &nsurl);
            if (error != NSERROR_OK) {
                NSLOG(netsurf, INFO,
                      "Unable to create default location url");
            } else {
                hlcache_handle_retrieve(nsurl,
                            HLCACHE_RETRIEVE_SNIFF_TYPE,
                            nsref, NULL,
                            browser_window_favicon_callback,
                            bw, NULL, CONTENT_IMAGE,
                            &bw->favicon.loading);
                nsurl_unref(nsurl);
            }
        }
        break;
    default:
        break;
    }
    return NSERROR_OK;
}

/**
 * update the favicon associated with the browser window
 *
 * \param c the page content handle.
 * \param bw A top level browser window.
 * \param link A link context or NULL to attempt fallback scanning.
 */
static nserror
browser_window_update_favicon(hlcache_handle *c,
                 struct browser_window *bw,
                 struct content_rfc5988_link *link)
{
    nsurl *nsref = NULL;
    nsurl *nsurl;
    nserror res;
    LOG(("browser_window_update_favicon: c = %p, bw = %p, link = %p", c, bw, link));
    assert(c != NULL);
    assert(bw !=NULL);
    if (bw->window == NULL) {
        /* Not top-level browser window; not interested */
        return NSERROR_OK;
    }
    /* already fetching the favicon - use that */
    if (bw->favicon.loading != NULL) {
        return NSERROR_OK;
    }
    bw->favicon.failed = false;
    if (link == NULL) {
        /* Look for "icon" */
        link = content_find_rfc5988_link(c, corestring_lwc_icon);
    }
    if (link == NULL) {
        /* Look for "shortcut icon" */
        link = content_find_rfc5988_link(c, corestring_lwc_shortcut_icon);
    }
    if (link == NULL) {
        lwc_string *scheme;
        bool speculative_default = false;
        bool match;
        nsurl = hlcache_handle_get_url(c);
        scheme = nsurl_get_component(nsurl, NSURL_SCHEME);
        /* If the document was fetched over http(s), then speculate
         * that there's a favicon living at /favicon.ico */
        if ((lwc_string_caseless_isequal(scheme,
                        corestring_lwc_http,
                        &match) == lwc_error_ok &&
            match) ||
           (lwc_string_caseless_isequal(scheme,
                        corestring_lwc_https,
                        &match) == lwc_error_ok &&
           match)) {
            speculative_default = true;
        }
        lwc_string_unref(scheme);
        if (speculative_default) {
            /* no favicon via link, try for the default location */
            res = nsurl_join(nsurl, "/favicon.ico", &nsurl);
        } else {
            bw->favicon.failed = true;
            res = nsurl_create("resource:favicon.ico", &nsurl);
        }
        if (res != NSERROR_OK) {
            NSLOG(netsurf, INFO,
                  "Unable to create default location url");
            return res;
        }
    } else {
        nsurl = nsurl_ref(link->href);
    }
    if (link == NULL) {
        NSLOG(netsurf, INFO,
              "fetching general favicon from '%s'",
              nsurl_access(nsurl));
    } else {
        NSLOG(netsurf, INFO,
              "fetching favicon rel:%s '%s'",
              lwc_string_data(link->rel),
              nsurl_access(nsurl));
    }
    res = hlcache_handle_retrieve(nsurl,
                      HLCACHE_RETRIEVE_SNIFF_TYPE,
                      nsref,
                      NULL,
                      browser_window_favicon_callback,
                      bw,
                      NULL,
                      CONTENT_IMAGE,
                      &bw->favicon.loading);
    nsurl_unref(nsurl);
    return res;
}

/**
 * Handle meta http-equiv refresh time elapsing by loading a new page.
 *
 * \param p browser window to refresh with new page
 */
static void browser_window_refresh(void *p)
{
    struct browser_window *bw = p;
    nsurl *url;
    nsurl *refresh;
    hlcache_handle *parent = NULL;
    enum browser_window_nav_flags flags = BW_NAVIGATE_UNVERIFIABLE;
    LOG(("browser_window_refresh: bw = %p", bw));
    LOG(("browser_window_refresh: current_content = %p", bw ? bw->current_content : NULL));
    assert(bw->current_content != NULL &&
           (content_get_status(bw->current_content) ==
        CONTENT_STATUS_READY ||
        content_get_status(bw->current_content) ==
        CONTENT_STATUS_DONE));
    /* Ignore if the refresh URL has gone
     * (may happen if a fetch error occurred) */
    refresh = content_get_refresh_url(bw->current_content);
    if (refresh == NULL)
        return;
    /* mark this content as invalid so it gets flushed from the cache */
    content_invalidate_reuse_data(bw->current_content);
    url = hlcache_handle_get_url(bw->current_content);
    if ((url == NULL) || (nsurl_compare(url, refresh, NSURL_COMPLETE))) {
        flags |= BW_NAVIGATE_HISTORY;
    }
    /* Treat an (almost) immediate refresh in a top-level browser window as
     * if it were an HTTP redirect, and thus make the resulting fetch
     * verifiable.
     *
     * See fetchcache.c for why redirected fetches should be verifiable at
     * all.
     */
    if (bw->refresh_interval <= 100 && bw->parent == NULL) {
        flags &= ~BW_NAVIGATE_UNVERIFIABLE;
    } else {
        parent = bw->current_content;
    }
    browser_window_navigate(bw,
                refresh,
                url,
                flags,
                NULL,
                NULL,
                parent);
}

/**
 * Transfer the loading_content to a new download window.
 */
static void
browser_window_convert_to_download(struct browser_window *bw,
                  llcache_handle *stream)
{
    struct browser_window *root = browser_window_get_root(bw);
    nserror error;
    LOG(("browser_window_convert_to_download: bw = %p, stream = %p", bw, stream));
    assert(root != NULL);
    error = download_context_create(stream, root->window);
    if (error != NSERROR_OK) {
        llcache_handle_abort(stream);
        llcache_handle_release(stream);
    }
    /* remove content from browser window */
    hlcache_handle_release(bw->loading_content);
    bw->loading_content = NULL;
    browser_window_stop_throbber(bw);
}

/**
 * scroll to a fragment if present
 *
 * \param bw browser window
 * \return true if the scroll was sucessful
 */
static bool frag_scroll(struct browser_window *bw)
{
    struct rect rect;
    LOG(("frag_scroll: bw = %p, frag_id = %s", bw, bw->frag_id ? lwc_string_data(bw->frag_id) : "NULL"));
    if (bw->frag_id == NULL) {
        return false;
    }
    if (!html_get_id_offset(bw->current_content,
                bw->frag_id,
                &rect.x0,
                &rect.y0)) {
        return false;
    }
    rect.x1 = rect.x0;
    rect.y1 = rect.y0;
    if (browser_window_set_scroll(bw, &rect) == NSERROR_OK) {
        if (bw->current_content != NULL &&
            bw->history != NULL &&
            bw->history->current != NULL) {
            browser_window_history_update(bw, bw->current_content);
        }
        return true;
    }
    return false;
}

/**
 * Redraw browser window, set extent to content, and update title.
 *
 * \param  bw         browser_window
 * \param  scroll_to_top  move view to top of page
 */
static void browser_window_update(struct browser_window *bw, bool scroll_to_top)
{
    LOG(("browser_window_update: bw = %p, scroll_to_top = %d", bw, scroll_to_top));
    static const struct rect zrect = {
        .x0 = 0,
        .y0 = 0,
        .x1 = 0,
        .y1 = 0
    };
    if (bw->current_content == NULL) {
        return;
    }
    switch (bw->browser_window_type) {
    case BROWSER_WINDOW_NORMAL:
        /* Root browser window, constituting a front end window/tab */
        guit->window->set_title(bw->window,
                    content_get_title(bw->current_content));
        browser_window_update_extent(bw);
        /* if frag_id exists, then try to scroll to it */
        /** @todo don't do this if the user has scrolled */
        if (!frag_scroll(bw)) {
            if (scroll_to_top) {
                browser_window_set_scroll(bw, &zrect);
            }
        }
        guit->window->invalidate(bw->window, NULL);
        break;
    case BROWSER_WINDOW_IFRAME:
        /* Internal iframe browser window */
        assert(bw->parent != NULL);
        assert(bw->parent->current_content != NULL);
        browser_window_update_extent(bw);
        if (scroll_to_top) {
            browser_window_set_scroll(bw, &zrect);
        }
        /* if frag_id exists, then try to scroll to it */
        /** @todo don't do this if the user has scrolled */
        frag_scroll(bw);
        browser_window_invalidate_iframe(bw);
        break;
    case BROWSER_WINDOW_FRAME:
        {
            struct rect rect;
            browser_window_update_extent(bw);
            if (scroll_to_top) {
                browser_window_set_scroll(bw, &zrect);
            }
            /* if frag_id exists, then try to scroll to it */
            /** @todo don't do this if the user has scrolled */
            frag_scroll(bw);
            rect.x0 = scrollbar_get_offset(bw->scroll_x);
            rect.y0 = scrollbar_get_offset(bw->scroll_y);
            rect.x1 = rect.x0 + bw->width;
            rect.y1 = rect.y0 + bw->height;
            browser_window_invalidate_rect(bw, &rect);
        }
        break;
    default:
    case BROWSER_WINDOW_FRAMESET:
        /* Nothing to do */
        break;
    }
}

/**
 * handle message for content ready on browser window
 */
static nserror browser_window_content_ready(struct browser_window *bw)
{
    int width, height;
    nserror res = NSERROR_OK;
    LOG(("browser_window_content_ready: bw = %p", bw));
    /* close and release the current window content */
    if (bw->current_content != NULL) {
        content_close(bw->current_content);
        hlcache_handle_release(bw->current_content);
    }
    bw->current_content = bw->loading_content;
    bw->loading_content = NULL;
    if (!bw->internal_nav) {
        /* Transfer the fetch parameters */
        browser_window__free_fetch_parameters(&bw->current_parameters);
        bw->current_parameters = bw->loading_parameters;
        memset(&bw->loading_parameters, 0, sizeof(bw->loading_parameters));
        /* Transfer the certificate chain */
        cert_chain_free(bw->current_cert_chain);
        bw->current_cert_chain = bw->loading_cert_chain;
        bw->loading_cert_chain = NULL;
    }
    /* Format the new content to the correct dimensions */
    browser_window_get_dimensions(bw, &width, &height);
    width /= bw->scale;
    height /= bw->scale;
    content_reformat(bw->current_content, false, width, height);
    /* history */
    if (bw->history_add && bw->history && !bw->internal_nav) {
        nsurl *url = hlcache_handle_get_url(bw->current_content);
        if (urldb_add_url(url)) {
            urldb_set_url_title(url, content_get_title(bw->current_content));
            urldb_update_url_visit_data(url);
            urldb_set_url_content_type(url,
                          content_get_type(bw->current_content));
            /* This is safe as we've just added the URL */
            global_history_add(urldb_get_url(url));
        }
        /**
         * \todo Urldb / Thumbnails / Local history brokenness
         *
         * We add to local history after calling urldb_add_url rather
         *  than in the block above.  If urldb_add_url fails (as it
         *  will for urls like "about:about", "about:config" etc),
         *  there would be no local history node, and later calls to
         *  history_update will either explode or overwrite the node
         *  for the previous URL.
         *
         * We call it after, rather than before urldb_add_url because
         *  history_add calls bitmap render, which tries to register
         *  the thumbnail with urldb.  That thumbnail registration
         *  fails if the url doesn't exist in urldb already, and only
         *  urldb-registered thumbnails get freed.  So if we called
         *  history_add before urldb_add_url we would leak thumbnails
         *  for all newly visited URLs.  With the history_add call
         *  after, we only leak the thumbnails when urldb does not add
         *  the URL.
         *
         * Also, since browser_window_history_add can create a
         *  thumbnail (content_redraw), we need to do it after
         *  content_reformat.
         */
        browser_window_history_add(bw, bw->current_content, bw->frag_id);
    }
    browser_window_remove_caret(bw, false);
    if (bw->window != NULL) {
        guit->window->event(bw->window, GW_EVENT_NEW_CONTENT);
        browser_window_refresh_url_bar(bw);
    }
    /* new content; set scroll_to_top */
    browser_window_update(bw, true);
    content_open(bw->current_content, bw, 0, 0);
    browser_window_set_status(bw, content_get_status_message(bw->current_content));
    /* frames */
    res = browser_window_create_frameset(bw);
    /* iframes */
    res = browser_window_create_iframes(bw);
    /* Indicate page status may have changed */
    if (res == NSERROR_OK) {
        struct browser_window *root = browser_window_get_root(bw);
        res = guit->window->event(root->window, GW_EVENT_PAGE_INFO_CHANGE);
    }
    return res;
}

/**
 * handle message for content done on browser window
 */
static nserror
browser_window_content_done(struct browser_window *bw)
{
    float sx, sy;
    struct rect rect;
    int scrollx;
    int scrolly;
    LOG(("browser_window_content_done: bw = %p", bw));
    if (bw->window == NULL) {
        /* Updated browser window's scrollbars. */
        /**
         * \todo update browser window scrollbars before CONTENT_MSG_DONE
         */
        browser_window_reformat(bw, true, bw->width, bw->height);
        browser_window_handle_scrollbars(bw);
    }
    browser_window_update(bw, false);
    browser_window_set_status(bw, content_get_status_message(bw->current_content));
    browser_window_stop_throbber(bw);
    browser_window_update_favicon(bw->current_content, bw, NULL);
    if (browser_window_history_get_scroll(bw, &sx, &sy) == NSERROR_OK) {
        scrollx = (int)((float)content_get_width(bw->current_content) * sx);
        scrolly = (int)((float)content_get_height(bw->current_content) * sy);
        rect.x0 = rect.x1 = scrollx;
        rect.y0 = rect.y1 = scrolly;
        if (browser_window_set_scroll(bw, &rect) != NSERROR_OK) {
            NSLOG(netsurf, WARNING,
                  "Unable to set browser scroll offsets to %d by %d",
                  scrollx, scrolly);
        }
    }
    if (!bw->internal_nav) {
        browser_window_history_update(bw, bw->current_content);
        hotlist_update_url(hlcache_handle_get_url(bw->current_content));
    }
    LOG(("browser_window_content_done: current_content = %p (%s)", 
         bw->current_content, content_get_type(bw->current_content)));
    if (bw->refresh_interval != -1) {
        guit->misc->schedule(bw->refresh_interval * 10,
                    browser_window_refresh, bw);
    }
    return NSERROR_OK;
}

/**
 * Handle query responses from SSL requests
 */
static nserror
browser_window__handle_ssl_query_response(bool proceed, void *pw)
{
    struct browser_window *bw = (struct browser_window *)pw;
    LOG(("browser_window__handle_ssl_query_response: bw = %p, proceed = %d", bw, proceed));
    /* If we're in the process of loading, stop the load */
    if (bw->loading_content != NULL) {
        /* We had a loading content (maybe auth page?) */
        browser_window_stop(bw);
        browser_window_remove_caret(bw, false);
        browser_window_destroy_children(bw);
        browser_window_destroy_iframes(bw);
    }
    if (!proceed) {
        /* We're processing a "back to safety", do a rough-and-ready
         * nav to the old 'current' parameters, with any post data
         * stripped away
         */
        return browser_window__reload_current_parameters(bw);
    }
    /* We're processing a "proceed" attempt from the form */
    /* First, we permit the SSL */
    urldb_set_cert_permissions(bw->loading_parameters.url, true);
    /* And then we navigate to the original loading parameters */
    bw->internal_nav = false;
    return browser_window__navigate_internal(bw, &bw->loading_parameters);
}

/**
 * Unpack a "username:password" to components.
 *
 * \param[in]  userpass     The input string to split.
 * \param[in]  username_out  Returns username on success.  Owned by caller.
 * \param[out] password_out  Returns password on success.  Owned by caller.
 * \return NSERROR_OK, or appropriate error code.
 */
static nserror
browser_window__unpack_userpass(const char *userpass,
               char **username_out,
               char **password_out)
{
    const char *tmp;
    char *username;
    char *password;
    size_t len;
    LOG(("browser_window__unpack_userpass: userpass = %s", userpass ? userpass : "NULL"));
    if (userpass == NULL) {
        username = malloc(1);
        password = malloc(1);
        if (username == NULL || password == NULL) {
            free(username);
            free(password);
            return NSERROR_NOMEM;
        }
        username[0] = '\0';
        password[0] = '\0';
        *username_out = username;
        *password_out = password;
        return NSERROR_OK;
    }
    tmp = strchr(userpass, ':');
    if (tmp == NULL) {
        return NSERROR_BAD_PARAMETER;
    } else {
        size_t len2;
        len = tmp - userpass;
        len2 = strlen(++tmp);
        username = malloc(len + 1);
        password = malloc(len2 + 1);
        if (username == NULL || password == NULL) {
            free(username);
            free(password);
            return NSERROR_NOMEM;
        }
        memcpy(username, userpass, len);
        username[len] = '\0';
        memcpy(password, tmp, len2 + 1);
    }
    *username_out = username;
    *password_out = password;
    return NSERROR_OK;
}

/**
 * Build a "username:password" from components.
 *
 * \param[in]  username     The username component.
 * \param[in]  password     The password component.
 * \param[out] userpass_out  Returns combined string on success.
 *                           Owned by caller.
 * \return NSERROR_OK, or appropriate error code.
 */
static nserror
browser_window__build_userpass(const char *username,
                  const char *password,
                  char **userpass_out)
{
    char *userpass;
    size_t len;
    LOG(("browser_window__build_userpass: username = %s, password = %s", username, password));
    len = strlen(username) + 1 + strlen(password) + 1;
    userpass = malloc(len);
    if (userpass == NULL) {
        return NSERROR_NOMEM;
    }
    snprintf(userpass, len, "%s:%s", username, password);
    *userpass_out = userpass;
    return NSERROR_OK;
}

/**
 * Handle a response from the UI when prompted for credentials
 */
static nserror
browser_window__handle_userpass_response(nsurl *url,
                    const char *realm,
                    const char *username,
                    const char *password,
                    void *pw)
{
    struct browser_window *bw = (struct browser_window *)pw;
    char *userpass;
    nserror err;
    LOG(("browser_window__handle_userpass_response: url = %s, realm = %s, username = %s", 
         nsurl_access(url), realm, username));
    err = browser_window__build_userpass(username, password, &userpass);
    if (err != NSERROR_OK) {
        return err;
    }
    urldb_set_auth_details(url, realm, userpass);
    free(userpass);
    /**
     * \todo QUERY - Eventually this should fill out the form *NOT* nav
     *               to the original location
     */
    /* Finally navigate to the original loading parameters */
    if (bw->loading_content != NULL) {
        /* We had a loading content (maybe auth page?) */
        browser_window_stop(bw);
        browser_window_remove_caret(bw, false);
        browser_window_destroy_children(bw);
        browser_window_destroy_iframes(bw);
    }
    bw->internal_nav = false;
    return browser_window__navigate_internal(bw, &bw->loading_parameters);
}

/**
 * Handle login request (BAD_AUTH) during fetch
 *
 */
static nserror
browser_window__handle_login(struct browser_window *bw,
                const char *realm,
                nsurl *url) {
    char *username = NULL, *password = NULL;
    nserror err = NSERROR_OK;
    struct browser_fetch_parameters params;
    LOG(("browser_window__handle_login: bw = %p, realm = %s, url = %s", bw, realm, nsurl_access(url)));
    memset(&params, 0, sizeof(params));
    /* Step one, retrieve what we have */
    err = browser_window__unpack_userpass(
                     urldb_get_auth_details(url, realm),
                     &username, &password);
    if (err != NSERROR_OK) {
        goto out;
    }
    /* Step two, construct our fetch parameters */
    params.url = nsurl_ref(corestring_nsurl_about_query_auth);
    params.referrer = nsurl_ref(url);
    params.flags = BW_NAVIGATE_HISTORY | BW_NAVIGATE_NO_TERMINAL_HISTORY_UPDATE | BW_NAVIGATE_INTERNAL;
    err = fetch_multipart_data_new_kv(&params.post_multipart,
                     "siteurl",
                     nsurl_access(url));
    if (err != NSERROR_OK) {
        goto out;
    }
    err = fetch_multipart_data_new_kv(&params.post_multipart,
                     "realm",
                     realm);
    if (err != NSERROR_OK) {
        goto out;
    }
    err = fetch_multipart_data_new_kv(&params.post_multipart,
                     "username",
                     username);
    if (err != NSERROR_OK) {
        goto out;
    }
    err = fetch_multipart_data_new_kv(&params.post_multipart,
                     "password",
                     password);
    if (err != NSERROR_OK) {
        goto out;
    }
    /* Now we issue the fetch */
    bw->internal_nav = true;
    err = browser_window__navigate_internal(bw, &params);
    if (err != NSERROR_OK) {
        goto out;
    }
    err = guit->misc->login(url, realm, username, password,
                browser_window__handle_userpass_response, bw);
    if (err == NSERROR_NOT_IMPLEMENTED) {
        err = NSERROR_OK;
    }
  out:
    if (username != NULL) {
        free(username);
    }
    if (password != NULL) {
        free(password);
    }
    browser_window__free_fetch_parameters(&params);
    return err;
}

/**
 * Handle a certificate verification request (BAD_CERTS) during a fetch
 */
static nserror
browser_window__handle_bad_certs(struct browser_window *bw,
                nsurl *url)
{
    struct browser_fetch_parameters params;
    nserror err;
    /* Initially we don't know WHY the SSL cert was bad */
    const char *reason = messages_get_sslcode(SSL_CERT_ERR_UNKNOWN);
    size_t depth;
    nsurl *chainurl = NULL;
    LOG(("browser_window__handle_bad_certs: bw = %p, url = %s", bw, nsurl_access(url)));
    memset(&params, 0, sizeof(params));
    params.url = nsurl_ref(corestring_nsurl_about_query_ssl);
    params.referrer = nsurl_ref(url);
    params.flags = BW_NAVIGATE_HISTORY | BW_NAVIGATE_NO_TERMINAL_HISTORY_UPDATE | BW_NAVIGATE_INTERNAL;
    err = fetch_multipart_data_new_kv(&params.post_multipart,
                     "siteurl",
                     nsurl_access(url));
    if (err != NSERROR_OK) {
        goto out;
    }
    if (bw->loading_cert_chain != NULL) {
        for (depth = 0; depth < bw->loading_cert_chain->depth; ++depth) {
            size_t idx = bw->loading_cert_chain->depth - (depth + 1);
            ssl_cert_err err = bw->loading_cert_chain->certs[idx].err;
            if (err != SSL_CERT_ERR_OK) {
                reason = messages_get_sslcode(err);
                break;
            }
        }
        err = cert_chain_to_query(bw->loading_cert_chain, &chainurl);
        if (err != NSERROR_OK) {
            goto out;
        }
        err = fetch_multipart_data_new_kv(&params.post_multipart,
                         "chainurl",
                         nsurl_access(chainurl));
        if (err != NSERROR_OK) {
            goto out;
        }
    }
    err = fetch_multipart_data_new_kv(&params.post_multipart,
                     "reason",
                     reason);
    if (err != NSERROR_OK) {
        goto out;
    }
    /* Now we issue the fetch */
    bw->internal_nav = true;
    err = browser_window__navigate_internal(bw, &params);
    if (err != NSERROR_OK) {
        goto out;
    }
  out:
    browser_window__free_fetch_parameters(&params);
    if (chainurl != NULL)
        nsurl_unref(chainurl);
    return err;
}

/**
 * Handle a timeout during a fetch
 */
static nserror
browser_window__handle_timeout(struct browser_window *bw, nsurl *url)
{
    struct browser_fetch_parameters params;
    nserror err;
    LOG(("browser_window__handle_timeout: bw = %p, url = %s", bw, nsurl_access(url)));
    memset(&params, 0, sizeof(params));
    params.url = nsurl_ref(corestring_nsurl_about_query_timeout);
    params.referrer = nsurl_ref(url);
    params.flags = BW_NAVIGATE_HISTORY | BW_NAVIGATE_NO_TERMINAL_HISTORY_UPDATE | BW_NAVIGATE_INTERNAL;
    err = fetch_multipart_data_new_kv(&params.post_multipart,
                     "siteurl",
                     nsurl_access(url));
    if (err != NSERROR_OK) {
        goto out;
    }
    /* Now we issue the fetch */
    bw->internal_nav = true;
    err = browser_window__navigate_internal(bw, &params);
    if (err != NSERROR_OK) {
        goto out;
    }
  out:
    browser_window__free_fetch_parameters(&params);
    return err;
}

/**
 * Handle non specific errors during a fetch
 */
static nserror
browser_window__handle_fetcherror(struct browser_window *bw,
                 const char *reason,
                 nsurl *url)
{
    struct browser_fetch_parameters params;
    nserror err;
    LOG(("browser_window__handle_fetcherror: bw = %p, reason = %s, url = %s", bw, reason, nsurl_access(url)));
    memset(&params, 0, sizeof(params));
    params.url = nsurl_ref(corestring_nsurl_about_query_fetcherror);
    params.referrer = nsurl_ref(url);
    params.flags = BW_NAVIGATE_HISTORY | BW_NAVIGATE_NO_TERMINAL_HISTORY_UPDATE | BW_NAVIGATE_INTERNAL;
    err = fetch_multipart_data_new_kv(&params.post_multipart,
                     "siteurl",
                     nsurl_access(url));
    if (err != NSERROR_OK) {
        goto out;
    }
    err = fetch_multipart_data_new_kv(&params.post_multipart,
                     "reason",
                     reason);
    if (err != NSERROR_OK) {
        goto out;
    }
    /* Now we issue the fetch */
    bw->internal_nav = true;
    err = browser_window__navigate_internal(bw, &params);
    if (err != NSERROR_OK) {
        goto out;
    }
  out:
    browser_window__free_fetch_parameters(&params);
    return err;
}

/**
 * Handle errors during content fetch
 */
static nserror
browser_window__handle_error(struct browser_window *bw,
                hlcache_handle *c,
                const hlcache_event *event)
{
    const char *message = event->data.errordata.errormsg;
    nserror code = event->data.errordata.errorcode;
    nserror res;
    nsurl *url = hlcache_handle_get_url(c);
    /* Unexpected OK? */
    assert(code != NSERROR_OK);
    LOG(("browser_window__handle_error: c = %p, type = %s, code = %d", c, content_get_type(c), code));
    if (message == NULL) {
        message = messages_get_errorcode(code);
    } else {
        message = messages_get(message);
    }
    if (c == bw->loading_content) {
        bw->loading_content = NULL;
    } else if (c == bw->current_content) {
        bw->current_content = NULL;
        browser_window_remove_caret(bw, false);
    }
    hlcache_handle_release(c);
    switch (code) {
    case NSERROR_BAD_AUTH:
        res = browser_window__handle_login(bw, message, url);
        break;
    case NSERROR_BAD_CERTS:
        res = browser_window__handle_bad_certs(bw, url);
        break;
    case NSERROR_TIMEOUT:
        res = browser_window__handle_timeout(bw, url);
        break;
    default:
        res = browser_window__handle_fetcherror(bw, message, url);
        break;
    }
    return res;
}

/**
 * Update URL bar for a given browser window to given URL
 *
 * \param bw    Browser window to update URL bar for.
 * \param url   URL for content displayed by bw including any fragment.
 */
static inline nserror
browser_window_refresh_url_bar_internal(struct browser_window *bw, nsurl *url)
{
    LOG(("browser_window_refresh_url_bar_internal: bw = %p, url = %s", bw, nsurl_access(url)));
    assert(bw);
    assert(url);
    if ((bw->parent != NULL) || (bw->window == NULL)) {
        /* Not root window or no gui window so do not set a URL */
        return NSERROR_OK;
    }
    return guit->window->set_url(bw->window, url);
}

const char *event_type_str[] = {
    [CONTENT_MSG_SSL_CERTS] = "SSL_CERTS",
    [CONTENT_MSG_LOG] = "LOG",
    [CONTENT_MSG_DOWNLOAD] = "DOWNLOAD",
    [CONTENT_MSG_LOADING] = "LOADING",
    [CONTENT_MSG_READY] = "READY",
    [CONTENT_MSG_DONE] = "DONE",
    [CONTENT_MSG_ERROR] = "ERROR",
    [CONTENT_MSG_REDIRECT] = "REDIRECT",
    [CONTENT_MSG_STATUS] = "STATUS",
    [CONTENT_MSG_REFORMAT] = "REFORMAT",
    [CONTENT_MSG_REDRAW] = "REDRAW",
    [CONTENT_MSG_REFRESH] = "REFRESH",
    [CONTENT_MSG_LINK] = "LINK",
    [CONTENT_MSG_GETTHREAD] = "GETTHREAD",
    [CONTENT_MSG_GETDIMS] = "GETDIMS",
    [CONTENT_MSG_SCROLL] = "SCROLL",
    [CONTENT_MSG_DRAGSAVE] = "DRAGSAVE",
    [CONTENT_MSG_SAVELINK] = "SAVELINK",
    [CONTENT_MSG_POINTER] = "POINTER",
    [CONTENT_MSG_DRAG] = "DRAG",
    [CONTENT_MSG_CARET] = "CARET",
    [CONTENT_MSG_SELECTION] = "SELECTION",
    [CONTENT_MSG_SELECTMENU] = "SELECTMENU",
    [CONTENT_MSG_GADGETCLICK] = "GADGETCLICK",
    [CONTENT_MSG_TEXTSEARCH] = "TEXTSEARCH"
};

/**
 * Browser window content event callback handler.
 */
static nserror
browser_window_callback(hlcache_handle *c, const hlcache_event *event, void *pw)
{
    struct browser_window *bw = pw;
    nserror res = NSERROR_OK;
    //LOG(("browser_window_callback: event = %p", event));
    LOG(("browser_window_callback: event->type = %d", event->type));
    LOG(("browser_window_callback: event->type = %d (%s)", event->type,
        event->type < sizeof(event_type_str)/sizeof(char *) && event_type_str[event->type] != NULL ?
        event_type_str[event->type] : "UNKNOWN"));
    switch (event->type) {
    case CONTENT_MSG_SSL_CERTS:
        /* SSL certificate information has arrived, store it */
        cert_chain_free(bw->loading_cert_chain);
        cert_chain_dup(event->data.chain, &bw->loading_cert_chain);
        break;
    case CONTENT_MSG_LOG:
        browser_window_console_log(bw,
                      event->data.log.src,
                      event->data.log.msg,
                      event->data.log.msglen,
                      event->data.log.flags);
        break;
    case CONTENT_MSG_DOWNLOAD:
        assert(bw->loading_content == c);
        browser_window_convert_to_download(bw, event->data.download);
        if (bw->current_content != NULL) {
            browser_window_refresh_url_bar(bw);
        }
        break;
    case CONTENT_MSG_LOADING:
        assert(bw->loading_content == c);
#ifdef WITH_THEME_INSTALL
        if (content_get_type(c) == CONTENT_THEME) {
            theme_install_start(c);
            bw->loading_content = NULL;
            browser_window_stop_throbber(bw);
        } else
#endif
        {
            bw->refresh_interval = -1;
            browser_window_set_status(bw,
                          content_get_status_message(c));
        }
        break;
    case CONTENT_MSG_READY:
        assert(bw->loading_content == c);
        res = browser_window_content_ready(bw);
        break;
    case CONTENT_MSG_DONE:
        assert(bw->current_content == c);
        res = browser_window_content_done(bw);
        break;
    case CONTENT_MSG_ERROR:
        res = browser_window__handle_error(bw, c, event);
        break;
    case CONTENT_MSG_REDIRECT:
        if (urldb_add_url(event->data.redirect.from)) {
            urldb_update_url_visit_data(event->data.redirect.from);
        }
        browser_window_refresh_url_bar_internal(bw, event->data.redirect.to);
        break;
    case CONTENT_MSG_STATUS:
        if (event->data.explicit_status_text == NULL) {
            /* Object content's status text updated */
            const char *status = NULL;
            if (bw->loading_content != NULL) {
                /* Give preference to any loading content */
                status = content_get_status_message(
                            bw->loading_content);
            }
            if (status == NULL) {
                status = content_get_status_message(c);
            }
            if (status != NULL) {
                browser_window_set_status(bw, status);
            }
        } else {
            /* Object content wants to set explicit message */
            browser_window_set_status(bw,
                    event->data.explicit_status_text);
        }
        break;
    case CONTENT_MSG_REFORMAT:
        if (c == bw->current_content) {
            /* recompute frameset */
            browser_window_recalculate_frameset(bw);
            /* recompute iframe positions, sizes and scrollbars */
            browser_window_recalculate_iframes(bw);
        }
        /* Hide any caret, but don't remove it */
        browser_window_remove_caret(bw, true);
        if (!(event->data.background)) {
            /* Reformatted content should be redrawn */
            browser_window_update(bw, false);
        }
        break;
    case CONTENT_MSG_REDRAW:
        {
            struct rect rect = {
                        .x0 = event->data.redraw.x,
                        .y0 = event->data.redraw.y,
                        .x1 = event->data.redraw.x + event->data.redraw.width,
                        .y1 = event->data.redraw.y + event->data.redraw.height
            };
            browser_window_invalidate_rect(bw, &rect);
        }
        break;
    case CONTENT_MSG_REFRESH:
        bw->refresh_interval = event->data.delay * 100;
        break;
    case CONTENT_MSG_LINK: /* content has an rfc5988 link element */
        {
            bool match;
            /* Handle "icon" and "shortcut icon" */
            if ((lwc_string_caseless_isequal(
                            event->data.rfc5988_link->rel,
                            corestring_lwc_icon,
                            &match) == lwc_error_ok && match) ||
                (lwc_string_caseless_isequal(
                            event->data.rfc5988_link->rel,
                            corestring_lwc_shortcut_icon,
                            &match) == lwc_error_ok && match)) {
                /* it's a favicon perhaps start a fetch for it */
                browser_window_update_favicon(c, bw,
                          event->data.rfc5988_link);
            }
        }
        break;
    case CONTENT_MSG_GETTHREAD:
        {
            /* only the content object created by the browser
             * window requires a new javascript thread object
             */
            jsthread *thread;
            assert(bw->loading_content == c);
            if (js_newthread(bw->jsheap,
                    bw,
                    hlcache_handle_get_content(c),
                    &thread) == NSERROR_OK) {
                /* The content which is requesting the thread
                 * is required to keep hold of it and
                 * to destroy it when it is finished with it.
                 */
                *(event->data.jsthread) = thread;
            }
        }
        break;
    case CONTENT_MSG_GETDIMS:
        {
            int width;
            int height;
            browser_window_get_dimensions(bw, &width, &height);
            *(event->data.getdims.viewport_width) = width / bw->scale;
            *(event->data.getdims.viewport_height) = height / bw->scale;
            break;
        }
    case CONTENT_MSG_SCROLL:
        {
            struct rect rect = {
                        .x0 = event->data.scroll.x0,
                        .y0 = event->data.scroll.y0,
            };
            /* Content wants to be scrolled */
            if (bw->current_content != c) {
                break;
            }
            if (event->data.scroll.area) {
                rect.x1 = event->data.scroll.x1;
                rect.y1 = event->data.scroll.y1;
            } else {
                rect.x1 = event->data.scroll.x0;
                rect.y1 = event->data.scroll.y0;
            }
            browser_window_set_scroll(bw, &rect);
            break;
        }
    case CONTENT_MSG_DRAGSAVE:
        {
            /* Content wants drag save of a content */
            struct browser_window *root = browser_window_get_root(bw);
            hlcache_handle *save = event->data.dragsave.content;
            if (save == NULL) {
                save = c;
            }
            switch(event->data.dragsave.type) {
            case CONTENT_SAVE_ORIG:
                guit->window->drag_save_object(root->window,
                                   save,
                                   GUI_SAVE_OBJECT_ORIG);
                break;
            case CONTENT_SAVE_NATIVE:
                guit->window->drag_save_object(root->window,
                                   save,
                                   GUI_SAVE_OBJECT_NATIVE);
                break;
            case CONTENT_SAVE_COMPLETE:
                guit->window->drag_save_object(root->window,
                                   save,
                                   GUI_SAVE_COMPLETE);
                break;
            case CONTENT_SAVE_SOURCE:
                guit->window->drag_save_object(root->window,
                                   save,
                                   GUI_SAVE_SOURCE);
                break;
            }
        }
        break;
    case CONTENT_MSG_SAVELINK:
        {
            /* Content wants a link to be saved */
            struct browser_window *root = browser_window_get_root(bw);
            guit->window->save_link(root->window,
                        event->data.savelink.url,
                        event->data.savelink.title);
        }
        break;
    case CONTENT_MSG_POINTER:
        /* Content wants to have specific mouse pointer */
        browser_window_set_pointer(bw, event->data.pointer);
        break;
    case CONTENT_MSG_DRAG:
        {
            browser_drag_type bdt = DRAGGING_NONE;
            switch (event->data.drag.type) {
            case CONTENT_DRAG_NONE:
                bdt = DRAGGING_NONE;
                break;
            case CONTENT_DRAG_SCROLL:
                bdt = DRAGGING_CONTENT_SCROLLBAR;
                break;
            case CONTENT_DRAG_SELECTION:
                bdt = DRAGGING_SELECTION;
                break;
            }
            browser_window_set_drag_type(bw, bdt, event->data.drag.rect);
        }
        break;
    case CONTENT_MSG_CARET:
        switch (event->data.caret.type) {
        case CONTENT_CARET_REMOVE:
            browser_window_remove_caret(bw, false);
            break;
        case CONTENT_CARET_HIDE:
            browser_window_remove_caret(bw, true);
            break;
        case CONTENT_CARET_SET_POS:
            browser_window_place_caret(bw,
                          event->data.caret.pos.x,
                          event->data.caret.pos.y,
                          event->data.caret.pos.height,
                          event->data.caret.pos.clip);
            break;
        }
        break;
    case CONTENT_MSG_SELECTION:
        browser_window_set_selection(bw,
                        event->data.selection.selection,
                        event->data.selection.read_only);
        break;
    case CONTENT_MSG_SELECTMENU:
        if (event->data.select_menu.gadget->type == GADGET_SELECT) {
            struct browser_window *root =
                browser_window_get_root(bw);
            guit->window->create_form_select_menu(root->window,
                          event->data.select_menu.gadget);
        }
        break;
    case CONTENT_MSG_GADGETCLICK:
        if (event->data.gadget_click.gadget->type == GADGET_FILE) {
            struct browser_window *root =
                browser_window_get_root(bw);
            guit->window->file_gadget_open(root->window, c,
                               event->data.gadget_click.gadget);
        }
        break;
    case CONTENT_MSG_TEXTSEARCH:
        switch (event->data.textsearch.type) {
        case CONTENT_TEXTSEARCH_FIND:
            guit->search->hourglass(event->data.textsearch.state,
                        event->data.textsearch.ctx);
            break;
        case CONTENT_TEXTSEARCH_MATCH:
            guit->search->status(event->data.textsearch.state,
                         event->data.textsearch.ctx);
            break;
        case CONTENT_TEXTSEARCH_BACK:
            guit->search->back_state(event->data.textsearch.state,
                         event->data.textsearch.ctx);
            break;
        case CONTENT_TEXTSEARCH_FORWARD:
            guit->search->forward_state(event->data.textsearch.state,
                           event->data.textsearch.ctx);
            break;
        case CONTENT_TEXTSEARCH_RECENT:
            guit->search->add_recent(event->data.textsearch.string,
                         event->data.textsearch.ctx);
            break;
        }
        break;
    default:
        break;
    }
    return res;
}

/**
 * internal scheduled reformat callback.
 *
 * scheduled reformat callback to allow reformats from unthreaded context.
 *
 * \param vbw The browser window to be reformatted
 */
static void scheduled_reformat(void *vbw)
{
    struct browser_window *bw = vbw;
    int width;
    int height;
    nserror res;
    LOG(("scheduled_reformat: bw = %p", bw));
    res = guit->window->get_dimensions(bw->window, &width, &height);
    if (res == NSERROR_OK) {
        browser_window_reformat(bw, false, width, height);
    }
}

/* exported interface documented in desktop/browser_private.h */
nserror browser_window_destroy_internal(struct browser_window *bw)
{
    assert(bw);
    LOG(("browser_window_destroy_internal: bw = %p", bw));
    browser_window_destroy_children(bw);
    browser_window_destroy_iframes(bw);
    LOG(("browser_window_destroy_internal: children destroyed for bw = %p", bw));
    /* Destroy scrollbars */
    if (bw->scroll_x != NULL) {
        scrollbar_destroy(bw->scroll_x);
        LOG(("browser_window_destroy_internal: scroll_x destroyed for bw = %p", bw));
    }
    if (bw->scroll_y != NULL) {
        scrollbar_destroy(bw->scroll_y);
        LOG(("browser_window_destroy_internal: scroll_y destroyed for bw = %p", bw));
    }
    /* clear any pending callbacks */
    LOG(("browser_window_destroy_internal: Clearing refresh callbacks for bw = %p", bw));
    guit->misc->schedule(-1, browser_window_refresh, bw);
    NSLOG(netsurf, INFO,
          "Clearing reformat schedule for browser window %p", bw);
    guit->misc->schedule(-1, scheduled_reformat, bw);
    /* If this brower window is not the root window, and has focus, unset
     * the root browser window's focus pointer. */
    if (!bw->window) {
        struct browser_window *top = browser_window_get_root(bw);
        if (top->focus == bw)
            top->focus = top;
        if (top->selection.bw == bw) {
            browser_window_set_selection(top, false, false);
        }
    }
    /* Destruction order is important: we must ensure that the frontend
     * destroys any window(s) associated with this browser window before
     * we attempt any destructive cleanup.
     */
    if (bw->window) {
        /* Only the root window has a GUI window */
        guit->window->destroy(bw->window);
    }
    if (bw->loading_content != NULL) {
        hlcache_handle_abort(bw->loading_content);
        hlcache_handle_release(bw->loading_content);
        bw->loading_content = NULL;
    }
    if (bw->current_content != NULL) {
        content_close(bw->current_content);
        hlcache_handle_release(bw->current_content);
        bw->current_content = NULL;
    }
    if (bw->favicon.loading != NULL) {
        hlcache_handle_abort(bw->favicon.loading);
        hlcache_handle_release(bw->favicon.loading);
        bw->favicon.loading = NULL;
    }
    if (bw->favicon.current != NULL) {
        content_close(bw->favicon.current);
        hlcache_handle_release(bw->favicon.current);
        bw->favicon.current = NULL;
    }
    if (bw->jsheap != NULL) {
        js_destroyheap(bw->jsheap);
        bw->jsheap = NULL;
    }
    /* These simply free memory, so are safe here */
    if (bw->frag_id != NULL) {
        lwc_string_unref(bw->frag_id);
    }
    browser_window_history_destroy(bw);
    cert_chain_free(bw->current_cert_chain);
    cert_chain_free(bw->loading_cert_chain);
    bw->current_cert_chain = NULL;
    bw->loading_cert_chain = NULL;
    free(bw->name);
    free(bw->status.text);
    bw->status.text = NULL;
    browser_window__free_fetch_parameters(&bw->current_parameters);
    browser_window__free_fetch_parameters(&bw->loading_parameters);
    NSLOG(netsurf, INFO, "Status text cache match:miss %d:%d",
          bw->status.match, bw->status.miss);
    return NSERROR_OK;
}

/**
 * Set browser window scale.
 *
 * \param bw Browser window.
 * \param absolute scale value.
 * \return NSERROR_OK on success else error code
 */
static nserror
browser_window_set_scale_internal(struct browser_window *bw, float scale)
{
    int i;
    nserror res = NSERROR_OK;
    LOG(("browser_window_set_scale_internal: bw = %p, scale = %f", bw, scale));
    /* do not apply tiny changes in scale */
    if (fabs(bw->scale - scale) < 0.0001)
        return res;
    bw->scale = scale;
    if (bw->current_content != NULL) {
        if (content_can_reformat(bw->current_content) == false) {
            browser_window_update(bw, false);
        } else {
            res = browser_window_schedule_reformat(bw);
        }
    }
    /* scale frames */
    for (i = 0; i < (bw->cols * bw->rows); i++) {
        res = browser_window_set_scale_internal(&bw->children[i], scale);
    }
    /* scale iframes */
    for (i = 0; i < bw->iframe_count; i++) {
        res = browser_window_set_scale_internal(&bw->iframes[i], scale);
    }
    return res;
}

/**
 * Find browser window.
 *
 * \param bw Browser window.
 * \param target Name of target.
 * \param depth Depth to scan.
 * \param page The browser window page.
 * \param rdepth The rdepth.
 * \param bw_target the output browser window.
 */
static void
browser_window_find_target_internal(struct browser_window *bw,
                   const char *target,
                   int depth,
                   struct browser_window *page,
                   int *rdepth,
                   struct browser_window **bw_target)
{
    int i;
    LOG(("browser_window_find_target_internal: bw = %p, target = %s, depth = %d", bw, target, depth));
    if ((bw->name) && (!strcasecmp(bw->name, target))) {
        if ((bw == page) || (depth > *rdepth)) {
            *rdepth = depth;
            *bw_target = bw;
        }
    }
    if ((!bw->children) && (!bw->iframes))
        return;
    depth++;
    if (bw->children != NULL) {
        for (i = 0; i < (bw->cols * bw->rows); i++) {
            if ((bw->children[i].name) &&
                (!strcasecmp(bw->children[i].name,
                     target))) {
                if ((page == &bw->children[i]) ||
                    (depth > *rdepth)) {
                    *rdepth = depth;
                    *bw_target = &bw->children[i];
                }
            }
            if (bw->children[i].children)
                browser_window_find_target_internal(
                                &bw->children[i],
                                target, depth, page,
                                rdepth, bw_target);
        }
    }
    if (bw->iframes != NULL) {
        for (i = 0; i < bw->iframe_count; i++) {
            browser_window_find_target_internal(&bw->iframes[i],
                                target,
                                depth,
                                page,
                                rdepth,
                                bw_target);
        }
    }
}

/**
 * Handles the end of a drag operation in a browser window.
 *
 * \param  bw     browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x      coordinate of mouse
 * \param  y      coordinate of mouse
 *
 * \todo Remove this function, once these things are associated with content,
 *       rather than bw.
 */
static void
browser_window_mouse_drag_end(struct browser_window *bw,
                 browser_mouse_state mouse,
                 int x, int y)
{
    int scr_x, scr_y;
    LOG(("browser_window_mouse_drag_end: bw = %p, mouse = %d, x = %d, y = %d", bw, mouse, x, y));
    switch (bw->drag.type) {
    case DRAGGING_SELECTION:
    case DRAGGING_OTHER:
    case DRAGGING_CONTENT_SCROLLBAR:
        /* Drag handled by content handler */
        break;
    case DRAGGING_SCR_X:
        browser_window_get_scrollbar_pos(bw, true, &scr_x, &scr_y);
        scr_x = x - scr_x - scrollbar_get_offset(bw->scroll_x);
        scr_y = y - scr_y - scrollbar_get_offset(bw->scroll_y);
        scrollbar_mouse_drag_end(bw->scroll_x, mouse, scr_x, scr_y);
        bw->drag.type = DRAGGING_NONE;
        break;
    case DRAGGING_SCR_Y:
        browser_window_get_scrollbar_pos(bw, false, &scr_x, &scr_y);
        scr_x = x - scr_x - scrollbar_get_offset(bw->scroll_x);
        scr_y = y - scr_y - scrollbar_get_offset(bw->scroll_y);
        scrollbar_mouse_drag_end(bw->scroll_y, mouse, scr_x, scr_y);
        bw->drag.type = DRAGGING_NONE;
        break;
    default:
        browser_window_set_drag_type(bw, DRAGGING_NONE, NULL);
        break;
    }
}

/**
 * Process mouse click event
 *
 * \param bw The browsing context receiving the event
 * \param mouse The mouse event state
 * \param x The scaled x co-ordinate of the event
 * \param y The scaled y co-ordinate of the event
 */
static void
browser_window_mouse_click_internal(struct browser_window *bw,
                   browser_mouse_state mouse,
                   int x, int y)
{
    hlcache_handle *c = bw->current_content;
    const char *status = NULL;
    browser_pointer_shape pointer = BROWSER_POINTER_DEFAULT;
    LOG(("browser_window_mouse_click_internal: bw = %p, mouse = %d, x = %d, y = %d", bw, mouse, x, y));
    if (bw->children) {
        /* Browser window has children (frames) */
        struct browser_window *child;
        int cur_child;
        int children = bw->rows * bw->cols;
        for (cur_child = 0; cur_child < children; cur_child++) {
            child = &bw->children[cur_child];
            if ((x < child->x) ||
                (y < child->y) ||
                (child->x + child->width < x) ||
                (child->y + child->height < y)) {
                /* Click not in this child */
                continue;
            }
            /* It's this child that contains the click; pass it
             * on to child. */
            browser_window_mouse_click_internal(
                child,
                mouse,
                x - child->x + scrollbar_get_offset(child->scroll_x),
                y - child->y + scrollbar_get_offset(child->scroll_y));
            /* Mouse action was for this child, we're done */
            return;
        }
        return;
    }
    if (!c)
        return;
    if (bw->scroll_x != NULL) {
        int scr_x, scr_y;
        browser_window_get_scrollbar_pos(bw, true, &scr_x, &scr_y);
        scr_x = x - scr_x - scrollbar_get_offset(bw->scroll_x);
        scr_y = y - scr_y - scrollbar_get_offset(bw->scroll_y);
        if (scr_x > 0 && scr_x < get_horz_scrollbar_len(bw) &&
            scr_y > 0 && scr_y < SCROLLBAR_WIDTH) {
            status = scrollbar_mouse_status_to_message(
                       scrollbar_mouse_action(
                           bw->scroll_x, mouse,
                           scr_x, scr_y));
            pointer = BROWSER_POINTER_DEFAULT;
            if (status != NULL)
                browser_window_set_status(bw, status);
            browser_window_set_pointer(bw, pointer);
            return;
        }
    }
    if (bw->scroll_y != NULL) {
        int scr_x, scr_y;
        browser_window_get_scrollbar_pos(bw, false, &scr_x, &scr_y);
        scr_x = x - scr_x - scrollbar_get_offset(bw->scroll_x);
        scr_y = y - scr_y - scrollbar_get_offset(bw->scroll_y);
        if (scr_y > 0 && scr_y < get_vert_scrollbar_len(bw) &&
            scr_x > 0 && scr_x < SCROLLBAR_WIDTH) {
            status = scrollbar_mouse_status_to_message(
                        scrollbar_mouse_action(
                            bw->scroll_y,
                            mouse,
                            scr_x,
                            scr_y));
            pointer = BROWSER_POINTER_DEFAULT;
            if (status != NULL) {
                browser_window_set_status(bw, status);
            }
            browser_window_set_pointer(bw, pointer);
            return;
        }
    }
    switch (content_get_type(c)) {
    case CONTENT_HTML:
    case CONTENT_TEXTPLAIN:
        {
            /* Give bw focus */
            struct browser_window *root_bw = browser_window_get_root(bw);
            if (bw != root_bw->focus) {
                browser_window_remove_caret(bw, false);
                browser_window_set_selection(bw, false, true);
                root_bw->focus = bw;
            }
            /* Pass mouse action to content */
            content_mouse_action(c, bw, mouse, x, y);
        }
        break;
    default:
        if (mouse & BROWSER_MOUSE_MOD_2) {
            if (mouse & BROWSER_MOUSE_DRAG_2) {
                guit->window->drag_save_object(bw->window, c,
                                   GUI_SAVE_OBJECT_NATIVE);
            } else if (mouse & BROWSER_MOUSE_DRAG_1) {
                guit->window->drag_save_object(bw->window, c,
                                   GUI_SAVE_OBJECT_ORIG);
            }
        } else if (mouse & (BROWSER_MOUSE_DRAG_1 |
                    BROWSER_MOUSE_DRAG_2)) {
            browser_window_page_drag_start(bw, x, y);
            browser_window_set_pointer(bw, BROWSER_POINTER_MOVE);
        }
        break;
    }
}


/**
 * Process mouse movement event
 *
 * \param bw The browsing context receiving the event
 * \param mouse The mouse event state
 * \param x The scaled x co-ordinate of the event
 * \param y The scaled y co-ordinate of the event
 */
static void
browser_window_mouse_track_internal(struct browser_window *bw,
    browser_mouse_state mouse,
    int x, int y)
{
    hlcache_handle *c = bw->current_content;
    const char *status = NULL;
    browser_pointer_shape pointer = BROWSER_POINTER_DEFAULT;
    LOG(("browser_window_mouse_track_internal: bw = %p, mouse = %d, x = %d, y = %d", bw, mouse, x, y));
    if (bw->window != NULL && bw->drag.window && bw != bw->drag.window) {
        /* This is the root browser window and there's an active drag
         * in a sub window.
         * Pass the mouse action straight on to that bw. */
        struct browser_window *drag_bw = bw->drag.window;
        int off_x = 0;
        int off_y = 0;
        LOG(("browser_window_mouse_track_internal: Active drag in subwindow, drag_bw = %p", drag_bw));
        browser_window_get_position(drag_bw, true, &off_x, &off_y);
        if (drag_bw->browser_window_type == BROWSER_WINDOW_FRAME) {
            LOG(("browser_window_mouse_track_internal: Passing to FRAME, adjusted x = %d, y = %d", x - off_x, y - off_y));
            browser_window_mouse_track_internal(drag_bw,
                mouse,
                x - off_x,
                y - off_y);
        } else if (drag_bw->browser_window_type == BROWSER_WINDOW_IFRAME) {
            LOG(("browser_window_mouse_track_internal: Passing to IFRAME, adjusted x = %d, y = %d", x - off_x / bw->scale, y - off_y / bw->scale));
            browser_window_mouse_track_internal(drag_bw, mouse,
                x - off_x / bw->scale,
                y - off_y / bw->scale);
        }
        return;
    }
    if (bw->children) {
        /* Browser window has children (frames) */
        struct browser_window *child;
        int cur_child;
        int children = bw->rows * bw->cols;
        LOG(("browser_window_mouse_track_internal: Processing %d child frames", children));
        for (cur_child = 0; cur_child < children; cur_child++) {
            child = &bw->children[cur_child];
            if ((x < child->x) ||
                (y < child->y) ||
                (child->x + child->width < x) ||
                (child->y + child->height < y)) {
                /* Click not in this child */
                continue;
            }
            LOG(("browser_window_mouse_track_internal: Passing to child %d, adjusted x = %d, y = %d", cur_child, x - child->x + scrollbar_get_offset(child->scroll_x), y - child->y + scrollbar_get_offset(child->scroll_y)));
            /* It's this child that contains the mouse; pass
             * mouse action on to child */
            browser_window_mouse_track_internal(
                child,
                mouse,
                x - child->x + scrollbar_get_offset(child->scroll_x),
                y - child->y + scrollbar_get_offset(child->scroll_y));
            /* Mouse action was for this child, we're done */
            return;
        }
        /* Odd if we reached here, but nothing else can use the click
         * when there are children. */
        LOG(("browser_window_mouse_track_internal: No child frame contains the mouse position"));
        return;
    }
    if (c == NULL && bw->drag.type != DRAGGING_FRAME) {
        LOG(("browser_window_mouse_track_internal: No content and not DRAGGING_FRAME, exiting"));
        return;
    }
    if (bw->drag.type != DRAGGING_NONE && !mouse) {
        LOG(("browser_window_mouse_track_internal: Ending drag, type = %d", bw->drag.type));
        browser_window_mouse_drag_end(bw, mouse, x, y);
    }
    /* Browser window's horizontal scrollbar */
    if (bw->scroll_x != NULL && bw->drag.type != DRAGGING_SCR_Y) {
        int scr_x, scr_y;
        browser_window_get_scrollbar_pos(bw, true, &scr_x, &scr_y);
        scr_x = x - scr_x - scrollbar_get_offset(bw->scroll_x);
        scr_y = y - scr_y - scrollbar_get_offset(bw->scroll_y);
        if ((bw->drag.type == DRAGGING_SCR_X) ||
            (scr_x > 0 &&
             scr_x < get_horz_scrollbar_len(bw) &&
             scr_y > 0 &&
             scr_y < SCROLLBAR_WIDTH &&
             bw->drag.type == DRAGGING_NONE)) {
            /* Start a scrollbar drag, or continue existing drag */
            LOG(("browser_window_mouse_track_internal: Handling horizontal scrollbar, scr_x = %d, scr_y = %d", scr_x, scr_y));
            status = scrollbar_mouse_status_to_message(
                scrollbar_mouse_action(bw->scroll_x,
                    mouse,
                    scr_x,
                    scr_y));
            pointer = BROWSER_POINTER_DEFAULT;
            if (status != NULL) {
                browser_window_set_status(bw, status);
                LOG(("browser_window_mouse_track_internal: Set status: %s", status));
            }
            browser_window_set_pointer(bw, pointer);
            return;
        }
    }
    /* Browser window's vertical scrollbar */
    if (bw->scroll_y != NULL) {
        int scr_x, scr_y;
        browser_window_get_scrollbar_pos(bw, false, &scr_x, &scr_y);
        scr_x = x - scr_x - scrollbar_get_offset(bw->scroll_x);
        scr_y = y - scr_y - scrollbar_get_offset(bw->scroll_y);
        if ((bw->drag.type == DRAGGING_SCR_Y) ||
            (scr_y > 0 &&
             scr_y < get_vert_scrollbar_len(bw) &&
             scr_x > 0 &&
             scr_x < SCROLLBAR_WIDTH &&
             bw->drag.type == DRAGGING_NONE)) {
            /* Start a scrollbar drag, or continue existing drag */
            LOG(("browser_window_mouse_track_internal: Handling vertical scrollbar, scr_x = %d, scr_y = %d", scr_x, scr_y));
            status = scrollbar_mouse_status_to_message(
                scrollbar_mouse_action(bw->scroll_y,
                    mouse,
                    scr_x,
                    scr_y));
            pointer = BROWSER_POINTER_DEFAULT;
            if (status != NULL) {
                browser_window_set_status(bw, status);
                LOG(("browser_window_mouse_track_internal: Set status: %s", status));
            }
            browser_window_set_pointer(bw, pointer);
            return;
        }
    }
    if (bw->drag.type == DRAGGING_FRAME) {
        LOG(("browser_window_mouse_track_internal: Resizing frame, new x = %d, y = %d", bw->x + x, bw->y + y));
        browser_window_resize_frame(bw, bw->x + x, bw->y + y);
    } else if (bw->drag.type == DRAGGING_PAGE_SCROLL) {
        /* mouse movement since drag started */
        struct rect rect;
        rect.x0 = bw->drag.start_x - x;
        rect.y0 = bw->drag.start_y - y;
        /* new scroll offsets */
        rect.x0 += bw->drag.start_scroll_x;
        rect.y0 += bw->drag.start_scroll_y;
        bw->drag.start_scroll_x = rect.x1 = rect.x0;
        bw->drag.start_scroll_y = rect.y1 = rect.y0;
        LOG(("browser_window_mouse_track_internal: Page scroll, rect = (%d, %d, %d, %d)", rect.x0, rect.y0, rect.x1, rect.y1));
        browser_window_set_scroll(bw, &rect);
    } else {
        assert(c != NULL);
        LOG(("browser_window_mouse_track_internal: Passing to content_mouse_track, content = %p", c));
        content_mouse_track(c, bw, mouse, x, y);
    }
}

/**
 * perform a scroll operation at a given coordinate
 *
 * \param bw The browsing context receiving the event
 * \param x The scaled x co-ordinate of the event
 * \param y The scaled y co-ordinate of the event
 */
static bool
browser_window_scroll_at_point_internal(struct browser_window *bw,
    int x, int y,
    int scrx, int scry)
{
    bool handled_scroll = false;
    LOG(("browser_window_scroll_at_point_internal: bw = %p, x = %d, y = %d, scrx = %d, scry = %d", bw, x, y, scrx, scry));
    assert(bw != NULL);
    /* Handle (i)frame scroll offset (core-managed browser windows only) */
    x += scrollbar_get_offset(bw->scroll_x);
    y += scrollbar_get_offset(bw->scroll_y);
    LOG(("browser_window_scroll_at_point_internal: Adjusted coordinates x = %d, y = %d", x, y));
    if (bw->children) {
        /* Browser window has children, so pass request on to
         * appropriate child */
        struct browser_window *bwc;
        int cur_child;
        int children = bw->rows * bw->cols;
        LOG(("browser_window_scroll_at_point_internal: Processing %d child frames", children));
        /* Loop through all children of bw */
        for (cur_child = 0; cur_child < children; cur_child++) {
            /* Set current child */
            bwc = &bw->children[cur_child];
            /* Skip this frame if (x, y) coord lies outside */
            if (x < bwc->x || bwc->x + bwc->width < x ||
                y < bwc->y || bwc->y + bwc->height < y)
                continue;
            LOG(("browser_window_scroll_at_point_internal: Passing to child %d, adjusted x = %d, y = %d", cur_child, x - bwc->x, y - bwc->y));
            /* Pass request into this child */
            return browser_window_scroll_at_point_internal(
                bwc,
                (x - bwc->x),
                (y - bwc->y),
                scrx, scry);
        }
    }
    /* Try to scroll any current content */
    if (bw->current_content != NULL &&
        content_scroll_at_point(bw->current_content, x, y, scrx, scry) == true) {
        /* Scroll handled by current content */
        LOG(("browser_window_scroll_at_point_internal: Scroll handled by content %p", bw->current_content));
        return true;
    }
    /* Try to scroll this window, if scroll not already handled */
    if (handled_scroll == false) {
        if (bw->scroll_y && scrollbar_scroll(bw->scroll_y, scry)) {
            handled_scroll = true;
            LOG(("browser_window_scroll_at_point_internal: Vertical scrollbar scrolled, scry = %d", scry));
        }
        if (bw->scroll_x && scrollbar_scroll(bw->scroll_x, scrx)) {
            handled_scroll = true;
            LOG(("browser_window_scroll_at_point_internal: Horizontal scrollbar scrolled, scrx = %d", scrx));
        }
    }
    LOG(("browser_window_scroll_at_point_internal: Scroll handled = %d", handled_scroll));
    return handled_scroll;
}

/**
 * allows a dragged file to be dropped into a browser window at a position
 *
 * \param bw The browsing context receiving the event
 * \param x The scaled x co-ordinate of the event
 * \param y The scaled y co-ordinate of the event
 * \param file filename to be put in the widget
 */
static bool
browser_window_drop_file_at_point_internal(struct browser_window *bw,
    int x, int y,
    char *file)
{
    LOG(("browser_window_drop_file_at_point_internal: bw = %p, x = %d, y = %d, file = %s", bw, x, y, file));
    assert(bw != NULL);
    /* Handle (i)frame scroll offset (core-managed browser windows only) */
    x += scrollbar_get_offset(bw->scroll_x);
    y += scrollbar_get_offset(bw->scroll_y);
    LOG(("browser_window_drop_file_at_point_internal: Adjusted coordinates x = %d, y = %d", x, y));
    if (bw->children) {
        /* Browser window has children, so pass request on to
         * appropriate child */
        struct browser_window *bwc;
        int cur_child;
        int children = bw->rows * bw->cols;
        LOG(("browser_window_drop_file_at_point_internal: Processing %d child frames", children));
        /* Loop through all children of bw */
        for (cur_child = 0; cur_child < children; cur_child++) {
            /* Set current child */
            bwc = &bw->children[cur_child];
            /* Skip this frame if (x, y) coord lies outside */
            if (x < bwc->x || bwc->x + bwc->width < x ||
                y < bwc->y || bwc->y + bwc->height < y)
                continue;
            LOG(("browser_window_drop_file_at_point_internal: Passing to child %d, adjusted x = %d, y = %d", cur_child, x - bwc->x, y - bwc->y));
            /* Pass request into this child */
            return browser_window_drop_file_at_point_internal(
                bwc,
                (x - bwc->x),
                (y - bwc->y),
                file);
        }
    }
    /* Pass file drop on to any content */
    if (bw->current_content != NULL) {
        LOG(("browser_window_drop_file_at_point_internal: Passing file drop to content %p", bw->current_content));
        return content_drop_file_at_point(bw->current_content,
            x, y, file);
    }
    LOG(("browser_window_drop_file_at_point_internal: No content to handle file drop"));
    return false;
}

/**
 * Check if this is an internal navigation URL.
 *
 * This safely checks if the given url is an internal navigation even
 * for urls with no scheme or path.
 *
 * \param url The URL to check
 * \return true if an internal navigation url else false
 */
static bool
is_internal_navigate_url(nsurl *url)
{
    bool is_internal = false;
    lwc_string *scheme, *path;
    LOG(("is_internal_navigate_url: url = %s", nsurl_access(url)));
    scheme = nsurl_get_component(url, NSURL_SCHEME);
    if (scheme != NULL) {
        path = nsurl_get_component(url, NSURL_PATH);
        if (path != NULL) {
            if (scheme == corestring_lwc_about) {
                if (path == corestring_lwc_query_auth) {
                    is_internal = true;
                    LOG(("is_internal_navigate_url: Detected about:query_auth"));
                } else if (path == corestring_lwc_query_ssl) {
                    is_internal = true;
                    LOG(("is_internal_navigate_url: Detected about:query_ssl"));
                } else if (path == corestring_lwc_query_timeout) {
                    is_internal = true;
                    LOG(("is_internal_navigate_url: Detected about:query_timeout"));
                } else if (path == corestring_lwc_query_fetcherror) {
                    is_internal = true;
                    LOG(("is_internal_navigate_url: Detected about:query_fetcherror"));
                }
            }
            lwc_string_unref(path);
        }
        lwc_string_unref(scheme);
    }
    LOG(("is_internal_navigate_url: is_internal = %d", is_internal));
    return is_internal;
}

/* exported interface, documented in netsurf/browser_window.h */
nserror
browser_window_get_name(struct browser_window *bw, const char **out_name)
{
    LOG(("browser_window_get_name: bw = %p", bw));
    assert(bw != NULL);
    *out_name = bw->name;
    LOG(("browser_window_get_name: name = %s", *out_name ? *out_name : "NULL"));
    return NSERROR_OK;
}

/* exported interface, documented in netsurf/browser_window.h */
nserror
browser_window_set_name(struct browser_window *bw, const char *name)
{
    char *nname = NULL;
    LOG(("browser_window_set_name: bw = %p, name = %s", bw, name ? name : "NULL"));
    assert(bw != NULL);
    if (name != NULL) {
        nname = strdup(name);
        if (nname == NULL) {
            LOG(("browser_window_set_name: Failed to allocate memory for name"));
            return NSERROR_NOMEM;
        }
    }
    if (bw->name != NULL) {
        LOG(("browser_window_set_name: Freeing old name %s", bw->name));
        free(bw->name);
    }
    bw->name = nname;
    LOG(("browser_window_set_name: Set name to %s", bw->name ? bw->name : "NULL"));
    return NSERROR_OK;
}

/* exported interface, documented in netsurf/browser_window.h */
bool
browser_window_redraw(struct browser_window *bw,
      int x, int y,
      const struct rect *clip,
      const struct redraw_context *ctx)
{
    struct redraw_context new_ctx = *ctx;
    int width = 0;
    int height = 0;
    bool plot_ok = true;
    content_type content_type;
    struct content_redraw_data data;
    struct rect content_clip;
    nserror res;
    LOG(("browser_window_redraw: bw = %p, x = %d, y = %d, clip = (%d, %d, %d, %d)", bw, x, y, clip->x0, clip->y0, clip->x1, clip->y1));
    if (bw == NULL) {
        NSLOG(netsurf, INFO, "NULL browser window");
        LOG(("browser_window_redraw: NULL browser window"));
        return false;
    }
    x /= bw->scale;
    y /= bw->scale;
    LOG(("browser_window_redraw: Adjusted coordinates x = %d, y = %d", x, y));
    if ((bw->current_content == NULL) &&
        (bw->children == NULL)) {
        /* Browser window has no content, render blank fill */
        LOG(("browser_window_redraw: No content or children, rendering blank fill"));
        ctx->plot->clip(ctx, clip);
        return (ctx->plot->rectangle(ctx, plot_style_fill_white, clip) == NSERROR_OK);
    }
    /* Browser window has content OR children (frames) */
    if ((bw->window != NULL) &&
        (ctx->plot->option_knockout)) {
        /* Root browser window: start knockout */
        LOG(("browser_window_redraw: Starting knockout for root window"));
        knockout_plot_start(ctx, &new_ctx);
    }
    new_ctx.plot->clip(ctx, clip);
    /* Handle redraw of any browser window children */
    if (bw->children) {
        struct browser_window *child;
        int cur_child;
        int children = bw->rows * bw->cols;
        LOG(("browser_window_redraw: Processing %d child frames", children));
        if (bw->window != NULL) {
            /* Root browser window; start with blank fill */
            LOG(("browser_window_redraw: Rendering blank fill for root window"));
            plot_ok &= (new_ctx.plot->rectangle(ctx,
                plot_style_fill_white,
                clip) == NSERROR_OK);
        }
        /* Loop through all children of bw */
        for (cur_child = 0; cur_child < children; cur_child++) {
            /* Set current child */
            child = &bw->children[cur_child];
            /* Get frame edge area in global coordinates */
            content_clip.x0 = (x + child->x) * child->scale;
            content_clip.y0 = (y + child->y) * child->scale;
            content_clip.x1 = content_clip.x0 +
                child->width * child->scale;
            content_clip.y1 = content_clip.y0 +
                child->height * child->scale;
            /* Intersect it with clip rectangle */
            if (content_clip.x0 < clip->x0)
                content_clip.x0 = clip->x0;
            if (content_clip.y0 < clip->y0)
                content_clip.y0 = clip->y0;
            if (clip->x1 < content_clip.x1)
                content_clip.x1 = clip->x1;
            if (clip->y1 < content_clip.y1)
                content_clip.y1 = clip->y1;
            /* Skip this frame if it lies outside clip rectangle */
            if (content_clip.x0 >= content_clip.x1 ||
                content_clip.y0 >= content_clip.y1)
                continue;
            LOG(("browser_window_redraw: Redrawing child %d, clip = (%d, %d, %d, %d)", cur_child, content_clip.x0, content_clip.y0, content_clip.x1, content_clip.y1));
            /* Redraw frame */
            plot_ok &= browser_window_redraw(child,
                x + child->x,
                y + child->y,
                &content_clip,
                &new_ctx);
        }
        /* Nothing else to redraw for browser windows with children;
         * cleanup and return
         */
        if (bw->window != NULL && ctx->plot->option_knockout) {
            /* Root browser window: knockout end */
            LOG(("browser_window_redraw: Ending knockout for root window"));
            knockout_plot_end(ctx);
        }
        LOG(("browser_window_redraw: Returning plot_ok = %d", plot_ok));
        return plot_ok;
    }
    /* Handle browser windows with content to redraw */
    content_type = content_get_type(bw->current_content);
    if (content_type != CONTENT_HTML && content_type != CONTENT_TEXTPLAIN) {
        /* Set render area according to scale */
        width = content_get_width(bw->current_content) * bw->scale;
        height = content_get_height(bw->current_content) * bw->scale;
        /* Non-HTML may not fill viewport to extents, so plot white
         * background fill */
        LOG(("browser_window_redraw: Non-HTML content, rendering white background, width = %d, height = %d", width, height));
        plot_ok &= (new_ctx.plot->rectangle(&new_ctx,
            plot_style_fill_white,
            clip) == NSERROR_OK);
    }
    /* Set up content redraw data */
    data.x = x - scrollbar_get_offset(bw->scroll_x);
    data.y = y - scrollbar_get_offset(bw->scroll_y);
    data.width = width;
    data.height = height;
    data.background_colour = 0xFFFFFF;
    data.scale = bw->scale;
    data.repeat_x = false;
    data.repeat_y = false;
    content_clip = *clip;
    if (!bw->window) {
        int x0 = x * bw->scale;
        int y0 = y * bw->scale;
        int x1 = (x + bw->width - ((bw->scroll_y != NULL) ?
            SCROLLBAR_WIDTH : 0)) * bw->scale;
        int y1 = (y + bw->height - ((bw->scroll_x != NULL) ?
            SCROLLBAR_WIDTH : 0)) * bw->scale;
        if (content_clip.x0 < x0) content_clip.x0 = x0;
        if (content_clip.y0 < y0) content_clip.y0 = y0;
        if (x1 < content_clip.x1) content_clip.x1 = x1;
        if (y1 < content_clip.y1) content_clip.y1 = y1;
        LOG(("browser_window_redraw: Adjusted content clip = (%d, %d, %d, %d)", content_clip.x0, content_clip.y0, content_clip.x1, content_clip.y1));
    }
    /* Render the content */
    LOG(("browser_window_redraw: Rendering content %p", bw->current_content));
    plot_ok &= content_redraw(bw->current_content, &data,
        &content_clip, &new_ctx);
    /* Back to full clip rect */
    new_ctx.plot->clip(&new_ctx, clip);
    if (!bw->window) {
        /* Render scrollbars */
        int off_x, off_y;
        if (bw->scroll_x != NULL) {
            browser_window_get_scrollbar_pos(bw, true,
                &off_x, &off_y);
            LOG(("browser_window_redraw: Redrawing horizontal scrollbar at (%d, %d)", x + off_x, y + off_y));
            res = scrollbar_redraw(bw->scroll_x,
                x + off_x, y + off_y, clip,
                bw->scale, &new_ctx);
            if (res != NSERROR_OK) {
                plot_ok = false;
                LOG(("browser_window_redraw: Horizontal scrollbar redraw failed, res = %d", res));
            }
        }
        if (bw->scroll_y != NULL) {
            browser_window_get_scrollbar_pos(bw, false,
                &off_x, &off_y);
            LOG(("browser_window_redraw: Redrawing vertical scrollbar at (%d, %d)", x + off_x, y + off_y));
            res = scrollbar_redraw(bw->scroll_y,
                x + off_x, y + off_y, clip,
                bw->scale, &new_ctx);
            if (res != NSERROR_OK) {
                plot_ok = false;
                LOG(("browser_window_redraw: Vertical scrollbar redraw failed, res = %d", res));
            }
        }
    }
    if (bw->window != NULL && ctx->plot->option_knockout) {
        /* Root browser window: end knockout */
        LOG(("browser_window_redraw: Ending knockout for root window"));
        knockout_plot_end(ctx);
    }
    LOG(("browser_window_redraw: Returning plot_ok = %d", plot_ok));
    return plot_ok;
}

/* exported interface, documented in netsurf/browser_window.h */
bool browser_window_redraw_ready(struct browser_window *bw)
{
    LOG(("browser_window_redraw_ready: bw = %p", bw));
    if (bw == NULL) {
        NSLOG(netsurf, INFO, "NULL browser window");
        LOG(("browser_window_redraw_ready: NULL browser window"));
        return false;
    } else if (bw->current_content != NULL) {
        /* Can't render locked contents */
        bool locked = content_is_locked(bw->current_content);
        LOG(("browser_window_redraw_ready: Content %p locked = %d", bw->current_content, locked));
        return !locked;
    }
    LOG(("browser_window_redraw_ready: No content, returning true"));
    return true;
}

/* exported interface, documented in browser_private.h */
void browser_window_update_extent(struct browser_window *bw)
{
    LOG(("browser_window_update_extent: bw = %p", bw));
    if (bw->window != NULL) {
        /* Front end window */
        LOG(("browser_window_update_extent: Sending GW_EVENT_UPDATE_EXTENT to front end window"));
        guit->window->event(bw->window, GW_EVENT_UPDATE_EXTENT);
    } else {
        /* Core-managed browser window */
        LOG(("browser_window_update_extent: Handling scrollbars for core-managed window"));
        browser_window_handle_scrollbars(bw);
    }
}

/* exported interface, documented in netsurf/browser_window.h */
void
browser_window_get_position(struct browser_window *bw,
    bool root,
    int *pos_x,
    int *pos_y)
{
    LOG(("browser_window_get_position: bw = %p, root = %d", bw, root));
    *pos_x = 0;
    *pos_y = 0;
    assert(bw != NULL);
    while (bw) {
        switch (bw->browser_window_type) {
        case BROWSER_WINDOW_FRAMESET:
            *pos_x += bw->x * bw->scale;
            *pos_y += bw->y * bw->scale;
            LOG(("browser_window_get_position: FRAMESET, pos_x = %d, pos_y = %d", *pos_x, *pos_y));
            break;
        case BROWSER_WINDOW_NORMAL:
            /* There is no offset to the root browser window */
            LOG(("browser_window_get_position: NORMAL, no offset"));
            break;
        case BROWSER_WINDOW_FRAME:
            /* Iframe and Frame handling is identical;
             * fall though */
        case BROWSER_WINDOW_IFRAME:
            *pos_x += (bw->x - scrollbar_get_offset(bw->scroll_x)) * bw->scale;
            *pos_y += (bw->y - scrollbar_get_offset(bw->scroll_y)) * bw->scale;
            LOG(("browser_window_get_position: FRAME/IFRAME, pos_x = %d, pos_y = %d", *pos_x, *pos_y));
            break;
        }
        bw = bw->parent;
        if (!root) {
            /* return if we just wanted the position in the parent
             * browser window. */
            LOG(("browser_window_get_position: Non-root requested, returning"));
            return;
        }
    }
    LOG(("browser_window_get_position: Final position pos_x = %d, pos_y = %d", *pos_x, *pos_y));
}

/* exported interface, documented in netsurf/browser_window.h */
void browser_window_set_position(struct browser_window *bw, int x, int y)
{
    LOG(("browser_window_set_position: bw = %p, x = %d, y = %d", bw, x, y));
    assert(bw != NULL);
    if (bw->window == NULL) {
        /* Core managed browser window */
        bw->x = x;
        bw->y = y;
        LOG(("browser_window_set_position: Set core-managed window position to x = %d, y = %d", x, y));
    } else {
        NSLOG(netsurf, INFO,
              "Asked to set position of front end window.");
        LOG(("browser_window_set_position: Attempt to set position of front end window"));
        assert(0);
    }
}

/* exported interface, documented in netsurf/browser_window.h */
void
browser_window_set_drag_type(struct browser_window *bw,
     browser_drag_type type,
     const struct rect *rect)
{
    struct browser_window *top_bw = browser_window_get_root(bw);
    gui_drag_type gtype;
    LOG(("browser_window_set_drag_type: bw = %p, type = %d, rect = %p", bw, type, rect));
    bw->drag.type = type;
    if (type == DRAGGING_NONE) {
        top_bw->drag.window = NULL;
        LOG(("browser_window_set_drag_type: Cleared drag, top_bw->drag.window = NULL"));
    } else {
        top_bw->drag.window = bw;
        LOG(("browser_window_set_drag_type: Set top_bw->drag.window = %p", bw));
        switch (type) {
        case DRAGGING_SELECTION:
            /** \todo tell front end */
            LOG(("browser_window_set_drag_type: DRAGGING_SELECTION, TODO: notify front end"));
            return;
        case DRAGGING_SCR_X:
        case DRAGGING_SCR_Y:
        case DRAGGING_CONTENT_SCROLLBAR:
            gtype = GDRAGGING_SCROLLBAR;
            LOG(("browser_window_set_drag_type: Scrollbar drag, gtype = GDRAGGING_SCROLLBAR"));
            break;
        default:
            gtype = GDRAGGING_OTHER;
            LOG(("browser_window_set_drag_type: Other drag, gtype = GDRAGGING_OTHER"));
            break;
        }
        guit->window->drag_start(top_bw->window, gtype, rect);
        LOG(("browser_window_set_drag_type: Started drag with rect = (%d, %d, %d, %d)", rect ? rect->x0 : 0, rect ? rect->y0 : 0, rect ? rect->x1 : 0, rect ? rect->y1 : 0));
    }
}

/* exported interface, documented in netsurf/browser_window.h */
browser_drag_type browser_window_get_drag_type(struct browser_window *bw)
{
    LOG(("browser_window_get_drag_type: bw = %p, returning type = %d", bw, bw->drag.type));
    return bw->drag.type;
}

/* exported interface, documented in netsurf/browser_window.h */
struct browser_window * browser_window_get_root(struct browser_window *bw)
{
    LOG(("browser_window_get_root: bw = %p", bw));
    while (bw && bw->parent) {
        LOG(("browser_window_get_root: Moving to parent %p", bw->parent));
        bw = bw->parent;
    }
    LOG(("browser_window_get_root: Returning root %p", bw));
    return bw;
}

/* exported interface, documented in netsurf/browser_window.h */
browser_editor_flags browser_window_get_editor_flags(struct browser_window *bw)
{
    browser_editor_flags ed_flags = BW_EDITOR_NONE;
    LOG(("browser_window_get_editor_flags: bw = %p", bw));
    assert(bw->window);
    assert(bw->parent == NULL);
    if (bw->selection.bw != NULL) {
        ed_flags |= BW_EDITOR_CAN_COPY;
        if (!bw->selection.read_only)
            ed_flags |= BW_EDITOR_CAN_CUT;
        LOG(("browser_window_get_editor_flags: Selection exists, ed_flags = %d", ed_flags));
    }
    if (bw->can_edit)
        ed_flags |= BW_EDITOR_CAN_PASTE;
    LOG(("browser_window_get_editor_flags: can_edit = %d, final ed_flags = %d", bw->can_edit, ed_flags));
    return ed_flags;
}

/* exported interface, documented in netsurf/browser_window.h */
bool browser_window_can_select(struct browser_window *bw)
{
    LOG(("browser_window_can_select: bw = %p", bw));
    if (bw == NULL || bw->current_content == NULL) {
        LOG(("browser_window_can_select: No browser window or content, returning false"));
        return false;
    }
    /* TODO: We shouldn't have to know about specific content types
     * here. There should be a content_is_selectable() call. */
    if (content_get_type(bw->current_content) != CONTENT_HTML &&
        content_get_type(bw->current_content) != CONTENT_TEXTPLAIN) {
        LOG(("browser_window_can_select: Content type %d not selectable", content_get_type(bw->current_content)));
        return false;
    }
    LOG(("browser_window_can_select: Content is selectable, returning true"));
    return true;
}

/* exported interface, documented in netsurf/browser_window.h */
char * browser_window_get_selection(struct browser_window *bw)
{
    LOG(("browser_window_get_selection: bw = %p", bw));
    assert(bw->window);
    assert(bw->parent == NULL);
    if (bw->selection.bw == NULL ||
        bw->selection.bw->current_content == NULL) {
        LOG(("browser_window_get_selection: No selection or content, returning NULL"));
        return NULL;
    }
    char *selection = content_get_selection(bw->selection.bw->current_content);
    LOG(("browser_window_get_selection: Returning selection %s", selection ? selection : "NULL"));
    return selection;
}

/* exported interface, documented in netsurf/browser_window.h */
bool browser_window_can_search(struct browser_window *bw)
{
    LOG(("browser_window_can_search: bw = %p", bw));
    if (bw == NULL || bw->current_content == NULL) {
        LOG(("browser_window_can_search: No browser window or content, returning false"));
        return false;
    }
    /** \todo We shouldn't have to know about specific content
     * types here. There should be a content_is_searchable() call.
     */
    if ((content_get_type(bw->current_content) != CONTENT_HTML) &&
        (content_get_type(bw->current_content) != CONTENT_TEXTPLAIN)) {
        LOG(("browser_window_can_search: Content type %d not searchable", content_get_type(bw->current_content)));
        return false;
    }
    LOG(("browser_window_can_search: Content is searchable, returning true"));
    return true;
}

/* exported interface, documented in netsurf/browser_window.h */
bool browser_window_is_frameset(struct browser_window *bw)
{
    bool is_frameset = (bw->children != NULL);
    LOG(("browser_window_is_frameset: bw = %p, is_frameset = %d", bw, is_frameset));
    return is_frameset;
}

/* exported interface, documented in netsurf/browser_window.h */
nserror
browser_window_get_scrollbar_type(struct browser_window *bw,
    browser_scrolling *h,
    browser_scrolling *v)
{
    LOG(("browser_window_get_scrollbar_type: bw = %p", bw));
    *h = bw->scrolling;
    *v = bw->scrolling;
    LOG(("browser_window_get_scrollbar_type: h = %d, v = %d", *h, *v));
    return NSERROR_OK;
}

/* exported interface, documented in netsurf/browser_window.h */
nserror
browser_window_get_features(struct browser_window *bw,
    int x, int y,
    struct browser_window_features *data)
{
    LOG(("browser_window_get_features: bw = %p, x = %d, y = %d", bw, x, y));
    /* clear the features structure to empty values */
    data->link = NULL;
    data->object = NULL;
    data->main = NULL;
    data->form_features = CTX_FORM_NONE;
    nserror ret = browser_window__get_contextual_content(bw,
        x / bw->scale,
        y / bw->scale,
        data);
    LOG(("browser_window_get_features: Returned ret = %d, main = %p", ret, data->main));
    return ret;
}

/* exported interface, documented in netsurf/browser_window.h */
bool
browser_window_scroll_at_point(struct browser_window *bw,
    int x, int y,
    int scrx, int scry)
{
    LOG(("browser_window_scroll_at_point: bw = %p, x = %d, y = %d, scrx = %d, scry = %d", bw, x, y, scrx, scry));
    bool result = browser_window_scroll_at_point_internal(bw,
        x / bw->scale,
        y / bw->scale,
        scrx,
        scry);
    LOG(("browser_window_scroll_at_point: Returning result = %d", result));
    return result;
}

/* exported interface, documented in netsurf/browser_window.h */
bool
browser_window_drop_file_at_point(struct browser_window *bw,
    int x, int y,
    char *file)
{
    LOG(("browser_window_drop_file_at_point: bw = %p, x = %d, y = %d, file = %s", bw, x, y, file));
    bool result = browser_window_drop_file_at_point_internal(bw,
        x / bw->scale,
        y / bw->scale,
        file);
    LOG(("browser_window_drop_file_at_point: Returning result = %d", result));
    return result;
}

/* exported interface, documented in netsurf/browser_window.h */
void
browser_window_set_gadget_filename(struct browser_window *bw,
    struct form_control *gadget,
    const char *fn)
{
    LOG(("browser_window_set_gadget_filename: bw = %p, gadget = %p, fn = %s", bw, gadget, fn ? fn : "NULL"));
    html_set_file_gadget_filename(bw->current_content, gadget, fn);
}

/* exported interface, documented in netsurf/browser_window.h */
nserror
browser_window_debug_dump(struct browser_window *bw,
    FILE *f,
    enum content_debug op)
{
    LOG(("browser_window_debug_dump: bw = %p, op = %d", bw, op));
    if (bw->current_content != NULL) {
        nserror ret = content_debug_dump(bw->current_content, f, op);
        LOG(("browser_window_debug_dump: content_debug_dump returned %d", ret));
        return ret;
    }
    LOG(("browser_window_debug_dump: No content, returning NSERROR_OK"));
    return NSERROR_OK;
}

/* exported interface, documented in netsurf/browser_window.h */
nserror
browser_window_debug(struct browser_window *bw, enum content_debug op)
{
    LOG(("browser_window_debug: bw = %p, op = %d", bw, op));
    if (bw->current_content != NULL) {
        nserror ret = content_debug(bw->current_content, op);
        LOG(("browser_window_debug: content_debug returned %d", ret));
        return ret;
    }
    LOG(("browser_window_debug: No content, returning NSERROR_OK"));
    return NSERROR_OK;
}

/* exported interface, documented in netsurf/browser_window.h */
nserror
browser_window_create(enum browser_window_create_flags flags,
    nsurl *url,
    nsurl *referrer,
    struct browser_window *existing,
    struct browser_window **bw)
{
    gui_window_create_flags gw_flags = GW_CREATE_NONE;
    struct browser_window *ret;
    nserror err;
    LOG(("browser_window_create: flags = 0x%x, url = %s, referrer = %s, existing = %p", flags, url ? nsurl_access(url) : "NULL", referrer ? nsurl_access(referrer) : "NULL", existing));
    /* Check parameters */
    if (flags & BW_CREATE_CLONE) {
        if (existing == NULL) {
            LOG(("browser_window_create: No existing window provided for clone"));
            assert(0 && "Failed: No existing window provided.");
            return NSERROR_BAD_PARAMETER;
        }
    }
    if (!(flags & BW_CREATE_HISTORY)) {
        if (!(flags & BW_CREATE_CLONE) || existing == NULL) {
            LOG(("browser_window_create: Must have existing for history"));
            assert(0 && "Failed: Must have existing for history.");
            return NSERROR_BAD_PARAMETER;
        }
    }
    ret = calloc(1, sizeof(struct browser_window));
    if (ret == NULL) {
        LOG(("browser_window_create: Failed to allocate memory for browser window"));
        return NSERROR_NOMEM;
    }
    /* Initialise common parts */
    err = browser_window_initialise_common(flags, ret, existing);
    if (err != NSERROR_OK) {
        LOG(("browser_window_create: browser_window_initialise_common failed, err = %d", err));
        browser_window_destroy(ret);
        return err;
    }
    /* window characteristics */
    ret->browser_window_type = BROWSER_WINDOW_NORMAL;
    ret->scrolling = BW_SCROLLING_YES;
    ret->border = true;
    ret->no_resize = true;
    ret->focus = ret;
    LOG(("browser_window_create: Initialized window characteristics, type = NORMAL, scrolling = YES"));
    /* initialise last action with creation time */
    nsu_getmonotonic_ms(&ret->last_action);
    LOG(("browser_window_create: last_action = %llu", ret->last_action));
    /* The existing gui_window is on the top-level existing
     * browser_window. */
    existing = browser_window_get_root(existing);
    LOG(("browser_window_create: Root existing window = %p", existing));
    /* Set up gui_window creation flags */
    if (flags & BW_CREATE_TAB)
        gw_flags |= GW_CREATE_TAB;
    if (flags & BW_CREATE_CLONE)
        gw_flags |= GW_CREATE_CLONE;
    if (flags & BW_CREATE_FOREGROUND)
        gw_flags |= GW_CREATE_FOREGROUND;
    if (flags & BW_CREATE_FOCUS_LOCATION)
        gw_flags |= GW_CREATE_FOCUS_LOCATION;
    LOG(("browser_window_create: gui_window create flags = 0x%x", gw_flags));
    ret->window = guit->window->create(ret,
        (existing != NULL) ? existing->window : NULL,
        gw_flags);
    if (ret->window == NULL) {
        LOG(("browser_window_create: Failed to create gui window"));
        browser_window_destroy(ret);
        return NSERROR_BAD_PARAMETER;
    }
    if (url != NULL) {
        enum browser_window_nav_flags nav_flags;
        nav_flags = BW_NAVIGATE_NO_TERMINAL_HISTORY_UPDATE;
        if (flags & BW_CREATE_UNVERIFIABLE) {
            nav_flags |= BW_NAVIGATE_UNVERIFIABLE;
        }
        if (flags & BW_CREATE_HISTORY) {
            nav_flags |= BW_NAVIGATE_HISTORY;
        }
        LOG(("browser_window_create: Navigating to %s, nav_flags = 0x%x", nsurl_access(url), nav_flags));
        browser_window_navigate(ret,
            url,
            referrer,
            nav_flags,
            NULL,
            NULL,
            NULL);
    }
    if (bw != NULL) {
        *bw = ret;
        LOG(("browser_window_create: Returning new browser window %p", ret));
    }
    return NSERROR_OK;
}

/* exported internal interface, documented in desktop/browser_private.h */
nserror
browser_window_initialise_common(enum browser_window_create_flags flags,
    struct browser_window *bw,
    const struct browser_window *existing)
{
    nserror err;
    LOG(("browser_window_initialise_common: flags = 0x%x, bw = %p, existing = %p", flags, bw, existing));
    assert(bw);
    /* new javascript context for each window/(i)frame */
    err = js_newheap(nsoption_int(script_timeout), &bw->jsheap);
    if (err != NSERROR_OK) {
        LOG(("browser_window_initialise_common: js_newheap failed, err = %d", err));
        return err;
    }
    if (flags & BW_CREATE_CLONE) {
        assert(existing != NULL);
        /* clone history */
        err = browser_window_history_clone(existing, bw);
        /* copy the scale */
        bw->scale = existing->scale;
        LOG(("browser_window_initialise_common: Cloned history, scale = %f", bw->scale));
    } else {
        /* create history */
        err = browser_window_history_create(bw);
        /* default scale */
        bw->scale = (float) nsoption_int(scale) / 100.0;
        LOG(("browser_window_initialise_common: Created history, default scale = %f", bw->scale));
    }
    if (err != NSERROR_OK) {
        LOG(("browser_window_initialise_common: History operation failed, err = %d", err));
        return err;
    }
    /* window characteristics */
    bw->refresh_interval = -1;
    bw->drag.type = DRAGGING_NONE;
    bw->scroll_x = NULL;
    bw->scroll_y = NULL;
    bw->focus = NULL;
    /* initialise status text cache */
    bw->status.text = NULL;
    bw->status.text_len = 0;
    bw->status.match = 0;
    bw->status.miss = 0;
    LOG(("browser_window_initialise_common: Initialized window characteristics"));
    return NSERROR_OK;
}

/* exported interface, documented in netsurf/browser_window.h */
void browser_window_destroy(struct browser_window *bw)
{
    LOG(("browser_window_destroy: bw = %p", bw));
    /* can't destroy child windows on their own */
    assert(!bw->parent);
    /* destroy */
    browser_window_destroy_internal(bw);
    free(bw);
    LOG(("browser_window_destroy: Freed browser window"));
}

/* exported interface, documented in netsurf/browser_window.h */
nserror browser_window_refresh_url_bar(struct browser_window *bw)
{
    nserror ret;
    nsurl *display_url, *url;
    LOG(("browser_window_refresh_url_bar: bw = %p", bw));
    assert(bw);
    if (bw->parent != NULL) {
        /* Not root window; don't set a URL in GUI URL bar */
        LOG(("browser_window_refresh_url_bar: Not root window, skipping"));
        return NSERROR_OK;
    }
    if (bw->current_content == NULL) {
        /* no content so return about:blank */
        LOG(("browser_window_refresh_url_bar: No content, setting to about:blank"));
        ret = browser_window_refresh_url_bar_internal(bw,
            corestring_nsurl_about_blank);
    } else if (bw->throbbing && bw->loading_parameters.url != NULL) {
        /* Throbbing and we have loading parameters, use those */
        url = bw->loading_parameters.url;
        LOG(("browser_window_refresh_url_bar: Throbbing, using loading URL %s", nsurl_access(url)));
        ret = browser_window_refresh_url_bar_internal(bw, url);
    } else if (bw->frag_id == NULL) {
        if (bw->internal_nav) {
            url = bw->loading_parameters.url;
            LOG(("browser_window_refresh_url_bar: Internal nav, using loading URL %s", url ? nsurl_access(url) : "NULL"));
        } else {
            url = hlcache_handle_get_url(bw->current_content);
            LOG(("browser_window_refresh_url_bar: Using current content URL %s", nsurl_access(url)));
        }
        ret = browser_window_refresh_url_bar_internal(bw, url);
    } else {
        /* Combine URL and Fragment */
        if (bw->internal_nav) {
            url = bw->loading_parameters.url;
            LOG(("browser_window_refresh_url_bar: Internal nav with fragment, using loading URL %s", url ? nsurl_access(url) : "NULL"));
        } else {
            url = hlcache_handle_get_url(bw->current_content);
            LOG(("browser_window_refresh_url_bar: Using current content URL %s with fragment %s", nsurl_access(url), lwc_string_data(bw->frag_id)));
        }
        ret = nsurl_refragment(
            url,
            bw->frag_id, &display_url);
        if (ret == NSERROR_OK) {
            LOG(("browser_window_refresh_url_bar: Refragmented URL to %s", nsurl_access(display_url)));
            ret = browser_window_refresh_url_bar_internal(bw,
                display_url);
            nsurl_unref(display_url);
        }
    }
    LOG(("browser_window_refresh_url_bar: Returning ret = %d", ret));
    return ret;
}

/* exported interface documented in netsurf/browser_window.h */
nserror
browser_window_navigate(struct browser_window *bw,
    nsurl *url,
    nsurl *referrer,
    enum browser_window_nav_flags flags,
    char *post_urlenc,
    struct fetch_multipart_data *post_multipart,
    hlcache_handle *parent)
{
    int depth = 0;
    struct browser_window *cur;
    uint32_t fetch_flags = 0;
    bool fetch_is_post = (post_urlenc != NULL || post_multipart != NULL);
    llcache_post_data post;
    hlcache_child_context child;
    nserror error;
    bool is_internal = false;
    struct browser_fetch_parameters params, *pass_params = NULL;
    LOG(("browser_window_navigate: bw = %p, url = %s, referrer = %s, flags = 0x%x", bw, nsurl_access(url), referrer ? nsurl_access(referrer) : "NULL", flags));
    assert(bw);
    assert(url);
    /*
     * determine if navigation is internal url, if so, we do not
     * do certain things during the load.
     */
    is_internal = is_internal_navigate_url(url);
    LOG(("browser_window_navigate: is_internal = %d", is_internal));
    if (is_internal &&
        !(flags & BW_NAVIGATE_INTERNAL)) {
        /* Internal navigation detected, but flag not set, only allow
         * this is there's a fetch multipart
         */
        if (post_multipart == NULL) {
            LOG(("browser_window_navigate: Internal nav without multipart data, returning NSERROR_NEED_DATA"));
            return NSERROR_NEED_DATA;
        }
        /* It *is* internal, set it as such */
        flags |= BW_NAVIGATE_INTERNAL | BW_NAVIGATE_HISTORY;
        /* If we were previously internal, don't update again */
        if (bw->internal_nav) {
            flags |= BW_NAVIGATE_NO_TERMINAL_HISTORY_UPDATE;
        }
        LOG(("browser_window_navigate: Updated flags = 0x%x", flags));
    }
    /* If we're navigating and we have a history entry and a content
     * then update the history entry before we navigate to save our
     * current state. However since history navigation pre-moves
     * the history state, we ensure that we only do this if we've not
     * been suppressed. In the suppressed case, the history code
     * updates the history itself before navigating.
     */
    if (bw->current_content != NULL &&
        bw->history != NULL &&
        bw->history->current != NULL &&
        !is_internal &&
        !(flags & BW_NAVIGATE_NO_TERMINAL_HISTORY_UPDATE)) {
        LOG(("browser_window_navigate: Updating history before navigation"));
        browser_window_history_update(bw, bw->current_content);
    }
    /* don't allow massively nested framesets */
    for (cur = bw; cur->parent; cur = cur->parent) {
        depth++;
    }
    if (depth > FRAME_DEPTH) {
        NSLOG(netsurf, INFO, "frame depth too high.");
        LOG(("browser_window_navigate: Frame depth %d exceeds limit %d", depth, FRAME_DEPTH));
        return NSERROR_FRAME_DEPTH;
    }
    LOG(("browser_window_navigate: Frame depth = %d", depth));
    /* Set up retrieval parameters */
    if (!(flags & BW_NAVIGATE_UNVERIFIABLE)) {
        fetch_flags |= LLCACHE_RETRIEVE_VERIFIABLE;
        LOG(("browser_window_navigate: Set fetch_flags to VERIFIABLE"));
    }
    if (post_multipart != NULL) {
        post.type = LLCACHE_POST_MULTIPART;
        post.data.multipart = post_multipart;
        LOG(("browser_window_navigate: POST multipart data set"));
    } else if (post_urlenc != NULL) {
        post.type = LLCACHE_POST_URL_ENCODED;
        post.data.urlenc = post_urlenc;
        LOG(("browser_window_navigate: POST url-encoded data set"));
    }
    child.charset = content_get_encoding(parent, CONTENT_ENCODING_NORMAL);
    if ((parent != NULL) && (content_get_type(parent) == CONTENT_HTML)) {
        child.quirks = content_get_quirks(parent);
        LOG(("browser_window_navigate: Parent charset = %s, quirks = %d", child.charset, child.quirks));
    } else {
        child.quirks = false;
        LOG(("browser_window_navigate: No parent or not HTML, quirks = false"));
    }
    url = nsurl_ref(url);
    if (referrer != NULL) {
        referrer = nsurl_ref(referrer);
        LOG(("browser_window_navigate: Referrer URL referenced"));
    }
    /* Get download out of the way */
    if ((flags & BW_NAVIGATE_DOWNLOAD) != 0) {
        LOG(("browser_window_navigate: Handling download navigation"));
        error = browser_window_download(bw,
            url,
            referrer,
            fetch_flags,
            fetch_is_post,
            &post);
        nsurl_unref(url);
        if (referrer != NULL) {
            nsurl_unref(referrer);
        }
        LOG(("browser_window_navigate: Download navigation returned error = %d", error));
        return error;
    }
    if (bw->frag_id != NULL) {
        LOG(("browser_window_navigate: Releasing fragment ID %s", lwc_string_data(bw->frag_id)));
        lwc_string_unref(bw->frag_id);
    }
    bw->frag_id = NULL;
    if (nsurl_has_component(url, NSURL_FRAGMENT)) {
        bool same_url = false;
        bw->frag_id = nsurl_get_component(url, NSURL_FRAGMENT);
        LOG(("browser_window_navigate: Fragment ID set to %s", lwc_string_data(bw->frag_id)));
        /* Compare new URL with existing one (ignoring fragments) */
        if ((bw->current_content != NULL) &&
            (hlcache_handle_get_url(bw->current_content) != NULL)) {
            same_url = nsurl_compare(
                url,
                hlcache_handle_get_url(bw->current_content),
                NSURL_COMPLETE);
            LOG(("browser_window_navigate: Same URL check = %d", same_url));
        }
        /* if we're simply moving to another ID on the same page,
         * don't bother to fetch, just update the window.
         */
        if ((same_url) &&
            (fetch_is_post == false) &&
            (nsurl_has_component(url, NSURL_QUERY) == false)) {
            LOG(("browser_window_navigate: Same page fragment navigation, updating window"));
            nsurl_unref(url);
            if (referrer != NULL) {
                nsurl_unref(referrer);
            }
            if ((flags & BW_NAVIGATE_HISTORY) != 0) {
                LOG(("browser_window_navigate: Adding to history with fragment %s", lwc_string_data(bw->frag_id)));
                browser_window_history_add(bw,
                    bw->current_content,
                    bw->frag_id);
            }
            browser_window_update(bw, false);
            if (bw->current_content != NULL) {
                browser_window_refresh_url_bar(bw);
            }
            return NSERROR_OK;
        }
    }
    LOG(("browser_window_navigate: Stopping current navigation"));
    browser_window_stop(bw);
    browser_window_remove_caret(bw, false);
    browser_window_destroy_children(bw);
    browser_window_destroy_iframes(bw);
    /* Set up the fetch parameters */
    memset(&params, 0, sizeof(params));
    params.url = nsurl_ref(url);
    if (referrer != NULL) {
        params.referrer = nsurl_ref(referrer);
    }
    params.flags = flags;
    if (post_urlenc != NULL) {
        params.post_urlenc = strdup(post_urlenc);
        LOG(("browser_window_navigate: Set post_urlenc"));
    }
    if (post_multipart != NULL) {
        params.post_multipart = fetch_multipart_data_clone(post_multipart);
        LOG(("browser_window_navigate: Cloned post_multipart"));
    }
    if (parent != NULL) {
        params.parent_charset = strdup(child.charset);
        params.parent_quirks = child.quirks;
        LOG(("browser_window_navigate: Set parent charset = %s, quirks = %d", params.parent_charset, params.parent_quirks));
    }
    bw->internal_nav = is_internal;
    LOG(("browser_window_navigate: internal_nav set to %d", bw->internal_nav));
    if (is_internal) {
        pass_params = &params;
    } else {
        /* At this point, we're navigating, so store the fetch parameters */
        browser_window__free_fetch_parameters(&bw->loading_parameters);
        memcpy(&bw->loading_parameters, &params, sizeof(params));
        memset(&params, 0, sizeof(params));
        pass_params = &bw->loading_parameters;
        LOG(("browser_window_navigate: Stored fetch parameters in loading_parameters"));
    }
    error = browser_window__navigate_internal(bw, pass_params);
    nsurl_unref(url);
    if (referrer != NULL) {
        nsurl_unref(referrer);
    }
    if (is_internal) {
        browser_window__free_fetch_parameters(&params);
        LOG(("browser_window_navigate: Freed temporary params for internal navigation"));
    }
    LOG(("browser_window_navigate: Returning error = %d", error));
    return error;
}

/* Internal navigation handler for normal fetches */
static nserror
navigate_internal_real(struct browser_window *bw,
    struct browser_fetch_parameters *params)
{
    uint32_t fetch_flags = 0;
    bool fetch_is_post;
    llcache_post_data post;
    hlcache_child_context child;
    nserror res;
    hlcache_handle *c;
    LOG(("navigate_internal_real: bw = %p, url = %s", bw, nsurl_access(params->url)));
    fetch_is_post = (params->post_urlenc != NULL || params->post_multipart != NULL);
    LOG(("navigate_internal_real: fetch_is_post = %d", fetch_is_post));
    /* Clear SSL info for load */
    cert_chain_free(bw->loading_cert_chain);
    bw->loading_cert_chain = NULL;
    LOG(("navigate_internal_real: Cleared loading_cert_chain"));
    if (!(params->flags & BW_NAVIGATE_UNVERIFIABLE)) {
        fetch_flags |= LLCACHE_RETRIEVE_VERIFIABLE;
        LOG(("navigate_internal_real: Set fetch_flags to VERIFIABLE"));
    }
    if (params->post_multipart != NULL) {
        post.type = LLCACHE_POST_MULTIPART;
        post.data.multipart = params->post_multipart;
        LOG(("navigate_internal_real: Set POST multipart"));
    } else if (params->post_urlenc != NULL) {
        post.type = LLCACHE_POST_URL_ENCODED;
        post.data.urlenc = params->post_urlenc;
        LOG(("navigate_internal_real: Set POST url-encoded"));
    }
    if (params->parent_charset != NULL) {
        child.charset = params->parent_charset;
        child.quirks = params->parent_quirks;
        LOG(("navigate_internal_real: Child context charset = %s, quirks = %d", child.charset, child.quirks));
    }
    bw->history_add = (params->flags & BW_NAVIGATE_HISTORY);
    LOG(("navigate_internal_real: history_add = %d", bw->history_add));
    if (!(params->flags & BW_NAVIGATE_UNVERIFIABLE)) {
        fetch_flags |= HLCACHE_RETRIEVE_MAY_DOWNLOAD;
        LOG(("navigate_internal_real: Added MAY_DOWNLOAD to fetch_flags"));
    }
    res = hlcache_handle_retrieve(params->url,
        fetch_flags | HLCACHE_RETRIEVE_SNIFF_TYPE,
        params->referrer,
        fetch_is_post ? &post : NULL,
        browser_window_callback,
        bw,
        params->parent_charset != NULL ? &child : NULL,
        CONTENT_ANY,
        &c);
    LOG(("navigate_internal_real: hlcache_handle_retrieve returned res = %d, handle = %p", res, c));
    switch (res) {
    case NSERROR_OK:
        bw->loading_content = c;
        browser_window_start_throbber(bw);
        if (bw->window != NULL) {
            guit->window->set_icon(bw->window, NULL);
            LOG(("navigate_internal_real: Cleared window icon"));
        }
        if (!bw->internal_nav) {
            res = browser_window_refresh_url_bar_internal(bw, params->url);
            LOG(("navigate_internal_real: Updated URL bar, res = %d", res));
        }
        break;
    case NSERROR_NO_FETCH_HANDLER:
        NSLOG(netsurf, WARNING, "No fetch handler, attempting to launch external URL.");
        LOG(("navigate_internal_real: No fetch handler, launching external URL"));
        res = guit->misc->launch_url(params->url);
        break;
    default:
        NSLOG(netsurf, ERROR, "Failed to retrieve content, code: %d", res);
        LOG(("navigate_internal_real: Failed to retrieve content, code = %d", res));
        browser_window_set_status(bw, messages_get_errorcode(res));
        break;
    }
    nsu_getmonotonic_ms(&bw->last_action);
    LOG(("navigate_internal_real: Updated last_action = %llu", bw->last_action));
    return res;
}

/* Internal navigation handler for the authentication query handler */
static nserror
navigate_internal_query_auth(struct browser_window *bw,
    struct browser_fetch_parameters *params)
{
    char *userpass = NULL;
    const char *username, *password, *realm, *siteurl;
    nsurl *sitensurl;
    nserror res;
    bool is_login = false, is_cancel = false;
    LOG(("navigate_internal_query_auth: bw = %p", bw));
    assert(params->post_multipart != NULL);
    is_login = fetch_multipart_data_find(params->post_multipart, "login") != NULL;
    is_cancel = fetch_multipart_data_find(params->post_multipart, "cancel") != NULL;
    LOG(("navigate_internal_query_auth: is_login = %d, is_cancel = %d", is_login, is_cancel));
    if (!(is_login || is_cancel)) {
        /* This is a request, so pass it on */
        LOG(("navigate_internal_query_auth: Passing request to navigate_internal_real"));
        return navigate_internal_real(bw, params);
    }
    if (is_cancel) {
        /* We're processing a cancel, do a rough-and-ready nav to
         * about:blank
         */
        LOG(("navigate_internal_query_auth: Processing cancel, navigating to about:blank"));
        browser_window__free_fetch_parameters(&bw->loading_parameters);
        bw->loading_parameters.url = nsurl_ref(corestring_nsurl_about_blank);
        bw->loading_parameters.flags = BW_NAVIGATE_NO_TERMINAL_HISTORY_UPDATE | BW_NAVIGATE_INTERNAL;
        bw->internal_nav = true;
        return browser_window__navigate_internal(bw, &bw->loading_parameters);
    }
    /* We're processing a "login" attempt from the form */
    /* Retrieve the data */
    username = fetch_multipart_data_find(params->post_multipart, "username");
    password = fetch_multipart_data_find(params->post_multipart, "password");
    realm = fetch_multipart_data_find(params->post_multipart, "realm");
    siteurl = fetch_multipart_data_find(params->post_multipart, "siteurl");
    LOG(("navigate_internal_query_auth: username = %s, realm = %s, siteurl = %s", username ? username : "NULL", realm ? realm : "NULL", siteurl ? siteurl : "NULL"));
    if (username == NULL || password == NULL ||
        realm == NULL || siteurl == NULL) {
        /* Bad inputs, simply fail */
        LOG(("navigate_internal_query_auth: Invalid inputs, returning NSERROR_INVALID"));
        return NSERROR_INVALID;
    }
    /* Parse the URL */
    res = nsurl_create(siteurl, &sitensurl);
    if (res != NSERROR_OK) {
        LOG(("navigate_internal_query_auth: Failed to create site URL, res = %d", res));
        return res;
    }
    /* Construct the username/password */
    res = browser_window__build_userpass(username, password, &userpass);
    if (res != NSERROR_OK) {
        nsurl_unref(sitensurl);
        LOG(("navigate_internal_query_auth: Failed to build userpass, res = %d", res));
        return res;
    }
    /* And let urldb know */
    urldb_set_auth_details(sitensurl, realm, userpass);
    LOG(("navigate_internal_query_auth: Set auth details for siteurl %s", nsurl_access(sitensurl)));
    /* Clean up */
    free(userpass);
    nsurl_unref(sitensurl);
    /* Finally navigate to the original loading parameters */
    bw->internal_nav = false;
    LOG(("navigate_internal_query_auth: Navigating to original loading parameters"));
    return navigate_internal_real(bw, &bw->loading_parameters);
}

/* Internal navigation handler for the SSL/privacy query page */
static nserror
navigate_internal_query_ssl(struct browser_window *bw,
    struct browser_fetch_parameters *params)
{
    bool is_proceed = false, is_back = false;
    const char *siteurl = NULL;
    nsurl *siteurl_ns;
    LOG(("navigate_internal_query_ssl: bw = %p", bw));
    assert(params->post_multipart != NULL);
    is_proceed = fetch_multipart_data_find(params->post_multipart, "proceed") != NULL;
    is_back = fetch_multipart_data_find(params->post_multipart, "back") != NULL;
    siteurl = fetch_multipart_data_find(params->post_multipart, "siteurl");
    LOG(("navigate_internal_query_ssl: is_proceed = %d, is_back = %d, siteurl = %s", is_proceed, is_back, siteurl ? siteurl : "NULL"));
    if (!(is_proceed || is_back) || siteurl == NULL) {
        /* This is a request, so pass it on */
        LOG(("navigate_internal_query_ssl: Passing request to navigate_internal_real"));
        return navigate_internal_real(bw, params);
    }
    if (nsurl_create(siteurl, &siteurl_ns) != NSERROR_OK) {
        NSLOG(netsurf, ERROR, "Unable to reset ssl loading parameters");
        LOG(("navigate_internal_query_ssl: Failed to create site URL"));
        return NSERROR_INVALID;
    }
    /* In order that we may proceed, replace the loading parameters */
    nsurl_unref(bw->loading_parameters.url);
    bw->loading_parameters.url = siteurl_ns;
    LOG(("navigate_internal_query_ssl: Replaced loading parameters URL with %s", nsurl_access(siteurl_ns)));
    return browser_window__handle_ssl_query_response(is_proceed, bw);
}

/* Internal navigation handler for the timeout query page */
static nserror
navigate_internal_query_timeout(struct browser_window *bw,
    struct browser_fetch_parameters *params)
{
    bool is_retry = false, is_back = false;
    LOG(("navigate_internal_query_timeout: bw = %p", bw));
    assert(params->post_multipart != NULL);
    is_retry = fetch_multipart_data_find(params->post_multipart, "retry") != NULL;
    is_back = fetch_multipart_data_find(params->post_multipart, "back") != NULL;
    LOG(("navigate_internal_query_timeout: is_retry = %d, is_back = %d", is_retry, is_back));
    if (is_back) {
        /* do a rough-and-ready nav to the old 'current'
         * parameters, with any post data stripped away
         */
        LOG(("navigate_internal_query_timeout: Navigating back to current parameters"));
        return browser_window__reload_current_parameters(bw);
    }
    if (is_retry) {
        /* Finally navigate to the original loading parameters */
        bw->internal_nav = false;
        LOG(("navigate_internal_query_timeout: Retrying with original loading parameters"));
        return navigate_internal_real(bw, &bw->loading_parameters);
    }
    LOG(("navigate_internal_query_timeout: Passing to navigate_internal_real"));
    return navigate_internal_real(bw, params);
}

/* Internal navigation handler for the fetch error query page */
static nserror
navigate_internal_query_fetcherror(struct browser_window *bw,
    struct browser_fetch_parameters *params)
{
    bool is_retry = false, is_back = false;
    LOG(("navigate_internal_query_fetcherror: bw = %p", bw));
    assert(params->post_multipart != NULL);
    is_retry = fetch_multipart_data_find(params->post_multipart, "retry") != NULL;
    is_back = fetch_multipart_data_find(params->post_multipart, "back") != NULL;
    LOG(("navigate_internal_query_fetcherror: is_retry = %d, is_back = %d", is_retry, is_back));
    if (is_back) {
        /* do a rough-and-ready nav to the old 'current'
         * parameters, with any post data stripped away
         */
        LOG(("navigate_internal_query_fetcherror: Navigating back to current parameters"));
        return browser_window__reload_current_parameters(bw);
    }
    if (is_retry) {
        /* Finally navigate to the original loading parameters */
        bw->internal_nav = false;
        LOG(("navigate_internal_query_fetcherror: Retrying with original loading parameters"));
        return navigate_internal_real(bw, &bw->loading_parameters);
    }
    LOG(("navigate_internal_query_fetcherror: Passing to navigate_internal_real"));
    return navigate_internal_real(bw, params);
}

/* dispatch to internal query handlers or normal navigation */
nserror
browser_window__navigate_internal(struct browser_window *bw,
    struct browser_fetch_parameters *params)
{
    lwc_string *scheme, *path;
    LOG(("browser_window__navigate_internal: bw = %p, url = %s", bw, nsurl_access(params->url)));
    /* All our special URIs are in the about: scheme */
    scheme = nsurl_get_component(params->url, NSURL_SCHEME);
    if (scheme != corestring_lwc_about) {
        lwc_string_unref(scheme);
        LOG(("browser_window__navigate_internal: Not an about: scheme, proceeding to normal fetch"));
        goto normal_fetch;
    }
    lwc_string_unref(scheme);
    /* Is it the auth query handler? */
    path = nsurl_get_component(params->url, NSURL_PATH);
    if (path == corestring_lwc_query_auth) {
        lwc_string_unref(path);
        LOG(("browser_window__navigate_internal: Handling query_auth"));
        return navigate_internal_query_auth(bw, params);
    }
    if (path == corestring_lwc_query_ssl) {
        lwc_string_unref(path);
        LOG(("browser_window__navigate_internal: Handling query_ssl"));
        return navigate_internal_query_ssl(bw, params);
    }
    if (path == corestring_lwc_query_timeout) {
        lwc_string_unref(path);
        LOG(("browser_window__navigate_internal: Handling query_timeout"));
        return navigate_internal_query_timeout(bw, params);
    }
    if (path == corestring_lwc_query_fetcherror) {
        lwc_string_unref(path);
        LOG(("browser_window__navigate_internal: Handling query_fetcherror"));
        return navigate_internal_query_fetcherror(bw, params);
    }
    if (path != NULL) {
        lwc_string_unref(path);
    }
    /* Fall through to a normal about: fetch */
normal_fetch:
    LOG(("browser_window__navigate_internal: Proceeding to normal fetch"));
    return navigate_internal_real(bw, params);
}

/* Exported interface, documented in netsurf/browser_window.h */
bool browser_window_up_available(struct browser_window *bw)
{
    bool result = false;
    LOG(("browser_window_up_available: bw = %p", bw));
    if (bw != NULL && bw->current_content != NULL) {
        nsurl *parent;
        nserror err;
        err = nsurl_parent(hlcache_handle_get_url(bw->current_content),
            &parent);
        if (err == NSERROR_OK) {
            result = nsurl_compare(hlcache_handle_get_url(
                bw->current_content),
                parent,
                NSURL_COMPLETE) == false;
            nsurl_unref(parent);
            LOG(("browser_window_up_available: Parent URL comparison result = %d", result));
        }
    }
    LOG(("browser_window_up_available: Returning result = %d", result));
    return result;
}

/* Exported interface, documented in netsurf/browser_window.h */
nserror browser_window_navigate_up(struct browser_window *bw, bool new_window)
{
    nsurl *current, *parent;
    nserror err;
    LOG(("browser_window_navigate_up: bw = %p, new_window = %d", bw, new_window));
    if (bw == NULL) {
        LOG(("browser_window_navigate_up: NULL browser window, returning NSERROR_BAD_PARAMETER"));
        return NSERROR_BAD_PARAMETER;
    }
    current = browser_window_access_url(bw);
    err = nsurl_parent(current, &parent);
    if (err != NSERROR_OK) {
        LOG(("browser_window_navigate_up: nsurl_parent failed, err = %d", err));
        return err;
    }
    if (nsurl_compare(current, parent, NSURL_COMPLETE) == true) {
        /* Can't go up to parent from here */
        nsurl_unref(parent);
        LOG(("browser_window_navigate_up: Same as parent, returning NSERROR_OK"));
        return NSERROR_OK;
    }
    if (new_window) {
        LOG(("browser_window_navigate_up: Creating new window for parent URL %s", nsurl_access(parent)));
        err = browser_window_create(BW_CREATE_CLONE,
            parent, NULL, bw, NULL);
    } else {
        LOG(("browser_window_navigate_up: Navigating to parent URL %s", nsurl_access(parent)));
        err = browser_window_navigate(bw, parent, NULL,
            BW_NAVIGATE_HISTORY,
            NULL, NULL, NULL);
    }
    nsurl_unref(parent);
    LOG(("browser_window_navigate_up: Returning err = %d", err));
    return err;
}


/* Exported interface, documented in netsurf/browser_window.h */
nserror browser_window_navigate_up1(struct browser_window *bw, bool new_window)
{
    nsurl *current, *parent;
    nserror err;
    LOG(("browser_window_navigate_up: bw = %p, new_window = %d", bw, new_window));
    if (bw == NULL) {
        LOG(("browser_window_navigate_up: NULL browser window, returning NSERROR_BAD_PARAMETER"));
        return NSERROR_BAD_PARAMETER;
    }
    current = browser_window_access_url(bw);
    LOG(("browser_window_navigate_up: Current URL = %s", nsurl_access(current)));
    err = nsurl_parent(current, &parent);
    if (err != NSERROR_OK) {
        LOG(("browser_window_navigate_up: nsurl_parent failed, err = %d", err));
        return err;
    }
    if (nsurl_compare(current, parent, NSURL_COMPLETE) == true) {
        /* Can't go up to parent from here */
        LOG(("browser_window_navigate_up: Current URL same as parent, releasing parent URL"));
        nsurl_unref(parent);
        LOG(("browser_window_navigate_up: Returning NSERROR_OK"));
        return NSERROR_OK;
    }
    if (new_window) {
        LOG(("browser_window_navigate_up: Creating new window for parent URL %s", nsurl_access(parent)));
        err = browser_window_create(BW_CREATE_CLONE,
                                   parent, NULL, bw, NULL);
    } else {
        LOG(("browser_window_navigate_up: Navigating to parent URL %s", nsurl_access(parent)));
        err = browser_window_navigate(bw, parent, NULL,
                                     BW_NAVIGATE_HISTORY,
                                     NULL, NULL, NULL);
    }
    LOG(("browser_window_navigate_up: Releasing parent URL"));
    nsurl_unref(parent);
    LOG(("browser_window_navigate_up: Returning err = %d", err));
    return err;
}

/* Exported interface, documented in include/netsurf/browser_window.h */
nsurl* browser_window_access_url(const struct browser_window *bw)
{
    LOG(("browser_window_access_url: bw = %p", bw));
    assert(bw != NULL);
    if (bw->current_content != NULL) {
        nsurl *url = hlcache_handle_get_url(bw->current_content);
        LOG(("browser_window_access_url: Returning current content URL %s", nsurl_access(url)));
        return url;
    } else if (bw->loading_content != NULL) {
        /* TODO: should we return this? */
        nsurl *url = hlcache_handle_get_url(bw->loading_content);
        LOG(("browser_window_access_url: Returning loading content URL %s", nsurl_access(url)));
        return url;
    }
    LOG(("browser_window_access_url: No content, returning about:blank"));
    return corestring_nsurl_about_blank;
}

/* Exported interface, documented in include/netsurf/browser_window.h */
nserror
browser_window_get_url(struct browser_window *bw, bool fragment, nsurl** url_out)
{
    nserror err;
    nsurl *url;
    LOG(("browser_window_get_url: bw = %p, fragment = %d", bw, fragment));
    assert(bw != NULL);
    if (!fragment || bw->frag_id == NULL || bw->loading_content != NULL) {
        /* If there's a loading content, then the bw->frag_id will have
         * been trampled, possibly with a new frag_id, but we will
         * still be returning the current URL, so in this edge case
         * we just drop any fragment. */
        url = nsurl_ref(browser_window_access_url(bw));
        LOG(("browser_window_get_url: No fragment or loading content, URL = %s", nsurl_access(url)));
    } else {
        LOG(("browser_window_get_url: Refragmenting with frag_id = %s", lwc_string_data(bw->frag_id)));
        err = nsurl_refragment(browser_window_access_url(bw),
                              bw->frag_id, &url);
        if (err != NSERROR_OK) {
            LOG(("browser_window_get_url: nsurl_refragment failed, err = %d", err));
            return err;
        }
        LOG(("browser_window_get_url: Refragmented URL = %s", nsurl_access(url)));
    }
    *url_out = url;
    LOG(("browser_window_get_url: Returning URL %s", nsurl_access(url)));
    return NSERROR_OK;
}

/* Exported interface, documented in netsurf/browser_window.h */
const char* browser_window_get_title(struct browser_window *bw)
{
    LOG(("browser_window_get_title: bw = %p", bw));
    assert(bw != NULL);
    if (bw->current_content != NULL) {
        const char *title = content_get_title(bw->current_content);
        LOG(("browser_window_get_title: Returning content title %s", title));
        return title;
    }
    /* no content so return about:blank */
    LOG(("browser_window_get_title: No content, returning about:blank"));
    return nsurl_access(corestring_nsurl_about_blank);
}

/* Exported interface, documented in netsurf/browser_window.h */
struct history * browser_window_get_history(struct browser_window *bw)
{
    LOG(("browser_window_get_history: bw = %p", bw));
    assert(bw != NULL);
    LOG(("browser_window_get_history: Returning history %p", bw->history));
    return bw->history;
}

/* Exported interface, documented in netsurf/browser_window.h */
bool browser_window_has_content(struct browser_window *bw)
{
    LOG(("browser_window_has_content: bw = %p", bw));
    assert(bw != NULL);
    if (bw->current_content == NULL) {
        LOG(("browser_window_has_content: No current content, returning false"));
        return false;
    }
    LOG(("browser_window_has_content: Current content exists, returning true"));
    return true;
}

/* Exported interface, documented in netsurf/browser_window.h */
struct hlcache_handle *browser_window_get_content(struct browser_window *bw)
{
    LOG(("browser_window_get_content: bw = %p", bw));
    LOG(("browser_window_get_content: Returning current_content %p", bw->current_content));
    return bw->current_content;
}

/* Exported interface, documented in netsurf/browser_window.h */
nserror browser_window_get_extents(struct browser_window *bw, bool scaled,
                                  int *width, int *height)
{
    LOG(("browser_window_get_extents: bw = %p, scaled = %d", bw, scaled));
    assert(bw != NULL);
    if (bw->current_content == NULL) {
        *width = 0;
        *height = 0;
        LOG(("browser_window_get_extents: No current content, setting width = 0, height = 0, returning NSERROR_BAD_CONTENT"));
        return NSERROR_BAD_CONTENT;
    }
    *width = content_get_width(bw->current_content);
    *height = content_get_height(bw->current_content);
    LOG(("browser_window_get_extents: Initial width = %d, height = %d", *width, *height));
    if (scaled) {
        *width *= bw->scale;
        *height *= bw->scale;
        LOG(("browser_window_get_extents: Scaled width = %d, height = %d", *width, *height));
    }
    LOG(("browser_window_get_extents: Returning NSERROR_OK"));
    return NSERROR_OK;
}

nserror
browser_window_get_dimensions(struct browser_window *bw,
                  int *width,
                  int *height)
{
    nserror res;
    
    LOG(("browser_window_get_dimensions: Starting function"));
    LOG(("browser_window_get_dimensions: bw=%p, width=%p, height=%p", bw, width, height));
    
    assert(bw);
    
    if (bw->window == NULL) {
        /* Core managed browser window */
        LOG(("browser_window_get_dimensions: Core managed window"));
        *width = bw->width;
        *height = bw->height;
        LOG(("browser_window_get_dimensions: Core dimensions - width=%d, height=%d", *width, *height));
        res = NSERROR_OK;
        LOG(("browser_window_get_dimensions: Core path - returning NSERROR_OK"));
    } else {
        /* Front end window */
        LOG(("browser_window_get_dimensions: Front end window, calling guit->window->get_dimensions"));
        res = guit->window->get_dimensions(bw->window, width, height);
        LOG(("browser_window_get_dimensions: Front end result - res=%d, width=%d, height=%d", 
             res, width ? *width : -1, height ? *height : -1));
    }
    
    LOG(("browser_window_get_dimensions: Returning res=%d", res));
    return res;
}

/* Exported interface, documented in netsurf/browser_window.h */
void
browser_window_set_dimensions(struct browser_window *bw, int width, int height)
{
    LOG(("browser_window_set_dimensions: bw = %p, width = %d, height = %d", bw, width, height));
    assert(bw);
    if (bw->window == NULL) {
        /* Core managed browser window */
        bw->width = width;
        bw->height = height;
        LOG(("browser_window_set_dimensions: Set core-managed window dimensions to width = %d, height = %d", width, height));
    } else {
        NSLOG(netsurf, INFO,
              "Asked to set dimensions of front end window.");
        LOG(("browser_window_set_dimensions: Attempt to set dimensions of front end window"));
        assert(0);
    }
}

/* Exported interface, documented in browser/browser_private.h */
nserror
browser_window_invalidate_rect(struct browser_window *bw, struct rect *rect)
{
    int pos_x;
    int pos_y;
    struct browser_window *top = bw;
    LOG(("browser_window_invalidate_rect: bw = %p, rect = (%d, %d, %d, %d)", bw, rect->x0, rect->y0, rect->x1, rect->y1));
    assert(bw);
    if (bw->window == NULL) {
        /* Core managed browser window */
        browser_window_get_position(bw, true, &pos_x, &pos_y);
        top = browser_window_get_root(bw);
        LOG(("browser_window_invalidate_rect: Core-managed window, pos_x = %d, pos_y = %d, top = %p", pos_x, pos_y, top));
        rect->x0 += pos_x / bw->scale;
        rect->y0 += pos_y / bw->scale;
        rect->x1 += pos_x / bw->scale;
        rect->y1 += pos_y / bw->scale;
        LOG(("browser_window_invalidate_rect: Adjusted rect = (%d, %d, %d, %d)", rect->x0, rect->y0, rect->x1, rect->y1));
    }
    rect->x0 *= top->scale;
    rect->y0 *= top->scale;
    rect->x1 *= top->scale;
    rect->y1 *= top->scale;
    LOG(("browser_window_invalidate_rect: Scaled rect = (%d, %d, %d, %d)", rect->x0, rect->y0, rect->x1, rect->y1));
    nserror ret = guit->window->invalidate(top->window, rect);
    LOG(("browser_window_invalidate_rect: Returning ret = %d", ret));
    return ret;
}

/* Exported interface, documented in netsurf/browser_window.h */
void browser_window_stop(struct browser_window *bw)
{
    int children, index;
    LOG(("browser_window_stop: bw = %p", bw));
    LOG(("browser_window_stop: current_content = %p", bw ? bw->current_content : NULL));
    if (bw->loading_content != NULL) {
        LOG(("browser_window_stop: Aborting and releasing loading_content %p", bw->loading_content));
        hlcache_handle_abort(bw->loading_content);
        hlcache_handle_release(bw->loading_content);
        bw->loading_content = NULL;
    }
    if (bw->current_content != NULL &&
        content_get_status(bw->current_content) != CONTENT_STATUS_DONE) {
        nserror error;
        LOG(("browser_window_stop: Current content %p status = %d, aborting", bw->current_content, content_get_status(bw->current_content)));
        assert(content_get_status(bw->current_content) ==
               CONTENT_STATUS_READY);
        error = hlcache_handle_abort(bw->current_content);
        assert(error == NSERROR_OK);
        LOG(("browser_window_stop: hlcache_handle_abort returned %d", error));
    }
    if (bw->current_content != NULL) {
        LOG(("browser_window_stop: Scheduling refresh for bw = %p", bw));
        guit->misc->schedule(-1, browser_window_refresh, bw);
    }
    if (bw->children) {
        children = bw->rows * bw->cols;
        LOG(("browser_window_stop: Stopping %d child frames", children));
        for (index = 0; index < children; index++)
            browser_window_stop(&bw->children[index]);
    }
    if (bw->iframes) {
        children = bw->iframe_count;
        LOG(("browser_window_stop: Stopping %d iframes", children));
        for (index = 0; index < children; index++)
            browser_window_stop(&bw->iframes[index]);
    }
    if (bw->current_content != NULL) {
        LOG(("browser_window_stop: Refreshing URL bar"));
        browser_window_refresh_url_bar(bw);
    }
    LOG(("browser_window_stop: Stopping throbber"));
    browser_window_stop_throbber(bw);
}

/* Exported interface, documented in netsurf/browser_window.h */
nserror browser_window_reload(struct browser_window *bw, bool all)
{
    hlcache_handle *c;
    unsigned int i;
    struct nsurl *reload_url;
    LOG(("browser_window_reload: bw = %p, all = %d", bw, all));
    if ((bw->current_content) == NULL ||
        (bw->loading_content) != NULL) {
        LOG(("browser_window_reload: No current content or loading in progress, returning NSERROR_INVALID"));
        return NSERROR_INVALID;
    }
    if (all && content_get_type(bw->current_content) == CONTENT_HTML) {
        struct html_stylesheet *sheets;
        struct content_html_object *object;
        unsigned int count;
        c = bw->current_content;
        LOG(("browser_window_reload: Invalidating HTML content objects"));
        /* invalidate objects */
        object = html_get_objects(c, &count);
        for (; object != NULL; object = object->next) {
            if (object->content != NULL) {
                LOG(("browser_window_reload: Invalidating object content %p", object->content));
                content_invalidate_reuse_data(object->content);
            }
        }
        /* invalidate stylesheets */
        sheets = html_get_stylesheets(c, &count);
        for (i = STYLESHEET_START; i != count; i++) {
            if (sheets[i].sheet != NULL) {
                LOG(("browser_window_reload: Invalidating stylesheet %d", i));
                content_invalidate_reuse_data(sheets[i].sheet);
            }
        }
    }
    LOG(("browser_window_reload: Invalidating current content %p", bw->current_content));
    content_invalidate_reuse_data(bw->current_content);
    reload_url = hlcache_handle_get_url(bw->current_content);
    LOG(("browser_window_reload: Reloading URL %s", nsurl_access(reload_url)));
    nserror err = browser_window_navigate(bw,
                                         reload_url,
                                         NULL,
                                         BW_NAVIGATE_NONE,
                                         NULL,
                                         NULL,
                                         NULL);
    LOG(("browser_window_reload: Returning err = %d", err));
    return err;
}

/* Exported interface, documented in netsurf/browser_window.h */
void browser_window_set_status(struct browser_window *bw, const char *text)
{
    int text_len;
    LOG(("browser_window_set_status: bw = %p, text = %s", bw, text));
    /* find topmost window */
    while (bw->parent)
        bw = bw->parent;
    LOG(("browser_window_set_status: Topmost window = %p", bw));
    if ((bw->status.text != NULL) &&
        (strcmp(text, bw->status.text) == 0)) {
        /* status text is unchanged */
        bw->status.match++;
        LOG(("browser_window_set_status: Status unchanged, match count = %d", bw->status.match));
        return;
    }
    /* status text is changed */
    text_len = strlen(text);
    LOG(("browser_window_set_status: New status text length = %d", text_len));
    if ((bw->status.text == NULL) || (bw->status.text_len < text_len)) {
        /* no current string allocation or it is not long enough */
        LOG(("browser_window_set_status: Allocating new status text"));
        free(bw->status.text);
        bw->status.text = strdup(text);
        bw->status.text_len = text_len;
    } else {
        /* current allocation has enough space */
        LOG(("browser_window_set_status: Reusing existing status text allocation"));
        memcpy(bw->status.text, text, text_len + 1);
    }
    bw->status.miss++;
    LOG(("browser_window_set_status: Status miss count = %d", bw->status.miss));
    guit->window->set_status(bw->window, bw->status.text);
    LOG(("browser_window_set_status: Set status to %s", bw->status.text));
}

/* Exported interface, documented in netsurf/browser_window.h */
void browser_window_set_pointer(struct browser_window *bw,
                               browser_pointer_shape shape)
{
    struct browser_window *root = browser_window_get_root(bw);
    gui_pointer_shape gui_shape;
    bool loading;
    uint64_t ms_now;
    LOG(("browser_window_set_pointer: bw = %p, shape = %d", bw, shape));
    assert(root);
    assert(root->window);
    loading = ((bw->loading_content != NULL) ||
              ((bw->current_content != NULL) &&
               (content_get_status(bw->current_content) == CONTENT_STATUS_READY)));
    LOG(("browser_window_set_pointer: loading = %d", loading));
    nsu_getmonotonic_ms(&ms_now);
    LOG(("browser_window_set_pointer: Current time = %llu, last_action = %llu", ms_now, bw->last_action));
    if (loading && ((ms_now - bw->last_action) < 1000)) {
        /* If loading and less than 1 second since last link followed,
         * force progress indicator pointer */
        gui_shape = GUI_POINTER_PROGRESS;
        LOG(("browser_window_set_pointer: Forcing progress indicator pointer"));
    } else if (shape == BROWSER_POINTER_AUTO) {
        /* Up to browser window to decide */
        if (loading) {
            gui_shape = GUI_POINTER_PROGRESS;
            LOG(("browser_window_set_pointer: Auto shape, loading, setting GUI_POINTER_PROGRESS"));
        } else {
            gui_shape = GUI_POINTER_DEFAULT;
            LOG(("browser_window_set_pointer: Auto shape, not loading, setting GUI_POINTER_DEFAULT"));
        }
    } else {
        /* Use what we were told */
        gui_shape = (gui_pointer_shape)shape;
        LOG(("browser_window_set_pointer: Using specified shape %d", gui_shape));
    }
    guit->window->set_pointer(root->window, gui_shape);
    LOG(("browser_window_set_pointer: Set pointer to %d for root window %p", gui_shape, root->window));
}

/* exported function documented in netsurf/browser_window.h */
nserror browser_window_schedule_reformat(struct browser_window *bw)
{
    LOG(("browser_window_schedule_reformat: bw = %p", bw));
    if (bw->window == NULL) {
        LOG(("browser_window_schedule_reformat: No GUI window, returning NSERROR_BAD_PARAMETER"));
        return NSERROR_BAD_PARAMETER;
    }
    nserror ret = guit->misc->schedule(0, scheduled_reformat, bw);
    LOG(("browser_window_schedule_reformat: Returning ret = %d", ret));
    return ret;
}

/* exported function documented in netsurf/browser_window.h */
void
browser_window_reformat(struct browser_window *bw,
                       bool background,
                       int width,
                       int height)
{
    hlcache_handle *c = bw->current_content;
    LOG(("browser_window_reformat: bw = %p, background = %d, width = %d, height = %d", bw, background, width, height));
    if (c == NULL) {
        LOG(("browser_window_reformat: No current content, returning"));
        return;
    }
    if (bw->browser_window_type != BROWSER_WINDOW_IFRAME) {
        /* Iframe dimensions are already scaled in parent's layout */
        width  /= bw->scale;
        height /= bw->scale;
        LOG(("browser_window_reformat: Adjusted dimensions for non-IFRAME: width = %d, height = %d", width, height));
    }
    if (bw->window == NULL) {
        /* Core managed browser window; subtract scrollbar width */
        width  -= bw->scroll_y ? SCROLLBAR_WIDTH : 0;
        height -= bw->scroll_x ? SCROLLBAR_WIDTH : 0;
        width  = width  > 0 ? width  : 0;
        height = height > 0 ? height : 0;
        LOG(("browser_window_reformat: Core-managed window, final dimensions: width = %d, height = %d", width, height));
    }
    LOG(("browser_window_reformat: Reformatting content %p", c));
    content_reformat(c, background, width, height);
}

/* exported interface documented in netsurf/browser_window.h */
nserror
browser_window_set_scale(struct browser_window *bw, float scale, bool absolute)
{
    nserror res;
    LOG(("browser_window_set_scale: bw = %p, scale = %f, absolute = %d", bw, scale, absolute));
    /* get top browser window */
    while (bw->parent) {
        bw = bw->parent;
        LOG(("browser_window_set_scale: Moving to parent window %p", bw));
    }
    if (!absolute) {
        /* snap small values around 1.0 */
        if ((scale + bw->scale) > (1.01 - scale) &&
            (scale + bw->scale) < (0.99 + scale)) {
            scale = 1.0;
            LOG(("browser_window_set_scale: Snapped scale to 1.0"));
        } else {
            scale += bw->scale;
            LOG(("browser_window_set_scale: Relative scale adjustment, new scale = %f", scale));
        }
    }
    /* clamp range between 0.1 and 10 (10% and 1000%) */
    if (scale < SCALE_MINIMUM) {
        scale = SCALE_MINIMUM;
        LOG(("browser_window_set_scale: Clamped scale to minimum %f", SCALE_MINIMUM));
    } else if (scale > SCALE_MAXIMUM) {
        scale = SCALE_MAXIMUM;
        LOG(("browser_window_set_scale: Clamped scale to maximum %f", SCALE_MAXIMUM));
    }
    res = browser_window_set_scale_internal(bw, scale);
    LOG(("browser_window_set_scale: browser_window_set_scale_internal returned %d", res));
    if (res == NSERROR_OK) {
        LOG(("browser_window_set_scale: Recalculating frameset"));
        browser_window_recalculate_frameset(bw);
    }
    LOG(("browser_window_set_scale: Returning res = %d", res));
    return res;
}

/* exported interface documented in netsurf/browser_window.h */
float browser_window_get_scale(struct browser_window *bw)
{
    LOG(("browser_window_get_scale: bw = %p", bw));
    if (bw == NULL) {
        LOG(("browser_window_get_scale: NULL browser window, returning 1.0"));
        return 1.0;
    }
    LOG(("browser_window_get_scale: Returning scale = %f", bw->scale));
    return bw->scale;
}

/* exported interface documented in netsurf/browser_window.h */
struct browser_window *
browser_window_find_target(struct browser_window *bw,
                          const char *target,
                          browser_mouse_state mouse)
{
    struct browser_window *bw_target;
    struct browser_window *top;
    hlcache_handle *c;
    int rdepth;
    nserror error;
    LOG(("browser_window_find_target: bw = %p, target = %s, mouse = %d", bw, target ? target : "NULL", mouse));
    /* use the base target if we don't have one */
    c = bw->current_content;
    if (target == NULL &&
        c != NULL &&
        content_get_type(c) == CONTENT_HTML) {
        target = html_get_base_target(c);
        LOG(("browser_window_find_target: Using base target %s", target ? target : "NULL"));
    }
    if (target == NULL) {
        target = "_self";
        LOG(("browser_window_find_target: Defaulting to _self"));
    }
    /* allow the simple case of target="_blank" to be ignored if requested */
    if ((!(mouse & BROWSER_MOUSE_CLICK_2)) &&
        (!((mouse & BROWSER_MOUSE_CLICK_2) &&
           (mouse & BROWSER_MOUSE_MOD_2))) &&
        (!nsoption_bool(target_blank))) {
        /* not a mouse button 2 click
         * not a mouse button 1 click with ctrl pressed
         * configured to ignore target="_blank" */
        if (!strcasecmp(target, "_blank")) {
            LOG(("browser_window_find_target: Ignoring _blank, returning current window %p", bw));
            return bw;
        }
    }
    /* handle reserved keywords */
    if (((nsoption_bool(button_2_tab)) &&
         (mouse & BROWSER_MOUSE_CLICK_2)) ||
        ((!nsoption_bool(button_2_tab)) &&
         ((mouse & BROWSER_MOUSE_CLICK_1) &&
          (mouse & BROWSER_MOUSE_MOD_2))) ||
        ((nsoption_bool(button_2_tab)) &&
         (!strcasecmp(target, "_blank")))) {
        /* open in new tab if:
         * - button_2 opens in new tab and button_2 was pressed
         * OR
         * - button_2 doesn't open in new tabs and button_1 was
         *   pressed with ctrl held
         * OR
         * - button_2 opens in new tab and the link target is "_blank"
         */
        LOG(("browser_window_find_target: Creating new tab for target %s", target));
        error = browser_window_create(BW_CREATE_TAB |
                                     BW_CREATE_HISTORY |
                                     BW_CREATE_CLONE,
                                     NULL,
                                     NULL,
                                     bw,
                                     &bw_target);
        if (error != NSERROR_OK) {
            LOG(("browser_window_find_target: Failed to create new tab, returning current window %p", bw));
            return bw;
        }
        LOG(("browser_window_find_target: Returning new tab window %p", bw_target));
        return bw_target;
    } else if (((!nsoption_bool(button_2_tab)) &&
                (mouse & BROWSER_MOUSE_CLICK_2)) ||
               ((nsoption_bool(button_2_tab)) &&
                ((mouse & BROWSER_MOUSE_CLICK_1) &&
                 (mouse & BROWSER_MOUSE_MOD_2))) ||
               ((!nsoption_bool(button_2_tab)) &&
                (!strcasecmp(target, "_blank")))) {
        /* open in new window if:
         * - button_2 doesn't open in new tabs and button_2 was pressed
         * OR
         * - button_2 opens in new tab and button_1 was pressed with
         *   ctrl held
         * OR
         * - button_2 doesn't open in new tabs and the link target is
         *   "_blank"
         */
        LOG(("browser_window_find_target: Creating new window for target %s", target));
        error = browser_window_create(BW_CREATE_HISTORY |
                                     BW_CREATE_CLONE,
                                     NULL,
                                     NULL,
                                     bw,
                                     &bw_target);
        if (error != NSERROR_OK) {
            LOG(("browser_window_find_target: Failed to create new window, returning current window %p", bw));
            return bw;
        }
        LOG(("browser_window_find_target: Returning new window %p", bw_target));
        return bw_target;
    } else if (!strcasecmp(target, "_self")) {
        LOG(("browser_window_find_target: Target _self, returning current window %p", bw));
        return bw;
    } else if (!strcasecmp(target, "_parent")) {
        if (bw->parent) {
            LOG(("browser_window_find_target: Target _parent, returning parent window %p", bw->parent));
            return bw->parent;
        }
        LOG(("browser_window_find_target: Target _parent, no parent, returning current window %p", bw));
        return bw;
    } else if (!strcasecmp(target, "_top")) {
        while (bw->parent) {
            bw = bw->parent;
            LOG(("browser_window_find_target: Moving to parent for _top, current = %p", bw));
        }
        LOG(("browser_window_find_target: Target _top, returning top window %p", bw));
        return bw;
    }
    /* find frame according to B.8, ie using the following priorities:
     * 1) current frame
     * 2) closest to front
     */
    rdepth = -1;
    bw_target = NULL;
    for (top = bw; top->parent; top = top->parent)
        ;
    LOG(("browser_window_find_target: Searching for target %s starting from top window %p", target, top));
    browser_window_find_target_internal(top, target, 0, bw, &rdepth,
                                       &bw_target);
    if (bw_target) {
        LOG(("browser_window_find_target: Found target window %p", bw_target));
        return bw_target;
    }
    /* we require a new window using the target name */
    if (!nsoption_bool(target_blank)) {
        LOG(("browser_window_find_target: target_blank disabled, returning current window %p", bw));
        return bw;
    }
    LOG(("browser_window_find_target: Creating new window for named target %s", target));
    error = browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY,
                                 NULL,
                                 NULL,
                                 bw,
                                 &bw_target);
    if (error != NSERROR_OK) {
        LOG(("browser_window_find_target: Failed to create new window, returning current window %p", bw));
        return bw;
    }
    /* frame names should begin with an alphabetic character (a-z,A-Z),
     * however in practice you get things such as '_new' and '2left'. The
     * only real effect this has is when giving out names as it can be
     * assumed that an author intended '_new' to create a new nameless
     * window (ie '_blank') whereas in the case of '2left' the intention
     * was for a new named window. As such we merely special case windows
     * that begin with an underscore. */
    if (target[0] != '_') {
        bw_target->name = strdup(target);
        LOG(("browser_window_find_target: Set new window name to %s", target));
    }
    LOG(("browser_window_find_target: Returning new window %p", bw_target));
    return bw_target;
}

/* exported interface documented in netsurf/browser_window.h */
void
browser_window_mouse_track(struct browser_window *bw,
                          browser_mouse_state mouse,
                          int x, int y)
{
//    LOG(("browser_window_mouse_track: bw = %p, mouse = %d, x = %d, y = %d", bw, mouse, x, y));
    browser_window_mouse_track_internal(bw,
                                       mouse,
                                       x / bw->scale,
                                       y / bw->scale);
//   LOG(("browser_window_mouse_track: Completed mouse track"));
}

/* exported interface documented in netsurf/browser_window.h */
void
browser_window_mouse_click(struct browser_window *bw,
                          browser_mouse_state mouse,
                          int x, int y)
{
    LOG(("browser_window_mouse_click: bw = %p, mouse = %d, x = %d, y = %d", bw, mouse, x, y));
    browser_window_mouse_click_internal(bw,
                                       mouse,
                                       x / bw->scale,
                                       y / bw->scale);
    LOG(("browser_window_mouse_click: Completed mouse click"));
}

/* exported interface documented in netsurf/browser_window.h */
void browser_window_page_drag_start(struct browser_window *bw, int x, int y)
{
    LOG(("browser_window_page_drag_start: bw = %p, x = %d, y = %d", bw, x, y));
    assert(bw != NULL);
    browser_window_set_drag_type(bw, DRAGGING_PAGE_SCROLL, NULL);
    bw->drag.start_x = x;
    bw->drag.start_y = y;
    LOG(("browser_window_page_drag_start: Set drag type to DRAGGING_PAGE_SCROLL, start_x = %d, start_y = %d", x, y));
    if (bw->window != NULL) {
        /* Front end window */
        LOG(("browser_window_page_drag_start: Front end window, getting scroll position"));
        guit->window->get_scroll(bw->window,
                                &bw->drag.start_scroll_x,
                                &bw->drag.start_scroll_y);
        LOG(("browser_window_page_drag_start: Scroll position start_scroll_x = %d, start_scroll_y = %d", bw->drag.start_scroll_x, bw->drag.start_scroll_y));
        guit->window->event(bw->window, GW_EVENT_SCROLL_START);
        LOG(("browser_window_page_drag_start: Sent GW_EVENT_SCROLL_START"));
    } else {
        /* Core managed browser window */
        bw->drag.start_scroll_x = scrollbar_get_offset(bw->scroll_x);
        bw->drag.start_scroll_y = scrollbar_get_offset(bw->scroll_y);
        LOG(("browser_window_page_drag_start: Core-managed window, start_scroll_x = %d, start_scroll_y = %d", bw->drag.start_scroll_x, bw->drag.start_scroll_y));
    }
}

/* exported interface documented in netsurf/browser_window.h */
bool browser_window_back_available(struct browser_window *bw)
{
    LOG(("browser_window_back_available: bw = %p", bw));
    if (bw != NULL && bw->internal_nav) {
        /* Internal nav, back is possible */
        LOG(("browser_window_back_available: Internal navigation, returning true"));
        return true;
    }
    bool result = (bw && bw->history && browser_window_history_back_available(bw));
    LOG(("browser_window_back_available: History available = %d, returning %d", bw && bw->history ? 1 : 0, result));
    return result;
}

/* exported interface documented in netsurf/browser_window.h */
bool browser_window_forward_available(struct browser_window *bw)
{
    bool result = (bw && bw->history && browser_window_history_forward_available(bw));
    LOG(("browser_window_forward_available: bw = %p, history available = %d, returning %d", bw, bw && bw->history ? 1 : 0, result));
    return result;
}

/* exported interface documented in netsurf/browser_window.h */
bool browser_window_reload_available(struct browser_window *bw)
{
    bool result = (bw && bw->current_content && !bw->loading_content);
    LOG(("browser_window_reload_available: bw = %p, current_content = %p, loading_content = %p, returning %d", bw, bw->current_content, bw->loading_content, result));
    return result;
}

/* exported interface documented in netsurf/browser_window.h */
bool browser_window_stop_available(struct browser_window *bw)
{
    bool result = (bw && (bw->loading_content ||
                          (bw->current_content &&
                           (content_get_status(bw->current_content) !=
                            CONTENT_STATUS_DONE))));
    LOG(("browser_window_stop_available: bw = %p, loading_content = %p, current_content status = %d, returning %d",
         bw, bw->loading_content, bw->current_content ? content_get_status(bw->current_content) : -1, result));
    return result;
}

/* exported interface documented in netsurf/browser_window.h */
bool
browser_window_exec(struct browser_window *bw, const char *src, size_t srclen)
{
    LOG(("browser_window_exec: bw = %p, src = %s, srclen = %zu", bw, src, srclen));
    assert(bw != NULL);
    if (!bw->current_content) {
        NSLOG(netsurf, DEEPDEBUG, "Unable to exec, no content");
        LOG(("browser_window_exec: No current content, returning false"));
        return false;
    }
    if (content_get_status(bw->current_content) != CONTENT_STATUS_DONE) {
        NSLOG(netsurf, DEEPDEBUG, "Unable to exec, content not done");
        LOG(("browser_window_exec: Content status = %d, not done, returning false", content_get_status(bw->current_content)));
        return false;
    }
    /* Okay it should be safe, forward the request through to the content
     * itself. Only HTML contents currently support executing code
     */
    bool result = content_exec(bw->current_content, src, srclen);
    LOG(("browser_window_exec: content_exec returned %d", result));
    return result;
}

/* exported interface documented in browser_window.h */
nserror
browser_window_console_log(struct browser_window *bw,
                          browser_window_console_source src,
                          const char *msg,
                          size_t msglen,
                          browser_window_console_flags flags)
{
    browser_window_console_flags log_level = flags & BW_CS_FLAG_LEVEL_MASK;
    struct browser_window *root = browser_window_get_root(bw);
    LOG(("browser_window_console_log: bw = %p, src = %d, msg = %s, msglen = %zu, flags = %d", bw, src, msg, msglen, flags));
    assert(msg != NULL);
    /* We don't assert msglen > 0, if someone wants to log a real empty
     * string then we won't stop them. It does sometimes happen from
     * JavaScript for example.
     */
    /* bw is the target of the log, but root is where we log it */
    NSLOG(netsurf, DEEPDEBUG, "Logging message in %p targetted at %p", root, bw);
    LOG(("browser_window_console_log: Logging to root window %p", root));
    NSLOG(netsurf, DEEPDEBUG, "Log came from %s",
          ((src == BW_CS_INPUT) ? "user input" :
           (src == BW_CS_SCRIPT_ERROR) ? "script error" :
           (src == BW_CS_SCRIPT_CONSOLE) ? "script console" :
           "unknown input location"));
    LOG(("browser_window_console_log: Log source = %d", src));
    switch (log_level) {
    case BW_CS_FLAG_LEVEL_DEBUG:
        NSLOG(netsurf, DEBUG, "%.*s", (int)msglen, msg);
        LOG(("browser_window_console_log: DEBUG level log: %.*s", (int)msglen, msg));
        break;
    case BW_CS_FLAG_LEVEL_LOG:
        NSLOG(netsurf, VERBOSE, "%.*s", (int)msglen, msg);
        LOG(("browser_window_console_log: VERBOSE level log: %.*s", (int)msglen, msg));
        break;
    case BW_CS_FLAG_LEVEL_INFO:
        NSLOG(netsurf, INFO, "%.*s", (int)msglen, msg);
        LOG(("browser_window_console_log: INFO level log: %.*s", (int)msglen, msg));
        break;
    case BW_CS_FLAG_LEVEL_WARN:
        NSLOG(netsurf, WARNING, "%.*s", (int)msglen, msg);
        LOG(("browser_window_console_log: WARNING level log: %.*s", (int)msglen, msg));
        break;
    case BW_CS_FLAG_LEVEL_ERROR:
        NSLOG(netsurf, ERROR, "%.*s", (int)msglen, msg);
        LOG(("browser_window_console_log: ERROR level log: %.*s", (int)msglen, msg));
        break;
    default:
        /* Unreachable */
        LOG(("browser_window_console_log: Unreachable default case"));
        break;
    }
    guit->window->console_log(root->window, src, msg, msglen, flags);
    LOG(("browser_window_console_log: Called console_log on root window"));
    return NSERROR_OK;
}

/* Exported interface, documented in browser_private.h */
nserror
browser_window__reload_current_parameters(struct browser_window *bw)
{
    LOG(("browser_window__reload_current_parameters: bw = %p", bw));
    assert(bw != NULL);
    if (bw->current_parameters.post_urlenc != NULL) {
        LOG(("browser_window__reload_current_parameters: Freeing post_urlenc"));
        free(bw->current_parameters.post_urlenc);
        bw->current_parameters.post_urlenc = NULL;
    }
    if (bw->current_parameters.post_multipart != NULL) {
        LOG(("browser_window__reload_current_parameters: Destroying post_multipart"));
        fetch_multipart_data_destroy(bw->current_parameters.post_multipart);
        bw->current_parameters.post_multipart = NULL;
    }
    if (bw->current_parameters.url == NULL) {
        /* We have never navigated so go to about:blank */
        bw->current_parameters.url = nsurl_ref(corestring_nsurl_about_blank);
        LOG(("browser_window__reload_current_parameters: Set URL to about:blank"));
    }
    bw->current_parameters.flags &= ~BW_NAVIGATE_HISTORY;
    LOG(("browser_window__reload_current_parameters: Cleared BW_NAVIGATE_HISTORY flag"));
    bw->internal_nav = false;
    LOG(("browser_window__reload_current_parameters: Set internal_nav = false"));
    browser_window__free_fetch_parameters(&bw->loading_parameters);
    LOG(("browser_window__reload_current_parameters: Freed loading_parameters"));
    memcpy(&bw->loading_parameters, &bw->current_parameters, sizeof(bw->loading_parameters));
    LOG(("browser_window__reload_current_parameters: Copied current_parameters to loading_parameters"));
    memset(&bw->current_parameters, 0, sizeof(bw->current_parameters));
    LOG(("browser_window__reload_current_parameters: Cleared current_parameters"));
    nserror ret = browser_window__navigate_internal(bw, &bw->loading_parameters);
    LOG(("browser_window__reload_current_parameters: Returning ret = %d", ret));
    return ret;
}

/* Exported interface, documented in browser_window.h */
browser_window_page_info_state browser_window_get_page_info_state(
        const struct browser_window *bw)
{
    lwc_string *scheme;
    bool match;
    LOG(("browser_window_get_page_info_state: bw = %p", bw));
    assert(bw != NULL);
    /* Do we have any content? If not -- UNKNOWN */
    if (bw->current_content == NULL) {
        LOG(("browser_window_get_page_info_state: No current content, returning PAGE_STATE_UNKNOWN"));
        return PAGE_STATE_UNKNOWN;
    }
    scheme = nsurl_get_component(
        hlcache_handle_get_url(bw->current_content), NSURL_SCHEME);
    LOG(("browser_window_get_page_info_state: Scheme = %s", lwc_string_data(scheme)));
    /* Is this an internal scheme? */
    if ((lwc_string_isequal(scheme, corestring_lwc_about,
                            &match) == lwc_error_ok &&
         (match == true)) ||
        (lwc_string_isequal(scheme, corestring_lwc_data,
                            &match) == lwc_error_ok &&
         (match == true)) ||
        (lwc_string_isequal(scheme, corestring_lwc_resource,
                            &match) == lwc_error_ok &&
         (match == true))) {
        lwc_string_unref(scheme);
        LOG(("browser_window_get_page_info_state: Internal scheme, returning PAGE_STATE_INTERNAL"));
        return PAGE_STATE_INTERNAL;
    }
    /* Is this file:/// ? */
    if (lwc_string_isequal(scheme, corestring_lwc_file,
                           &match) == lwc_error_ok &&
        match == true) {
        lwc_string_unref(scheme);
        LOG(("browser_window_get_page_info_state: File scheme, returning PAGE_STATE_LOCAL"));
        return PAGE_STATE_LOCAL;
    }
    /* If not https, from here on down that'd be insecure */
    if ((lwc_string_isequal(scheme, corestring_lwc_https,
                            &match) == lwc_error_ok &&
         (match == false))) {
        /* Some remote content, not https, therefore insecure */
        lwc_string_unref(scheme);
        LOG(("browser_window_get_page_info_state: Non-HTTPS, returning PAGE_STATE_INSECURE"));
        return PAGE_STATE_INSECURE;
    }
    lwc_string_unref(scheme);
    /* Did we have to override this SSL setting? */
    if (urldb_get_cert_permissions(hlcache_handle_get_url(bw->current_content))) {
        LOG(("browser_window_get_page_info_state: SSL override detected, returning PAGE_STATE_SECURE_OVERRIDE"));
        return PAGE_STATE_SECURE_OVERRIDE;
    }
    /* If we've seen insecure content internally then we need to say so */
    if (content_saw_insecure_objects(bw->current_content)) {
        LOG(("browser_window_get_page_info_state: Insecure objects detected, returning PAGE_STATE_SECURE_ISSUES"));
        return PAGE_STATE_SECURE_ISSUES;
    }
    /* All is well, return secure state */
    LOG(("browser_window_get_page_info_state: Returning PAGE_STATE_SECURE"));
    return PAGE_STATE_SECURE;
}

/* Exported interface, documented in browser_window.h */
nserror
browser_window_get_ssl_chain(struct browser_window *bw,
                            struct cert_chain **chain)
{
    LOG(("browser_window_get_ssl_chain: bw = %p", bw));
    assert(bw != NULL);
    if (bw->current_cert_chain == NULL) {
        LOG(("browser_window_get_ssl_chain: No current cert chain, returning NSERROR_NOT_FOUND"));
        return NSERROR_NOT_FOUND;
    }
    *chain = bw->current_cert_chain;
    LOG(("browser_window_get_ssl_chain: Returning cert chain %p", *chain));
    return NSERROR_OK;
}

/* Exported interface, documented in browser_window.h */
int browser_window_get_cookie_count(
        const struct browser_window *bw)
{
    int count = 0;
    LOG(("browser_window_get_cookie_count: bw = %p", bw));
    char *cookies = urldb_get_cookie(browser_window_access_url(bw), true);
    if (cookies == NULL) {
        LOG(("browser_window_get_cookie_count: No cookies, returning 0"));
        return 0;
    }
    for (char *c = cookies; *c != '\0'; c++) {
        if (*c == ';')
            count++;
    }
    LOG(("browser_window_get_cookie_count: Found %d cookies", count));
    free(cookies);
    return count;
}

/* Exported interface, documented in browser_window.h */
nserror browser_window_show_cookies(
        const struct browser_window *bw)
{
    nserror err;
    nsurl *url = browser_window_access_url(bw);
    lwc_string *host = nsurl_get_component(url, NSURL_HOST);
    const char *string = (host != NULL) ? lwc_string_data(host) : NULL;
    LOG(("browser_window_show_cookies: bw = %p, host = %s", bw, string ? string : "NULL"));
    err = guit->misc->present_cookies(string);
    LOG(("browser_window_show_cookies: present_cookies returned %d", err));
    if (host != NULL) {
        lwc_string_unref(host);
        LOG(("browser_window_show_cookies: Released host string"));
    }
    return err;
}

/* Exported interface, documented in browser_window.h */
nserror browser_window_show_certificates(struct browser_window *bw)
{
    nserror res;
    nsurl *url;
    LOG(("browser_window_show_certificates: bw = %p", bw));
    if (bw->current_cert_chain == NULL) {
        LOG(("browser_window_show_certificates: No current cert chain, returning NSERROR_NOT_FOUND"));
        return NSERROR_NOT_FOUND;
    }
    res = cert_chain_to_query(bw->current_cert_chain, &url);
    LOG(("browser_window_show_certificates: cert_chain_to_query returned %d, url = %s", res, url ? nsurl_access(url) : "NULL"));
    if (res == NSERROR_OK) {
        LOG(("browser_window_show_certificates: Creating new window for certificate URL %s", nsurl_access(url)));
        res = browser_window_create(BW_CREATE_HISTORY |
                                   BW_CREATE_FOREGROUND |
                                   BW_CREATE_TAB,
                                   url,
                                   NULL,
                                   bw,
                                   NULL);
        nsurl_unref(url);
        LOG(("browser_window_show_certificates: Released certificate URL"));
    }
    LOG(("browser_window_show_certificates: Returning res = %d", res));
    return res;
}
