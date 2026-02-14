#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdio.h>

struct Memory {
  char *data;
  size_t size;
};

FILE* select_main_menu(int choice, char* main_options[], int num_options); FILE* select_sub_menu(int choice, char* sub_options[], int main_options);

void get_user_path(char *dest, const char *subfolder, size_t size);

void mkdir_p(const char *path);

#endif
