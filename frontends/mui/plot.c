/* Enhanced color handling functions for MUI plotters - NetSurf 3.11 compatible */
/* Fixed version for AmigaOS 3.x - resolves background/foreground color issues */
/* NEW API plotter table */

#include "netsurf/plot_style.h"  /* For plot_style_t and NS_TRANSPARENT */
#include "mui/font.h"

/* Pattern definitions for line styles */
#ifndef PATT_LINE
#define PATT_LINE   0xFFFF
#endif
#ifndef PATT_DOT  
#define PATT_DOT    0xAAAA
#endif
#ifndef PATT_DASH
#define PATT_DASH   0xCCCC
#endif

/* Missing definition compatibility */
#ifndef NS_TRANSPARENT
#define NS_TRANSPARENT 0x01000000
#endif

bool plot_initiated = false;
void debug_screen_capabilities(void);

/* Fixed color conversion - handles NetSurf ABGR format properly */
static ULONG ConvertNetSurfColor(colour ns)
{
    if (!plot_initiated) {
        plot_initiated = true;
    }

    /* NetSurf uses ABGR format (0xAABBGGRR), convert to ARGB for Amiga */
    UBYTE a = (ns >> 24) & 0xFF;
    UBYTE b = (ns >> 16) & 0xFF; 
    UBYTE g = (ns >> 8) & 0xFF;
    UBYTE r = ns & 0xFF;
    
    /* Convert to ARGB format for Amiga systems */
    ULONG result = (a << 24) | (r << 16) | (g << 8) | b;
    
    /* Debug log for color conversion verification */
    LOG(("ConvertNetSurfColor: input=0x%08lX -> A=%d R=%d G=%d B=%d -> output=0x%08lX", 
         ns, a, r, g, b, result));
    
    return result;
}

/* Alternative function for RTG systems */
static ULONG GetDirectPixel(colour ns)
{
    /* NetSurf uses ABGR format, RTG expects RGB */
    UBYTE a = (ns >> 24) & 0xFF;
    UBYTE b = (ns >> 16) & 0xFF; 
    UBYTE g = (ns >> 8) & 0xFF;
    UBYTE r = ns & 0xFF;
    
    /* Return in RGB format for CyberGraphX */
    return (r << 16) | (g << 8) | b;
}

/* Initialize pen allocation */
bool InitializePens(void)
{
    if (!renderinfo.screen || !renderinfo.screen->ViewPort.ColorMap) {
        global_pen_a = 1;
        global_pen_b = 0;
        return true;
    }
    
    struct ColorMap *cm = renderinfo.screen->ViewPort.ColorMap;
    if (global_pen_a < 0) {
        global_pen_a = ObtainPen(cm, -1, 0, 0, 0, PEN_EXCLUSIVE);
    }
    if (global_pen_b < 0) {
        global_pen_b = ObtainPen(cm, -1, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, PEN_EXCLUSIVE);
    }
    
    if (global_pen_a < 0) global_pen_a = 1;
    if (global_pen_b < 0) global_pen_b = 0;
    
    return true;
}

static BOOL screen_initialized = FALSE;

/* Function for lazy-loading screen - based on original fix_renderinfo_screen() */
static BOOL ensure_screen_available(struct RastPort *rp)
{
    if (renderinfo.screen && screen_initialized) {
        return TRUE; /* Already have screen */
    }
    
    LOG(("ensure_screen_available: Trying to get screen from RastPort"));
    
    /* Try to get screen from RastPort layer/window if available */
    if (rp && rp->Layer) {
        struct Layer *layer = rp->Layer;
        
        /* Method 1: Try to get screen from window if layer has one */
        if (layer->Window) {
            struct Window *win = (struct Window *)layer->Window;
            renderinfo.screen = win->WScreen;
            LOG(("Got screen from Window->WScreen: %p", renderinfo.screen));
        }
    }
    
    /* Method 2: Try default public screen */
    if (!renderinfo.screen) {
        struct Screen *scr = LockPubScreen(NULL); /* Default public screen */
        if (scr) {
            renderinfo.screen = scr;
            UnlockPubScreen(NULL, scr);
            LOG(("Got default public screen: %p", renderinfo.screen));
        }
    }
    
    /* Method 3: Workbench screen as final fallback (like original code) */
    if (!renderinfo.screen) {
        renderinfo.screen = LockPubScreen("Workbench");
        if (renderinfo.screen) {
            LOG(("Got Workbench screen: %p", renderinfo.screen));
            /* Don't UnlockPubScreen here - keep the reference like original */
        }
    }
    
    if (renderinfo.screen) {
        screen_initialized = TRUE;
        LOG(("Screen initialized successfully"));
        debug_screen_capabilities();
        return TRUE;
    }
    
    LOG(("ERROR: Could not get screen"));
    return FALSE;
}

