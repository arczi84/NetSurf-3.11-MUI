/*
 * mui/plotters.c
 * NetSurf MUI render functions updated for new NetSurf API
 *
 * Copyright 2009 Ilkka Lehtoranta <ilkleht@isoveli.org>
 * Modifications 2025 by <Your Name> to update to new NetSurf API,
 * fix origin offset, unify font system, and correct color conversion.
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

#include <cybergraphx/cybergraphics.h>
#include <intuition/intuition.h>
#include <graphics/rpattr.h>
#include <graphics/gfx.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#undef NO_INLINE_STDARG
#ifdef TTENGINE
#include <proto/ttengine.h>
#endif
#include <proto/exec.h>
#include <math.h>
#include <stdint.h>

#ifndef MINTERM_SRCMASK
#define MINTERM_SRCMASK (ABC | ABNC | ANBC)
#endif

#include "mui/gui.h"
#include "mui/bitmap.h"
#include "mui/extrasrc.h"
#include "mui/font.h"
#include "mui/plotters.h"
#include "mui/utils.h"
#include "utils/utf8.h"
#include "utils/log0.h"
#include "utils/nsoption.h"
#include "netsurf/css.h"
#include "netsurf/plotters.h"

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftcache.h>
#include <freetype/ftglyph.h>



#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* The bitmap format is ABGR (0xAABBGGRR) */
#define ABGR_TO_ARGB(abgr) \
    (((abgr & 0x000000FF) << 16) /* R */ \
   | ((abgr & 0x0000FF00)      ) /* G */ \
   | ((abgr & 0x00FF0000) >> 16) /* B */)

#define BITMAP_FLAGS BMF_MINPLANES
#define PATT_DOT    0xAAAA
#define PATT_DASH   0xCCCC
#define PATT_LINE   0xFFFF

/* Keep the original global variables that other files expect */
struct RenderInfo renderinfo;

/* Track the most recent clip rectangle so tiling can honour it. */
static struct rect current_clip_rect = { 0, 0, 0, 0 };
static bool current_clip_valid = false;

static inline int
mod_floor_int(int value, int modulus)
{
    if (modulus <= 0) {
        return 0;
    }

    int remainder = value % modulus;
    if (remainder < 0) {
        remainder += modulus;
    }

    return remainder;
}

static inline int
align_start_to_span(int anchor, int tile, int span_min)
{
    if (tile <= 0) {
        return anchor;
    }

    int offset = mod_floor_int(anchor - span_min, tile);
    return span_min - offset;
}

struct bez_point {
    float x;
    float y;
};

static void
mui_blit_alpha_span(struct bitmap *bitmap, int src_x, int src_y,
    int width, int height, struct RastPort *rp, int dest_x, int dest_y,
    PLANEPTR mask_base, UWORD mask_modulo)
{
    if (width <= 0 || height <= 0 || bitmap == NULL || rp == NULL) {
        return;
    }

    size_t row_bytes = (size_t)width * sizeof(uint32_t);
    uint32_t *rowbuffer = AllocVec(row_bytes, MEMF_ANY);
    if (rowbuffer == NULL) {
        return;
    }

    for (int row = 0; row < height; row++) {
        const uint8_t *src_row = (const uint8_t *)bitmap->pixdata +
            (size_t)(src_y + row) * bitmap->modulo;
        const uint8_t *mask_row = NULL;
        if (mask_base != NULL) {
            mask_row = (const uint8_t *)mask_base + (size_t)(src_y + row) * mask_modulo;
        }

        for (int col = 0; col < width; col++) {
            const uint8_t *px = src_row + (size_t)(src_x + col) * 4;
            uint32_t argb = ((uint32_t)px[3] << 24) |
                ((uint32_t)px[0] << 16) |
                ((uint32_t)px[1] << 8) |
                (uint32_t)px[2];

            if (mask_row != NULL) {
                int mask_bit_index = src_x + col;
                uint8_t mask_byte = mask_row[mask_bit_index >> 3];
                uint8_t mask_bit = 7 - (mask_bit_index & 7);
                if ((mask_byte & (1U << mask_bit)) == 0) {
                    argb &= 0x00FFFFFF;
                }
            }

            rowbuffer[col] = argb;
        }

        WritePixelArrayAlpha(rowbuffer, 0, 0, row_bytes, rp,
            dest_x, dest_y + row, width, 1, 0xFFFFFFFF);
    }

    FreeVec(rowbuffer);
}

