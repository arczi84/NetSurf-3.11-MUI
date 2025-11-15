/*
 * System Font Implementation for New NetSurf API
 * Copyright 2009 Ilkka Lehtoranta <ilkleht@isoveli.org>
 * Modifications 2025 for new NetSurf API compatibility
 */

#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <graphics/rpattr.h>
#include <hardware/atomic.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/diskfont.h>

#include "utils/errors.h"
#include "netsurf/plot_style.h"  // Nowe API
#include "netsurf/utf8.h"
#include "netsurf/layout.h"
#include "mui/font.h"
#include "mui/plotters.h"
#include "mui/utils.h"
#include "html/font.h"
#include "utils/utf8.h"
#include "utils/log0.h"
#include "macros/vapor.h"
#include "libcss/fpmath.h"

#include "os3.h"

extern css_fixed nscss_screen_dpi;


/* Global variables */
STATIC struct SignalSemaphore semaphore;
STATIC struct RastPort fontrp;
STATIC struct MinList fontcache = {
    (APTR)&fontcache.mlh_Tail, NULL, (APTR)&fontcache
};

/* Optional FreeType support */
BOOL use_freetype = FALSE;
extern BOOL ft_init(void);
extern void ft_cleanup(void);
extern APTR ft_create_font_node(const plot_font_style_t *fstyle);
extern int ft_text_width(const plot_font_style_t *fstyle, const char *string, size_t length);
extern int ft_position_in_string(const plot_font_style_t *fstyle, const char *string, size_t length, int x, size_t *char_offset);
extern int ft_split_string(const plot_font_style_t *fstyle, const char *string, size_t length, int x, size_t *char_offset);

/* System language support */
char *system_language = NULL;
static char localized_font_name[64];

/* Font base names (without .font extension) */
static const char *font_base_names[] = {
    "AfA AmiKit",                    /* 0 - default */
    "AfA AmiKit Bold",              /* 1 - bold */
    "AfA AmiKit Italic",            /* 2 - italic */
    "AfA AmiKit Bold Italic",       /* 3 - bold italic */
    "AfA AmiKit Mono",              /* 4 - mono */
    "AfA AmiKit Mono Bold",         /* 5 - mono bold */
    "AfA AmiKit Mono Italic",       /* 6 - mono italic */
    "AfA AmiKit Mono BoldI",        /* 7 - mono bold italic */
    "AfA AmiKit Serif",             /* 8 - serif */
    "AfA AmiKit Serif Bold",        /* 9 - serif bold */
    "AfA AmiKit Serif It",          /* 10 - serif italic */
    "AfA AmiKit Serif BI",          /* 11 - serif bold italic */
    "helvetica",                    /* 12 - helvetica fallback */
    "topaz",                        /* 13 - topaz fallback 8 */
    "topaz",                        /* 14 - topaz fallback 11 */
    "topaz"                         /* 15 - topaz fallback 13 */
};

/* Static TextAttr structures that will be updated dynamically */
static struct TextAttr font_attrs[16];

/* Function to get localized font name */
static const char *get_localized_font_name(const char *base_name)
{
    const char *suffix = NULL;
    
    if (!base_name || strlen(base_name) == 0) {
        LOG(("ERROR: get_localized_font_name called with empty base_name"));
        return "topaz.font";
    }
    
    if (system_language && strlen(system_language) > 0) {      
        if (strstr(system_language, "pl") || strstr(system_language, "PL") ||
            strstr(system_language, "polski") || strstr(system_language, "Polish")) {
            suffix = "-PL";
        } 
        else if (strstr(system_language, "eu") || strstr(system_language, "EU") ||
                 strstr(system_language, "european") || strstr(system_language, "European") ||
                 strstr(system_language, "de") || strstr(system_language, "DE") ||
                 strstr(system_language, "fr") || strstr(system_language, "FR")) {
            suffix = "-EU";
        }
    }
    
    /* Construct localized font name */
    if (suffix) {
        snprintf(localized_font_name, sizeof(localized_font_name), "%s%s.font", base_name, suffix);
    } else {
        snprintf(localized_font_name, sizeof(localized_font_name), "%s.font", base_name);
    }
    
    return localized_font_name;
}

static unsigned char convert_codepoint_to_iso(uint32_t utf8_char);

