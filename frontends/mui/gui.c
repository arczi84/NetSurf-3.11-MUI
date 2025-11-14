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
#include <stdbool.h>

#include <cybergraphx/cybergraphics.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/ports.h>
#include <devices/timer.h>
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
#ifdef TTENGINE
#include <proto/ttengine.h>
#endif
#include <proto/utility.h>
#include <proto/keymap.h>
#include <proto/diskfont.h>

#include "netsurf/types.h"
#include "content/urldb.h"

#include "desktop/gui_internal.h"
#include "netsurf/browser_window.h"
#include "desktop/browser_private.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
///////added
#include "utils/errors.h"
#include "netsurf/browser.h"
#include "netsurf/browser_window.h"
#include "desktop/browser_history.h"
#include "netsurf/clipboard.h"
#include "netsurf/download.h"
#include "desktop/download.h"
#include "desktop/gui_table.h"
#include "netsurf/utf8.h"
#include "netsurf/mouse.h"
#include "netsurf/plotters.h"
#include "netsurf/netsurf.h"
#include "netsurf/window.h"
#include "netsurf/misc.h"
#include "netsurf/content.h"

#include "utils/nsoption.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/backing_store.h"
///////
#include "mui/applicationclass.h"
#include "mui/bitmap.h"
#include "mui/clipboard.h"
#include "mui/cookies.h"
#include "mui/fetch.h"
#include "mui/findfile.h"
#include "mui/font.h"
#include "mui/gui.h"
#include "mui/history.h"
#include "mui/methodstack.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
//#include "mui/options.h"
#include "mui/plotters.h"
#include "mui/save_complete.h"
#include "mui/schedule.h"
#include "mui/transferanimclass.h"
#include "mui/utils.h"
#include "html/form_internal.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "utils/log0.h"

#include "amiga/misc.h"

bool netsurf_quit = false;
extern struct plotter_table plot;
extern struct gui_utf8_table *amiga_utf8_table;
//struct gui_download_table *amiga_download_table = &download_table;

#define kprintf

APTR global_obj = NULL;
static bool cleanup_done = false;

/* Forward declarations */
static bool browser_reformat_pending = false;
bool mui_redraw_pending = false;

static nserror gui_window_get_dimensions(struct gui_window *g, int *width, int *height, bool scaled);
static nserror gui_window_set_scroll_rect(struct gui_window *g, const struct rect *rect);
static bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy);

char *default_stylesheet_url;
char *adblock_stylesheet_url;

#if defined(__MORPHOS__)
struct Library *ZBase;
struct Library *JFIFBase;
#endif

#ifdef TTENGINE
struct Library *TTEngineBase;
#endif
struct Library *IconBase;
struct IntuitionBase *IntuitionBase;
struct Library *SocketBase;
struct Library *AsyncIOBase;
struct Library *OpenURLBase;
struct Library *DiskfontBase;
struct Library *CyberGfxBase;
struct Library *MUIMasterBase;
//Added for AmigaOS 3.x
struct Library *AslBase, *LayersBase;
struct UtilityBase *UtilityBase = NULL;
struct GfxBase *GfxBase = NULL;
struct Library *IFFParseBase = NULL;
struct LocaleBase *LocaleBase = NULL;
struct Library *KeymapBase = NULL;
struct Device *TimerBase = NULL;
static struct timerequest *timer_request = NULL;
static struct MsgPort *timer_port = NULL;
/////////
#if !defined(__MORPHOS2__)
struct Library *CodesetsBase;
#endif

STATIC struct MinList download_list = { (APTR)&download_list.mlh_Tail, NULL, (APTR)&download_list };
STATIC struct MinList window_list = { (APTR)&window_list.mlh_Tail, NULL, (APTR)&window_list };
STATIC LONG process_priority;

static struct gui_download_table mui_download_table;

static bool frontend_has_pending_work(void)
{
	return browser_reformat_pending || mui_redraw_pending || mui_schedule_has_tasks();
}

static void service_frontend(bool allow_block)
{
	if (netsurf_quit) {
		return;
	}

	static unsigned long call_count = 0;
	call_count++;

	bool log_this_call = false;
	if (call_count <= 50) {
		log_this_call = true;
	} else if (call_count <= 1000 && (call_count % 50) == 0) {
		log_this_call = true;
	}

	const bool schedule_pending_before = mui_schedule_has_tasks();
	const bool work_pending_before = browser_reformat_pending || mui_redraw_pending;
	const bool pending_before = schedule_pending_before || work_pending_before;
	const bool poll_only = !allow_block || work_pending_before;
	if (log_this_call) {
		write_to_log("service_frontend: allow_block=%d pending_before=%d poll_only=%d\n",
		    allow_block ? 1 : 0,
		    pending_before ? 1 : 0,
		    poll_only ? 1 : 0);
	}

	ULONG signals = netsurf_check_events(poll_only, schedule_sig);
	if (log_this_call) {
		write_to_log("service_frontend: netsurf_check_events returned signals=0x%08lx\n",
		    signals);
	}

	if (signals & SIGBREAKF_CTRL_C) {
		netsurf_quit = true;
		return;
	}

	if (signals & schedule_sig) {
		if (log_this_call) {
			write_to_log("service_frontend: schedule signal set -> calling mui_schedule_poll\n");
		}
		mui_schedule_poll();
		SetSignal(0, schedule_sig);
	} else if (schedule_pending_before && allow_block && poll_only) {
		static unsigned forced_poll_spam_guard = 0;
		if (forced_poll_spam_guard < 10 && log_this_call) {
			write_to_log("service_frontend: schedule pending without signal -> forcing poll\n");
			forced_poll_spam_guard++;
		}
		mui_schedule_poll();
		SetSignal(0, schedule_sig);
	} else {
		static unsigned idle_spam_guard = 0;
		if (!work_pending_before && !allow_block) {
			Delay(1);
		} else if (!pending_before && allow_block && idle_spam_guard < 10 && log_this_call) {
			write_to_log("service_frontend: idle after netsurf_check_events\n");
			idle_spam_guard++;
		}
	}
}


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

void gui_window_destroy(struct gui_window *g);

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
	if (cleanup_done) {
		return;
	}
	cleanup_done = true;

	netsurf_exit();
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

	mui_clipboard_free();

	/* When NetSurf is aborted we must cleanup everything manually */
	//mui_fetch_file_finalise(NULL);

	abort_downloads();

	if (timer_request) {
		if (TimerBase) {
			CloseDevice((struct IORequest *)timer_request);
			TimerBase = NULL;
		}
		DeleteIORequest((struct IORequest *)timer_request);
		timer_request = NULL;
	}
	if (timer_port) {
		DeleteMsgPort(timer_port);
		timer_port = NULL;
	}

#if !defined(__MORPHOS2__)
	if (CodesetsBase) {
		CloseLibrary(CodesetsBase);
		CodesetsBase = NULL;
	}
#endif

#if defined(__MORPHOS__)
	if (JFIFBase) {
		CloseLibrary(JFIFBase);
		JFIFBase = NULL;
	}
	if (ZBase) {
		CloseLibrary(ZBase);
		ZBase = NULL;
	}
