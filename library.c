#define _POSIX_C_SOURCE 200809L

#include "library.h"
#include "ui.h"
#include "controller.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ncurses.h>
#include <limits.h>

void open_library() {
    char lib_path[PATH_MAX];
    get_user_path(lib_path, "library", sizeof(lib_path));

    DIR *dir = opendir(lib_path);
    if (!dir) {
        // Ncurses-safe error message
        clear();
        mvprintw(0, 0, "Library folder could not be accessed.");
        mvprintw(1, 0, "Press any key to return...");
        getch();
        return;
    }

    struct dirent *entry;
    char *books[200];
    int count = 0;

    while ((entry = readdir(dir)) != NULL && count < 200) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", lib_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            books[count++] = strdup(entry->d_name);
        }
    }
    closedir(dir);

    if (count == 0) {
        clear();
        attron(COLOR_PAIR(4));
        mvprintw(0, 0, "Library is empty.");
        mvprintw(1, 0, "Press any key to return...");
        attroff(COLOR_PAIR(4));
        refresh();
        getch();
        return;
    }

    while (1) {
        int choice = display_menu(books, count);
        if (choice == -1) break;

        char path[PATH_MAX]; // Changed from 512 to PATH_MAX
        snprintf(path, sizeof(path), "%s/%s", lib_path, books[choice]);

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
    char lib_path[512];
    get_user_path(lib_path, "library", sizeof(lib_path));
    
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s.txt", lib_path, book_name);
    return fopen(full_path, "r");
}
