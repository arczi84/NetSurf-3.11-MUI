/*
 * MUI Compatibility Layer for MUI 3.8
 * Provides missing defines/macros for older MUI versions
 */

#ifndef NETSURF_MUI_COMPAT_H
#define NETSURF_MUI_COMPAT_H

#include <libraries/mui.h>

/* Check MUI version */
#ifndef MUIMASTER_VLATEST
#define MUIMASTER_VLATEST 0
#endif

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

/* PushMethod values */
#ifndef MUIV_PushMethod_Delay
/* MUI 3.8 doesn't support delayed methods, just return number of args without delay flag */
#define MUIV_PushMethod_Delay(ms) (0)
#endif

/* Helper macro for delayed push method calls on MUI 3.8 */
#define MUI_PUSH_METHOD_DELAYED(app, obj, numargs, method, ...) \
    DoMethod((app), MUIM_Application_PushMethod, (obj), (numargs), (method), ##__VA_ARGS__)

/* Textinput attributes */
#ifndef MUIA_Textinput_ResetMarkOnCursor
#define MUIA_Textinput_ResetMarkOnCursor 0x80429AB2UL /* Fake tag, will be ignored */
#endif

/* Title class (doesn't exist in MUI 3.8) */
#ifndef MUIC_Title
#define MUIC_Title "Title.mui"
#endif

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

#endif /* MUIMASTER_VLATEST < 20 */

#endif /* NETSURF_MUI_COMPAT_H */