#endif

	if (OpenURLBase) {
		CloseLibrary(OpenURLBase);
		OpenURLBase = NULL;
	}
	if (AsyncIOBase) {
		CloseLibrary(AsyncIOBase);
		AsyncIOBase = NULL;
	}
	if (AslBase) {
		CloseLibrary(AslBase);
		AslBase = NULL;
	}
	//CloseLibrary(SocketBase);
#ifdef TTENGINE
	if (TTEngineBase) {
		CloseLibrary(TTEngineBase);
		TTEngineBase = NULL;
	}
#endif
	if (DiskfontBase) {
		CloseLibrary(DiskfontBase);
		DiskfontBase = NULL;
	}
	if (IntuitionBase) {
		CloseLibrary((struct Library *)IntuitionBase);
		IntuitionBase = NULL;
	}
	if (IconBase) {
		CloseLibrary(IconBase);
		IconBase = NULL;
	}
	if (CyberGfxBase) {
		CloseLibrary(CyberGfxBase);
		CyberGfxBase = NULL;
	}
	if (GfxBase) {
		CloseLibrary((struct Library *)GfxBase);
		GfxBase = NULL;
	}
	if (MUIMasterBase) {
		CloseLibrary(MUIMasterBase);
		MUIMasterBase = NULL;
	}

	if (UtilityBase) {
		CloseLibrary((struct Library *)UtilityBase);
		UtilityBase = NULL;
	}
	if (IFFParseBase) {
		CloseLibrary(IFFParseBase);
		IFFParseBase = NULL;
	}
	if (LocaleBase) {
		CloseLibrary((struct Library *)LocaleBase);
		LocaleBase = NULL;
	}
	if (LayersBase) {
		CloseLibrary(LayersBase);
		LayersBase = NULL;
	}
	if (KeymapBase) {
		CloseLibrary(KeymapBase);
		KeymapBase = NULL;
	}
	
	#if defined(__MORPHOS__)
	NewSetTaskAttrsA(NULL, &process_priority, sizeof(ULONG), TASKINFOTYPE_PRI, NULL);
	#else
	SetTaskPri(SysBase->ThisTask, process_priority);
	#endif
}

