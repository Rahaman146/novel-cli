#define _GNU_SOURCE

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

static void decode_title_entities(char *str) {
  if (!str || !*str) return;
  char buf[512] = {0};
  char *d = buf;
  const char *s = str;

  while (*s && (d - buf) < 500) {
    if (*s == '&') {
      if (strncmp(s, "&amp;", 5) == 0) { *d++ = '&'; s += 5; }
      else if (strncmp(s, "&quot;", 6) == 0) { *d++ = '"'; s += 6; }
      else if (strncmp(s, "&apos;", 6) == 0) { *d++ = '\''; s += 6; }
      else if (strncmp(s, "&#39;", 5) == 0) { *d++ = '\''; s += 5; }
      else if (strncmp(s, "&#x27;", 6) == 0) { *d++ = '\''; s += 6; }
      else if (strncmp(s, "&#x2B;", 6) == 0 || strncmp(s, "&#43;", 5) == 0) { *d++ = '+'; s += (s[2] == 'x' ? 6 : 5); }
      else if (strncmp(s, "&lt;", 4) == 0) { *d++ = '<'; s += 4; }
      else if (strncmp(s, "&gt;", 4) == 0) { *d++ = '>'; s += 4; }
      else if (strncmp(s, "&nbsp;", 6) == 0) { *d++ = ' '; s += 6; }
      else { *d++ = *s++; }
    } else {
      *d++ = *s++;
    }
  }
  *d = '\0';
  strcpy(str, buf);
}

