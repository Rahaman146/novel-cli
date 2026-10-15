#include "platform.h"
#include "network.h"
#include "chapter_controller.h"
#include "history.h"

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
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
int show_chapter_browser(char chapters[3500][128], char chapter_titles[3500][128], int total, const char* novel_title, const char* novel_slug __attribute__((unused)), int start_idx) {
  int highlight = start_idx; // Initialize to the saved chapter
  int offset = start_idx - 5; // Center the view slightly
  if (offset < 0) offset = 0;
  if (highlight >= total) highlight = total - 1;

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
      case 10: { // Enter key
        int nav_status = 0;
        do {
          char* chapter_html = fetch_chapter_content(chapters[highlight]);

          if (chapter_html && strlen(chapter_html) > 1000) {
            // Get navigation intent from the reader window
            nav_status = display_chapter_content(novel_title, highlight + 1, chapter_html);
            save_to_history(novel_title, chapter_titles[highlight], chapters[highlight], novel_slug, highlight + 1);
            free(chapter_html);

            if (nav_status == 1 && highlight < total - 1) { 
              highlight++; // Move to next chapter
              if (highlight >= offset + CHAPTERS_VISIBLE) offset++;
            } 
            else if (nav_status == -1 && highlight > 0) {
              highlight--; // Move to previous chapter
              if (highlight < offset) offset = highlight;
            }
            else {
              nav_status = 0; // Exit to chapter list
            }
          } else {
            mvprintw(20, 0, "❌ Failed to fetch chapter!");
            refresh(); getch();
            nav_status = 0;
          }
        } while (nav_status != 0); // Keep looping if user navigated
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

// Helper to decode basic HTML entities inline
static void append_decoded_entity(const char **src, char **dst) {
  long val = -1;
  const char *start_p = *src;

  if (strncmp(*src, "&#x", 3) == 0 || strncmp(*src, "&#X", 3) == 0) {
    char *endptr;
    val = strtol(*src + 3, &endptr, 16);
    if (*endptr == ';') *src = endptr;
    else val = -1;
  } else if (strncmp(*src, "&#", 2) == 0 && isdigit((*src)[2])) {
    char *endptr;
    val = strtol(*src + 2, &endptr, 10);
    if (*endptr == ';') *src = endptr;
    else val = -1;
  }

  if (val > 0) {
    if (val <= 0x7F) {
      *(*dst)++ = (char)val;
    } else if (val <= 0x7FF) {
      *(*dst)++ = (char)(0xC0 | ((val >> 6) & 0x1F));
      *(*dst)++ = (char)(0x80 | (val & 0x3F));
    } else if (val <= 0xFFFF) {
      *(*dst)++ = (char)(0xE0 | ((val >> 12) & 0x0F));
      *(*dst)++ = (char)(0x80 | ((val >> 6) & 0x3F));
      *(*dst)++ = (char)(0x80 | (val & 0x3F));
    } else if (val <= 0x10FFFF) {
      *(*dst)++ = (char)(0xF0 | ((val >> 18) & 0x07));
      *(*dst)++ = (char)(0x80 | ((val >> 12) & 0x3F));
      *(*dst)++ = (char)(0x80 | ((val >> 6) & 0x3F));
      *(*dst)++ = (char)(0x80 | (val & 0x3F));
    }
    return;
  }

  if (strncmp(start_p, "&lt;", 4) == 0)    { *src += 3; *(*dst)++ = '<'; return; }
  if (strncmp(start_p, "&gt;", 4) == 0)    { *src += 3; *(*dst)++ = '>'; return; }
  if (strncmp(start_p, "&amp;", 5) == 0)   { *src += 4; *(*dst)++ = '&'; return; }
  if (strncmp(start_p, "&quot;", 6) == 0)  { *src += 5; *(*dst)++ = '"'; return; }
  if (strncmp(start_p, "&apos;", 6) == 0)  { *src += 5; *(*dst)++ = '\''; return; }
  if (strncmp(start_p, "&nbsp;", 6) == 0)  { *src += 5; *(*dst)++ = ' '; return; }
  if (strncmp(start_p, "&mdash;", 7) == 0) { *src += 6; *(*dst)++ = '-'; return; }
  if (strncmp(start_p, "&ndash;", 7) == 0) { *src += 6; *(*dst)++ = '-'; return; }
  if (strncmp(start_p, "&rsquo;", 7) == 0) { *src += 6; *(*dst)++ = '\''; return; }
  if (strncmp(start_p, "&lsquo;", 7) == 0) { *src += 6; *(*dst)++ = '\''; return; }
  if (strncmp(start_p, "&rdquo;", 7) == 0) { *src += 6; *(*dst)++ = '"'; return; }
  if (strncmp(start_p, "&ldquo;", 7) == 0) { *src += 6; *(*dst)++ = '"'; return; }

  *(*dst)++ = **src;
}

char decode_entity(const char **src) {
  char buf[8] = {0};
  char *ptr = buf;
  append_decoded_entity(src, &ptr);
  return buf[0];
}

static int is_boilerplate_line(const char *buf) {
  if (!buf || !*buf) return 0;

  while (*buf && isspace(*buf)) buf++;
  if (!*buf) return 0;

  if (strcasestr(buf, "Read More With Pictures")) return 1;
  if (strcasestr(buf, "20+ Chapters In Advance")) return 1;
  if (strcasestr(buf, "patreon.com")) return 1;
  if (strcasestr(buf, "patreon at")) return 1;
  if (strcasestr(buf, "check out my patreon")) return 1;
  if (strcasestr(buf, "support me than check out my patreon")) return 1;
  if (strcasestr(buf, "If you Like this story")) return 1;
  if (strcasestr(buf, "Check out my other stories")) return 1;
  if (strcasestr(buf, "Solo leveling in Westeros")) return 1;
  if (strcasestr(buf, "Use arrow keys")) return 1;
  if (strcasestr(buf, "PREV/NEXT chapter")) return 1;
  if (strcasestr(buf, "Please read these rules before posting")) return 1;
  if (strcasestr(buf, "submitting a comment, you agree to follow them")) return 1;
  if (strcasestr(buf, "Site administrators and editors may moderate")) return 1;
  if (strcasestr(buf, "Update requests will not make chapters arrive faster")) return 1;
  if (strcasestr(buf, "Do not flood the comment section")) return 1;
  if (strcasestr(buf, "Violations may result in a warning")) return 1;
  if (strcasestr(buf, "commenting privileges")) return 1;
  if (strcasestr(buf, "Access to 3 More Chapters") || strcasestr(buf, "Access to 7 More Chapters") || strcasestr(buf, "More Chapters if you want")) return 1;
  if (strstr(buf, "/*****") || strstr(buf, "*****\\") || strstr(buf, "/***") || strstr(buf, "***\\")) return 1;

  if (strcasestr(buf, "Royal Road") && strcasestr(buf, "home of web novels")) return 1;
  if (strcasestr(buf, "In our amazing community, you can find various talented individuals")) return 1;
  if (strcasestr(buf, "write as a hobby or even professionally")) return 1;
  if (strcasestr(buf, "artists who create art for them")) return 1;
  if (strcasestr(buf, "provide valuable feedback and encouragement")) return 1;
  if (strcasestr(buf, "stolen from Royal Road") || strcasestr(buf, "taken from Royal Road")) return 1;
  if (strncasecmp(buf, "Bio:", 4) == 0 || strncasecmp(buf, "Bio :", 5) == 0) return 1;
  if (strcasestr(buf, "Royal Road") && strcasestr(buf, "Bio:")) return 1;

  return 0;
}

int display_chapter_content(const char* novel_title, int chapter_num, const char* html) {
  if (!html) return 1;

  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);
  int width = max_x - 4; // Margin

  // --- STEP 1: Extract & Clean Text ---
  char *clean_text = calloc(strlen(html) * 2 + 1, 1);
  char *dst = clean_text;

  const char *script_start = strstr(html, "<script id=\"__NEXT_DATA__\" type=\"application/json\">");
  if (script_start) {
    script_start += strlen("<script id=\"__NEXT_DATA__\" type=\"application/json\">");
    const char *script_end = strstr(script_start, "</script>");
    if (script_end) {
      int json_len = script_end - script_start;
      char *json_buf = malloc(json_len + 1);
      if (json_buf) {
        strncpy(json_buf, script_start, json_len);
        json_buf[json_len] = '\0';

        cJSON *root = cJSON_Parse(json_buf);
        if (root) {
          cJSON *props = cJSON_GetObjectItemCaseSensitive(root, "props");
          cJSON *pageProps = cJSON_GetObjectItemCaseSensitive(props, "pageProps");
          cJSON *data = cJSON_GetObjectItemCaseSensitive(pageProps, "data");
          if (!data) data = pageProps;

          cJSON *ch = cJSON_GetObjectItemCaseSensitive(data, "chapter");
          cJSON *paragraphs = ch ? cJSON_GetObjectItemCaseSensitive(ch, "paragraphs") : NULL;

          if (paragraphs && cJSON_IsArray(paragraphs)) {
            int p_size = cJSON_GetArraySize(paragraphs);
            for (int i = 0; i < p_size; i++) {
              cJSON *p_item = cJSON_GetArrayItem(paragraphs, i);
              if (p_item && cJSON_IsString(p_item) && p_item->valuestring) {
                const char *s = p_item->valuestring;
                if (!is_boilerplate_line(s)) {
                  size_t len = strlen(s);
                  memcpy(dst, s, len);
                  dst += len;
                  *dst++ = '\n';
                  *dst++ = '\n';
                }
              }
            }
          }
          cJSON_Delete(root);
        }
        free(json_buf);
      }
    }
  }

  if (dst == clean_text) {
    if (strstr(html, "id=\"chapterText\"")) {
    const char *cursor = html;
    while ((cursor = strstr(cursor, "id=\"chapterText\""))) {
      const char *start = strchr(cursor, '>');
      if (!start) break;
      start++;

      const char *end = strchr(start, '<');
      if (!end) break;

      int space_pending = 0;
      for (const char *p = start; p < end; p++) {
        if (isspace(*p)) {
          if (dst > clean_text) space_pending = 1;
        } else {
          if (space_pending && *(dst-1) != '\n') *dst++ = ' ';
          space_pending = 0;

          if (*p == '&') append_decoded_entity(&p, &dst);
          else *dst++ = *p;
        }
      }

      if (dst > clean_text && *(dst-1) != '\n') {
        *dst++ = '\n'; 
        *dst++ = '\n';
      }
      cursor = end;
    }
  } else {
    const char *containers[] = {
      "chapter-inner chapter-content",
      "class=\"txt \"",
      "class=\"txt\"",
      "id=\"article\"",
      "class=\"entry-content\"",
      NULL
    };

    const char *c_start = NULL;
    for (int ci = 0; containers[ci]; ci++) {
      c_start = strstr(html, containers[ci]);
      if (c_start) break;
    }
    if (!c_start) c_start = html;

    const char *p_cursor = c_start;
    int last_was_boilerplate = 0;

    while (1) {
      const char *p_open = strstr(p_cursor, "<p");
      if (!p_open) break;

      char next_ch = p_open[2];
      if (next_ch != '>' && next_ch != ' ' && next_ch != '\n' && next_ch != '\r') {
        p_cursor = p_open + 2;
        continue;
      }

      const char *gt = strchr(p_open, '>');
      if (!gt) break;
      const char *text_start = gt + 1;

      const char *p_close = strstr(text_start, "</p>");
      if (!p_close) break;

      size_t raw_len = (size_t)(p_close - text_start);
      char *p_buf = malloc(raw_len * 2 + 1);
      if (!p_buf) break;

      char *p_dst = p_buf;
      int space_pending = 0;
      int in_tag = 0;
      const char *p_ptr = text_start;

      while (p_ptr < p_close) {
        if (*p_ptr == '<') {
          in_tag = 1;
        } else if (*p_ptr == '>') {
          in_tag = 0;
        } else if (!in_tag) {
          if (isspace(*p_ptr)) {
            if (p_dst > p_buf && *(p_dst - 1) != ' ') space_pending = 1;
          } else {
            if (space_pending && p_dst > p_buf && *(p_dst - 1) != ' ') *p_dst++ = ' ';
            space_pending = 0;

            if (*p_ptr == '&') append_decoded_entity(&p_ptr, &p_dst);
            else *p_dst++ = *p_ptr;
          }
        }
        p_ptr++;
      }
      *p_dst = '\0';

      int is_bp = is_boilerplate_line(p_buf);
      if (!is_bp) {
        char *t = p_buf;
        while (*t && isspace(*t)) t++;
        if (strncmp(t, "---", 3) == 0 || strncmp(t, "***", 3) == 0 || strncmp(t, "/***", 4) == 0) {
          is_bp = 1;
        } else if (last_was_boilerplate && strcasecmp(t, "AND") == 0) {
          is_bp = 1;
        }
      }

      if (is_bp) {
        last_was_boilerplate = 1;
        if (strcasestr(p_buf, "Use arrow keys") ||
            strcasestr(p_buf, "Please read these rules") ||
            strcasestr(p_buf, "Site administrators") ||
            strcasestr(p_buf, "Do not flood the comment") ||
            strcasestr(p_buf, "Violations may result") ||
            strcasestr(p_buf, "commenting privileges") ||
            strcasestr(p_buf, "Update requests will not make") ||
            strncasecmp(p_buf, "Bio:", 4) == 0 ||
            strncasecmp(p_buf, "Bio :", 5) == 0 ||
            (strcasestr(p_buf, "Royal Road") && strcasestr(p_buf, "home of web novels"))) {
          free(p_buf);
          break;
        }
        free(p_buf);
        p_cursor = p_close + 4;
        continue;
      }

      last_was_boilerplate = 0;
      size_t plen = strlen(p_buf);
      if (plen > 0) {
        memcpy(dst, p_buf, plen);
        dst += plen;
        *dst++ = '\n';
        *dst++ = '\n';
      }
      free(p_buf);

      p_cursor = p_close + 4;
    }
  }
  }
  *dst = '\0';


  // --- STEP 2: Word Wrap into Line List ---
  int line_cap = 1000;
  int n_lines = 0;
  char **lines = malloc(line_cap * sizeof(char*));
  char *ptr = clean_text;

  while (*ptr) {
    if (n_lines >= line_cap) lines = realloc(lines, (line_cap *= 2) * sizeof(char*));

    // Handle paragraph breaks (empty lines)
    if (*ptr == '\n') {
      lines[n_lines++] = strdup(""); 
      ptr++;
      continue;
    }

    // Find where to cut the line
    int len = 0;
    char *line_start = ptr;

    // Greedily grab characters until width or newline
    while (*ptr && *ptr != '\n' && len < width) {
      ptr++;
      len++;
    }

    // Backtrack to the last space if we split a word in the middle
    if (*ptr && *ptr != '\n' && !isspace(*ptr)) {
      int back_steps = 0;
      char *temp = ptr;
      while (back_steps < len && !isspace(*temp)) {
        temp--; back_steps++;
      }
      // If we found a space, cut there. If not (giant word), keep the split.
      if (back_steps < len) {
        ptr = temp;
        len -= back_steps;
      }
    }

    // Copy this line
    lines[n_lines] = malloc(len + 1);
    strncpy(lines[n_lines], line_start, len);
    lines[n_lines][len] = '\0';
    n_lines++;

    // Skip the space/newline we just broke on
    if (isspace(*ptr)) ptr++;
  }

  // --- STEP 3: Display Loop ---
  int scroll = 0;
  int ch = 0;
  int content_h = max_y - 4; // Reserve space for header/footer
  int status = 0;

  while (1) {
    clear();

    int total_lines_disp = n_lines > 0 ? n_lines : 1;
    // Display header
    attron(COLOR_PAIR(4));
    mvprintw(0, 0, "📖 %s - Chapter %d               Line %d/%d", novel_title, chapter_num, scroll + 1, total_lines_disp);
    attroff(COLOR_PAIR(4));

    attron(COLOR_PAIR(5) | A_DIM);
    mvprintw(1, 0, "════════════════════════════════════════════════════════════════════");
    attroff(COLOR_PAIR(5) | A_DIM);

    // Body
    for (int i = 0; i < content_h; i++) {
      int line_idx = scroll + i;
      if (line_idx < n_lines) {
        mvprintw(i + 2, 2, "%s", lines[line_idx]);
      }
    }

    // Display footer
    attron(COLOR_PAIR(5) | A_DIM);
    mvprintw(max_y - 2, 0, "════════════════════════════════════════════════════════════════════");
    attroff(COLOR_PAIR(5) | A_DIM);

    attron(COLOR_PAIR(4));
    mvprintw(max_y - 1, 2, "← Prev Chap   → Next Chap   ↑↓ Scroll   PgUp/PgDn Page ↑↓   q Back");
    attroff(COLOR_PAIR(4));

    refresh();
    ch = getch();

    if (ch == 'q' || ch == 'Q') { status = 0; break; }
    else if (ch == KEY_LEFT) { status = -1; break; }
    else if (ch == KEY_RIGHT) { status = 1; break; }
    else if (ch == KEY_UP && scroll > 0) scroll--;
    else if (ch == KEY_DOWN && scroll < n_lines - content_h) scroll++;
    else if (ch == KEY_PPAGE) scroll = (scroll - content_h > 0) ? scroll - content_h + 2: 0;
    else if (ch == KEY_NPAGE) scroll = (scroll + content_h < n_lines - content_h) ? scroll + content_h - 2: (n_lines - content_h > 0 ? n_lines - content_h : 0);
  }

  // Cleanup
  free(clean_text);
  for (int i = 0; i < n_lines; i++) free(lines[i]);
  free(lines);

  return status;
}
