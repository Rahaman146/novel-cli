#define _POSIX_C_SOURCE 200809L

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

  return newwin(CHAPTER_WINDOW_HEIGHT, CHAPTER_WINDOW_WIDTH, (max_y - CHAPTER_WINDOW_HEIGHT) / 2,  (max_x - CHAPTER_WINDOW_WIDTH) / 2);
}

void display_chapter_list(char chapters[3500][128], int total, int offset, int highlight, const char* novel_title) {

  clear();
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  (void) cols;

  attron(COLOR_PAIR(4));
  mvprintw(0, 0, "📖 %s CHAPTERS (%d/%d total)", novel_title, total, total); 
  attroff(COLOR_PAIR(4));
  attron(COLOR_PAIR(5) | A_DIM);
  mvprintw(1, 0, "═══════════════════════════════════════════════════════════════");
  attroff(COLOR_PAIR(5) | A_DIM);

  int start = offset;
  int end = offset + CHAPTERS_VISIBLE;
  if (end > total) end = total;
  int row = 0;

  for (int i = start; i < end; i++) {
    row = 3 + (i - start);
    if (i == highlight) {
      attron(COLOR_PAIR(5) | A_DIM);
      mvprintw(row, 0, "➤ %4d: %s", i+1, chapters[i]);
      attroff(COLOR_PAIR(5) | A_DIM);
    } else {
      mvprintw(row, 0, "  %4d: %s", i+1, chapters[i]);
    }
  }

  attron(COLOR_PAIR(5) | A_DIM);
  mvprintw(row + 2, 0, "═══════════════════════════════════════════════════════════════");
  attroff(COLOR_PAIR(5) | A_DIM);

  attron(COLOR_PAIR(4));
  mvprintw(rows - 1, 2, "↑↓ Move   / Search Chapter   q Back   Enter Open");
  attroff(COLOR_PAIR(4));
  move(21, 0);
  refresh();
}

int show_chapter_browser(char chapters[3500][128], char chapter_titles[3500][128], int total, const char* novel_title, const char* novel_slug __attribute__((unused))) {
  int offset = 0;
  int highlight = 0;

  while (1) {
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
        echo(); 
        attron(COLOR_PAIR(4));
        mvprintw(21, 0, "Enter chapter: "); 
        attroff(COLOR_PAIR(4));
        refresh();
        char search_buf[16]; getnstr(search_buf, 15); noecho();
        int target = atoi(search_buf);
        if (target > 0 && target <= total) {
          offset = target - 5; if (offset < 0) offset = 0;
          highlight = target - 1;
        }
        break;
      }

      case 10: {
        char* chapter_html = fetch_chapter_content(chapters[highlight]);

        FILE* debug = fopen("debug1.html", "w");
        if (debug && chapter_html) { fputs(chapter_html, debug); fclose(debug); }

        if (chapter_html && strlen(chapter_html) > 1000) {
          display_chapter_content(novel_title, highlight + 1, chapter_html);
          free(chapter_html);
        } else {
          mvprintw(20, 0, "❌ Failed to fetch chapter! Check debug1.html");
          refresh(); getch();
        }
        break;
      }

      case 'q': case 'Q': case KEY_LEFT:
        return -1;
    }
  }
}

static void extract_text_recursive(myhtml_tree_node_t* node, char** buffer, size_t* buffer_size, size_t* current_len) {
  if (!node) return;

  myhtml_tag_id_t tag_id = myhtml_node_tag_id(node);

  if (tag_id == MyHTML_TAG__TEXT) {
    size_t text_len = 0;
    const char* text = myhtml_node_text(node, &text_len);

    if (text && text_len > 0) {
      for (size_t i = 0; i < text_len; i++) {
        if (*current_len + 2 >= *buffer_size) {
          *buffer_size *= 2;
          *buffer = realloc(*buffer, *buffer_size);
        }

        unsigned char c = (unsigned char)text[i];

        if (isspace(c) || c == 0xA0) {
          if (*current_len > 0 && (*buffer)[*current_len - 1] != ' ' && (*buffer)[*current_len - 1] != '\n') {
            (*buffer)[(*current_len)++] = ' ';
          }
        } else {
          (*buffer)[(*current_len)++] = c;
        }
      }
      (*buffer)[*current_len] = '\0';
    }
  }

  if (tag_id == MyHTML_TAG_P || tag_id == MyHTML_TAG_BR) {
    if (*current_len > 0 && (*buffer)[*current_len - 1] != '\n') {
      (*buffer)[(*current_len)++] = '\n';
    }
  }

  myhtml_tree_node_t* child = myhtml_node_child(node);
  while (child) {
    extract_text_recursive(child, buffer, buffer_size, current_len);
    child = myhtml_node_next(child);
  }
}

