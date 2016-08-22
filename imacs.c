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

#ifndef BUFF_SIZE
  #ifdef OTA
    #define BUFF_SIZE (4096)
  #else
    #define BUFF_SIZE (1024*24)
  #endif
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
    int dirty;
} imacs_buffer;

char* getpos(imacs_buffer* b, int r, int c) {
    char* p = b->buff;
    char* end = b->end;
    while (r > 0 && *p && p < end) if (*p++ == '\n') r--;
    while (c > 0 && *p && p < end && *p++ != '\n') c--;
    return p < end ? p : (char*)end - 1;
}

static int countLines(imacs_buffer *b) {
    char* p = b->buff;
    int c = 0;
    while (*p && p < b->end) if (*p++ == '\n') c++;
    return c;
}

// TODO: base on pos passed in walk back to newline then forward?
// or walk both directions in two loops and add?
// cols + count to \n!
static int currentLineLength(imacs_buffer* b) {
    char* start = getpos(b, b->row, 0);
    char* p = start;
    while (*p != '\n' && *p && p < b->end) p++;
    return (p - start);
}

static void f() { fflush(stdout); }

// vt100 codes
// http://wiki.bash-hackers.org/scripting/terminalcodes
// ftp://www.columbia.edu/kermit/ckermit/ckmvt1.txt
// http://paulbourke.net/dataformats/ascii/
//
// Scrolling Functions:
//
// *  ESC [ pt ; pb r   set scroll region
// *  ESC [ ? 6 h       turn on region - origin mode
// *  ESC [ ? 6 l       turn off region - full screen mode

//DECSTBM – Set Top and Bottom Margins (DEC Private)

// ESC [ Pn; Pn r    
//
// This sequence sets the top and bottom margins to define the
// scrolling region. The first parameter is the line number of the first
// line in the scrolling region; the second parameter is the line number
// of the bottom line in the scrolling region. Default is the entire
// screen (no margins). The minimum size of the scrolling region allowed
// is two lines, i.e., the top margin must be less than the bottom
// margin. The cursor is placed in the home position (see Origin Mode
// DECOM).

// http://www.termsys.demon.co.uk/vtansi.htm#scroll
// Scrolling
//
// Scroll Screen: <ESC> [ r
//   Enable scrolling for entire display.
// Scroll Screen: <ESC> [ {start} ; {end} r
//   Enable scrolling from row {start} to row {end}.
// Scroll Down: <ESC> D
//   Scroll display down one line.
// Scroll Up: <ESC> M
//   Scroll display up one line.

//                
//          *  ESC D             cursor down - at bottom of region, scroll up
//          *  ESC M             cursor up - at top of region, scroll down
//          *  ESC 7             save cursor position(char attr,char set,org)
//          *  ESC 8             restore position (char attr,char set,origin)
//
// Character Attributes:
// *  ESC [ m           turn off attributes - normal video
// *  ESC [ 0 m         turn off attributes - normal video
// *  ESC [ 4 m         turn on underline mode 
// *  ESC [ 7 m         turn on inverse video mode
// *  ESC [ 1 m         highlight
// *  ESC [ 5 m         blink
//
void clear() { printf("\x1b[2J\x1b[H"); }
static void clearend() { printf("\x1b[K"); }
static void cleareos() { printf("\x1b[J'"); } // TODO: add redraw rest of screen (from pointer)
static void gotorc(int r, int c) { printf("\x1b[%d;%dH", r+1, c+1); }
static void cursoroff() { printf("\x1b[?25l"); }
static void cursoron() { printf("\x1b[?25h"); }
static void inverse(int on) { printf(on ? "\x1b[7m" : "\x1b[m"); }
static void fgcolor(int c) { printf("\x1b[[3%dm", c); } // 0black 1red 2green 3yellow 4blue 5magnenta 6cyan 7white 9default
static void bgcolor(int c) { printf("\x1b[[4%dm", c); } // 0black 1red 2green 3yellow 4blue 5magnenta 6cyan 7white 9default
static void savescreen() { printf("\x1b[?47h"); }
static void restorescreen() { printf("\x1b[?47l"); }
// can use insert characters/line from
// - http://vt100.net/docs/vt102-ug/chapter5.html
static void insertmode(int on) { printf("\x1b[4%c", on ? 'h' : 'l'); }

static void restoreTerminalAndExit(int dummy) {
    clear();
    restorescreen();
    printf("imacs bids you farewell!\n");
#ifndef OTA
    int ignore = system("stty echo");
    ignore += system("stty sane");
    exit(0);
#endif
}

static void error(imacs_buffer* b, char* msg, char* arg) {
    gotorc(b->lines-1, 0);
    printf("%s%s\n", msg, arg); f();
    return;
}

