# imacs

Have a relase demo of "emacs" for esp8266
- https://github.com/yesco/imacs/releases

Help text, loaded by default

      imacs - possibly the worlds smallest emacs clone!
      (imacs said in Swedish sounds like [eeemacs])
      (c) 2015 Jonas S Karlsson
      It's an one file embeddable device emacs in
      about 200+ lines c code.

      Usage: ./imacs file.txt

      Help
      ====
      char    forward: ^B      back: ^F
      delete  forward: DEL     back: ^D
      pages      down: ^V        up: ESC V
      file        top: ESC <    end: ESC >
      lines      prev: ^P      next: ^N
      lines beginning: ^A       end: ^E
      newline   after: ^O    before: RET

      delete line after cursor: ^K
      exit: ^X^C ^X^Z ^X^F
      redraw: ^L
      eval: ^X^E (it has integration with [esp-lisp](https://github.com/yesco/esp-lisp))

      TAB indents cleverly, newline too
      1. return will create new line and indent like previous line
      2. tab will indent an empty line to the line above
      3. tab pressed anywhere on a line that start with spaces will not
         mess with it, just move cursor to beginning of text (after space)
      4. another press on tab will then indent an additional 2 spaces each time

      Why
      ===
      Support simple editing on embedded systems ala ESP8266 wifi device,
      on speeds like 115200 baud

      Limitations
      ===========
      - vt100 only
      - no undo/redo
      - no way to save a file!
      - can't handle files with longer lines than terminal width
      - tabs... get's funny
      - some things rewrite whole screen!
      - when you move till after the file end, it gets funny (TOFIX)
      - big files should mess it up
      - Not yet: no arrow keys, just as above and letters

# how to build

## linux

just run
      linux> ./imacs

it'll compile and run!

if you use bigger screen, try

      linux> export LINES
      linux> export COLUMNS

then start again, it should use whole screen

## nodemcu esp8266

In a directory:

- Get https://github.com/SuperHouse/esp-open-rtos (and all it wants)
- Build it.

This is temporary; we need to patch in for read/write to get
readline interactive on device!

- (temp) patch it for IO read using uart
  https://github.com/SuperHouse/esp-open-rtos/pull/31
- (temp) instructions on
  https://help.github.com/articles/checking-out-pull-requests-locally/
- (temp) esp-open-rtos> git fetch origin pull/ID/head:uart
- (temp) esp-open-rtos> git checkout uart
- (temp) buid it...

- Get https://github.com/yesco/imacs

These will now be in the same directory

Note: the build needs to do some special stuff for compiling to
a plain `make` will not work.

For the nodemcu instead:

- ./make

  if no errors, flash it:

- make flash

- ./mcu

to connect to it, then press ^L to redraw the screen!

# Related projects

[Femto-Emacs](https://github.com/FemtoEmacs/Femto-Emacs/blob/master/README.original.md)


    Editor         Binary   BinSize     KLOC  Files
    
    imacs          imacs      18808     0.364     1
    femto          femto      43397     2.1k     11
    atto           atto       33002     1.9k     10


