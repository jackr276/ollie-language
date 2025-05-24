# Makefile for OC
CC = gcc
CFLAGS = -Wall -Wextra -c -Wimplicit-fallthrough=0
CFLAGSLINK = -Wall -Wextra
TEST_SUITE_PATH = ./oc/compiler/test_suites
LEX_PATH = ./oc/compiler/lexer
STACK_PATH = ./oc/compiler/stack
STACK_DATA_AREA_PATH = ./oc/compiler/stack_data_area
SYMTAB_PATH = ./oc/compiler/symtab
PARSER_PATH = ./oc/compiler/parser
TYPE_SYSTEM_PATH = ./oc/compiler/type_system
CALL_GRAPH_PATH = ./oc/compiler/call_graph
AST_PATH = ./oc/compiler/ast
CFG_PATH = ./oc/compiler/cfg
OPTIMIZER_PATH = ./oc/compiler/optimizer
PREPROC_PATH = ./oc/compiler/preprocessor
DEPENDENCY_TREE_PATH = ./oc/compiler/dependency_tree
DYNAMIC_ARRAY_PATH = ./oc/compiler/dynamic_array
INSTRUCTION_PATH = ./oc/compiler/instruction
INSTRUCTION_SELECTOR_PATH = ./oc/compiler/instruction_selector
INSTRUCTION_SCHEDULER_PATH = ./oc/compiler/instruction_scheduler
REGISTER_ALLOCATOR_PATH = ./oc/compiler/register_allocator
CODE_GENERATOR_PATH = ./oc/compiler/code_generator
JUMP_TABLE_PATH = ./oc/compiler/jump_table
QUEUE_PATH = ./oc/compiler/queue
TEST_FILE_DIR = ./oc/test_files/
OUT = ./oc/out
PROGS = lexer_test symtab_test parser_test oc

all: $(PROGS)

ltest: lexer_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/lexer_test

lexer_test: lexer.o lexer_test.o lexstack.o
	$(CC) -o $(OUT)/lexer_test $(OUT)/lexer_test.o $(OUT)/lexer.o $(OUT)/lexstack.o

lexer_test.o: $(TEST_SUITE_PATH)/lexer_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/lexer_test.c -o $(OUT)/lexer_test.o

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

dependency_tree.o: $(DEPENDENCY_TREE_PATH)/dependency_tree.c
	$(CC) $(CFLAGS) $(DEPENDENCY_TREE_PATH)/dependency_tree.c -o $(OUT)/dependency_tree.o

dependency_treed.o: $(DEPENDENCY_TREE_PATH)/dependency_tree.c
	$(CC) $(CFLAGS) -g $(DEPENDENCY_TREE_PATH)/dependency_tree.c -o $(OUT)/dependency_treed.o

stack_data_area.o: $(STACK_DATA_AREA_PATH)/stack_data_area.c
	$(CC) $(CFLAGS) $(STACK_DATA_AREA_PATH)/stack_data_area.c -o $(OUT)/stack_data_area.o

stack_data_aread.o: $(STACK_DATA_AREA_PATH)/stack_data_area.c
	$(CC) $(CFLAGS) -g $(STACK_DATA_AREA_PATH)/stack_data_area.c -o $(OUT)/stack_data_aread.o

stack_data_area_test.o: $(TEST_SUITE_PATH)/stack_data_area_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/stack_data_area_test.c -o $(OUT)/stack_data_area_test.o

stack_data_area_testd.o: $(TEST_SUITE_PATH)/stack_data_area_test.c
	$(CC) $(CFLAGS) -g $(TEST_SUITE_PATH)/stack_data_area_test.c -o $(OUT)/stack_data_area_testd.o

symtab.o: $(SYMTAB_PATH)/symtab.c
	$(CC) $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT)/symtab.o

symtabd.o: $(SYMTAB_PATH)/symtab.c
	$(CC) -g $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT)/symtabd.o

jump_table.o: $(JUMP_TABLE_PATH)/jump_table.c
	$(CC) $(CFLAGS) $(JUMP_TABLE_PATH)/jump_table.c -o $(OUT)/jump_table.o

