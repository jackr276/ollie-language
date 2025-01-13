# Makefile for OC
CC = gcc
CFLAGS = -Wall -Wextra -c
CFLAGSLINK = -Wall -Wextra
LEX_PATH = ./oc/compiler/lexer
STACK_PATH = ./oc/compiler/stack
SYMTAB_PATH = ./oc/compiler/symtab
PARSER_PATH = ./oc/compiler/parser
OUT = ./oc/out
PROGS = lexer_test symtab_test parser_test


all: $(PROGS)

compiler: compiler.o parser.o lexer.o

ltest: lexer_test
	cat ./oc/test_files/test_files.txt | xargs ./oc/out/lexer_test

lexer_test: lexer.o lexer_test.o
	$(CC) -o $(OUT)/lexer_test $(OUT)/lexer_test.o $(OUT)/lexer.o

lexer_test.o: $(LEX_PATH)/lexer_test.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer_test.c -o $(OUT)/lexer_test.o

lexer.o: $(LEX_PATH)/lexer.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer.c -o $(OUT)/lexer.o

stack.o: $(STACK_PATH)/stack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/stack.c -o $(OUT)/stack.o

symtab.o: $(SYMTAB_PATH)/symtab.c
	$(CC) $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT)/symtab.o

parser.o: $(PARSER_PATH)/parser.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser.c -o $(OUT)/parser.o

symtab_test.o: $(SYMTAB_PATH)/symtab_test.c
	$(CC) $(CFLAGS) $(SYMTAB_PATH)/symtab_test.c -o $(OUT)/symtab_test.o

parser_test.o: $(PARSER_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser_test.c -o $(OUT)/parser_test.o

parser_test: parser.o lexer.o parser_test.o symtab.o stack.o 
	$(CC) -o $(OUT)/parser_test $(OUT)/parser_test.o $(OUT)/parser.o $(OUT)/lexer.o $(OUT)/stack.o $(OUT)/symtab.o

parser_test_debug: parser.o lexer.o parser_test.o symtab.o stack.o 
	$(CC) -g -o $(OUT)/debug $(OUT)/parser_test.o $(OUT)/parser.o $(OUT)/lexer.o $(OUT)/stack.o $(OUT)/symtab.o

symtab_test: symtab.o symtab_test.o lexer.o
	$(CC) -o $(OUT)/symtab_test $(OUT)/symtab_test.o $(OUT)/symtab.o

stest: symtab_test
	$(OUT)/symtab_test

ptest: parser_test
	cat ./oc/test_files/test_files.txt | xargs ./oc/out/parser_test

clean:
	rm -f ./oc/out/*