static LONG startup(void)
{
#ifdef TTENGINE
#if defined(__MORPHOS2__)
#define TTENGINE_VERSION 8
#else
#define TTENGINE_VERSION 7
#endif
#endif

    LOG(("DEBUG: Starting library initialization\n"));

    // Open muimaster.library
	MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (MUIMasterBase) {
        LOG(("DEBUG: muimaster.library opened, version %ld.%ld\n", 
             MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision));
    } else {
        LOG(("ERROR: Failed to open muimaster.library version 19+\n"));
    }
    if (!MUIMasterBase) goto cleanup;

    // Open cybergraphics.library
    CyberGfxBase = OpenLibrary("cybergraphics.library", 39);
    LOG(("DEBUG: cybergraphics.library %s\n", CyberGfxBase ? "opened" : "FAILED to open"));
    if (!CyberGfxBase) goto cleanup;

	// Open GFXBase
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
	LOG(("DEBUG: graphics.library %s\n", GfxBase ? "opened" : "FAILED to open"));
	if (!GfxBase) goto cleanup;

    #ifdef __MORPHOS__
    // Open z.library
    ZBase = OpenLibrary("z.library", 51);
    LOG(("DEBUG: z.library %s\n", ZBase ? "opened" : "FAILED to open"));
    if (!ZBase) goto cleanup;

    // Open jfif.library
    JFIFBase = OpenLibrary("jfif.library", 0);
    LOG(("DEBUG: jfif.library %s\n", JFIFBase ? "opened" : "FAILED to open"));
    if (!JFIFBase) goto cleanup;
    #endif
    //Open DiskfontBase
    DiskfontBase = OpenLibrary("diskfont.library", 39);
    if (!DiskfontBase) {
            LOG(("DEBUG: diskfont.library not available\n"));
            DiskfontBase = NULL; // Set to NULL if not available
        } else {
            LOG(("DEBUG: diskfont.library opened successfully\n"));
        } 	
	LOG(("DEBUG: Opening timer.device for TimerBase"));

	timer_port = CreateMsgPort();
	if (timer_port == NULL) {
		LOG(("ERROR: Failed to create timer message port"));
		goto cleanup;
	}

	timer_request = (struct timerequest *)CreateIORequest(timer_port, sizeof(struct timerequest));
	if (timer_request == NULL) {
		LOG(("ERROR: Failed to create IORequest for timer.device"));
		DeleteMsgPort(timer_port);
		timer_port = NULL;
		goto cleanup;
	}

	if (OpenDevice("timer.device", UNIT_VBLANK, (struct IORequest *)timer_request, 0) == 0) {
		TimerBase = timer_request->tr_node.io_Device;
		LOG(("DEBUG: TimerBase opened successfully: %p", TimerBase));
	} else {
		LOG(("ERROR: Failed to open timer.device for TimerBase"));
		DeleteIORequest((struct IORequest *)timer_request);
		timer_request = NULL;
		DeleteMsgPort(timer_port);
		timer_port = NULL;
		goto cleanup;
	}
    // Open asl.library
    AslBase = OpenLibrary("asl.library", 37);
    LOG(("DEBUG: asl.library %s\n", AslBase ? "opened" : "FAILED to open"));
    if (!AslBase) goto cleanup;

#ifdef TTENGINE
    // Open ttengine.library
    TTEngineBase = OpenLibrary("ttengine.library", TTENGINE_VERSION);
    LOG(("DEBUG: ttengine.library %s\n", TTEngineBase ? "opened" : "FAILED to open"));
    if (!TTEngineBase) goto cleanup;
#undef TTENGINE_VERSION
#endif

    // Open icon.library
    IconBase = OpenLibrary("icon.library", 0);
    LOG(("DEBUG: icon.library %s\n", IconBase ? "opened" : "FAILED to open"));
    if (!IconBase) goto cleanup;

    // Open intuition.library
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    LOG(("DEBUG: intuition.library %s\n", IntuitionBase ? "opened" : "FAILED to open"));
    if (!IntuitionBase) goto cleanup;

    // Open bsdsocket.library
	//if (!SocketBase)
    //SocketBase = OpenLibrary("bsdsocket.library", 0);
    //LOG(("DEBUG: bsdsocket.library %s\n", SocketBase ? "opened" : "FAILED to open"));
   // if (!SocketBase) goto cleanup;

    // Open asyncio.library
    AsyncIOBase = OpenLibrary("asyncio.library", 0);
    LOG(("DEBUG: asyncio.library %s\n", AsyncIOBase ? "opened" : "FAILED to open"));
    if (!AsyncIOBase) goto cleanup;

    // Open utility.library
    UtilityBase = (struct UtilityBase *)OpenLibrary("utility.library", 37);
    LOG(("DEBUG: utility.library %s\n", UtilityBase ? "opened" : "FAILED to open"));
    if (!UtilityBase) goto cleanup;

    // Open iffparse.library
    IFFParseBase = OpenLibrary("iffparse.library", 0);
    LOG(("DEBUG: iffparse.library %s\n", IFFParseBase ? "opened" : "FAILED to open"));
    if (!IFFParseBase) goto cleanup;

	LayersBase = OpenLibrary("layers.library", 0);
	LOG(("DEBUG: layers.library %s\n", LayersBase ? "opened" : "FAILED to open"));
	if (!LayersBase) {
		LOG(("DEBUG: layers.library not available\n"));
		goto cleanup;
	}
    // Open locale.library
    LocaleBase = OpenLibrary("locale.library", 37);
    LOG(("DEBUG: locale.library %s\n", LocaleBase ? "opened" : "FAILED to open"));
    if (!LocaleBase) {
        LOG(("DEBUG: locale.library not available\n"));
        goto cleanup;
    }
	KeymapBase = OpenLibrary("keymap.library", 0);
	LOG(("DEBUG: keymap.library %s\n", KeymapBase ? "opened" : "FAILED to open"));
	if (!KeymapBase) {
		LOG(("DEBUG: keymap.library not available\n"));
		goto cleanup;
	}
	    // Initialize classes
    LOG(("DEBUG: Initializing classes...\n"));
    if (!classes_init()) {
        LOG(("DEBUG: classes_init FAILED\n"));
        goto cleanup;
    }
    LOG(("DEBUG: classes_init succeeded\n"));

    // Initialize mui_schedule
    LOG(("DEBUG: Initializing mui_schedule...\n"));
    if (!mui_schedule_init()) {
        LOG(("DEBUG: mui_schedule_init FAILED\n"));
        goto cleanup;
    }
    LOG(("DEBUG: mui_schedule_init succeeded\n"));

    // Initialize mui_clipboard
    LOG(("DEBUG: Initializing mui_clipboard...\n"));
    if (!mui_clipboard_init()) {
        LOG(("DEBUG: mui_clipboard_init FAILED\n"));
        goto cleanup;
    }
    LOG(("DEBUG: mui_clipboard_init succeeded\n"));

    // Initialize methodstack
    LOG(("DEBUG: Initializing methodstack...\n"));
    methodstack_init();
    LOG(("DEBUG: methodstack_init succeeded\n"));


    // Load transferanimclass
    LOG(("DEBUG: Loading transferanimclass...\n"));
    transferanimclass_load(); //arczi
    LOG(("DEBUG: transferanimclass loaded\n"));

    LOG(("DEBUG: Startup completed successfully\n"));
    return TRUE;

cleanup:
    LOG(("DEBUG: Cleaning up due to initialization failure\n"));
	if (timer_request) {
		if (TimerBase) {
			CloseDevice((struct IORequest *)timer_request);
			TimerBase = NULL;
		}
		DeleteIORequest((struct IORequest *)timer_request);
		timer_request = NULL;
	}
	if (timer_port) {
		DeleteMsgPort(timer_port);
		timer_port = NULL;
	}
	if (KeymapBase) CloseLibrary(KeymapBase);
    if (LocaleBase) CloseLibrary((struct Library *)LocaleBase);
    if (IFFParseBase) CloseLibrary(IFFParseBase);
    if (UtilityBase) CloseLibrary((struct Library *)UtilityBase);
    if (AsyncIOBase) CloseLibrary(AsyncIOBase);
    if (SocketBase) CloseLibrary(SocketBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
	if (IconBase) CloseLibrary(IconBase);
#ifdef TTENGINE
	if (TTEngineBase) CloseLibrary(TTEngineBase);
#endif
	if (DiskfontBase) CloseLibrary(DiskfontBase);
    if (AslBase) CloseLibrary(AslBase);
    #ifdef __MORPHOS__
    if (JFIFBase) CloseLibrary(JFIFBase);
    if (ZBase) CloseLibrary(ZBase);
    #endif
    if (AsyncIOBase) CloseLibrary(AsyncIOBase);
    //if (SocketBase) CloseLibrary(SocketBase);
    if (MUIMasterBase) CloseLibrary(MUIMasterBase);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
	if (GfxBase) CloseLibrary((struct Library *)GfxBase);
	if (LayersBase) CloseLibrary(LayersBase);
    return FALSE;
}

/**
 * Set option defaults for mui frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror set_defaults(struct nsoption_s *defaults)
{

	/* Set defaults for absent option strings */
	nsoption_setnull_charp(cookie_file, strdup("PROGDIR:Resources/Cookies"));
	nsoption_setnull_charp(cookie_jar, strdup("PROGDIR:Resources/Cookies"));
	//nsoption_setnull_charp(url_file, strdup("PROGDIR:Resources/URLs"));
	nsoption_setnull_charp(ca_bundle, strdup("PROGDIR:Resources/ca-bundle"));
	nsoption_setnull_charp(ca_path, strdup("PROGDIR:Resources/ca-path"));
	nsoption_setnull_charp(homepage_url, strdup("file:///PROGDIR:Resources/Welcome.html"));

	if (defaults != NULL) {
		if (defaults[NSOPTION_redraw_tile_size_x].value.i <= 0) {
			defaults[NSOPTION_redraw_tile_size_x].value.i = 100;
		}
		if (defaults[NSOPTION_redraw_tile_size_y].value.i <= 0) {
			defaults[NSOPTION_redraw_tile_size_y].value.i = 100;
		}
	}

	//nsoption_setnull_charp(cache_dir, strdup("PROGDIR:Resources/Cache"));
	nsoption_setnull_charp(homepage_url, strdup("www.netsurf-browser.org/welcome"));

	if (nsoption_charp(cookie_file) == NULL ||
	    nsoption_charp(cookie_jar) == NULL) {
		NSLOG(netsurf, INFO,"Failed initialising cookie options");
		return NSERROR_BAD_PARAMETER;
	}

	return NSERROR_OK;
}

void gui_launch_url(const char *url);

void gui_quit(void)
{
		urldb_save_cookies(nsoption_charp(cookie_jar));
		set_log0_enabled(false);
		close_log_file();
}

static struct gui_misc_table mui_misc_table = {
	.schedule = mui_schedule,
	.launch_url = gui_launch_url,

	.quit = gui_quit,
};

//main

