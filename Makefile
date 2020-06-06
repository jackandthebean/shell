#
# Student Makefile for CS154 Project 4
#
# This project requires that code compiles cleanly
# (without warnings), hence the -Werror option

myshell: myshell.c
	gcc -Wall -Werror -o myshell myshell.c

clean:
	rm -f myshell
