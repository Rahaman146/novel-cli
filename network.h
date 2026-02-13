#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <cjson/cJSON.h>

size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream);

FILE* download_book(cJSON *results, int choice, char *options[]);

int fetch_novel_chapters(const char* novel_slug, char chapters[3500][128], char chapter_titles[3500][128]);

char* fetch_chapter_content(const char* chapter_slug);

#endif
