
char * DownloadWindow(char *url, char *pathname, char *name);
int GetFile(char *url, char *pathname, char *name);
char * RemoveSpaces(char * source, char * target);
void My_Rename(char *old,  char *new);
int progress_func(void* ptr, double TotalToDownload, double NowDownloaded, 
                    double TotalToUpload, double NowUploaded);

#define CreateApp CreateDownloadApp
#define DisposeApp DisposeDownloadApp
#define CleanExit CleanDownloadExit

void CleanExit(CONST_STRPTR s);

__inline Object * MUI_NewObject(CONST_STRPTR cl, Tag tags, ...);
__inline Object * MUI_MakeObject(LONG type, ...);
