#!/usr/bin/env bash
gcc -g -o triangle.o triangle.c -c
gcc -g -o patric_handout.o patric_handout.c -c -pthread -Wall -Werror
gcc -g -o patric.out triangle.o patric_handout.o -pthread -Wall -Werror