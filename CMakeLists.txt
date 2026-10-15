cmake_minimum_required(VERSION 3.10)
project(novel-cli C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.c
    src/ui.c
    src/chapter_controller.c
    src/controller.c
    src/network.c
    src/cache.c
    src/library.c
    src/webnovel.c
    src/history.c
)

add_executable(novel-cli ${SOURCES})
target_include_directories(novel-cli PRIVATE src)

# Compiler warnings
if(MSVC)
    target_compile_options(novel-cli PRIVATE /W4)
else()
    target_compile_options(novel-cli PRIVATE -Wall -Wextra)
endif()

# Find and link CURL
find_package(CURL REQUIRED)
target_link_libraries(novel-cli PRIVATE CURL::libcurl)

# Platform curses handling: PDCurses on Windows, ncurses on Linux/macOS
if(WIN32)
    add_compile_definitions(PDC_WIDE PDC_FORCE_UTF8)
    find_path(PDCURSES_INCLUDE_DIR NAMES pdcurses.h pdcurses/curses.h curses.h PATH_SUFFIXES pdcurses)
    if(PDCURSES_INCLUDE_DIR)
        target_include_directories(novel-cli PRIVATE ${PDCURSES_INCLUDE_DIR})
    endif()

    find_library(PDCURSES_LIB NAMES pdcurses_wincon pdcurses curses)
    if(PDCURSES_LIB)
        target_link_libraries(novel-cli PRIVATE ${PDCURSES_LIB})
    else()
        message(FATAL_ERROR "PDCurses library not found. In MSYS2 install via: pacman -S mingw-w64-x86_64-pdcurses")
    endif()

    target_link_libraries(novel-cli PRIVATE ws2_32 winmm)
else()
    find_package(Curses REQUIRED)
    target_include_directories(novel-cli PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(novel-cli PRIVATE ${CURSES_LIBRARIES})
endif()

# Find and link cJSON
find_package(cJSON QUIET)
if(TARGET cjson)
    target_link_libraries(novel-cli PRIVATE cjson)
else()
    find_library(CJSON_LIB NAMES cjson)
    if(CJSON_LIB)
        target_link_libraries(novel-cli PRIVATE ${CJSON_LIB})
    else()
        message(FATAL_ERROR "cJSON library not found. Please install cjson.")
    endif()
endif()

# Installation
if(NOT WIN32)
    include(GNUInstallDirs)
    install(TARGETS novel-cli DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
