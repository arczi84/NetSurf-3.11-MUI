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

#include <string.h>

#include <cybergraphx/cybergraphics.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/system.h>
#include <intuition/pointerclass.h>
#include <workbench/startup.h>
#include <proto/asyncio.h>
#include <proto/codesets.h>
#include <proto/cybergraphics.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <proto/locale.h>
#include <proto/openurl.h>
#include <proto/ttengine.h>
#include <proto/utility.h>

#ifdef WITH_HUBBUB
#include <hubbub/hubbub.h>
#endif

#include "content/urldb.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "mui/applicationclass.h"
#include "mui/bitmap.h"
#include "mui/clipboard.h"
#include "mui/cookies.h"
#include "mui/fetch_file.h"
#include "mui/font.h"
#include "mui/gui.h"
#include "mui/history.h"
#include "mui/methodstack.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
#include "mui/options.h"
#include "mui/plotters.h"
#include "mui/save_complete.h"
#include "mui/schedule.h"
#include "mui/transferanimclass.h"
#include "mui/utils.h"
#include "render/form.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"

char *default_stylesheet_url;
char *adblock_stylesheet_url;

#if defined(__MORPHOS__)
struct Library *ZBase;
struct Library *JFIFBase;
#endif

struct Library *TTEngineBase;
struct Library *IconBase;
struct IntuitionBase *IntuitionBase;
struct Library *SocketBase;
struct Library *AsyncIOBase;
struct Library *OpenURLBase;

struct Library *CyberGfxBase;
struct Library *MUIMasterBase;

#if !defined(__MORPHOS2__)
struct Library *CodesetsBase;
#endif

#ifdef WITH_HUBBUB
static void *myrealloc(void *ptr, size_t len, void *pw)
{
	return realloc(ptr, len);
}
#endif

STATIC struct MinList download_list = { (APTR)&download_list.mlh_Tail, NULL, (APTR)&download_list };
STATIC struct MinList window_list = { (APTR)&window_list.mlh_Tail, NULL, (APTR)&window_list };
STATIC LONG process_priority;

#if defined(__MORPHOS2__)
LONG altivec_accelerated;
#endif

/*********************************************************************/

static void abort_downloads(void)
{
	struct gui_download_window *dw, *next;

	ITERATELISTSAFE(dw, next, &download_list)
	{
		CloseAsync(dw->fh);
		FreeMem(dw->dl, sizeof(struct download));
		FreeMem(dw, sizeof(*dw));
	}
}

static void gui_delete_windowlist(void)
{
	struct gui_window *g, *next;

	ITERATELISTSAFE(g, next, &window_list)
	{
		gui_window_destroy(g);
	}
}

static void cleanup(void)
{
	netsurf_cleanup();
	mui_schedule_finalise();
	methodstack_cleanup();

	gui_delete_windowlist();
	bitmap_cleanup();
	transferanimclass_unload();

#if 0
	if (option_url_file && option_url_file[0])
		urldb_save(option_url_file);

	urldb_save_cookies(APPLICATION_COOKIES_FILE);
#endif

	mui_global_history_save();

	classes_cleanup();

	font_cleanup();

#ifdef WITH_HUBBUB
	hubbub_finalise(myrealloc, NULL);
#endif

	mui_clipboard_free();

	/* When NetSurf is aborted we must cleanup everything manually */
	mui_fetch_file_finalise(NULL);

	abort_downloads();

	#if !defined(__MORPHOS2__)
	CloseLibrary(CodesetsBase);
	#endif

	#if defined(__MORPHOS__)
	CloseLibrary(JFIFBase);
	CloseLibrary(ZBase);
	#endif

	CloseLibrary(OpenURLBase);
	CloseLibrary(AsyncIOBase);
	CloseLibrary(SocketBase);
	CloseLibrary(TTEngineBase);
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary(IconBase);
	CloseLibrary(CyberGfxBase);
	CloseLibrary(MUIMasterBase);

	#if defined(__MORPHOS__)
	NewSetTaskAttrsA(NULL, &process_priority, sizeof(ULONG), TASKINFOTYPE_PRI, NULL);
	#else
	SetTaskPri(SysBase->ThisTask, process_priority);
	#endif
}
#error "This file should not be included directly, use mui/gui.h instead."
static LONG startup(void)
{
	#if defined(__MORPHOS2__)
	#define TTENGINE_VERSION 8
	#else
	#define TTENGINE_VERSION 7
	#endif

	if ((MUIMasterBase = OpenLibrary("muimaster.library", 11)))
	if ((CyberGfxBase = OpenLibrary("cybergraphics.library", 41)))
	if (classes_init())
	if (mui_schedule_init())
	if (mui_clipboard_init())
#if defined(__MORPHOS__)
	if ((ZBase = OpenLibrary("z.library", 51)))
	if ((JFIFBase = OpenLibrary("jfif.library", 0)))
#endif
	if ((TTEngineBase = OpenLibrary("ttengine.library", TTENGINE_VERSION)))
	if ((IconBase = OpenLibrary("icon.library", 0)))
	if ((IntuitionBase = (APTR)OpenLibrary("intuition.library", 36)))
	if ((SocketBase = OpenLibrary("bsdsocket.library", 0)))
	if ((AsyncIOBase = OpenLibrary("asyncio.library", 0)))
	{
		struct Resident *res;

		methodstack_init();

		#if defined(__MORPHOS2__)
		res = FindResident("MorphOS");

		if (!res || res->rt_Version < 2 || (res->rt_Version == 2 && res->rt_Revision < 2)) 
		{
			die("MorphOS 2.2 or newer required!");
		}

		/* Notez-bien: return value is wrong in MorphOS 1. However AltiVec is not enabled in MorphOS 1 anyway. */
		NewGetSystemAttrsA(&altivec_accelerated, sizeof(altivec_accelerated), SYSTEMINFOTYPE_PPC_ALTIVEC, NULL);
		#endif

		transferanimclass_load();

		return TRUE;
	}

	return FALSE;
}

