#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "ui.h"
#include "controller.h"

#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <locale.h>
#include <ctype.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 1024
#define ITEMS_PER_PAGE 12

void truncate_with_ellipsis(char *dest, const char *src, int max_width)
{
  int len = strlen(src);

  if (len <= max_width) {
    strcpy(dest, src);
    return;
  }

  if (max_width <= 3) {
    strncpy(dest, src, max_width);
    dest[max_width] = '\0';
    return;
  }

  strncpy(dest, src, max_width - 3);
  dest[max_width - 3] = '\0';
  strcat(dest, "...");
}

int display_menu(char *options[], int n_options) {
  int highlight = 0;
  int choice = -1;
  int c;

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  char truncated[512];

  while (1) {
    clear();

    for (int i = 0; i < n_options; i++) {

      if (i == highlight) {
        attron(COLOR_PAIR(5));
        mvprintw(i, 0, "▌");
        attroff(COLOR_PAIR(5));

        attron(COLOR_PAIR(5) | A_DIM);
        mvprintw(i, 2, "%d\n", i+1);
        truncate_with_ellipsis(truncated, options[i], cols - 6);
        mvprintw(i, 5, "%s", truncated);
        attroff(COLOR_PAIR(5) | A_DIM);
      } else {
        mvprintw(i, 2, "%d\n", i+1);
        truncate_with_ellipsis(truncated, options[i], cols - 6);
        mvprintw(i, 5, "%s", truncated);
      }
    }
    attron(COLOR_PAIR(4));
    mvprintw(rows - 1, 2, "↑↓ Move   ← Prev Page   q Back   Enter Open");
    attroff(COLOR_PAIR(4));

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

      case KEY_LEFT:
        return -1;
      case 'q':
        return -1;
    }

    if (choice != -1)
      break;
  }

  return choice;
}

int display_webnovel_list(
  char title[][256],
  int count,
  char yearly_views[][256],
  char chapters[][256],
  char ratings[][256],
  char slugs[][256],
  int *current_page)
{
  (void) slugs;
  int highlight = 0;
  int choice = -1;
  int c;

  char search[256] = "";
  int search_len = 0;

  int filtered[50];
  int filtered_count = 0;

  while (1) {

    clear();
    filtered_count = 0;

    for (int i = 0; i < count; i++) {
      if (strcasestr(title[i], search) != NULL) {
        filtered[filtered_count++] = i;
      }
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    attron(COLOR_PAIR(4));
    mvprintw(0, 2, "Search webnovel: %s", search);
    attroff(COLOR_PAIR(4));

    attron(COLOR_PAIR(5));
    mvprintw(0, cols - 15, "Page %d", *current_page);
    attroff(COLOR_PAIR(5));

    attron(COLOR_PAIR(5));
    mvhline(1, 0, ACS_HLINE, cols);
    attroff(COLOR_PAIR(5));

    int row = 2;

    for (int i = 0; i < filtered_count; i++) {

      int idx = filtered[i];

      if (i == highlight) {
        attron(COLOR_PAIR(5));
        mvprintw(row, 0, "▌");
        mvprintw(row, 1, "%d", 12*(*current_page - 1) + i + 1);
        mvprintw(row, 5, "%s (Views:%s | Ch:%s | Rating:%s)", title[idx], yearly_views[idx], chapters[idx], ratings[idx]);
        attroff(COLOR_PAIR(5));
      } else {
        mvprintw(row, 1, "%d", 12*(*current_page - 1) + i + 1);
        mvprintw(row, 5, "%s (Views:%s | Ch:%s | Rating:%s)", title[idx], yearly_views[idx], chapters[idx], ratings[idx]);
      }

      row++;
    }

    attron(COLOR_PAIR(4));
    mvprintw(rows - 1, 2, "← Prev Page   → Next Page   ↑↓ Navigate   Enter Select   ESC Exit");
    attroff(COLOR_PAIR(4));

    move(0, 19 + search_len);
    curs_set(1);

    c = getch();

    if (c == 27) {
      return -2;
    }
    else if (c == KEY_BACKSPACE || c == 127) {
      if (search_len > 0) {
        search[--search_len] = '\0';
      }
    }
    else if (isprint(c) && search_len < 255) {
      search[search_len++] = c;
      search[search_len] = '\0';
    }
    else {

      switch (c) {

        case KEY_UP:
          if (highlight > 0)
            highlight--;
          break;

        case KEY_DOWN:
          if (highlight < filtered_count - 1)
            highlight++;
          break;

        case KEY_RIGHT:
          (*current_page)++;
          return -1;

        case KEY_LEFT:
          if (*current_page > 1) {
            (*current_page)--;
            return -1;
          }
          break;

        case 10:
          if (filtered_count > 0)
            choice = filtered[highlight];
          break;
      }
    }

    if (choice != -1)
      break;
  }

  return choice;
}

int display_book(FILE* fp, const char* book_title) {
  if (!fp) {
    perror("File open failed");
    return -2;
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

  int offset = 0;

  char progress_dir[512];
  get_user_path(progress_dir, "progress", sizeof(progress_dir));

  char progress_path[1024];
  snprintf(progress_path, sizeof(progress_path), "%s/%s.txt", progress_dir, book_title);

  FILE* progress_file = fopen(progress_path, "r");
  // Use progress_file instead of the hardcoded ".progress"
  if (progress_file) {
    fscanf(progress_file, "%d", &offset);
    fclose(progress_file);
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

    attron(COLOR_PAIR(4));
    mvprintw(rows - 1, 0, "<- Main Menu   q = Quit   ↑↓ Scroll   PgUp/PgDn page");
    attroff(COLOR_PAIR(4));

    refresh();

    ch = getch();

    if (ch == KEY_UP && offset > 0)
      offset--;
    else if (ch == KEY_DOWN && offset < count - 1)
      offset++;
    else if (ch == KEY_NPAGE)
      offset += rows;
    else if (ch == KEY_PPAGE)
      offset -= rows;
    else if (ch == 'q' || ch == KEY_LEFT){
      progress_file = fopen(progress_path, "w");
      if (progress_file) {
        fprintf(progress_file, "%d\n", offset);
        fclose(progress_file);
      }

      for (int i = 0; i < count; i++)
        free(lines[i]);
      return -1;
    }

    if (offset < 0)
      offset = 0;

    int max_offset = count - rows + 1;
    if (max_offset < 0)
      max_offset = 0;
    if (offset > max_offset)
      offset = max_offset;
  }

  progress_file = fopen(progress_path, "w");
  if (progress_file) {
    fprintf(progress_file, "%d\n", offset);
    fclose(progress_file);
  }

  for (int i = 0; i < count; i++)
    free(lines[i]);
  free(lines);
  return 0;
}
