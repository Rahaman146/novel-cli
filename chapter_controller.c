#define _POSIX_C_SOURCE 200809L

#include "network.h"
#include "chapter_controller.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <ncurses.h>
#include <ctype.h>

/**
 * Creates a centered window for displaying chapter content.
 * 
 * @return A new ncurses WINDOW pointer centered on the screen
 */
WINDOW* create_chapter_window() {
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  return newwin(CHAPTER_WINDOW_HEIGHT, CHAPTER_WINDOW_WIDTH, (max_y - CHAPTER_WINDOW_HEIGHT) / 2,  (max_x - CHAPTER_WINDOW_WIDTH) / 2);
}

/**
 * Displays a paginated list of chapters in the terminal UI.
 * 
 * @param chapters Array of chapter slugs/URLs
 * @param total Total number of chapters
 * @param offset Current scroll offset (first visible chapter)
 * @param highlight Currently highlighted chapter index
 * @param novel_title Title of the novel to display in header
 */
void display_chapter_list(char chapters[3500][128], int total, int offset, int highlight, const char* novel_title) {

  clear();
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  (void) cols; // Unused variable

  // Display header with novel title and chapter count
  attron(COLOR_PAIR(4));
  mvprintw(0, 0, "📖 %s CHAPTERS (%d/%d total)", novel_title, total, total); 
  attroff(COLOR_PAIR(4));
  attron(COLOR_PAIR(5) | A_DIM);
  mvprintw(1, 0, "═══════════════════════════════════════════════════════════════");
  attroff(COLOR_PAIR(5) | A_DIM);

  // Calculate which chapters to display on screen
  int start = offset;
  int end = offset + CHAPTERS_VISIBLE;
  if (end > total) end = total;
  int row = 0;

  // Display visible chapters with highlighting for selected chapter
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

  // Display bottom border and controls
  attron(COLOR_PAIR(5) | A_DIM);
  mvprintw(row + 2, 0, "═══════════════════════════════════════════════════════════════");
  attroff(COLOR_PAIR(5) | A_DIM);

  attron(COLOR_PAIR(4));
  mvprintw(rows - 1, 2, "↑↓ Move   / Search Chapter   q Back   Enter Open");
  attroff(COLOR_PAIR(4));
  move(21, 0);
  refresh();
}

/**
 * Main chapter browser interface - handles navigation and chapter selection.
 * 
 * @param chapters Array of chapter slugs/URLs for fetching content
 * @param chapter_titles Array of chapter titles for display
 * @param total Total number of chapters
 * @param novel_title Novel title for display
 * @param novel_slug Novel slug (unused)
 * @return -1 when user exits back to previous screen
 */
