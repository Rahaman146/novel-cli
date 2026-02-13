#define _XOPEN_SOURCE_EXTENDED 1
#include <locale.h>

#include "ui.h"
#include "cache.h"
#include "controller.h"
#include "network.h"
#include "library.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <ncurses.h>
#include <locale.h>
#include <dirent.h>
#include <sys/stat.h>

int main() {
  setlocale(LC_ALL, "");
  curl_global_init(CURL_GLOBAL_ALL);

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  char *main_options[] = {"Search Book (Gutenberg)", "Open Library", "Search WebNovel", "Exit"};
  int size_main_options = sizeof(main_options) / sizeof(char*);

  while (1) {
    int choice = display_menu(main_options, size_main_options);
    FILE* cached_book = select_main_menu(choice, main_options, size_main_options);

    if (cached_book) {
      display_book(cached_book);
      fclose(cached_book);
    }
  }

  endwin();
  curl_global_cleanup();
  return 0;
}