#ifndef NO_MAIN
static int readfile(imacs_buffer* b, char* filename) {
#ifdef OTA
    strncpy((char*)b->buff, (char*)README_md, sizeof(README_md));
    int len = strlen(b->buff) + 1;
#else
    if (!filename) return -1;
    FILE *f = fopen(filename, "r");
    if (!f) return error(b, "No such file: ", filename), -1;
    memset(b->buff, 0, b->size);
    int len = fread(b->buff, b->size - 1, 1, f);
    fclose(f);
#endif
    b->row = 0;
    b->col = 0;
    b->filename = filename;
    return len;
}
#endif

// http://www2.lib.uchicago.edu/keith/tcl-course/emacs-tutorial.html
// Modeline shows:

// - state: modified (**), unmodified (--), or read-only (%%)
// - filename or *scratch*
// - "(mode)"
// - All/Top/Bot/33%
static void print_modeline(imacs_buffer* b) {
    gotorc(b->lines - 2, 0);
    inverse(1);
    int len = strlen(b->buff) + 1;
    int chars = 
        printf("---%s-- %-20s L%d C%d (text) --%d bytes (%d free)-------",
               (b->dirty ? "**" : "--"),
               b->filename, b->row, b->col,
               len, b->size - len);
    while (++chars < b->columns) putchar('-');
    inverse(0);
    f();

    /// resize buffer if needed
    if (b->size - len < 100) {
        char* p = b->buff;
        int sz = b->size * 130 / 100;
        b->buff = realloc(b->buff, sz);
        if (!b->buff) {
            error(b, "Can't reallocate bigger buffer! PANIC!!!", "");
            b->buff = p;
        } else {
            b->size = sz;
        }
    }
}

#ifndef OTA
  #include <unistd.h>

  // simulate slow terminal!
  //#define putchar(c) ({ usleep(100); putchar(c); f(); }) 
#endif

static void redraw(imacs_buffer* b) {
    cursoroff();
    clear();

    // print this first as dark it changes background and will flicker much
    print_modeline(b);

    gotorc(0, 0);
    // print visible text from rows [scroll, scroll+lines-3]
    char* p = getpos(b, b->scroll, 0);
    char* endVisible = getpos(b, b->scroll + b->lines - 3, b->columns);
    while (p < endVisible) putchar(*p++);

    gotorc(b->row - b->scroll, b->col);
    cursoron();
    f();
}

static void update(imacs_buffer* b, int why) {
    static imacs_buffer last;
    static int last_len = -1;
    
    cursoroff();
    int len = strlen(b->buff);
    if (why > 0 || // ctrl-L
        b->filename != last.filename || b->buff != last.buff || // new file
        b->lines != last.lines || b->columns != last.columns || // new terminal size
        b->scroll != last.scroll || // all changed by scrolling
        (why < -10 && len != last_len) || // edit changed massively stuff
        0) {
        redraw(b);
    } if (why < 0) {
        // none, already handled
    } else {
        // redraw current line at least, default
        gotorc(b->row - b->scroll, 0);
        clearend();
        char* p = getpos(b, b->row, 0);
        // TODO: too long line overflow, handle somewhere somehow...
        // TODO: make function use here and in redraw()
        while (*p && *p != '\n' && p < b->end) putchar(*p++);
    }
    print_modeline(b);

    // set cursor
    gotorc(b->row - b->scroll, b->col);
    cursoron();
    f();

    // remember last state to determine what to redraw
    memcpy(&last, b, sizeof(last));
    last_len = strlen(b->buff);
}

