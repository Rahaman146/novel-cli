#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdio.h>

void open_library(char* options[], int n_options);

FILE* in_Library(char* book_name);

#endif
