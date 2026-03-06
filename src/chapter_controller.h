#ifndef CHAPTER_CONTROLLER_H
#define CHAPTER_CONTROLLER_H

#include <stdio.h>
#include <ncurses.h>

#define CHAPTERS_VISIBLE 15
#define CHAPTER_WINDOW_HEIGHT 15
#define CHAPTER_WINDOW_WIDTH 60
#define MAX_CONTENT_LINES 10000
#define CONTENT_WIDTH 100

void display_chapter_list(
    char chapters[3500][128], 
    int total, int offset, 
    int highlight, 
    const char* novel_title
    );

int chapter_search_menu(WINDOW* win, char chapters[3500][128], int total, int* new_offset);

int show_chapter_browser(
    char chapters[3500][128], 
    char chapter_titles[3500][128], 
    int total, 
    const char* novel_title, 
    const char* novel_slug,
    int start_idx
    );

int display_chapter_content(const char* novel_title, int chapter_num, const char* html);

#endif
