#include "network.h"
#include "controller.h"
#include "webnovel.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct Memory *mem = (struct Memory *)userp;

  char *ptr = realloc(mem->data, mem->size + realsize + 1);
  if(ptr == NULL)
    return 0;

  mem->data = ptr;
  memcpy(&(mem->data[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->data[mem->size] = 0;

  return realsize;
}

FILE* download_book(cJSON* results, int choice, char *options[])
{
  CURL* handle = curl_easy_init();
  cJSON* formats = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(results, choice), "formats");
  cJSON* plain = cJSON_GetObjectItemCaseSensitive(formats, "text/plain; charset=utf-8");

  if (!plain) {
    // Note: Since we are in ncurses mode, printf might mess up the UI. 
    // Consider a mvprintw here instead.
    curl_easy_cleanup(handle);
    return NULL;
  }

  char* download_url = plain->valuestring;
  
  // FIX STARTS HERE
  char lib_path[512];
  get_user_path(lib_path, "library", sizeof(lib_path)); // This creates the dir if missing

  char filedir[1024];
  snprintf(filedir, sizeof(filedir), "%s/%s.txt", lib_path, options[choice]);
  // FIX ENDS HERE

  FILE* download = fopen(filedir, "wb");
  if (!download) {
    curl_easy_cleanup(handle);
    return NULL;
  }

  curl_easy_setopt(handle, CURLOPT_URL, download_url);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, download);
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode result = curl_easy_perform(handle);

  if (result != CURLE_OK) {
    fclose(download);
    curl_easy_cleanup(handle);
    return NULL;
  }

  fclose(download);
  curl_easy_cleanup(handle);

  return fopen(filedir, "r");
}

static int fetch_freewebnovel_chapters(const char* slug, char chapters[3500][128], char chapter_titles[3500][128]) {
  const char *raw_slug = slug;
  if (strncmp(raw_slug, "freewebnovel:", 13) == 0) raw_slug += 13;
  if (strncmp(raw_slug, "/novel/", 7) == 0) raw_slug += 7;

  int count = 0;
  int current_page = 1;
  int total_page = 1;

  do {
    char url[512];
    snprintf(url, sizeof(url), "https://freewebnovel.com/novel/%s?ajax=chapters&page=%d&pageSize=5000", raw_slug, current_page);

    char *json_data = fetch_url(url);
    if (!json_data) break;

    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
      free(json_data);
      break;
    }

    cJSON *tp_item = cJSON_GetObjectItemCaseSensitive(root, "totalPage");
    if (tp_item && cJSON_IsNumber(tp_item)) {
      total_page = tp_item->valueint;
    }

    cJSON *html_item = cJSON_GetObjectItemCaseSensitive(root, "html");
    if (html_item && cJSON_IsString(html_item) && html_item->valuestring) {
      char *p = html_item->valuestring;
      while (count < 3500) {
        char *a_pos = strstr(p, "href=");
        if (!a_pos) break;
        a_pos += 5;
        while (*a_pos == '\\' || *a_pos == '"' || *a_pos == '\'') a_pos++;

        char *a_end = a_pos;
        while (*a_end && *a_end != '"' && *a_end != '\\' && *a_end != '\'' && *a_end != '>') a_end++;

        int href_len = a_end - a_pos;
        if (href_len > 0 && href_len < 120) {
          char href[128];
          strncpy(href, a_pos, href_len);
          href[href_len] = '\0';

          char clean_href[128] = {0};
          int cidx = 0;
          for (int k = 0; href[k]; k++) {
            if (href[k] != '\\') clean_href[cidx++] = href[k];
          }
          clean_href[cidx] = '\0';

          snprintf(chapters[count], 128, "freewebnovel:%.100s", clean_href);
        }

        char *title_attr = strstr(a_end, "title=\"");
        if (title_attr && title_attr - a_end < 50) {
          title_attr += 7;
          char *title_end = strchr(title_attr, '"');
          if (title_end && title_end - title_attr < 120) {
            int tlen = title_end - title_attr;
            strncpy(chapter_titles[count], title_attr, tlen);
            chapter_titles[count][tlen] = '\0';
          }
        } else {
          char *gt = strchr(a_end, '>');
          if (gt) {
            gt++;
            char *lt = strchr(gt, '<');
            if (lt && lt - gt < 120) {
              int tlen = lt - gt;
              strncpy(chapter_titles[count], gt, tlen);
              chapter_titles[count][tlen] = '\0';
            }
          }
        }

        count++;
        p = a_end + 1;
      }
    }

    cJSON_Delete(root);
    free(json_data);
    current_page++;
  } while (current_page <= total_page && count < 3500);

  return count;
}

