#!/usr/bin/env bash
CC="${CC:-cc}"
for f in *.c
 do
  case "$f" in
   umeshview.c)
    "${CC}" -march=native -std=c11 -Os -pipe -Wall -Wextra -Werror -pedantic -D_REENTRANT -pthread -lSDL2 -lm "${f}" -o "bin/${f%%.c}"
    ;;
   *)
    "${CC}" -march=native -std=c11 -Os -pipe -Wall -Wextra -Werror -pedantic "${f}" -o "bin/${f%%.c}"
    ;;
  esac
 done
