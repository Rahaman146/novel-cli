#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32

  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  // Must include winsock2 before windows.h to prevent winsock 1.x conflicts
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>

  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <ctype.h>
  #include <direct.h>
  #include <io.h>
  #include <sys/stat.h>

  // Undefine conflicting Windows SDK macros before curses
  #ifdef MOUSE_MOVED
    #undef MOUSE_MOVED
  #endif

  // MSYS2 / MinGW PDCursesMod ABI compatibility flags
  #ifndef PDC_WIDE
    #define PDC_WIDE 1
  #endif
  #ifndef PDC_FORCE_UTF8
    #define PDC_FORCE_UTF8 1
  #endif

  // Curses header detection for Windows (PDCurses / PDCursesMod)
  #if defined(__has_include)
    #if __has_include(<pdcurses.h>)
      #include <pdcurses.h>
    #elif __has_include(<pdcurses/curses.h>)
      #include <pdcurses/curses.h>
    #elif __has_include(<curses.h>)
      #include <curses.h>
    #elif __has_include(<ncurses/ncurses.h>)
      #include <ncurses/ncurses.h>
    #elif __has_include(<ncursesw/ncurses.h>)
      #include <ncursesw/ncurses.h>
    #else
      #include <pdcurses.h>
    #endif
  #else
    #include <pdcurses.h>
  #endif

  // Case-insensitive string comparisons
  #if defined(_MSC_VER)
    #ifndef strcasecmp
      #define strcasecmp _stricmp
    #endif
    #ifndef strncasecmp
      #define strncasecmp _strnicmp
    #endif
    #ifndef strdup
      #define strdup _strdup
    #endif
  #endif

  // Path limits
  #ifndef PATH_MAX
    #ifdef _MAX_PATH
      #define PATH_MAX _MAX_PATH
    #else
      #define PATH_MAX 4096
    #endif
  #endif

  // POSIX file mode macros if missing
  #ifndef S_ISREG
    #if defined(_S_IFMT) && defined(_S_IFREG)
      #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #elif defined(S_IFMT) && defined(S_IFREG)
      #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #else
      #define S_ISREG(m) 1
    #endif
  #endif

  // Portable strcasestr implementation
  static inline char *portable_strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
      if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
          h++;
          n++;
        }
        if (!*n) return (char *)haystack;
      }
    }
    return NULL;
  }

  #ifndef strcasestr
    #define strcasestr portable_strcasestr
  #endif

  // Console initialization for UTF-8 and ANSI sequences on Windows
  static inline void platform_init(void) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
      DWORD dwMode = 0;
      if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        SetConsoleMode(hOut, dwMode);
      }
    }
  }

#else // Non-Windows (Linux, macOS, BSD)

  #ifndef _GNU_SOURCE
    #define _GNU_SOURCE
  #endif
  #ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
  #endif

  #include <ncurses.h>
  #include <unistd.h>
  #include <sys/stat.h>
  #include <strings.h>
  #include <limits.h>

  static inline void platform_init(void) {
    // No-op on POSIX systems
  }

#endif // _WIN32

#endif // PLATFORM_H
