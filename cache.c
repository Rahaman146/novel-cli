#define _POSIX_C_SOURCE 200809L

#include "cache.h"
#include "controller.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void save_to_cache(cJSON* json, char *options[], int count) {
  char cache_dir[512];
  get_user_path(cache_dir, "cache", sizeof(cache_dir));

  char cache_path[1024];
  snprintf(cache_path, sizeof(cache_path), "%s/%s.json", cache_dir, options[count]);

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
    char cache_dir[512];
    get_user_path(cache_dir, "cache", sizeof(cache_dir));

    char cache_path[1024];
    snprintf(cache_path, sizeof(cache_path), "%s/%s.json", cache_dir, book_name);
    
    FILE* cache_file = fopen(cache_path, "r");
    if (cache_file) {
        cJSON* json = parse_json(cache_file); // parse_json handles fclose
        return json;
    }
    return NULL;
}

void delete_old_cache() {
    char cache_dir[512];
    get_user_path(cache_dir, "cache", sizeof(cache_dir)); // Get the real XDG path

    DIR *dir = opendir(cache_dir);
    if (!dir) return;

    struct dirent *entry;
    int count = 0;
    char *files[100];

    while ((entry = readdir(dir)) != NULL && count < 100) {
        if (entry->d_name[0] == '.') continue; // Skip . and ..

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", cache_dir, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            files[count++] = strdup(entry->d_name);
        }
    }
    closedir(dir);

    if (count > CACHE_SIZE) {
        for (int i = 0; i < count - CACHE_SIZE; i++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", cache_dir, files[i]);
            remove(path);
        }
    }

    for (int i = 0; i < count; i++)
        free(files[i]);
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
