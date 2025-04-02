# Makefile for OC
CC = gcc
CFLAGS = -Wall -Wextra -c -Wimplicit-fallthrough=0
CFLAGSLINK = -Wall -Wextra
LEX_PATH = ./oc/compiler/lexer
STACK_PATH = ./oc/compiler/stack
SYMTAB_PATH = ./oc/compiler/symtab
PARSER_PATH = ./oc/compiler/parser
TYPE_SYSTEM_PATH = ./oc/compiler/type_system
CALL_GRAPH_PATH = ./oc/compiler/call_graph
AST_PATH = ./oc/compiler/ast
CFG_PATH = ./oc/compiler/cfg
PREPROC_PATH = ./oc/compiler/preprocessor
DEPENDENCY_ANALYZER_PATH = ./oc/compiler/dependency_analyzer
DYNAMIC_ARRAY_PATH = ./oc/compiler/dynamic_array
THREE_ADDRESS_CODE_PATH = ./oc/compiler/three_addr_code
QUEUE_PATH = ./oc/compiler/queue
TEST_FILE_DIR = ./oc/test_files/
OUT = ./oc/out
PROGS = lexer_test symtab_test parser_test oc

all: $(PROGS)

ltest: lexer_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/lexer_test

lexer_test: lexer.o lexer_test.o lexstack.o
	$(CC) -o $(OUT)/lexer_test $(OUT)/lexer_test.o $(OUT)/lexer.o $(OUT)/lexstack.o

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

heap_queue.o: $(QUEUE_PATH)/heap_queue.c
	$(CC) $(CFLAGS) $(QUEUE_PATH)/heap_queue.c -o $(OUT)/heap_queue.o

heap_queued.o: $(QUEUE_PATH)/heap_queue.c
	$(CC) $(CFLAGS) -g $(QUEUE_PATH)/heap_queue.c -o $(OUT)/heap_queued.o

dynamic_array.o: $(DYNAMIC_ARRAY_PATH)/dynamic_array.c
	$(CC) $(CFLAGS) $(DYNAMIC_ARRAY_PATH)/dynamic_array.c -o $(OUT)/dynamic_array.o

dynamic_arrayd.o: $(DYNAMIC_ARRAY_PATH)/dynamic_array.c
	$(CC) $(CFLAGS) -g $(DYNAMIC_ARRAY_PATH)/dynamic_array.c -o $(OUT)/dynamic_arrayd.o

priority_queue.o: $(QUEUE_PATH)/priority_queue.c
	$(CC) $(CFLAGS) $(QUEUE_PATH)/priority_queue.c -o $(OUT)/priority_queue.o

priority_queued.o: $(QUEUE_PATH)/priority_queue.c
	$(CC) $(CFLAGS) -g $(QUEUE_PATH)/priority_queue.c -o $(OUT)/priority_queued.o

lexstack.o: $(STACK_PATH)/lexstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/lexstack.c -o $(OUT)/lexstack.o

lexstackd.o: $(STACK_PATH)/lexstack.c
	$(CC) $(CFLAGS) -g $(STACK_PATH)/lexstack.c -o $(OUT)/lexstackd.o

lightstack.o: $(STACK_PATH)/lightstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/lightstack.c -o $(OUT)/lightstack.o

lightstackd.o: $(STACK_PATH)/lightstack.c
	$(CC) $(CFLAGS) -g $(STACK_PATH)/lightstack.c -o $(OUT)/lightstackd.o

ast.o: $(AST_PATH)/ast.c
	$(CC) $(CFLAGS) $(AST_PATH)/ast.c -o $(OUT)/ast.o

astd.o: $(AST_PATH)/ast.c
	$(CC) -g $(CFLAGS) $(AST_PATH)/ast.c -o $(OUT)/astd.o

preproc.o: $(PREPROC_PATH)/preprocessor.c
	$(CC) $(CFLAGS) $(PREPROC_PATH)/preprocessor.c -o $(OUT)/preproc.o

preprocd.o: $(PREPROC_PATH)/preprocessor.c
	$(CC) $(CFLAGS) -g $(PREPROC_PATH)/preprocessor.c -o $(OUT)/preprocd.o

dependency_analyzer.o: $(DEPENDENCY_ANALYZER_PATH)/dependency_analyzer.c
	$(CC) $(CFLAGS) $(DEPENDENCY_ANALYZER_PATH)/dependency_analyzer.c -o $(OUT)/dependency_analyzer.o

dependency_analyzerd.o: $(DEPENDENCY_ANALYZER_PATH)/dependency_analyzer.c
	$(CC) $(CFLAGS) -g $(DEPENDENCY_ANALYZER_PATH)/dependency_analyzer.c -o $(OUT)/dependency_analyzerd.o

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

dynamic_array_test.o: $(DYNAMIC_ARRAY_PATH)/dynamic_array_test.c
	$(CC) $(CFLAGS) $(DYNAMIC_ARRAY_PATH)/dynamic_array_test.c -o $(OUT)/dynamic_array_test.o

dynamic_array_testd.o: $(DYNAMIC_ARRAY_PATH)/dynamic_array_test.c
	$(CC) $(CFLAGS) -g $(DYNAMIC_ARRAY_PATH)/dynamic_array_test.c -o $(OUT)/dynamic_array_testd.o

dynamic_array_test: dynamic_array_test.o dynamic_array.o
	$(CC) -o $(OUT)/dynamic_array_test $(OUT)/dynamic_array_test.o $(OUT)/dynamic_array.o

