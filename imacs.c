// Distributed under Mozilla Public Licence 2.0
// https://www.mozilla.org/en-US/MPL/2.0/
// (c) 2015 Jonas S Karlsson
// imacs - possibly the worlds smallest emacs clone!

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

#define BUFF_SIZE (1024*24)
int buff_size = BUFF_SIZE;
char buff[BUFF_SIZE] =
    "imacs - possibly the worlds smallest emacs clone!\n"
    "(imacs said in Swedish sounds like [eeemacs])\n"
    "(c) 2015 Jonas S Karlsson\n"
    "\n"
    "Usage: ./imacs file.txt\n"
    "\n"
    "Help\n"
    "====\n"
    "lines: ^A, ^P, ^N, ^K, ^E\n"
    "chars: ^B, ^F, ^D, DEL/^H\n"
    "exit: ^C\n"
    "\n"
    "Limitations\n"
    "===========\n"
    "- can't handle files with longer lines that terminal width, or more lines than terminal\n"
    "- tabs... get's funny\n"
    "- vt100 only\n"
    "- each keystroke rewrite the screen!\n"
    "\n"
    "Why\n"
    "===\n"
    "Support simple editing on embedded systems ala ESP8266 wifi device, 115200 baud\n"
    ;

char* buff_end = buff + BUFF_SIZE;

int row = 0;
int col = 0;

int lines, columns;
char* filename = NULL;

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
void clear() { printf("\x1b[2J\x1b[H%"); }
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
    system("stty echo");
    system("stty sane");
    exit(0);
}

void error(char* msg, char* arg) {
    clear(); f();
    inverse(1);
    printf("%s %s\n", msg, arg);
    inverse(0);
    exit(1);
    // TODO:
}

void readfile(char* filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return error("No such file: ", filename);
    memset(buff, 0, buff_size);
    int len = fread(buff, buff_size-1, 1, f);
    fclose(f);
}

void update() {
//    int n = 100; while(n--) printf("\n");
    clear(); printf("%s", buff); printf("\n\n\n"); f();
    gotorc(row, col);
}

int getch()
{
    struct termios old;
    struct termios tmp;

    if (tcgetattr(STDIN_FILENO, &old)) return -1;
    memcpy(&tmp, &old, sizeof(old));
    tmp.c_lflag &= ~ICANON & ~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, (const struct termios*) &tmp)) return -1;

    int c = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, (const struct termios*) &old);

    return c;
}

#define GETENV(name, default) ({ char* v = getenv(name); v ? atoi(v) : (default); })

int main(int argc, char* argv[]) {
    // init
    lines = GETENV("LINES", 24);
    columns = GETENV("COLUMNS", 80);

    signal(SIGINT, restoreTerminalAndExit);

    if (argc > 1) filename = argv[1];
    if (filename) readfile(filename);

    // loop
    update();
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
        else if (c == 10) { insert(c); col = 0; row++; }
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
            if (col < 0) col = 0;
            if (row < 0) row = 0;
            if (row > lines-3) row = lines-3;
            len = currentLineLength();
        } while (!(r == row && c == col && l == len));
        if (col > len) { exit(42); col = 0; row++; len = -1; }

        update();
        gotorc(lines-2, 0);
        inverse(1);
        printf("-- imacs --**-- %-20s          L%d C%d (text) -------------------------------------", filename, row, col, len);
        inverse(0);
        gotorc(row, col);

        //printf("\m\m [%d]%c%c\n", c, *p, *p);
    }
}