void gui_init(int argc, char** argv)
{
	font_init();
	#if defined(__MORPHOS__)
	NewGetTaskAttrsA(NULL, &process_priority, sizeof(ULONG), TASKINFOTYPE_PRI, NULL);
	#else
	process_priority = SysBase->ThisTask->tc_Node.ln_Pri;
	#endif

	atexit(cleanup);

	if (startup())
	{
		struct Locale *locale;
		TEXT lang[100];
		BPTR lock, file;
		ULONG i, found;

		options_read("PROGDIR:Resources/Options");

		verbose_log = option_verbose_log;

		if((!option_url_file) || (option_url_file[0] == '\0'))
			option_url_file = (char *)strdup("Resources/URLs");

/*
		if((!option_cookie_jar) || (option_cookie_jar[0] == '\0'))
			option_cookie_jar = (char *)strdup("Resources/CookieJar");
*/

		if((!option_ca_bundle) || (option_ca_bundle[0] == '\0'))
			option_ca_bundle = (char *)strdup("PROGDIR:curl-ca-bundle.crt");

		if (lock = Lock("PROGDIR:Resources/LangNames", ACCESS_READ)) {
			UnLock(lock);
			messages_load("PROGDIR:Resources/LangNames");
		}

		locale = OpenLocale(NULL);
		found = FALSE;

		for (i = 0;i < 10; i++) {
			if (locale->loc_PrefLanguages[i] == NULL)
				continue;

			strcpy(lang, "PROGDIR:Resources/");
			strcat(lang, messages_get(locale->loc_PrefLanguages[i]));
			strcat(lang, "/messages");

			if (lock = Lock(lang, ACCESS_READ)) {
				UnLock(lock);
				found = TRUE;
				break;
			}
		}

		if (!found)
			strcpy(lang, "PROGDIR:Resources/en/messages");

		CloseLocale(locale);

		messages_load(lang); // check locale language and read appropriate file

		default_stylesheet_url = "file:///Resources/default.css";
		adblock_stylesheet_url = "file:///Resources/adblock.css";

		netsurf_setup();

#ifdef WITH_HUBBUB
		if (hubbub_initialise("Resources/Aliases", myrealloc, NULL) != HUBBUB_OK)
			die(messages_get("NoMemory"));
#endif
		css_screen_dpi = 90;

		plot = muiplot;

		urldb_load(option_url_file);
		urldb_load_cookies(APPLICATION_COOKIES_FILE);

		mui_global_history_initialise();
		mui_cookies_initialise();
		save_complete_init();

		return;
	}

	exit(20);
}