int extract_novel_info(char *html, char titles[12][256], char yearly_views[12][256], char chapters[12][256], char ratings[12][256], char slugs[12][256]) {
  int count = 0;

  char *card_pos = html;
  while (count < 12) {
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
          decode_title_entities(titles[count]);
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

int extract_freewebnovel_info(char *html, char titles[12][256], char yearly_views[12][256], char chapters[12][256], char ratings[12][256], char slugs[12][256]) {
  int count = 0;
  char *card_pos = html;
  while (count < 12) {
    char *tit_pos = strstr(card_pos, "<h3 class=\"tit\">");
    if (!tit_pos) tit_pos = strstr(card_pos, "tit");
    if (!tit_pos) break;

    char *a_pos = strstr(tit_pos, "<a href=\"");
    if (!a_pos) break;
    a_pos += 9;

    char *a_end = strchr(a_pos, '"');
    if (!a_end) break;

    char href[256] = {0};
    int href_len = a_end - a_pos;
    if (href_len > 0 && href_len < 200) {
      strncpy(href, a_pos, href_len);
      href[href_len] = '\0';
    }

    if (strncmp(href, "/novel/", 7) == 0) {
      snprintf(slugs[count], 256, "freewebnovel:%.200s", href + 7);
    } else {
      snprintf(slugs[count], 256, "freewebnovel:%.200s", href);
    }

    char *title_gt = strchr(a_end, '>');
    if (title_gt) {
      title_gt++;
      char *title_lt = strchr(title_gt, '<');
      if (title_lt && title_lt - title_gt < 250) {
        int tlen = title_lt - title_gt;
        strncpy(titles[count], title_gt, tlen);
        titles[count][tlen] = '\0';
        decode_title_entities(titles[count]);
      }
    }

    strcpy(ratings[count], "N/A");
    char *core_pos = strstr(tit_pos, "class=\"core\"");
    if (core_pos && core_pos - tit_pos < 1000) {
      char *span_pos = strstr(core_pos, "<span>");
      if (span_pos) {
        span_pos += 6;
        char *span_end = strchr(span_pos, '<');
        if (span_end && span_end - span_pos < 10) {
          int rlen = span_end - span_pos;
          strncpy(ratings[count], span_pos, rlen);
          ratings[count][rlen] = '\0';
        }
      }
    }

    strcpy(chapters[count], "N/A");
    char *chap_pos = strstr(tit_pos, "class=\"chapter\"");
    if (chap_pos && chap_pos - tit_pos < 2000) {
      char *s1_pos = strstr(chap_pos, "class=\"s1\">");
      if (s1_pos) {
        s1_pos += 11;
        char *s1_end = strchr(s1_pos, '<');
        if (s1_end && s1_end - s1_pos < 30) {
          int clen = s1_end - s1_pos;
          strncpy(chapters[count], s1_pos, clen);
          chapters[count][clen] = '\0';
        }
      }
    }

    strcpy(yearly_views[count], "N/A");
    long long ch_num = 0;
    if (chapters[count][0] != '\0') {
      for (const char *p = chapters[count]; *p; p++) {
        if (isdigit(*p)) ch_num = ch_num * 10 + (*p - '0');
        else if (*p == ' ') break;
      }
    }
    double rat_num = atof(ratings[count]);
    if (rat_num <= 0.0) rat_num = 1.0;

    long long est_views = (long long)(ch_num * 1000.0 * rat_num);
    if (est_views >= 1000000) {
      snprintf(yearly_views[count], 256, "%.1fM Views", est_views / 1000000.0);
    } else if (est_views >= 1000) {
      snprintf(yearly_views[count], 256, "%lldk Views", est_views / 1000);
    } else if (est_views > 0) {
      snprintf(yearly_views[count], 256, "%lld Views", est_views);
    }

    count++;
    card_pos = a_end + 1;
  }
  return count;
}

int extract_royalroad_info(char *html, char titles[12][256], char yearly_views[12][256], char chapters[12][256], char ratings[12][256], char slugs[12][256]) {
  int count = 0;
  char *card_pos = html;
  while (count < 12) {
    char *tit_pos = strstr(card_pos, "class=\"fiction-title\"");
    if (!tit_pos) break;

    char *a_pos = strstr(tit_pos, "<a href=\"");
    if (!a_pos) break;
    a_pos += 9;

    char *a_end = strchr(a_pos, '"');
    if (!a_end) break;

    char href[256] = {0};
    int href_len = a_end - a_pos;
    if (href_len > 0 && href_len < 200) {
      strncpy(href, a_pos, href_len);
      href[href_len] = '\0';
    }

    snprintf(slugs[count], 256, "royalroad:%.200s", href);


    char *title_gt = strchr(a_end, '>');
    if (title_gt) {
      title_gt++;
      char *title_lt = strchr(title_gt, '<');
      if (title_lt && title_lt - title_gt < 250) {
        int tlen = title_lt - title_gt;
        strncpy(titles[count], title_gt, tlen);
        titles[count][tlen] = '\0';
        decode_title_entities(titles[count]);
      }
    }

    strcpy(ratings[count], "N/A");
    char *rat_pos = strstr(tit_pos, "Rating:");
    if (rat_pos && rat_pos - tit_pos < 3000) {
      rat_pos += 7;
      while (*rat_pos && !isdigit(*rat_pos)) rat_pos++;
      char *rat_end = rat_pos;
      while (*rat_end && (*rat_end == '.' || isdigit(*rat_end))) rat_end++;
      int rlen = rat_end - rat_pos;
      if (rlen > 0 && rlen < 10) {
        strncpy(ratings[count], rat_pos, rlen);
        ratings[count][rlen] = '\0';
      }
    }

    strcpy(chapters[count], "N/A");
    char *chap_pos = strstr(tit_pos, "fa-list");
    if (!chap_pos) chap_pos = strstr(tit_pos, "Chapters</span>");
    if (chap_pos && chap_pos - tit_pos < 4000) {
      char *span_start = strstr(chap_pos, "<span>");
      if (span_start) {
        span_start += 6;
        char *span_end = strchr(span_start, '<');
        if (span_end && span_end - span_start < 40) {
          int clen = span_end - span_start;
          strncpy(chapters[count], span_start, clen);
          chapters[count][clen] = '\0';
        }
      }
    }
    if (strcmp(chapters[count], "N/A") == 0) {
      char *page_pos = strstr(tit_pos, "fa-book");
      if (!page_pos) page_pos = strstr(tit_pos, "Pages</span>");
      if (page_pos && page_pos - tit_pos < 4000) {
        char *span_start = strstr(page_pos, "<span>");
        if (span_start) {
          span_start += 6;
          char *span_end = strchr(span_start, '<');
          if (span_end && span_end - span_start < 40) {
            int clen = span_end - span_start;
            strncpy(chapters[count], span_start, clen);
            chapters[count][clen] = '\0';
          }
        }
      }
    }

    strcpy(yearly_views[count], "N/A");
    char *views_pos = strstr(tit_pos, "fa-eye");
    if (!views_pos) views_pos = strstr(tit_pos, "Views</span>");
    if (views_pos && views_pos - tit_pos < 4000) {
      char *span_start = strstr(views_pos, "<span>");
      if (span_start) {
        span_start += 6;
        char *span_end = strchr(span_start, '<');
        if (span_end && span_end - span_start < 40) {
          int vlen = span_end - span_start;
          strncpy(yearly_views[count], span_start, vlen);
          yearly_views[count][vlen] = '\0';
        }
      }
    }

    count++;
    card_pos = a_end + 1;
  }
  return count;
}

static long long parse_view_count(const char *view_str, const char *chap_str, const char *rating_str) {
  if (view_str && *view_str && strcmp(view_str, "N/A") != 0) {
    char num_buf[64] = {0};
    int nidx = 0;
    char unit = 0;

    for (const char *p = view_str; *p; p++) {
      if (isdigit(*p) || *p == '.') {
        if (nidx < 63) num_buf[nidx++] = *p;
      } else if (*p == 'k' || *p == 'K') {
        unit = 'K';
        break;
      } else if (*p == 'm' || *p == 'M') {
        unit = 'M';
        break;
      } else if (*p == ',' || isspace(*p)) {
        continue;
      } else if (nidx > 0) {
        break;
      }
    }
    num_buf[nidx] = '\0';

    if (nidx > 0) {
      double num = atof(num_buf);
      if (unit == 'M') return (long long)(num * 1000000.0);
      if (unit == 'K') return (long long)(num * 1000.0);
      return (long long)num;
    }
  }

  long long ch_num = 0;
  if (chap_str) {
    for (const char *p = chap_str; *p; p++) {
      if (isdigit(*p)) ch_num = ch_num * 10 + (*p - '0');
      else if (*p == ' ' || *p == 'P' || *p == 'C') break;
    }
  }

  double rating_num = 0.0;
  if (rating_str) rating_num = atof(rating_str);
  if (rating_num <= 0.0) rating_num = 1.0;

  return (long long)(ch_num * 1000.0 * rating_num);
}

void sort_search_results(
  char titles[12][256],
  char yearly_views[12][256],
  char chapters[12][256],
  char ratings[12][256],
  char slugs[12][256],
  int count)
{
  if (count <= 1) return;

  long long scores[12];
  for (int i = 0; i < count; i++) {
    scores[i] = parse_view_count(yearly_views[i], chapters[i], ratings[i]);
  }

  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (scores[j] > scores[i]) {
        long long tmp_score = scores[i];
        scores[i] = scores[j];
        scores[j] = tmp_score;

        char tmp_buf[256];

        strcpy(tmp_buf, titles[i]);
        strcpy(titles[i], titles[j]);
        strcpy(titles[j], tmp_buf);

        strcpy(tmp_buf, yearly_views[i]);
        strcpy(yearly_views[i], yearly_views[j]);
        strcpy(yearly_views[j], tmp_buf);

        strcpy(tmp_buf, chapters[i]);
        strcpy(chapters[i], chapters[j]);
        strcpy(chapters[j], tmp_buf);

        strcpy(tmp_buf, ratings[i]);
        strcpy(ratings[i], ratings[j]);
        strcpy(ratings[j], tmp_buf);

        strcpy(tmp_buf, slugs[i]);
        strcpy(slugs[i], slugs[j]);
        strcpy(slugs[j], tmp_buf);
      }
    }
  }
}

