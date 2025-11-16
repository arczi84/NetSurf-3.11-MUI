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
#include <stdbool.h>
#include <devices/rawkeycodes.h>
#include <exec/execbase.h>
#include <graphics/rpattr.h>
#include <intuition/pointerclass.h>
#include <devices/printer.h>
#undef NO_INLINE_STDARG
#include <proto/codesets.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/keymap.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#ifdef TTENGINE
#include <proto/ttengine.h>
#endif
#include <proto/utility.h>
#include <exec/types.h>

#include <stdint.h>

//nserror
#include "utils/errors.h"
#include "netsurf/types.h"
#include "netsurf/browser_window.h"
#include "netsurf/mouse.h"
#include "netsurf/content.h"
#include "netsurf/content_type.h"
#include "netsurf/plotters.h"
#include "netsurf/keypress.h"

#include <time.h>

#include "desktop/browser_history.h"  // Updated header
#include "desktop/browser_private.h"
#include "desktop/knockout.h"
#include "desktop/global_history.h"
#include "desktop/textinput.h"
#include "content/content.h"
#include "frontends/mui/gui.h"
#include "frontends/mui/mui.h"
#include "frontends/mui/netsurf.h"
#include "frontends/mui/plotters.h"
#include "frontends/mui/print.h"
#include "frontends/mui/search.h"
#include "frontends/mui/utils.h"
#include "utils/log0.h"

void
mui_plot_reset_stats(void)
{
    /* Plot statistics not gathered on this build path. */
}

void
mui_plot_log_stats(const char *label)
{
    (void)label;
}

#include "os3.h"
#define MIN(a,b) (((a)<(b))?(a):(b))

//#define BITMAP_FLAGS BMF_MINPLANES
#define BITMAP_FLAGS (BMF_MINPLANES | BMF_DISPLAYABLE)

LONG global_pen_a = -1;
LONG global_pen_b = -1;

struct Data
{
    struct MUI_EventHandlerNode  ehnode;
    struct gui_window           *context;
    struct browser_window       *browser;

    WORD mouse_state, key_state;
    WORD mwidth, mheight;

    ULONG content_width, content_height;

    ULONG pointertype;
    ULONG mouse_inside;

    BYTE back_available;
    BYTE forward_available;
    BYTE stop_available;
    BYTE reload_available;
    WORD redraw_pending, redraw;
    LONG vleft_old, vtop_old;
    LONG changed;

    LONG loading;
    STRPTR status_text;
    STRPTR url;
    STRPTR title;

    struct RastPort *RastPort;
    struct BitMap   *BitMap;
    struct Layer    *Layer;
    APTR LayerInfo;
    LONG bm_width, bm_height;

    APTR title_obj;
    
    // Dodane dla retry mechanizmu
    LONG retry_count;
};

struct mui_tile_flush_ctx {
    APTR obj;
    struct Data *data;
    unsigned long flush_ticks;
    unsigned long flush_tiles;
    unsigned long flush_pixels;
};