static int getch_() {
#ifdef OTA
    // TODO: fix....
    return mygetchar();
    char c;
    int r = 0;
    int lastr = r; char lastc = c;
    while ((r = read(0, (void*)&c, 1)) <= 0) {
        if (lastr != r || lastc != c) {
            printf("[%d %c]", r, c); f();
        }
        lastr = r; lastc = c;
    }
    printf("==========>[%d : %c]", r, c); f();
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

#define ESC 96 // haha! ESC A or M-A!
#define META (ESC+'A'-1)
#define CTRL -64
#define XTRA 256

#define UPCASE(c) ({ int _c = (c); _c >= 'a' ? _c - 32 : _c; })

// returns 'a', 'A', CTRL + 'A', ESC + 'V', etc...
static int getch() {
    int c = getch_();
    if (c == 27) return ESC + UPCASE(getch_());
    if (c == CTRL + 'X') return XTRA + UPCASE(getch_());
    return c;
}

#define GETENV(name, default) ({ char* v = getenv(name); v ? atoi(v) : (default); })

void imacs_init(imacs_buffer* b, char* s, int maxlen) {
    memset(b, 0, sizeof(*b));

    int len = s ? strlen(s)*130/100 + 1 : 0;
    if (BUFF_SIZE > len) len = BUFF_SIZE;
    if (maxlen > len) len = maxlen;

    b->size = len;
    b->buff = (char*) calloc(1, len);
    if (s) strncpy((char*)b->buff, s, strlen(s) + 1);
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

void fix(imacs_buffer* b) {
    int len = currentLineLength(b);

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
}

void error_key(imacs_buffer* b, int c) {
    char key[2] = {0};
    key[0] = (c < 32) ? c + 64 : c - ESC;
    error(b, (c < 32) ? "Key not implemented: ^" : "Key not implemented: ESC ", key);
}

// '[A': 'Up',
// '[B': 'Down',
// '[C': 'Right',
// '[D': 'Left',
// '[F': 'End',
// '[H': 'Pos1',
// '[2~': 'Ins',
// '[3~': 'Del',
// '[5~': 'PgUp',
// '[6~': 'PdDown',

// TODO: ugly...
#ifdef OTA
  #define NO_MAIN
#endif

#ifdef NO_MAIN
int maineditor(imacs_buffer* b) {
#else
int main(int argc, char* argv[]) {
    imacs_buffer* b = alloca(sizeof(imacs_buffer));
    imacs_init(b, NULL, 0);
    if (argc > 1)
        readfile(b, argv[1]);
    else // LOL
    readfile(b, "README.md");
#endif

    int screenfull = b->lines - 3;

    savescreen();
    update(b, 1);
    
    // loop
    int c;
    while ((c = getch()) || 1) {
        int enter = (c == 13 || c == 99 || c == CTRL + 'J');
        int why = 0;
        int len = currentLineLength(b);
        int lines = countLines(b);
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
        else if (c == ESC  + '<') { b->row = 0; b->col = 0; b->scroll = 0; }
        else if (c == ESC  + '>') { b->row = countLines(b); b->scroll = (b->row / screenfull) * screenfull; }
        else if (c == CTRL + 'L') why = 1;
        // modifiers
        #define MOVE(toRel, fromRel) ({ memmove(p + (toRel), p + (fromRel), strlen(p + (fromRel)) + 1); b->dirty++; })
        #define INS(c) ({ MOVE(1, 0); *p = (c); })
        else if (c == CTRL + 'D') MOVE(0, 1);
        else if ((c == CTRL + 'H'  || c == 127) && p > b->buff) { MOVE(-1, 0); b->col--; }
        else if (c == CTRL + 'K') { MOVE(0, len - b->col + (b->col == len)); clearend(); }
        // inserts
        else if (c == CTRL + 'O') INS('\n');
        else if (c == CTRL + 'I' || enter) { // indent
            if (enter) { INS('\n'); b->row++; p++; }
            b->col = 0;
            char* prev = getpos(b, b->row-1, 0);
            char* this = getpos(b, b->row, 0);
            if (*this == '\n') while (*prev++ == ' ') { INS(' '); b->col++; }
            else if (*this == ' ') while (*this++ == ' ') b->col++;
            if (*p != ' ' && *(p-1) == ' ') { INS(' '); INS(' '); b->col += 2; }
        }
        else if (c == CTRL + 'Q') do {
            printf("  [%c %d %d %d] ", c, c, CTRL + 'V', ESC + 'V'); } while (c=getch() != CTRL + 'C');
        else if (c == XTRA + CTRL + 'C') break;
        // lisp interaction TODO: https://www.emacswiki.org/emacs/EvaluatingExpressions
        else if (c == XTRA + CTRL + 'E') break; // eval expression before point
        else if (c == META + ':') break; // enter expression on modeline, eval expression 
        else if (c == XTRA + CTRL + 'Z') break; // suspend
        else if (c == XTRA + CTRL + 'F') break; // find-file
        //
        else if (c == 195); // prefix for M-A..
        else if (c < 32 || c > META) error_key(b, c); // not CTRL / ESC / META
        else { INS(c); b->col++; insertmode(1); putchar(c); insertmode(0); why = -1; }
        #undef MOVE
        #undef INS
        
        fix(b);

        if (lines != countLines(b)) why = 1;

        update(b, why);

        // uncomment to read keys...
        
    }
    return 0;
}

#ifndef NO_MAIN
#ifdef OTA
void user_init(void)
{
    sdk_uart_div_modify(0, UART_CLK_FREQ / 115200);
    
    main(0, NULL);

    // for now run in a task, in order to allocate a bigger stack
    //xTaskCreate(lispTask, (signed char *)"lispTask", 2048, NULL, 2, NULL);
}
#endif
#endif
