# Makefile for OC
CC = gcc
CFLAGS = -Wall -Wextra -c
CFLAGSLINK = -Wall -Wextra
LEX_PATH = ./oc/compiler/lexer
STACK_PATH = ./oc/compiler/stack
SYMTAB_PATH = ./oc/compiler/symtab
PARSER_PATH = ./oc/compiler/parser
TYPE_SYSTEM_PATH = ./oc/compiler/type_system
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

lexerd.o: $(LEX_PATH)/lexer.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer.c -o $(OUT)/lexerd.o

stack.o: $(STACK_PATH)/stack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/stack.c -o $(OUT)/stack.o

stackd.o: $(STACK_PATH)/stack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/stack.c -o $(OUT)/stackd.o

symtab.o: $(SYMTAB_PATH)/symtab.c
	$(CC) $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT)/symtab.o

symtabd.o: $(SYMTAB_PATH)/symtab.c
	$(CC) -g $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT)/symtabd.o

type_system.o: $(TYPE_SYSTEM_PATH)/type_system.c
	$(CC) $(CFLAGS) $(TYPE_SYSTEM_PATH)/type_system.c -o $(OUT)/type_system.o

type_systemd.o: $(TYPE_SYSTEM_PATH)/type_system.c
	$(CC) -g $(CFLAGS) $(TYPE_SYSTEM_PATH)/type_system.c -o $(OUT)/type_systemd.o

parser.o: $(PARSER_PATH)/parser.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser.c -o $(OUT)/parser.o

parserd.o: $(PARSER_PATH)/parser.c
	$(CC) -g $(CFLAGS) $(PARSER_PATH)/parser.c -o $(OUT)/parserd.o

symtab_test.o: $(SYMTAB_PATH)/symtab_test.c
	$(CC) $(CFLAGS) $(SYMTAB_PATH)/symtab_test.c -o $(OUT)/symtab_test.o

symtab_testd.o: $(SYMTAB_PATH)/symtab_test.c
	$(CC) -g $(CFLAGS) $(SYMTAB_PATH)/symtab_test.c -o $(OUT)/symtab_testd.o

parser_test.o: $(PARSER_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser_test.c -o $(OUT)/parser_test.o

parser_testd.o: $(PARSER_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser_test.c -o $(OUT)/parser_testd.o

parser_test: parser.o lexer.o parser_test.o symtab.o stack.o type_system.o
	$(CC) -o $(OUT)/parser_test $(OUT)/parser_test.o $(OUT)/parser.o $(OUT)/lexer.o $(OUT)/stack.o $(OUT)/symtab.o $(OUT)/type_system.o

parser_test_debug: parserd.o lexerd.o parser_testd.o symtabd.o stackd.o type_systemd.o
	$(CC) -g -o $(OUT)/debug $(OUT)/parser_testd.o $(OUT)/parserd.o $(OUT)/lexerd.o $(OUT)/stackd.o $(OUT)/symtabd.o $(OUT)/type_systemd.o

symtab_test: symtab.o symtab_test.o lexer.o type_system.o
	$(CC) -o $(OUT)/symtab_test $(OUT)/lexer.o $(OUT)/symtab_test.o $(OUT)/symtab.o $(OUT)/type_system.o

symtab_testd: symtabd.o symtab_testd.o lexerd.o type_systemd.o
	$(CC) -o $(OUT)/symtab_testd $(OUT)/lexerd.o $(OUT)/symtab_testd.o $(OUT)/symtabd.o $(OUT)/type_systemd.o

stest: symtab_test
	$(OUT)/symtab_test

stestd: symtab_testd
	$(OUT)/symtab_testd

ptest: parser_test
	cat ./oc/test_files/test_files.txt | xargs ./oc/out/parser_test

clean:
	rm -f ./oc/out/*
