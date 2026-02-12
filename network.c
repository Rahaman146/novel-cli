#include "network.h"
#include "controller.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct Memory *mem = (struct Memory *)userp;

  char *ptr = realloc(mem->data, mem->size + realsize + 1);
  if(ptr == NULL)
    return 0;

  mem->data = ptr;
  memcpy(&(mem->data[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->data[mem->size] = 0;

  return realsize;
}

FILE* download_book(cJSON* results, int choice, char *options[])
{
    CURL* handle = curl_easy_init();
    cJSON* formats = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(results, choice), "formats");
    cJSON* plain = cJSON_GetObjectItemCaseSensitive(formats, "text/plain; charset=utf-8");

    if (!plain) {
        printf("Plain text not available\n");
        curl_easy_cleanup(handle);
        return NULL;
    }
    char* download_url = plain->valuestring;
    char filedir[512];
    snprintf(filedir, sizeof(filedir), "./library/%s.txt", options[choice]);

    FILE* download = fopen(filedir, "wb");
    if (!download) {
        curl_easy_cleanup(handle);
        return NULL;
    }

    curl_easy_setopt(handle, CURLOPT_URL, download_url);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, download);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode result = curl_easy_perform(handle);

    if (result != CURLE_OK) {
        fprintf(stderr, "Download failed: %s\n",
                curl_easy_strerror(result));
        fclose(download);
        curl_easy_cleanup(handle);
        return NULL;
    }

    fclose(download);
    curl_easy_cleanup(handle);

    return fopen(filedir, "r");
}