/* Enhanced fallback color mapping with better NetSurf color handling */
static LONG get_fallback_pen(ULONG rgb_color, struct ViewPort *vp) 
{
    UBYTE r = (rgb_color >> 16) & 0xFF;
    UBYTE g = (rgb_color >> 8) & 0xFF;
    UBYTE b = rgb_color & 0xFF;
    
    LOG(("get_fallback_pen: analyzing color r=%d g=%d b=%d", r, g, b));
    
    /* Precise matches for commonly used NetSurf colors */
    switch (rgb_color & 0x00FFFFFF) { /* Mask out alpha */
        case 0x000000: return 0; /* Black */
        case 0xFFFFFF: return 1; /* White */
        case 0xFF0000: return 2; /* Red */
        case 0x00FF00: return 3; /* Green */  
        case 0x0000FF: return 4; /* Blue */
        case 0xFFFF00: return 5; /* Yellow */
        case 0xFF00FF: return 6; /* Magenta */
        case 0x00FFFF: return 7; /* Cyan */
        case 0x808080: return 8; /* Gray */
    }
    
    /* Special mappings for NetSurf CSS colors - after proper conversion */
    ULONG pure_color = rgb_color & 0x00FFFFFF;
    
    /* Gray levels - better mapping */
    if (r == g && g == b) { /* This is gray */
        if (r < 32) return 0;   /* Very dark -> black */
        if (r < 64) return 8;   /* Dark gray */  
        if (r < 96) return 9;   /* Medium dark gray */
        if (r < 128) return 10; /* Medium gray */
        if (r < 160) return 11; /* Light gray */
        if (r < 224) return 12; /* Very light gray */
        return 1; /* Very light -> white */
    }
    
    /* Primary colors by dominant component */
    if (r > g + 40 && r > b + 40) {
        if (r > 200) return 2; /* Red */
        if (r > 128) return 19; /* Dark red */
        return 20; /* Very dark red */
    }
    
    if (g > r + 40 && g > b + 40) {
        if (g > 200) return 3; /* Green */
        if (g > 128) return 21; /* Dark green */
        return 22; /* Very dark green */
    }
    
    if (b > r + 40 && b > g + 40) {
        if (b > 200) return 4; /* Blue */
        if (b > 128) return 23; /* Dark blue */
        return 24; /* Very dark blue */
    }
    
    /* Mixed colors */
    if (r > 200 && g > 200 && b < 100) return 5; /* Yellow */
    if (r > 200 && b > 200 && g < 100) return 6; /* Magenta */
    if (g > 200 && b > 200 && r < 100) return 7; /* Cyan */
    
    /* Default fallback based on intensity */
    ULONG intensity = (r + g + b);
    if (intensity < 128) return 0; /* Dark -> black */
    if (intensity > 600) return 1; /* Light -> white */
    
    return 8; /* Medium gray for unknown colors */
}



void debug_screen_capabilities(void);


/* Get best pen from RGB color using ObtainBestPen with fallback */
UWORD get_pen_from_rgb(struct ColorMap *cmap, ULONG rgb) {
    UBYTE r = (rgb >> 16) & 0xFF;
    UBYTE g = (rgb >> 8) & 0xFF;
    UBYTE b = rgb & 0xFF;


    ULONG r32 = r << 24;
    ULONG g32 = g << 24;
    ULONG b32 = b << 24;


    UWORD pen = ObtainBestPen(cmap, r32, g32, b32,
    OBP_Precision, PRECISION_IMAGE,
    TAG_DONE);


    if (pen == (UWORD)-1) return 1;

    return pen;
}


/* Use get_pen_from_rgb instead of SetRPAttrs (not available in OS3) */
/*static*/ VOID SetRGBColor(struct RastPort *rp, ULONG rgb_color, BOOL is_background) {
    if (!rp || !renderinfo.screen) return;

    struct ColorMap *cmap = renderinfo.screen->ViewPort.ColorMap;

    if (!cmap) return;

    UWORD pen = get_pen_from_rgb(cmap, rgb_color);
    if (is_background) SetBPen(rp, pen);
    else SetAPen(rp, pen);
}

