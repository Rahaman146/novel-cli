#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <ncurses.h>
#include <locale.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 1024
#define CACHE_SIZE 20

struct Memory {
  char *data;
  size_t size;
};

cJSON* parse_json(FILE* fp) {
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);

  char *buffer = malloc(size + 1);
  fread(buffer, 1, size, fp);
  buffer[size] = '\0';
  fclose(fp);

  cJSON *json = cJSON_Parse(buffer);
  if (json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      printf("Error: %s\n", error_ptr);
    }
    cJSON_Delete(json);
    return NULL;
  }
  return json;
}

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

int display_menu(char *options[], int n_options) {
  int highlight = 0;
  int choice = -1;
  int c;

  initscr();
  noecho();
  cbreak();
  keypad(stdscr, TRUE);

  while (1) {
    clear();

    for (int i = 0; i < n_options; i++) {
      if (i == highlight) {
        attron(A_REVERSE);
        printw("%s\n", options[i]);
        attroff(A_REVERSE);
      } else {
        printw("%s\n", options[i]);
      }
    }

    c = getch();

    switch (c) {
      case KEY_UP:
        highlight--;
        if (highlight < 0)
          highlight = n_options - 1;
        break;

      case KEY_DOWN:
        highlight++;
        if (highlight >= n_options)
          highlight = 0;
        break;

      case 10:  // Enter key
        choice = highlight;
        break;
    }

    if (choice != -1)
      break;
  }

  clear();
  endwin();
  return choice;
}

void display_book(FILE* fp) {
  if (!fp) {
    perror("File open failed");
    return;
  }

  char **lines = NULL;
  int capacity = 0;
  int count = 0;

  char buffer[MAX_LINE];

  while (fgets(buffer, MAX_LINE, fp)) {

    buffer[strcspn(buffer, "\n")] = '\0';

    if (count >= capacity) {
      capacity = capacity == 0 ? 100 : capacity * 2;
      lines = realloc(lines, capacity * sizeof(char *));
    }

    lines[count] = strdup(buffer);
    count++;
  }

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  int offset = 0;

  FILE *progress = fopen(".progress", "r");
  if (progress) {
    fscanf(progress, "%d", &offset);
    fclose(progress);
  }
  int ch;

  while (1) {
    clear();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int screen_row = 0;

    for (int i = offset; i < count && screen_row < rows - 1; i++) {

      char *line = lines[i];
      int len = strlen(line);
      int start = 0;

      while (start < len && screen_row < rows - 1) {
        mvprintw(screen_row, 0, "%.*s", cols - 1, line + start);
        start += cols - 1;
        screen_row++;
      }
    }

    mvprintw(rows - 1, 0, "q = quit | ↑↓ scroll | PgUp/PgDn page");

    refresh();

    ch = getch();

    if (ch == 'q')
      break;
    else if (ch == KEY_UP && offset > 0)
      offset--;
    else if (ch == KEY_DOWN && offset < count - 1)
      offset++;
    else if (ch == KEY_NPAGE)   // Page Down
      offset += rows;
    else if (ch == KEY_PPAGE)   // Page Up
      offset -= rows;

    if (offset < 0)
      offset = 0;
    if (offset > count - rows + 1)
      offset = count - rows + 1;
  }

  progress = fopen(".progress", "w");
  if (progress) {
    fprintf(progress, "%d\n", offset);
    fclose(progress);
  }
  endwin();

  // Free memory
  for (int i = 0; i < count; i++)
    free(lines[i]);

  return;
}

void open_library() {
  DIR *dir = opendir("library");
  if (!dir) {
    printf("Library folder not found.\n");
    return;
  }

  struct dirent *entry;
  char *books[200];
  int count = 0;

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG) {
      books[count++] = strdup(entry->d_name);
    }
  }

  closedir(dir);

  if (count == 0) {
    printf("Library is empty.\n");
    return;
  }

  int choice = display_menu(books, count);

  char path[512];
  snprintf(path, sizeof(path), "./library/%s", books[choice]);

  FILE *fp = fopen(path, "r");
  display_book(fp);

  for (int i = 0; i < count; i++) free(books[i]);
}

