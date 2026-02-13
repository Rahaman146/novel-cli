#ifndef WEBNOVEL_H
#define WEBNOVEL_H

int extract_novel_info(char *html, char titles[10][256], char yearly_views[10][256], char chapters[10][256], char ratings[10][256], char slugs[10][256]);

void search_webnovel();

char *fetch_url(const char *url);

#endif