void mui_prepare_converted_text(const char *string, size_t length,
    struct converted_text *out)
{
    out->text = string;
    out->length = length;
    out->buffer = NULL;
    out->index_map = NULL;

    if (string == NULL || length == 0) {
        return;
    }

    bool convert = needs_character_conversion();
    if (!convert) {
        for (size_t i = 0; i < length; i++) {
            if (((unsigned char)string[i]) >= 0x80) {
                convert = true;
                break;
            }
        }
    }

    if (!convert) {
        return;
    }

    size_t max_len = length * 2 + 1;
    char *converted = AllocVec(max_len, MEMF_ANY);
    size_t *map = AllocVec(sizeof(size_t) * (max_len + 1), MEMF_ANY);

    if (converted == NULL || map == NULL) {
        if (converted) {
            FreeVec(converted);
        }
        if (map) {
            FreeVec(map);
        }
        return;
    }

    size_t utf8_pos = 0;
    size_t iso_pos = 0;

    while (utf8_pos < length && iso_pos < max_len - 1) {
        uint32_t cp = utf8_to_ucs4(string + utf8_pos, length - utf8_pos);
        map[iso_pos] = utf8_pos;
        converted[iso_pos++] = convert_codepoint_to_iso(cp);
        size_t next = utf8_next(string, length, utf8_pos);
        if (next <= utf8_pos) {
            break;
        }
        utf8_pos = next;
    }

    map[iso_pos] = utf8_pos;
    converted[iso_pos] = '\0';

    out->text = converted;
    out->length = iso_pos;
    out->buffer = converted;
    out->index_map = map;
}

void mui_destroy_converted_text(struct converted_text *text)
{
    if (text->buffer) {
        FreeVec(text->buffer);
    }
    if (text->index_map) {
        FreeVec(text->index_map);
    }
    text->buffer = NULL;
    text->index_map = NULL;
}

/* Character encoding conversion for Polish */
static const struct {
    uint32_t utf8_code;
    unsigned char iso_code;
} polish_char_map[] = {
    { 0x0105, 0xB1 },  /* ą -> 0xB1 */
    { 0x0107, 0xE6 },  /* ć -> 0xE6 */
    { 0x0119, 0xEA },  /* ę -> 0xEA */
    { 0x0142, 0xB3 },  /* ł -> 0xB3 */
    { 0x0144, 0xF1 },  /* ń -> 0xF1 */
    { 0x00F3, 0xF3 },  /* ó -> 0xF3 */
    { 0x015B, 0xB6 },  /* ś -> 0xB6 */
    { 0x017A, 0xBC },  /* ź -> 0xBC */
    { 0x017C, 0xBF },  /* ż -> 0xBF */
    { 0x0104, 0xA1 },  /* Ą -> 0xA1 */
    { 0x0106, 0xC6 },  /* Ć -> 0xC6 */
    { 0x0118, 0xCA },  /* Ę -> 0xCA */
    { 0x0141, 0xA3 },  /* Ł -> 0xA3 */
    { 0x0143, 0xD1 },  /* Ń -> 0xD1 */
    { 0x00D3, 0xD3 },  /* Ó -> 0xD3 */
    { 0x015A, 0xA6 },  /* Ś -> 0xA6 */
    { 0x0179, 0xAC },  /* Ź -> 0xAC */
    { 0x017B, 0xAF },  /* Ż -> 0xAF */
    { 0, 0 }
};

