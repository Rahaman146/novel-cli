#ifndef UI_H
#define UI_H

#include <stdio.h>

#define MAX_LINE 1024

#define MAX_RESULTS 300

int display_webnovel_list(
    char title[][256],
    int count,
    char yearly_views[][256],
    char chapters[][256],
    char ratings[][256],
    char slugs[][256],
    int* current_page
);

int display_menu(char *options[], int n_options);

int display_book(FILE *fp, const char* book_title);

#endif
