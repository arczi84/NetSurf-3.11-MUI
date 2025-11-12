/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <libwapcaplet/libwapcaplet.h>

#include "content/urldb.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "utils/utils.h"
#include "netsurf/browser_window.h"
#include "desktop/treeview.h"
#include "mui/history.h"
#include "mui/netsurf.h"

#define MAXIMUM_URL_LENGTH 2048
#define MAXIMUM_BASE_NODES 16
#define GLOBAL_HISTORY_RECENT_URLS 16
#define N_DAYS 28
#define N_SEC_PER_DAY (60 * 60 * 24)

static bool global_history_add_internal(nsurl *url, const struct url_data *data);
void global_history_add_recent(const char *url);

enum global_history_folders {
	GH_TODAY = 0,
	GH_YESTERDAY,
	GH_2_DAYS_AGO,
	GH_3_DAYS_AGO,
	GH_4_DAYS_AGO,
	GH_5_DAYS_AGO,
	GH_6_DAYS_AGO,
	GH_LAST_WEEK,
	GH_2_WEEKS_AGO,
	GH_3_WEEKS_AGO,
	GH_N_FOLDERS
};

enum global_history_fields {
	GH_TITLE,
	GH_URL,
	GH_LAST_VISIT,
	GH_VISITS,
	GH_PERIOD,
	N_FIELDS
};

struct global_history_folder {
	treeview_node *folder;
	struct treeview_field_data data;
};

struct global_history_ctx {
	treeview *tree;
	struct treeview_field_desc fields[N_FIELDS];
	struct global_history_folder folders[GH_N_FOLDERS];
	time_t today;
	int weekday;
	bool built;
};

struct global_history_entry {
	bool user_delete;
	int slot;
	nsurl *url;
	time_t t;
	treeview_node *entry;
	struct global_history_entry *next;
	struct global_history_entry *prev;
	struct treeview_field_data data[N_FIELDS - 1];
};

static struct global_history_ctx gh_ctx;
static struct global_history_entry *gh_list[N_DAYS];
static char *global_history_recent_url[GLOBAL_HISTORY_RECENT_URLS];
static int global_history_recent_count = 0;
static bool global_history_init = false;

/**
 * Find an entry in the global history
 *
 * \param url The URL to find
 * \return Pointer to history entry, or NULL if not found
 */
static struct global_history_entry *mui_global_history_find(nsurl *url)
{
	int i;
	struct global_history_entry *e;

	for (i = 0; i < N_DAYS; i++) {
		e = gh_list[i];
		while (e != NULL) {
			if (nsurl_compare(e->url, url, NSURL_COMPLETE) == true) {
				return e;
			}
			e = e->next;
		}
	}
	return NULL;
}

/**
 * Initialise the treeview directories
 */
static nserror mui_global_history_create_dir(enum global_history_folders f)
{
	nserror err;
	treeview_node *relation = NULL;
	enum treeview_relationship rel = TREE_REL_FIRST_CHILD;
	const char *label;
	int i;

	switch (f) {
	case GH_TODAY:
		label = "DateToday";
		break;
	case GH_YESTERDAY:
		label = "DateYesterday";
		break;
	case GH_2_DAYS_AGO:
		label = "Date2Days";
		break;
	case GH_3_DAYS_AGO:
		label = "Date3Days";
		break;
	case GH_4_DAYS_AGO:
		label = "Date4Days";
		break;
	case GH_5_DAYS_AGO:
		label = "Date5Days";
		break;
	case GH_6_DAYS_AGO:
		label = "Date6Days";
		break;
	case GH_LAST_WEEK:
		label = "Date1Week";
		break;
	case GH_2_WEEKS_AGO:
		label = "Date2Week";
		break;
	case GH_3_WEEKS_AGO:
		label = "Date3Week";
		break;
	default:
		return NSERROR_BAD_PARAMETER;
	}

	label = messages_get(label);

	for (i = f - 1; i >= 0; i--) {
		if (gh_ctx.folders[i].folder != NULL) {
			relation = gh_ctx.folders[i].folder;
			rel = TREE_REL_NEXT_SIBLING;
			break;
		}
	}

	gh_ctx.folders[f].data.field = gh_ctx.fields[N_FIELDS - 1].field;
	gh_ctx.folders[f].data.value = label;
	gh_ctx.folders[f].data.value_len = strlen(label);
	
	err = treeview_create_node_folder(gh_ctx.tree,
			&gh_ctx.folders[f].folder,
			relation, rel,
			&gh_ctx.folders[f].data,
			&gh_ctx.folders[f],
			gh_ctx.built ? TREE_OPTION_NONE :
					TREE_OPTION_SUPPRESS_RESIZE |
					TREE_OPTION_SUPPRESS_REDRAW);

	return err;
}

