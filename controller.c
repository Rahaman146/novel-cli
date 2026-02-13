#include "controller.h"
#include "ui.h"
#include "network.h"
#include "cache.h"
#include "library.h"
#include "webnovel.h"

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <ncurses.h>

FILE* select_main_menu(int choice, char* main_options[], int size_main_options) {
  if(choice == 0){
    CURL* handle;
    CURLcode result;
    cJSON* json;
    char book_name[100];

    handle = curl_easy_init();

    echo();
    clear();
    mvprintw(0, 0, "Enter book name: ");
    refresh();
    getnstr(book_name, sizeof(book_name) - 1);
    noecho();

    book_name[strcspn(book_name, "\n")] = 0;

    json = load_from_cache(book_name);
    if (!json) {
      char* encoded = curl_easy_escape(handle, book_name, 0);
      char url[512];
      snprintf(url, sizeof(url),"https://gutendex.com/books/?search=%s",encoded);

      struct Memory chunk;
      chunk.data = malloc(1);
      chunk.size = 0;

      curl_easy_setopt(handle, CURLOPT_URL, url);
      curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
      curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&chunk);

      curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

      result = curl_easy_perform(handle);

      if (result != CURLE_OK) {
        printf("Curl error: %s\n", curl_easy_strerror(result));
      }

      json = cJSON_Parse(chunk.data);
      free(chunk.data);
      if (json) save_to_cache(json, main_options, choice);
      delete_old_cache();
      curl_free(encoded);
    }

    cJSON* count = cJSON_GetObjectItemCaseSensitive(json, "count");
    int count_val = count->valueint;
    int display_count = count_val > 20 ? 20 : count_val;
    cJSON* results = cJSON_GetObjectItemCaseSensitive(json, "results");
    if (!cJSON_IsArray(results)) {
      printf("Search not found\n");
    }

    cJSON* result_item = results->child;
    char *book_options[display_count];
    for (int i = 0; i < display_count; i++) {
      cJSON* title = cJSON_GetObjectItemCaseSensitive(result_item, "title");
      book_options[i] = title->valuestring;
      result_item = result_item->next;
    }

    int book_choice = display_menu(book_options, display_count);
    if (book_choice == -1){
      return NULL;
    }
    FILE* cached_book = in_Library(book_options[book_choice]);

    if(cached_book == NULL) {
      cached_book = download_book(results, book_choice, book_options);
    }

    cJSON_Delete(json);
    curl_easy_cleanup(handle);
    return cached_book;
  }
  if(choice == 1){ 
    open_library(main_options, size_main_options); 
    return NULL;
  }
  if(choice == 2){
    search_webnovel(); 
    return NULL;
  }
  if(choice == 3){ 
    endwin(); 
    exit(0); 
  }
  return NULL;
}
