#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <cjson/cJSON.h>

size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream);

FILE* download_book(cJSON *results, int choice, char *options[]);

#endif