char *fetch_url(const char *url) {
  CURL *curl = curl_easy_init();
  if (!curl) return NULL;

  struct Memory chunk = { .data = NULL, .size = 0 };
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 Firefox/120.0");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK || chunk.size < 100) {
    free(chunk.data);
    return NULL;
  }
  return chunk.data;
}

int fetch_page_provider(WebnovelProvider provider, int current_page, PageCache* out_page, char* escaped)
{
  if (!out_page)
    return -1;

  char url[512];
  if (provider == PROVIDER_FREEWEBNOVEL) {
    snprintf(url, sizeof(url), "https://freewebnovel.com/search?keyword=%s&page=%d", escaped, current_page);
  } else if (provider == PROVIDER_ROYALROAD) {
    snprintf(url, sizeof(url), "https://www.royalroad.com/fictions/search?title=%s&orderBy=views&page=%d", escaped, current_page);
  } else {
    snprintf(url, sizeof(url), "https://wuxia.click/search/%s?page=%d&order_by=-total_views", escaped, current_page);
  }

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

  int count = 0;
  if (provider == PROVIDER_FREEWEBNOVEL) {
    count = extract_freewebnovel_info(html, titles, yearly_views, chapters, ratings, slugs);
  } else if (provider == PROVIDER_ROYALROAD) {
    count = extract_royalroad_info(html, titles, yearly_views, chapters, ratings, slugs);
  } else {
    count = extract_novel_info(html, titles, yearly_views, chapters, ratings, slugs);
  }

  free(html);

  if (count <= 0)
    return -1;

  sort_search_results(titles, yearly_views, chapters, ratings, slugs, count);

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

int fetch_page(int current_page, PageCache* out_page, char* escaped)
{
  return fetch_page_provider(PROVIDER_WUXIA, current_page, out_page, escaped);
}

void search_webnovel() {
  char *provider_options[] = {
    "Wuxia (Wuxia.click)",
    "FreeWebNovel (FreeWebNovel.com)",
    "Royal Road (RoyalRoad.com)"
  };
  int provider_choice = display_menu(provider_options, 3);
  if (provider_choice < 0) return;

  WebnovelProvider provider = (WebnovelProvider)provider_choice;

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
    int cache_idx = (current_page >= 0 ? current_page : 0) % MAX_PAGES;

    if (!cached) {
      if (fetch_page_provider(provider, current_page, &page_cache[cache_idx], escaped) <= 0){
        clear();
        attron(COLOR_PAIR(4));
        mvprintw(0, 0, "No Search Results Found.");
        mvprintw(1, 0, "Press any key to go back...");
        attroff(COLOR_PAIR(4));
        refresh();
        getch();
        break;
      }

      cached = &page_cache[cache_idx];
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

