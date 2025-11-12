#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <clib/intuition_protos.h>
#include <exec/memory.h>
#ifdef SDLLIB
#include <SDL/SDL_inline.h>
#endif
#include "amigaos3-art/misc.h"
#include "amigaos3-art/version.h"
#include "utils/nsoption.h"

struct EasyStruct ESupdate =
    {
    sizeof(struct EasyStruct),
    0,
	"",
    "New version is available!",
	"Get it!|No thanks|Don't bother me anymore",
    };
	
int check_version(void) {
    CURL *curl;
    FILE *fp;
    CURLcode res;
	
#if defined AGA
	remove("env:nsagaversion");
	remove("NetSurf-AGA-bak");
	char *url = "http://netsurf.baderman.net/nsagaversion";
    char outfilename[FILENAME_MAX] = "ENV:nsagaversion";
#else
	remove("ENV:nsversion");
	remove("NetSurf-bak");
    char *url = "http://netsurf.baderman.net/nsversion";
    char outfilename[FILENAME_MAX] = "ENV:nsversion";
#endif

    curl = curl_easy_init();
    if (curl) {
        fp = fopen(outfilename,"w+");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
		fclose(fp);
		      /* always cleanup */
        curl_easy_cleanup(curl);
		#if defined AGA
		/*Read RAm:version*/		
		char *ver = getenv("ENV:nsagaversion");
		#else
		char *ver = getenv("ENV:nsversion");
	    #endif

		int iver = atoi(ver);
		//printf("VERSION=%d",iver);
		
		if (iver > VERSION )
		{	
			int answer = EasyRequest(NULL, &ESupdate, NULL);
			if (answer == 0)
				nsoption_bool(autoupdate) = false;
			
			if (answer == 1) {
				DownloadWindow("http://netsurf.baderman.net/Changes.html","RAM:", "Changes.html");
				//DownloadWindow("http://netsurf.baderman.net/EP_LoadModule.rexx","RAM:", "EP_LoadModule.rexx");					
				//DownloadWindow("http://git.netsurf-browser.org/netsurf.git/plain/resources/ca-bundle","RAM:", "ca-bundle");
				//DownloadWindow("http://git.netsurf-browser.org/netsurf.git/plain/resources/default.css","RAM:", "default.css");	
						
			#if defined AGA 
				DownloadWindow("http://netsurf.baderman.net/NetSurf-AGA.zip","RAM:", "NetSurf-AGA.zip");
			#else
				DownloadWindow("http://netsurf.baderman.net/NetSurf.zip","RAM:", "NetSurf.zip");
			#endif

	   #if defined AGA			
			rename("NetSurf-AGA", "NetSurf-AGA-bak");
			Execute("unzip -o ram:NetSurf-AGA.zip -d ram:",0,0);
		
			Execute("protect RAM:NetSurf-AGA +E >NIL:",0,0);
			Execute("copy ram:NetSurf-AGA NetSurf-AGA",0,0);	

		#else		
			rename("NetSurf", "NetSurf-bak");
			Execute("unzip -o ram:NetSurf.zip -d ram:",0,0);

			Execute("protect RAM:NetSurf +E >NIL:",0,0);
			Execute("copy ram:NetSurf NetSurf",0,0);
		#endif	
				
				return 1;
			}
			else  if (answer == 2)
				return -1;
		}
        
    }
    return 0;
}