/* Enhanced debug function */
void debug_screen_capabilities(void)
{
    LOG(("=== ENHANCED SCREEN CAPABILITIES DEBUG ==="));
    LOG(("renderinfo.screen = %p", renderinfo.screen));
    
    if (!renderinfo.screen) {
        LOG(("ERROR: No screen available!"));
        return;
    }
    
    struct ViewPort *vp = &renderinfo.screen->ViewPort;
    LOG(("ViewPort: %p", vp));
    LOG(("ColorMap: %p", vp->ColorMap));
    
    if (renderinfo.rp) {
        ULONG depth = GetBitMapAttr(renderinfo.rp->BitMap, BMA_DEPTH);
        LOG(("BitMap depth: %lu", depth));
    }
    
    LOG(("CyberGfxBase: %p", CyberGfxBase));
    LOG(("Screen dimensions: %dx%d", renderinfo.screen->Width, renderinfo.screen->Height));
    
    if (vp->ColorMap) {
        LOG(("ColorMap Count: %d", vp->ColorMap->Count));
        LOG(("ColorMap Type: %d", vp->ColorMap->Type));
        
        /* Test common web colors */
        ULONG test_colors[] = {
            0x000000, /* Black */
            0xFFFFFF, /* White */
            0xFF0000, /* Red */
            0x00FF00, /* Green */
            0x0000FF, /* Blue */
            0x666666, /* Text gray */
            0x444444, /* Darker text gray */
            0xCCCCCC  /* Light gray */
        };
        
        for (int i = 0; i < 8; i++) {
            ULONG color = test_colors[i];
            UBYTE r = (color >> 16) & 0xFF;
            UBYTE g = (color >> 8) & 0xFF;
            UBYTE b = color & 0xFF;
            
            LONG pen = ObtainBestPen(vp->ColorMap, 
                                     (ULONG)r << 24, 
                                     (ULONG)g << 24, 
                                     (ULONG)b << 24,
                                     OBP_Precision, PRECISION_GUI, 
                                     TAG_DONE);
            
            LOG(("Color 0x%06lX -> pen %ld", color, pen));
            if (pen >= 0) ReleasePen(vp->ColorMap, pen);
        }
    }
    
    LOG(("=== END ENHANCED CAPABILITIES DEBUG ==="));
}

/* Initialize with debug */
nserror mui_plotters_init(void)
{
    /* Initialize global variables */
    last_fg_color = 0xFFFFFFFF;
    last_bg_color = 0xFFFFFFFF;
    global_pen_a = -1;
    global_pen_b = -1;
    
    /* Debug screen capabilities */
   // debug_screen_capabilities();
    
    /* Initialize pens */
    InitializePens();
    
    return NSERROR_OK;
}