static int fetch_royalroad_chapters(const char* slug, char chapters[3500][128], char chapter_titles[3500][128]) {
  const char *raw_slug = slug;
  if (strncmp(raw_slug, "royalroad:", 10) == 0) raw_slug += 10;

  char url[512];
  if (raw_slug[0] == '/') {
    snprintf(url, sizeof(url), "https://www.royalroad.com%s", raw_slug);
  } else {
    snprintf(url, sizeof(url), "https://www.royalroad.com/%s", raw_slug);
  }

  char *html = fetch_url(url);
  if (!html) return 0;

  int count = 0;

  char *window_ch = strstr(html, "window.chapters =");
  if (window_ch) {
    char *arr_start = strchr(window_ch, '[');
    if (arr_start) {
      char *arr_end = strstr(arr_start, "];");
      if (arr_end) {
        int json_len = arr_end - arr_start + 1;
        char *json_buf = malloc(json_len + 1);
        if (json_buf) {
          strncpy(json_buf, arr_start, json_len);
          json_buf[json_len] = '\0';

          cJSON *root = cJSON_Parse(json_buf);
          if (root && cJSON_IsArray(root)) {
            int size = cJSON_GetArraySize(root);
            for (int i = 0; i < size && count < 3500; i++) {
              cJSON *item = cJSON_GetArrayItem(root, i);
              cJSON *t_field = cJSON_GetObjectItemCaseSensitive(item, "title");
              cJSON *u_field = cJSON_GetObjectItemCaseSensitive(item, "url");

              if (t_field && cJSON_IsString(t_field) && t_field->valuestring) {
                strncpy(chapter_titles[count], t_field->valuestring, 127);
                chapter_titles[count][127] = '\0';
              }
              if (u_field && cJSON_IsString(u_field) && u_field->valuestring) {
                snprintf(chapters[count], 128, "royalroad:%.100s", u_field->valuestring);
                count++;
              }
            }
          }
          if (root) cJSON_Delete(root);
          free(json_buf);
        }
      }
    }
  }

  if (count == 0) {
    char *p = html;
    while (count < 3500) {
      char *chap_a = strstr(p, "href=\"/fiction/");
      if (!chap_a) break;
      char *c_url = strstr(chap_a, "/chapter/");
      if (!c_url || c_url - chap_a > 100) {
        p = chap_a + 6;
        continue;
      }

      char *url_start = chap_a + 6;
      char *url_end = strchr(url_start, '"');
      if (!url_end) break;

      int ulen = url_end - url_start;
      if (ulen > 0 && ulen < 120) {
        char cur_url[128];
        strncpy(cur_url, url_start, ulen);
        cur_url[ulen] = '\0';
        snprintf(chapters[count], 128, "royalroad:%.100s", cur_url);
      }

      char *gt = strchr(url_end, '>');
      if (gt) {
        gt++;
        char *lt = strchr(gt, '<');
        if (lt && lt - gt < 120) {
          int tlen = lt - gt;
          strncpy(chapter_titles[count], gt, tlen);
          chapter_titles[count][tlen] = '\0';
        }
      }

      count++;
      p = url_end + 1;
    }
  }

  free(html);
  return count;
}