/* Enhanced color handling functions for MUI plotters */

/* Global pen management */
static LONG global_pen_a = -1, global_pen_b = -1;
static ULONG last_fg_color = 0xFFFFFFFF;
static ULONG last_bg_color = 0xFFFFFFFF;

#include "mui/plot.c"


/* Compatibility function for old API */
void SetColorAB(struct RastPort *rp, ULONG fg, ULONG bg)
{
    SetRGBColor(rp, fg, FALSE);
    SetRGBColor(rp, bg, TRUE);
}

static void mui_arc_gfxlib(struct RastPort *rp, int x, int y, int radius, int angle1, int angle2)
{
    float angle1_r = (float)(angle1) * (M_PI / 180.0);
    float angle2_r = (float)(angle2) * (M_PI / 180.0);
    float angle, b, c;
    float step = 0.1;
    int x0, y0, x1, y1;

    x0 = x;
    y0 = y;
    
    b = angle1_r;
    c = angle2_r;
    
    x1 = (int)(cos(b) * (float)radius);
    y1 = (int)(sin(b) * (float)radius);
    Move(rp, x0 + x1, y0 - y1);
        
    for (angle = (b + step); angle <= c; angle += step) {
        x1 = (int)(cos(angle) * (float)radius);
        y1 = (int)(sin(angle) * (float)radius);
        Draw(rp, x0 + x1, y0 - y1);
    }
}

static void mui_bezier(struct bez_point *restrict a, struct bez_point *restrict b,
                       struct bez_point *restrict c, struct bez_point *restrict d,
                       float t, struct bez_point *restrict p) 
{
    p->x = pow((1 - t), 3) * a->x + 3 * t * pow((1 - t), 2) * b->x + 
           3 * (1 - t) * pow(t, 2) * c->x + pow(t, 3) * d->x;
    p->y = pow((1 - t), 3) * a->y + 3 * t * pow((1 - t), 2) * b->y + 
           3 * (1 - t) * pow(t, 2) * c->y + pow(t, 3) * d->y;
}

/**
 * \brief Sets a clip rectangle for subsequent plot operations.
 */
static nserror
mui_clip(const struct redraw_context *ctx, const struct rect *clip)
{
    //LOG(("[mui_plotter] Entered mui_clip(%d, %d, %d, %d)", clip->x0, clip->y0, clip->x1, clip->y1));
    
    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp || !rp->Layer) {
        //LOG(("[mui_plotter] mui_clip: Invalid RastPort or Layer"));
        return NSERROR_INVALID;
    }

    //LOG(("[mui_plotter] mui_clip: RastPort OK, creating Region"));
    
    static struct Rectangle R;
    struct Region *reg = InstallClipRegion(rp->Layer, NULL);
    
    if (!reg) {
        reg = NewRegion(); 
    } else {
        ClearRectRegion(reg, &R);
    }
    
    R.MinX = clip->x0;
    R.MinY = clip->y0;
    R.MaxX = clip->x1 - 1;
    R.MaxY = clip->y1 - 1;
    current_clip_rect = *clip;
    current_clip_valid = true;
    
    OrRectRegion(reg, &R);
    reg = InstallClipRegion(rp->Layer, reg);
    
    if (reg) {
        DisposeRegion(reg);
    }
    
    return NSERROR_OK;
}

/**
 * Plots an arc
 */
static nserror
mui_arc(const struct redraw_context *ctx,
        const plot_style_t *style,
        int x, int y, int radius, int angle1, int angle2)
{
    LOG(("[mui_plotter] Entered mui_arc()"));

    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp) return NSERROR_INVALID;

    if (angle2 < angle1) {
        angle2 += 360;
    }

    ULONG pixel = ConvertNetSurfColor(style->fill_colour);
    SetRGBColor(rp, pixel, FALSE);
    mui_arc_gfxlib(rp, x, y, radius, angle1, angle2);

    return NSERROR_OK;
}

/**
 * Plots a circle
 */
