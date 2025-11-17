/*
 * MUI Compatibility Layer for MUI 3.8
 * Provides missing defines/macros for older MUI versions
 */

#ifndef NETSURF_MUI_COMPAT_H
#define NETSURF_MUI_COMPAT_H

#include <libraries/mui.h>

/* Check MUI version */
#ifndef MUIMASTER_VLATEST
#ifdef MUIV_PushMethod_Delay
#define MUIMASTER_VLATEST 20
#else
#define MUIMASTER_VLATEST 0
#endif
#endif

/* PushMethod helper always available */
void mui_queue_method_delay(Object *app, Object *obj, ULONG delay_ms, ULONG method_id);

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

/* Helper macro retained for legacy code */
#define MUI_PUSH_METHOD_DELAYED(app, obj, delay_ms, method) \
    mui_queue_method_delay((app), (obj), (delay_ms), (method))

/* MUI 3.8 compatibility defines */
#if MUIMASTER_VLATEST < 20

/* Base for MUI methods */
#ifndef MUIB_MUI
#define MUIB_MUI 0x80420000UL
#endif

/* Group methods that don't exist in MUI 3.8 */
#ifndef MUIM_Group_AddTail
#define MUIM_Group_AddTail OM_ADDMEMBER
#endif

#ifndef MUIM_Group_Remove
#define MUIM_Group_Remove OM_REMMEMBER
#endif

/* Scrollgroup attributes */
#ifndef MUIA_Scrollgroup_AutoBars
#define MUIA_Scrollgroup_AutoBars 0x8042C2C4UL /* Fake tag, will be ignored */
#endif

/* Application attributes */
#ifndef MUIA_Application_UsedClasses
#define MUIA_Application_UsedClasses 0x8042E9A7UL /* Fake tag, will be ignored */
#endif

/* Textinput attributes */
#ifndef MUIA_Textinput_ResetMarkOnCursor
#define MUIA_Textinput_ResetMarkOnCursor 0x80429AB2UL /* Fake tag, will be ignored */
#endif

/* Pointer attributes and values (new in MUI4) */
#ifndef MUIA_PointerType
#define MUIA_PointerType 0x8042b467UL /* matches MUI4 tag, harmless on 3.8 */
#endif

#ifndef MUIV_PointerType_Normal
#define MUIV_PointerType_Normal 0
#endif

#ifndef MUIV_PointerType_Link
#define MUIV_PointerType_Link 13
#endif

#ifndef MUIV_PointerType_Text
#define MUIV_PointerType_Text 30
#endif

#ifndef MUIV_PointerType_Cross
#define MUIV_PointerType_Cross 7
#endif

#ifndef MUIV_PointerType_DragAndDrop
#define MUIV_PointerType_DragAndDrop 8
#endif

#ifndef MUIV_PointerType_Busy
#define MUIV_PointerType_Busy 1
#endif

#ifndef MUIV_PointerType_Help
#define MUIV_PointerType_Help 12
#endif

#ifndef MUIV_PointerType_NoDrop
#define MUIV_PointerType_NoDrop 15
#endif

#ifndef MUIV_PointerType_Progress
#define MUIV_PointerType_Progress 24
#endif

#ifndef MUIV_PointerType_None
#define MUIV_PointerType_None 16
#endif

/* Title class (doesn't exist in MUI 3.8) */
#ifndef MUIC_Title
#define MUIC_Title "Title.mui"
#endif

/* Some newer stock images are missing entirely on 3.8, provide best-effort fallbacks */
#ifdef MUII_Close
#undef MUII_Close
#endif
#define MUII_Close MUII_CheckMark

#ifdef MUII_ButtonBack
#undef MUII_ButtonBack
#endif
#define MUII_ButtonBack MUII_GroupBack

#ifdef MUII_RequesterBack
#undef MUII_RequesterBack
#endif
#define MUII_RequesterBack MUII_WindowBack

#ifdef MUII_PopDrawer
#undef MUII_PopDrawer
#endif
#define MUII_PopDrawer MUII_PopUp

#ifdef MUII_PopUp
#undef MUII_PopUp
#endif
#define MUII_PopUp MUII_PopFile

#ifdef MUII_Network
#undef MUII_Network
#endif
#define MUII_Network MUII_Assign

/* List methods */
#ifndef MUIM_List_Construct
#define MUIM_List_Construct (MUIB_MUI|0x0000842A)
#endif

#ifndef MUIM_List_Destruct
#define MUIM_List_Destruct (MUIB_MUI|0x00068C93)
#endif

#ifndef MUIM_List_Display  
#define MUIM_List_Display (MUIB_MUI|0x00042D73)
#endif

/* List method structures */
#ifndef MUIP_List_Construct
struct MUIP_List_Construct {
    ULONG MethodID;
    APTR pool;
    APTR entry;
};
#endif

#ifndef MUIP_List_Destruct  
struct MUIP_List_Destruct {
    ULONG MethodID;
    APTR pool;
    APTR entry;
};
#endif

#ifndef MUIP_List_Display
struct MUIP_List_Display {
    ULONG MethodID;
    APTR entry;
    STRPTR *array;
};
#endif

/* File mode helper missing on some 3.8 SDKs */
#if !defined(MODE_WRITE) && defined(MODE_NEWFILE)
#define MODE_WRITE MODE_NEWFILE
#endif

#endif /* MUIMASTER_VLATEST < 20 */

#endif /* NETSURF_MUI_COMPAT_H */
