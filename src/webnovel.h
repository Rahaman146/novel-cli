#ifndef WEBNOVEL_H
#define WEBNOVEL_H

typedef enum {
  PROVIDER_WUXIA = 0,
  PROVIDER_FREEWEBNOVEL,
  PROVIDER_ROYALROAD
} WebnovelProvider;

int extract_novel_info(
    char *html, 
    char titles[12][256], 
    char yearly_views[12][256], 
    char chapters[12][256], 
    char ratings[12][256], 
    char slugs[12][256]
    );

int extract_freewebnovel_info(
    char *html, 
    char titles[12][256], 
    char yearly_views[12][256], 
    char chapters[12][256], 
    char ratings[12][256], 
    char slugs[12][256]
    );

int extract_royalroad_info(
    char *html, 
    char titles[12][256], 
    char yearly_views[12][256], 
    char chapters[12][256], 
    char ratings[12][256], 
    char slugs[12][256]
    );

void search_webnovel();

void sort_search_results(
    char titles[12][256],
    char yearly_views[12][256],
    char chapters[12][256],
    char ratings[12][256],
    char slugs[12][256],
    int count
);

char *fetch_url(const char *url);

#endif