static void normalize_whitespace(char* str) {
  char* src = str;
  char* dst = str;
  int last_was_space = 1;
  int last_was_newline = 0;

  while (*src) {
    if (*src == '\n') {
      if (!last_was_newline) {
        *dst++ = '\n';
        last_was_newline = 1;
      }
      last_was_space = 1;
      src++;
    }
    else if (isspace(*src)) {
      if (!last_was_space && !last_was_newline) {
        *dst++ = ' ';
        last_was_space = 1;
      }
      src++;
    }
    else {
      *dst++ = *src++;
      last_was_space = 0;
      last_was_newline = 0;
    }
  }

  while (dst > str && (dst[-1] == ' ' || dst[-1] == '\n'))
    dst--;

  *dst = '\0';
}

int display_chapter_content(const char* novel_title, int chapter_num, const char* html) {
  if (!html) return 1;

  myhtml_t* myhtml = myhtml_create();
  myhtml_init(myhtml, MyHTML_OPTIONS_DEFAULT, 1, 0);
  myhtml_tree_t* tree = myhtml_tree_create();
  myhtml_tree_init(tree, myhtml);
  myhtml_parse(tree, MyENCODING_UTF_8, html, strlen(html));

  myhtml_collection_t* divs = myhtml_get_nodes_by_tag_id(tree, NULL, MyHTML_TAG_DIV, NULL);

  char* chapter_text = malloc(32768);
  if (!chapter_text) {
    if (divs) myhtml_collection_destroy(divs);
    myhtml_tree_destroy(tree);
    myhtml_destroy(myhtml);
    return 1;
  }
  chapter_text[0] = '\0';
  size_t buffer_size = 32768;
  size_t current_len = 0;

  int found_content = 0;

  if (divs && divs->list) {
    for (size_t i = 0; i < divs->length; i++) {
      myhtml_tree_node_t* node = divs->list[i];
      myhtml_tree_attr_t* attr = myhtml_attribute_by_key(node, "id", 2);

      if (attr) {
        const char* id_val = myhtml_attribute_value(attr, NULL);
        if (id_val && strcmp(id_val, "chapterText") == 0) {
          found_content = 1;

          size_t para_start = current_len;
          extract_text_recursive(node, &chapter_text, &buffer_size, &current_len);

          normalize_whitespace(chapter_text + para_start);
          current_len = strlen(chapter_text);

          if (current_len + 3 < buffer_size) {
            strcat(chapter_text, "\n\n");
            current_len += 2;
          }
        }
      }
    }
  }

  if (divs) myhtml_collection_destroy(divs);
  myhtml_tree_destroy(tree);
  myhtml_destroy(myhtml);

  if (!found_content || strlen(chapter_text) < 10) {
    mvprintw(20, 0, "❌ No chapter content found!");
    refresh();
    getch();
    free(chapter_text);
    return 1;
  }

  int offset = 0;
  int ch = 0;

  while (1) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    clear();

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(0, 2, "📖 %s - Chapter %d", novel_title, chapter_num);
    attroff(COLOR_PAIR(2) | A_BOLD);

    attron(A_DIM);
    mvhline(1, 0, ACS_HLINE, max_x);
    attroff(A_DIM);

    int line = 0;
    int col = 4;
    char *p = chapter_text;
    int visible_start = 2;
    int visible_end = max_y - 2;
    int margin_right = 4;

    while (*p) {
      if (col == 4) {
        while (*p == ' ') p++;
      }

      if (!*p) break;

      if (*p == '\n') {
        line++;
        col = 4;
        p++;
        continue;
      }

      char *word_start = p;
      while (*p && *p != ' ' && *p != '\n') p++;
      int word_len = p - word_start;

      if (word_len == 0) {
        if (*p == ' ') p++;
        continue;
      }

      if (col > 4 && col + word_len >= max_x - margin_right) {
        line++;
        col = 4;
      }

      if (line >= offset && line < offset + (visible_end - visible_start)) {
        int screen_y = visible_start + (line - offset);
        mvprintw(screen_y, col, "%.*s", word_len, word_start);
      }

      col += word_len;

      if (*p == ' ') {
        if (col < max_x - margin_right) {
          if (line >= offset && line < offset + (visible_end - visible_start)) {
            mvaddch(visible_start + (line - offset), col, ' ');
          }
          col++;
        }
        p++;
      }
    }

    int total_lines = line;

    attron(COLOR_PAIR(4) | A_DIM);
    mvprintw(max_y - 1, 2, "← Prev Page   q Back   ↑/↓ Scroll   Line %d/%d", offset + 1, total_lines > 0 ? total_lines : 1);
    attroff(COLOR_PAIR(4) | A_DIM);

    refresh();

    ch = getch();
    if (ch == 'q' || ch == 'Q' || ch == KEY_LEFT) break;

    if (ch == KEY_UP && offset > 0) offset--;
    if (ch == KEY_DOWN && offset < total_lines - (visible_end - visible_start)) offset++;
    if (ch == KEY_PPAGE) {
      offset -= (visible_end - visible_start);
      if (offset < 0) offset = 0;
    }
    if (ch == KEY_NPAGE) {
      offset += (visible_end - visible_start);
      if (offset > total_lines - (visible_end - visible_start)) {
        offset = total_lines - (visible_end - visible_start);
        if (offset < 0) offset = 0;
      }
    }
  }

  free(chapter_text);
  return 0;
}