dynamic_array_testd: dynamic_array_testd.o dynamic_arrayd.o
	$(CC) -o $(OUT)/dynamic_array_testd $(OUT)/dynamic_array_testd.o $(OUT)/dynamic_arrayd.o

parser_test.o: $(PARSER_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser_test.c -o $(OUT)/parser_test.o

parser_testd.o: $(PARSER_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser_test.c -o $(OUT)/parser_testd.o

parser_test: parser.o lexer.o parser_test.o symtab.o lexstack.o heapstack.o type_system.o ast.o call_graph.o heap_queue.o lightstack.o
	$(CC) -o $(OUT)/parser_test $(OUT)/parser_test.o $(OUT)/parser.o $(OUT)/lexstack.o $(OUT)/lexer.o $(OUT)/heapstack.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/ast.o $(OUT)/call_graph.o $(OUT)/heap_queue.o $(OUT)/lightstack.o

parser_test_debug: parserd.o lexerd.o parser_testd.o symtabd.o lexstack.o heapstackd.o type_systemd.o astd.o call_graphd.o heap_queued.o lightstackd.o
	$(CC) -g -o $(OUT)/debug $(OUT)/parser_testd.o $(OUT)/parserd.o $(OUT)/lexstackd.o $(OUT)/lexerd.o $(OUT)/heapstackd.o $(OUT)/symtabd.o $(OUT)/type_systemd.o $(OUT)/astd.o $(OUT)/call_graphd.o $(OUT)/heap_queued.o $(OUT)/lightstackd.o

symtab_test: symtab.o symtab_test.o lexer.o type_system.o lexstack.o lightstack.o
	$(CC) -o $(OUT)/symtab_test $(OUT)/lexer.o $(OUT)/symtab_test.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/lexstack.o $(OUT)/lightstack.o

symtab_testd: symtabd.o symtab_testd.o lexerd.o type_systemd.o lexstackd.o lightstackd.o
	$(CC) -o $(OUT)/symtab_testd $(OUT)/lexerd.o $(OUT)/symtab_testd.o $(OUT)/symtabd.o $(OUT)/type_systemd.o $(OUT)/lexstackd.o $(OUT)/lightstackd.o

call_graph.o : $(CALL_GRAPH_PATH)/call_graph.c
	$(CC) $(CFLAGS) $(CALL_GRAPH_PATH)/call_graph.c -o $(OUT)/call_graph.o

call_graphd.o : $(CALL_GRAPH_PATH)/call_graph.c
	$(CC) -g $(CFLAGS) $(CALL_GRAPH_PATH)/call_graph.c -o $(OUT)/call_graphd.o

compiler.o: ./oc/compiler/compiler.c 
	$(CC) $(CFLAGS) -o $(OUT)/compiler.o ./oc/compiler/compiler.c

compilerd.o: ./oc/compiler/compiler.c 
	$(CC) $(CFLAGS) -g -o $(OUT)/compilerd.o ./oc/compiler/compiler.c

three_address_code.o: $(THREE_ADDRESS_CODE_PATH)/three_address_code.c
	$(CC) $(CFLAGS) -o $(OUT)/three_address_code.o $(THREE_ADDRESS_CODE_PATH)/three_address_code.c

three_address_coded.o: $(THREE_ADDRESS_CODE_PATH)/three_address_code.c
	$(CC) $(CFLAGS) -g -o $(OUT)/three_address_coded.o $(THREE_ADDRESS_CODE_PATH)/three_address_code.c

oc: compiler.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o three_address_code.o heap_queue.o preproc.o dependency_analyzer.o priority_queue.o dynamic_array.o lightstack.o
	$(CC) -o $(OUT)/oc $(OUT)/compiler.o $(OUT)/parser.o $(OUT)/lexer.o $(OUT)/heapstack.o $(OUT)/lexstack.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/ast.o $(OUT)/cfg.o $(OUT)/call_graph.o $(OUT)/three_address_code.o $(OUT)/heap_queue.o $(OUT)/preproc.o $(OUT)/dependency_analyzer.o $(OUT)/priority_queue.o $(OUT)/dynamic_array.o $(OUT)/lightstack.o

oc_debug: compilerd.o parserd.o lexerd.o symtabd.o heapstackd.o type_systemd.o astd.o cfgd.o call_graphd.o lexstackd.o three_address_coded.o heap_queued.o preprocd.o dependency_analyzerd.o priority_queued.o dynamic_arrayd.o lightstackd.o
	$(CC) -o $(OUT)/ocd $(OUT)/compilerd.o $(OUT)/parserd.o $(OUT)/lexerd.o $(OUT)/heapstackd.o $(OUT)/symtabd.o $(OUT)/lexstackd.o $(OUT)/type_systemd.o $(OUT)/astd.o $(OUT)/cfgd.o $(OUT)/call_graphd.o $(OUT)/three_address_coded.o $(OUT)/heap_queued.o $(OUT)/preprocd.o $(OUT)/dependency_analyzerd.o $(OUT)/priority_queued.o $(OUT)/dynamic_arrayd.o $(OUT)/lightstackd.o

stest: symtab_test
	$(OUT)/symtab_test

stestd: symtab_testd
	$(OUT)/symtab_testd

ptest: parser_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/parser_test

compiler_test: oc
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/oc

array_test: dynamic_array_test
	$(OUT)/dynamic_array_test

clean:
	rm -f ./oc/out/*
