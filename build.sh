#!/usr/bin/env bash
CC="${CC:-cc}"
PKGCONF=${PKGCONF:-pkg-config}
for f in *.c
 do
  case "$f" in
   umeshview.c)
    "${CC}" -march=native -std=c11 -Os -pipe -Wall -Wextra -Werror -pedantic -D__USE_MINGW_ANSI_STDIO "${f}" $("${PKGCONF}" --cflags --libs sdl2 SDL2_image epoxy) -lm -o "bin/${f%%.c}"
    ;;
   *)
    "${CC}" -march=native -std=c11 -Os -pipe -Wall -Wextra -Werror -pedantic -D__USE_MINGW_ANSI_STDIO "${f}" -o "bin/${f%%.c}"
    ;;
  esac
 done
