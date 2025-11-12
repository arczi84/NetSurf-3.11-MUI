#include <proto/intuition.h>
#include <proto/utility.h>
#include <clib/keymap_protos.h> //MapRawKey
//#include <clib/intuition_protos.h> //SetAttrs
#include <libraries/mui.h>
#include "mui/gui.h"
#include "mui/mui.h"
#include "mui/netsurf.h"

/* No need for your own Data for the NEW method */
struct Data
{
    struct MUI_EventHandlerNode ehnode;
};

/* Constructor: call MUIC_Title’s DoSuperNew so the base title gets built */
DEFNEW
{
    /* Create the MUIC_Title with exactly the tags the caller passed */
    obj = (Object *)DoSuperNew(cl, obj,
        /* any default attributes you want here, e.g.: */
        /* MUIA_Title_SetMin, TRUE, */
        TAG_MORE, INITTAGS
    );
    return (ULONG)obj;
}

/* Your custom “close” behavior stays as-is */
DEFMMETHOD(Title_Close)
{
    DOSUPER;

    /* if no children left, close the window */
    if (getv(obj, MUIA_Group_ChildCount) == 0)
        set(_win(obj), MUIA_Window_CloseRequest, TRUE);

    return 0;
}

/* Hide/Show and HandleEvent likewise stay untouched... */
DEFMMETHOD(Hide)
{
    GETDATA;
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode);
    return DOSUPER;
}

DEFMMETHOD(Show)
{
    ULONG rc = DOSUPER;
    if (rc) {
        GETDATA;
        data->ehnode.ehn_Object   = obj;
        data->ehnode.ehn_Class    = cl;
        data->ehnode.ehn_Priority = -1;
        data->ehnode.ehn_Flags    = MUI_EHF_GUIMODE;
        data->ehnode.ehn_Events   = IDCMP_RAWKEY;
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehnode);
    }
    return rc;
}

DEFMMETHOD(HandleEvent)
{
    struct IntuiMessage *imsg = msg->imsg;
    ULONG rc = 0;

    if (imsg && imsg->Class == IDCMP_RAWKEY) {
        struct InputEvent ie;
        TEXT buffer[4];

        ie.ie_Class        = IECLASS_RAWKEY;
        ie.ie_SubClass     = 0;
        ie.ie_Code         = imsg->Code;
        ie.ie_Qualifier    = 0;
        ie.ie_EventAddress = NULL;

        if (MapRawKey(&ie, buffer, sizeof(buffer), NULL) == 1) {
            LONG page = buffer[0] - '0';
            if (page >= 0 && page <= 9) {
                /* wrap-around and qualifiers omitted for brevity */
                set((APTR)getv(obj, MUIA_Parent), MUIA_Group_ActivePage, page);
                rc = MUI_EventHandlerRC_Eat;
            }
        }
    }

    return rc;
}

/* Dispatcher table */
BEGINMTABLE
    DECNEW
    DECMMETHOD(Title_Close)
    DECMMETHOD(Hide)
    DECMMETHOD(Show)
    DECMMETHOD(HandleEvent)
ENDMTABLE

/* Register this class as a subclass of MUIC_Title */
DECSUBCLASS_NC(MUIC_Title, titleclass)
