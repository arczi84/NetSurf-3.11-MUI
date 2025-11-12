/* AmigaOS 3 misc.h file */

#include <inttypes.h>
#include <dos/dos.h>

#include "utils/nsurl.h"
#include "netsurf/browser_window.h"
#include <proto/asl.h>

bool play_mpg;
bool play_mp3;
bool play_mp4;

bool pointer_off;
bool endnotify;
VOID            notify(UBYTE  *filename);

#ifdef __clib2__
char *strndup(const char *s, size_t n);
#undef HAVE_SCANDIR
#endif
void rerun_netsurf(const char *url);
int check_version(void);
int TimeOut;
struct string *s;
static size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s);
static nserror warn_user(const char *warning, const char *detail);
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream); 

char * fetching_title;
char * DownloadWindow(char *url, char *pathname, char *name);
void My_Rename(char *old,  char *new);
int convert(char *key, char* url);

struct netsurf_table *framebuffer_table;
struct netsurf_table *framebuffer_table_internal;

char * RemoveSpaces(char * source, char * target);
const char *add_theme_path(const char* icon);
void init_string(struct string *s);
char * itoa(unsigned int n);
char *str_replace(char *orig, char *rep, char *with);
void reverse_string(char *str);

void PrintG(char *str,...);
int detect_screen(void);
int order;
int64_t GetFileSize(BPTR fh);
struct browser_window *bw_window;
struct gui_window *g2;
nsurl *url_global;
int OpenPrefs(void);
char *GetVideo (char *url);
char *geturl(char *url);
bool restart, reload_bw, save;
int scale_cp;
bool play_youtube;
char* get_yt_links(char* url);
BPTR cfh;
void url_enter(char* url);
int AskQuit(char *str);
