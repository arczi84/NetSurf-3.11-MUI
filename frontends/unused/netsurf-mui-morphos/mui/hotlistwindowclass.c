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

#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>

#include <mui/Listtree_mcc.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "content/content.h"
#include "mui/methodstack.h"
#include "mui/mui.h"
#include "mui/netsurf.h"
#include "mui/utils.h"

struct Data
{
	APTR active_node;
	APTR listtree;
	APTR title;
	APTR location;
	APTR remove;
	LONG bookmarks;
};

STATIC CONST TEXT header[] =
{
	"<!-- NetSurf Hotlist -->\n"
	"<html><head><title>NetSurf Hotlist<title></head>\n"
	"<body><b><p align=\"center\">NetSurf Hotlist</p></b>\n"
	"<hr>\n"
};

STATIC CONST TEXT tail[] = "</body></html>";
STATIC CONST TEXT start[] = "<ul>\n";
STATIC CONST TEXT end[] = "</ul>\n";

extern xmlNode *options_find_tree_element(xmlNode *node, const char *name);

static void options_load_tree_directory(APTR obj, xmlNode *ul, struct MUIS_Listtree_TreeNode * node);

static void add_leaf(APTR obj, xmlNode *n, struct MUIS_Listtree_TreeNode * node)
{
	char *title;

	title = (char *) xmlNodeGetContent(n);

	if (title)
	{
		APTR newnode;

		for (n = n->next; n && n->type != XML_ELEMENT_NODE; n = n->next);

		if (n && strcmp((const char *) n->name, "ul") == 0)
		{
			STRPTR p;

			p = utf8_to_amiga(title);

			if (p)
			{
				APTR newnode;

				newnode = (APTR)DoMethod(obj, MUIM_Listtree_Insert, title, NULL, node, MUIV_Listtree_Insert_PrevNode_Tail, TNF_LIST);

				FreeVec(p);

				if (newnode)
				{
					options_load_tree_directory(obj, n, newnode);
				}
			}
		}

		xmlFree(title);
	}
}

static void options_load_tree_entry(APTR obj, xmlNode *li, struct MUIS_Listtree_TreeNode * node)
{
	char *url, *title;
	xmlNode *n;

	url = title = NULL;

	for (n = li->children; n; n = n->next)
	{
		if (n->type == XML_ELEMENT_NODE)
		{
			if (strcmp((const char *) n->name, "a") == 0)
			{
				url = (char *) xmlGetProp(n, (const xmlChar *) "href");
				title = (char *) xmlNodeGetContent(n);

				if (url && title)
				{
					STRPTR p1, p2;

					p1 = utf8_to_amiga(url);
					p2 = utf8_to_amiga(title);

					if (p1 && p2)
						DoMethod(obj, MUIM_Listtree_Insert, p2, p1, node, MUIV_Listtree_Insert_PrevNode_Tail, 0, 0);

					FreeVec(p1);
					FreeVec(p2);
				}

				if (url)
					xmlFree(url);

				if (title)
					xmlFree(title);

				break;
			}
			else if (strcmp((const char *)n->name, "b") == 0)
			{
				add_leaf(obj, n, node);
				break;;
			}
		}
	}
}

static void options_load_tree_directory(APTR obj, xmlNode *ul, struct MUIS_Listtree_TreeNode * node)
{
	xmlNode *n;

	for (n = ul->children; n; n = n->next)
	{
		char *title, *url;

		if (n->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char *) n->name, "li") == 0)
		{
			options_load_tree_entry(obj, n, node);
		}
		else if (strcmp((const char *) n->name, "h4") == 0)
		{
			add_leaf(obj, n, node);
		}
	}
}