/**
 * Initialise the treeview entry fields
 */
static nserror mui_global_history_initialise_entry_fields(void)
{
	int i;
	const char *label;

	for (i = 0; i < N_FIELDS; i++)
		gh_ctx.fields[i].field = NULL;

	gh_ctx.fields[GH_TITLE].flags = TREE_FLAG_DEFAULT;
	label = messages_get("TreeviewLabelTitle");
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[GH_TITLE].field) != lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[GH_URL].flags = TREE_FLAG_COPY_TEXT | TREE_FLAG_SEARCHABLE;
	label = messages_get("TreeviewLabelURL");
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[GH_URL].field) != lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[GH_LAST_VISIT].flags = TREE_FLAG_SHOW_NAME;
	label = messages_get("TreeviewLabelLastVisit");
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[GH_LAST_VISIT].field) != lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[GH_VISITS].flags = TREE_FLAG_SHOW_NAME;
	label = messages_get("TreeviewLabelVisits");
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[GH_VISITS].field) != lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[GH_PERIOD].flags = TREE_FLAG_DEFAULT;
	label = messages_get("TreeviewLabelPeriod");
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[GH_PERIOD].field) != lwc_error_ok) {
		goto error;
	}

	return NSERROR_OK;

error:
	for (i = 0; i < N_FIELDS; i++)
		if (gh_ctx.fields[i].field != NULL)
			lwc_string_unref(gh_ctx.fields[i].field);

	return NSERROR_UNKNOWN;
}

/**
 * Initialise the time
 */
static nserror mui_global_history_initialise_time(void)
{
	struct tm *full_time;
	time_t t;

	t = time(NULL);
	if (t == -1) {
		LOG(("time info unavailable"));
		return NSERROR_UNKNOWN;
	}

	full_time = localtime(&t);
	full_time->tm_sec = 0;
	full_time->tm_min = 0;
	full_time->tm_hour = 0;
	t = mktime(full_time);
	if (t == -1) {
		LOG(("mktime failed"));
		return NSERROR_UNKNOWN;
	}

	gh_ctx.today = t;
	gh_ctx.weekday = full_time->tm_wday;

	return NSERROR_OK;
}

/**
 * Callback functions for treeview
 */
static nserror mui_global_history_tree_node_folder_cb(
		struct treeview_node_msg msg, void *data)
{
	struct global_history_folder *f = data;

	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
		f->folder = NULL;
		break;
	case TREE_MSG_NODE_EDIT:
		break;
	case TREE_MSG_NODE_LAUNCH:
		break;
	}

	return NSERROR_OK;
}

static nserror mui_global_history_tree_node_entry_cb(
		struct treeview_node_msg msg, void *data)
{
	struct global_history_entry *e = data;
	nserror ret = NSERROR_OK;

	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
		e->entry = NULL;
		e->user_delete = msg.data.delete.user;
		/* Delete entry from list and free memory */
		if (e->prev)
			e->prev->next = e->next;
		else
			gh_list[e->slot] = e->next;
		
		if (e->next)
			e->next->prev = e->prev;

		if (e->user_delete) {
			urldb_reset_url_visit_data(e->url);
		}

		free((void *)e->data[GH_TITLE].value);
		free((void *)e->data[GH_LAST_VISIT].value);
		free((void *)e->data[GH_VISITS].value);
		nsurl_unref(e->url);
		free(e);
		break;

	case TREE_MSG_NODE_EDIT:
		break;

	case TREE_MSG_NODE_LAUNCH:
	{
		struct browser_window *existing = NULL;
		enum browser_window_create_flags flags = BW_CREATE_HISTORY;

		if (msg.data.node_launch.mouse &
				(BROWSER_MOUSE_MOD_1 | BROWSER_MOUSE_MOD_2) ||
				existing == NULL) {
			/* Shift or Ctrl launch, open in new window */
		}

		ret = browser_window_create(flags, e->url, NULL, existing, NULL);
	}
		break;
	}
	return ret;
}

static struct treeview_callback_table gh_tree_cb_t = {
	.folder = mui_global_history_tree_node_folder_cb,
	.entry = mui_global_history_tree_node_entry_cb
};