void gui_init2(int argc, char** argv)
{
	LOG(("DEBUG: gui_init2 called"));
	LOG(("DEBUG: argc=%d, argv=%p", argc, argv));
	LONG priority, got_window;
	struct nsurl *url;
	nserror error;

	priority = -1;
	got_window = 0;

	//mui_fetch_file_register();
	LOG(("DEBUG: gui_init2: mui_fetch_file_register called"));
#ifdef MULTITHREADED
	NewSetTaskAttrsA(NULL, &priority, sizeof(ULONG), TASKINFOTYPE_PRI, NULL);
#endif

	if (argc) {
		LOG(("DEBUG: gui_init2: argc > 0, processing command line arguments"));
		enum
		{
			ARG_URL = 0,
			ARG_COUNT
		};

		STATIC CONST TEXT template[] = "URL/A";
		struct RDArgs *args;
		IPTR array[ARG_COUNT] = {0}; //arczi było {NULL}

		if (args = ReadArgs(template, array, NULL)) {
			if (array[ARG_URL]) {
				STRPTR url_str;

				url_str = (char *)DupStr((char *)array[ARG_URL]);
				LOG(("DEBUG: gui_init2: URL from command line: %s", url_str));
				if (url_str) {
					error = nsurl_create(url_str, &url);
					LOG(("DEBUG: gui_init2: nsurl_create returned %d", error));
					if (error == NSERROR_OK) {
						struct browser_window *bw;
						LOG(("DEBUG: gui_init2: URL created successfully"));
						error = browser_window_create(BW_CREATE_HISTORY, url, NULL, NULL, &bw);
						if (error == NSERROR_OK)
							got_window = 1;
						nsurl_unref(url);
					}
					free(url_str);
				}
			}

			FreeArgs(args);
		}
	} else {
		LOG(("DEBUG: gui_init2: argc == 0, checking Workbench startup message"));
		extern struct WBStartup *_WBenchMsg;
		struct WBStartup *WBenchMsg;
		struct WBArg *wbarg;
		ULONG i;

		WBenchMsg = (struct WBStartup *)argv;

		for (i = 1, wbarg = WBenchMsg->sm_ArgList + 1; i < WBenchMsg->sm_NumArgs; i++, wbarg++) {
			if (wbarg->wa_Lock && *wbarg->wa_Name) {
				STRPTR path = GetNameFromLock(wbarg->wa_Lock);
				LOG(("DEBUG: gui_init2: Path from lock: %s", path));
				if (path) {
					ULONG length;
					STRPTR file;

					length = strlen(path) + strlen(wbarg->wa_Name) + 4;
					file = AllocTaskPooled(length);
					LOG(("DEBUG: gui_init2: Allocated file %s buffer of length %lu",file, length));
					if (file) {
						strcpy(file, path);
						AddPart(file, wbarg->wa_Name, length);

						//error = nsurl_create_from_text(file, &url);
						error = nsurl_create(file, &url);
						LOG(("DEBUG: gui_init2: Converted path to URL"));
						if (error == NSERROR_OK) {
							struct browser_window *bw;
							error = browser_window_create(BW_CREATE_HISTORY, url, NULL, NULL, &bw);
							if (error == NSERROR_OK)
								got_window = 1;
							nsurl_unref(url);
						}

						FreeTaskPooled(file, length);
					}

					FreeVecTaskPooled(path);
				}
			}
		}
	}

    if (!got_window) {
        LOG(("DEBUG: No window from command line, creating default page"));
        methodstack_push_imm(application, 2, MM_Window_MenuAction, MNA_NEW_PAGE);
    }
	LOG(("DEBUG: function gui_init2 completed"));
}

void gui_multitask(void)
{
	service_frontend(false);
}

void gui_poll1(bool active)
{
	service_frontend(!active);
}
void gui_poll(void)
{
	service_frontend(true);
}

void gui_poll3(/*bool active*/)
{
	service_frontend(false);
}
struct gui_window *gui_create_browser_window(struct browser_window *bw,
        struct browser_window *clone, bool new_tab)
{
    LOG(("DEBUG: gui_create_browser_window ENTRY"));
    LOG(("DEBUG: bw=%p, clone=%p, new_tab=%s", bw, clone, new_tab ? "true" : "false"));
    
    struct gui_window *gw;

    LOG(("DEBUG: Allocating gui_window structure"));
    gw = AllocMem(sizeof(*gw), MEMF_ANY);

    if (gw) {
        LOG(("DEBUG: gui_create_browser_window: gw allocated = %p", gw));
        gw->bw = bw;
        gw->pointertype = -1;
        LOG(("DEBUG: gui_create_browser_window - bw = %p", bw));
        LOG(("DEBUG: gui_create_browser_window - gw->bw = %p", gw->bw));
        LOG(("DEBUG: About to call methodstack_push_sync for MM_Application_AddBrowser"));
        LOG(("DEBUG: application object = %p", application));
        
        gw->obj = (APTR)methodstack_push_sync(application, 2,
                MM_Application_AddBrowser, gw);

        LOG(("DEBUG: methodstack_push_sync returned obj = %p", gw->obj));

        if (gw->obj == NULL) {
            LOG(("ERROR: methodstack_push_sync failed - freeing gw"));
            FreeMem(gw, sizeof(*gw));
            gw = NULL;
        } else {
            gw->BitMap = NULL;
            gw->Layer = NULL;
            gw->LayerInfo = NULL;
            //gw->RastPort = NULL;
            
            LOG(("DEBUG: gui_window created successfully"));
            LOG(("DEBUG: gw=%p, gw->obj=%p", gw, gw->obj));
            
            ADDTAIL(&window_list, gw);
            LOG(("DEBUG: Added to window_list"));
        }
    } else {
        LOG(("ERROR: Failed to allocate gui_window structure"));
    }

    LOG(("DEBUG: gui_create_browser_window returning %p", gw));
    return gw;
}



