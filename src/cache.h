#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <cjson/cJSON.h>

#define CACHE_SIZE 20

typedef struct {
  int page_number;
  int count;

  char titles[12][256];
  char yearly_views[12][256];
  char slugs[12][256];
  char ratings[12][256];
  char chapters[12][256];

  int is_valid;
} PageCache;

void save_to_cache(cJSON* json, char *options[], int count);

cJSON* load_from_cache(char *book_name);

void delete_old_cache(void);

cJSON* parse_json(FILE *fp);

#endif
