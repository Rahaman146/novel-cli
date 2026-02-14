#define _POSIX_C_SOURCE 200809L

#include "library.h"
#include "ui.h"
#include "controller.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    char path[512];
    snprintf(path, sizeof(path), "./library/%s", entry->d_name);

    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
      books[count++] = strdup(entry->d_name);
    }
  }

  closedir(dir);

  if (count == 0) {
    printf("Library is empty.\n");
    return;
  }

  while (1) {
    int choice = display_menu(books, count);

    if (choice == -1) {
      break;
    }

    char path[512];
    snprintf(path, sizeof(path), "./library/%s", books[choice]);

    FILE *fp = fopen(path, "r");
    if (fp) {
      display_book(fp);
      fclose(fp);
    }
  }

  for (int i = 0; i < count; i++)
    free(books[i]);
}

FILE* in_Library(char *book_name) {
  char path[512];
  snprintf(path, sizeof(path), "./library/%s.txt", book_name);
  FILE* fp = fopen(path,"r");
  return fp;
}
