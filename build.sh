#!/usr/bin/env bash
CC="${CC:-cc}"
for f in *.c
 do
  "${CC}" -march=native -std=c11 -Os -pipe -Wall -Wextra -Werror -pedantic "${f}" -o "bin/${f%%.c}"
 done
