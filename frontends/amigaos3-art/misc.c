/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <sys/time.h>
#include <ctype.h>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "amigaos3-art/misc.h"
#include "utils/nsoption.h"
#include "utils/messages.h"


static void
reverse(char *s)
{
	int i, j, c;

	for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}

void PrintG(char *str,...)
{
	if (strlen(str) > 0)
	{
		char *cmd = malloc(strlen(str) + (strlen("RequestChoice TITLE=\"NetSurf\" BODY=\" \" GADGETS=\"OK"))+20);
		
		strcpy(cmd, "run >nil: RequestChoice TITLE=\"NetSurf\" BODY=\"");
		strcat(cmd, str);
		strcat(cmd,"\" GADGETS=\"OK\" ");
		
		Execute(cmd,0,0);
		
		free(cmd);
	}
}

int AskQuit(char *str)
{
	if (strlen(str) > 0)
	{
		char *cmd = malloc(strlen(str) + (strlen("RequestChoice TITLE=\"NetSurf\" BODY=\" \" GADGETS=\"OK"))+35);
		
		strcpy(cmd, "RequestChoice > ENV:NSquit TITLE=\"NetSurf\" BODY=\"");
		strcat(cmd, str);
		strcat(cmd,"\" GADGETS=\"OK| ");
		strcat(cmd, (char*)(messages_get("Cancel")));
		strcat(cmd,"\"");
				
		Execute(cmd,0,0);
		
		free(cmd);
		
		int askquit = atoi(getenv("NSquit"));

		return askquit;	
		
	}
}

/* DOS */
int64_t GetFileSize(BPTR fh)
{
	int32_t size = 0;
	struct FileInfoBlock *fib = AllocVec(sizeof(struct FileInfoBlock), MEMF_ANY);
	if(fib == NULL) return 0;

	ExamineFH(fh, fib);
	size = fib->fib_Size;

	FreeVec(fib);
	return (int64_t)size;
}

char * RemoveSpaces(char * source, char * target)
{
     while(*source++ && *target)
     {
        if (!isspace(*source)) 
             *target++ = *source;
     }
	 *target = 0;
	 
     return target;
}


// You must free the result if result is non-NULL.
char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig)
        return NULL;
    if (!rep)
        rep = "";
    len_rep = strlen(rep);
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

#ifdef JS
void __show_error(char *msg);

void __show_error(char *msg)
{
		Printf(msg);
}
#endif


