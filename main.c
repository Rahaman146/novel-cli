#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <ncurses.h>
#include <locale.h>

#define MAX_LINE 1024

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

int display_menu(char *options[], int n_options) {
  int highlight = 0;
  int choice = -1;
  int c;

  setlocale(LC_ALL, "");
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

    buffer[strcspn(buffer, "\n")] = '\0';   // 🔥 remove newline

    if (count >= capacity) {
      capacity = capacity == 0 ? 100 : capacity * 2;
      lines = realloc(lines, capacity * sizeof(char *));
    }

    lines[count] = strdup(buffer);
    count++;
  }

  fclose(fp);

  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  int offset = 0;
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

  endwin();

  // Free memory
  for (int i = 0; i < count; i++)
    free(lines[i]);
  free(lines);

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
  printf("Downloading Book...\n");
  char filename[512];
  snprintf(filename, sizeof(filename), "%s.txt", options[choice]);
  FILE* download = fopen(filename,"wb");

  curl_easy_setopt(handle, CURLOPT_URL, download_url);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, download);
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

  result = curl_easy_perform(handle);

  fflush(download);
  fclose(download);
  FILE* check = fopen(filename, "r");
  fseek(check, 0, SEEK_END);
  fclose(check);
  FILE* read_book = fopen(filename,"r");
  return read_book;
}

int main() {
  char *options[] = {
    "Search Book",
    "Download Book",
    "Exit"
  };
  int choice = display_menu(options, sizeof(options) / sizeof(char*));

  if (choice == 0) {
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* handle;
    CURLcode result;
    FILE* book;
    char book_name[100];

    handle = curl_easy_init();

    printf("Enter book name: ");
    fgets(book_name, sizeof(book_name), stdin);

    book_name[strcspn(book_name, "\n")] = 0;
    char* encoded = curl_easy_escape(handle, book_name, 0);
    char url[512];
    snprintf(url, sizeof(url),"https://gutendex.com/books/?search=%s",encoded);

    FILE* search_results = fopen("./search_results.json", "w+");
    if (!search_results) {
      printf("Could not open file\n");
      return 1;
    }

    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, search_results);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

    result = curl_easy_perform(handle);

    if (result != CURLE_OK) {
      printf("Curl error: %s\n", curl_easy_strerror(result));
    }

    cJSON* json = parse_json(search_results);
    cJSON* count = cJSON_GetObjectItemCaseSensitive(json, "count");
    int count_val = count->valueint;
    int display_count = count_val > 20 ? 20 : count_val;
    printf("Number of books found: %d\n", count_val);
    cJSON* results = cJSON_GetObjectItemCaseSensitive(json, "results");
    if (!cJSON_IsArray(results)) {
      printf("Search not found\n");
    }
    cJSON* result_item = results->child;

    char *options[count_val];
    for (int i = 0; i < (count_val); i++) {
      cJSON* title = cJSON_GetObjectItemCaseSensitive(result_item, "title");
      options[i] = title->valuestring;
      result_item = result_item->next;
    }

    int choice = display_menu(options, sizeof(options) / sizeof(char*));
    FILE* read_book = download_book(results, choice, options);
    display_book(read_book);

    curl_free(encoded);
    curl_easy_cleanup(handle);
    curl_global_cleanup();
  } 
  else if (choice == 1) {
    printw("Enter book name to download: ");
  }
  else if (choice == 2) {
    endwin();
    return 0;
  }

}
