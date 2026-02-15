#include "controller.h"
#include "ui.h"
#include "network.h"
#include "cache.h"
#include "library.h"
#include "webnovel.h"
#include "history.h"

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <ncurses.h>
#include <sys/stat.h>
#include <unistd.h>

void get_user_path(char *dest, const char *subfolder, size_t size) {
  const char *home = getenv("HOME");
  if (!home) home = "."; // Fallback to current dir

  snprintf(dest, size, "%s/.local/share/novel-cli/%s", home, subfolder);

  // Create the directory if it doesn't exist
  mkdir_p(dest); 
}

// Helper to create nested directories (like mkdir -p)
void mkdir_p(const char *path) {
  char tmp[512];
  char *p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);
  if (tmp[len - 1] == '/') tmp[len - 1] = 0;
  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  mkdir(tmp, 0755);
}

FILE* select_main_menu(int choice, char* main_options[], int size_main_options) {
  (void) size_main_options;
  if(choice == 0){
    CURL* handle;
    CURLcode result;
    cJSON* json;
    char book_name[100];

    handle = curl_easy_init();

    echo();
    clear();
    attron(COLOR_PAIR(4));
    mvprintw(0, 0, "Enter book name: ");
    attroff(COLOR_PAIR(4));
    getnstr(book_name, sizeof(book_name) - 1);
    refresh();
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

      if (json) {
        char *name_ptr[1] = { book_name };
        save_to_cache(json, name_ptr, 0); 
      }
      delete_old_cache();
      curl_free(encoded);
    }

    cJSON* count = cJSON_GetObjectItemCaseSensitive(json, "count");
    int count_val = count->valueint;
    int display_count = count_val > 20 ? 20 : count_val;
    cJSON* results = cJSON_GetObjectItemCaseSensitive(json, "results");

    if (count_val == 0) {
      clear();
      attron(COLOR_PAIR(4));
      mvprintw(0, 0, "Search result not found.");
      mvprintw(1, 0, "Press any key to continue...");
      attroff(COLOR_PAIR(4));
      refresh();
      getch();
      cJSON_Delete(json);
      curl_easy_cleanup(handle);
      return NULL;
    }
    cJSON* result_item = results->child;
    char *book_options[display_count];
    for (int i = 0; i < display_count; i++) {
      cJSON* title = cJSON_GetObjectItemCaseSensitive(result_item, "title");
      book_options[i] = title->valuestring;
      result_item = result_item->next;
    }

    while (1) {

      int book_choice = display_menu(book_options, display_count);

      if (book_choice == -1) {
        break;
      }

      FILE* cached_book = in_Library(book_options[book_choice]);

      if (cached_book == NULL) {
        cached_book = download_book(results, book_choice, book_options);
      }

      if (cached_book) {
        display_book(cached_book, book_options[book_choice]);
        fclose(cached_book);
      }
    }

    cJSON_Delete(json);
    curl_easy_cleanup(handle);
    return NULL;
  }
  if(choice == 1){ 
    open_library(); 
    return NULL;
  }
  if(choice == 2){
    search_webnovel(); 
    return NULL;
  }
  if(choice == 3){ 
    show_history_menu();
    return NULL;
  }
  if(choice == 4){ 
    endwin(); 
    exit(0); 
  }
  return NULL;
}
