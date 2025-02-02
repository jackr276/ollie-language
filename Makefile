# Makefile for OC
CC = gcc
CFLAGS = -Wall -Wextra -c
CFLAGSLINK = -Wall -Wextra
LEX_PATH = ./oc/compiler/lexer
STACK_PATH = ./oc/compiler/stack
SYMTAB_PATH = ./oc/compiler/symtab
PARSER_PATH = ./oc/compiler/parser
TYPE_SYSTEM_PATH = ./oc/compiler/type_system
CALL_GRAPH_PATH = ./oc/compiler/call_graph
AST_PATH = ./oc/compiler/ast
CFG_PATH = ./oc/compiler/cfg
TEST_FILE_DIR = ./oc/test_files/
OUT = ./oc/out
PROGS = lexer_test symtab_test parser_test oc

all: $(PROGS)

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

heapstack.o: $(STACK_PATH)/heapstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/heapstack.c -o $(OUT)/heapstack.o

heapstackd.o: $(STACK_PATH)/heapstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/heapstack.c -o $(OUT)/heapstackd.o

lexstack.o: $(STACK_PATH)/lexstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/lexstack.c -o $(OUT)/lexstack.o

lexstackd.o: $(STACK_PATH)/lexstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/lexstack.c -o $(OUT)/lexstackd.o

ast.o: $(AST_PATH)/ast.c
	$(CC) $(CFLAGS) $(AST_PATH)/ast.c -o $(OUT)/ast.o

astd.o: $(AST_PATH)/ast.c
	$(CC) -g $(CFLAGS) $(AST_PATH)/ast.c -o $(OUT)/astd.o

symtab.o: $(SYMTAB_PATH)/symtab.c
	$(CC) $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT)/symtab.o

symtabd.o: $(SYMTAB_PATH)/symtab.c
	$(CC) -g $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT)/symtabd.o

cfg.o: $(CFG_PATH)/cfg.c
	$(CC) $(CFLAGS) $(CFG_PATH)/cfg.c -o $(OUT)/cfg.o

cfgd.o: $(CFG_PATH)/cfg.c
	$(CC) -g $(CFLAGS) $(CFG_PATH)/cfg.c -o $(OUT)/cfgd.o

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

parser_test: parser.o lexer.o parser_test.o symtab.o lexstack.o heapstack.o type_system.o ast.o cfg.o call_graph.o
	$(CC) -o $(OUT)/parser_test $(OUT)/parser_test.o $(OUT)/parser.o $(OUT)/lexstack.o $(OUT)/lexer.o $(OUT)/heapstack.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/ast.o $(OUT)/cfg.o $(OUT)/call_graph.o

parser_test_debug: parserd.o lexerd.o parser_testd.o symtabd.o lexstack.o heapstackd.o type_systemd.o astd.o cfgd.o call_graphd.o
	$(CC) -g -o $(OUT)/debug $(OUT)/parser_testd.o $(OUT)/parserd.o $(OUT)/lexstackd.o $(OUT)/lexerd.o $(OUT)/heapstackd.o $(OUT)/symtabd.o $(OUT)/type_systemd.o $(OUT)/astd.o $(OUT)/cfgd.o $(OUT)/call_graphd.o

symtab_test: symtab.o symtab_test.o lexer.o type_system.o
	$(CC) -o $(OUT)/symtab_test $(OUT)/lexer.o $(OUT)/symtab_test.o $(OUT)/symtab.o $(OUT)/type_system.o

symtab_testd: symtabd.o symtab_testd.o lexerd.o type_systemd.o
	$(CC) -o $(OUT)/symtab_testd $(OUT)/lexerd.o $(OUT)/symtab_testd.o $(OUT)/symtabd.o $(OUT)/type_systemd.o

call_graph.o : $(CALL_GRAPH_PATH)/call_graph.c
	$(CC) $(CFLAGS) $(CALL_GRAPH_PATH)/call_graph.c -o $(OUT)/call_graph.o

call_graphd.o : $(CALL_GRAPH_PATH)/call_graph.c
	$(CC) -g $(CFLAGS) $(CALL_GRAPH_PATH)/call_graph.c -o $(OUT)/call_graphd.o

compiler.o: ./oc/compiler/compiler.c 
	$(CC) $(CFLAGS) -o $(OUT)/compiler.o ./oc/compiler/compiler.c

compilerd.o: ./oc/compiler/compiler.c 
	$(CC) $(CFLAGS) -g -o $(OUT)/compilerd.o ./oc/compiler/compiler.c

oc: compiler.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o
	$(CC) -o $(OUT)/oc $(OUT)/compiler.o $(OUT)/parser.o $(OUT)/lexer.o $(OUT)/heapstack.o $(OUT)/lexstack.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/ast.o $(OUT)/cfg.o $(OUT)/call_graph.o

oc_debug: compilerd.o parserd.o lexerd.o symtabd.o heapstackd.o type_systemd.o astd.o cfgd.o call_graphd.o lexstackd.o
	$(CC) -o $(OUT)/ocd $(OUT)/compilerd.o $(OUT)/parserd.o $(OUT)/lexerd.o $(OUT)/heapstackd.o $(OUT)/symtabd.o $(OUT)/lexstackd.o $(OUT)/type_systemd.o $(OUT)/astd.o $(OUT)/cfgd.o $(OUT)/call_graphd.o

stest: symtab_test
	$(OUT)/symtab_test

stestd: symtab_testd
	$(OUT)/symtab_testd

ptest: parser_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/parser_test

compiler_test: oc
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/oc

clean:
	rm -f ./oc/out/*
