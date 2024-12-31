# Makefile for OC
CC = gcc
CFLAGS = -Wall -Wextra -c
LEX_PATH = ./oc/compiler/lexer
STACK_PATH = ./oc/compiler/stack
SYMTAB_PATH = ./oc/compiler/symtab
OUT = ./oc/out
PROGS = lexer_test


all: $(PROGS)

compiler: compiler.o parser.o lexer.o

ltest: lexer_test
	$(OUT)/lexer_test ./oc/test_files/lex_test1.ol ./oc/test_files/lex_test2.ol ./oc/test_files/lex_test3.ol ./oc/test_files/lex_test4.ol

lexer_test: lexer.o lexer_test.o
	$(CC) -o $(OUT)/lexer_test $(OUT)/lexer_test.o $(OUT)/lexer.o

lexer_test.o: $(LEX_PATH)/lexer_test.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer_test.c -o $(OUT)/lexer_test.o

lexer.o: $(LEX_PATH)/lexer.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer.c -o $(OUT)/lexer.o

stack.o: $(STACK_PATH)/stack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/stack.c -o $(OUT)/stack.o

symtab.o: $(SYMTAB_PATH)/symtab.c
	$(CC) $(CFLAGS) $(SYMTAB_PATH)/symtb.c -o $(OUT)/symtab.o

clean:
	rm -f ./out/*
