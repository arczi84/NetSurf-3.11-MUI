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
#include "desktop/treeview.h"
#include "utils/messages.h"
#include "utils/errors.h"
#include "mui/gui.h"

/* Stub implementations for backward compatibility */

void tree_initialise_redraw(struct tree *tree)
{
	/* This function is no longer needed with treeview API */
}

void tree_redraw_area(struct tree *tree, int x, int y, int width, int height)
{
	/* Redrawing is now handled by treeview_redraw() */
}

void tree_draw_line(int x, int y, int width, int height)
{
	/* Line drawing is handled internally by treeview */
}

void tree_draw_node_element(struct tree *tree, struct node_element *element)
{
	/* Node element drawing is handled internally by treeview */
}

void tree_draw_node_expansion(struct tree *tree, struct node *node)
{
	/* Expansion drawing is handled internally by treeview */
}

void tree_recalculate_node_element(struct node_element *element)
{
	/* Recalculation is handled internally by treeview */
}

/* Updated function to work with new treeview API */
void tree_update_URL_node(struct node *node, const char *url, const struct url_data *data)
{
	/* This function needs to be completely rewritten for the new treeview API
	 * The old tree API used node elements, but the new treeview API uses
	 * field data structures. This function should be replaced with code that:
	 * 
	 * 1. Gets the treeview_node corresponding to the old node
	 * 2. Creates treeview_field_data structures for the URL, last visit, visits
	 * 3. Calls treeview_update_node_entry() with the new field data
	 */
	
	/* For now, this is a stub that does nothing */
	/* TODO: Implement using treeview_update_node_entry() */
}

/* Example of how to update a node with new treeview API */
nserror update_url_node_treeview(treeview *tree, treeview_node *node, 
				  const char *url, const struct url_data *data)
{
	struct treeview_field_data fields[3];
	char last_visit_buffer[256];
	char visits_buffer[256];
	
	if (!data) {
		data = urldb_get_url_data(url);
		if (!data)
			return NSERROR_NOT_FOUND;
	}
	
	/* Field 0: URL/Title */
	fields[0].field = NULL; /* This should be set to the appropriate lwc_string */
	fields[0].value = data->title ? data->title : url;
	fields[0].value_len = strlen(fields[0].value);
	
	/* Field 1: Last Visit */
	if (data->last_visit > 0) {
		snprintf(last_visit_buffer, sizeof(last_visit_buffer), 
			 "%s", messages_get("TreeLast"));
		/* Format the date properly */
		char *date_str = ctime((time_t *)&data->last_visit);
		if (date_str) {
			strncat(last_visit_buffer, date_str, 
				sizeof(last_visit_buffer) - strlen(last_visit_buffer) - 1);
			/* Remove newline */
			char *newline = strchr(last_visit_buffer, '\n');
			if (newline) *newline = '\0';
		}
	} else {
		snprintf(last_visit_buffer, sizeof(last_visit_buffer), 
			 "%s", messages_get("TreeUnknown"));
	}
	
	fields[1].field = NULL; /* This should be set to the appropriate lwc_string */
	fields[1].value = last_visit_buffer;
	fields[1].value_len = strlen(last_visit_buffer);
	
	/* Field 2: Visits */
	snprintf(visits_buffer, sizeof(visits_buffer), 
		 "%s%d", messages_get("TreeVisits"), data->visits);
	
	fields[2].field = NULL; /* This should be set to the appropriate lwc_string */
	fields[2].value = visits_buffer;
	fields[2].value_len = strlen(visits_buffer);
	
	return treeview_update_node_entry(tree, node, fields, NULL);
}

void tree_resized(struct tree *tree)
{
	/* Tree resizing is handled by the core window callbacks in new API */
}

void tree_set_node_sprite_folder(struct node *node)
{
	/* Sprites/icons are handled differently in the new treeview API */
	/* This functionality would need to be implemented in the treeview's
	 * rendering code or through custom field types */
}

void tree_set_node_sprite(struct node *node, const char *sprite, const char *expanded)
{
	/* Sprites/icons are handled differently in the new treeview API */
	/* This functionality would need to be implemented in the treeview's
	 * rendering code or through custom field types */
}