/* Convert UTF-8 character to ISO-8859-2 */
static unsigned char convert_codepoint_to_iso(uint32_t utf8_char)
{
    if (utf8_char < 128) {
        return (unsigned char)utf8_char;
    }
    
    for (int i = 0; polish_char_map[i].utf8_code != 0; i++) {
        if (polish_char_map[i].utf8_code == utf8_char) {
            return polish_char_map[i].iso_code;
        }
    }

    /* Provide ASCII/ISO fallbacks for common glyphs that Amiga fonts lack. */
    switch (utf8_char) {
    case 0x00A0: /* NO-BREAK SPACE */
    case 0x2000: /* EN QUAD */
    case 0x2001: /* EM QUAD */
    case 0x2002: /* EN SPACE */
    case 0x2003: /* EM SPACE */
    case 0x2004: /* THREE-PER-EM SPACE */
    case 0x2005: /* FOUR-PER-EM SPACE */
    case 0x2006: /* SIX-PER-EM SPACE */
    case 0x2007: /* FIGURE SPACE */
    case 0x2008: /* PUNCTUATION SPACE */
    case 0x2009: /* THIN SPACE */
    case 0x200A: /* HAIR SPACE */
    case 0x200B: /* ZERO WIDTH SPACE */
    case 0x202F: /* NARROW NO-BREAK SPACE */
    case 0x205F: /* MEDIUM MATHEMATICAL SPACE */
    case 0x3000: /* IDEOGRAPHIC SPACE */
        return ' ';
    case 0x00AB: /* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
    case 0x2039: /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
        return '<';
    case 0x00BB: /* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
    case 0x203A: /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
        return '>';
    case 0x2022: /* BULLET */
    case 0x25CF: /* BLACK CIRCLE */
        return MUI_BULLET_PLACEHOLDER;
    case 0x25CB: /* WHITE CIRCLE */
        return 'o';
    case 0x25A0: /* BLACK SQUARE */
    case 0x25AA: /* BLACK SMALL SQUARE */
        return '#';
    default:
        break;
    }
    
    return '?';
}

/* Convert UTF-8 string to ISO-8859-2 */
size_t convert_utf8_to_iso8859_2(const char *utf8_str, size_t utf8_len,
                                 char *iso_buffer, size_t iso_buffer_size)
{
    size_t utf8_pos = 0;
    size_t iso_pos = 0;
    
    if (!utf8_str || !iso_buffer || iso_buffer_size == 0) {
        return 0;
    }
    
    while (utf8_pos < utf8_len && iso_pos < (iso_buffer_size - 1)) {
        uint32_t utf8_char = utf8_to_ucs4(utf8_str + utf8_pos, utf8_len - utf8_pos);
    unsigned char iso_char = convert_codepoint_to_iso(utf8_char);
        
        iso_buffer[iso_pos++] = iso_char;
        utf8_pos = utf8_next(utf8_str, utf8_len, utf8_pos);
    }
    
    iso_buffer[iso_pos] = '\0';
    return iso_pos;
}

/* Check if current language needs character conversion */
BOOL needs_character_conversion(void)
{
    if (!system_language) return FALSE;
    
    if (strstr(system_language, "pl") || strstr(system_language, "PL") ||
        strstr(system_language, "polski") || strstr(system_language, "Polish")) {
        return TRUE;
    }
    
    return FALSE;
}

/* Initialize localized fonts */
static void init_localized_fonts(void)
{
    int i;
    
    for (i = 0; i < 16; i++) {
        const char *localized_name = get_localized_font_name(font_base_names[i]);
        
        font_attrs[i].ta_Name = AllocVec(strlen(localized_name) + 1, MEMF_ANY);
        if (font_attrs[i].ta_Name) {
            strcpy(font_attrs[i].ta_Name, localized_name);
        } else {
            font_attrs[i].ta_Name = (STRPTR)font_base_names[i];
        }
        
        font_attrs[i].ta_Flags = FPF_DISKFONT;
        font_attrs[i].ta_Style = FS_NORMAL;
        
        switch (i) {
            case 0: case 1: case 2: case 3:
            case 4: case 5: case 6: case 7:
            case 8: case 9: case 10: case 11:
            case 12:
                font_attrs[i].ta_YSize = 11;
                break;
            case 13:
                font_attrs[i].ta_YSize = 8;
                break;
            case 14:
                font_attrs[i].ta_YSize = 11;
                break;
            case 15:
                font_attrs[i].ta_YSize = 13;
                break;
        }
    }
}

/* Get font attribute based on type and size */
static struct TextAttr *get_font_attr(int font_type, ULONG ysize)
{
    static struct TextAttr size_adjusted_attrs[16];
    static int initialized = 0;
    
    if (!initialized) {
        for (int i = 0; i < 16; i++) {
            size_adjusted_attrs[i] = font_attrs[i];
        }
        initialized = 1;
    }
    
    if (font_type < 0 || font_type >= 16) {
        font_type = 13;
    }
    
