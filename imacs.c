// Distributed under Mozilla Public Licence 2.0
// https://www.mozilla.org/en-US/MPL/2.0/
// (c) 2015 Jonas S Karlsson
// imacs - possibly the worlds smallest emacs clone!

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#ifdef OTA
  #include "espressif/esp_common.h"
  #include "espressif/sdk_private.h"
  #include "FreeRTOS.h"
  #include "task.h"
  #include "queue.h"
#else
  #include <termios.h>
#endif

// TODO: refactor to create a struct with all the states of a buffer?

#ifdef OTA
  #define BUFF_SIZE (2048)
#else
  #define BUFF_SIZE (1024*24)
#endif

typedef struct {
    char* buff;
    char* end;
    int size;

    int row;
    int col;
    int scroll;
    char* filename;
    
    int lines;
    int columns;
} imacs_buffer;

imacs_buffer main_buff = {
    .size = BUFF_SIZE,
    .buff = NULL,
    .end = NULL,
    .filename =  "README.md",
    .row = 0,
    .col = 0,
    .scroll = 0,
    .lines = 24,
    .columns = 80,
};

char* getpos(imacs_buffer* b, int r, int c) {
    char* p = b->buff;
    char* end = b->end;
    while (r > 0 && *p && p < end) if (*p++ == '\n') r--;
    while (c > 0 && *p && p < end && *p++ != '\n') c--;
    return p < end ? p : (char*)end - 1;
}

// TODO: base on pos passed in walk back to newline then forward?
// or walk both directions in two loops and add?
// cols + count to \n!
int currentLineLength(imacs_buffer* b) {
    char* start = getpos(b, b->row, 0);
    char* p = start;
    while (*p != '\n' && *p && p < b->end) p++;
    return (p - start);
}

void f() { fflush(stdout); }

// http://wiki.bash-hackers.org/scripting/terminalcodes
// http://paulbourke.net/dataformats/ascii/
void clear() { printf("\x1b[2J\x1b[H"); }
void gotorc(int r, int c) { printf("\x1b[%d;%dH", r+1, c+1); }
void inverse(int on) { printf(on ? "\x1b[7m" : "\x1b[m"); }

void restoreTerminalAndExit(int dummy) {
    clear();
    printf("imacs bids you farewell!\n");
#ifndef OTA
    system("stty echo");
    system("stty sane");
    exit(0);
#endif
}

void error(char* msg, char* arg) {
    clear(); f();
    inverse(1);
    printf("%s %s\n", msg, arg);
    inverse(0);
    #ifdef OTA
    return;
    #else
    exit(1);
    #endif
}

int readfile(imacs_buffer* b, char* filename) {
#ifdef OTA
    strncpy((char*)b->buff, (char*)README_md, sizeof(README_md));
    int len = strlen(b->buff) + 1;
#else
    if (!filename) return -1;
    FILE *f = fopen(filename, "r");
    if (!f) return error("No such file: ", filename), -1;
    memset(b->buff, 0, b->size);
    int len = fread(b->buff, b->size - 1, 1, f);
    fclose(f);
#endif
    b->row = 0;
    b->col = 0;
    b->filename = filename;
    return len;
}

void print_modeline(imacs_buffer* b) {
    gotorc(b->lines - 2, 0);
    inverse(1);
    printf("-- imacs --**-- %-20s L%d C%d (text) --------------------------",
           b->filename, b->row, b->col);
    inverse(0);
    f();
}

#ifndef OTA
  #include <unistd.h>

  // simulate slow terminal!
  #define putchar(c) ({ usleep(100); putchar(c); f(); }) 
#endif

static void redraw(imacs_buffer* b) {
    clear();

    // print this first as dark it changes background and will flicker much
    print_modeline(b);

    gotorc(0, 0);
    // print visible text from rows [scroll, scroll+lines-3]
    char* p = getpos(b, b->scroll, 0);
    char* endVisible = getpos(b, b->scroll + b->lines - 3, b->columns);
    while (p < endVisible) putchar(*p++);

    gotorc(b->row - b->scroll, b->col);
    f();
}

void update(imacs_buffer* b, int all) {
    static imacs_buffer last;
    static int last_len = -1;
    
    int len = strlen(b->buff);
    if (all || // ctrl-L
        len != last_len || b->filename != last.filename || b->buff != last.buff || // new file
        b->lines != last.lines || b->columns != last.columns || // new terminal size
        b->scroll != last.scroll) {
        redraw(b);
    }

    // set cursor
    gotorc(b->row - b->scroll, b->col);
    f();

    // remember last state to determine what to redraw
    memcpy(&last, b, sizeof(last));
    last_len = strlen(b->buff);
}