static nserror
mui_disc(const struct redraw_context *ctx,
         const plot_style_t *style,
         int x, int y, int radius)
{
    LOG(("[mui_plotter] Entered mui_disc()"));

    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp) return NSERROR_INVALID;

    if (style->fill_type != PLOT_OP_TYPE_NONE) {
        ULONG pixel = ConvertNetSurfColor(style->fill_colour);
        SetRGBColor(rp, pixel, FALSE);
        
        // Zamiast AreaCircle, użyj DrawEllipse z wypełnieniem
        // Lub narysuj okrąg punktami/liniami
        for (int i = 0; i < radius; i++) {
            DrawEllipse(rp, x, y, i, i);
        }
    }

    if (style->stroke_type != PLOT_OP_TYPE_NONE) {
        ULONG pixel = ConvertNetSurfColor(style->stroke_colour);
        SetRGBColor(rp, pixel, FALSE);
        DrawEllipse(rp, x, y, radius, radius);
    }

    return NSERROR_OK;
}

/**
 * Plots a line
 */
static nserror
mui_line(const struct redraw_context *ctx,
         const plot_style_t *style,
         const struct rect *line)
{
    LOG(("[mui_plotter] Entered mui_line()"));

    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp) return NSERROR_INVALID;

    rp->PenWidth = plot_style_fixed_to_int(style->stroke_width);
    rp->PenHeight = plot_style_fixed_to_int(style->stroke_width);

    switch (style->stroke_type) {
        case PLOT_OP_TYPE_SOLID:
        default:
            rp->LinePtrn = PATT_LINE;
            break;

        case PLOT_OP_TYPE_DOT:
            rp->LinePtrn = PATT_DOT;
            break;

        case PLOT_OP_TYPE_DASH:
            rp->LinePtrn = PATT_DASH;
            break;
    }

    ULONG pixel = ConvertNetSurfColor(style->stroke_colour);
    SetRGBColor(rp, pixel, FALSE);
    Move(rp, line->x0, line->y0);
    Draw(rp, line->x1, line->y1);

    rp->PenWidth = 1;
    rp->PenHeight = 1;
    rp->LinePtrn = PATT_LINE;

    return NSERROR_OK;
}

/*static bool mui_fill(int x0, int y0, int x1, int y1, colour col)
{
	LOG(("[mui_plotter] Entered mui_fill()"));
    struct RastPort *rp = renderinfo.rp;
    if (!rp) return false;
    x0+=DX; y0+=DY; x1+=DX; y1+=DY;
    ULONG pixel = ConvertNetSurfColor(col);
    ULONG depth = GetBitMapAttr(rp->BitMap, BMA_DEPTH);
    if (depth>8 && CyberGfxBase) {
        if (FillPixelArray(rp, x0, y0, x1-x0, y1-y0, pixel)==-1) {
            SetRGBColor(rp,pixel,FALSE);
            RectFill(rp,x0,y0,x1-1,y1-1);
        }
    } else {
        SetRGBColor(rp,pixel,FALSE);
        RectFill(rp,x0,y0,x1-1,y1-1);
    }
    return true;
}*/

/**
 * Plot a polygon
 */
static nserror
mui_polygon(const struct redraw_context *ctx,
            const plot_style_t *style,
            const int *p,
            unsigned int n)
{
    LOG(("[mui_plotter] Entered mui_polygon()"));

    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp || n < 3) return NSERROR_INVALID;

    ULONG pixel = ConvertNetSurfColor(style->fill_colour);
    SetRGBColor(rp, pixel, FALSE);

    if (AreaMove(rp, p[0], p[1]) == -1) {
        LOG(("AreaMove: vector list full"));
    }

    for (uint32_t k = 1; k < n; k++) {
        if (AreaDraw(rp, p[k*2], p[(k*2)+1]) == -1) {
            LOG(("AreaDraw: vector list full"));
        }
    }

    if (AreaEnd(rp) == -1) {
        LOG(("AreaEnd: error"));
    }

    return NSERROR_OK;
}

/**
 * Plots a path.
 */