    size_adjusted_attrs[font_type] = font_attrs[font_type];
    size_adjusted_attrs[font_type].ta_YSize = ysize + 2;

    return &size_adjusted_attrs[font_type];
}

/* Font initialization */
void font_init(void)
{
    use_freetype = FALSE;
    InitSemaphore(&semaphore);
    
    /* Clear font cache */
    struct fontnode *next, *node;
    ITERATELISTSAFE(node, next, &fontcache) {
        REMOVE(node);
        if (node->sysfont) {
            CloseFont(node->sysfont);
        }
        FreeMem(node, sizeof(*node));
    }
    
    NEWLIST(&fontcache);
    
    /* Detect system language */
    if (!system_language) {
        const char *detected_lang = getenv("LANGUAGE");
        if (!detected_lang) {
            detected_lang = getenv("LANG");
        }
        
        if (detected_lang) {
            system_language = AllocVec(strlen(detected_lang) + 1, MEMF_ANY);
            if (system_language) {
                strcpy(system_language, detected_lang);
                LOG(("INFO: Detected system language: %s", system_language));
            }
        }
    }
    
    init_localized_fonts();
    LOG(("INFO: Font system initialized with system fonts"));
}

/* Font cleanup */
void font_cleanup(void)
{
    struct fontnode *next, *node;
#if 0
    if (use_freetype) {
        ft_cleanup();
        use_freetype = FALSE;
    }
#endif
    ITERATELISTSAFE(node, next, &fontcache) {
        if (node->sysfont) {
            CloseFont(node->sysfont);
        }
        FreeMem(node, sizeof(*node));
    }
    
    /* Clean up localized font names */
    for (int i = 0; i < 16; i++) {
        if (font_attrs[i].ta_Name && 
            font_attrs[i].ta_Name != (STRPTR)font_base_names[i]) {
            FreeVec(font_attrs[i].ta_Name);
            font_attrs[i].ta_Name = NULL;
        }
    }
    
    if (system_language) {
        FreeVec(system_language);
        system_language = NULL;
    }
}

/* Font cache maintenance */
void font_cache_check(void)
{
    struct fontnode *next, *node;
    ULONG secs, dummy, time;

    CurrentTime(&secs, &dummy);
    time = secs - 180;

    ObtainSemaphore(&semaphore);
    ITERATELISTSAFE(node, next, &fontcache) {
        if (node->usecount == 0 && node->last_access < time) {
            REMOVE(node);
            if (node->sysfont) {
                CloseFont(node->sysfont);
            }
            FreeMem(node, sizeof(*node));
        }
    }
    ReleaseSemaphore(&semaphore);
}

/* Close font and update usage count */
void mui_close_font(struct RastPort *rp, APTR font)
{
    struct fontnode *node = font;
    ULONG dummy;

    if (!node) return;
    
    CurrentTime(&node->last_access, &dummy);
    ATOMIC_SUB((ULONG *)&node->usecount, 1);
}

/* Create system font node */
static inline ULONG css_size_to_pixels(const plot_font_style_t *fstyle)
{
    double size_pt = plot_style_fixed_to_double(fstyle->size);
    double dpi = (double)FIXTOFLT(nscss_screen_dpi);
    double px = (size_pt * dpi) / 72.0;

    if (px < 1.0) px = 1.0;
    if (px > 256.0) px = 256.0;

    return (ULONG)(px + 0.5);
}