void gui_window_destroy(struct gui_window *g)
{
	REMOVE(g);

	if (g->win && !netsurf_quit)
		methodstack_push_sync(g->win, 2, MM_Window_RemovePage, g->obj);

	if (g->Layer) {
#ifdef TTENGINE
		if (g->RastPort) {
			TT_DoneRastPort(g->RastPort);
		}
#endif
		DeleteLayer(0, g->Layer);
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

static bool
mui_redraw_tiles_impl(struct gui_window *g,
	struct RastPort *rp,
	int viewport_left,
	int viewport_top,
	int viewport_w,
	int viewport_h,
	int scroll_x,
	int scroll_y,
	int x0,
	int y0,
	int x1,
	int y1,
	void (*flush_fn)(struct gui_window *g,
		void *ctx,
		int screen_x0,
		int screen_y0,
		int screen_x1,
		int screen_y1),
	void *flush_ctx)
{
	if (g == NULL || g->bw == NULL) {
		return false;
	}

	if (!browser_window_redraw_ready(g->bw)) {
		return true;
	}

	if (rp == NULL) {
		LOG(("ERROR: mui_redraw_tiles_impl called with NULL RastPort"));
		return false;
	}

	if (viewport_w <= 0 || viewport_h <= 0) {
		return false;
	}

	int content_w = viewport_w;
	int content_h = viewport_h;
	if (browser_window_get_extents(g->bw, false, &content_w, &content_h) != NSERROR_OK) {
		content_w = viewport_w;
		content_h = viewport_h;
	}

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > content_w) x1 = content_w;
	if (y1 > content_h) y1 = content_h;
	if (x1 <= x0 || y1 <= y0) {
		return true;
	}

	int visible_x0 = x0;
	if (visible_x0 < scroll_x) visible_x0 = scroll_x;
	int visible_y0 = y0;
	if (visible_y0 < scroll_y) visible_y0 = scroll_y;
	int visible_x1 = x1;
	int max_vis_x = scroll_x + viewport_w;
	if (visible_x1 > max_vis_x) visible_x1 = max_vis_x;
	int visible_y1 = y1;
	int max_vis_y = scroll_y + viewport_h;
	if (visible_y1 > max_vis_y) visible_y1 = max_vis_y;

	if (visible_x1 <= visible_x0 || visible_y1 <= visible_y0) {
		return true;
	}

	int tile_w = nsoption_int(redraw_tile_size_x);
	if (tile_w <= 0) {
		tile_w = 100;
	}
	if (tile_w > viewport_w) {
		tile_w = viewport_w;
	}

	int tile_h = nsoption_int(redraw_tile_size_y);
	if (tile_h <= 0) {
		tile_h = 100;
	}
	if (tile_h > viewport_h) {
		tile_h = viewport_h;
	}

	LOG(("DEBUG: mui_redraw_tiles viewport=%dx%d visible=(%d,%d)-(%d,%d) tile=%dx%d scroll=(%d,%d)",
	    viewport_w, viewport_h,
	    visible_x0, visible_y0, visible_x1, visible_y1,
	    tile_w, tile_h,
	    scroll_x, scroll_y));

	struct RastPort *prev_rp = renderinfo.rp;
	renderinfo.rp = rp;
	int prev_origin_x = renderinfo.origin_x;
	int prev_origin_y = renderinfo.origin_y;
	int prev_width = renderinfo.width;
	int prev_height = renderinfo.height;
	renderinfo.width = viewport_w;
	renderinfo.height = viewport_h;
	renderinfo.origin_x = viewport_left;
	renderinfo.origin_y = viewport_top;

	int origin_x = viewport_left - scroll_x;
	int origin_y = viewport_top - scroll_y;

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &muiplot,
		.priv = rp
	};

	bool ok = true;

	for (int doc_y = visible_y0; doc_y < visible_y1 && ok; doc_y += tile_h) {
		int next_doc_y = doc_y + tile_h;
		if (next_doc_y > visible_y1) next_doc_y = visible_y1;
		int screen_y0 = viewport_top + (doc_y - scroll_y);
		int screen_y1 = viewport_top + (next_doc_y - scroll_y);
		if (screen_y1 <= screen_y0) {
			continue;
		}

		for (int doc_x = visible_x0; doc_x < visible_x1; doc_x += tile_w) {
			int next_doc_x = doc_x + tile_w;
			if (next_doc_x > visible_x1) next_doc_x = visible_x1;
			int screen_x0 = viewport_left + (doc_x - scroll_x);
			int screen_x1 = viewport_left + (next_doc_x - scroll_x);
			if (screen_x1 <= screen_x0) {
				continue;
			}

			renderinfo.origin_x = screen_x0;
			renderinfo.origin_y = screen_y0;
			renderinfo.width = screen_x1 - screen_x0;
			renderinfo.height = screen_y1 - screen_y0;

			struct rect clip = {
				.x0 = screen_x0,
				.y0 = screen_y0,
				.x1 = screen_x1,
				.y1 = screen_y1
			};

			if (!browser_window_redraw(g->bw, origin_x, origin_y, &clip, &ctx)) {
				ok = false;
				break;
			}

			if (flush_fn != NULL) {
				flush_fn(g, flush_ctx, screen_x0, screen_y0, screen_x1, screen_y1);
			}
		}
	}

	renderinfo.origin_x = prev_origin_x;
	renderinfo.origin_y = prev_origin_y;
	renderinfo.width = prev_width;
	renderinfo.height = prev_height;
	renderinfo.rp = prev_rp;

	return ok;
}

bool
mui_redraw_tiles_surface(struct gui_window *g,
	struct RastPort *rp,
	int viewport_w,
	int viewport_h,
	int scroll_x,
	int scroll_y,
	void (*flush_fn)(struct gui_window *g,
		void *ctx,
		int screen_x0,
		int screen_y0,
		int screen_x1,
		int screen_y1),
	void *flush_ctx)
{
	if (rp == NULL) {
		return false;
	}

	int x0 = scroll_x;
	int y0 = scroll_y;
	int x1 = scroll_x + viewport_w;
	int y1 = scroll_y + viewport_h;

	return mui_redraw_tiles_impl(g, rp,
	    0, 0,
	    viewport_w, viewport_h,
	    scroll_x, scroll_y,
	    x0, y0, x1, y1,
	    flush_fn,
	    flush_ctx);
}

static bool
mui_redraw_tiles(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	if (g == NULL || g->obj == NULL) {
		return false;
	}

	if (!_win(g->obj)) {
		LOG(("WARNING: Window not ready, skipping redraw"));
		return false;
	}

	if (!browser_window_redraw_ready(g->bw)) {
		return true;
	}

	struct RastPort *rp = _rp(g->obj);
	if (rp == NULL) {
		rp = (struct RastPort *)g->RastPort;
		if (rp == NULL) {
			LOG(("ERROR: No RastPort available for redraw"));
			return false;
		}
	}

	int viewport_left = _mleft(g->obj);
	int viewport_top = _mtop(g->obj);
	int viewport_w = _mwidth(g->obj);
	int viewport_h = _mheight(g->obj);

	int scroll_x = 0;
	int scroll_y = 0;
	if (!gui_window_get_scroll(g, &scroll_x, &scroll_y)) {
		scroll_x = 0;
		scroll_y = 0;
	}

	return mui_redraw_tiles_impl(g, rp,
	    viewport_left, viewport_top,
	    viewport_w, viewport_h,
	    scroll_x, scroll_y,
	    x0, y0, x1, y1,
	    NULL,
	    NULL);
}

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	LOG(("DEBUG: gui_window_redraw called for browser window %p", g ? g->bw : NULL));
	if (!mui_redraw_tiles(g, x0, y0, x1, y1)) {
		LOG(("ERROR: gui_window_redraw failed"));
	}
}

void gui_window_redraw_window(struct gui_window *g)
{
	LOG(("DEBUG: gui_window_redraw_window called for browser window %p", g->bw));
	methodstack_push_sync(g->obj, 1, MM_Browser_Redraw);
}

void gui_window_update_box(struct gui_window *g, const union content_msg_data *data)
{
	if (g == NULL || data == NULL) {
		return;
	}

	LOG(("DEBUG: gui_window_update_box called for browser window %p", g->bw));

	int x0 = data->redraw.x;
	int y0 = data->redraw.y;
	int x1 = x0 + (int)data->redraw.width;
	int y1 = y0 + (int)data->redraw.height;

	if (!mui_redraw_tiles(g, x0, y0, x1, y1)) {
		LOG(("ERROR: gui_window_update_box failed"));
	}
}

static bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	methodstack_push(g->obj, 3, OM_GET, MUIA_Virtgroup_Left, sx);
	methodstack_push_imm(g->obj, 3, OM_GET, MUIA_Virtgroup_Top, sy);
	return true;
}