static nserror
mui_path(const struct redraw_context *ctx,
         const plot_style_t *pstyle,
         const float *p,
         unsigned int n,
         const float transform[6])
{
    unsigned int i;
    struct bez_point start_p = {0, 0}, cur_p = {0, 0}, p_a, p_b, p_c, p_r;

    LOG(("[mui_plotter] Entered mui_path()"));

    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp || n == 0) return NSERROR_INVALID;

    if (p[0] != PLOTTER_PATH_MOVE) {
        LOG(("Path does not start with move"));
        return NSERROR_INVALID;
    }

    if (pstyle->fill_colour != NS_TRANSPARENT) {
        ULONG pixel = ConvertNetSurfColor(pstyle->fill_colour);
        SetRGBColor(rp, pixel, FALSE);
        if (pstyle->stroke_colour != NS_TRANSPARENT) {
            ULONG stroke_pixel = ConvertNetSurfColor(pstyle->stroke_colour);
            SetRGBColor(rp, stroke_pixel, TRUE);
        }
    } else {
        if (pstyle->stroke_colour != NS_TRANSPARENT) {
            ULONG pixel = ConvertNetSurfColor(pstyle->stroke_colour);
            SetRGBColor(rp, pixel, FALSE);
        } else {
            return NSERROR_OK;
        }
    }

    /* Construct path */
    for (i = 0; i < n; ) {
        if (p[i] == PLOTTER_PATH_MOVE) {
            if (pstyle->fill_colour != NS_TRANSPARENT) {
                if (AreaMove(rp, p[i+1], p[i+2]) == -1) {
                    LOG(("AreaMove: vector list full"));
                }
            } else {
                Move(rp, p[i+1], p[i+2]);
            }
            start_p.x = p[i+1];
            start_p.y = p[i+2];
            cur_p.x = start_p.x;
            cur_p.y = start_p.y;
            i += 3;
        } else if (p[i] == PLOTTER_PATH_CLOSE) {
            if (pstyle->fill_colour != NS_TRANSPARENT) {
                if (AreaEnd(rp) == -1) {
                    LOG(("AreaEnd: error"));
                }
            } else {
                Draw(rp, start_p.x, start_p.y);
            }
            i++;
        } else if (p[i] == PLOTTER_PATH_LINE) {
            if (pstyle->fill_colour != NS_TRANSPARENT) {
                if (AreaDraw(rp, p[i+1], p[i+2]) == -1) {
                    LOG(("AreaDraw: vector list full"));
                }
            } else {
                Draw(rp, p[i+1], p[i+2]);
            }
            cur_p.x = p[i+1];
            cur_p.y = p[i+2];
            i += 3;
        } else if (p[i] == PLOTTER_PATH_BEZIER) {
            p_a.x = p[i+1];
            p_a.y = p[i+2];
            p_b.x = p[i+3];
            p_b.y = p[i+4];
            p_c.x = p[i+5];
            p_c.y = p[i+6];

            for (float t = 0.0; t <= 1.0; t += 0.1) {
                mui_bezier(&cur_p, &p_a, &p_b, &p_c, t, &p_r);
                if (pstyle->fill_colour != NS_TRANSPARENT) {
                    if (AreaDraw(rp, p_r.x, p_r.y) == -1) {
                        LOG(("AreaDraw: vector list full"));
                    }
                } else {
                    Draw(rp, p_r.x, p_r.y);
                }
            }
            cur_p.x = p_c.x;
            cur_p.y = p_c.y;
            i += 7;
        } else {
            LOG(("bad path command %f", p[i]));
            return NSERROR_INVALID;
        }
    }

    return NSERROR_OK;
}

/**
 * Plot a bitmap
 */
