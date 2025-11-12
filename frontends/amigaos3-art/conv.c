
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsoption.h"

#ifdef SDLLIB
#include <SDL/SDL_inline.h>
#endif

#include <proto/dos.h>
#include "amigaos3-art/misc.h"

char *pagedst;

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t written = fwrite(ptr, size, nmemb, stream);

  return written;
}

struct string {
  char *ptr;
  size_t len;
};

void init_string(struct string *s);
void init_string2(struct string *s);

char *geturl(char *url);

void init_string2(struct string *s) {
  s->len = 0;
  s->ptr = malloc(s->len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }

	memcpy(s->ptr+s->len, ptr, size*nmemb);
  
  s->ptr[new_len] = '\0';
  s->len = new_len;

  return size*nmemb;
}

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  
  return realsize;
}


#define addurl(x) strcat(conurl, x)

extern int
fb_url_enter(void *pw, char *text);

int convert(char *key, char* url)
{
	int ret = 0;
	char *conurl = malloc(5000);
	CURL *curl;
	url = strdup(curl_easy_escape( curl , url , 0));
	
	strcpy(conurl, "https://api.cloudconvert.com/convert?apikey=");
	addurl(key);
	addurl("&inputformat=mp4&outputformat=mpg&input=download&file=");
	addurl(url);
	addurl("&filename=video.mp4&converteroptions%5Bvideo_codec%5D=MPEG1VIDEO&converteroptions%5Bvideo_bitrate%5D=");
	addurl(nsoption_charp(mpg_bitrate));
	addurl("&converteroptions%5Bvideo_resolution%5D=");
	addurl(nsoption_charp(mpg_width));
	addurl("x");
	addurl(nsoption_charp(mpg_height));	
	addurl("&converteroptions%5Bvideo_ratio%5D=&converteroptions%5Bvideo_fps%5D=");
	addurl(nsoption_charp(mpg_framerate));
	addurl("&converteroptions%5Baudio_codec%5D=");
	addurl(nsoption_charp(mpg_audio_format));
	addurl("&converteroptions%5Baudio_bitrate%5D=");
	addurl(nsoption_charp(mpg_audio_rate));
	addurl("&converteroptions%5Baudio_channels%5D=&converteroptions%5Baudio_frequency%5D=");
	addurl(nsoption_charp(mpg_audio_frequency));
	addurl("&wait=true&download=true");
//	addurl("&converteroptions%5Bcommand%5D=-bf+3+-ac+2");
	/*
	-i {INPUTFILE} -vcodec mpeg1video -filter:v scale=640:360 -b:v 2500k -bf 3 -ac 2 -acodec mp2 -b:a 128k -r 24 {OUTPUTFILE}
	*/
	curl_free(url);
	
	play_mpg = true;
	
	if (nsoption_int(mpg_prebuffer) < 100)
			ret = -1;
		
	//printf("****conurl=%s****\n",conurl);
	
	url_enter(conurl);
	free(conurl);
	
	return ret;
}

char* get_yt_links(char* url)
{
	CURL *curl;
	CURLcode ret = CURLE_OK;
	static const char *pagefilename = "t:page.out";
 
	char *video;
	
	curl = curl_easy_init();	
	
	if (curl) {
		
    curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Chrome/73.0.3683.103");
   /* send all data to this function  */ 	
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);	
 
 //printf("get_yt_links\n");
 
	/* open the file */ 
	FILE *pagefile = fopen(pagefilename, "wb");
	char *final_video;
	
	if (pagefile) {
	 
		/* write the page body to this file handle */ 
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, pagefile);
	 
		/* get it! */
		curl_easy_perform(curl);
		
		fclose(pagefile);
		
		FILE *f=fopen(pagefilename, "r");
		fseek(f, 0, SEEK_END);
		unsigned int fsize = ftell(f);	 
		rewind(f); //same as //fseek(f, 0, SEEK_SET); 
		char *string = malloc(fsize);
		//printf("page.out size =%d\n"	,fsize);
		//int n=0;
		
		fread(string, fsize, fsize - 3, f);          
		
		/* close the header file */ 
		fclose(f);

		//char *ext;
		//printf("page.out=%s\n"	, string);
		
	//	if (nsoption_int(youtube_autoplay) == 1)
		{
			video = strndup(strstr(string,"https://redirector"),2000);
			video = strdup(strstr(video,"https://redirector"));//http://ss.prx"));
				
			video  = strtok((char    *)video,"\"");		
			//video = strdup(str_replace(video," ",""));
			//printf("genvideo1 url=%s\n"	,string);		
			
		/*}
		else
		{
			video = strndup(strstr(string,"MP3 128kbps"),2000);	
			video = strdup(strstr(video,"https://redirector"));		
			
			video  = strtok((char    *)video,"\"");	*/
		}
		

		free(string);		
		free(video);


		}
	
	
	return video;
	}

	return NULL;
}
