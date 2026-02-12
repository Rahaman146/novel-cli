#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <cjson/cJSON.h>

#define CACHE_SIZE 20

void save_to_cache(cJSON* json, char *options[], int count);

cJSON* load_from_cache(char *book_name);

void delete_old_cache(void);

cJSON* parse_json(FILE *fp);

#endif
