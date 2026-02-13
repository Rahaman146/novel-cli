#include "network.h"
#include "chapter_controller.h"

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <ncurses.h>
#include <ctype.h>
#include <myhtml/api.h>


WINDOW* create_chapter_window() {
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  return newwin(CHAPTER_WINDOW_HEIGHT, CHAPTER_WINDOW_WIDTH, 
                (max_y - CHAPTER_WINDOW_HEIGHT) / 2, 
                (max_x - CHAPTER_WINDOW_WIDTH) / 2);
}

void display_chapter_list(char chapters[3500][128], int total, int offset, int highlight, const char* novel_title) {
  clear();
  mvprintw(0, 0, "📖 %s CHAPTERS (%d/%d total)", novel_title, total, total); 
  mvprintw(1, 0, "═══════════════════════════════════════════════════════════════");

  int start = offset;
  int end = offset + CHAPTERS_VISIBLE;
  if (end > total) end = total;

  for (int i = start; i < end; i++) {
    int row = 3 + (i - start);
    if (i == highlight) {
      attron(A_REVERSE | A_BOLD);
      mvprintw(row, 0, "➤ %4d: %s", i+1, chapters[i]);
      attroff(A_REVERSE | A_BOLD);
    } else {
      mvprintw(row, 0, "  %4d: %s", i+1, chapters[i]);
    }
  }

  if (offset > 0) 
    mvprintw(15, 0, "↑ ↑ ↑ PAGE UP ↑ ↑ ↑");
  if (end < total) 
    mvprintw(16, 0, "↓ ↓ ↓ PAGE DOWN ↓ ↓ ↓");

  mvprintw(18, 0, "[↑↓=move] [PgUp/PgDn=page] [/=search chap] [q=back] [Enter=open]");
  refresh();
}

int show_chapter_browser(char chapters[3500][128], char chapter_titles[3500][128], int total, const char* novel_title, const char* novel_slug) {
  int offset = 0;
  int highlight = 0;

  while (1) {  // Changed to infinite loop
    display_chapter_list(chapter_titles, total, offset, highlight, novel_title);

    int ch = getch();
    switch (ch) {
      case KEY_UP:
        if (highlight > 0) highlight--;
        if (highlight < offset) offset = highlight;
        break;

      case KEY_DOWN:
        if (highlight < total - 1) highlight++;
        if (highlight >= offset + CHAPTERS_VISIBLE) offset++;
        break;

      case KEY_PPAGE:
        offset -= CHAPTERS_VISIBLE; if (offset < 0) offset = 0; highlight = offset; break;
      case KEY_NPAGE:
        offset += CHAPTERS_VISIBLE; 
        if (offset > total - CHAPTERS_VISIBLE) offset = total - CHAPTERS_VISIBLE;
        highlight = offset; 
        break;

      case '/': {
        echo(); mvprintw(20, 0, "Enter chapter: "); refresh();
        char search_buf[16]; getnstr(search_buf, 15); noecho();
        int target = atoi(search_buf);
        if (target > 0 && target <= total) {
          offset = target - 5; if (offset < 0) offset = 0;
          highlight = target - 1;
        }
        break;
      }

      case 10: {  // ENTER - READ CHAPTER IMMEDIATELY
        printf("\n🔍 Opening Chapter #%d: %s\n", highlight+1, chapter_titles[highlight]);
        char* chapter_html = fetch_chapter_content(chapters[highlight]);  // Use chapters[highlight] URL

        FILE* debug = fopen("debug1.html", "w");
        if (debug && chapter_html) { fputs(chapter_html, debug); fclose(debug); }

        if (chapter_html && strlen(chapter_html) > 1000) {
          int result = display_chapter_content(novel_title, highlight + 1, chapter_html);
          free(chapter_html);
          if (result == 0) return highlight;  // 0 = back to book list
        } else {
          mvprintw(20, 0, "❌ Failed to fetch chapter! Check debug1.html");
          refresh(); getch();
        }
        break;  // Stay in chapter browser
      }

      case 'q': case 'Q':
        return -1;  // Back to book list
    }
  }
}

int display_chapter_content(const char* novel_title, int chapter_num, const char* html) {
  if (!html) return 1;

  /* ---------- Parse HTML using MyHTML ---------- */
  myhtml_t* myhtml = myhtml_create();
  myhtml_init(myhtml, MyHTML_OPTIONS_DEFAULT, 1, 0);

  myhtml_tree_t* tree = myhtml_tree_create();
  myhtml_tree_init(tree, myhtml);

  myhtml_parse(tree, MyENCODING_UTF_8, html, strlen(html));

  /* ---------- Find main chapter content ---------- */
  myhtml_collection_t* divs =
    myhtml_get_nodes_by_tag_id(tree, NULL, MyHTML_TAG_DIV, NULL);

  const char* clean_text = NULL;

  for (size_t i = 0; i < divs->length; i++) {
    myhtml_tree_node_t* node = divs->list[i];

    /* Use the modern MyHTML getter for attributes */
    myhtml_tree_attr_t* attr = myhtml_attribute_by_key(node, "class", 5);
    if (!attr) continue;

    size_t attr_len = 0;
    const char* attr_val = (const char*) myhtml_attribute_value(attr, &attr_len);

    if (attr_val && (strstr(attr_val, "prose") || strstr(attr_val, "chapter-content"))) {
      size_t text_size = 0;
      clean_text = myhtml_node_text(node, &text_size);
      break;
    }
  }

  if (!clean_text) {
    myhtml_tree_destroy(tree);
    myhtml_destroy(myhtml);
    return 1;
  }

  /* ---------- Ncurses Display ---------- */
  int offset = 0;
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  while (1) {
    clear();
    mvprintw(0, 0,
             "📖 %s - Chapter %d  [q=back ↑↓ PgUp/PgDn]",
             novel_title, chapter_num);
    mvprintw(1, 0,
             "══════════════════════════════════════════════════════════");

    int printed = 0;
    int current_line = 0;

    char* line_ptr = (char*) clean_text; // cast only for strchr

    while (line_ptr && printed < rows - 4) {
      char* next_line = strchr(line_ptr, '\n');
      if (next_line) *next_line = '\0';

      if (current_line >= offset) {
        mvprintw(3 + printed, 0,
                 "%.*s", cols - 1, line_ptr);
        printed++;
      }

      if (!next_line) break;
      line_ptr = next_line + 1;
      current_line++;
    }

    refresh();

    int ch = getch();
    if (ch == 'q') break;
    if (ch == KEY_UP && offset > 0) offset--;
    if (ch == KEY_DOWN) offset++;
    if (ch == KEY_PPAGE && offset > 10) offset -= 10;
    if (ch == KEY_NPAGE) offset += 10;
  }

  /* ---------- Cleanup ---------- */
  myhtml_tree_destroy(tree);
  myhtml_destroy(myhtml);

  return 1;
}