static APTR mui_open_system_font(const plot_font_style_t *fstyle)
{
    struct fontnode *node;
    struct TextAttr *attr, *fallback_attr;
    struct TextFont *sysfont;
    ULONG ysize;
    BOOL is_italic, is_bold;

    ysize = css_size_to_pixels(fstyle);
    if (ysize < 8) ysize = 8;
    if (ysize > 72) ysize = 72;
    
    /* Determine style flags */
    is_italic = (fstyle->flags & FONTF_ITALIC);
    is_bold = (fstyle->weight >= 700);
    
    /* Choose appropriate font based on family */
    if (fstyle->family == PLOT_FONT_FAMILY_MONOSPACE) {
        if (is_bold && is_italic) {
            attr = get_font_attr(7, ysize);  /* mono bold italic */
        } else if (is_bold) {
            attr = get_font_attr(5, ysize);  /* mono bold */
        } else if (is_italic) {
            attr = get_font_attr(6, ysize);  /* mono italic */
        } else {
            attr = get_font_attr(4, ysize);  /* mono */
        }
        fallback_attr = get_font_attr(4, ysize);
    } else if (fstyle->family == PLOT_FONT_FAMILY_SERIF) {
        if (is_bold && is_italic) {
            attr = get_font_attr(11, ysize);  /* serif bold italic */
        } else if (is_bold) {
            attr = get_font_attr(9, ysize);   /* serif bold */
        } else if (is_italic) {
            attr = get_font_attr(10, ysize);  /* serif italic */
        } else {
            attr = get_font_attr(8, ysize);   /* serif */
        }
        fallback_attr = get_font_attr(13, ysize);
    } else {
        /* Sans-serif or default */
        if (is_bold && is_italic) {
            attr = get_font_attr(3, ysize);   /* bold italic */
        } else if (is_bold) {
            attr = get_font_attr(1, ysize);   /* bold */
        } else if (is_italic) {
            attr = get_font_attr(2, ysize);   /* italic */
        } else {
            attr = get_font_attr(0, ysize);   /* default */
        }
        fallback_attr = get_font_attr(13, ysize);
    }

    /* Check cache */
    ObtainSemaphore(&semaphore);
    ITERATELIST(node, &fontcache) {
        if (node->ysize == ysize && 
            node->familytable == (APTR)attr &&
            node->style == attr->ta_Style &&
            node->weight == (is_bold ? FSF_BOLD : FS_NORMAL)) {
            ATOMIC_ADD((ULONG *)&node->usecount, 1);
            ReleaseSemaphore(&semaphore);
            return node;
        }
    }
    ReleaseSemaphore(&semaphore);

    /* Create new font node */
    node = AllocMem(sizeof(*node), MEMF_ANY | MEMF_CLEAR);
    if (node) {
        LOG(("DEBUG: Attempting to open font: '%s'", attr->ta_Name));
        sysfont = OpenDiskFont(attr);
        
        if (!sysfont) {
            LOG(("INFO: Failed to open %s, trying fallbacks", attr->ta_Name));
            
            /* Try Helvetica fallback */
            if (fstyle->family == PLOT_FONT_FAMILY_SANS_SERIF) {
                struct TextAttr helvetica_attr = { "helvetica.font", ysize, FS_NORMAL, FPF_DISKFONT };
                sysfont = OpenDiskFont(&helvetica_attr);
            }
            
            /* Final fallback to Topaz */
            if (!sysfont) {
                sysfont = OpenDiskFont(fallback_attr);
            }
        }
        
        if (sysfont) {
            node->ysize = ysize;
            node->familytable = (APTR)attr;
            node->style = attr->ta_Style;
            node->weight = (is_bold ? FSF_BOLD : FS_NORMAL);
            node->ft_face = NULL;
            node->sysfont = sysfont;
            node->usecount = 1;

            ObtainSemaphore(&semaphore);
            ADDTAIL(&fontcache, node);
            ReleaseSemaphore(&semaphore);
            
            LOG(("INFO: Successfully opened font: %s", sysfont->tf_Message.mn_Node.ln_Name));
        } else {
            FreeMem(node, sizeof(*node));
            node = NULL;
            LOG(("ERROR: All font fallbacks failed!"));
        }
    }

    return node;
}

/* Main font opening function */
APTR mui_open_font(struct RastPort *rp, const plot_font_style_t *fstyle)
{
    APTR font_node = NULL;

    if (!fstyle) {
        LOG(("ERROR: fstyle is NULL"));
        return NULL;
    }
#if 0
    /* Try FreeType if enabled */
    if (use_freetype) {
        font_node = ft_create_font_node(fstyle);
        if (font_node) {
            LOG(("INFO: Using FreeType font"));
            return font_node;
        }
    }
#endif
    /* Use system fonts */
    font_node = mui_open_system_font(fstyle);
    if (font_node && rp) {
        struct fontnode *node = font_node;
        SetFont(rp, node->sysfont);
        //LOG(("INFO: Using system font: %s", node->sysfont->tf_Message.mn_Node.ln_Name));
    }

    return font_node;
}