static void
copy_bitmap_tile_to_screen(APTR obj, struct Data *data,
    int screen_x0, int screen_y0,
    int screen_x1, int screen_y1)
{
    if (data == NULL || data->BitMap == NULL) {
        return;
    }

    if (muiRenderInfo(obj) == NULL) {
        return;
    }

    struct RastPort *screen_rp = _rp(obj);
    if (screen_rp == NULL) {
        return;
    }

    const LONG dest_left = _mleft(obj);
    const LONG dest_top = _mtop(obj);
    const LONG dest_width = _mwidth(obj);
    const LONG dest_height = _mheight(obj);

    int width = screen_x1 - screen_x0;
    int height = screen_y1 - screen_y0;
    if (width <= 0 || height <= 0) {
        return;
    }

    // Source coordinates in the bitmap
    int src_x = screen_x0;
    int src_y = screen_y0;
    // Destination coordinates on screen
    int dest_x = dest_left + screen_x0;
    int dest_y = dest_top + screen_y0;

    if (src_x < 0) {
        dest_x -= src_x;
        width += src_x;
        src_x = 0;
    }
    if (src_y < 0) {
        dest_y -= src_y;
        height += src_y;
        src_y = 0;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    if (src_x >= data->bm_width || src_y >= data->bm_height) {
        return;
    }

    if (src_x + width > data->bm_width) {
        width = data->bm_width - src_x;
    }
    if (src_y + height > data->bm_height) {
        height = data->bm_height - src_y;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    int max_width = (dest_left + dest_width) - dest_x;
    int max_height = (dest_top + dest_height) - dest_y;
    if (max_width <= 0 || max_height <= 0) {
        return;
    }

    if (width > max_width) {
        width = max_width;
    }
    if (height > max_height) {
        height = max_height;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    BltBitMapRastPort(data->BitMap,
              src_x,
              src_y,
              screen_rp,
              dest_x,
              dest_y,
              width,
              height,
              0xC0);
}

static void
copy_bitmap_to_screen(APTR obj, struct Data *data)
{
    if (data == NULL || data->BitMap == NULL) {
        return;
    }

    if (muiRenderInfo(obj) == NULL) {
        return;
    }

    const ULONG dest_width = _mwidth(obj);
    const ULONG dest_height = _mheight(obj);

    if ((dest_width == 0) || (dest_height == 0)) {
        return;
    }

    const ULONG copy_width = MIN(dest_width, data->bm_width);
    const ULONG copy_height = MIN(dest_height, data->bm_height);

    if ((copy_width == 0) || (copy_height == 0)) {
        return;
    }

    // Bitmap contains the visible area, copy from 0,0
    copy_bitmap_tile_to_screen(obj, data,
        0, 0,
        (int)copy_width, (int)copy_height);
}

static void
flush_tile_to_screen(struct gui_window *g, void *ctx,
    int screen_x0, int screen_y0,
    int screen_x1, int screen_y1)
{
    struct mui_tile_flush_ctx *flush_ctx = (struct mui_tile_flush_ctx *)ctx;
    int width;
    int height;
    clock_t start;
    clock_t delta;
    (void)g;
    if (flush_ctx == NULL) {
        return;
    }

    width = screen_x1 - screen_x0;
    height = screen_y1 - screen_y0;
    start = clock();

    copy_bitmap_tile_to_screen(flush_ctx->obj, flush_ctx->data,
        screen_x0, screen_y0, screen_x1, screen_y1);

    delta = clock() - start;
    flush_ctx->flush_ticks += (unsigned long)delta;
    if (width > 0 && height > 0) {
        unsigned long pixels = (unsigned long)width * (unsigned long)height;
        flush_ctx->flush_pixels += pixels;
        flush_ctx->flush_tiles++;
    }
}

// ====== NOWE: Content Ready Callback System ======

// Callback wywoływany gdy content jest gotowy
static void content_ready_callback(struct browser_window *bw, void *user_data) 
{
    Object *obj = (Object *)user_data;
    struct Data *data = INST_DATA(OCLASS(obj), obj);
    
    LOG(("DEBUG: Content ready callback triggered for bw=%p", bw));
    
    if (data && data->browser == bw) {
        LOG(("DEBUG: Content ready - triggering redraw"));
        data->retry_count = 0;  // Reset retry counter
        
        // Wymuś ponowne renderowanie
    mui_queue_method_delay(_app(obj), obj, 50, MM_Browser_Redraw);
    }
}

// Alternatywny callback przez gui_window_event
 void mui_trigger_content_ready_redraw(Object *obj)
{
    struct Data *data = INST_DATA(OCLASS(obj), obj);
    
    LOG(("DEBUG: Content ready event received"));
    if (data) {
        data->retry_count = 0;
    mui_queue_method_delay(_app(obj), obj, 10, MM_Browser_Redraw);
    }
}


void textinput_enable(struct browser_window *bw)
{
    LOG(("DEBUG: textinput_enable called"));
    // Implement AmigaOS 3.x-specific text input enable logic, e.g., activate MUI text gadget or set focus
    if (bw) {
        // Example: Set focus to browser gadget or enable keyboard input
    }
}

void textinput_disable(struct browser_window *bw)
{
    LOG(("DEBUG: textinput_disable called"));
    // Implement AmigaOS 3.x-specific text input disable logic
    if (bw) {
        // Example: Remove focus or disable input
    }
}

MUI_HOOK(layoutfunc, APTR grp, struct MUI_LayoutMsg *lm)
{
    ULONG rc = MUILM_UNKNOWN;

    switch (lm->lm_Type) {
        case MUILM_MINMAX:
            lm->lm_MinMax.MinWidth = 1;
            lm->lm_MinMax.MinHeight = 1;
            lm->lm_MinMax.DefWidth = 640;
            lm->lm_MinMax.DefHeight = 480;
            lm->lm_MinMax.MaxWidth = MUI_MAXMAX;
            lm->lm_MinMax.MaxHeight = MUI_MAXMAX;
            rc = 0;
            break;

        case MUILM_LAYOUT:
        {
            struct Data *data = INST_DATA(OCLASS(grp), grp);
            lm->lm_Layout.Width = data->content_width;
            lm->lm_Layout.Height = data->content_height;
            rc = TRUE;
        }
            break;
    }

    return rc;
}

STATIC VOID delete_offscreen_bitmap(struct Data *data)
{
    LOG(("DEBUG: delete_offscreen_bitmap called"));
    if (data->Layer) {
        //if (TTEngineBase) {
        //    TT_DoneRastPort(data->RastPort);
        //}
        DeleteLayer(NULL, data->Layer);
        data->Layer = NULL;
    }

    if (data->BitMap) {
        FreeBitMap(data->BitMap);
        data->BitMap = NULL;
    }

    if (data->LayerInfo) {
        DisposeLayerInfo(data->LayerInfo);
        data->LayerInfo = NULL;
    }

    data->changed = 0;
    data->redraw_pending = 1;
    data->context->redraw = 1;

    data->RastPort = NULL;
    data->context->RastPort = NULL;
    data->context->BitMap = NULL;
    data->context->Layer = NULL;
}

STATIC BOOL create_offscreen_bitmap(APTR obj, struct Data *data, ULONG width, ULONG height)
{
    LOG(("DEBUG: create_offscreen_bitmap called with width=%lu, height=%lu", width, height));

    if (data->BitMap) {
        delete_offscreen_bitmap(data);
    }

    if (!muiRenderInfo(obj)) {
        LOG(("ERROR: muiRenderInfo not available"));
        return FALSE;
    }

    struct BitMap *screen_bm = _screen(obj)->RastPort.BitMap;
    if (!screen_bm) {
        LOG(("ERROR: Screen BitMap is NULL"));
        return FALSE;
    }

    ULONG depth = GetBitMapAttr(screen_bm, BMA_DEPTH);

    if (!data->LayerInfo) {
        data->LayerInfo = NewLayerInfo();
        if (!data->LayerInfo) {
            LOG(("ERROR: Failed to create LayerInfo"));
            return FALSE;
        }
    }

    data->BitMap = AllocBitMap(width, height, depth, BITMAP_FLAGS, screen_bm);
    if (!data->BitMap) {
        LOG(("ERROR: Failed to allocate BitMap"));
        return FALSE;
    }

    //LOG(("INFO: Using standard bitmap (chip memory)"));
    //LOG(("INFO: Using RTG bitmap (fast memory)"));

    data->Layer = CreateUpfrontHookLayer(data->LayerInfo, data->BitMap, 0, 0,
                                         width - 1, height - 1,
                                         LAYERSIMPLE, LAYERS_NOBACKFILL, NULL);
    if (!data->Layer) {
        LOG(("ERROR: Failed to create Layer"));
        FreeBitMap(data->BitMap);
        data->BitMap = NULL;
        return FALSE;
    }

    data->RastPort = data->Layer->rp;
    data->context->RastPort = data->RastPort;
    data->context->BitMap = data->BitMap;
    data->context->Layer = data->Layer;

    data->bm_width = width;
    data->bm_height = height;

    LOG(("DEBUG: Bitmap created: %lux%lux%lu", width, height, depth));
#ifdef TTENGINE
    if (TTEngineBase) {
        //TT_InitRastPort(data->RastPort); // Assuming such function if needed
    }
#endif
    return TRUE;
}

extern APTR global_obj;

// ====== ZMODYFIKOWANA FUNKCJA render_content_to_bitmap ======

STATIC VOID render_content_to_bitmap(APTR obj, struct Data *data)
{
    LOG(("DEBUG: render_content_to_bitmap called"));
    LOG(("DEBUG: context=%p bw=%p", data->context, data->context ? data->context->bw : NULL));
    
    if (!data->context || !data->context->bw || !data->RastPort) {
        LOG(("ERROR: Invalid context, browser, or RastPort"));
        return;
    }

    // Sprawdzenie gotowości content
    struct hlcache_handle *content = browser_window_get_content(data->browser);
    if (!content) {
        LOG(("DEBUG: No content available yet - will retry when ready"));
        data->retry_count++;
        
        if (data->retry_count < 100) {
            mui_queue_method_delay(_app(obj), obj, 50, MM_Browser_Redraw);
        } else {
            LOG(("DEBUG: Timeout waiting for content - giving up"));
            data->retry_count = 0;
        }
        return;
    }
    
    content_status status = content_get_status(content);
    if (status != CONTENT_STATUS_READY && status != CONTENT_STATUS_DONE) {
        LOG(("DEBUG: Content not ready (status=%d) - waiting for callback", status));
        data->retry_count++;
        
        if (data->retry_count < 50) {
            mui_queue_method_delay(_app(obj), obj, 100, MM_Browser_Redraw);
        } else {
            LOG(("DEBUG: Content status timeout"));
            data->retry_count = 0;
        }
        return;
    }
    
    LOG(("DEBUG: Content ready - proceeding with render"));
    data->retry_count = 0;

    if (data->RastPort == NULL) {
        LOG(("ERROR: No RastPort available for off-screen redraw"));
        return;
    }

    float scale = browser_window_get_scale(data->context->bw);
    
    // Get actual content dimensions
    int content_draw_width = content_get_width(content) * scale;
    int content_draw_height = content_get_height(content) * scale;

    LONG vleft = getv(obj, MUIA_Virtgroup_Left);
    LONG vtop = getv(obj, MUIA_Virtgroup_Top);

    // Clamp scroll positions to prevent scrolling beyond content
    LONG max_vleft = content_draw_width - data->bm_width;
    LONG max_vtop = content_draw_height - data->bm_height;
    
    if (max_vleft < 0) max_vleft = 0;
    if (max_vtop < 0) max_vtop = 0;
    
    if (vleft > max_vleft) vleft = max_vleft;
    if (vtop > max_vtop) vtop = max_vtop;
    if (vleft < 0) vleft = 0;
    if (vtop < 0) vtop = 0;
    mui_plot_reset_stats();
    struct mui_tile_flush_ctx flush_ctx = {
        .obj = obj,
        .data = data,
        .flush_ticks = 0,
        .flush_tiles = 0,
        .flush_pixels = 0
    };
    const clock_t redraw_start = clock();
    bool ok = mui_redraw_tiles_surface(data->context,
        data->RastPort,
        data->bm_width,
        data->bm_height,
        vleft,
        vtop,
        flush_tile_to_screen,
        &flush_ctx);
    const clock_t redraw_end = clock();
    const unsigned long redraw_ms = (unsigned long)((redraw_end - redraw_start) * 1000UL / CLOCKS_PER_SEC);
    const unsigned long flush_ms = (unsigned long)((flush_ctx.flush_ticks * 1000UL) / CLOCKS_PER_SEC);

    mui_plot_log_stats("off-screen");

    LOG(("PERF: off-screen redraw %lums (tiles=%lu, pixels=%lu, flush=%lu ms, status=%s)",
        redraw_ms,
        flush_ctx.flush_tiles,
        flush_ctx.flush_pixels,
        flush_ms,
        ok ? "ok" : "fail"));

    if (!ok) {
        LOG(("WARNING: mui_redraw_tiles_surface failed"));
    } else {
		LOG(("DEBUG: Off-screen tiled redraw completed successfully"));
    }
    
    // Fill remaining areas if content is smaller than bitmap
    if (content_draw_width < data->bm_width) {
        SetRast(data->RastPort, 0);
        RectFill(data->RastPort, content_draw_width, 0, data->bm_width - 1, data->bm_height - 1);
    }

    if (content_draw_height < data->bm_height) {
        SetRast(data->RastPort, 0);
        RectFill(data->RastPort, 0, content_draw_height, data->bm_width - 1, data->bm_height - 1);
    }
}

DEFSMETHOD(Browser_GetBitMap)
{
    GETDATA;
    struct RastPort *rp;
    #warning "Browser_GetBitMap is not fully implemented yet, returning 0";
return 0;
    LOG(("DEBUG: Browser_GetBitMap called, width=%d, height=%d", msg->width, msg->height));

    if (msg->width <= 0 || msg->height <= 0) {
        LOG(("ERROR: Invalid dimensions: %dx%d", msg->width, msg->height));
        return 0;
    }

    if (data->changed || data->bm_width != msg->width || data->bm_height != msg->height) {
        delete_offscreen_bitmap(data);
        data->bm_width = msg->width;
        data->bm_height = msg->height;
    }

    rp = data->RastPort;
    if (!rp && muiRenderInfo(obj)) {
        if (!create_offscreen_bitmap(obj, data, data->bm_width, data->bm_height)) {
            LOG(("WARNING: Falling back to window RastPort"));
            rp = _rp(obj);
            if (rp) {
                data->RastPort = rp;
                data->context->RastPort = rp;
                data->context->BitMap = rp->BitMap;
                data->context->Layer = rp->Layer;
            } else {
                LOG(("ERROR: Window RastPort is NULL"));
                return 0;
            }
        }
    }

    LOG(("DEBUG: Browser_GetBitMap returning rp = %p", rp));
    return (IPTR)rp;
}

STATIC VOID doset(APTR obj, struct Data *data, struct TagItem *tags)
{
    struct TagItem *tag, *tstate;
    STRPTR p;

    tstate = tags;
    while ((tag = NextTagItem(&tstate)) != NULL) {
        IPTR tdata = tag->ti_Data;
        switch (tag->ti_Tag) {
            case MA_Browser_Loading:
                data->loading = tdata;
                break;

            case MA_Browser_Pointer:
                if (data->pointertype != tdata) {
                    data->pointertype = tdata;
                    if (mui_supports_pointertype) {
                        // Apply the pointer change via MUI (only supported on MUI 4+)
                        LOG(("DEBUG: Setting pointer type to %ld", tdata));
                        set(obj, MUIA_PointerType, tdata);
                    }
                }
                break;

            case MA_Browser_StatusText:
                data->status_text = (APTR)tdata;
                break;

            case MA_Browser_Title:
                if (data->title) {
                    free(data->title);
                    data->title = NULL;
                }
                p = (APTR)tdata;
                if (p) {
                    p = DupStr(p);
                    if (!p) {
                        LOG(("ERROR: Failed to duplicate title string"));
                    }
                }
                data->title = p;
                break;

            case MA_Browser_URL:
            	LOG(("DEBUG: MA_Browser_URL set to %s", (char *)tdata));
                if (data->url) {
                    free(data->url);
                    data->url = NULL;
                }
                p = (APTR)tdata;
                if (p) {
                    p = DupStr(p);
                    if (!p) {
                        LOG(("ERROR: Failed to duplicate URL string"));
                    }
                }
                data->url = p;
                break;
        }
    }
}

DEFNEW
{
    obj = DoSuperNew(cl, obj,
                     MUIA_Group_LayoutHook, (IPTR)&layoutfunc_hook,
                     MUIA_FillArea, FALSE,
			         MUIA_CustomBackfill, TRUE,
                     InnerSpacing(0, 0),
                     TAG_MORE, msg->ops_AttrList);

    if (obj) {
        GETDATA;
        data->ehnode.ehn_Object = obj;
        data->ehnode.ehn_Class = cl;
        data->ehnode.ehn_Priority = 1;
        data->ehnode.ehn_Flags = MUI_EHF_GUIMODE;

        data->mwidth = 640; // Default for AmigaOS 3.x
        data->mheight = 480;
        data->pointertype = POINTERTYPE_NORMAL;

        data->context = (APTR)FindTagItem(MA_Browser_Context, msg->ops_AttrList)->ti_Data;
        data->browser = data->context ? data->context->bw : NULL;
        data->title_obj = (APTR)FindTagItem(MA_Browser_TitleObj, msg->ops_AttrList)->ti_Data;

        // ===== NOWE: Inicjalizacja retry counter =====
        data->retry_count = 0;
        // ===== KONIEC NOWEGO KODU =====

        LOG(("DEBUG: NEW - data->context = %p", data->context));
        LOG(("DEBUG: NEW - data->context->bw = %p", data->context ? data->context->bw : NULL));
        LOG(("DEBUG: NEW - data->browser = %p", data->browser));

        if (!data->browser) {
            LOG(("ERROR: Invalid browser context"));
            MUI_DisposeObject(obj);
            return 0;
        }
    }

    return (IPTR)obj;
}

DEFDISP
{
    GETDATA;

    if (data->url) {
        free(data->url);
        data->url = NULL;
    }
    if (data->title) {
        free(data->title);
        data->title = NULL;
    }
    delete_offscreen_bitmap(data);

    return DOSUPER;
}

// Reszta kodu pozostaje bez zmian...
// [Tutaj wstaw pozostałe funkcje bez modyfikacji]
DEFMMETHOD(Setup)
{
    GETDATA;
    IPTR rc = DOSUPER;

    if (rc) {
        struct Screen *screen;
        struct RastPort *rp;
        struct BitMap *bm;
        struct ColorMap *cm;
        ULONG depth;

        LOG(("DEBUG: Setup called, checking graphics libraries"));

        if (CyberGfxBase) {
            LOG(("DEBUG: CyberGraphX available"));
        } else {
            LOG(("WARNING: CyberGraphX not available, limited to native chipset"));
        }

        // Bail out early if the window does not have a screen yet (seen on old MUI versions)
        screen = _screen(obj);
        if (!screen) {
            LOG(("ERROR: Setup: _screen(obj) returned NULL, skipping palette setup"));
            return rc;
        }

        rp = &screen->RastPort;
        bm = rp ? rp->BitMap : NULL;
        cm = screen->ViewPort.ColorMap;
        depth = bm ? GetBitMapAttr(bm, BMA_DEPTH) : 0;
        LOG(("DEBUG: Screen depth in Setup: %lu", depth));

        if (depth <= 8 && cm) {  // Only for paletted modes
            if (global_pen_a < 0) {
                global_pen_a = ObtainPen(cm, -1, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, PENF_EXCLUSIVE);  // Initial white
                if (global_pen_a >= 0) {
                    LOG(("DEBUG: Pen A allocated: %ld", global_pen_a));
                } else {
                    LOG(("ERROR: ObtainPen A failed - possible palette full or RTG emulation issue"));
                }
            }
            if (global_pen_b < 0) {
                global_pen_b = ObtainPen(cm, -1, 0, 0, 0, PENF_EXCLUSIVE);  // Initial black
                if (global_pen_b >= 0) {
                    LOG(("DEBUG: Pen B allocated: %ld", global_pen_b));
                } else {
                    LOG(("ERROR: ObtainPen B failed"));
                }
            }
        } else {
            LOG(("DEBUG: Skipping global pen allocation - using fixed pens for hi/truecolor"));
            global_pen_a = -1;  // Force fallback path
            global_pen_b = -1;
        }
    }

    return rc;
}

DEFMMETHOD(Cleanup)
{
    GETDATA;

    struct Screen *screen = _screen(obj);
    struct ColorMap *cm = screen ? screen->ViewPort.ColorMap : NULL;
    if (cm) {
        if (global_pen_a >= 0) {
            ReleasePen(cm, global_pen_a);
            global_pen_a = -1;
        }
        if (global_pen_b >= 0) {
            ReleasePen(cm, global_pen_b);
            global_pen_b = -1;
        }
    } else if (global_pen_a >= 0 || global_pen_b >= 0) {
        LOG(("WARNING: Cleanup: missing ColorMap, cannot release pens (pen_a=%ld, pen_b=%ld)",
            global_pen_a, global_pen_b));
        global_pen_a = -1;
        global_pen_b = -1;
    }

    data->changed = 1;
    delete_offscreen_bitmap(data);

    return DOSUPER;
}

DEFMMETHOD(Show)
{
    ULONG rc;
    GETDATA;

    LOG(("DEBUG: Browser Show called"));
    if ((rc = DOSUPER)) {
        ULONG mwidth = _mwidth(obj);
        ULONG mheight = _mheight(obj);

        LOG(("DEBUG: Show successful, _win(obj) = %p, muiRenderInfo = %p", _win(obj), muiRenderInfo(obj)));

    LOG(("DEBUG: Current offscreen bitmap: %p (%lux%lu)", data->BitMap, data->bm_width, data->bm_height));
        LOG(("DEBUG: data->browser=%p, data->context=%p", data->browser, data->context));

        if (data->mwidth != mwidth || data->mheight != mheight) {
            data->mwidth = mwidth;
            data->mheight = mheight;
            data->changed = 1;
            browser_window_reformat(data->browser, false, mwidth, mheight);
            LOG(("DEBUG: Browser window reformatted to %lux%lu", mwidth, mheight));
        }

        data->ehnode.ehn_Events = IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE;
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehnode);
        LOG(("DEBUG: Show completed successfully"));
    }

    return rc;
}

DEFMMETHOD(Hide)
{
    GETDATA;
    LOG(("DEBUG: Browser Hide called"));
    LOG(("DEBUG: Removing event handler %p from window %p", &data->ehnode, _win(obj)));
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode);
    return DOSUPER;
}

DEFTMETHOD(Browser_Print)
{
    GETDATA;

    LOG(("DEBUG: Browser_Print called"));
    if (data->RastPort) {
        struct IORequest *req = CreateIORequest(NULL, sizeof(struct IODRPReq));
        if (req) {
            if (OpenDevice("printer.device", 0, req, 0) == 0) {
                print_doc(data->RastPort, data->bm_width, data->bm_height);
                CloseDevice(req);
            } else {
                LOG(("ERROR: Failed to open printer.device"));
            }
            DeleteIORequest(req);
        } else {
            LOG(("ERROR: Failed to create IO request for printing"));
        }
    } else {
        LOG(("ERROR: No RastPort available for printing"));
    }

    return 0;
}

// Updated function using the new API
STATIC VOID update_buttons(APTR obj, struct Data *data)
{
    LOG(("DEBUG: update_buttons called"));
    // Use new API browser_window_history_*
    data->back_available = browser_window_history_back_available(data->browser);
    data->forward_available = browser_window_history_forward_available(data->browser);

    SetAttrs(obj,
             MA_Browser_BackAvailable, data->back_available,
             MA_Browser_ForwardAvailable, data->forward_available,
             TAG_DONE);
}

DEFSMETHOD(Browser_SetContentSize)
{
    GETDATA;
    LOG(("DEBUG: Browser_SetContentSize called, width=%d, height=%d", msg->width, msg->height));
    data->content_width = msg->width;
    data->content_height = msg->height;

    data->vleft_old = 0;
    data->vtop_old = 0;

    if (data->mwidth != data->content_width || data->mheight != data->content_height) {
        APTR parent = (APTR)getv(obj, MUIA_Parent);
        data->changed = 1;

        if (parent) {
            DoMethod(parent, MUIM_Group_InitChange);
            DoMethod(parent, MUIM_Group_ExitChange);
        }
    }

    update_buttons(obj, data);

    return SetAttrs(obj, MA_Browser_ReloadAvailable, TRUE, MA_Browser_StopAvailable, TRUE, TAG_DONE);
}

DEFSMETHOD(Browser_SetContentType)
{
    GETDATA;

    LOG(("DEBUG: Browser_SetContentType called, type=%d", msg->type));
    if (msg->type <= CONTENT_CSS) {
        LOG(("DEBUG: Enabling textual content handling"));
        if (data->context && data->context->bw) {
            textinput_enable(data->context->bw);
        }
    } else {
        LOG(("DEBUG: Enabling graphical content handling"));
        if (data->context && data->context->bw) {
            textinput_disable(data->context->bw);
        }
    }

    return 0;
}

// Updated functions using the new API
DEFTMETHOD(Browser_Back)
{
    GETDATA;
    nserror error = browser_window_history_back(data->browser, false);
    if (error != NSERROR_OK) {
        LOG(("ERROR: browser_window_history_back failed: %d", error));
		return 0;
    }
    update_buttons(obj, data);
    return 0;
}

DEFTMETHOD(Browser_Forward)
{
    GETDATA;
    nserror error = browser_window_history_forward(data->browser, false);
    if (error != NSERROR_OK) {
        LOG(("ERROR: browser_window_history_forward failed: %d", error));
		return 0;
    }
    update_buttons(obj, data);
    return 0;
}

DEFTMETHOD(Browser_Reload)
{
    GETDATA;
    LOG(("DEBUG: Browser_Reload called, key_state=%d", data->key_state));
    browser_window_reload(data->browser, data->key_state & BROWSER_MOUSE_MOD_1 ? TRUE : FALSE);
    return 0;
}

DEFTMETHOD(Browser_Stop)
{
    GETDATA;
    LOG(("DEBUG: Browser_Stop called"));
    browser_window_stop(data->browser);
    return 0;
}

DEFSMETHOD(Browser_Find)
{
    GETDATA;
   // start_search(msg->flags & MF_Browser_Find_Previous ? FALSE : TRUE,
      //           msg->flags & MF_Browser_Find_CaseSensitive ? TRUE : FALSE,
     //            msg->string);
    return 0;
}

DEFSMETHOD(Browser_Go)
{
    GETDATA;
    LOG(("DEBUG: Browser_Go: %s", msg->url));
	nsurl *url;
	nserror error;

	error = nsurl_create(msg->url, &url);
    LOG(("DEBUG: Browser_Go: url=%p, error=%d", url, error));
	if (error != NSERROR_OK) {
	    warn_user("Errorcode:", messages_get_errorcode(error));
	} else {
        error = browser_window_navigate(data->browser, url, NULL, BW_NAVIGATE_HISTORY,
                        NULL, NULL, NULL);
        if (error != NSERROR_OK) {
            LOG(("ERROR: browser_window_navigate failed: %d", error));
        }
		nsurl_unref(url);
	}
    return 0;
}
DEFGET
{
    GETDATA;
    //LOG(("DEBUG: Browser_Get called, opg_AttrID=%lu", msg->opg_AttrID));
    
    switch (msg->opg_AttrID) {
        case MA_Browser_BackAvailable:
            //LOG(("DEBUG: Browser_Get MA_Browser_BackAvailable"));
            *msg->opg_Storage = data->back_available;
            //LOG(("DEBUG: Browser_Get MA_Browser_BackAvailable = %s", data->back_available ? "TRUE" : "FALSE"));
            return TRUE;
            
        case MA_Browser_Box:
        {
            //LOG(("DEBUG: Browser_Get MA_Browser_Box"));
            struct IBox *box = (APTR)msg->opg_Storage;
            box->Left = getv(obj, MUIA_Virtgroup_Left);
            box->Top = getv(obj, MUIA_Virtgroup_Top);
            box->Width = data->mwidth;
            box->Height = data->mheight;
            //LOG(("DEBUG: Browser_Get MA_Browser_Box = Left=%d, Top=%d, Width=%d, Height=%d", 
               // box->Left, box->Top, box->Width, box->Height));
            return FALSE;
        }
        
        case MA_Browser_Browser:
            //LOG(("DEBUG: Browser_Get MA_Browser_Browser"));
            *msg->opg_Storage = (IPTR)data->browser;
            //LOG(("DEBUG: Browser_Get MA_Browser_Browser = %p", data->browser));
            return FALSE;
            
        case MA_Browser_ForwardAvailable:
            //LOG(("DEBUG: Browser_Get MA_Browser_ForwardAvailable"));
            *msg->opg_Storage = data->forward_available;
            //LOG(("DEBUG: Browser_Get MA_Browser_ForwardAvailable = %s", data->forward_available ? "TRUE" : "FALSE"));
            return TRUE;
            
        case MA_Browser_Loading:
            //LOG(("DEBUG: Browser_Get MA_Browser_Loading"));
            *msg->opg_Storage = data->loading;
            //LOG(("DEBUG: Browser_Get MA_Browser_Loading = %s", data->loading ? "TRUE" : "FALSE"));
            return TRUE;
            
        case MA_Browser_ReloadAvailable:
            //LOG(("DEBUG: Browser_Get MA_Browser_ReloadAvailable"));
            *msg->opg_Storage = (IPTR)data->reload_available;
            //LOG(("DEBUG: Browser_Get MA_Browser_ReloadAvailable = %s", data->reload_available ? "TRUE" : "FALSE"));
            return TRUE;
            
        case MA_Browser_StatusText:
            //LOG(("DEBUG: Browser_Get MA_Browser_StatusText"));
            *msg->opg_Storage = (IPTR)data->status_text;
            //LOG(("DEBUG: Browser_Get MA_Browser_StatusText = '%s'", data->status_text ? data->status_text : "NULL"));
            return TRUE;
            
        case MA_Browser_StopAvailable:
            //LOG(("DEBUG: Browser_Get MA_Browser_StopAvailable"));
            *msg->opg_Storage = (IPTR)data->stop_available;
            //LOG(("DEBUG: Browser_Get MA_Browser_StopAvailable = %s", data->stop_available ? "TRUE" : "FALSE"));
            return TRUE;
            
        case MA_Browser_Title:
            //LOG(("DEBUG: Browser_Get MA_Browser_Title"));
            *msg->opg_Storage = (IPTR)data->title;
            //LOG(("DEBUG: Browser_Get MA_Browser_Title = '%s'", data->title ? data->title : "NULL"));
            return TRUE;
            
        case MA_Browser_TitleObj:
            //LOG(("DEBUG: Browser_Get MA_Browser_TitleObj"));
            *msg->opg_Storage = (IPTR)data->title_obj;
            //LOG(("DEBUG: Browser_Get MA_Browser_TitleObj = %p", data->title_obj));
            return FALSE;
            
        case MA_Browser_URL:
            //LOG(("DEBUG: Browser_Get MA_Browser_URL"));
            *msg->opg_Storage = (IPTR)data->url;
            //LOG(("DEBUG: Browser_Get MA_Browser_URL = '%s'", data->url ? data->url : "NULL"));
            return TRUE;
            
        default:
            //LOG(("DEBUG: Browser_Get unknown attribute ID=%lu", msg->opg_AttrID));
            break;
    }
    
    //LOG(("DEBUG: Browser_Get calling DOSUPER for attribute ID=%lu", msg->opg_AttrID));
    return DOSUPER;
}

DEFSET
{
    GETDATA;
    doset(obj, data, msg->ops_AttrList);
    return DOSUPER;
}

DEFMMETHOD(HandleEvent)
{
    struct IntuiMessage *imsg = msg->imsg;
    ULONG rc = 0;

    if (imsg) {
        GETDATA;
        ULONG mouse_inside = 0;
        LONG MouseX = imsg->MouseX - _mleft(obj);
        LONG MouseY = imsg->MouseY - _mtop(obj);

        if (imsg->Class == IDCMP_RAWKEY) {
            mouse_inside = data->mouse_inside;
            switch (imsg->Code) {
                case RAWKEY_UP:
                    browser_window_key_press(data->browser, NS_KEY_UP);
                    break;
                case RAWKEY_DOWN:
                    browser_window_key_press(data->browser, NS_KEY_DOWN);
                    break;
                case RAWKEY_LEFT:
                    browser_window_key_press(data->browser, NS_KEY_LEFT);
                    break;
                case RAWKEY_RIGHT:
                    browser_window_key_press(data->browser, NS_KEY_RIGHT);
                    break;
                case RAWKEY_ESCAPE:
                    browser_window_key_press(data->browser, 27);
                    break;
                case RAWKEY_LSHIFT:
                    data->key_state = BROWSER_MOUSE_MOD_1;
                    break;
                case RAWKEY_LSHIFT + 0x80:
                    data->key_state = 0;
                    break;
                case RAWKEY_CONTROL:
                    data->key_state = BROWSER_MOUSE_MOD_2;
                    break;
                case RAWKEY_CONTROL + 0x80:
                    data->key_state = 0;
                    break;
                default:
                    if (imsg->Code < 0x80) {
                        ULONG ucs4 = 0;
                        struct InputEvent ie;
                        TEXT buffer[4];

                        ie.ie_Class = IECLASS_RAWKEY;
                        ie.ie_SubClass = 0;
                        ie.ie_Code = imsg->Code;
                        ie.ie_Qualifier = imsg->Qualifier;
                        ie.ie_EventAddress = NULL;

                        if (MapRawKey(&ie, (STRPTR)&buffer, sizeof(buffer), NULL) == 1) {
                            ucs4 = buffer[0];
                            if (CodesetsBase) {
                                APTR cset = CodesetsFindA("UTF-32", NULL);
                                if (cset) {
                                    ULONG *dst = (APTR)CodesetsConvertStr(CSA_Source, &buffer, CSA_SourceLen, 1, CSA_DestCodeset, cset, TAG_DONE);
                                    if (dst) {
                                        ucs4 = *dst;
                                        CodesetsFreeA(dst, NULL);
                                    }
                                }
                            }
                        }
                        browser_window_key_press(data->browser, ucs4);
                    }
                    break;
            }
            rc = MUI_EventHandlerRC_Eat;
        } else if (imsg->Class == IDCMP_MOUSEBUTTONS) {
            if (MouseX >= 0 && MouseY >= 0 && MouseX < data->mwidth && MouseY < data->mheight) {
                LONG button = imsg->Code & IECODE_LBUTTON ? BROWSER_MOUSE_PRESS_1 : BROWSER_MOUSE_PRESS_2;
                LONG click = imsg->Code & IECODE_LBUTTON ? BROWSER_MOUSE_CLICK_1 : BROWSER_MOUSE_CLICK_2;
                mouse_inside = 1;
                
                // Get and clamp scroll positions for mouse coordinate adjustment
                LONG vleft = getv(obj, MUIA_Virtgroup_Left);
                LONG vtop = getv(obj, MUIA_Virtgroup_Top);
                
                if (data->context && data->context->bw && browser_window_has_content(data->context->bw)) {
                    struct hlcache_handle *content = browser_window_get_content(data->context->bw);
                    if (content) {
                        float scale = browser_window_get_scale(data->context->bw);
                        int content_width = content_get_width(content) * scale;
                        int content_height = content_get_height(content) * scale;
                        
                        LONG max_vleft = content_width - data->mwidth;
                        LONG max_vtop = content_height - data->mheight;
                        
                        if (max_vleft < 0) max_vleft = 0;
                        if (max_vtop < 0) max_vtop = 0;
                        
                        if (vleft > max_vleft) vleft = max_vleft;
                        if (vtop > max_vtop) vtop = max_vtop;
                        if (vleft < 0) vleft = 0;
                        if (vtop < 0) vtop = 0;
                    }
                }
                
                MouseX += vleft;
                MouseY += vtop;

                switch (imsg->Code) {
                    case SELECTDOWN:
                    case MIDDLEDOWN:
                        browser_window_mouse_click(data->browser, button | data->key_state, MouseX, MouseY);
                        data->mouse_state = button;
                        rc = MUI_EventHandlerRC_Eat;
                        break;
                    case SELECTUP:
                    case MIDDLEUP:
                        if (data->mouse_state & button) {
                            browser_window_mouse_click(data->browser, click | data->key_state, MouseX, MouseY);
                        } else {
                            //browser_window_mouse_drag_end(data->browser, 0, MouseX, MouseY);
                        }
                        data->mouse_state = 0;
                        rc = MUI_EventHandlerRC_Eat;
                        break;
                }
            }
        } else if (imsg->Class == IDCMP_MOUSEMOVE) {
            if (MouseX >= 0 && MouseY >= 0 && MouseX < data->mwidth && MouseY < data->mheight) {
                mouse_inside = 1;
                
                // Get and clamp scroll positions for mouse coordinate adjustment
                LONG vleft = getv(obj, MUIA_Virtgroup_Left);
                LONG vtop = getv(obj, MUIA_Virtgroup_Top);
                
                if (data->context && data->context->bw && browser_window_has_content(data->context->bw)) {
                    struct hlcache_handle *content = browser_window_get_content(data->context->bw);
                    if (content) {
                        float scale = browser_window_get_scale(data->context->bw);
                        int content_width = content_get_width(content) * scale;
                        int content_height = content_get_height(content) * scale;
                        
                        LONG max_vleft = content_width - data->mwidth;
                        LONG max_vtop = content_height - data->mheight;
                        
                        if (max_vleft < 0) max_vleft = 0;
                        if (max_vtop < 0) max_vtop = 0;
                        
                        if (vleft > max_vleft) vleft = max_vleft;
                        if (vtop > max_vtop) vtop = max_vtop;
                        if (vleft < 0) vleft = 0;
                        if (vtop < 0) vtop = 0;
                    }
                }
                
                MouseX += vleft;
                MouseY += vtop;

                if (data->mouse_state & BROWSER_MOUSE_PRESS_1) {
                    browser_window_mouse_track(data->browser, BROWSER_MOUSE_DRAG_1 | data->key_state, MouseX, MouseY);
                    data->mouse_state = BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON;
                } else if (data->mouse_state & BROWSER_MOUSE_PRESS_2) {
                    browser_window_mouse_track(data->browser, BROWSER_MOUSE_DRAG_2 | data->key_state, MouseX, MouseY);
                    data->mouse_state = BROWSER_MOUSE_HOLDING_2 | BROWSER_MOUSE_DRAG_ON;
                } else {
                    browser_window_mouse_track(data->browser, data->key_state, MouseX, MouseY);
                }
                rc = MUI_EventHandlerRC_Eat;
            }
        }

        if (data->mouse_inside != mouse_inside) {
            data->mouse_inside = mouse_inside;
        }
    }

    return rc;
}

DEFMMETHOD(Backfill)
{
    GETDATA;
    LOG(("DEBUG: Backfill called"));
    
    if (data->BitMap && data->RastPort) {
        LOG(("DEBUG: Backfill copying cached bitmap"));
        copy_bitmap_to_screen(obj, data);
        return 0;
    }

    if (_rp(obj)) {
        LOG(("DEBUG: Backfill fallback fill"));
        SetAPen(_rp(obj), 0);
        RectFill(_rp(obj), _mleft(obj), _mtop(obj),
                 _mleft(obj) + _mwidth(obj) - 1,
                 _mtop(obj) + _mheight(obj) - 1);
    }

    return 0;
}
DEFMMETHOD(Backfill3)
{
    GETDATA;
    LOG(("DEBUG: Backfill called"));
    
    if (_rp(obj)) {
        // Ustaw kolor wypełnienia (np. biały)
        SetAPen(_rp(obj), 2); // Pen 2 = biały w większości palet
        
        // Lub dla RTG ustaw bezpośrednio RGB:
        if (CyberGfxBase) {
            SetRGBColor(_rp(obj), 0xFFFFFF, FALSE); // Biały
        }
        
        // Wypełnij obszar jednym kolorem
        RectFill(_rp(obj), _mleft(obj), _mtop(obj), 
                 _mleft(obj) + data->mwidth - 1, 
                 _mtop(obj) + data->mheight - 1);
        
        LOG(("DEBUG: Backfilled with solid color"));
    }
    return 0;
}

DEFMMETHOD(Backfill2)
{
    GETDATA;
        LOG(("DEBUG: Backfill called"));
//return;
    //_screen(obj)->RastPort.BitMap,
    if (   data->context->BitMap && data->context->Layer) {
        LOG(("data->context->BitMap = %p data->context->Layer = %p", data->context->BitMap, data->context->Layer));
        LOG(("DEBUG: Backfilling from BitMap to RastPort mleft=%lu mtop=%lu mwidth=%lu mheight=%lu",
             _mleft(obj), _mtop(obj), data->mwidth, data->mheight));
        BltBitMapRastPort(data->BitMap, 0, 0, data->RastPort,
                        _mleft(obj), _mtop(obj),
                        data->mwidth, data->mheight, 0xc0);
    }

    return 0;
}
// ===== ZMODYFIKOWANA FUNKCJA Browser_Redraw =====
DEFTMETHOD(Browser_Redraw)
{
    GETDATA;

    LOG(("DEBUG: Browser_Redraw called"));
    
    // Sprawdź czy content jest dostępny
    if (!browser_window_has_content(data->browser)) {
        data->retry_count++;
        LOG(("DEBUG: No content yet - retry %d/50", data->retry_count));
       
        if (data->retry_count > 50) {
            LOG(("DEBUG: Timeout - stopping redraw loop"));
            data->redraw_pending = 0;
            data->retry_count = 0;
            
            // Wypełnij białym tłem jako fallback
            if (data->RastPort) {
                LOG(("DEBUG: Filling RastPort with white as fallback while waiting for content"));
                SetRast(data->RastPort, 0);
            }
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            return 0;
        }
       
        // Zaplanuj kolejny Browser_Redraw za 100ms
        LOG(("DEBUG: Scheduling next Browser_Redraw in 100ms"));
    mui_queue_method_delay(_app(obj), obj, 50, MM_Browser_Redraw);
//                 1 | MUIV_PushMethod_Delay(100), MM_Browser_Redraw);
        if (data->RastPort) {
            LOG(("DEBUG: Filling RastPort with white while waiting for content 2"));
            SetRast(data->RastPort, 0);
        }
        MUI_Redraw(obj, MADF_DRAWUPDATE);
        return 0;
    }
    
    // Content dostępny - reset retry counter
    data->retry_count = 0;
    LOG(("SUCCESS: Content available - proceeding with render"));
    
    data->redraw_pending = 0;
    
    // Use window size for bitmap - we render only the visible area
    if (!data->BitMap || data->changed || 
        data->bm_width != data->mwidth || data->bm_height != data->mheight) {
        if (create_offscreen_bitmap(obj, data, data->mwidth, data->mheight)) {
            LOG(("DEBUG: Created offscreen bitmap %lux%lu, rendering content", data->mwidth, data->mheight));
            render_content_to_bitmap(obj, data);
            data->changed = 0;
        } else {
            LOG(("WARNING: Using window RastPort for redraw"));
            data->RastPort = _rp(obj);
            if (data->RastPort) {
                render_content_to_bitmap(obj, data);
            }
        }
    } else {
        render_content_to_bitmap(obj, data);
    }

    copy_bitmap_to_screen(obj, data);
    MUI_Redraw(obj, MADF_DRAWUPDATE);

    if (!data->reload_available) {
        data->reload_available = 1;
        set(obj, MA_Browser_ReloadAvailable, TRUE);
    }

    update_buttons(obj, data);
    LOG(("DEBUG: Browser_Redraw completed"));

    return 0;
}

DEFMMETHOD(Draw)
{
    DOSUPER;
    LOG(("DEBUG: Browser_Draw called, flags=%lx", msg->flags));
    if (msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)) {
        GETDATA;

        const LONG vleft = getv(obj, MUIA_Virtgroup_Left);
        const LONG vtop = getv(obj, MUIA_Virtgroup_Top);

        // Check if scrolled
        const bool scrolled = (data->vleft_old != vleft) || (data->vtop_old != vtop);
        
        // Need new render if content changed (but not just scrolled)
        const bool need_new_render =
            data->changed ||
            data->redraw_pending ||
            (data->BitMap == NULL);

        if (scrolled) {
            data->vleft_old = vleft;
            data->vtop_old = vtop;
            
            // Schedule delayed redraw for scroll (250ms delay to allow smooth scrolling)
            if (!data->redraw_pending) {
                data->redraw_pending = 1;
                LOG(("DEBUG: Scheduling delayed redraw after scroll"));
                mui_queue_method_delay(_app(obj), obj, 250, MM_Browser_Redraw);
            }
        } else if (need_new_render) {
            data->changed = 0;
            
            if (!data->redraw_pending) {
                data->redraw_pending = 1;
                LOG(("DEBUG: Scheduling immediate redraw due to content change"));
                DoMethod(_app(obj), MUIM_Application_PushMethod, obj,
                         1, MM_Browser_Redraw);
            }
        }

        // Always show existing bitmap immediately (even if outdated during scroll)
        if (data->BitMap) {
            if (data->RastPort == NULL) {
                LOG(("ERROR: RastPort is NULL, cannot draw"));
                return 0;
            }

            LOG(("DEBUG: Copying cached BitMap to screen"));
            copy_bitmap_to_screen(obj, data);
        }
    }

    return 0;
}