void mui_global_history_initialise(void)
{
	char s[MAXIMUM_URL_LENGTH];
	BPTR fp;
	nserror err;

	/* Initialize time */
	err = mui_global_history_initialise_time();
	if (err != NSERROR_OK) {
		LOG(("Failed to initialize time"));
		return;
	}

	/* Initialize entry fields */
	err = mui_global_history_initialise_entry_fields();
	if (err != NSERROR_OK) {
		LOG(("Failed to initialize entry fields"));
		return;
	}

	/* Load recent URLs */
	fp = Open(APPLICATION_RECENT_FILE, MODE_OLDFILE);
	if (fp) {
		while (FGets(fp, s, MAXIMUM_URL_LENGTH)) {
			if (s[strlen(s) - 1] == '\n')
				s[strlen(s) - 1] = '\0';
			global_history_add_recent(s);
		}
		Close(fp);
	} else {
		LOG(("Failed to open file '%s' for reading", APPLICATION_RECENT_FILE));
	}

	global_history_init = true;
	urldb_iterate_entries(global_history_add_internal);
	global_history_init = false;

	/* Create the treeview */
	err = treeview_create(&gh_ctx.tree, &gh_tree_cb_t,
			N_FIELDS, gh_ctx.fields,
			NULL, NULL,
			TREEVIEW_NO_MOVES | TREEVIEW_DEL_EMPTY_DIRS |
			TREEVIEW_SEARCHABLE);
	if (err != NSERROR_OK) {
		LOG(("Failed to create treeview"));
		return;
	}

	/* Create folder for today */
	err = mui_global_history_create_dir(GH_TODAY);
	if (err != NSERROR_OK) {
		LOG(("Failed to create today folder"));
		return;
	}

	gh_ctx.built = true;
}

static nserror global_history_add(nsurl *url)
{
	const struct url_data *data;

	if (gh_ctx.tree == NULL)
		return NSERROR_OK;

	data = urldb_get_url_data(url);
	if (data == NULL) {
		LOG(("Can't add URL to history that's not present in urldb."));
		return NSERROR_BAD_PARAMETER;
	}

	global_history_add_internal(url, data);
	return NSERROR_OK;
}

/**
 * Internal routine to actually perform global history addition
 */
static bool global_history_add_internal(nsurl *url, const struct url_data *data)
{
	struct global_history_entry *e;
	int slot;
	time_t visit_date;
	time_t earliest_date = gh_ctx.today - (N_DAYS - 1) * N_SEC_PER_DAY;
	bool got_treeview = gh_ctx.tree != NULL;
	char buffer[16];
	const char *last_visited;
	char *last_visited2;
	const char *title;
	int len;
	nserror err;
	treeview_node *parent;

	assert((url != NULL) && (data != NULL));

	visit_date = data->last_visit;

	/* Find day array slot for entry */
	if (visit_date >= gh_ctx.today) {
		slot = 0;
	} else if (visit_date >= earliest_date) {
		slot = (gh_ctx.today - visit_date) / N_SEC_PER_DAY + 1;
	} else {
		/* too old */
		return true;
	}

	if (got_treeview == true) {
		/* Delete any existing entry for this URL */
		e = mui_global_history_find(url);
		if (e != NULL) {
			treeview_delete_node(gh_ctx.tree, e->entry,
					TREE_OPTION_SUPPRESS_REDRAW |
					TREE_OPTION_SUPPRESS_RESIZE);
		}
	}

	/* Create new entry */
	e = malloc(sizeof(struct global_history_entry));
	if (e == NULL) {
		return false;
	}

	e->user_delete = false;
	e->slot = slot;
	e->url = nsurl_ref(url);
	e->t = data->last_visit;
	e->entry = NULL;
	e->next = NULL;
	e->prev = NULL;

	/* Set up field data */
	title = (data->title != NULL) ? data->title : messages_get("NoTitle");
	
	e->data[GH_TITLE].field = gh_ctx.fields[GH_TITLE].field;
	e->data[GH_TITLE].value = strdup(title);
	e->data[GH_TITLE].value_len = strlen(title);

	e->data[GH_URL].field = gh_ctx.fields[GH_URL].field;
	e->data[GH_URL].value = nsurl_access(e->url);
	e->data[GH_URL].value_len = nsurl_length(e->url);

	last_visited = ctime(&data->last_visit);
	last_visited2 = strdup(last_visited);
	if (last_visited2 != NULL) {
		last_visited2[24] = '\0';
	}

	e->data[GH_LAST_VISIT].field = gh_ctx.fields[GH_LAST_VISIT].field;
	e->data[GH_LAST_VISIT].value = last_visited2;
	e->data[GH_LAST_VISIT].value_len = (last_visited2 != NULL) ? 24 : 0;

	len = snprintf(buffer, 16, "%u", data->visits);
	if (len == 16) {
		len--;
		buffer[len] = '\0';
	}

	e->data[GH_VISITS].field = gh_ctx.fields[GH_VISITS].field;
	e->data[GH_VISITS].value = strdup(buffer);
	e->data[GH_VISITS].value_len = len;

	/* Insert into linked list */
	if (gh_list[slot] == NULL) {
		gh_list[slot] = e;
	} else if (gh_list[slot]->t < e->t) {
		e->next = gh_list[slot];
		gh_list[slot]->prev = e;
		gh_list[slot] = e;
	} else {
		struct global_history_entry *prev = gh_list[slot];
		struct global_history_entry *curr = prev->next;
		while (curr != NULL) {
			if (curr->t < e->t) {
				break;
			}
			prev = curr;
			curr = curr->next;
		}

		e->next = curr;
		e->prev = prev;
		prev->next = e;

		if (curr != NULL)
			curr->prev = e;
	}

	if (got_treeview) {
		/* Get parent folder */
		int folder_index;
		if (slot < 7) {
			folder_index = slot;
		} else if (slot < 14) {
			folder_index = GH_LAST_WEEK;
		} else if (slot < 21) {
			folder_index = GH_2_WEEKS_AGO;
		} else if (slot < N_DAYS) {
			folder_index = GH_3_WEEKS_AGO;
		} else {
			return false;
		}

		if (gh_ctx.folders[folder_index].folder == NULL) {
			err = mui_global_history_create_dir(folder_index);
			if (err != NSERROR_OK) {
				return false;
			}
		}

		parent = gh_ctx.folders[folder_index].folder;

		err = treeview_create_node_entry(gh_ctx.tree, &(e->entry),
				parent, TREE_REL_FIRST_CHILD, e->data, e,
				gh_ctx.built ? TREE_OPTION_NONE :
						TREE_OPTION_SUPPRESS_RESIZE |
						TREE_OPTION_SUPPRESS_REDRAW);
		if (err != NSERROR_OK) {
			return false;
		}
	}

	return true;
}

