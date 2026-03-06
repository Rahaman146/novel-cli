#define _POSIX_C_SOURCE 200809L

#include "history.h"
#include "chapter_controller.h"
#include "network.h"
#include "ui.h"
#include "controller.h"

#include <stdio.h>
#include <string.h>
#include <ncurses.h>
#include <limits.h>

void save_to_history(const char* n_title, const char* c_title,
                     const char* c_slug, const char* n_slug, int c_num) {

  HistoryEntry history[MAX_HISTORY];
  int count = load_history(history);

  int existing_idx = -1;
  for(int i = 0; i < count; i++) {
    if(strcmp(history[i].chapter_slug, c_slug) == 0) {
      existing_idx = i;
      break;
    }
  }

  int move_count = (existing_idx != -1) ? existing_idx :
    (count < MAX_HISTORY ? count : MAX_HISTORY - 1);

  for(int i = move_count; i > 0; i--) {
    history[i] = history[i-1];
  }

  strncpy(history[0].novel_title, n_title, sizeof(history[0].novel_title)-1);
  history[0].novel_title[sizeof(history[0].novel_title)-1] = '\0';

  strncpy(history[0].chapter_title, c_title, sizeof(history[0].chapter_title)-1);
  history[0].chapter_title[sizeof(history[0].chapter_title)-1] = '\0';

  strncpy(history[0].chapter_slug, c_slug, sizeof(history[0].chapter_slug)-1);
  history[0].chapter_slug[sizeof(history[0].chapter_slug)-1] = '\0';

  strncpy(history[0].novel_slug, n_slug, sizeof(history[0].novel_slug)-1);
  history[0].novel_slug[sizeof(history[0].novel_slug)-1] = '\0';

  history[0].chapter_num = c_num;

  if (existing_idx == -1 && count < MAX_HISTORY)
    count++;

  char dir[PATH_MAX];
  get_user_path(dir, "history", sizeof(dir));

  char file[PATH_MAX];
  int n = snprintf(file, sizeof(file), "%s/history.bin", dir);
  if (n < 0 || n >= (int)sizeof(file)) return;

  FILE* f = fopen(file, "wb");

  if(f) {
    fwrite(&count, sizeof(int), 1, f);
    fwrite(history, sizeof(HistoryEntry), count, f);
    fclose(f);
  }
}

int load_history(HistoryEntry history[MAX_HISTORY]) {

  char dir[PATH_MAX];
  get_user_path(dir, "history", sizeof(dir));

  char file[PATH_MAX];
  int n = snprintf(file, sizeof(file), "%s/history.bin", dir);
  if (n < 0 || n >= (int)sizeof(file)) return 0;

  FILE* f = fopen(file, "rb");

  if(!f) return 0;

  int count = 0;

  if (fread(&count, sizeof(int), 1, f) != 1) {
    fclose(f);
    return 0;
  }

  if (count > MAX_HISTORY)
    count = MAX_HISTORY;

  fread(history, sizeof(HistoryEntry), count, f);
  fclose(f);

  return count;
}

void show_history_menu() {
  HistoryEntry history[MAX_HISTORY];
  int count = load_history(history);

  if (count == 0) {
    clear();
    mvprintw(0, 0, "No history found. Press any key...");
    getch();
    return;
  }

  char* options[MAX_HISTORY];
  char display_strings[MAX_HISTORY][512];
  for(int i = 0; i < count; i++) {
    snprintf(display_strings[i], sizeof(display_strings[i]),
             "[%.200s] %.200s",
             history[i].novel_title,
             history[i].chapter_title);
    options[i] = display_strings[i];
  }

  int choice = display_menu(options, count); // Using your existing menu UI
  if (choice >= 0) {
    char chapters[3500][128] = {0};
    char titles[3500][128] = {0};

    // Fetch full list for the selected novel to support Prev/Next navigation
    int total = fetch_novel_chapters(history[choice].novel_slug, chapters, titles);

    int start_idx = history[choice].chapter_num - 1;
    // Jump straight to the browser at the correct chapter
    show_chapter_browser(chapters, titles, total, history[choice].novel_title, history[choice].novel_slug, start_idx);
  }
}
