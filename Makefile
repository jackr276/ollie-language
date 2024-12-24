# Makefile for OC
CC = gcc
CFLAGS = -Wall -Wextra -c
LEX_PATH = ./oc/compiler/lexer
OUT = ./oc/out
PROGS = lexer_test


all: $(PROGS)

compiler: compiler.o parser.o lexer.o

lexer_test_run: lexer_test
	$(OUT)/lexer_test ./oc/test_files/lex_test1.ol


lexer_test: lexer.o lexer_test.o
	$(CC) -o $(OUT)/lexer_test $(OUT)/lexer_test.o $(OUT)/lexer.o

lexer_test.o: $(LEX_PATH)/lexer_test.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer_test.c -o $(OUT)/lexer_test.o

lexer.o: $(LEX_PATH)/lexer.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer.c -o $(OUT)/lexer.o

clean:
	rm -f ./out/*
