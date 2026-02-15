#ifndef HISTORY_H
#define HISTORY_H

#define MAX_HISTORY 15

typedef struct {
    char novel_title[256];
    char chapter_title[256];
    char chapter_slug[256];
    char novel_slug[256];
    int chapter_num;
} HistoryEntry;

// Saves a new entry, maintaining only the last 15
void save_to_history(const char* n_title, const char* c_title, const char* c_slug, const char* n_slug, int c_num);

// Loads history from disk
int load_history(HistoryEntry history[MAX_HISTORY]);

// Displays the history menu and handles selection
void show_history_menu();

#endif
