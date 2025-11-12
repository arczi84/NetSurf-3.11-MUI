#ifndef GUI_LOCALE_H
#define GUI_LOCALE_H 1

/*
** gui_locale.h
**
** (c) 2006 by Guido Mersmann
**
** Object source created by SimpleCat
*/

/*************************************************************************/

#include "locale_strings.h" /* change name to correct locale header if needed */

/*
** Prototypes
*/

BOOL   Locale_Open( STRPTR catname, ULONG version, ULONG revision);
void   Locale_Close(void);
STRPTR GetMBString(long ID);

/*************************************************************************/

#endif /* GUI_LOCALE_H */
