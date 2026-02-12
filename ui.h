#ifndef UI_H
#define UI_H

#include <stdio.h>

#define MAX_LINE 1024

int display_menu(char *options[], int n_options);

int display_book(FILE *fp);

#endif
