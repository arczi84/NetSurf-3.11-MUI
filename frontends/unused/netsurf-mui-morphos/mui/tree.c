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

#include "content/urldb.h"
#include "desktop/tree.h"
#include "mui/gui.h"

void tree_initialise_redraw(struct tree *tree)
{
}

void tree_redraw_area(struct tree *tree, int x, int y, int width, int height)
{
}

void tree_draw_line(int x, int y, int width, int height)
{
}

void tree_draw_node_element(struct tree *tree, struct node_element *element)
{
}

void tree_draw_node_expansion(struct tree *tree, struct node *node)
{

}

void tree_recalculate_node_element(struct node_element *element)
{
}

void tree_update_URL_node(struct node *node, const char *url, const struct url_data *data)
{
	struct node_element *element;
	char buffer[256];

	element = tree_find_element(node, TREE_ELEMENT_URL);

	if (!element)
		return;

	if (data) {
		/* node is linked, update */
		if (!data->title)
			urldb_set_url_title(url, url);

		if (!data->title)
			return;

		node->data.text = data->title;
	} else {
		/* node is not linked, find data */
		data = urldb_get_url_data(element->text);
		if (!data)
			return;
	}

/* not implemented yet
	if (element) {
		sprintf(buffer, "small_%.3x", ro_content_filetype_from_type(data->type));
		if (ro_gui_wimp_sprite_exists(buffer))
			tree_set_node_sprite(node, buffer, buffer);
		else
			tree_set_node_sprite(node, "small_xxx", "small_xxx");
	}
*/

	element = tree_find_element(node, TREE_ELEMENT_LAST_VISIT);
	if (element) {
		snprintf(buffer, 256, (char *)messages_get("TreeLast"),
				(data->last_visit > 0) ?
					ctime((time_t *)&data->last_visit) :
					(char *)messages_get("TreeUnknown"));
		if (data->last_visit > 0)
			buffer[strlen(buffer) - 1] = '\0';
		free((void *)element->text);
		element->text = (char *)strdup(buffer);
	}

	element = tree_find_element(node, TREE_ELEMENT_VISITS);
	if (element) {
		snprintf(buffer, 256, (char *)messages_get("TreeVisits"),
				data->visits);
		free((void *)element->text);
		element->text = (char *)strdup(buffer);
	}
}

void tree_resized(struct tree *tree)
{
}

void tree_set_node_sprite_folder(struct node *node)
{
}

void tree_set_node_sprite(struct node *node, const char *sprite, const char *expanded)
{
}