int show_chapter_browser(char chapters[3500][128], char chapter_titles[3500][128], int total, const char* novel_title, const char* novel_slug __attribute__((unused))) {
  int offset = 0;      // Scroll position
  int highlight = 0;   // Selected chapter

  // Main navigation loop
  while (1) {
    display_chapter_list(chapter_titles, total, offset, highlight, novel_title);

    int ch = getch();
    switch (ch) {
      // Navigate up one chapter
      case KEY_UP:
        if (highlight > 0) highlight--;
        if (highlight < offset) offset = highlight;
        break;

      // Navigate down one chapter
      case KEY_DOWN:
        if (highlight < total - 1) highlight++;
        if (highlight >= offset + CHAPTERS_VISIBLE) offset++;
        break;

      // Page Up - jump up by one screen of chapters
      case KEY_PPAGE:
        offset -= CHAPTERS_VISIBLE; if (offset < 0) offset = 0; highlight = offset; break;
      
      // Page Down - jump down by one screen of chapters
      case KEY_NPAGE:
        offset += CHAPTERS_VISIBLE; 
        if (offset > total - CHAPTERS_VISIBLE) offset = total - CHAPTERS_VISIBLE;
        highlight = offset; 
        break;

      // Search/Jump to chapter by number
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

      // Enter key - fetch and display chapter content
      case 10: {
        // Fetch chapter HTML from server
        char* chapter_html = fetch_chapter_content(chapters[highlight]);

        // Save raw HTML to debug file for troubleshooting
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

      // Quit back to novel list
      case 'q': case 'Q': case KEY_LEFT:
        return -1;
    }
  }
}

/**
 * Extracts text from all id="chapterText" divs in HTML.
 * 
 * @param html HTML string to parse
 * @param output Buffer to store extracted text
 * @param max_size Maximum size of output buffer
 */
static void extract_chapter_text_divs(const char* html, char* output, size_t max_size) {
  const char* p = html;
  char* out = output;
  size_t remaining = max_size - 1;
  int para_count = 0;
  

  
  // Search for all id="chapterText" occurrences
  while ((p = strstr(p, "id=\"chapterText\"")) != NULL) {
    para_count++;
    
    // Move past id="chapterText" to find the closing '>'
    p += strlen("id=\"chapterText\"");
    while (*p && *p != '>') p++;
    if (!*p) break;
    p++; // Skip the '>'
    
    // Now extract text until we hit '<' (which starts the closing tag)
    while (*p && *p != '<' && remaining > 2) {
      unsigned char c = (unsigned char)*p;
      
      // Copy character as-is, but handle special cases
      if (c == 0xC2 && (unsigned char)*(p+1) == 0xA0) {
        // Non-breaking space (UTF-8: 0xC2 0xA0)
        *out++ = ' ';
        remaining--;
        p += 2;
      } else if (isspace(c)) {
        // Regular whitespace
        *out++ = ' ';
        remaining--;
        p++;
      } else {
        // Regular character
        *out++ = c;
        remaining--;
        p++;
      }
    }
    
    // Add paragraph break (two newlines)
    if (remaining > 2) {
      *out++ = '\n';
      *out++ = '\n';
      remaining -= 2;
    }
    

  }
  
  // Remove trailing whitespace
  while (out > output && (out[-1] == ' ' || out[-1] == '\n')) {
    out--;
  }
  
  *out = '\0';
  

}

// /**
//  * Normalizes whitespace in a string by collapsing multiple spaces/newlines.
//  * 
//  * Rules:
//  * - Multiple spaces become single space
//  * - Multiple newlines become single newline
//  * - Trailing whitespace is removed
//  * 
//  * @param str String to normalize (modified in-place)
//  */
// static void normalize_whitespace(char* str) {
//   char* src = str;
//   char* dst = str;
//   int last_was_space = 1;    // Start as if we had a space (prevents leading space)
//   int last_was_newline = 0;

//   while (*src) {
//     if (*src == '\n') {
//       if (!last_was_newline) {
//         *dst++ = '\n';
//         last_was_newline = 1;
//       }
//       last_was_space = 1;
//       src++;
//     }
//     else if (isspace(*src)) {
//       if (!last_was_space && !last_was_newline) {
//         *dst++ = ' ';
//         last_was_space = 1;
//       }
//       src++;
//     }
//     else {
//       *dst++ = *src++;
//       last_was_space = 0;
//       last_was_newline = 0;
//     }
//   }

//   while (dst > str && (dst[-1] == ' ' || dst[-1] == '\n'))
//     dst--;

//   *dst = '\0';
// }


// #include <ncurses.h>
// #include <string.h>
// #include <strings.h>  // Add this for strncasecmp
// #include <stdlib.h>
// #include <ctype.h>

/**
 * Helper function to decode HTML entities
 */
void decode_html_entities(char* str) {
    char* dst = str;
    char* src = str;
    
    while (*src) {
        if (*src == '&') {
            if (strncmp(src, "&lt;", 4) == 0) {
                *dst++ = '<';
                src += 4;
            } else if (strncmp(src, "&gt;", 4) == 0) {
                *dst++ = '>';
                src += 4;
            } else if (strncmp(src, "&amp;", 5) == 0) {
                *dst++ = '&';
                src += 5;
            } else if (strncmp(src, "&quot;", 6) == 0) {
                *dst++ = '"';
                src += 6;
            } else if (strncmp(src, "&apos;", 6) == 0) {
                *dst++ = '\'';
                src += 6;
            } else if (strncmp(src, "&#39;", 5) == 0) {
                *dst++ = '\'';
                src += 5;
            } else if (strncmp(src, "&nbsp;", 6) == 0) {
                *dst++ = ' ';
                src += 6;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/**
 * Check if a tag is a block-level element
 */
int is_block_tag(const char* tag_content) {
    const char* block_tags[] = {
        "div", "p", "br", "h1", "h2", "h3", "h4", "h5", "h6",
        "ul", "ol", "li", "blockquote", "hr", "pre", "table",
        "tr", "td", "th", "section", "article", "header", "footer"
    };
    
    // Extract tag name (skip / for closing tags)
    const char* tag_name = tag_content;
    if (*tag_name == '/') tag_name++;
    
    // Skip whitespace
    while (*tag_name && isspace(*tag_name)) tag_name++;
    
    // Check against block tags
    size_t num_tags = sizeof(block_tags) / sizeof(block_tags[0]);
    for (size_t i = 0; i < num_tags; i++) {
        size_t len = strlen(block_tags[i]);
        if (strncasecmp(tag_name, block_tags[i], len) == 0) {
            // Make sure it's followed by space, > or end
            char next = tag_name[len];
            if (next == '\0' || next == '>' || isspace(next)) {
                return 1;
            }
        }
    }
    
    return 0;
}

/**
 * Helper function to normalize whitespace
 */
void normalize_whitespace(char* str) {
    char* dst = str;
    char* src = str;
    int last_was_space = 1; // Trim leading spaces
    
    while (*src) {
        if (*src == '\n') {
            if (!last_was_space) {
                // Preserve paragraph breaks
                *dst++ = '\n';
                last_was_space = 1;
            }
            src++;
        } else if (isspace(*src)) {
            src++;
            if (!last_was_space && *src && !isspace(*src) && *src != '\n') {
                *dst++ = ' ';
                last_was_space = 1;
            }
        } else {
            *dst++ = *src++;
            last_was_space = 0;
        }
    }
    
    // Trim trailing space/newline
    while (dst > str && (*(dst - 1) == ' ' || *(dst - 1) == '\n')) {
        dst--;
    }
    
    *dst = '\0';
}

/**
 * Helper function to wrap text to specified width, respecting paragraphs
 */
char** wrap_text(const char* text, int width, int* line_count) {
    int capacity = 200;
    char** lines = malloc(capacity * sizeof(char*));
    *line_count = 0;
    
    const char* ptr = text;
    
    while (*ptr) {
        // Skip leading whitespace (except newlines)
        while (*ptr && *ptr == ' ') ptr++;
        if (!*ptr) break;
        
        // Check for paragraph break
        if (*ptr == '\n') {
            // Add empty line for paragraph
            if (capacity <= *line_count) {
                capacity *= 2;
                lines = realloc(lines, capacity * sizeof(char*));
            }
            lines[(*line_count)++] = strdup("");
            ptr++;
            continue;
        }
        
        // Allocate line buffer
        char* line = malloc((size_t)width + 1);
        int line_len = 0;
        
        // Build line word by word
        while (*ptr && *ptr != '\n') {
            // Find next word
            const char* word_start = ptr;
            int word_len = 0;
            
            while (*ptr && !isspace(*ptr)) {
                ptr++;
                word_len++;
            }
            
            if (word_len == 0) break;
            
            // Check if word fits on current line
            if (line_len == 0) {
                // First word on line
                if (word_len > width) {
                    // Word too long, break it
                    strncpy(line, word_start, (size_t)width);
                    line[width] = '\0';
                    line_len = width;
                    ptr = word_start + width;
                } else {
                    strncpy(line, word_start, (size_t)word_len);
                    line[word_len] = '\0';
                    line_len = word_len;
                }
            } else if (line_len + 1 + word_len <= width) {
                // Word fits with space
                line[line_len++] = ' ';
                strncpy(line + line_len, word_start, (size_t)word_len);
                line_len += word_len;
                line[line_len] = '\0';
            } else {
                // Word doesn't fit, break line here
                ptr = word_start;
                break;
            }
            
            // Skip whitespace after word (but not newlines)
            while (*ptr && *ptr == ' ') ptr++;
        }
        
        // Add line to array
        if (capacity <= *line_count) {
            capacity *= 2;
            lines = realloc(lines, capacity * sizeof(char*));
        }
        lines[(*line_count)++] = line;
    }
    
    return lines;
}

/**
 * Parses HTML and displays chapter content with word-wrapping and scrolling.
 */
int display_chapter_content(const char* novel_title, int chapter_num, const char* html) {
    if (!novel_title || !html) {
        return 1;
    }
    
    // Find "chapterText" in HTML
    const char* chapter_start = strstr(html, "id=\"chapterText\"");
    if (!chapter_start) {
        return 1;
    }
    
    // Skip past the id="chapterText" attribute and find the closing '>'
    chapter_start += strlen("id=\"chapterText\"");
    while (*chapter_start && *chapter_start != '>') chapter_start++;
    if (*chapter_start == '>') chapter_start++; // Skip the '>'
    
    // Extract text content between tags, adding breaks after block elements
    size_t html_len = strlen(html);
    char* content = malloc(html_len * 2 + 1);
    size_t content_len = 0;
    const char* ptr = chapter_start;
    int in_tag = 0;
    char tag_buffer[100];
    size_t tag_pos = 0;
    
    while (*ptr) {
        if (*ptr == '<') {
            in_tag = 1;
            tag_pos = 0;
            memset(tag_buffer, 0, sizeof(tag_buffer));
        } else if (*ptr == '>') {
            in_tag = 0;
            
            // Check if this was a block-level closing tag
            if (tag_pos > 0 && is_block_tag(tag_buffer)) {
                // Add a newline after block elements
                if (content_len > 0 && content[content_len - 1] != '\n') {
                    content[content_len++] = '\n';
                }
            }
        } else if (in_tag) {
            // Store tag content for analysis
            if (tag_pos < sizeof(tag_buffer) - 1) {
                tag_buffer[tag_pos++] = *ptr;
            }
        } else {
            // Copy text content
            content[content_len++] = *ptr;
        }
        ptr++;
    }
    content[content_len] = '\0';
    
    // Decode HTML entities
    decode_html_entities(content);
    
    // Normalize whitespace
    normalize_whitespace(content);
    
    // Get terminal dimensions
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Reserve lines for header and footer
    int header_lines = 2;
    int footer_lines = 1;
    int content_height = max_y - header_lines - footer_lines;
    int content_width = max_x - 4; // Leave margins
    
    if (content_width < 20) content_width = 20; // Minimum width
    
    // Wrap text
    int line_count;
    char** lines = wrap_text(content, content_width, &line_count);
    
    // Scrolling display
    int scroll_pos = 0;
    int ch;
    
    while (1) {
        clear();
        
        // Display header
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(0, 2, "📖 %s - Chapter %d", novel_title, chapter_num);
        attroff(COLOR_PAIR(2) | A_BOLD);

        attron(A_DIM);
        mvhline(1, 0, ACS_HLINE, max_x);
        attroff(A_DIM);
        
        // Display content
        for (int i = 0; i < content_height && (scroll_pos + i) < line_count; i++) {
            mvprintw(header_lines + i, 2, "%s", lines[scroll_pos + i]);
        }
        
        // Display footer
        attron(COLOR_PAIR(4) | A_DIM);
        mvprintw(max_y - 1, 2, "← Prev Page   q Back   ↑/↓ Scroll   Line %d/%d", scroll_pos + 1, line_count > 0 ? line_count : 1);
        attroff(COLOR_PAIR(4) | A_DIM);
        
        refresh();
        
        // Handle input
        ch = getch();
        
        if (ch == 'q' || ch == 'Q' || ch == KEY_LEFT) {
            break;
        } else if (ch == KEY_UP && scroll_pos > 0) {
            scroll_pos--;
        } else if (ch == KEY_DOWN && scroll_pos + content_height < line_count) {
            scroll_pos++;
        } else if (ch == KEY_PPAGE) { // Page Up
            scroll_pos -= content_height;
            if (scroll_pos < 0) scroll_pos = 0;
        } else if (ch == KEY_NPAGE) { // Page Down
            scroll_pos += content_height;
            if (scroll_pos + content_height > line_count) {
                scroll_pos = line_count - content_height;
            }
            if (scroll_pos < 0) scroll_pos = 0;
        } else if (ch == KEY_HOME) {
            scroll_pos = 0;
        } else if (ch == KEY_END) {
            scroll_pos = line_count - content_height;
            if (scroll_pos < 0) scroll_pos = 0;
        }
    }
    
    // Cleanup
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(lines);
    free(content);
    
    return 0;
}