/* Font width measurement */
static bool nsfont_width(const plot_font_style_t *fstyle,
                        const char *string, size_t length, int *width)
{
    APTR font_node;
    int w = 0;

    font_node = mui_open_font(&fontrp, fstyle);
    if (font_node) {
        struct fontnode *node = font_node;
        
        /*if (use_freetype && node->ft_face) {
            w = ft_text_width(fstyle, string, length);
        } else*/ if (node->sysfont) {
            struct converted_text measure;
            mui_prepare_converted_text(string, length, &measure);

            w = TextLength(&fontrp, measure.text, measure.length);

            mui_destroy_converted_text(&measure);
        }
        
        mui_close_font(&fontrp, font_node);
    }

    *width = w;
    return true;
}

/* Font position in string */
static bool nsfont_position_in_string(const plot_font_style_t *fstyle,
                                     const char *string, size_t length,
                                     int x, size_t *char_offset, int *actual_x)
{
    APTR font_node;
    int off = 0, act_x = 0;

    font_node = mui_open_font(&fontrp, fstyle);
    if (font_node) {
        struct fontnode *node = font_node;
        
        /*if (use_freetype && node->ft_face) {
            act_x = ft_position_in_string(fstyle, string, length, x, (size_t *)&off);
        } else*/ if (node->sysfont) {
            struct converted_text measure;
            mui_prepare_converted_text(string, length, &measure);

            struct TextExtent extent;
            off = TextFit(&fontrp, measure.text, measure.length, &extent, NULL, 1, x, 32767);
            act_x = extent.te_Extent.MaxX;

            if (measure.index_map && off <= measure.length) {
                off = measure.index_map[off];
            }

            mui_destroy_converted_text(&measure);
        }
        
        mui_close_font(&fontrp, font_node);
    }

    *char_offset = off;
    *actual_x = act_x;
    return true;
}

/* Font string splitting */
static bool nsfont_split(const plot_font_style_t *fstyle,
                        const char *string, size_t length,
                        int x, size_t *char_offset, int *actual_x)
{
    APTR font_node;
    LONG act_x = 0;
    size_t count = 0;

    font_node = mui_open_font(&fontrp, fstyle);
    if (font_node) {
        struct fontnode *node = font_node;
        
        /*if (use_freetype && node->ft_face) {
            act_x = ft_split_string(fstyle, string, length, x, &count);
        } else*/ if (node->sysfont) {
            struct converted_text measure;
            mui_prepare_converted_text(string, length, &measure);

            struct TextExtent extent;
            size_t iso_break = TextFit(&fontrp, measure.text, measure.length,
                &extent, NULL, 1, x, 32767);

            while (iso_break > 0) {
                unsigned char ch = (unsigned char)measure.text[iso_break - 1];
                if (isspace(ch)) {
                    break;
                }
                iso_break--;
            }

            if (iso_break == 0) {
                iso_break = measure.length;
            }

            act_x = TextLength(&fontrp, measure.text, iso_break);

            if (measure.index_map) {
                count = measure.index_map[iso_break];
            } else {
                count = iso_break;
            }

            mui_destroy_converted_text(&measure);
        }
        
        mui_close_font(&fontrp, font_node);
    }

    *char_offset = count;
    *actual_x = act_x;
    return true;
}

/* Optional: Enable FreeType fonts */
BOOL font_enable_freetype(void)
{
    if (use_freetype) return TRUE;
    
    /*if (ft_init()) {
        use_freetype = TRUE;
        font_cache_check();
        LOG(("INFO: FreeType fonts enabled"));
        return TRUE;
    }*/
    
    return FALSE;
}

/* Optional: Disable FreeType fonts */
void font_disable_freetype(void)
{
    if (!use_freetype) return;
    
    /*ft_cleanup();
    use_freetype = FALSE;
    font_cache_check();*/
    LOG(("INFO: FreeType disabled, using system fonts"));
}

/* Font function table */
static const struct font_functions nsfont = {
    .width = nsfont_width,
    .position_in_string = nsfont_position_in_string,
    .split = nsfont_split
};

/* Layout table for new API */
static struct gui_layout_table layout_table = {
    .width = nsfont_width,
    .position = nsfont_position_in_string,
    .split = nsfont_split
};

struct gui_layout_table *mui_layout_table = &layout_table;