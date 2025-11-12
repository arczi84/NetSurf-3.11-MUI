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

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "utils/errors.h"
#include "netsurf/browser.h"
#include "netsurf/browser_window.h"
#include "utils/nsoption.h"
#include "mui/applicationclass.h"
#include "mui/gui.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
#include "libraries/mui.h"
#include "utils/log.h"

#define APPLICATION_NAME "NetSurf"
#define WINDOW_TITLE_FORMAT "NetSurf: %s"

struct Data
{
    APTR windownode;
    APTR active_browser;
    APTR navbar;
    APTR addressbar;
    APTR bgroup;
    APTR findbar;
    APTR statusbar;
    APTR pagetitles;
    STRPTR windowtitle;
};
#if 0
DEFNEW
{
    APTR navbar = NULL, addressbar = NULL, searchbar = NULL, findbar = NULL;
    APTR statusbar = NULL, bgroup = NULL;

    LOG(("Entering NEW method"));
    // Verify MUI classes
#if defined(__MORPHOS2__)
    if (!getnavigationbargroupclass() || !getsearchbargroupclass() || !getaddressbargroupclass() ||
        !getfindtextclass() || !gettitleclass()) {
#else
    if (!getnavigationbargroupclass() || !getsearchbargroupclass() || !getaddressbargroupclass() ||
        !getfindtextclass()) {
#endif
        LOG(("One or more MUI classes unavailable"));
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Missing required MUI classes!");
        return 0;
    }

    LOG(("Creating navigation bar"));
    navbar = NewObject(getnavigationbargroupclass(), NULL, TAG_DONE);
    if (!navbar) {
        LOG(("Failed to create navbar"));
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create navigation bar!");
        return 0;
    }

    LOG(("Creating search bar"));
    searchbar = NewObject(getsearchbargroupclass(), NULL, TAG_DONE);
    if (!searchbar) {
        LOG(("Failed to create searchbar"));
        MUI_DisposeObject(navbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create search bar!");
        return 0;
    }

    LOG(("Creating address bar"));
    addressbar = NewObject(getaddressbargroupclass(), NULL, TAG_DONE);
    if (!addressbar) {
        LOG(("Failed to create addressbar"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create address bar!");
        return 0;
    }

    LOG(("Creating browser group"));
    bgroup = VGroup, MUIA_Group_PageMode, TRUE, End;
    if (!bgroup) {
        LOG(("Failed to create bgroup"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create browser group!");
        return 0;
    }

    LOG(("Creating find bar"));
    findbar = NewObject(getfindtextclass(), NULL, MUIA_ShowMe, FALSE, TAG_DONE);
    if (!findbar) {
        LOG(("Failed to create findbar"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_DisposeObject(bgroup);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create find bar!");
        return 0;
    }

    LOG(("Creating status bar"));
    statusbar = TextObject, MUIA_Text_Contents, "", TAG_DONE);
    if (!statusbar) {
        LOG(("Failed to create statusbar"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_DisposeObject(bgroup);
        MUI_DisposeObject(findbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create status bar!");
        return 0;
    }

    LOG(("Creating window object"));
    obj = DoSuperNew(cl, obj,
        MUIA_Window_Width, MUIV_Window_Width_Visible(75),
        MUIA_Window_Height, MUIV_Window_Height_Visible(90),
        MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
        MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
        MUIA_Window_AppWindow, TRUE,
        MUIA_Window_RootObject, VGroup,
            Child, HGroup,
                Child, navbar,
                Child, searchbar,
            End,
            Child, addressbar,
            Child, bgroup,
            Child, findbar,
            Child, statusbar,
        End,
        TAG_MORE, msg->ops_AttrList);

    if (!obj) {
        LOG(("DoSuperNew failed"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_DisposeObject(bgroup);
        MUI_DisposeObject(findbar);
        MUI_DisposeObject(statusbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create window!");
        return 0;
    }

    GETDATA;
    data->navbar = navbar;
    data->addressbar = addressbar;
    data->bgroup = bgroup;
    data->findbar = findbar;
    data->statusbar = statusbar;
#if 0
    LOG(("Creating page titles"));
    data->pagetitles = NewObject(gettitleclass(), NULL, MUII_Close, 1, MUIA_Title_Closable, TRUE, TAG_DONE);
    //data->pagetitles = NewObject(gettitleclass(), NULL, TAG_DONE);
    if (!data->pagetitles) {
        LOG(("Failed to create pagetitles"));
        MUI_DisposeObject(obj);
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_DisposeObject(bgroup);
        MUI_DisposeObject(findbar);
        MUI_DisposeObject(statusbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create page titles!");
        return 0;
    }
    LOG(("pagetitles created"));
#endif    
    data->windownode = (APTR)GetTagData(MA_Window_Node, NULL, msg->ops_AttrList);
    LOG(("windownode=%p", data->windownode));

    LOG(("Setting up window notifications"));
    DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             MUIV_Notify_Application, 2, MM_Application_CloseWindow, data->windownode);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, MUIV_EveryTime,
             obj, 3, MM_Window_MenuAction, MUIV_TriggerValue);

    LOG(("Window created successfully, obj=%p", obj));
    return (IPTR)obj;
}
#endif
DEFNEW
{
    APTR navbar = NULL, addressbar = NULL, searchbar = NULL, findbar = NULL;
    APTR statusbar = NULL, bgroup = NULL;

    LOG(("Entering NEW method"));
    // Verify MUI classes
#if defined(__MORPHOS2__)
    if (!getnavigationbargroupclass() || !getsearchbargroupclass() || !getaddressbargroupclass() ||
        !getfindtextclass() || !gettitleclass()) {
#else
    if (!getnavigationbargroupclass() || !getsearchbargroupclass() || !getaddressbargroupclass() ||
        !getfindtextclass()) {
#endif
        LOG(("One or more MUI classes unavailable"));
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Missing required MUI classes!");
        return 0;
    }

    LOG(("Creating navigation bar"));
    navbar = NewObject(getnavigationbargroupclass(), NULL, TAG_DONE);
    if (!navbar) {
        LOG(("Failed to create navbar"));
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create navigation bar!");
        return 0;
    }
    LOG(("DEBUG: navbar created successfully: %p", navbar));

    LOG(("Creating search bar"));
    searchbar = NewObject(getsearchbargroupclass(), NULL, TAG_DONE);
    if (!searchbar) {
        LOG(("Failed to create searchbar"));
        MUI_DisposeObject(navbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create search bar!");
        return 0;
    }
    LOG(("DEBUG: searchbar created successfully: %p", searchbar));

    LOG(("Creating address bar"));
    addressbar = NewObject(getaddressbargroupclass(), NULL, TAG_DONE);
    if (!addressbar) {
        LOG(("Failed to create addressbar"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create address bar!");
        return 0;
    }
    LOG(("DEBUG: addressbar created successfully: %p", addressbar));

    LOG(("Creating browser group"));
    bgroup = VGroup, MUIA_Group_PageMode, TRUE, End;
    if (!bgroup) {
        LOG(("Failed to create bgroup"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create browser group!");
        return 0;
    }
    LOG(("DEBUG: bgroup created successfully: %p", bgroup));

    LOG(("Creating find bar"));
    findbar = NewObject(getfindtextclass(), NULL, MUIA_ShowMe, FALSE, TAG_DONE);
    if (!findbar) {
        LOG(("Failed to create findbar"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_DisposeObject(bgroup);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create find bar!");
        return 0;
    }
    LOG(("DEBUG: findbar created successfully: %p", findbar));

    LOG(("Creating status bar"));
    statusbar = TextObject, MUIA_Text_Contents, "", TAG_DONE);
    if (!statusbar) {
        LOG(("Failed to create statusbar"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_DisposeObject(bgroup);
        MUI_DisposeObject(findbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create status bar!");
        return 0;
    }
    LOG(("DEBUG: statusbar created successfully: %p", statusbar));
    // DEBUG: Make UI elements visible with distinct backgrounds
    LOG(("DEBUG: Setting background colors for visibility"));
    set(navbar, MUIA_Background, MUII_ButtonBack);      // Button-like background
    set(addressbar, MUIA_Background, MUII_StringBack);  // String field background  
    set(statusbar, MUIA_Background, MUII_TextBack);     // Text background
    
    // Also try setting explicit frame types
    set(navbar, MUIA_Frame, MUIV_Frame_Button);
    set(addressbar, MUIA_Frame, MUIV_Frame_String);
    set(statusbar, MUIA_Frame, MUIV_Frame_Text);

    LOG(("Creating window object with layout"));
    LOG(("Creating window object with layout"));
    LOG(("DEBUG: Layout structure:"));
    LOG(("DEBUG: - HGroup (navbar + searchbar)"));
    LOG(("DEBUG: - addressbar"));
    LOG(("DEBUG: - bgroup (browser content)"));
    LOG(("DEBUG: - findbar"));
    LOG(("DEBUG: - statusbar"));
    
    obj = DoSuperNew(cl, obj,
        MUIA_Window_Width, MUIV_Window_Width_Visible(75),
        MUIA_Window_Height, MUIV_Window_Height_Visible(90),
        MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
        MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
        MUIA_Window_AppWindow, TRUE,
        MUIA_Window_RootObject, VGroup,
            Child, HGroup,
                MUIA_Group_Columns, 2,  // Add explicit column count for debugging
                Child, navbar,
                Child, searchbar,
            End,
            Child, addressbar,
            Child, bgroup,
            Child, findbar,
            Child, statusbar,
        End,
        TAG_MORE, msg->ops_AttrList);

    if (!obj) {
        LOG(("DoSuperNew failed"));
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_DisposeObject(bgroup);
        MUI_DisposeObject(findbar);
        MUI_DisposeObject(statusbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create window!");
        return 0;
    }

    LOG(("DEBUG: Window object created successfully: %p", obj));

    GETDATA;
    data->navbar = navbar;
    data->addressbar = addressbar;
    data->bgroup = bgroup;
    data->findbar = findbar;
    data->statusbar = statusbar;
    
    // DEBUG: Check visibility of UI elements
    LOG(("DEBUG: UI element visibility check:"));
    LOG(("DEBUG: navbar ShowMe: %ld", getv(navbar, MUIA_ShowMe)));
    LOG(("DEBUG: addressbar ShowMe: %ld", getv(addressbar, MUIA_ShowMe)));
    LOG(("DEBUG: bgroup ShowMe: %ld", getv(bgroup, MUIA_ShowMe)));
    LOG(("DEBUG: findbar ShowMe: %ld", getv(findbar, MUIA_ShowMe)));
    LOG(("DEBUG: statusbar ShowMe: %ld", getv(statusbar, MUIA_ShowMe)));

#if 0
    LOG(("Creating page titles"));
    data->pagetitles = NewObject(gettitleclass(), NULL, MUII_Close, 1, MUIA_Title_Closable, TRUE, TAG_DONE);
    //data->pagetitles = NewObject(gettitleclass(), NULL, TAG_DONE);
    if (!data->pagetitles) {
        LOG(("Failed to create pagetitles"));
        MUI_DisposeObject(obj);
        MUI_DisposeObject(navbar);
        MUI_DisposeObject(searchbar);
        MUI_DisposeObject(addressbar);
        MUI_DisposeObject(bgroup);
        MUI_DisposeObject(findbar);
        MUI_DisposeObject(statusbar);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create page titles!");
        return 0;
    }
    LOG(("pagetitles created"));
#endif    
    data->windownode = (APTR)GetTagData(MA_Window_Node, NULL, msg->ops_AttrList);
    LOG(("windownode=%p", data->windownode));

    LOG(("Setting up window notifications"));
    DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             MUIV_Notify_Application, 2, MM_Application_CloseWindow, data->windownode);
    DoMethod(obj, MUIM_Notify, MUIA_Window_MenuAction, MUIV_EveryTime,
             obj, 3, MM_Window_MenuAction, MUIV_TriggerValue);

    LOG(("Window created successfully, obj=%p", obj));
    LOG(("DEBUG: Final UI element pointers:"));
    LOG(("DEBUG: data->navbar=%p", data->navbar));
    LOG(("DEBUG: data->addressbar=%p", data->addressbar));
    LOG(("DEBUG: data->bgroup=%p", data->bgroup));
    LOG(("DEBUG: data->findbar=%p", data->findbar));
    LOG(("DEBUG: data->statusbar=%p", data->statusbar));
    
    return (IPTR)obj;
}
DEFDISP
{
    GETDATA;
    LOG(("Disposing window"));
    if (data->windowtitle) {
        LOG(("Freeing windowtitle=%p", data->windowtitle));
        FreeVec(data->windowtitle);
        data->windowtitle = NULL;
    }
    if (data->pagetitles) {
        LOG(("Disposing pagetitles=%p", data->pagetitles));
        MUI_DisposeObject(data->pagetitles);
        data->pagetitles = NULL;
    }
    LOG(("Calling DOSUPER for dispose"));
    return DOSUPER;
}

DEFGET
{
    GETDATA;
    switch (msg->opg_AttrID) {
        case MA_Browser_Browser:
        case MA_Browser_URL:
            if (!data->active_browser) {
                LOG(("No active browser for GET, attr=%ld", msg->opg_AttrID));
                return 0;
            }
            return DoMethodA(data->active_browser, (Msg)msg);
    }
    return DOSUPER;
}

DEFTMETHOD(Window_InsertBookmark)
{
    GETDATA;
    LOG(("Inserting bookmark"));
    STRPTR url = (STRPTR)getv(data->active_browser, MA_Browser_URL);
    STRPTR title = (STRPTR)getv(data->active_browser, MA_Browser_Title);
    if (!url || !title) {
        LOG(("Invalid url=%p or title=%p for bookmark", url, title));
        return 0;
    }
    return DoMethod(_app(obj), MM_HotlistWindow_Insert, title, url);
}

DEFSMETHOD(Window_MenuAction)
{
    GETDATA;
    APTR app = _app(obj) ? _app(obj) : application;
    LOG(("Handling menu action: %ld", msg->action));

    switch (msg->action) {
        case MNA_RELOAD:
            DoMethod(obj, MM_Window_Navigate, NAV_RELOAD);
            break;
        case MNA_SAVE_AS_TEXT:
            DoMethod(app, MM_Application_SaveDocument, SAVEDOC_HTML);
            break;
        case MNA_SAVE_AS_SOURCE:
            DoMethod(app, MM_Application_SaveDocument, SAVEDOC_SOURCE);
            break;
        case MNA_SAVE_AS_PDF:
            DoMethod(app, MM_Application_SaveDocument, SAVEDOC_PDF);
            break;
        case MNA_PRINT:
            if (data->active_browser) {
                DoMethod(data->active_browser, MM_Browser_Print);
            } else {
                LOG(("No active browser for print"));
            }
            break;
        case MNA_ABOUT:
            DoMethod(app, MM_Application_About);
            break;
        case MNA_FIND:
            set(data->findbar, MUIA_ShowMe, TRUE);
            break;
        case MNA_DOWNLOADS_WINDOW:
            DoMethod(app, MM_Application_OpenWindow, WINDOW_DOWNLOADS);
            break;
        case MNA_HOTLIST_WINDOW:
            DoMethod(app, MM_Application_OpenWindow, WINDOW_HOTLIST);
            break;
        case MNA_MUI_SETTINGS:
            DoMethod(app, MUIM_Application_OpenConfigWindow, 0, NULL);
            break;
        case MNA_NETSURF_SETTINGS:
            DoMethod(app, MM_Application_OpenWindow, WINDOW_SETTINGS);
            break;
        case MNA_NEW_PAGE:
            LOG(("Creating new page with homepage_url"));
            
            LOG(("DEBUG: Step 1 - Creating nsurl"));
            struct nsurl *url;
            nserror ret = nsurl_create(nsoption_charp(homepage_url), &url);
            
            LOG(("DEBUG: nsurl_create returned %d, url=%p", ret, url));
            
            if (ret == NSERROR_OK) {
                LOG(("DEBUG: Step 2 - About to call browser_window_create"));
                LOG(("DEBUG: Parameters: flags=BW_CREATE_HISTORY, url=%p, referrer=NULL, existing=NULL", url));
                
                struct browser_window *bw = NULL;
                LOG(("DEBUG: Calling browser_window_create..."));
                
                ret = browser_window_create(BW_CREATE_HISTORY, url, NULL, NULL, &bw);
                
                LOG(("DEBUG: browser_window_create returned %d, bw=%p", ret, bw));
                
                if (ret != NSERROR_OK) {
                    LOG(("ERROR: browser_window_create failed with error %d", ret));
                } else {
                    LOG(("SUCCESS: browser_window_create succeeded, bw=%p", bw));
                }
                
                LOG(("DEBUG: Step 3 - Unreferencing URL"));
                nsurl_unref(url);
                LOG(("DEBUG: nsurl_unref completed"));
                
            } else {
                LOG(("ERROR: nsurl_create failed with error %d", ret));
            }
            
            LOG(("DEBUG: MNA_NEW_PAGE case completed"));
            break;
        case MNA_CLOSE_PAGE:
            LOG(("Closing page"));
            if (data->pagetitles) {
                DoMethod(data->pagetitles, MUIM_Title_Close, NULL);
            } else {
                LOG(("No pagetitles for close page"));
                DoMethod(app, MUIM_Application_PushMethod, app, 2,
                         MM_Application_CloseWindow, data->windownode);
            }
            break;
        case MNA_CLOSE_WINDOW:
            LOG(("Closing window"));
            if (data->pagetitles && getv(data->pagetitles, MUIA_Group_ChildCount) > 1) {
                if (!MUI_RequestA(_app(obj), obj, 0, "NetSurf", "_Yes|*_No",
                                  "Do you want to close all pages?", NULL)) {
                    break;
                }
            }
            DoMethod(app, MUIM_Application_PushMethod, app, 2,
                     MM_Application_CloseWindow, data->windownode);
            break;
    }
    return 0;
}

DEFSMETHOD(Window_RemovePage)
{
    GETDATA;
    LOG(("Removing page, browser=%p", msg->browser));
    if (!data->pagetitles || !msg->browser) {
        LOG(("Invalid pagetitles=%p or browser=%p", data->pagetitles, msg->browser));
        return 0;
    }

    APTR title = (APTR)getv(msg->browser, MA_Browser_TitleObj);
    if (!title) {
        LOG(("No title object for browser=%p", msg->browser));
        return 0;
    }

    DoMethod(data->bgroup, MUIM_Group_InitChange);
    DoMethod(data->bgroup, OM_REMMEMBER, msg->browser);
    DoMethod(data->pagetitles, MUIM_Group_InitChange);
    DoMethod(data->pagetitles, OM_REMMEMBER, title);
    DoMethod(data->pagetitles, MUIM_Group_ExitChange);
    DoMethod(data->bgroup, MUIM_Group_ExitChange);

    MUI_DisposeObject(title);
    MUI_DisposeObject(msg->browser);

    if (getv(data->pagetitles, MUIA_Group_ChildCount) == 0) {
        LOG(("No pages left, closing window"));
        DoMethod(application, MUIM_Application_PushMethod, application, 2,
                 MM_Application_CloseWindow, data->windownode);
    }

    return 0;
}

STATIC VOID setup_browser_notifications(APTR obj, struct Data *data, APTR bw)
{
    if (!bw || !data->navbar || !data->statusbar || !data->addressbar) {
        LOG(("Invalid browser=%p or UI objects: navbar=%p, statusbar=%p, addressbar=%p",
                  bw, data->navbar, data->statusbar, data->addressbar));
        return;
    }

    LOG(("Setting up notifications for browser=%p", bw));
    DoMethod(bw, MUIM_Notify, MA_Browser_Loading, MUIV_EveryTime,
             data->navbar, 3, MUIM_Set, MA_TransferAnim_Animate, MUIV_TriggerValue);
    LOG(("Set MA_Browser_Loading notification"));
    DoMethod(bw, MUIM_Notify, MA_Browser_StatusText, MUIV_EveryTime,
             data->statusbar, 3, MUIM_Set, MUIA_Text_Contents, MUIV_TriggerValue);
    LOG(("Set MA_Browser_StatusText notification"));
    DoMethod(bw, MUIM_Notify, MA_Browser_URL, MUIV_EveryTime,
             data->addressbar, 3, MUIM_Set, MUIA_String_Contents, MUIV_TriggerValue);
    LOG(("Set MA_Browser_URL notification"));
    DoMethod(bw, MUIM_Notify, MA_Browser_Title, MUIV_EveryTime,
             obj, 2, MM_Window_SetTitle, MUIV_TriggerValue);
    LOG(("Set MA_Browser_Title notification"));
    DoMethod(bw, MUIM_Notify, MA_Browser_BackAvailable, MUIV_EveryTime,
             data->navbar, 3, MUIM_Set, MA_Navigation_BackEnabled, MUIV_TriggerValue);
    LOG(("Set MA_Browser_BackAvailable notification"));
    DoMethod(bw, MUIM_Notify, MA_Browser_ForwardAvailable, MUIV_EveryTime,
             data->navbar, 3, MUIM_Set, MA_Navigation_ForwardEnabled, MUIV_TriggerValue);
    LOG(("Set MA_Browser_ForwardAvailable notification"));
    DoMethod(bw, MUIM_Notify, MA_Browser_ReloadAvailable, MUIV_EveryTime,
             data->navbar, 3, MUIM_Set, MA_Navigation_ReloadEnabled, MUIV_TriggerValue);
    LOG(("Set MA_Browser_ReloadAvailable notification"));
    DoMethod(bw, MUIM_Notify, MA_Browser_StopAvailable, MUIV_EveryTime,
             data->navbar, 3, MUIM_Set, MA_Navigation_StopEnabled, MUIV_TriggerValue);
    LOG(("Set MA_Browser_StopAvailable notification"));
}

DEFSMETHOD(Window_AddPage)
{
    GETDATA;
    APTR bwgroup = NULL, bw = NULL, title = NULL;

    LOG(("Creating title object"));
    title = TextObject,
        MUIA_Text_Contents, APPLICATION_NAME,
        MUIA_UserData, msg->context,
    End;
    if (!title) {
        LOG(("Failed to create title object"));
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create page title!");
        return 0;
    }

    LOG(("Creating browser object, context=%p", msg->context));
    if (!getbrowserclass()) {
        LOG(("Browser class unavailable"));
        MUI_DisposeObject(title);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Browser class unavailable!");
        return 0;
    }

    bwgroup = ScrollgroupObject,
        MUIA_Scrollgroup_AutoBars, TRUE,
        MUIA_FillArea, FALSE,
        MUIA_Scrollgroup_Contents, bw = NewObject(getbrowserclass(), NULL,
            MA_Browser_Context, msg->context,
            MA_Browser_TitleObj, title,
        TAG_DONE),
    TAG_DONE);
    if (!bwgroup || !bw) {
        LOG(("Failed to create browser group=%p or browser=%p", bwgroup, bw));
        MUI_DisposeObject(title);
        MUI_DisposeObject(bwgroup);
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to create browser page!");
        return 0;
    }

    LOG(("bwgroup=%p, bw=%p, title=%p", bwgroup, bw, title));
    DoMethod(data->bgroup, MUIM_Group_InitChange);
    DoMethod(data->pagetitles, MUIM_Group_InitChange);
    DoMethod(data->pagetitles, OM_ADDMEMBER, title);
    DoMethod(data->bgroup, OM_ADDMEMBER, bwgroup);
    DoMethod(data->pagetitles, MUIM_Group_ExitChange);
    DoMethod(data->bgroup, MUIM_Group_ExitChange);

    data->active_browser = bw;
    set(data->bgroup, MUIA_Group_ActivePage, MUIV_Group_ActivePage_Last);

    LOG(("Setting title notification for bw=%p", bw));
    DoMethod(bw, MUIM_Notify, MA_Browser_Title, MUIV_EveryTime,
             title, 3, MUIM_Set, MUIA_Text_Contents, MUIV_TriggerValue);
    setup_browser_notifications(obj, data, bw);

    LOG(("Page created successfully, browser=%p", bw));
    return (IPTR)bw;
}

DEFSMETHOD(Window_ActivePage)
{
    GETDATA;
    ULONG idx = 0;

    LOG(("Switching to page %ld", msg->pagenum));
    FORCHILD(data->bgroup, MUIA_Group_ChildList) {
        if (data->pagetitles == child) continue;

        if (msg->pagenum == idx) {
            APTR old = data->active_browser;
            if (old) {
                LOG(("Removing notifications for old browser=%p", old));
                DoMethod(old, MUIM_KillNotify, MA_Browser_Loading);
                DoMethod(old, MUIM_KillNotify, MA_Browser_StatusText);
                DoMethod(old, MUIM_KillNotify, MA_Browser_URL);
                DoMethod(old, MUIM_KillNotifyObj, MA_Browser_Title, obj);
                DoMethod(old, MUIM_KillNotify, MA_Browser_BackAvailable);
                DoMethod(old, MUIM_KillNotify, MA_Browser_ForwardAvailable);
                DoMethod(old, MUIM_KillNotify, MA_Browser_ReloadAvailable);
                DoMethod(old, MUIM_KillNotify, MA_Browser_StopAvailable);
            }

            setup_browser_notifications(obj, data, child);
            data->active_browser = child;

            DoMethod(data->addressbar, MUIM_NoNotifySet, MUIA_String_Contents,
                     getv(child, MA_Browser_URL));
            DoMethod(data->statusbar, MUIM_NoNotifySet, MUIA_Text_Contents,
                     getv(child, MA_Browser_StatusText));
            LOG(("Set status text to %s", (STRPTR)getv(child, MA_Browser_StatusText)));
            DoMethod(obj, MM_Window_SetTitle, getv(child, MA_Browser_Title));
            LOG(("Set title to %s", (STRPTR)getv(child, MA_Browser_Title)));
            SetAttrs(data->navbar,
                MA_TransferAnim_Animate, getv(child, MA_Browser_Loading),
                MA_Navigation_BackEnabled, getv(child, MA_Browser_BackAvailable),
                MA_Navigation_ForwardEnabled, getv(child, MA_Browser_ForwardAvailable),
                MA_Navigation_ReloadEnabled, getv(child, MA_Browser_ReloadAvailable),
                MA_Navigation_StopEnabled, getv(child, MA_Browser_StopAvailable),
                TAG_DONE);
            break;
        }
        idx++;
    }
    NEXTCHILD
    return 0;
}

DEFSMETHOD(Window_SetTitle)
{
    GETDATA;
    if (!msg->title) {
        LOG(("msg->title is NULL"));
        return 0;
    }

    LOG(("Setting window title: %s", msg->title));
    ULONG length = strlen(WINDOW_TITLE_FORMAT) + strlen(msg->title) + 1; // +1 for null terminator
    STRPTR newtitle = AllocVec(length, MEMF_CLEAR);
    if (!newtitle) {
        LOG(("AllocVec failed for window title, length=%ld", length));
        MUI_Request(NULL, NULL, 0, "Error", "OK", "Failed to set window title!");
        return 0;
    }

    snprintf(newtitle, length, WINDOW_TITLE_FORMAT, msg->title);
    set(obj, MUIA_Window_Title, newtitle);

    if (data->windowtitle) {
        LOG(("Freeing old windowtitle=%p", data->windowtitle));
        FreeVec(data->windowtitle);
    }
    data->windowtitle = newtitle;
    LOG(("Window title set to: %s", newtitle));
    return 0;
}

DEFSMETHOD(Window_Navigate)
{
    GETDATA;
    LOG(("Navigating: %ld", msg->Navigate));
    if (!data->active_browser) {
        LOG(("No active browser for navigation"));
        return 0;
    }
    switch (msg->Navigate) {
        case NAV_BACK:   return DoMethod(data->active_browser, MM_Browser_Back);
        case NAV_FORWARD: return DoMethod(data->active_browser, MM_Browser_Forward);
        case NAV_RELOAD: return DoMethod(data->active_browser, MM_Browser_Reload);
        case NAV_STOP:   return DoMethod(data->active_browser, MM_Browser_Stop);
    }
    return 0;
}

DEFSMETHOD(Browser_Go)
{
    GETDATA;
    LOG(("Browser go"));
    if (!data->active_browser) {
        LOG(("No active browser for Browser_Go"));
        return 0;
    }
    return DoMethodA(data->active_browser, (Msg)msg);
}

DEFSMETHOD(Browser_Find)
{
    GETDATA;
    LOG(("Browser find"));
    if (!data->active_browser) {
        LOG(("No active browser for Browser_Find"));
        return 0;
    }
    return DoMethodA(data->active_browser, (Msg)msg);
}

DEFSMETHOD(FindText_DisableButtons)
{
    GETDATA;
    LOG(("Disabling find text buttons"));
    if (!data->findbar) {
        LOG(("No findbar for FindText_DisableButtons"));
        return 0;
    }
    return DoMethodA(data->findbar, (Msg)msg);
}

BEGINMTABLE
DECNEW
DECDISP
DECGET
DECSMETHOD(Browser_Find)
DECSMETHOD(Browser_Go)
DECSMETHOD(FindText_DisableButtons)
DECSMETHOD(Window_ActivePage)
DECSMETHOD(Window_AddPage)
DECSMETHOD(Window_InsertBookmark)
DECSMETHOD(Window_Navigate)
DECSMETHOD(Window_MenuAction)
DECSMETHOD(Window_RemovePage)
DECSMETHOD(Window_SetTitle)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Window, windowclass)