void gui_init2(int argc, char** argv)
{
	LONG priority, got_window;

	priority = -1;
	got_window = 0;

	mui_fetch_file_register();

#ifdef MULTITHREADED
	NewSetTaskAttrsA(NULL, &priority, sizeof(ULONG), TASKINFOTYPE_PRI, NULL);
#endif

	if (argc) {
		enum
		{
			ARG_URL = 0,
			ARG_COUNT
		};

		STATIC CONST TEXT template[] = "URL/A";
		struct RDArgs *args;
		IPTR array[ARG_COUNT] = { NULL };

		if (args = ReadArgs(template, array, NULL)) {
			if (array[ARG_URL]) {
				STRPTR url;

				url = (char *)DupStr((char *)array[ARG_URL]);

				if (url) {
					if (browser_window_create(url, 0, 0, true, false))
						got_window = 1;

					free(url);
				}
			}

			FreeArgs(args);
		}
	} else {
		extern struct WBStartup *_WBenchMsg;
		struct WBStartup *WBenchMsg;
		struct WBArg *wbarg;
		ULONG i;

		WBenchMsg = (struct WBStartup *)argv;

		for (i = 1, wbarg = WBenchMsg->sm_ArgList + 1; i < WBenchMsg->sm_NumArgs; i++, wbarg++) {
			if (wbarg->wa_Lock && *wbarg->wa_Name) {
				STRPTR path = GetNameFromLock(wbarg->wa_Lock);

				if (path) {
					ULONG length;
					STRPTR file;

					length = strlen(path) + strlen(wbarg->wa_Name) + 4;
					file = AllocTaskPooled(length);

					if (file) {
						char *url;

						strcpy(file, path);
						AddPart(file, wbarg->wa_Name, length);

						url = path_to_url(file);

						if (url) {
							if (browser_window_create(url, 0, 0, true, false))
								got_window = 1;

							free(url);
						}

						FreeTaskPooled(file, length);
					}

					FreeVecTaskPooled(path);
				}
			}
		}
	}

	if (!got_window)
		methodstack_push_imm(application, 2, MM_Window_MenuAction, MNA_NEW_PAGE);
}

void gui_multitask(void)
{
	if (netsurf_check_events(TRUE, schedule_sig) & schedule_sig)
		mui_schedule_poll();
}

void gui_poll(bool active)
{
	if (active || browser_reformat_pending)
		gui_multitask();
	else
		if (netsurf_check_events(FALSE, schedule_sig) & schedule_sig)
			mui_schedule_poll();
}

void gui_quit(void)
{
}

struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone, bool new_tab)
{
	struct gui_window *gw;

	gw = AllocMem(sizeof(*gw), MEMF_ANY);

	if (gw) {
		gw->bw = bw;
		gw->pointertype = -1;
		gw->obj = (APTR)methodstack_push_sync(application, 2,
				MM_Application_AddBrowser, gw);

		if (gw->obj == NULL) {
			FreeMem(gw, sizeof(*gw));
			gw = NULL;
		} else {
			gw->BitMap = NULL;
			gw->Layer = NULL;
			gw->LayerInfo = NULL;
			ADDTAIL(&window_list, gw);
		}
	}

	return gw;
}

void gui_window_destroy(struct gui_window *g)
{
	REMOVE(g);

	if (g->win && !netsurf_quit)
		methodstack_push_sync(g->win, 2, MM_Window_RemovePage, g->obj);

	if (g->Layer) {
		TT_DoneRastPort(g->RastPort);
		DeleteLayer(NULL, g->Layer);
	}

	if (g->LayerInfo)
		DisposeLayerInfo(g->LayerInfo);

	FreeBitMap(g->BitMap);
	FreeMem(g, sizeof(*g));
}

void gui_window_set_title(struct gui_window *g, const char *title)
{
	methodstack_push_sync(g->obj, 3, MUIM_Set, MA_Browser_Title, title);
}

static void gui_redraw_all(struct gui_window *g)
{

}

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
#if 0
	struct IBox bbox;

	methodstack_push_sync(g->obj, 3, OM_GET, MA_Browser_Box, &bbox);

	if (bbox.Left >= x0 && bbox.Top >= y0)
	{
		if (bbox.Left < x1 && bbox.Top < y1)
			methodstack_push_sync(g->obj, 1, MM_Browser_Redraw);
	}
#else
	struct content *c;

	c = g->bw->current_content;

	if (c)
	{
		renderinfo.rp = (APTR)methodstack_push_sync(g->obj, 3, MM_Browser_GetBitMap, c->width, c->height);

		if (g->RastPort)
		{
			g->redraw = 0;
			current_redraw_browser = g->bw;
			plot = muiplot;

			renderinfo.rp = g->RastPort;
			renderinfo.width = c->width;
			renderinfo.height = c->height;	// * RENDER_MULTIPLIER;
			//renderinfo.maxwidth = _screen(g->obj)->Width;
			//renderinfo.maxheight = _screen(g->obj)->Height;

			content_redraw(c, 0, 0, c->width, c->height, x0, y0, x1, y1, g->bw->scale, 0xfffff);

			current_redraw_browser = NULL;
			methodstack_push_imm(g->obj, 1, MM_Browser_Redraw);
		}
	}
