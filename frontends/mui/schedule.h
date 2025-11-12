#ifndef MUI_SCHEDULE_H
#define MUI_SCHEDULE_H

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

#include "utils/errors.h"
#include <stdbool.h>

extern unsigned long schedule_sig;

bool mui_schedule_init(void);
void mui_schedule_poll(void);
void mui_schedule_finalise(void);
bool mui_schedule_has_tasks(void);
nserror mui_schedule(int t, void (*callback)(void *p), void *p);

#endif
