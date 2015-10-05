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

int buff_size = BUFF_SIZE;
char* buff = NULL;
char* buff_end = NULL;

int row = 0;
int col = 0;
int scroll = 0;

char* filename = "README.md";

int lines = 24;
int columns = 80;

char* getpos(int r, int c) {
    char* p = buff;
    while (r > 0 && *p && p < buff_end) if (*p++ == '\n') r--;
    while (c > 0 && *p && p < buff_end && *p++ != '\n') c--;
    return p < buff_end ? p : (char*)buff_end-1;
}

int currentLineLength() {
    char* start = getpos(row, 0);
    char* p = start;
    while (*p != '\n' && *p && p < buff_end) p++;
    return (p - start);
}

void f() { fflush(stdout); }

// http://wiki.bash-hackers.org/scripting/terminalcodes
// http://paulbourke.net/dataformats/ascii/
void clear() { printf("\x1b[2J\x1b[H"); }
void gotorc(int r, int c) { printf("\x1b[%d;%dH", r+1, c+1); }
void inverse(int on) { printf(on ? "\x1b[7m" : "\x1b[m"); }
void insert(char c) {
    char* from = getpos(row, col);
    memmove(from+1, from, strlen(from)+1); // buff+len-from ?
    *from = c;

    // TODO: make function and use for ^F ?
    col++;
    if (col > columns) {
        col = 0;
        row++;
    }
}

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
    // TODO:
}

int readfile(char* filename) {
#ifdef OTA
    //strcpy(buff, "a file\nfor\nyou to\nedit!\n");
    strncpy((char*)buff, (char*)README_md, sizeof(README_md));
    return strlen(buff)+1;
#else
    FILE *f = fopen(filename, "r");
    if (!f) return error("No such file: ", filename), -1;
    memset(buff, 0, buff_size);
    int len = fread(buff, buff_size-1, 1, f);
    fclose(f);
    return len;
#endif
}

void print_modeline(char* filename, int row, int col) {
    gotorc(lines-2, 0);
    inverse(1);
    printf("-- imacs --**-- %-20s L%d C%d (text) --------------------------", filename, row, col);
    inverse(0);
    f();
}

void update(char* filename, int row, int col, int scroll) {
    clear(); // maybe can clear only edit area?

    print_modeline(filename, row, col);

    gotorc(0, 0);
    // print visible text from rows [scroll, scroll+lines-3]
    char* p = getpos(scroll, 0);
    char* end = getpos(scroll+lines-3, columns);
    while (p < end) putchar(*p++);

    // set cursor
    gotorc(row - scroll, col); f();
}

int getch()
{
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

#define GETENV(name, default) ({ char* v = getenv(name); v ? atoi(v) : (default); })

int main(int argc, char* argv[]) {
#ifndef OTA
    filename = argc > 1 ? argv[1] : "README.md";

    // system specific
    lines = GETENV("LINES", 24);
    columns = GETENV("COLUMNS", 80);
    signal(SIGINT, restoreTerminalAndExit);
#endif
    // init
    buff_size = BUFF_SIZE;
    buff = (char*) calloc(1, BUFF_SIZE);
    buff_end = buff + buff_size;

    if (filename) readfile(filename);

    // loop
    update(filename, row, col, scroll);
    int c;
    //while (read(0, &c, 1) > 0) { //esp?
    while ((c = getch()) > 0) {
        int len = currentLineLength();
        char* p = getpos(row, col);
        if (0) ;
        else if (c == 'P' - 64) row--;
        else if (c == 'N' - 64) row++;
        else if (c == 'F' - 64) col++;
        else if (c == 'B' - 64) col--;
        else if (c == 'A' - 64) col = 0;
        else if (c == 'E' - 64) col = len;
        else if (c == 10 || c == 'J' - 64) { insert('J' - 64); col = 0; row++; }
        else if (c == 12) ; // redraw
        else if (c == 'D' - 64) { memmove(p, p+1, strlen(p+1)+1); }
        else if (c == 'H' - 64 && p > buff) { memmove(p-1, p, strlen(p)+1); col--; }
        else if (c == 'K' - 64) { char* from = p-col+len+(col==len); memmove(p, from, strlen(from)+1); } 
        else
            insert(c);

        int r, c, l;
        do {
            r = row; c = col; l = len;
            if (col > len) { col = 0; row++; }
            if (col < 0 && row) { row--; col = currentLineLength(); }
            if (row < scroll) scroll--;
            if (col < 0) col = 0;
            if (row < 0) row = 0;
            if (scroll < 0) scroll = 0;
            if (row-scroll > lines-3) scroll++;
            len = currentLineLength();
        } while (!(r == row && c == col && l == len));
        if (col > len) { col = 0; row++; len = -1; } // never get called?

        update(filename, row, col, scroll);

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
