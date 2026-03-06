#ifndef WEBNOVEL_H
#define WEBNOVEL_H

int extract_novel_info(
    char *html, 
    char titles[12][256], 
    char yearly_views[12][256], 
    char chapters[12][256], 
    char ratings[12][256], 
    char slugs[12][256]
    );

void search_webnovel();

char *fetch_url(const char *url);

#endif
