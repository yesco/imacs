# imacs

Help text, loaded by default

      imacs - possibly the worlds smallest emacs clone!
      (imacs said in Swedish sounds like [eeemacs])
      (c) 2015 Jonas S Karlsson

      Usage: ./imacs file.txt

      Help
      ====
      lines: ^A, ^P, ^N, ^K, ^E
      chars: ^B, ^F, ^D, DEL/^H
      exit: ^C

      Limitations
      ===========
      - no way to save a file!
      - can't handle files with longer lines that terminal width, or more lines than terminal
      - tabs... get's funny
      - vt100 only
      - each keystroke rewrite the screen!
      - when you move till after the file end, it gets funny

      Why
      ===
      Support simple editing on embedded systems ala ESP8266 wifi device, 115200 baud
