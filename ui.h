#ifndef UI_H
#define UI_H

#include <stdio.h>

#define MAX_LINE 1024

int display_menu(char *options[], int n_options);

int display_webnovel_list(char title[10][256], int count, char yearly_views[10][256], char chapters[10][256], char ratings[10][256], char slugs[10][256]);

int display_book(FILE *fp);

#endif