jump_tabled.o: $(JUMP_TABLE_PATH)/jump_table.c
	$(CC) $(CFLAGS) -g $(JUMP_TABLE_PATH)/jump_table.c -o $(OUT)/jump_tabled.o

cfg.o: $(CFG_PATH)/cfg.c
	$(CC) $(CFLAGS) $(CFG_PATH)/cfg.c -o $(OUT)/cfg.o

cfgd.o: $(CFG_PATH)/cfg.c
	$(CC) -g $(CFLAGS) $(CFG_PATH)/cfg.c -o $(OUT)/cfgd.o

optimizer.o: $(OPTIMIZER_PATH)/optimizer.c
	$(CC) $(CFLAGS) $(OPTIMIZER_PATH)/optimizer.c -o $(OUT)/optimizer.o

optimizerd.o: $(OPTIMIZER_PATH)/optimizer.c
	$(CC) $(CFLAGS) -g $(OPTIMIZER_PATH)/optimizer.c -o $(OUT)/optimizerd.o

instruction_selector.o: $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c
	$(CC) $(CFLAGS) $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c -o $(OUT)/instruction_selector.o

instruction_selectord.o: $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c
	$(CC) $(CFLAGS) -g $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c -o $(OUT)/instruction_selectord.o

instruction_scheduler.o: $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c
	$(CC) $(CFLAGS) $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c -o $(OUT)/instruction_scheduler.o

instruction_schedulerd.o: $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c
	$(CC) $(CFLAGS) -g $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c -o $(OUT)/instruction_schedulerd.o

register_allocator.o: $(REGISTER_ALLOCATOR_PATH)/register_allocator.c
	$(CC) $(CFLAGS) $(REGISTER_ALLOCATOR_PATH)/register_allocator.c -o $(OUT)/register_allocator.o

register_allocatord.o: $(REGISTER_ALLOCATOR_PATH)/register_allocator.c
	$(CC) $(CFLAGS) -g $(REGISTER_ALLOCATOR_PATH)/register_allocator.c -o $(OUT)/register_allocatord.o

code_generator.o: $(CODE_GENERATOR_PATH)/code_generator.c
	$(CC) $(CFLAGS) $(CODE_GENERATOR_PATH)/code_generator.c -o $(OUT)/code_generator.o

code_generatord.o: $(CODE_GENERATOR_PATH)/code_generator.c
	$(CC) $(CFLAGS) -g $(CODE_GENERATOR_PATH)/code_generator.c -o $(OUT)/code_generatord.o

type_system.o: $(TYPE_SYSTEM_PATH)/type_system.c
	$(CC) $(CFLAGS) $(TYPE_SYSTEM_PATH)/type_system.c -o $(OUT)/type_system.o

type_systemd.o: $(TYPE_SYSTEM_PATH)/type_system.c
	$(CC) -g $(CFLAGS) $(TYPE_SYSTEM_PATH)/type_system.c -o $(OUT)/type_systemd.o

parser.o: $(PARSER_PATH)/parser.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser.c -o $(OUT)/parser.o

parserd.o: $(PARSER_PATH)/parser.c
	$(CC) -g $(CFLAGS) $(PARSER_PATH)/parser.c -o $(OUT)/parserd.o

symtab_test.o: $(TEST_SUITE_PATH)/symtab_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/symtab_test.c -o $(OUT)/symtab_test.o

symtab_testd.o: $(TEST_SUITE_PATH)/symtab_test.c
	$(CC) -g $(CFLAGS) $(TEST_SUITE_PATH)/symtab_test.c -o $(OUT)/symtab_testd.o

dynamic_array_test.o: $(TEST_SUITE_PATH)/dynamic_array_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/dynamic_array_test.c -o $(OUT)/dynamic_array_test.o

dynamic_array_testd.o: $(TEST_SUITE_PATH)/dynamic_array_test.c
	$(CC) $(CFLAGS) -g $(TEST_SUITE_PATh)/dynamic_array_test.c -o $(OUT)/dynamic_array_testd.o

dynamic_array_test: dynamic_array_test.o dynamic_array.o
	$(CC) -o $(OUT)/dynamic_array_test $(OUT)/dynamic_array_test.o $(OUT)/dynamic_array.o

