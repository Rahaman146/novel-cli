#define _POSIX_C_SOURCE 200809L

#include "webnovel.h"
#include "network.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>

struct Memory {
    char *data;
    size_t size;
};

static char *title_patterns[] = {
    "mantine-pdrfb7",      // Wuxia.click EXACT match
    "novel-title",         // Generic novel sites
    "book-title", "story-title", "series-title",
    "h2", "h3", "h4",      // Headers (most common)
    "title", "name", "novel-name",
    ".card-title", "[data-title]", "item-title"
};

int extract_novel_titles(char *html, char titles[10][256]) {
    int count = 0;
    size_t num_patterns = sizeof(title_patterns) / sizeof(title_patterns[0]);

    for (size_t p = 0; p < num_patterns && count < 10; p++) {
        char *pos = html;
        while ((pos = strstr(pos, title_patterns[p])) && count < 10) {
            char *start = strstr(pos, ">");
            if (!start) { pos += strlen(title_patterns[p]); continue; }
            start++;
            
            char *end = strchr(start, '<');
            if (!end || end - start < 3 || end - start > 100) {
                pos = end ? end + 1 : pos + 50;
                continue;
            }
            
            int len = end - start;
            char title[256] = {0};
            strncpy(title, start, len);
            
            for (int i = 0; title[i]; i++) {
                if (title[i] == '&' && strncmp(title + i, "&amp;", 5) == 0) {
                    memmove(title + i, title + i + 4, strlen(title + i));
                    title[i] = ' ';
                }
                if (!isprint(title[i])) title[i] = ' ';
            }
            
            int is_dup = 0;
            for (int j = 0; j < count; j++) {
                if (strncmp(titles[j], title, 20) == 0) {
                    is_dup = 1; break;
                }
            }
            if (is_dup || strlen(title) < 3) {
                pos = end + 1;
                continue;
            }
            
            strncpy(titles[count], title, 255);
            count++;
            pos = end + 1;
        }
    }
    
    char *pos = strstr(html, "title=\"");
    while (pos && count < 10) {
        pos += 7;
        char *end = strchr(pos, '"');
        if (!end || end - pos < 3 || end - pos > 80) break;
        
        strncpy(titles[count], pos, end - pos);
        titles[count][end - pos] = 0;
        count++;
        pos = strstr(end, "title=\"");
    }
    
    return count;
}

static char *fetch_url(const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    struct Memory chunk = { .data = NULL, .size = 0 };
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (compatible; NovelBot/1.0)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || chunk.size < 100) {
        free(chunk.data);
        return NULL;
    }

    return chunk.data;
}

void search_webnovel() {
    echo();
    clear();
    mvprintw(0, 0, "🔍 Enter novel title to search: ");
    refresh();
    
    char query[256];
    getnstr(query, sizeof(query) - 1);
    noecho();
    
    // Build URL
    CURL *curl = curl_easy_init();
    char *escaped = curl_easy_escape(curl, query, 0);
    char url[512];
    snprintf(url, sizeof(url), "https://wuxia.click/search/%s?order_by=-yearly_views", escaped);
    curl_free(escaped);
    curl_easy_cleanup(curl);
    
    clear();
    mvprintw(0, 0, "Searching for %s ....", query);
    refresh();
    
    // Fetch page
    char *html = fetch_url(url);
    if (!html) {
        mvprintw(3, 0, "Failed to fetch page");
        refresh();
        getch();
        return;
    }
    
    // Debug file
    FILE *debug = fopen("debug.html", "w");
    if (debug) {
        fputs(html, debug);
        fclose(debug);
    }
    
    char titles[10][256] = {0};
    int count = extract_novel_titles(html, titles);
    
    clear();
    mvprintw(0, 0, "Found %d results for '%s':\n\n", count, query);
    
    if (count == 0) {
        mvprintw(2, 0, "No titles found. Check debug.html");
    } else {
        for (int i = 0; i < count; i++) {
            mvprintw(2 + i, 0, "%d  %s", i + 1, titles[i]);
        }
    }
    
    mvprintw(15, 0, "\nPress any key to continue...");
    refresh();
    getch();
    free(html);
}