static int fetch_wuxia_chapters(const char* slug, char chapters[3500][128], char chapter_titles[3500][128]) {
  const char *raw_slug = slug;
  if (strncmp(raw_slug, "wuxia:", 6) == 0) raw_slug += 6;
  if (strncmp(raw_slug, "/novel/", 7) == 0) raw_slug += 7;

  int count = 0;
  int current_page = 1;

  while (count < 3500 && current_page <= 50) {
    char url[512];
    snprintf(url, sizeof(url), "https://wuxia.click/novel/%s?page=%d", raw_slug, current_page);

    char *html = fetch_url(url);
    if (!html) break;

    int page_added = 0;
    char *script_start = strstr(html, "<script id=\"__NEXT_DATA__\" type=\"application/json\">");
    if (script_start) {
      script_start += strlen("<script id=\"__NEXT_DATA__\" type=\"application/json\">");
      char *script_end = strstr(script_start, "</script>");
      if (script_end) {
        int json_len = script_end - script_start;
        char *json_buf = malloc(json_len + 1);
        if (json_buf) {
          strncpy(json_buf, script_start, json_len);
          json_buf[json_len] = '\0';

          cJSON *root = cJSON_Parse(json_buf);
          if (root) {
            cJSON *props = cJSON_GetObjectItemCaseSensitive(root, "props");
            cJSON *pageProps = cJSON_GetObjectItemCaseSensitive(props, "pageProps");
            cJSON *toc = cJSON_GetObjectItemCaseSensitive(pageProps, "toc");
            cJSON *items = toc ? cJSON_GetObjectItemCaseSensitive(toc, "items") : NULL;

            if (items && cJSON_IsArray(items)) {
              int size = cJSON_GetArraySize(items);
              for (int i = 0; i < size && count < 3500; i++) {
                cJSON *item = cJSON_GetArrayItem(items, i);
                cJSON *num_item = cJSON_GetObjectItemCaseSensitive(item, "number");
                cJSON *title_item = cJSON_GetObjectItemCaseSensitive(item, "title");

                if (title_item && cJSON_IsString(title_item) && title_item->valuestring) {
                  strncpy(chapter_titles[count], title_item->valuestring, 127);
                  chapter_titles[count][127] = '\0';
                } else {
                  snprintf(chapter_titles[count], 128, "Chapter %d", count + 1);
                }

                if (num_item && cJSON_IsNumber(num_item)) {
                  if (num_item->valuedouble == (double)num_item->valueint) {
                    snprintf(chapters[count], 128, "%s-%d", raw_slug, num_item->valueint);
                  } else {
                    snprintf(chapters[count], 128, "%s-%.2f", raw_slug, num_item->valuedouble);
                  }
                } else if (num_item && cJSON_IsString(num_item) && num_item->valuestring) {
                  snprintf(chapters[count], 128, "%s-%s", raw_slug, num_item->valuestring);
                } else {
                  snprintf(chapters[count], 128, "%s-%d", raw_slug, count + 1);
                }

                count++;
                page_added++;
              }
            }
            cJSON_Delete(root);
          }
          free(json_buf);
        }
      }
    }

    free(html);
    if (page_added == 0) break;
    current_page++;
  }

  return count;
}

char* fetch_chapter_content(const char* chapter_slug)
{
  if (strncmp(chapter_slug, "freewebnovel:", 13) == 0 || strncmp(chapter_slug, "/novel/", 7) == 0) {
    const char *path = chapter_slug;
    if (strncmp(path, "freewebnovel:", 13) == 0) path += 13;

    char url[512];
    if (path[0] == '/') {
      snprintf(url, sizeof(url), "https://freewebnovel.com%s", path);
    } else {
      snprintf(url, sizeof(url), "https://freewebnovel.com/%s", path);
    }
    return fetch_url(url);
  } else if (strncmp(chapter_slug, "royalroad:", 10) == 0 || strncmp(chapter_slug, "/fiction/", 9) == 0) {
    const char *path = chapter_slug;
    if (strncmp(path, "royalroad:", 10) == 0) path += 10;

    char url[512];
    if (path[0] == '/') {
      snprintf(url, sizeof(url), "https://www.royalroad.com%s", path);
    } else {
      snprintf(url, sizeof(url), "https://www.royalroad.com/%s", path);
    }
    return fetch_url(url);
  } else {
    char url[512];
    if (strncmp(chapter_slug, "http://", 7) == 0 || strncmp(chapter_slug, "https://", 8) == 0) {
      snprintf(url, sizeof(url), "%s", chapter_slug);
    } else {
      snprintf(url, sizeof(url), "https://wuxia.click/chapter/%s", chapter_slug);
    }
    return fetch_url(url);
  }
}

int fetch_novel_chapters(const char* slug, char chapters[3500][128],char chapter_titles[3500][128])
{
  if (strncmp(slug, "freewebnovel:", 13) == 0 || strncmp(slug, "/novel/", 7) == 0) {
    return fetch_freewebnovel_chapters(slug, chapters, chapter_titles);
  } else if (strncmp(slug, "royalroad:", 10) == 0 || strncmp(slug, "/fiction/", 9) == 0) {
    return fetch_royalroad_chapters(slug, chapters, chapter_titles);
  } else {
    return fetch_wuxia_chapters(slug, chapters, chapter_titles);
  }
}