static void options_load_tree(APTR obj, const char *filename)
{
	BPTR fh = Open(APPLICATION_HOTLIST_FILE, MODE_OLDFILE);

	if (fh)
	{
		xmlDoc *doc;
		xmlNode *html, *body, *ul;

		doc = htmlParseFile(filename, "iso-8859-1");

		if (doc)
		{
			html = options_find_tree_element((xmlNode *) doc, "html");
			body = options_find_tree_element(html, "body");
			ul = options_find_tree_element(body, "ul");

			if (ul)
			{
				options_load_tree_directory(obj, ul, MUIV_Listtree_Insert_ListNode_Root);
			}

			xmlFreeDoc(doc);
		}

		Close(fh);
	}
}

DEFNEW
{
	APTR add_group, add_link, remove;
	APTR title, location;
	APTR listtree;

	obj = DoSuperNew(cl, obj,
			MUIA_Window_ID, MAKE_ID('W','B','M','K'),
			MUIA_Window_Width , MUIV_Window_Width_Visible(50),
			MUIA_Window_Height , MUIV_Window_Height_Visible(50),
			MUIA_Window_TopEdge , MUIV_Window_TopEdge_Centered,
			MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
			MUIA_Window_Title, "Hotlist",
			MUIA_Window_NoMenus, TRUE,
			WindowContents, VGroup,
				Child, listtree = ListtreeObject, InputListFrame,
					MUIA_Listtree_SortHook, MUIV_Listtree_SortHook_LeavesBottom,
					MUIA_Listtree_ConstructHook, MUIV_Listtree_ConstructHook_String,
					MUIA_Listtree_DestructHook, MUIV_Listtree_DestructHook_String,
				End,
				Child, HGroup,
					Child, add_group = MakeButton("Add _Group"),
					Child, add_link = MakeButton("Add _Link"),
					Child, remove = MakeButton("_Remove"),
				End,
				Child, MakeHBar(),
				Child, ColGroup(2),
					Child, MakeLabel("_Name"),
					Child, title = MakeNewString("_Name", 120),
					Child, MakeLabel("L_ocation"),
					Child, location = MakeNewString("L_ocation", 1024),
				End,
			End,
			TAG_MORE, msg->ops_AttrList);

	if (obj)
	{
		GETDATA;

		data->listtree = listtree;
		data->title = title;
		data->location = location;
		data->remove = remove;

		DoMethod(obj, MUIM_MultiSet, MUIA_Disabled, TRUE, remove, title, location, NULL);

		DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, obj, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(listtree, MUIM_Notify, MUIA_Listtree_Active, MUIV_EveryTime, obj, 2, MM_HotlistWindow_Activate, MUIV_TriggerValue);
		DoMethod(listtree, MUIM_Notify, MUIA_Listtree_DoubleClick, MUIV_EveryTime, obj, 2, MM_Browser_Go, MUIV_TriggerValue);
		DoMethod(add_link, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MM_HotlistWindow_Modify, BM_ADD_LINK);
		DoMethod(add_group, MUIM_Notify, MUIA_Pressed, FALSE,  obj, 2, MM_HotlistWindow_Modify, BM_ADD_GROUP);
		DoMethod(remove, MUIM_Notify, MUIA_Pressed, FALSE, obj, 2, MM_HotlistWindow_Modify, BM_REMOVE);

//		DoMethod(title, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, obj, 2,  NULL, NULL);
//		DoMethod(location, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, obj, 2,  NULL, NULL);
	}

	return (IPTR)obj;
}

STATIC VOID traverse_nodes(APTR obj, BPTR fh, struct MUIS_Listtree_TreeNode * node)
{
	LONG pos;

	pos = MUIV_Listtree_GetEntry_Position_Head;

	FWrite(fh, (APTR)start, sizeof(start)-1, 1);

	while ((node = (APTR)DoMethod(obj, MUIM_Listtree_GetEntry, node, pos, 0)))
	{
		pos = MUIV_Listtree_GetEntry_Position_Next;

		if (node->tn_Flags & TNF_LIST)
		{
			FWritef(fh, "<li><b>%s</b>\n", node->tn_Name);
			traverse_nodes(obj, fh, node);
		}
		else
		{
			FWritef(fh, "<li><a href=\"%s\">%s</a></li>\n", node->tn_User, node->tn_Name);
		}
	}

	FWrite(fh, (APTR)end, sizeof(end)-1, 1);
}

