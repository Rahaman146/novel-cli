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
  move(19, 0);
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
        echo(); mvprintw(20, 0, "Enter chapter: "); refresh();
        char search_buf[16]; getnstr(search_buf, 15); noecho();
        int target = atoi(search_buf);
        if (target > 0 && target <= total) {
          offset = target - 5; if (offset < 0) offset = 0;
          highlight = target - 1;
        }
        break;
      }

      case 10: {  // Enter key
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

      case 'q': case 'Q':
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
        // Buffer management
        if (*current_len + 2 >= *buffer_size) {
          *buffer_size *= 2;
          *buffer = realloc(*buffer, *buffer_size);
        }

        unsigned char c = (unsigned char)text[i];

        // Treat all "junk" whitespace (tabs, multiple newlines in HTML source) as a single space
        if (isspace(c) || c == 0xA0) {
          // Only add a space if the previous character wasn't already a space/newline
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

  // Force structural breaks for specific tags to ensure paragraphs don't run together
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

  // trim trailing space/newline
  while (dst > str && (dst[-1] == ' ' || dst[-1] == '\n'))
    dst--;

  *dst = '\0';
}

int display_chapter_content(const char* novel_title, int chapter_num, const char* html) {
  if (!html) return 1;

  // Setup HTML parser
  myhtml_t* myhtml = myhtml_create();
  myhtml_init(myhtml, MyHTML_OPTIONS_DEFAULT, 1, 0);
  myhtml_tree_t* tree = myhtml_tree_create();
  myhtml_tree_init(tree, myhtml);
  myhtml_parse(tree, MyENCODING_UTF_8, html, strlen(html));

  // Find all divs with id="chapterText"
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

          // Extract text from this div
          size_t para_start = current_len;
          extract_text_recursive(node, &chapter_text, &buffer_size, &current_len);

          // Clean this paragraph
          normalize_whitespace(chapter_text + para_start);
          current_len = strlen(chapter_text);

          // Add paragraph break
          if (current_len + 3 < buffer_size) {
            strcat(chapter_text, "\n\n");
            current_len += 2;
          }
        }
      }
    }
  }

  // Cleanup HTML parser
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

  // Render loop
  int offset = 0;
  int ch = 0;

  while (1) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    clear();

    // Header
    attron(A_BOLD);
    mvprintw(0, 2, "📖 %s - Chapter %d", novel_title, chapter_num);
    attroff(A_BOLD);

    // Separator
    mvprintw(1, 0, "─");
    for (int i = 1; i < max_x - 1; i++) printw("─");

    // Render text with word wrapping
    int line = 0;
    int col = 4;
    char *p = chapter_text;
    int visible_start = 2;
    int visible_end = max_y - 2;
    int margin_right = 4;

    while (*p) {
      // Skip leading spaces at line start
      if (col == 4) {
        while (*p == ' ') p++;
      }

      if (!*p) break;

      // Handle newlines
      if (*p == '\n') {
        line++;
        col = 4;
        p++;
        continue;
      }

      // Find next word
      char *word_start = p;
      while (*p && *p != ' ' && *p != '\n') p++;
      int word_len = p - word_start;

      if (word_len == 0) {
        if (*p == ' ') p++;
        continue;
      }

      // Wrap if needed
      if (col > 4 && col + word_len >= max_x - margin_right) {
        line++;
        col = 4;
      }

      // Draw if visible
      if (line >= offset && line < offset + (visible_end - visible_start)) {
        int screen_y = visible_start + (line - offset);
        mvprintw(screen_y, col, "%.*s", word_len, word_start);
      }

      col += word_len;

      // Add space
      if (*p == ' ') {
        if (col < max_x - margin_right) {
          // Only print the space if we aren't about to wrap anyway
          if (line >= offset && line < offset + (visible_end - visible_start)) {
            mvaddch(visible_start + (line - offset), col, ' ');
          }
          col++;
        }
        p++; // Always advance p if it was a space
      }
    }

    int total_lines = line;

    // Footer
    attron(A_DIM);
    mvprintw(max_y - 1, 2, "[q] Back  [↑/↓] Scroll  [PgUp/PgDn] Page  Line %d/%d", 
             offset + 1, total_lines > 0 ? total_lines : 1);
    attroff(A_DIM);

    refresh();

    // Handle input
    ch = getch();
    if (ch == 'q' || ch == 'Q') break;

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