static nserror gui_window_set_scroll_rect(struct gui_window *g, const struct rect *rect)
{
	IPTR tags[5];
	int sx = 0;
	int sy = 0;
	int viewport_w = 0;
	int viewport_h = 0;
	int content_w = 0;
	int content_h = 0;

	if (g == NULL || g->obj == NULL || g->bw == NULL || rect == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	if (!browser_window_has_content(g->bw)) {
		return NSERROR_BAD_PARAMETER;
	}

	if (rect->x0 > 0) {
		sx = rect->x0;
	}
	if (rect->y0 > 0) {
		sy = rect->y0;
	}

	if (gui_window_get_dimensions(g, &viewport_w, &viewport_h, false) != NSERROR_OK) {
		viewport_w = 0;
		viewport_h = 0;
	}

	if (browser_window_get_extents(g->bw, false, &content_w, &content_h) != NSERROR_OK) {
		content_w = viewport_w;
		content_h = viewport_h;
	}

	int max_x = content_w - viewport_w;
	int max_y = content_h - viewport_h;

	if (max_x < 0) {
		max_x = 0;
	}
	if (max_y < 0) {
		max_y = 0;
	}

	if (sx > max_x) {
		sx = max_x;
	}
	if (sy > max_y) {
		sy = max_y;
	}

	if (viewport_w >= content_w) {
		sx = 0;
	}
	if (viewport_h >= content_h) {
		sy = 0;
	}

	if (sx < 0) {
		sx = 0;
	}
	if (sy < 0) {
		sy = 0;
	}

	tags[0] = MUIA_Virtgroup_Left;
	tags[1] = sx;
	tags[2] = MUIA_Virtgroup_Top;
	tags[3] = sy;
	tags[4] = TAG_DONE;

	methodstack_push_sync(g->obj, 2, OM_SET, tags);
	return NSERROR_OK;
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	struct rect target = { sx, sy, sx, sy };
	(void)gui_window_set_scroll_rect(g, &target);
}

void gui_window_scroll_visible(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	struct rect target = { x0, y0, x1, y1 };
	(void)gui_window_set_scroll_rect(g, &target);
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0, int x1, int y1)
{
}

/**
 * Find the current dimensions of a amiga browser window content area.
 *
 * \param gw The gui window to measure content area of.
 * \param width receives width of window
 * \param height receives height of window
 * \return NSERROR_OK on sucess and width and height updated
 *          else error code.
 */
static nserror gui_window_get_dimensions(struct gui_window *g, int *width, int *height, bool scaled)
{
	struct IBox bbox;
	int w, h;

	methodstack_push_sync(g->obj, 3, OM_GET, MA_Browser_Box, &bbox);
	
	LOG(("gui_window_get_dimensions() called, bbox: Left=%d, Top=%d, Width=%d, Height=%d",
		bbox.Left, bbox.Top, bbox.Width, bbox.Height));

	w = bbox.Width;
	h = bbox.Height;

#if 0
	if (scaled)
	{
		w /= g->bw->scale;
		h /= g->bw->scale;
	}
#else
	//#warning gui_window_get_dimensions(): scaled boolean not supported
#endif

	*width = w;
	*height = h;

	return NSERROR_OK;
}

void gui_window_update_extent(struct gui_window *g)
{
	LOG(("gui_window_update_extent() called"));

	if (g == NULL || g->bw == NULL) {
		LOG(("Warning: gui_window_update_extent called with NULL gui_window or browser_window"));
		return;
	}

	int width;
	int height;
	// Use false to get unscaled document dimensions
	browser_window_get_extents(g->bw, false, &width, &height);

	methodstack_push(g->obj, 3, MM_Browser_SetContentSize, width, height);

	LOG(("Updating content size to %d x %d", width, height));
}


void gui_window_set_status(struct gui_window *g, const char *text)
{
	global_obj = g->obj;

	methodstack_push_sync(g->obj, 3, MUIM_Set, MA_Browser_StatusText, text);
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{

	ULONG pointertype = POINTERTYPE_NORMAL;

	LOG(("DEBUG: gui_window_set_pointer called with shape=%d", shape));

	switch (shape) {
	case GUI_POINTER_DEFAULT:
		pointertype = POINTERTYPE_NORMAL;
		break;

	case GUI_POINTER_POINT:
		pointertype = POINTERTYPE_LINK;
		LOG(("DEBUG: GUI_POINTER_POINT -> POINTERTYPE_LINK = %d", pointertype));
		break;

	case GUI_POINTER_CARET:
		pointertype = POINTERTYPE_TEXT;
		break;

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
//	#endif
}

void gui_window_hide_pointer(struct gui_window *g)
{
	if (g->pointertype != POINTERTYPE_INVISIBLE) {
		g->pointertype = POINTERTYPE_INVISIBLE;
		methodstack_push(g->obj, 3, MUIM_Set, MA_Browser_Pointer, POINTERTYPE_INVISIBLE);
	}
}

static nserror
gui_window_set_url(struct gui_window *g, nsurl *url)
{
	LOG(("DEBUG: gui_window_set_url called with URL: %s", nsurl_access(url)));

	methodstack_push_sync(g->obj, 3, MUIM_Set, MA_Browser_URL, nsurl_access(url));
	return NSERROR_OK;
}

void gui_window_start_throbber(struct gui_window *g)
{
	methodstack_push_imm(g->obj, 3, MUIM_Set, MA_Browser_Loading, TRUE);
	//mui_redraw_pending = TRUE; // Ensure the throbber is redrawn
}

void gui_window_stop_throbber(struct gui_window *g)
{
	methodstack_push_imm(g->obj, 3, MUIM_Set, MA_Browser_Loading, FALSE);
	//mui_redraw_pending = false; // Ensure the throbber is redrawn
}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{

}


void gui_window_remove_caret(struct gui_window *g)
{

}

/**
 * Called when the gui_window has new content.
 *
 * \param  g  the gui_window that has new content
 */

static void gui_window_new_content22(struct gui_window *g)
{
	struct hlcache_handle *c = browser_window_get_content(g->bw);
	LOG(("DEBUG: gui_window_new_content called for browser window"));
	if (c) {
		LOG(("DEBUG: Content available, setting content type"));
		content_type type = content_get_type(c);
		methodstack_push(g->obj, 2, MM_Browser_SetContentType, type);
	}
	else
		LOG(("DEBUG: gui_window_new_content called but no content available for browser window %p", g->bw));
}
static void gui_window_new_content(struct gui_window *g)
{
    struct hlcache_handle *c = browser_window_get_content(g->bw);
    LOG(("DEBUG: gui_window_new_content called for browser window"));
    
    if (c) {
        LOG(("DEBUG: Content available, setting content type"));
        content_type type = content_get_type(c);
        methodstack_push(g->obj, 2, MM_Browser_SetContentType, type);
        
        // ⭐ NOWE: Wywołaj content ready trigger
        LOG(("DEBUG: Content ready - triggering redraw"));
        mui_trigger_content_ready_redraw(g->obj);
    }
    else {
        LOG(("DEBUG: gui_window_new_content called but no content available for browser window %p", g->bw));
    }
}
bool gui_window_scroll_start(struct gui_window *g)
{
	//return true;
}

bool gui_window_box_scroll_start(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	//return true;
}

bool gui_window_frame_resize_start(struct gui_window *g)
{
	//return true;
}

void gui_window_save_as_link(struct gui_window *g, struct hlcache_handle *c)
{
}

void gui_window_set_scale(struct gui_window *g, float scale)
{
}

static void
mui_download_window_cleanup(struct gui_download_window *dw)
{
	if (!dw)
		return;

	if (dw->fh)
	{
		CloseAsync(dw->fh);
		dw->fh = NULL;
	}

	if (dw->node.mln_Succ || dw->node.mln_Pred)
	{
		REMOVE(dw);
		dw->node.mln_Succ = NULL;
		dw->node.mln_Pred = NULL;
	}

	if (dw->ctx)
	{
		download_context_destroy(dw->ctx);
		dw->ctx = NULL;
	}

	FreeMem(dw, sizeof(*dw));
}

struct gui_download_window *gui_download_window_create(download_context *ctx,
		struct gui_window *gui)
{
	const char *url = nsurl_access(download_context_get_url(ctx));
	const char *suggested = download_context_get_filename(ctx);
	struct gui_download_window *dw;
	struct download *dl;
	BPTR drawer_lock;
	BPTR previous_dir;

	(void)gui; /* Currently unused */

	dw = AllocMem(sizeof(*dw), MEMF_ANY | MEMF_CLEAR);

	if (dw == NULL)
		return NULL;

	dl = (APTR)methodstack_push_sync(application, 3,
			MM_Application_Download,
			(IPTR)url,
			(IPTR)suggested);

	if (dl == NULL)
	{
		FreeMem(dw, sizeof(*dw));
		return NULL;
	}

	dw->dl = dl;
	dw->ctx = ctx;
	dl->size = download_context_get_total_length(ctx);
	dl->done = 0;

	drawer_lock = Lock(dl->path, ACCESS_READ);
	if (drawer_lock == 0)
	{
		methodstack_push_sync(application, 2, MM_Application_DownloadError, dl);
		FreeMem(dw, sizeof(*dw));
		return NULL;
	}

	previous_dir = CurrentDir(drawer_lock);
	dw->fh = OpenAsync(dl->filename, MODE_WRITE, 8192);

	if (dw->fh)
	{
		ADDTAIL(&download_list, dw);
		if (url)
			SetComment(dl->filename, url);
	}
	else
	{
		methodstack_push_sync(application, 2, MM_Application_DownloadError, dl);
		CurrentDir(previous_dir);
		UnLock(drawer_lock);
		FreeMem(dw, sizeof(*dw));
		return NULL;
	}

	CurrentDir(previous_dir);
	UnLock(drawer_lock);

	return dw;
}

/**
 * process miscellaneous window events
 *
 * \param gw The window receiving the event.
 * \param event The event code.
 * \return NSERROR_OK when processed ok
 */
static nserror
gui_window_event1(struct gui_window *gw, enum gui_window_event event)
{
	switch (event) {
	case GW_EVENT_UPDATE_EXTENT:
		gui_window_update_extent(gw);
		break;

	case GW_EVENT_REMOVE_CARET:
		gui_window_remove_caret(gw);
		break;

	case GW_EVENT_NEW_CONTENT:
		gui_window_new_content(gw);
		break;

	case GW_EVENT_START_THROBBER:
		gui_window_start_throbber(gw);
		break;

	case GW_EVENT_STOP_THROBBER:
		gui_window_stop_throbber(gw);
		break;

	default:
		break;
	}
	return NSERROR_OK;
}
static nserror
gui_window_event(struct gui_window *gw, enum gui_window_event event)
{
    LOG(("DEBUG: gui_window_event called with event=%d", event));
    
    switch (event) {
    case GW_EVENT_UPDATE_EXTENT:
        LOG(("DEBUG: GW_EVENT_UPDATE_EXTENT"));
        gui_window_update_extent(gw);
        break;

    case GW_EVENT_REMOVE_CARET:
        LOG(("DEBUG: GW_EVENT_REMOVE_CARET"));
        //gui_window_remove_caret(gw);
        break;

    case GW_EVENT_NEW_CONTENT:
        LOG(("DEBUG: GW_EVENT_NEW_CONTENT - calling gui_window_new_content"));
        gui_window_new_content(gw);
		
		if (gw && gw->obj) {
            mui_trigger_content_ready_redraw(gw->obj);
        }
        break;

    case GW_EVENT_START_THROBBER:
        LOG(("DEBUG: GW_EVENT_START_THROBBER"));
        gui_window_start_throbber(gw);
        break;

    case GW_EVENT_STOP_THROBBER:
        LOG(("DEBUG: GW_EVENT_STOP_THROBBER"));
        gui_window_stop_throbber(gw);
        break;

    case GW_EVENT_PAGE_INFO_CHANGE:
        LOG(("DEBUG: GW_EVENT_PAGE_INFO_CHANGE"));
        // Handle page info changes if needed
        break;

    case GW_EVENT_SCROLL_START:
        LOG(("DEBUG: GW_EVENT_SCROLL_START"));
        // Handle scroll start if needed  
        break;

    case GW_EVENT_START_SELECTION:
        LOG(("DEBUG: GW_EVENT_START_SELECTION"));
        // Handle selection start if needed
        break;

    default:
        LOG(("DEBUG: Unknown window event: %d", event));
        break;
    }
    return NSERROR_OK;
}
static struct gui_window *
gui_window_create1(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags)
{
	LOG(("DEBUG: gui_window_create called - bw=%p, existing=%p, flags=%d", bw, existing, flags));
	
	// Wywołaj istniejącą funkcję gui_create_browser_window
	// ale z właściwymi parametrami
	struct gui_window *gw = gui_create_browser_window(bw, 
		(existing && (flags & GW_CREATE_CLONE)) ? existing->bw : NULL, 
		(flags & GW_CREATE_TAB) ? true : false);
	
	LOG(("DEBUG: gui_window_create returning gw=%p", gw));
	return gw;
}
static struct gui_window *
gui_window_create(struct browser_window *bw,
        struct gui_window *existing,
        gui_window_create_flags flags)
{
    LOG(("DEBUG: gui_window_create ENTRY"));
    LOG(("DEBUG: bw=%p, existing=%p, flags=%d", bw, existing, flags));
    
    if (!bw) {
        LOG(("ERROR: gui_window_create: bw is NULL"));
        return NULL;
    }
    
    LOG(("DEBUG: About to call gui_create_browser_window"));
    
    // Wywołaj istniejącą funkcję gui_create_browser_window
    // ale z właściwymi parametrami
    struct gui_window *gw = gui_create_browser_window(bw, 
        (existing && (flags & GW_CREATE_CLONE)) ? existing->bw : NULL, 
        (flags & GW_CREATE_TAB) ? true : false);
    
    LOG(("DEBUG: gui_create_browser_window returned gw=%p", gw));
    
    if (!gw) {
        LOG(("ERROR: gui_create_browser_window failed"));
        return NULL;
    }
    
    LOG(("DEBUG: gui_window_create SUCCESS - returning gw=%p", gw));
    return gw;
}
/**
 * Ensures output logging stream is correctly configured
 */
static bool nslog_stream_configure(FILE *fptr)
{
		/* set log stream to be non-buffering */
	setbuf(fptr, NULL);

	return true;
}

/**
 * Invalidate an area of a window
 *
 * \param gw The window to invalidate
 * \param rect The rectangle to invalidate (in window coordinates)
 * \return NSERROR_OK on success
 */
static nserror gui_window_invalidate(struct gui_window *gw, const struct rect *rect)
{
    LOG(("DEBUG: gui_window_invalidate called"));
    LOG(("DEBUG: gw=%p, rect=%p", gw, rect));
   
    if (!gw) {
        LOG(("ERROR: gui_window_invalidate: gw is NULL"));
        return NSERROR_BAD_PARAMETER;
    }
   
    if (rect != NULL) {
        LOG(("DEBUG: gui_window_invalidate: invalidating area x0=%d, y0=%d, x1=%d, y1=%d",
             rect->x0, rect->y0, rect->x1, rect->y1));
        
        // Invalidate konkretny obszar
        gui_window_redraw(gw, rect->x0, rect->y0, rect->x1, rect->y1);
		
    } else {
        LOG(("DEBUG: gui_window_invalidate: invalidating ENTIRE window"));
        
        // ✅ Invalidate całe okno - wywołaj pełny redraw!
        methodstack_push_sync(gw->obj, 1, MM_Browser_Redraw);
    }
   
    LOG(("DEBUG: gui_window_invalidate: completed successfully"));
    return NSERROR_OK;
}

static struct gui_window_table mui_window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.invalidate = gui_window_invalidate,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll_rect,
	.get_dimensions = gui_window_get_dimensions,
	.event = gui_window_event,

	.set_title = gui_window_set_title,
	.set_url = gui_window_set_url,
	.set_status = gui_window_set_status,
	.set_pointer = gui_window_set_pointer,
	.place_caret = gui_window_place_caret,
};

/**
 * Poll components which require it.
 */

void netsurf_poll(void)
{
	static unsigned int last_clean = 0;
	unsigned int current_time = wallclock();

	/* avoid calling content_clean() more often than once every 5
	 * seconds.
	 */
	if (last_clean + 500 < current_time) {
		last_clean = current_time;
	}
	service_frontend(false);

}
/**
 * Gui NetSurf main loop.
 */

int netsurf_main_loop(void)
{
    LOG(("DEBUG: netsurf_main_loop ENTRY"));
    
    int iteration = 0;
    while (!netsurf_quit) {
        iteration++;
        
        if (iteration % 100 == 0) {
            LOG(("DEBUG: Main loop iteration %d", iteration));
        }
        
        // WAŻNE: Sprawdź scheduler PRZED netsurf_poll
        ULONG signals = SetSignal(0, 0);
        if (signals & schedule_sig) {
            LOG(("DEBUG: Schedule signal in main loop"));
            mui_schedule_poll();
        }
        
        netsurf_poll();
        
        // Mała pauza żeby nie obciążać CPU
        //Delay(3);  // 1/50 sekundy
    }
    
    LOG(("DEBUG: netsurf_main_loop ended"));
    return 0;
}


void main(int argc, char** argv)
{
	struct browser_window *bw;
	char *options;
	nsurl *url;
	nserror ret;

	// Initialize the GUI system
	struct netsurf_table mui_table = {
		.misc = &mui_misc_table,
		.window = &mui_window_table,
		.clipboard = mui_clipboard_table,
		.bitmap = mui_bitmap_table,
		.layout = mui_layout_table,
		.fetch = amiga_fetch_table,
		.file = amiga_file_table,
		.utf8 = amiga_utf8_table,
		.download = &mui_download_table,
		.llcache = filesystem_llcache_table,
		};
	ret = netsurf_register(&mui_table);

	if (ret != NSERROR_OK) {
		LOG(("NetSurf operation table failed registration"));
		die("NetSurf operation table failed registration");
	}
	/* initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	SetTaskPri(FindTask("LimpidClock"), 0);


	process_priority = SysBase->ThisTask->tc_Node.ln_Pri;

	//LOG(("DEBUG: FUNCTION=%s LINE=%d \n", __FUNCTION__, __LINE__));
	atexit(cleanup);
	//LOG(("DEBUG: FUNCTION=%s LINE=%d \n", __FUNCTION__, __LINE__));
	if (startup())
	{
		struct Locale *locale;
		TEXT lang[100];
		BPTR lock, file;
		ULONG i, found;

	/* user options setup */
	ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		die("Options failed to initialise");
	}
	nsoption_commandline(&argc, argv, nsoptions);
	options = strdup("PROGDIR:Resources/Options");
	nsoption_read(options, nsoptions);
	LOG(("DEBUG: Options read from file: %s", options));
	verbose_log = false;
			
	font_init();

	nslog_init(NULL, &argc, argv);
	LOG(("DEBUG: mui_table registered successfully"));

		/* message init */
		unsigned char messages[200];
		if (ami_locate_resource(messages, "Messages") == false) {
			ami_misc_fatal_error("Cannot open Messages file");
			return RETURN_FAIL;
		}

		ret = messages_add_from_file(messages);
		if (ret != NSERROR_OK)
			fprintf(stderr, "Cannot open Messages file");

		ret = netsurf_init("PROGDIR:Resources/Cache");
		//nsoption_charp(cache_dir));
		/* Override, since we have no support for non-core SELECT menu */
		nsoption_set_bool(core_select_menu, true);
	
		netsurf_setup();	

		LOG(("DEBUG: NSERROR_OK returned from netsurf_init"));

		urldb_load_cookies(nsoption_charp(cookie_file));

		/* create an initial browser window */

		NSLOG(netsurf, INFO,"calling browser_window_create");	
	} 

    	LOG(("DEBUG: About to call gui_init2"));

   	 gui_init2(argc, argv);
	 	
	LOG(("DEBUG: gui_init2 completed"));
	LOG(("DEBUG: Starting MUI main loop using netsurf_check_events"));

while (!netsurf_quit) {
	netsurf_poll();
	if (netsurf_quit) {
		break;
	}
	service_frontend(true);
}
	
	cleanup();
	/* finalise options */
	nsoption_finalise(nsoptions, nsoptions_default);

	/* finalise logging */
	nslog_finalise();
	LOG(("DEBUG: Main loop ended"));
	return 0;
}

