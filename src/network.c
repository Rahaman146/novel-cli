#include "network.h"
#include "controller.h"
#include "webnovel.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
    // Note: Since we are in ncurses mode, printf might mess up the UI. 
    // Consider a mvprintw here instead.
    curl_easy_cleanup(handle);
    return NULL;
  }

  char* download_url = plain->valuestring;
  
  // FIX STARTS HERE
  char lib_path[512];
  get_user_path(lib_path, "library", sizeof(lib_path)); // This creates the dir if missing

  char filedir[1024];
  snprintf(filedir, sizeof(filedir), "%s/%s.txt", lib_path, options[choice]);
  // FIX ENDS HERE

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
    fclose(download);
    curl_easy_cleanup(handle);
    return NULL;
  }

  fclose(download);
  curl_easy_cleanup(handle);

  return fopen(filedir, "r");
}

char* fetch_chapter_content(const char* chapter_slug)
{
  char url[512];
  snprintf(url, sizeof(url), "https://wuxia.click/chapter/%s", chapter_slug);
  return fetch_url(url);
}

int fetch_novel_chapters(const char* slug, char chapters[3500][128],char chapter_titles[3500][128])
{
  char url[512];
  snprintf(url, sizeof(url), "https://wuxiaworld.eu/api/chapters/%s/", slug);

  char* json_data = fetch_url(url);
  if (!json_data) return 0;

  cJSON* root = cJSON_Parse(json_data);
  if (!root) {
    free(json_data);
    return 0;
  }

  int count = 0;

  if (cJSON_IsArray(root)) {
    int size = cJSON_GetArraySize(root);

    for (int i = 0; i < size && count < 3500; i++) {
      cJSON* item = cJSON_GetArrayItem(root, i);
      cJSON* title_field = cJSON_GetObjectItem(item, "title");

      if (cJSON_IsString(title_field) && title_field->valuestring) {
        strncpy(chapter_titles[count],
                title_field->valuestring,
                255);
        chapter_titles[count][255] = '\0';
      }
      cJSON* slug_field = cJSON_GetObjectItem(item, "novSlugChapSlug");

      if (cJSON_IsString(slug_field) && slug_field->valuestring) {
        strncpy(chapters[count],
                slug_field->valuestring,
                127);

        chapters[count][127] = '\0';
        count++;
      }
    }
  }

  cJSON_Delete(root);
  free(json_data);

  return count;
}