static nserror
mui_bitmap_tile(const struct redraw_context *ctx,
                struct bitmap *bitmap,
                int x, int y,
                int width,
                int height,
                colour bg,
                bitmap_flags_t flags)
{
    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp) {
        return NSERROR_INVALID;
    }

    bool repeat_x = (flags & BITMAPF_REPEAT_X);
    bool repeat_y = (flags & BITMAPF_REPEAT_Y);

    LOG(("[mui_plotter] Entered mui_bitmap_tile()"));

    if ((width <= 0) || (height <= 0) || (bitmap == NULL)) {
        return NSERROR_OK;
    }

    /* Ensure we have a native bitmap cache for this surface. */
    struct BitMap *src = bitmap->bitmap;
    if (!src) {
        src = AllocBitMap(bitmap->width, bitmap->height, 32, BITMAP_FLAGS, rp->BitMap);
        bitmap->bitmap = src;
        bitmap->update = 1;
    }

    if (bitmap->update) {
        struct RastPort trp;
        InitRastPort(&trp);
        trp.BitMap = src;
        WritePixelArray(bitmap->pixdata, 0, 0, bitmap->modulo, &trp, 0, 0,
                        bitmap->width, bitmap->height, RECTFMT_RGBA);
        bitmap->update = 0;
        bitmap->mask_valid = FALSE;
    }

    PLANEPTR mask_base = NULL;
    UWORD mask_modulo = 0;
    if (!bitmap->opaque) {
        mask_base = mui_bitmap_get_mask(bitmap, src, &mask_modulo);
    }

    int src_w = bitmap->width;
    int src_h = bitmap->height;
    if (src_w <= 0 || src_h <= 0) {
        return NSERROR_OK;
    }

    int clip_min_x;
    int clip_min_y;
    int clip_max_x;
    int clip_max_y;

    if (current_clip_valid) {
        clip_min_x = current_clip_rect.x0;
        clip_min_y = current_clip_rect.y0;
        clip_max_x = current_clip_rect.x1;
        clip_max_y = current_clip_rect.y1;
    } else {
        clip_min_x = renderinfo.origin_x;
        clip_min_y = renderinfo.origin_y;
        clip_max_x = renderinfo.origin_x + (int)renderinfo.width;
        clip_max_y = renderinfo.origin_y + (int)renderinfo.height;
    }

    if (clip_max_x <= clip_min_x || clip_max_y <= clip_min_y) {
        return NSERROR_OK;
    }

    int tile_w = width;
    int tile_h = height;

    int span_min_x = repeat_x ? clip_min_x : MAX(clip_min_x, x);
    int span_min_y = repeat_y ? clip_min_y : MAX(clip_min_y, y);
    int span_max_x = repeat_x ? clip_max_x : MIN(clip_max_x, x + tile_w);
    int span_max_y = repeat_y ? clip_max_y : MIN(clip_max_y, y + tile_h);

    if (span_max_x <= span_min_x || span_max_y <= span_min_y) {
        return NSERROR_OK;
    }

    int start_x = repeat_x ? align_start_to_span(x, tile_w, span_min_x) : x;
    int start_y = repeat_y ? align_start_to_span(y, tile_h, span_min_y) : y;

    for (int dest_y = span_min_y; dest_y < span_max_y; ) {
        int src_y = mod_floor_int(dest_y - y, tile_h);
        int copy_h = tile_h - src_y;
        if (copy_h > span_max_y - dest_y) {
            copy_h = span_max_y - dest_y;
        }

        if (src_h > 0) {
            src_y = mod_floor_int(src_y, src_h);
            if (copy_h > src_h - src_y) {
                copy_h = src_h - src_y;
            }
        }

        if (copy_h <= 0) {
            dest_y++;
            continue;
        }

        for (int dest_x = span_min_x; dest_x < span_max_x; ) {
            int src_x = mod_floor_int(dest_x - x, tile_w);
            int copy_w = tile_w - src_x;
            if (copy_w > span_max_x - dest_x) {
                copy_w = span_max_x - dest_x;
            }

            if (src_w > 0) {
                src_x = mod_floor_int(src_x, src_w);
                if (copy_w > src_w - src_x) {
                    copy_w = src_w - src_x;
                }
            }

            if (copy_w <= 0) {
                dest_x++;
                continue;
            }

            if (mask_base != NULL) {
                mui_blit_alpha_span(bitmap, src_x, src_y, copy_w, copy_h,
                    rp, dest_x, dest_y, mask_base, mask_modulo);
            } else {
                BltBitMapRastPort(src, src_x, src_y, rp, dest_x, dest_y,
                    copy_w, copy_h, 0xC0);
            }

            dest_x += copy_w;
        }

        dest_y += copy_h;
    }

    return NSERROR_OK;
}

