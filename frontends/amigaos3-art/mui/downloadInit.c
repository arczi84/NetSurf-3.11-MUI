
#include <clib/intuition_protos.h>
#include "amigaos3-art/mui/mui.h"
#include "amigaos3-art/misc.h"
#include "utils/nsoption.h"

#include <SDI_hook.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <math.h>

#include "downloadExtern.h"
#include "download.h"

struct ObjApp * obj;

struct Library *MUIMasterBase;

size_t dnld_header_parse(void *hdr, size_t size, size_t nmemb, void *userdata);

bool file_exists(const char *fname);

void CleanExit(CONST_STRPTR s)
{
	if (s)
	{
		PutStr(s);
	}

	CloseLibrary(MUIMasterBase);
}

void InitApp(void)
{
	if ((MUIMasterBase = OpenLibrary("muimaster.library", 19)) == NULL)
	{
		CleanExit("Can't open muimaster.library v19\n");
	}

}

char *filename;
char *filename2;

static const char *get_outname_from_url(char const* url)
{
  
	char *ssc;
	int l = 0;
	ssc = strstr(url, "/");
	do{
		l = strlen(ssc) + 1;
		url = &url[strlen(url)-l+2];
		ssc = strstr(url, "/");
	}while(ssc);

	//printf("namefromurl=%s\n", url);

    return url;
}

char out[1024];
char out2[1024];


void My_Rename(char *old,  char *new)
{
	char rename[1024];
	
	
	snprintf(rename, sizeof(rename), "rename %s %s", old, new);	
	//printf("rename=%s\n",rename);

	Execute(rename,0,0);
}

size_t dnld_header_parse(void *hdr, size_t size, size_t nmemb, void *userdata)
{
    const   size_t  cb      = size * nmemb;
    const char    *hdr_str= hdr;

    char const*const cdtag = "Content-disposition:";

    if (!strncasecmp(hdr_str, cdtag, strlen(cdtag))) {
      //printf ("Found c-d: %s\n", hdr_str);

	  if (!filename)
	  {
		if (strstr(hdr_str,"=")!=NULL) {
			
			filename = strdup(strstr(hdr_str,"=")+2);
			strlcpy(filename,filename,strlen(filename)-2);
			
			filename2 = strdup(filename);
			
			RemoveSpaces(filename, filename2);

		}
		if (strstr(hdr_str,"on:")!=NULL) {
			filename = strdup(strstr(hdr_str,"on:")+3);

			filename2 = strdup(filename);
			
			RemoveSpaces(filename, filename2);
			
			
		}
		else 
			filename = strdup("getfromurl");
  
	  // printf("out2=%s\n",out2);
	
	  } 
	  
	}
	else
		filename = strdup("getfromurl");

	
    return cb;
}

int fno = 0;

bool file_exists(const char *fname)
{
FILE *fp;

	fp = fopen(fname, "rb");
	if (!fp) return 0;
	fclose(fp);
	return 1;
}

char perc[5];
int perc_int =0;
bool playing = false;


int progress_func(void* ptr, double TotalToDownload, double NowDownloaded, 
                    double TotalToUpload, double NowUploaded)
{
    // ensure that the file to be downloaded is not empty
    // because that would cause a division by zero error later on
    if (TotalToDownload <= 0.0) {
        return 0;
    }

    // how wide you want the progress meter to be
    int totaldotz=100;
    double fractiondownloaded = NowDownloaded / TotalToDownload;
    // part of the progressmeter that's already "full"
    int dotz = round(fractiondownloaded * totaldotz);
	
    // create the "meter"
    int ii=0;	
	perc_int = fractiondownloaded*100;
	
	ULONG signals;
	BOOL running = TRUE;
	
	while(ii < dotz)
		{
			ULONG id = DoMethod(obj->App,MUIM_Application_Input,&signals);

			switch(id)
			{
					case MUIV_Application_ReturnID_Quit:
						if((MUI_RequestA(obj->App,0,0,messages_get("Fetching"),"_Yes|_No",messages_get("AbortDownload"),0)) == 1)
							return 1;
						
			}

			ii++;
		
			//sprintf(perc, "%ld %%",perc_int); //disable "%"
			sprintf(perc, "%ld %",perc_int);
			// part  that's full already
			SetAttrs(obj->GA_progress,
				MUIA_Gauge_Current, perc_int,	
				MUIA_Gauge_InfoText, (LONG)perc,
				TAG_END);
				
			if (play_mpg||play_mp4) {
				if ((perc_int == nsoption_int(mpg_prebuffer)) )		
				{	
					if (playing == false)
					{
						playing = true;
						char *run = malloc(256);
						
						strcpy(run, "run > nil: ");	
						
						if (play_mpg)
							{
							play_mpg = false;
							
							strcat(run, nsoption_charp(mpeg_player));
							strcat(run, " ");
							strcat(run, out);

							}
						else if (play_mp4)
							{			
							play_mp4 = false;
							
							strcat(run, "ffplay  -nogui 0 ");	
							strcat(run, "\"");
							strcat(run, out);
							strcat(run, "\"");
							}
							
						//printf("vid=%s\n", run);
						
						Execute(run,0,0);
						free(run);
					}
					
				}	
			}	
		}
	
    // if you don't return 0, the transfer will be aborted - see the documentation
    return 0; 
}

char * DownloadWindow(char *url, char *pathname, char *name)
{
	InitApp();
	obj = CreateApp();
	
	if (obj)
	{

	CURL *curl;
	FILE *fp;
	CURLcode res;

	if (!name)	
		snprintf(out, sizeof(out), "%s%s", pathname, "temp");
	else
	{
		snprintf(out, sizeof(out), "%s%s", pathname, name);
	

		/*while (file_exists(out)){
			fno++;
			snprintf(out, sizeof(out), "%s%s(%d)", pathname, name, fno);		
		}*/
		

	}
	
	curl = curl_easy_init();
	
	if (curl)
	  {	
		fp = fopen(out,"wb+");
		
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, FALSE);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_func);
		if(!name)
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, dnld_header_parse);	
		
		//printf("filename2=%s\n", filename2);
			
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);	

		fclose(fp);
	
		if (!name)
		{
			if (strcmp(filename,"getfromurl") ==0)
			{
				filename2 = strdup(get_outname_from_url(url));
				//printf("getfromurl2=%s\n", filename2);	
			}
			
			//snprintf(out, sizeof(out), "%s%stemp%d", pathname, fno);
			snprintf(out2, sizeof(out2), "%s%s", pathname, filename2);
			
			while (file_exists(out2)){
				fno++;
				snprintf(out2, sizeof(out2), "%s(%d)",out2, fno);
			}
		
			//printf("out2=%s\n", out2);
			
			My_Rename(out,out2);
			
			if (strlen(filename2)>0)
				free(filename2);
			if (strlen(filename)>0)
				free(filename);	

		}
		fno = 0;
		playing = false;
		DisposeApp(obj);
	  }
	}
	else
	{
		CleanExit("Can't create application\n");
	}
	CleanExit(NULL);
	
}
