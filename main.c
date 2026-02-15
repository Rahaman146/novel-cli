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
  if (has_colors()) {
    start_color();
    use_default_colors();

    init_pair(1, -1, -1);            
    init_pair(2, COLOR_CYAN, -1);     
    init_pair(3, 187, -1);    // light yellow
    init_pair(4, 153, -1);     // light blue
    init_pair(5, 218, -1);     // light pink
    init_pair(6, -1, 237); 
    bkgd(COLOR_PAIR(1)); 
  }

  char *main_options[] = {"Search Book (Gutenberg)", "Open Library", "Search WebNovel", "History", "Exit"};
  int size_main_options = sizeof(main_options) / sizeof(char*);

  while (1) {
    int choice = display_menu(main_options, size_main_options);
    if (choice == -1){
      endwin();
      return 0;
    }
    select_main_menu(choice, main_options, size_main_options);
  }

  endwin();
  curl_global_cleanup();
  return 0;
}
