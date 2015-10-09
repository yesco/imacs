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
      exit: ^C
      redraw: ^L

      Limitations
      ===========
      - no arrow keys, just as above and letters
      - no way to save a file!
      - no undo/redo
      - can't handle files with longer lines than terminal width
      - tabs... get's funny
      - vt100 only
      - each keystroke rewrite the screen!
      - when you move till after the file end, it gets funny
      - big files should mess it up

      Why
      ===
      Support simple editing on embedded systems ala ESP8266 wifi device,
      115200 baud

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




