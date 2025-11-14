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
#include <stddef.h> //arczi

#include <libraries/asl.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "mui/mui.h"
#include "utils/messages.h"

/* Classes management */

struct classdesc {
	char * name;
	APTR initfunc;
	APTR cleanupfunc;
};

#define CLASSENT(s) {#s, (APTR) create_##s##class, (APTR) delete_##s##class}

/* classes declaration */

static const struct classdesc cd[] =
{
	CLASSENT(application),
	CLASSENT(window),
	CLASSENT(navigationbargroup),
	CLASSENT(transferanim),
	CLASSENT(searchbargroup),
	CLASSENT(toolbutton),
#if defined(__MORPHOS2__)
	CLASSENT(title),
#endif
	CLASSENT(titlelabel),
	CLASSENT(browser),
	CLASSENT(popstring),
	CLASSENT(historylist),
	CLASSENT(historypopstring),
	CLASSENT(addressbargroup),
	CLASSENT(findtext),
	CLASSENT(pdf),
	CLASSENT(hotlistwindow),
	CLASSENT(downloadwindow),
	//CLASSENT(loginwindow),
	CLASSENT(quicklinkgroup),
	CLASSENT(quicklinkbuttongroup),
	CLASSENT(quicklinkparentgroup),

	{ 0, 0, 0 }
};

ULONG classes_init(void)
{
	ULONG i;

	for (i = 0; cd[i].name; i++)
	{
		if (!(*(int(*)(void))cd[i].initfunc)())
		{
			//fprintf(stderr, "Couldn't create class %s.\n", cd[i].name);
			return (FALSE);
		}
	}
	return (TRUE);
}

void classes_cleanup(void)
{
	LONG i;

	for (i = sizeof(cd) / sizeof(struct classdesc) - 2; i >= 0; i--)
	{
		(*(void(*)(void))cd[i].cleanupfunc)();
	}
}

/* MUI objects helper functions */

APTR MakeCheck(CONST_STRPTR str, ULONG checked)
{
	APTR obj;

	obj = MUI_MakeObject(MUIO_Checkmark, (IPTR)str);

	if (obj)
		SetAttrs(obj,
			MUIA_CycleChain	, TRUE,
			MUIA_Selected		, checked,
			TAG_DONE);

	return (obj);
}

APTR MakeRect(void)
{
	return RectangleObject, End;
}

APTR MakeButton(CONST_STRPTR msg)
{
	APTR	obj;

	if ((obj = MUI_MakeObject(MUIO_Button, (IPTR)messages_get(msg))))
		SetAttrs(obj, MUIA_CycleChain, TRUE, TAG_DONE);

	return obj;
}

APTR MakeVBar(void)
{
	return RectangleObject, MUIA_Rectangle_VBar, TRUE, MUIA_Weight, 0, End;
}

APTR MakeHBar(void)
{
	return RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Weight, 0, End;
}

APTR MakeLabel(CONST_STRPTR msg)
{
	return MUI_MakeObject(MUIO_Label, (IPTR)messages_get(msg), 0);
}

APTR MakeNewString(CONST_STRPTR str, ULONG maxlen)
{
	APTR	obj;

	if ((obj = MUI_MakeObject(MUIO_String, (IPTR)str, maxlen)))
		SetAttrs(obj, MUIA_CycleChain, 1, MUIA_String_AdvanceOnCR, TRUE, TAG_DONE);

	return obj;
}

APTR MakeString(CONST_STRPTR def, ULONG secret)
{
	return StringObject, StringFrame, MUIA_CycleChain, 1, MUIA_String_AdvanceOnCR, TRUE, MUIA_String_Contents, def, MUIA_String_Secret, secret, TAG_DONE);
}

APTR MakePrefsString(CONST_STRPTR str, CONST_STRPTR def, ULONG maxlen, ULONG id)
{
	APTR	obj;

	if ((obj = MUI_MakeObject(MUIO_String, (IPTR)str, maxlen)))
		SetAttrs(obj, MUIA_CycleChain, 1, MUIA_String_AdvanceOnCR, TRUE, MUIA_ObjectID, id, MUIA_String_Contents, def, TAG_DONE);

	return obj;
}

APTR MakeDirString(CONST_STRPTR str, CONST_STRPTR def, ULONG id)
{
	APTR obj, pop;

	pop = PopButton(MUII_PopDrawer);

	obj = PopaslObject,
		ASLFR_DrawersOnly, TRUE,
		ASLFR_InitialShowVolumes, TRUE,
		MUIA_Popstring_Button, (ULONG)pop,
		MUIA_Popstring_String, (ULONG)MakePrefsString(str, def, 1024, id),
		MUIA_Popasl_Type, ASL_FileRequest,
		TAG_DONE);

	if (obj)
		set(pop, MUIA_CycleChain, 1);

	return obj;
}

APTR MakeCycle(CONST_STRPTR label, const CONST_STRPTR *entries, ULONG id)
{
    APTR obj = MUI_MakeObject(MUIO_Cycle, (IPTR)label, (IPTR)entries);

    if (obj)
        SetAttrs(obj, MUIA_CycleChain, 1, MUIA_ObjectID, id, TAG_DONE);

    return obj;
}

ULONG getv(APTR obj, ULONG attr)
{
	ULONG val;
	GetAttr(attr, obj, &val);
	return val;
}