FILE* in_Library(char *book_name) {
  char path[512];
  snprintf(path, sizeof(path), "./library/%s.txt", book_name);
  FILE* fp = fopen(path,"r");
  return fp;
}

void save_to_cache(cJSON* json, char *options[], int count) {
  struct stat st = {0};
  if (stat(".cache", &st) == -1) mkdir(".cache", 0700);

  char cache_path[512];
  snprintf(cache_path, sizeof(cache_path), ".cache/%s.json", options[count]);
  FILE* cache_file = fopen(cache_path, "w");
  if (cache_file) {
    char* json_str = cJSON_Print(json);
    fprintf(cache_file, "%s", json_str);
    free(json_str);
    fclose(cache_file);
  }
  return;
}

cJSON* load_from_cache(char *book_name) {
  char cache_path[512];
  snprintf(cache_path, sizeof(cache_path), ".cache/%s.json", book_name);
  FILE* cache_file = fopen(cache_path, "r");
  if (cache_file) {
    cJSON* json = parse_json(cache_file);
    fclose(cache_file);
    return json;
  }
  return NULL;
}

void delete_old_cache() {
  DIR *dir = opendir(".cache");
  if (!dir) return;

  struct dirent *entry;
  int count = 0;
  char *files[100];

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG) {
      files[count++] = strdup(entry->d_name);
    }
  }
  closedir(dir);

  if (count > CACHE_SIZE) {
    for (int i = 0; i < count - CACHE_SIZE; i++) {
      char path[512];
      snprintf(path, sizeof(path), ".cache/%s", files[i]);
      remove(path);
    }
  }

  for (int i = 0; i < count; i++)
    free(files[i]);

  return;
}

FILE* download_book(cJSON* results, int choice, char *options[]) {
  CURL* handle = curl_easy_init();
  CURLcode result;
  cJSON* formats = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(results, choice), "formats");
  cJSON* plain = cJSON_GetObjectItemCaseSensitive(formats, "text/plain; charset=utf-8");
  if (!plain) {
    printf("Plain text not available\n");
    return NULL;
  }
  char* download_url = cJSON_GetObjectItemCaseSensitive(formats, "text/plain; charset=utf-8")->valuestring;
  char filedir[512];
  snprintf(filedir, sizeof(filedir), "./library/%s.txt", options[choice]);
  FILE* download = fopen(filedir,"wb");

  curl_easy_setopt(handle, CURLOPT_URL, download_url);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, download);
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

  result = curl_easy_perform(handle);

  fflush(download);
  fclose(download);
  FILE* read_book = fopen(filedir,"r");
  return read_book;
}

int main() {
  setlocale(LC_ALL, "");
  char *options[] = {
    "Search Book",
    "Open Library",
    "Exit"
  };
  int choice = display_menu(options, sizeof(options) / sizeof(char*));

  if (choice == 0) {
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* handle;
    CURLcode result;
    cJSON* json;
    char book_name[100];

    handle = curl_easy_init();

    printf("Enter book name: ");
    fgets(book_name, sizeof(book_name), stdin);

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
      if (json) save_to_cache(json, options, choice);
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

    char *options[display_count];
    for (int i = 0; i < display_count; i++) {
      cJSON* title = cJSON_GetObjectItemCaseSensitive(result_item, "title");
      options[i] = title->valuestring;
      result_item = result_item->next;
    }

    int choice = display_menu(options, display_count);
    FILE* cached_book = in_Library(options[choice]);

    if (cached_book != NULL) {
      display_book(cached_book);
    } else {
      FILE* read_book = download_book(results, choice, options);
      display_book(read_book);
      if (read_book) fclose(read_book);
    }

    if (cached_book) fclose(cached_book);
    cJSON_Delete(json);
    curl_easy_cleanup(handle);
    curl_global_cleanup();
  } 
  else if (choice == 1) {
    open_library();
  }
  else if (choice == 2) {
    endwin();
    return 0;
  }

}