DEFTMETHOD(Browser_CheckContent)
{
    GETDATA;
    
    if (browser_window_has_content(data->browser)) {
        LOG(("DEBUG: Content is now available!"));
        DoMethod(obj, MM_Browser_Redraw);
    } else {
        LOG(("DEBUG: Still no content, checking again..."));
        // Sprawdź ponownie za chwilę
    mui_queue_method_delay(_app(obj), obj, 100, MM_Browser_CheckContent);
    }
    
    return 0;
}

BEGINMTABLE
DECNEW
DECDISP
DECGET
DECSET
DECMMETHOD(Backfill)
DECMMETHOD(Cleanup)
DECMMETHOD(Draw)
DECMMETHOD(Hide)
DECMMETHOD(Setup)
DECMMETHOD(Show)
DECMMETHOD(HandleEvent)
DECSMETHOD(Browser_Back)
DECSMETHOD(Browser_Find)
DECSMETHOD(Browser_Forward)
DECSMETHOD(Browser_GetBitMap)
DECSMETHOD(Browser_Go)
DECSMETHOD(Browser_Print)
DECSMETHOD(Browser_Redraw)
DECSMETHOD(Browser_Reload)
DECSMETHOD(Browser_SetContentSize)
DECSMETHOD(Browser_SetContentType)
DECSMETHOD(Browser_CheckContent)
DECSMETHOD(Browser_Stop)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Virtgroup, browserclass)