#endif
}

void gui_window_redraw_window(struct gui_window *g)
{
#if 1
	struct content *c;

	c = g->bw->current_content;

	if (c)
		gui_window_redraw(g, 0, 0, c->width, c->height);
#else
	methodstack_push_sync(g->obj, 1, MM_Browser_Redraw);
#endif
}

void gui_window_update_box(struct gui_window *g, const union content_msg_data *data)
{
	struct content *c;

	c = g->bw->current_content;
	return;

	if (c)
	{
		renderinfo.rp = (APTR)methodstack_push_sync(g->obj, 3, MM_Browser_GetBitMap, c->width, c->height);

		if (g->RastPort)
		{
			if (g->redraw)
			{
				gui_window_redraw(g, 0, 0, c->width, c->height);
			}
			else
			{
				current_redraw_browser = g->bw;
				plot = muiplot;

				renderinfo.rp = g->RastPort;
				renderinfo.width = c->width;
				renderinfo.height = c->height;	// * RENDER_MULTIPLIER;
				//renderinfo.maxwidth = _screen(g->obj)->Width;
				//renderinfo.maxheight = _screen(g->obj)->Height;

				content_redraw(data->redraw.object,
					0, 0,
					data->redraw.width + data->redraw.x, data->redraw.height + data->redraw.y,

					data->redraw.x, data->redraw.y,
					data->redraw.x + data->redraw.width, data->redraw.y + data->redraw.height,

					g->bw->scale, 0xFFFFFF);

				current_redraw_browser = NULL;
				methodstack_push_imm(g->obj, 1, MM_Browser_Redraw);
			}
		}
	}
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	methodstack_push(g->obj, 3, OM_GET, MUIA_Virtgroup_Left, sx);
	methodstack_push_imm(g->obj, 3, OM_GET, MUIA_Virtgroup_Top, sy);
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	IPTR tags[5];

	if (sx < 0)
		sx = 0;

	if (sy < 0)
		sy = 0;

	tags[0] = MUIA_Virtgroup_Left;
	tags[1] = sx;
	tags[2] = MUIA_Virtgroup_Top;
	tags[3] = sy;
	tags[4] = TAG_DONE;

	methodstack_push_sync(g->obj, 2, OM_SET, tags);
}

void gui_window_scroll_visible(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	gui_window_set_scroll(g, x0, y0);
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0, int x1, int y1)
{
}

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height, bool scaled)
{
	struct IBox bbox;
	int w, h;

	methodstack_push_sync(g->obj, 3, OM_GET, MA_Browser_Box, &bbox);

	w = bbox.Width;
	h = bbox.Height;

#if 0
	if (scaled)
	{
		w /= g->bw->scale;
		h /= g->bw->scale;
	}
#else
	#warning gui_window_get_dimensions(): scaled boolean not supported
#endif

	*width = w;
	*height = h;
}

void gui_window_update_extent(struct gui_window *g)
{
	methodstack_push(g->obj, 3, MM_Browser_SetContentSize, g->bw->current_content->width, g->bw->current_content->height);
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
	methodstack_push_sync(g->obj, 3, MUIM_Set, MA_Browser_StatusText, text);
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	#if defined(__MORPHOS__)
	ULONG pointertype = POINTERTYPE_NORMAL;

	switch (shape) {
	case GUI_POINTER_DEFAULT:
		pointertype = POINTERTYPE_NORMAL;
		break;

	case GUI_POINTER_POINT:
		pointertype = POINTERTYPE_SELECTLINK;
		break;

	case GUI_POINTER_CARET:
	case GUI_POINTER_MENU:
	case GUI_POINTER_UP:
	case GUI_POINTER_DOWN:
	case GUI_POINTER_LEFT:
	case GUI_POINTER_RIGHT:
	case GUI_POINTER_RU:
	case GUI_POINTER_LD:
	case GUI_POINTER_LU:
	case GUI_POINTER_RD:
		break;

	case GUI_POINTER_CROSS:
		pointertype = POINTERTYPE_AIMING;
		break;

	case GUI_POINTER_MOVE:
		pointertype = POINTERTYPE_MOVE;
		break;

	case GUI_POINTER_WAIT:
		pointertype = POINTERTYPE_BUSY;
		break;

	case GUI_POINTER_HELP:
		pointertype = POINTERTYPE_HELP;
		break;

	case GUI_POINTER_NO_DROP:
	case GUI_POINTER_NOT_ALLOWED:
		pointertype = POINTERTYPE_NOTAVAILABLE;
		break;

	case GUI_POINTER_PROGRESS:
		pointertype = POINTERTYPE_WORKING;
		break;
	}

	if (g->pointertype != pointertype) {
		g->pointertype = pointertype;
		methodstack_push(g->obj, 3, MUIM_Set, MA_Browser_Pointer, pointertype);
	}
	#endif
}