/**
 * Saves the global history's recent URL data.
 */
void mui_global_history_save(void)
{
	BPTR fp;

	fp = Open(APPLICATION_RECENT_FILE, MODE_NEWFILE);
	if (fp) {
		int i;
		for (i = global_history_recent_count - 1; i >= 0; i--) {
			FPrintf(fp, "%s\n", global_history_recent_url[i]);
		}
		Close(fp);
	}
}

void global_history_add_recent(const char *url)
{
	int i;
	int j = -1;
	char *current;

	/* try to find a string already there */
	for (i = 0; i < global_history_recent_count; i++)
		if (global_history_recent_url[i] &&
				!strcmp(global_history_recent_url[i], url))
			j = i;

	/* already at head of list */
	if (j == 0)
		return;

	if (j < 0) {
		/* add to head of list */
		free(global_history_recent_url[GLOBAL_HISTORY_RECENT_URLS - 1]);
		memmove(&global_history_recent_url[1],
				&global_history_recent_url[0],
				(GLOBAL_HISTORY_RECENT_URLS - 1) * sizeof(char *));
		global_history_recent_url[0] = strdup(url);
		global_history_recent_count++;
		if (global_history_recent_count > GLOBAL_HISTORY_RECENT_URLS)
			global_history_recent_count = GLOBAL_HISTORY_RECENT_URLS;
	} else {
		/* move to head of list */
		current = global_history_recent_url[j];
		for (i = j; i > 0; i--)
			global_history_recent_url[i] = global_history_recent_url[i - 1];
		global_history_recent_url[0] = current;
	}
}

/**
 * Gets details of the currently used URL list.
 */
char **global_history_get_recent(int *count)
{
	*count = global_history_recent_count;
	return global_history_recent_url;
}

void mui_global_history_finalise(void)
{
	int i;

	if (gh_ctx.tree != NULL) {
		treeview_destroy(gh_ctx.tree);
		gh_ctx.tree = NULL;
	}

	/* Free field names */
	for (i = 0; i < N_FIELDS; i++)
		if (gh_ctx.fields[i].field != NULL)
			lwc_string_unref(gh_ctx.fields[i].field);

	gh_ctx.built = false;
}