DEFDISP
{
	GETDATA;

	if (data->bookmarks)
	{
		BPTR fh;

		fh = Open(APPLICATION_HOTLIST_FILE, MODE_NEWFILE);

		if (fh)
		{
			FWrite(fh, (APTR)header, sizeof(header)-1, 1);
			traverse_nodes(data->listtree, fh, (APTR)MUIV_Listtree_GetEntry_ListNode_Root);
			FWrite(fh, (APTR)tail, sizeof(tail)-1, 1);
			Close(fh);
		}
	}

	return DOSUPER;
}

STATIC VOID load_hotlist(struct Data *data)
{
	if (data->bookmarks == FALSE)
	{
		data->bookmarks = TRUE;
		options_load_tree(data->listtree, APPLICATION_HOTLIST_FILE);
	}
}

DEFMMETHOD(Window_Setup)
{
	GETDATA;
	load_hotlist(data);
	return DOSUPER;
}

DEFSMETHOD(HotlistWindow_Activate)
{
	struct MUIS_Listtree_TreeNode *node;
	GETDATA;

	node = msg->node;

	data->active_node = node;

	set(data->title, MUIA_String_Contents, node ? node->tn_Name : "");
	set(data->location, MUIA_String_Contents, node ? node->tn_User : "");

	DoMethod(obj, MUIM_MultiSet, MUIA_Disabled, node ? TRUE : FALSE, data->remove, data->title, data->location, NULL);

	return 0;
}

DEFSMETHOD(HotlistWindow_Insert)
{
	GETDATA;
	APTR active;

	load_hotlist(data);

	active = data->active_node;

	DoMethod(data->listtree, MUIM_Listtree_Insert, msg->title, msg->url, active ? active : MUIV_Listtree_Insert_ListNode_Root, MUIV_Listtree_Insert_PrevNode_Active, 0);

	if (getv(obj, MUIA_Window_Open))
		DoMethod(obj, MUIM_Window_ToFront);

	return 0;
}

DEFSMETHOD(HotlistWindow_Modify)
{
	GETDATA;
	ULONG flags;
	APTR active, node;

	flags = TNF_LIST;

	switch (msg->operation)
	{
		case BM_ADD_LINK:
			set(data->location, MUIA_Disabled, FALSE);
			flags = 0;
		case BM_ADD_GROUP:
			set(data->title, MUIA_Disabled, FALSE);

			active = data->active_node;

			DoMethod(data->listtree, MUIM_Listtree_Insert, "", "", active ? active : MUIV_Listtree_Insert_ListNode_Root, MUIV_Listtree_Insert_PrevNode_Tail, flags);
			break;

		case BM_REMOVE:
			DoMethod(data->listtree, MUIM_Listtree_Remove, MUIV_Listtree_Remove_ListNode_Active, MUIV_Listtree_Remove_TreeNode_Active, 0);
			break;
	}

	return 0;
}

DEFSMETHOD(Browser_Go)
{
	struct MUIS_Listtree_TreeNode *node;

	node = (APTR)msg->url;

	NewRawDoFmt("%s\n", (APTR)1, NULL, node->tn_Name);

	if (node)
	{
		msg->url = node->tn_User;
		DoMethodA(_app(obj), msg);
		set(obj, MUIA_Window_Open, FALSE);
	}

	return 0;
}

BEGINMTABLE
DECNEW
DECDISP
DECSMETHOD(Browser_Go)
DECSMETHOD(HotlistWindow_Activate)
DECSMETHOD(HotlistWindow_Insert)
DECSMETHOD(HotlistWindow_Modify)
DECMMETHOD(Window_Setup)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Window, hotlistwindowclass)

void hotlist_visited(struct content *content)
{
	if (content && content->url)
	{
		//mui_gui_hotlist_visited(content, hotlist, hotlist->root);
	}
}