static nserror gui_download_window_data(struct gui_download_window *dw, const char *data, unsigned int size)
{
	if (dw == NULL || dw->fh == NULL)
		return NSERROR_SAVE_FAILED;

	WriteAsync(dw->fh, (APTR)data, size);
	dw->dl->done += size;
	methodstack_push(application, 1, MM_Application_DownloadUpdate);

	return NSERROR_OK;
}

static void gui_download_window_error(struct gui_download_window *dw, const char *error_msg)
{
	(void)error_msg;
	if (dw == NULL)
		return;

	methodstack_push_sync(application, 2, MM_Application_DownloadError, dw->dl);
	mui_download_window_cleanup(dw);
}

static void gui_download_window_done(struct gui_download_window *dw)
{
	if (dw == NULL)
		return;

	methodstack_push_sync(application, 2, MM_Application_DownloadDone, dw->dl);
	mui_download_window_cleanup(dw);
}

static struct gui_download_table mui_download_table = {
	.create = gui_download_window_create,
	.data = gui_download_window_data,
	.error = gui_download_window_error,
	.done = gui_download_window_done,
};

//void gui_drag_save_object(gui_save_type type, struct hlcache_handle *c, struct gui_window *g)
//{
//}

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

void gui_cert_verify(struct browser_window *bw, struct hlcache_handle *c, const struct ssl_cert_info *certs, unsigned long num)
{
}