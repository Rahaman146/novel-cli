#define _POSIX_C_SOURCE 200809L

#include "webnovel.h"
#include "network.h"
#include "ui.h"
#include "cache.h"
#include "chapter_controller.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>

#define MAX_RESULTS 300
#define MAX_PAGES 50
PageCache page_cache[MAX_PAGES];

struct Memory {
  char *data;
  size_t size;
};

PageCache* get_cached_page(int page)
{
  for (int i = 0; i < MAX_PAGES; i++) {
    if (page_cache[i].is_valid &&
      page_cache[i].page_number == page)
      return &page_cache[i];
  }
  return NULL;
}

void clear_cache()
{
  for (int i = 0; i < MAX_PAGES; i++) {
    page_cache[i].is_valid = 0;
    page_cache[i].page_number = -1;
    page_cache[i].count = 0;
  }
}

int extract_novel_info(char *html, char titles[12][256], char yearly_views[12][256], char chapters[12][256], char ratings[12][256], char slugs[12][256]) {
  int count = 0;

  char *card_pos = html;
  while (count < 15) {
    char *slug_pos = strstr(card_pos, "/novel/");
    if (!slug_pos) break;
    if (slug_pos) {
      slug_pos += 7;
      char *slug_end = strchr(slug_pos, '"');
      if (slug_end && slug_end - slug_pos < 100) {
        int len = slug_end - slug_pos;
        strncpy(slugs[count], slug_pos, len);
        slugs[count][len] = '\0';
      }
    }

    card_pos = strstr(card_pos, "mantine-Card-root");
    if (!card_pos) break;

    char *rating_pos = strstr(card_pos, "mantine-807m0k");
    if (rating_pos) {
      rating_pos = strstr(rating_pos, "⭐");
      rating_pos+=2;
      while(*rating_pos && !isdigit(*rating_pos)) rating_pos++;
      char *rating_end = rating_pos;
      while (*rating_end && (*rating_end == '.' || isdigit(*rating_end))) rating_end++;

      int len = rating_end - rating_pos;
      if (len > 0 && len < 10) {
        strncpy(ratings[count], rating_pos, len);
        ratings[count][len] = '\0';
      }
    }

    char *title_pos = strstr(card_pos, "mantine-pdrfb7");
    if (title_pos) {
      title_pos = strstr(title_pos, ">");
      if (title_pos) {
        title_pos++;
        char *title_end = strchr(title_pos, '<');
        if (title_end && title_end - title_pos > 3 && title_end - title_pos < 80) {
          int len = title_end - title_pos;
          strncpy(titles[count], title_pos, len);
          titles[count][len] = '\0';

          for (int i = 0; titles[count][i]; i++) {
            if (titles[count][i] == '&' && strncmp(titles[count] + i, "&amp;", 5) == 0) {
              memmove(titles[count] + i, titles[count] + i + 4, strlen(titles[count] + i));
              titles[count][i] = ' ';
            }
          }
        }
      }
    }

    char *views_pos = strstr(card_pos, "mantine-w7z63c");
    if (views_pos) {
      views_pos = strstr(views_pos, "Views:");
      if (views_pos) {
        views_pos += 6;
        while (*views_pos && !isdigit(*views_pos)) views_pos++;

        char *views_end = views_pos;
        while (*views_end && !isspace(*views_end) && *views_end != '<') views_end++;

        int len = (views_end - views_pos);
        if (len > 0 && len < 20) {
          strncpy(yearly_views[count], views_pos, len);
          yearly_views[count][len] = '\0';
        }
      }
    }

    char *chap_pos = strstr(card_pos, "mantine-175mpop");
    if (chap_pos) {
      chap_pos = strstr(chap_pos, "Chapters:");
      if (chap_pos) {
        chap_pos += 9;
        while (*chap_pos && !isdigit(*chap_pos)) chap_pos++;

        char *chap_end = chap_pos;
        while (*chap_end && !isspace(*chap_end) && *chap_end != '<') chap_end++;

        int len = chap_end - chap_pos;
        if (len > 0 && len < 20) {
          strncpy(chapters[count], chap_pos, len);
          chapters[count][len] = '\0';
        }
      }
    }

    count++;
    card_pos += 1000;
  }
  return count;
}

char *fetch_url(const char *url) {
  CURL *curl = curl_easy_init();
  if (!curl) return NULL;

  struct Memory chunk = { .data = NULL, .size = 0 };
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (compatible; NovelBot/1.0)");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK || chunk.size < 100) {
    free(chunk.data);
    return NULL;
  }
  return chunk.data;
}

int fetch_page(int current_page, PageCache* out_page, char* escaped)
{
  if (!out_page)
    return -1;

  char url[512];
  snprintf(url, sizeof(url),
           "https://wuxia.click/search/%s?page=%d&order_by=-total_views",
           escaped, current_page);

  clear();
  attron(COLOR_PAIR(2));
  mvprintw(0, 0, "Fetching page %d...", current_page);
  attroff(COLOR_PAIR(2));
  refresh();

  /* ---- Fetch HTML ---- */
  char* html = fetch_url(url);
  if (!html) {
    return -1;
  }

  char titles[12][256] = {0};
  char yearly_views[12][256] = {0};
  char chapters[12][256] = {0};
  char ratings[12][256] = {0};
  char slugs[12][256] = {0};
  int count = extract_novel_info(html, titles, yearly_views, chapters, ratings, slugs);

  free(html);

  if (count <= 0)
    return -1;

  out_page->page_number = current_page;
  out_page->count = count;
  out_page->is_valid = 1;

  memcpy(out_page->titles, titles, sizeof(titles));
  memcpy(out_page->slugs, slugs, sizeof(slugs));
  memcpy(out_page->ratings, ratings, sizeof(ratings));
  memcpy(out_page->yearly_views, yearly_views, sizeof(yearly_views));
  memcpy(out_page->chapters, chapters, sizeof(chapters));

  return count;
}

void search_webnovel() {

  initscr();
  cbreak();
  echo();
  clear();

  attron(COLOR_PAIR(4));
  mvprintw(0, 0, "🔍 Enter novel title: ");
  attroff(COLOR_PAIR(4));
  refresh();

  char query[256];
  getnstr(query, sizeof(query) - 1);
  noecho();

  CURL *curl = curl_easy_init();
  char *escaped = curl_easy_escape(curl, query, 0);

  clear_cache();
  int current_page = 1;

  while (1) {

    PageCache* cached = get_cached_page(current_page);
    int count, action;

    if (!cached) {
      if (fetch_page(current_page, &page_cache[current_page], escaped) <= 0){
        clear();
        attron(COLOR_PAIR(4));
        mvprintw(0, 0, "No Search Results Found.");
        mvprintw(1, 0, "Press any key to go back...");
        attroff(COLOR_PAIR(4));
        refresh();
        getch();
        break;
      }

      cached = &page_cache[current_page];
    }

    count = cached->count;

    action = display_webnovel_list(
      cached->titles,
      count,
      cached->yearly_views,
      cached->chapters,
      cached->ratings,
      cached->slugs,
      &current_page
    );

    if (action == -2) {
      break;
    }
    else if (action >= 0) {
      char novel_chapters[3500][128] = {0};
      char novel_chapter_titles[3500][128] = {0};

      int total_chapters = fetch_novel_chapters(cached->slugs[action], novel_chapters, novel_chapter_titles);
      show_chapter_browser(novel_chapters, novel_chapter_titles, total_chapters, cached->titles[action], cached->slugs[action], 0);
    }
  }

  curl_free(escaped);
  curl_easy_cleanup(curl);
  endwin();
}
