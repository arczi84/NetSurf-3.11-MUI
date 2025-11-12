/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
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

#include <proto/exec.h>

#include "utils/errors.h"
#include "content/urldb.h"
#include "netsurf/browser.h"
#include "utils/utils.h"

static struct tree hotlist_tree;
#if 0
struct tree *hotlist;

#endif

/**
 * Informs the hotlist that some content has been visited
 *
 * \param content  the content visited
 * \param tree	   the tree to find the URL data from
 * \param node	   the node to update siblings and children of
 */
#if 0
void mui_gui_hotlist_visited(struct content *content, struct tree *tree,
		struct node *node)
{
	struct node_element *element;

	for (; node; node = node->next) {
		if (!node->folder) {
			element = tree_find_element(node, TREE_ELEMENT_URL);
			if ((element) && (!strcmp(element->text,
					content->url))) {
				tree_update_URL_node(node, content->url, NULL);
				tree_handle_node_changed(tree, node, true,
						false);
			}
		}
		if (node->child)
			mui_gui_hotlist_visited(content, tree, node->child);
	}
}

void mui_hotlist_add(struct node *node,struct content *c)
{
	const struct url_data *data;

	data = urldb_get_url_data(c->url);
	if (!data)
	{
		urldb_add_url(c->url);
		urldb_set_url_persistence(c->url,true);
		data = urldb_get_url_data(c->url);
	}

	if (data)
	{
		tree_create_URL_node(node,c->url,data,c->title);
	}

	tree_handle_node_changed(hotlist,node,false,true);

	if(hotlist->handle)
		ami_recreate_listbrowser((struct treeview_window *)hotlist->handle);
}
#endif
