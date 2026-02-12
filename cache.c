#define _POSIX_C_SOURCE 200809L

#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void save_to_cache(cJSON* json, char *options[], int count) {
  struct stat st = {0};
  if (stat(".cache", &st) == -1) mkdir(".cache", 0700);

  char cache_path[512];
  snprintf(cache_path, sizeof(cache_path), ".cache/%s.json", options[count]);
  FILE* cache_file = fopen(cache_path, "w");
  if (cache_file) {
    char* json_str = cJSON_Print(json);
    fprintf(cache_file, "%s", json_str);
    free(json_str);
    fclose(cache_file);
  }
  return;
}

cJSON* load_from_cache(char *book_name) {
  char cache_path[512];
  snprintf(cache_path, sizeof(cache_path), ".cache/%s.json", book_name);
  FILE* cache_file = fopen(cache_path, "r");
  if (cache_file) {
    cJSON* json = parse_json(cache_file);
    fclose(cache_file);
    return json;
  }
  return NULL;
}

void delete_old_cache() {
  DIR *dir = opendir(".cache");
  if (!dir) return;

  struct dirent *entry;
  int count = 0;
  char *files[100];

  while ((entry = readdir(dir)) != NULL) {
    char path[512];
    snprintf(path, sizeof(path), ".cache/%s", entry->d_name);

    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
      files[count++] = strdup(entry->d_name);
    }
  }
  closedir(dir);

  if (count > CACHE_SIZE) {
    for (int i = 0; i < count - CACHE_SIZE; i++) {
      char path[512];
      snprintf(path, sizeof(path), ".cache/%s", files[i]);
      remove(path);
    }
  }

  for (int i = 0; i < count; i++)
    free(files[i]);

  return;
}

cJSON* parse_json(FILE* fp) {
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);

  char *buffer = malloc(size + 1);
  fread(buffer, 1, size, fp);
  buffer[size] = '\0';
  fclose(fp);

  cJSON *json = cJSON_Parse(buffer);
  if (json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      printf("Error: %s\n", error_ptr);
    }
    cJSON_Delete(json);
    return NULL;
  }
  return json;
}