void gui_window_hide_pointer(struct gui_window *g)
{
	if (g->pointertype != POINTERTYPE_INVISIBLE) {
		g->pointertype = POINTERTYPE_INVISIBLE;
		methodstack_push(g->obj, 3, MUIM_Set, MA_Browser_Pointer, POINTERTYPE_INVISIBLE);
	}
}

void gui_window_set_url(struct gui_window *g, const char *url)
{
	methodstack_push_sync(g->obj, 3, MUIM_Set, MA_Browser_URL, url);
}

void gui_window_start_throbber(struct gui_window *g)
{
	methodstack_push_imm(g->obj, 3, MUIM_Set, MA_Browser_Loading, FALSE);
}

void gui_window_stop_throbber(struct gui_window *g)
{
	methodstack_push_imm(g->obj, 3, MUIM_Set, MA_Browser_Loading, TRUE);
}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	gui_window_remove_caret(g);
}

void gui_window_remove_caret(struct gui_window *g)
{
}

/**
 * Called when the gui_window has new content.
 *
 * \param  g  the gui_window that has new content
 */

void gui_window_new_content(struct gui_window *g)
{
	methodstack_push(g->obj, 2, MM_Browser_SetContentType, g->bw->current_content->type);
}

bool gui_window_scroll_start(struct gui_window *g)
{
	return true;
}

bool gui_window_box_scroll_start(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	return true;
}

bool gui_window_frame_resize_start(struct gui_window *g)
{
}

void gui_window_save_as_link(struct gui_window *g, struct content *c)
{
}

void gui_window_set_scale(struct gui_window *g, float scale)
{
}

struct gui_download_window *gui_download_window_create(const char *url,
		const char *mime_type, struct fetch *fetch,
		unsigned int total_size, struct gui_window *gui)
{
	struct gui_download_window *dw;

	dw = AllocMem(sizeof(*dw), MEMF_ANY);

	if (dw)
	{
		struct download *dl;
		ULONG ok;

		dl = (APTR)methodstack_push_sync(application, 3, MM_Application_Download, (IPTR)url, 0);
		ok = 0;

		if (dl)
		{
			BPTR lock;

			dw->dl = dl;
			dl->size = total_size;

			lock = Lock(dl->path, ACCESS_READ);

			if (lock)
			{
				lock = CurrentDir(lock);

				dw->fh = OpenAsync(dl->filename, MODE_WRITE, 8192);

				if (dw->fh)
				{
					ADDTAIL(&download_list, dw);
					SetComment(dl->filename, url);
					ok = 1;
				}

				UnLock(CurrentDir(lock));
			}
		}

		if (!ok)
		{
			if (dl)
			{
				methodstack_push_sync(application, 2, MM_Application_DownloadError, dw->dl);
			}
			else
			{
				FreeMem(dw, sizeof(*dw));
			}

			dw = NULL;
		}
	}

	return dw;
}

void gui_download_window_data(struct gui_download_window *dw, const char *data, unsigned int size)
{
	WriteAsync(dw->fh, (APTR)data, size);

	dw->dl->done += size;

	methodstack_push(application, 1, MM_Application_DownloadUpdate);
}

void gui_download_window_error(struct gui_download_window *dw, const char *error_msg)
{
	methodstack_push_sync(application, 2, MM_Application_DownloadError, dw->dl);
}

void gui_download_window_done(struct gui_download_window *dw)
{
	methodstack_push_sync(application, 2, MM_Application_DownloadDone, dw->dl);
}

void gui_drag_save_object(gui_save_type type, struct content *c, struct gui_window *g)
{
}

void gui_create_form_select_menu(struct browser_window *bw, struct form_control *control)
{
}

void gui_launch_url(const char *url)
{
	#if defined(__MORPHOS__)
	if (!strncmp("mailto:", url, 7))
	{
		if (OpenURLBase == NULL)
			OpenURLBase = OpenLibrary("openurl.library", 6);

		if (OpenURLBase)
			URL_OpenA((STRPTR)url, NULL);
	}
	#endif
}

void gui_cert_verify(struct browser_window *bw, struct content *c, const struct ssl_cert_info *certs, unsigned long num)
{
}
