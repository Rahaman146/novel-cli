#define _POSIX_C_SOURCE 200809L

#include "ui.h"
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <locale.h>

#define MAX_LINE 1024

int display_menu(char *options[], int n_options) {
  int highlight = 0;
  int choice = -1;
  int c;

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

  return choice;
}

int display_book(FILE* fp) {
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

    mvprintw(rows - 1, 0, "<- = main menu | q = quit | ↑↓ scroll | PgUp/PgDn page");

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
    else if (ch == KEY_LEFT){
      progress = fopen(".progress", "w");
      if (progress) {
        fprintf(progress, "%d\n", offset);
        fclose(progress);
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

  progress = fopen(".progress", "w");
  if (progress) {
    fprintf(progress, "%d\n", offset);
    fclose(progress);
  }

  for (int i = 0; i < count; i++)
    free(lines[i]);
  free(lines);
  return 0;
}