static nserror
mui_text2(const struct redraw_context *ctx,
         const struct plot_font_style *fstyle,
         int x,
         int y,
         const char *text,
         size_t length)
{
    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp || !text || length == 0) return NSERROR_OK;
    
    /* Open appropriate font */
    APTR font_node = mui_open_font(rp, fstyle);
    if (!font_node) {
        LOG(("WARNING: No font available, using default"));
        /* Fallback rendering with color support */
        ULONG pixel = ConvertNetSurfColor(fstyle->foreground);
        SetRGBColor(rp, pixel, FALSE);
        
        /* Handle background color if specified */
        if (fstyle->background != NS_TRANSPARENT) {
            ULONG bg_pixel = ConvertNetSurfColor(fstyle->background);
            SetRGBColor(rp, bg_pixel, TRUE);
            SetDrMd(rp, JAM2); /* Use both foreground and background */
        } else {
            SetDrMd(rp, JAM1); /* Use only foreground */
        }
        
        int baseline_y = y + (rp->Font ? rp->Font->tf_Baseline : 12) - 13;
        Move(rp, x, baseline_y);
        Text(rp, text, length);
        
        SetDrMd(rp, JAM1); /* Reset to default */
        return NSERROR_OK;
    }
    
    struct fontnode *node = (struct fontnode *)font_node;
    UBYTE oldDm = GetDrMd(rp);
    
    /* Set up drawing mode based on background */
    if (fstyle->background != NS_TRANSPARENT) {
        ULONG bg_pixel = ConvertNetSurfColor(fstyle->background);
        SetRGBColor(rp, bg_pixel, TRUE);
        SetDrMd(rp, JAM2); /* Use both foreground and background */
    } else {
        SetDrMd(rp, JAM1); /* Use only foreground */
    }
    
    /* Set foreground color */
    ULONG pixel = ConvertNetSurfColor(fstyle->foreground);
    SetRGBColor(rp, pixel, FALSE);
    
    bool ok = false;
    
    /* Try system font rendering */
    if (!ok && node->sysfont) {
        struct converted_text converted;
        mui_prepare_converted_text(text, length, &converted);

        int baseline_y = y + node->sysfont->tf_Baseline - 11;
        Move(rp, x, baseline_y);
        Text(rp, converted.text, converted.length);

        mui_destroy_converted_text(&converted);
        ok = true;
    }
    
    /* Fallback to default font */
    if (!ok && rp->Font) {
        int baseline_y = y + rp->Font->tf_Baseline - 13;
        Move(rp, x, baseline_y);
        Text(rp, text, length);
        ok = true;
    }
    
    /* Restore drawing mode */
    SetDrMd(rp, oldDm);
    
    /* Close font */
    mui_close_font(rp, font_node);
    
    return ok ? NSERROR_OK : NSERROR_INVALID;
}

/* Add initialization function to be called at startup */
nserror mui_plotters_init2(void)
{
    /* Initialize global variables */
    last_fg_color = 0xFFFFFFFF;
    last_bg_color = 0xFFFFFFFF;
    global_pen_a = -1;
    global_pen_b = -1;
    
    /* Initialize pens */
    InitializePens();
    
    return NSERROR_OK;
}
/* Cleanup function */
void mui_plotters_fini2(void)
{
    if (renderinfo.screen && renderinfo.screen->ViewPort.ColorMap) {
        struct ColorMap *cm = renderinfo.screen->ViewPort.ColorMap;
        if (global_pen_a >= 0) {
            ReleasePen(cm, global_pen_a);
            global_pen_a = -1;
        }
        if (global_pen_b >= 0) {
            ReleasePen(cm, global_pen_b);
            global_pen_b = -1;
        }
    }
}
static nserror
mui_text1(const struct redraw_context *ctx,
         const struct plot_font_style *fstyle,
         int x,
         int y,
         const char *text,
         size_t length)
{
    LOG(("[mui_plotter] Entered mui_text()"));
    
    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp || !text || length == 0) return NSERROR_OK;
    
    /* Open appropriate font */
    APTR font_node = mui_open_font(rp, fstyle);
    if (!font_node) {
        LOG(("WARNING: No font available, using default"));
        /* Fallback to simple rendering */
        ULONG pixel = ConvertNetSurfColor(fstyle->foreground);
        SetRGBColor(rp, pixel, FALSE);
        
        /* Użyj korekcji -13 jak w starej wersji dla fallback */
        int baseline_y = y + (rp->Font ? rp->Font->tf_Baseline : 12) - 13;
        Move(rp, x, baseline_y);
        Text(rp, text, length);
        return NSERROR_OK;
    }
    
    struct fontnode *node = (struct fontnode *)font_node;
    UBYTE oldDm = GetDrMd(rp);
    SetDrMd(rp, JAM1);
    
    /* Set text color */
    ULONG pixel = ConvertNetSurfColor(fstyle->foreground);
    SetRGBColor(rp, pixel, FALSE);
    
    bool ok = false;
    
    /* Use system font rendering */
    if (!ok && node->sysfont) {
        LOG(("DEBUG: Using system font rendering"));
        
        struct converted_text rendered;
        mui_prepare_converted_text(text, length, &rendered);

        /* Użyj tej samej korekcji co w starej wersji: baseline - 11 */
        int baseline_y = y + node->sysfont->tf_Baseline - 11;
        
        LOG(("DEBUG: Rendering text '%.*s' at (%d,%d), baseline=%d, correction=-11", 
             (int)rendered.length, rendered.text, x, baseline_y, node->sysfont->tf_Baseline));
        
        Move(rp, x, baseline_y);
        Text(rp, rendered.text, rendered.length);

        mui_destroy_converted_text(&rendered);
        ok = true;
    }
    
    /* Final fallback to default RastPort font */
    if (!ok && rp->Font) {
        LOG(("DEBUG: Using default RastPort font"));
        
        /* Użyj tej samej korekcji co w starej wersji: baseline - 13 */
        int baseline_y = y + rp->Font->tf_Baseline - 13;
        
        Move(rp, x, baseline_y);
        Text(rp, text, length);
        ok = true;
    }
    
    /* Restore draw mode */
    SetDrMd(rp, oldDm);
    
    /* Close font */
    mui_close_font(rp, font_node);
    
    if (!ok) {
        LOG(("ERROR: All text rendering methods failed"));
        return NSERROR_INVALID;
    }
    
    return NSERROR_OK;
}
/* Debug renderinfo and screen fallback */
void debug_renderinfo_init(void)
{
    LOG(("=== RENDERINFO DEBUG ==="));
    LOG(("renderinfo.rp = %p", renderinfo.rp));
    LOG(("renderinfo.screen = %p", renderinfo.screen));
    LOG(("renderinfo.width = %d", renderinfo.width));
    LOG(("renderinfo.height = %d", renderinfo.height));
    LOG(("renderinfo.origin_x = %d", renderinfo.origin_x));
    LOG(("renderinfo.origin_y = %d", renderinfo.origin_y));
}