/* FIXED text function with proper background color handling - compatible with MUI plotters */
/* FIXED text function with proper background color handling - compatible with MUI plotters */
nserror
mui_text(const struct redraw_context *ctx,
         const struct plot_font_style *fstyle,
         int x,
         int y,
         const char *text,
         size_t length)
{
    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp || !text || length == 0) return NSERROR_OK;

    ULONG fg_pixel = ConvertNetSurfColor(fstyle->foreground);
    ULONG bg_pixel = 0xFFFFFF; // default white

    UBYTE fg_r = (fg_pixel >> 16) & 0xFF;
    UBYTE fg_g = (fg_pixel >> 8) & 0xFF;
    UBYTE fg_b = fg_pixel & 0xFF;

    bool use_background = (fstyle->background != NS_TRANSPARENT);
    if (use_background) {
        bg_pixel = ConvertNetSurfColor(fstyle->background);
    }

    UBYTE bg_r = (bg_pixel >> 16) & 0xFF;
    UBYTE bg_g = (bg_pixel >> 8) & 0xFF;
    UBYTE bg_b = bg_pixel & 0xFF;

    // Sprawdzenie kontrastu (nawet jeśli tło jest przezroczyste)
    int lum_fg = (fg_r * 299 + fg_g * 587 + fg_b * 114) / 1000;
    int lum_bg = (bg_r * 299 + bg_g * 587 + bg_b * 114) / 1000;
    int lum_diff = abs(lum_fg - lum_bg);

    if (lum_diff < 75) {
        // Kontrast zbyt niski — wymuś widoczne tło
        use_background = true;
        bg_pixel = (lum_fg > 128) ? 0x000000 : 0xFFFFFF;
        LOG(("mui_text: kontrast za niski (lum_fg=%d lum_bg=%d), wymuszam tło: 0x%06lX",
             lum_fg, lum_bg, bg_pixel));
    }

    // Otwórz font
    APTR font_node = mui_open_font(rp, fstyle);
    if (!font_node) {
        LOG(("mui_text: WARNING: brak fontu, używam domyślnego"));
    }

    struct fontnode *node = (struct fontnode *)font_node;
    UBYTE oldDm = GetDrMd(rp);

    // Ustaw kolory
    if (use_background) {
        SetRGBColor(rp, bg_pixel, TRUE);
        SetRGBColor(rp, fg_pixel, FALSE);
        SetDrMd(rp, JAM2);
        LOG(("mui_text: z tłem fg=0x%06lX bg=0x%06lX", fg_pixel, bg_pixel));
    } else {
        SetRGBColor(rp, fg_pixel, FALSE);
        SetDrMd(rp, JAM1);
        LOG(("mui_text: bez tła fg=0x%06lX", fg_pixel));
    }

    // Rysowanie tekstu
    bool ok = false;

    if (!ok && node && node->sysfont) {
        struct converted_text measure;
        mui_prepare_converted_text(text, length, &measure);

        Move(rp, x, y);
        Text(rp, measure.text, measure.length);

        mui_destroy_converted_text(&measure);
        ok = true;
    }

    if (!ok && rp->Font) {
        Move(rp, x, y);
        Text(rp, text, length);
        ok = true;
    }

    // Przywróć tryb rysowania
    SetDrMd(rp, oldDm);

    if (font_node) {
        mui_close_font(rp, font_node);
    }

    return ok ? NSERROR_OK : NSERROR_INVALID;
}
nserror
mui_rectangle(const struct redraw_context *ctx,
                      const plot_style_t *style,
                      const struct rect *rect)
{
    struct RastPort *rp = (struct RastPort *)ctx->priv;
    if (!rp) return NSERROR_INVALID;

    /* Lazy screen initialization */
    if (!screen_initialized) {
        ensure_screen_available(rp);
    }

    /* Handle fill (background) */
    if (style->fill_type != PLOT_OP_TYPE_NONE) {
        ULONG pixel = ConvertNetSurfColor(style->fill_colour);
        ULONG depth = GetBitMapAttr(rp->BitMap, BMA_DEPTH);
        
        LOG(("mui_rectangle_enhanced: Fill rect (%d,%d)-(%d,%d) with color 0x%08lX", 
             rect->x0, rect->y0, rect->x1, rect->y1, pixel));
        
        /* For RTG screens, try direct pixel writing first */
        //if (depth >= 15 && CyberGfxBase) {
            LONG result = FillPixelArray(rp, rect->x0, rect->y0, 
                                        rect->x1 - rect->x0, rect->y1 - rect->y0, pixel);
            //Delay(2);
            if (result != -1) {
                // WaitBlit();
                LOG(("mui_rectangle_enhanced: FillPixelArray success"));
                goto handle_stroke;
            
            }
            LOG(("mui_rectangle_enhanced: FillPixelArray failed, using pen method"));
       // }

        /* Pen-based filling for palette modes or RTG fallback */
        SetRGBColor(rp, pixel, FALSE);
        RectFill(rp, rect->x0, rect->y0, rect->x1 - 1, rect->y1 - 1);
        LOG(("mui_rectangle_enhanced: RectFill completed"));
    }

handle_stroke:
    /* Handle stroke (border) */
    if (style->stroke_type != PLOT_OP_TYPE_NONE) {
        LONG oldPenWidth = rp->PenWidth;
        LONG oldPenHeight = rp->PenHeight;
        UWORD oldLinePtrn = rp->LinePtrn;
        
        LONG width = MAX(1, plot_style_fixed_to_int(style->stroke_width));
        rp->PenWidth = width;
        rp->PenHeight = width;

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
        
        LOG(("mui_rectangle_enhanced: Stroke rect with width %ld, color 0x%08lX", width, pixel));
        
        Move(rp, rect->x0, rect->y0);
        Draw(rp, rect->x1 - 1, rect->y0);
        Draw(rp, rect->x1 - 1, rect->y1 - 1);
        Draw(rp, rect->x0, rect->y1 - 1);
        Draw(rp, rect->x0, rect->y0);

        /* Restore pen settings */
        rp->PenWidth = oldPenWidth;
        rp->PenHeight = oldPenHeight;
        rp->LinePtrn = oldLinePtrn;
    }

    return NSERROR_OK;
}

/* Cleanup function */
void mui_plotters_fini(void)
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