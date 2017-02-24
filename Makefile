# -----------------------------
# projekt:         barbers
# autor:           Miloslav Ciz
# posledni zmena:  26.4.2011
# popis:
#
# makefile soubor k projektu
# pro FIT VUT
# -----------------------------

barbers:
	gcc -lrt -std=gnu99 -Wall -Wextra -Werror -pedantic -o barbers barbers.c