static nserror mui_flush(const struct redraw_context *ctx)
{
    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp || !rp->BitMap) {
        LOG(("mui_flush: No valid RastPort"));
        return NSERROR_OK;
    }
    
    LOG(("mui_flush: Starting flush"));
    
    /* Sprawdź czy to off-screen bitmap */
    ULONG flags = GetBitMapAttr(rp->BitMap, BMA_FLAGS);
    if (!(flags & BMF_DISPLAYABLE)) {
        LOG(("mui_flush: OFF-SCREEN bitmap - need to copy to screen"));
        
        /* Spróbuj znaleźć docelowy screen/window RastPort */
        if (rp->Layer && rp->Layer->Window) {
            struct Window *win = rp->Layer->Window;
            struct RastPort *screen_rp = win->RPort;
            
            LOG(("mui_flush: Found window RastPort %p", screen_rp));
            
            /* Kopiuj cały off-screen buffer na ekran */
            ULONG width = GetBitMapAttr(rp->BitMap, BMA_WIDTH);
            ULONG height = GetBitMapAttr(rp->BitMap, BMA_HEIGHT);
            
            LOG(("mui_flush: Copying %lux%lu bitmap to screen", width, height));
            
            /* Użyj BltBitMapRastPort żeby skopiować */
            BltBitMapRastPort(rp->BitMap, 0, 0, screen_rp, 
                             win->BorderLeft + DX, win->BorderTop + DY, 
                             width, height, 0xC0); /* COPY operation */
            
            LOG(("mui_flush: Bitmap copied to window"));
        } else {
            LOG(("mui_flush: No window found for off-screen bitmap"));
        }
    } else {
        LOG(("mui_flush: On-screen bitmap - no copy needed"));
    }
    
    /* Wymuszaj synchronizację */
    WaitBlit();
    WaitTOF();
    
    LOG(("mui_flush: Flush completed"));
    return NSERROR_OK;
}


const struct plotter_table muiplot = {
    .rectangle = mui_rectangle,
    .line = mui_line,
    .polygon = mui_polygon,
    .clip = mui_clip,
    .text = mui_text,
    .disc = mui_disc,
    .arc = mui_arc,
    .bitmap = mui_bitmap_tile,
    .path = mui_path,
    .flush = mui_flush,
    .option_knockout = true,
};