int getch_() {
#ifdef OTA
    char c;
    read(0, (void*)&c, 1);
#else
    struct termios old;
    struct termios tmp;

    if (tcgetattr(STDIN_FILENO, &old)) return -1;
    memcpy(&tmp, &old, sizeof(old));
    tmp.c_lflag &= ~ICANON & ~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, (const struct termios*) &tmp)) return -1;

    int c = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, (const struct termios*) &old);
#endif
    return c;
}

#define ESC 1032
#define CTRL -64

// returns 'a', 'A', CTRL + 'A', ESC + 'V', etc...
int getch() {
    int c = getch_();
    if (c == 27) ESC + getch_();
    return c;
}

#define GETENV(name, default) ({ char* v = getenv(name); v ? atoi(v) : (default); })

void imacs_init(imacs_buffer* b) {
    memset(b, 0, sizeof(*b));
    b->size = BUFF_SIZE;
    b->buff = (char*) calloc(1, BUFF_SIZE);
    b->end = b->buff + b->size;
    b->lines = 24;
    b->columns = 80;

#ifndef OTA
    // system specific
    b->lines = GETENV("LINES", 24);
    b->columns = GETENV("COLUMNS", 80);
    signal(SIGINT, restoreTerminalAndExit);
#endif
}

int main(int argc, char* argv[]) {
    imacs_buffer* b = alloca(sizeof(imacs_buffer));
    imacs_init(b);
#ifndef OTA
    if (argc > 1)
        readfile(b, argv[1]);
    else // LOL
#endif
    readfile(b, "README.md");
    int screenfull = b->lines - 3;

    // loop
    update(b, 1);
    int c;
    //while (read(0, &c, 1) > 0) { //esp?
    while ((c = getch()) || 1) {
        int len = currentLineLength(b);
        char* p = getpos(b, b->row, b->col);
        if (0) ;
        else if (c == CTRL + 'P') b->row--;
        else if (c == CTRL + 'N') b->row++;
        else if (c == CTRL + 'F') b->col++;
        else if (c == CTRL + 'B') b->col--;
        else if (c == CTRL + 'A') b->col = 0;
        else if (c == CTRL + 'E') b->col = len;
        else if (c == CTRL + 'V') { b->row += screenfull; b->scroll += screenfull; }
        else if (c == ESC  + 'V') { b->row -= screenfull; b->scroll -= screenfull; }
        else if (c == CTRL + 'L') update(b, 1);
        // modifiers
        #define MOVE(toRel, fromRel) memmove(p + (toRel), p + (fromRel), strlen(p + (fromRel)) + 1)
        else if (c == CTRL + 'D') MOVE(0, 1);
        else if (c == CTRL + 'H' && p > b->buff) { MOVE(-1, 0); b->col--; }
        else if (c == CTRL + 'K') MOVE(0, len - b->col + (b->col == len));
        // inserts
        else if (c == 10 || c == CTRL + 'J') { memmove(p + 1, p, strlen(p) + 1); *p = '\n'; b->col = 0; b->row++; }
        else { MOVE(1, 0); *p = c; b->col++; }
        #undef MOVE
        
        len = currentLineLength(b);

        // fix out of bounds relative to content and screen, loop till no change
        int r, c, l;
        do {
            r = b->row; c = b->col; l = len;
            if (b->col > len) { b->col = 0; b->row++; }
            if (b->col < 0 && b->row) { b->row--; b->col = currentLineLength(b); }
            if (b->row < b->scroll) b->scroll--;
            if (b->col < 0) b->col = 0;
            if (b->row < 0) b->row = 0;
            if (b->scroll < 0) b->scroll = 0;
            if (b->row - b->scroll > b->lines - 3) b->scroll++;
            len = currentLineLength(b);
        } while (!(r == b->row && c == b->col && l == len));
        if (b->col > len) { b->col = 0; b->row++; len = -1; } // never get called?

        update(b, 0);

        //printf("\m\m [%d]%c%c\n", c, *p, *p); // enable to read keyboard codes
    }
}

#ifdef OTA
void user_init(void)
{
    sdk_uart_div_modify(0, UART_CLK_FREQ / 115200);
    
    main(0, NULL);

    // for now run in a task, in order to allocate a bigger stack
    //xTaskCreate(lispTask, (signed char *)"lispTask", 2048, NULL, 2, NULL);
}
#endif