dynamic_array_testd: dynamic_array_testd.o dynamic_arrayd.o
	$(CC) -o $(OUT)/dynamic_array_testd $(OUT)/dynamic_array_testd.o $(OUT)/dynamic_arrayd.o

parser_test.o: $(TEST_SUITE_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/parser_test.c -o $(OUT)/parser_test.o

parser_testd.o: $(TEST_SUITE_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/parser_test.c -o $(OUT)/parser_testd.o

parser_test: parser.o lexer.o parser_test.o symtab.o lexstack.o heapstack.o type_system.o ast.o call_graph.o heap_queue.o lightstack.o dynamic_array.o stack_data_area.o instruction.o
	$(CC) -o $(OUT)/parser_test $(OUT)/parser_test.o $(OUT)/parser.o $(OUT)/lexstack.o $(OUT)/lexer.o $(OUT)/heapstack.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/ast.o $(OUT)/call_graph.o $(OUT)/heap_queue.o $(OUT)/lightstack.o $(OUT)/dynamic_array.o $(OUT)/stack_data_area.o $(OUT)/instruction.o

parser_test_debug: parserd.o lexerd.o parser_testd.o symtabd.o lexstack.o heapstackd.o type_systemd.o astd.o call_graphd.o heap_queued.o lightstackd.o dynamic_arrayd.o stack_data_aread.o instructiond.o
	$(CC) -g -o $(OUT)/debug $(OUT)/parser_testd.o $(OUT)/parserd.o $(OUT)/lexstackd.o $(OUT)/lexerd.o $(OUT)/heapstackd.o $(OUT)/symtabd.o $(OUT)/type_systemd.o $(OUT)/astd.o $(OUT)/call_graphd.o $(OUT)/heap_queued.o $(OUT)/lightstackd.o $(OUT)/dynamic_arrayd.o $(OUT)/stack_data_aread.o $(OUT)/instructiond.o

symtab_test: symtab.o symtab_test.o lexer.o type_system.o lexstack.o lightstack.o stack_data_area.o instruction.o dynamic_array.o parser.o cfg.o ast.o call_graph.o heap_queue.o heapstack.o jump_table.o 
	$(CC) -o $(OUT)/symtab_test $(OUT)/lexer.o $(OUT)/symtab_test.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/lexstack.o $(OUT)/lightstack.o $(OUT)/stack_data_area.o $(OUT)/instruction.o $(OUT)/dynamic_array.o $(OUT)/parser.o $(OUT)/cfg.o $(OUT)/ast.o $(OUT)/call_graph.o $(OUT)/heap_queue.o $(OUT)/heapstack.o $(OUT)/jump_table.o

symtab_testd: symtabd.o symtab_testd.o lexerd.o type_systemd.o lexstackd.o lightstackd.o stack_data_aread.o instructiond.o dynamic_arrayd.o
	$(CC) -o $(OUT)/symtab_testd $(OUT)/lexerd.o $(OUT)/symtab_testd.o $(OUT)/symtabd.o $(OUT)/type_systemd.o $(OUT)/lexstackd.o $(OUT)/lightstackd.o $(OUT)/stack_data_aread.o $(OUT)/instructiond.o $(OUT)/dynamic_arrayd.o

stack_data_area_test: stack_data_area_test.o type_system.o lexstack.o lightstack.o symtab.o lexer.o instruction.o stack_data_area.o dynamic_array.o ast.o call_graph.o cfg.o parser.o heap_queue.o heapstack.o jump_table.o
	$(CC) -o $(OUT)/stack_data_area_test $(OUT)/lexer.o $(OUT)/stack_data_area_test.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/lexstack.o $(OUT)/lightstack.o $(OUT)/stack_data_area.o $(OUT)/instruction.o $(OUT)/dynamic_array.o $(OUT)/ast.o $(OUT)/call_graph.o $(OUT)/cfg.o $(OUT)/parser.o $(OUT)/heap_queue.o $(OUT)/heapstack.o $(OUT)/jump_table.o
	
stack_data_area_testd: stack_data_area_testd.o type_systemd.o lexstackd.o lightstackd.o symtabd.o lexerd.o instructiond.o stack_data_aread.o dynamic_arrayd.o astd.o call_graphd.o cfgd.o parserd.o heap_queued.o heapstackd.o jump_tabled.o
	$(CC) -o $(OUT)/stack_data_area_testd $(OUT)/lexerd.o $(OUT)/stack_data_area_testd.o $(OUT)/symtabd.o $(OUT)/type_systemd.o $(OUT)/lexstackd.o $(OUT)/lightstackd.o $(OUT)/instructiond.o $(OUT)/stack_data_aread.o $(OUT)/dynamic_arrayd.o $(OUT)/astd.o $(OUT)/call_graphd.o $(OUT)/cfgd.o $(OUT)/parserd.o $(OUT)/heap_queued.o $(OUT)/heapstackd.o $(OUT)/jump_tabled.o

call_graph.o : $(CALL_GRAPH_PATH)/call_graph.c
	$(CC) $(CFLAGS) $(CALL_GRAPH_PATH)/call_graph.c -o $(OUT)/call_graph.o

call_graphd.o : $(CALL_GRAPH_PATH)/call_graph.c
	$(CC) -g $(CFLAGS) $(CALL_GRAPH_PATH)/call_graph.c -o $(OUT)/call_graphd.o

compiler.o: ./oc/compiler/compiler.c 
	$(CC) $(CFLAGS) -o $(OUT)/compiler.o ./oc/compiler/compiler.c

compilerd.o: ./oc/compiler/compiler.c 
	$(CC) $(CFLAGS) -g -o $(OUT)/compilerd.o ./oc/compiler/compiler.c

instruction.o: $(INSTRUCTION_PATH)/instruction.c
	$(CC) $(CFLAGS) -o $(OUT)/instruction.o $(INSTRUCTION_PATH)/instruction.c

instructiond.o: $(INSTRUCTION_PATH)/instruction.c
	$(CC) $(CFLAGS) -g -o $(OUT)/instructiond.o $(INSTRUCTION_PATH)/instruction.c

front_end_test.o: $(TEST_SUITE_PATH)/front_end_test.c
	$(CC) $(CFLAGS) -o $(OUT)/front_end_test.o $(TEST_SUITE_PATH)/front_end_test.c

front_end_testd.o: $(TEST_SUITE_PATH)/front_end_test.c
	$(CC) $(CFLAGS) -g  -o $(OUT)/front_end_testd.o $(TEST_SUITE_PATH)/front_end_test.c

middle_end_test.o: $(TEST_SUITE_PATH)/middle_end_test.c
	$(CC) $(CFLAGS) -o $(OUT)/middle_end_test.o $(TEST_SUITE_PATH)/middle_end_test.c

middle_end_testd.o: $(TEST_SUITE_PATH)/middle_end_test.c
	$(CC) $(CFLAGS) -g -o $(OUT)/middle_end_testd.o $(TEST_SUITE_PATH)/middle_end_test.c

front_end_test: front_end_test.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instruction.o heap_queue.o preproc.o dependency_tree.o priority_queue.o dynamic_array.o lightstack.o jump_table.o stack_data_area.o
	$(CC) -o $(OUT)/front_end_test $(OUT)/front_end_test.o $(OUT)/parser.o $(OUT)/lexer.o $(OUT)/heapstack.o $(OUT)/lexstack.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/ast.o $(OUT)/cfg.o $(OUT)/call_graph.o $(OUT)/instruction.o $(OUT)/heap_queue.o $(OUT)/preproc.o $(OUT)/dependency_tree.o $(OUT)/priority_queue.o $(OUT)/dynamic_array.o $(OUT)/lightstack.o $(OUT)/jump_table.o $(OUT)/stack_data_area.o

front_end_testd: front_end_testd.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instructiond.o heap_queue.o preproc.o dependency_treed.o priority_queue.o dynamic_array.o lightstack.o jump_tabled.o stack_data_aread.o
	$(CC) -o $(OUT)/front_end_testd $(OUT)/front_end_test.o $(OUT)/parser.o $(OUT)/lexer.o $(OUT)/heapstack.o $(OUT)/lexstack.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/ast.o $(OUT)/cfg.o $(OUT)/call_graph.o $(OUT)/instructiond.o $(OUT)/heap_queue.o $(OUT)/preproc.o $(OUT)/dependency_treed.o $(OUT)/priority_queue.o $(OUT)/dynamic_array.o $(OUT)/lightstack.o $(OUT)/jump_tabled.o $(OUT)/stack_data_aread.o

middle_end_test: middle_end_test.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instruction.o heap_queue.o preproc.o dependency_tree.o priority_queue.o dynamic_array.o lightstack.o jump_table.o optimizer.o stack_data_area.o
	$(CC) -o $(OUT)/middle_end_test $(OUT)/middle_end_test.o $(OUT)/parser.o $(OUT)/lexer.o $(OUT)/heapstack.o $(OUT)/lexstack.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/ast.o $(OUT)/cfg.o $(OUT)/call_graph.o $(OUT)/instruction.o $(OUT)/heap_queue.o $(OUT)/preproc.o $(OUT)/dependency_tree.o $(OUT)/priority_queue.o $(OUT)/dynamic_array.o $(OUT)/lightstack.o $(OUT)/optimizer.o $(OUT)/jump_table.o $(OUT)/stack_data_area.o

oc: compiler.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instruction.o heap_queue.o preproc.o dependency_tree.o priority_queue.o dynamic_array.o lightstack.o optimizer.o instruction_selector.o jump_table.o code_generator.o stack_data_area.o register_allocator.o instruction_scheduler.o
	$(CC) -o $(OUT)/oc $(OUT)/compiler.o $(OUT)/parser.o $(OUT)/lexer.o $(OUT)/heapstack.o $(OUT)/lexstack.o $(OUT)/symtab.o $(OUT)/type_system.o $(OUT)/ast.o $(OUT)/cfg.o $(OUT)/call_graph.o $(OUT)/instruction.o $(OUT)/heap_queue.o $(OUT)/preproc.o $(OUT)/dependency_tree.o $(OUT)/priority_queue.o $(OUT)/dynamic_array.o $(OUT)/lightstack.o $(OUT)/optimizer.o $(OUT)/instruction_selector.o $(OUT)/jump_table.o $(OUT)/code_generator.o $(OUT)/stack_data_area.o $(OUT)/register_allocator.o $(OUT)/instruction_scheduler.o

oc_debug: compilerd.o parserd.o lexerd.o symtabd.o heapstackd.o type_systemd.o astd.o cfgd.o call_graphd.o lexstackd.o instructiond.o heap_queued.o preprocd.o dependency_treed.o priority_queued.o dynamic_arrayd.o lightstackd.o optimizerd.o instruction_selectord.o jump_tabled.o code_generatord.o stack_data_aread.o register_allocatord.o instruction_schedulerd.o
	$(CC) -o $(OUT)/ocd $(OUT)/compilerd.o $(OUT)/parserd.o $(OUT)/lexerd.o $(OUT)/heapstackd.o $(OUT)/symtabd.o $(OUT)/lexstackd.o $(OUT)/type_systemd.o $(OUT)/astd.o $(OUT)/cfgd.o $(OUT)/call_graphd.o $(OUT)/instructiond.o $(OUT)/heap_queued.o $(OUT)/preprocd.o $(OUT)/dependency_treed.o $(OUT)/priority_queued.o $(OUT)/dynamic_arrayd.o $(OUT)/lightstackd.o $(OUT)/optimizerd.o $(OUT)/instruction_selectord.o $(OUT)/jump_tabled.o $(OUT)/code_generatord.o $(OUT)/stack_data_aread.o $(OUT)/register_allocatord.o $(OUT)/instruction_schedulerd.o

stest: symtab_test
	$(OUT)/symtab_test

stestd: symtab_testd
	$(OUT)/symtab_testd

test_data_area: stack_data_area_test
	$(OUT)/stack_data_area_test ./oc/test_files/data_area_test_input.ol

ptest: parser_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/parser_test

front_test: front_end_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/front_end_test

middle_test: middle_end_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/middle_end_test

compiler_test: oc
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 ./oc/out/oc

array_test: dynamic_array_test
	$(OUT)/dynamic_array_test

clean:
	rm -f ./oc/out/*
