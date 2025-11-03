# Makefile for OC
CC = gcc
CFLAGS = -Wall -Wextra -c -Wimplicit-fallthrough=0
CFLAGSLINK = -Wall -Wextra
TEST_SUITE_PATH = ./oc/compiler/test_suites
LEX_PATH = ./oc/compiler/lexer
STACK_PATH = ./oc/compiler/utils/stack
STACK_DATA_AREA_PATH = ./oc/compiler/stack_data_area
FILE_BUILDER_PATH = ./oc/compiler/file_builder
SYMTAB_PATH = ./oc/compiler/symtab
PARSER_PATH = ./oc/compiler/parser
TYPE_SYSTEM_PATH = ./oc/compiler/type_system
CALL_GRAPH_PATH = ./oc/compiler/call_graph
AST_PATH = ./oc/compiler/ast
CFG_PATH = ./oc/compiler/cfg
OPTIMIZER_PATH = ./oc/compiler/optimizer
PREPROC_PATH = ./oc/compiler/preprocessor
POSTPROCESSOR_PATH = ./oc/compiler/postprocessor
DEPENDENCY_TREE_PATH = ./oc/compiler/dependency_tree
DYNAMIC_ARRAY_PATH = ./oc/compiler/utils/dynamic_array
DYNAMIC_STRING_PATH = ./oc/compiler/utils/dynamic_string
INSTRUCTION_PATH = ./oc/compiler/instruction
INSTRUCTION_SELECTOR_PATH = ./oc/compiler/instruction_selector
INSTRUCTION_SCHEDULER_PATH = ./oc/compiler/instruction_scheduler
REGISTER_ALLOCATOR_PATH = ./oc/compiler/register_allocator
INTERFERENCE_GRAPH_PATH = ./oc/compiler/interference_graph
JUMP_TABLE_PATH = ./oc/compiler/jump_table
QUEUE_PATH = ./oc/compiler/utils/queue
TEST_FILE_DIR = ./oc/test_files/
OUTPUTTED_ASSEMBLY_DIR = ./oc/generated_assembly/
OUT_LOCAL = ./oc/out
OUT_CI = $$RUNNER_TEMP
PROGS = lexer_test symtab_test parser_test oc

# Input and output arrays
inputs := $(shell find $(TEST_FILE_DIR) -type f -name "*.ol" | sort)

all: $(PROGS)

ltest: lexer_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_LOCAL)/lexer_test

lexer_test: lexer.o lexer_test.o lexstack.o dynamic_string.o
	$(CC) -o $(OUT_LOCAL)/lexer_test $(OUT_LOCAL)/lexer_test.o $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/dynamic_string.o

lexer_test.o: $(TEST_SUITE_PATH)/lexer_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/lexer_test.c -o $(OUT_LOCAL)/lexer_test.o

lexer.o: $(LEX_PATH)/lexer.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer.c -o $(OUT_LOCAL)/lexer.o

lexerd.o: $(LEX_PATH)/lexer.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer.c -o $(OUT_LOCAL)/lexerd.o

heapstack.o: $(STACK_PATH)/heapstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/heapstack.c -o $(OUT_LOCAL)/heapstack.o

heapstackd.o: $(STACK_PATH)/heapstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/heapstack.c -o $(OUT_LOCAL)/heapstackd.o

nesting_stack.o: $(STACK_PATH)/nesting_stack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/nesting_stack.c -o $(OUT_LOCAL)/nesting_stack.o

nesting_stackd.o: $(STACK_PATH)/nesting_stack.c
	$(CC) $(CFLAGS) -g $(STACK_PATH)/nesting_stack.c -o $(OUT_LOCAL)/nesting_stackd.o

heap_queue.o: $(QUEUE_PATH)/heap_queue.c
	$(CC) $(CFLAGS) $(QUEUE_PATH)/heap_queue.c -o $(OUT_LOCAL)/heap_queue.o

heap_queued.o: $(QUEUE_PATH)/heap_queue.c
	$(CC) $(CFLAGS) -g $(QUEUE_PATH)/heap_queue.c -o $(OUT_LOCAL)/heap_queued.o

dynamic_array.o: $(DYNAMIC_ARRAY_PATH)/dynamic_array.c
	$(CC) $(CFLAGS) $(DYNAMIC_ARRAY_PATH)/dynamic_array.c -o $(OUT_LOCAL)/dynamic_array.o

dynamic_arrayd.o: $(DYNAMIC_ARRAY_PATH)/dynamic_array.c
	$(CC) $(CFLAGS) -g $(DYNAMIC_ARRAY_PATH)/dynamic_array.c -o $(OUT_LOCAL)/dynamic_arrayd.o

priority_queue.o: $(QUEUE_PATH)/priority_queue.c
	$(CC) $(CFLAGS) $(QUEUE_PATH)/priority_queue.c -o $(OUT_LOCAL)/priority_queue.o

priority_queued.o: $(QUEUE_PATH)/priority_queue.c
	$(CC) $(CFLAGS) -g $(QUEUE_PATH)/priority_queue.c -o $(OUT_LOCAL)/priority_queued.o

lexstack.o: $(STACK_PATH)/lexstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/lexstack.c -o $(OUT_LOCAL)/lexstack.o

lexstackd.o: $(STACK_PATH)/lexstack.c
	$(CC) $(CFLAGS) -g $(STACK_PATH)/lexstack.c -o $(OUT_LOCAL)/lexstackd.o

lightstack.o: $(STACK_PATH)/lightstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/lightstack.c -o $(OUT_LOCAL)/lightstack.o

lightstackd.o: $(STACK_PATH)/lightstack.c
	$(CC) $(CFLAGS) -g $(STACK_PATH)/lightstack.c -o $(OUT_LOCAL)/lightstackd.o

dynamic_string.o: $(DYNAMIC_STRING_PATH)/dynamic_string.c
	$(CC) $(CFLAGS) $(DYNAMIC_STRING_PATH)/dynamic_string.c -o $(OUT_LOCAL)/dynamic_string.o

dynamic_stringd.o: $(DYNAMIC_STRING_PATH)/dynamic_string.c
	$(CC) $(CFLAGS) -g $(DYNAMIC_STRING_PATH)/dynamic_string.c -o $(OUT_LOCAL)/dynamic_stringd.o

ast.o: $(AST_PATH)/ast.c
	$(CC) $(CFLAGS) $(AST_PATH)/ast.c -o $(OUT_LOCAL)/ast.o

astd.o: $(AST_PATH)/ast.c
	$(CC) -g $(CFLAGS) $(AST_PATH)/ast.c -o $(OUT_LOCAL)/astd.o

preproc.o: $(PREPROC_PATH)/preprocessor.c
	$(CC) $(CFLAGS) $(PREPROC_PATH)/preprocessor.c -o $(OUT_LOCAL)/preproc.o

preprocd.o: $(PREPROC_PATH)/preprocessor.c
	$(CC) $(CFLAGS) -g $(PREPROC_PATH)/preprocessor.c -o $(OUT_LOCAL)/preprocd.o

postprocessor.o: $(POSTPROCESSOR_PATH)/postprocessor.c
	$(CC) $(CFLAGS) $(POSTPROCESSOR_PATH)/postprocessor.c -o $(OUT_LOCAL)/postprocessor.o

postprocessord.o: $(POSTPROCESSOR_PATH)/postprocessor.c
	$(CC) $(CFLAGS) -g $(POSTPROCESSOR_PATH)/postprocessor.c -o $(OUT_LOCAL)/postprocessord.o

dependency_tree.o: $(DEPENDENCY_TREE_PATH)/dependency_tree.c
	$(CC) $(CFLAGS) $(DEPENDENCY_TREE_PATH)/dependency_tree.c -o $(OUT_LOCAL)/dependency_tree.o

dependency_treed.o: $(DEPENDENCY_TREE_PATH)/dependency_tree.c
	$(CC) $(CFLAGS) -g $(DEPENDENCY_TREE_PATH)/dependency_tree.c -o $(OUT_LOCAL)/dependency_treed.o

stack_data_area.o: $(STACK_DATA_AREA_PATH)/stack_data_area.c
	$(CC) $(CFLAGS) $(STACK_DATA_AREA_PATH)/stack_data_area.c -o $(OUT_LOCAL)/stack_data_area.o

stack_data_aread.o: $(STACK_DATA_AREA_PATH)/stack_data_area.c
	$(CC) $(CFLAGS) -g $(STACK_DATA_AREA_PATH)/stack_data_area.c -o $(OUT_LOCAL)/stack_data_aread.o

stack_data_area_test.o: $(TEST_SUITE_PATH)/stack_data_area_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/stack_data_area_test.c -o $(OUT_LOCAL)/stack_data_area_test.o

stack_data_area_testd.o: $(TEST_SUITE_PATH)/stack_data_area_test.c
	$(CC) $(CFLAGS) -g $(TEST_SUITE_PATH)/stack_data_area_test.c -o $(OUT_LOCAL)/stack_data_area_testd.o

interference_graph_test.o: $(TEST_SUITE_PATH)/interference_graph_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/interference_graph_test.c -o $(OUT_LOCAL)/interference_graph_test.o

interference_graph_testd.o: $(TEST_SUITE_PATH)/interference_graph_test.c
	$(CC) $(CFLAGS) -g $(TEST_SUITE_PATH)/interference_graph_test.c -o $(OUT_LOCAL)/interference_graph_testd.o

symtab.o: $(SYMTAB_PATH)/symtab.c
	$(CC) $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT_LOCAL)/symtab.o

symtabd.o: $(SYMTAB_PATH)/symtab.c
	$(CC) -g $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT_LOCAL)/symtabd.o

jump_table.o: $(JUMP_TABLE_PATH)/jump_table.c
	$(CC) $(CFLAGS) $(JUMP_TABLE_PATH)/jump_table.c -o $(OUT_LOCAL)/jump_table.o

jump_tabled.o: $(JUMP_TABLE_PATH)/jump_table.c
	$(CC) $(CFLAGS) -g $(JUMP_TABLE_PATH)/jump_table.c -o $(OUT_LOCAL)/jump_tabled.o

cfg.o: $(CFG_PATH)/cfg.c
	$(CC) $(CFLAGS) $(CFG_PATH)/cfg.c -o $(OUT_LOCAL)/cfg.o

cfgd.o: $(CFG_PATH)/cfg.c
	$(CC) -g $(CFLAGS) $(CFG_PATH)/cfg.c -o $(OUT_LOCAL)/cfgd.o

optimizer.o: $(OPTIMIZER_PATH)/optimizer.c
	$(CC) $(CFLAGS) $(OPTIMIZER_PATH)/optimizer.c -o $(OUT_LOCAL)/optimizer.o

optimizerd.o: $(OPTIMIZER_PATH)/optimizer.c
	$(CC) $(CFLAGS) -g $(OPTIMIZER_PATH)/optimizer.c -o $(OUT_LOCAL)/optimizerd.o

instruction_selector.o: $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c
	$(CC) $(CFLAGS) $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c -o $(OUT_LOCAL)/instruction_selector.o

instruction_selectord.o: $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c
	$(CC) $(CFLAGS) -g $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c -o $(OUT_LOCAL)/instruction_selectord.o

instruction_scheduler.o: $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c
	$(CC) $(CFLAGS) $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c -o $(OUT_LOCAL)/instruction_scheduler.o

instruction_schedulerd.o: $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c
	$(CC) $(CFLAGS) -g $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c -o $(OUT_LOCAL)/instruction_schedulerd.o

register_allocator.o: $(REGISTER_ALLOCATOR_PATH)/register_allocator.c
	$(CC) $(CFLAGS) $(REGISTER_ALLOCATOR_PATH)/register_allocator.c -o $(OUT_LOCAL)/register_allocator.o

register_allocatord.o: $(REGISTER_ALLOCATOR_PATH)/register_allocator.c
	$(CC) $(CFLAGS) -g $(REGISTER_ALLOCATOR_PATH)/register_allocator.c -o $(OUT_LOCAL)/register_allocatord.o

file_builder.o: $(FILE_BUILDER_PATH)/file_builder.c
	$(CC) $(CFLAGS) $(FILE_BUILDER_PATH)/file_builder.c -o $(OUT_LOCAL)/file_builder.o

file_builderd.o: $(FILE_BUILDER_PATH)/file_builder.c
	$(CC) $(CFLAGS) -g $(FILE_BUILDER_PATH)/file_builder.c -o $(OUT_LOCAL)/file_builderd.o

interference_graph.o: $(INTERFERENCE_GRAPH_PATH)/interference_graph.c
	$(CC) $(CFLAGS) $(INTERFERENCE_GRAPH_PATH)/interference_graph.c -o $(OUT_LOCAL)/interference_graph.o

interference_graphd.o: $(INTERFERENCE_GRAPH_PATH)/interference_graph.c
	$(CC) $(CFLAGS) -g $(INTERFERENCE_GRAPH_PATH)/interference_graph.c -o $(OUT_LOCAL)/interference_graphd.o

type_system.o: $(TYPE_SYSTEM_PATH)/type_system.c
	$(CC) $(CFLAGS) $(TYPE_SYSTEM_PATH)/type_system.c -o $(OUT_LOCAL)/type_system.o

type_systemd.o: $(TYPE_SYSTEM_PATH)/type_system.c
	$(CC) -g $(CFLAGS) $(TYPE_SYSTEM_PATH)/type_system.c -o $(OUT_LOCAL)/type_systemd.o

parser.o: $(PARSER_PATH)/parser.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser.c -o $(OUT_LOCAL)/parser.o

parserd.o: $(PARSER_PATH)/parser.c
	$(CC) -g $(CFLAGS) $(PARSER_PATH)/parser.c -o $(OUT_LOCAL)/parserd.o

symtab_test.o: $(TEST_SUITE_PATH)/symtab_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/symtab_test.c -o $(OUT_LOCAL)/symtab_test.o

symtab_testd.o: $(TEST_SUITE_PATH)/symtab_test.c
	$(CC) -g $(CFLAGS) $(TEST_SUITE_PATH)/symtab_test.c -o $(OUT_LOCAL)/symtab_testd.o

dynamic_array_test.o: $(TEST_SUITE_PATH)/dynamic_array_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/dynamic_array_test.c -o $(OUT_LOCAL)/dynamic_array_test.o

dynamic_array_testd.o: $(TEST_SUITE_PATH)/dynamic_array_test.c
	$(CC) $(CFLAGS) -g $(TEST_SUITE_PATh)/dynamic_array_test.c -o $(OUT_LOCAL)/dynamic_array_testd.o

dynamic_string_test.o: $(TEST_SUITE_PATH)/dynamic_string_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/dynamic_string_test.c -o $(OUT_LOCAL)/dynamic_string_test.o

dynamic_string_testd.o: $(TEST_SUITE_PATH)/dynamic_string_test.c
	$(CC) $(CFLAGS) -g $(TEST_SUITE_PATH)/dynamic_string_test.c -o $(OUT_LOCAL)/dynamic_string_testd.o

dynamic_string_test: dynamic_string_test.o dynamic_string.o
	$(CC) -o $(OUT_LOCAL)/dynamic_string_test $(OUT_LOCAL)/dynamic_string_test.o $(OUT_LOCAL)/dynamic_string.o

dynamic_string_testd: dynamic_string_testd.o dynamic_stringd.o
	$(CC) -o $(OUT_LOCAL)/dynamic_string_testd $(OUT_LOCAL)/dynamic_string_testd.o $(OUT_LOCAL)/dynamic_stringd.o

dynamic_array_test: dynamic_array_test.o dynamic_array.o
	$(CC) -o $(OUT_LOCAL)/dynamic_array_test $(OUT_LOCAL)/dynamic_array_test.o $(OUT_LOCAL)/dynamic_array.o

dynamic_array_testd: dynamic_array_testd.o dynamic_arrayd.o
	$(CC) -o $(OUT_LOCAL)/dynamic_array_testd $(OUT_LOCAL)/dynamic_array_testd.o $(OUT_LOCAL)/dynamic_arrayd.o

parser_test.o: $(TEST_SUITE_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/parser_test.c -o $(OUT_LOCAL)/parser_test.o

parser_testd.o: $(TEST_SUITE_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/parser_test.c -o $(OUT_LOCAL)/parser_testd.o

parser_test: parser.o lexer.o parser_test.o symtab.o lexstack.o heapstack.o type_system.o ast.o call_graph.o heap_queue.o lightstack.o dynamic_array.o stack_data_area.o instruction.o dynamic_string.o nesting_stack.o
	$(CC) -o $(OUT_LOCAL)/parser_test $(OUT_LOCAL)/parser_test.o $(OUT_LOCAL)/parser.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/heapstack.o $(OUT_LOCAL)/symtab.o $(OUT_LOCAL)/type_system.o $(OUT_LOCAL)/ast.o $(OUT_LOCAL)/call_graph.o $(OUT_LOCAL)/heap_queue.o $(OUT_LOCAL)/lightstack.o $(OUT_LOCAL)/dynamic_array.o $(OUT_LOCAL)/stack_data_area.o $(OUT_LOCAL)/instruction.o $(OUT_LOCAL)/dynamic_string.o $(OUT_LOCAL)/nesting_stack.o

parser_test_debug: parserd.o lexerd.o parser_testd.o symtabd.o lexstack.o heapstackd.o type_systemd.o astd.o call_graphd.o heap_queued.o lightstackd.o dynamic_arrayd.o stack_data_aread.o instructiond.o dynamic_stringd.o nesting_stackd.o
	$(CC) -g -o $(OUT_LOCAL)/parser_test_debug $(OUT_LOCAL)/parser_testd.o $(OUT_LOCAL)/parserd.o $(OUT_LOCAL)/lexstackd.o $(OUT_LOCAL)/lexerd.o $(OUT_LOCAL)/heapstackd.o $(OUT_LOCAL)/symtabd.o $(OUT_LOCAL)/type_systemd.o $(OUT_LOCAL)/astd.o $(OUT_LOCAL)/call_graphd.o $(OUT_LOCAL)/heap_queued.o $(OUT_LOCAL)/lightstackd.o $(OUT_LOCAL)/dynamic_arrayd.o $(OUT_LOCAL)/stack_data_aread.o $(OUT_LOCAL)/instructiond.o $(OUT_LOCAL)/dynamic_stringd.o $(OUT_LOCAL)/nesting_stackd.o

symtab_test: symtab.o symtab_test.o lexer.o type_system.o lexstack.o lightstack.o stack_data_area.o instruction.o dynamic_array.o parser.o cfg.o ast.o call_graph.o heap_queue.o heapstack.o jump_table.o dynamic_string.o nesting_stack.o
	$(CC) -o $(OUT_LOCAL)/symtab_test $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/symtab_test.o $(OUT_LOCAL)/symtab.o $(OUT_LOCAL)/type_system.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/lightstack.o $(OUT_LOCAL)/stack_data_area.o $(OUT_LOCAL)/instruction.o $(OUT_LOCAL)/dynamic_array.o $(OUT_LOCAL)/parser.o $(OUT_LOCAL)/cfg.o $(OUT_LOCAL)/ast.o $(OUT_LOCAL)/call_graph.o $(OUT_LOCAL)/heap_queue.o $(OUT_LOCAL)/heapstack.o $(OUT_LOCAL)/jump_table.o $(OUT_LOCAL)/dynamic_string.o $(OUT_LOCAL)/nesting_stack.o

symtab_testd: symtabd.o symtab_testd.o lexerd.o type_systemd.o lexstackd.o lightstackd.o stack_data_aread.o instructiond.o dynamic_arrayd.o dynamic_stringd.o nesting_stackd.o
	$(CC) -o $(OUT_LOCAL)/symtab_testd $(OUT_LOCAL)/lexerd.o $(OUT_LOCAL)/symtab_testd.o $(OUT_LOCAL)/symtabd.o $(OUT_LOCAL)/type_systemd.o $(OUT_LOCAL)/lexstackd.o $(OUT_LOCAL)/lightstackd.o $(OUT_LOCAL)/stack_data_aread.o $(OUT_LOCAL)/instructiond.o $(OUT_LOCAL)/dynamic_arrayd.o $(OUT_LOCAL)/dynamic_stringd.o $(OUT_LOCAL)/nesting_stackd.o

stack_data_area_test: stack_data_area_test.o type_system.o lexstack.o lightstack.o symtab.o lexer.o instruction.o stack_data_area.o dynamic_array.o ast.o call_graph.o cfg.o parser.o heap_queue.o heapstack.o jump_table.o dynamic_string.o nesting_stack.o
	$(CC) -o $(OUT_LOCAL)/stack_data_area_test $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/stack_data_area_test.o $(OUT_LOCAL)/symtab.o $(OUT_LOCAL)/type_system.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/lightstack.o $(OUT_LOCAL)/stack_data_area.o $(OUT_LOCAL)/instruction.o $(OUT_LOCAL)/dynamic_array.o $(OUT_LOCAL)/ast.o $(OUT_LOCAL)/call_graph.o $(OUT_LOCAL)/cfg.o $(OUT_LOCAL)/parser.o $(OUT_LOCAL)/heap_queue.o $(OUT_LOCAL)/heapstack.o $(OUT_LOCAL)/jump_table.o $(OUT_LOCAL)/dynamic_string.o $(OUT_LOCAL)/nesting_stack.o
	
stack_data_area_testd: stack_data_area_testd.o type_systemd.o lexstackd.o lightstackd.o symtabd.o lexerd.o instructiond.o stack_data_aread.o dynamic_arrayd.o astd.o call_graphd.o cfgd.o parserd.o heap_queued.o heapstackd.o jump_tabled.o dynamic_stringd.o nesting_stackd.o
	$(CC) -o $(OUT_LOCAL)/stack_data_area_testd $(OUT_LOCAL)/lexerd.o $(OUT_LOCAL)/stack_data_area_testd.o $(OUT_LOCAL)/symtabd.o $(OUT_LOCAL)/type_systemd.o $(OUT_LOCAL)/lexstackd.o $(OUT_LOCAL)/lightstackd.o $(OUT_LOCAL)/instructiond.o $(OUT_LOCAL)/stack_data_aread.o $(OUT_LOCAL)/dynamic_arrayd.o $(OUT_LOCAL)/astd.o $(OUT_LOCAL)/call_graphd.o $(OUT_LOCAL)/cfgd.o $(OUT_LOCAL)/parserd.o $(OUT_LOCAL)/heap_queued.o $(OUT_LOCAL)/heapstackd.o $(OUT_LOCAL)/jump_tabled.o $(OUT_LOCAL)/dynamic_stringd.o $(OUT_LOCAL)/nesting_stackd.o

call_graph.o : $(CALL_GRAPH_PATH)/call_graph.c
	$(CC) $(CFLAGS) $(CALL_GRAPH_PATH)/call_graph.c -o $(OUT_LOCAL)/call_graph.o

call_graphd.o : $(CALL_GRAPH_PATH)/call_graph.c
	$(CC) -g $(CFLAGS) $(CALL_GRAPH_PATH)/call_graph.c -o $(OUT_LOCAL)/call_graphd.o

compiler.o: ./oc/compiler/compiler.c 
	$(CC) $(CFLAGS) -o $(OUT_LOCAL)/compiler.o ./oc/compiler/compiler.c

compilerd.o: ./oc/compiler/compiler.c 
	$(CC) $(CFLAGS) -g -o $(OUT_LOCAL)/compilerd.o ./oc/compiler/compiler.c

instruction.o: $(INSTRUCTION_PATH)/instruction.c
	$(CC) $(CFLAGS) -o $(OUT_LOCAL)/instruction.o $(INSTRUCTION_PATH)/instruction.c

instructiond.o: $(INSTRUCTION_PATH)/instruction.c
	$(CC) $(CFLAGS) -g -o $(OUT_LOCAL)/instructiond.o $(INSTRUCTION_PATH)/instruction.c

front_end_test.o: $(TEST_SUITE_PATH)/front_end_test.c
	$(CC) $(CFLAGS) -o $(OUT_LOCAL)/front_end_test.o $(TEST_SUITE_PATH)/front_end_test.c

front_end_testd.o: $(TEST_SUITE_PATH)/front_end_test.c
	$(CC) $(CFLAGS) -g  -o $(OUT_LOCAL)/front_end_testd.o $(TEST_SUITE_PATH)/front_end_test.c

middle_end_test.o: $(TEST_SUITE_PATH)/middle_end_test.c
	$(CC) $(CFLAGS) -o $(OUT_LOCAL)/middle_end_test.o $(TEST_SUITE_PATH)/middle_end_test.c

instruction_selector_test.o: $(TEST_SUITE_PATH)/instruction_selector_test.c
	$(CC) $(CFLAGS) -o $(OUT_LOCAL)/instruction_selector_test.o $(TEST_SUITE_PATH)/instruction_selector_test.c

instruction_selector_testd.o: $(TEST_SUITE_PATH)/instruction_selector_test.c
	$(CC) $(CFLAGS) -g -o $(OUT_LOCAL)/instruction_selector_testd.o $(TEST_SUITE_PATH)/instruction_selector_test.c

middle_end_testd.o: $(TEST_SUITE_PATH)/middle_end_test.c
	$(CC) $(CFLAGS) -g -o $(OUT_LOCAL)/middle_end_testd.o $(TEST_SUITE_PATH)/middle_end_test.c

interference_graph_tester: parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instruction.o heap_queue.o priority_queue.o dynamic_array.o lightstack.o optimizer.o instruction_selector.o jump_table.o stack_data_area.o interference_graph.o interference_graph_test.o dynamic_string.o nesting_stack.o
	$(CC) -o $(OUT_LOCAL)/interference_graph_test $(OUT_LOCAL)/interference_graph_test.o $(OUT_LOCAL)/parser.o $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/heapstack.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/symtab.o $(OUT_LOCAL)/type_system.o $(OUT_LOCAL)/ast.o $(OUT_LOCAL)/cfg.o $(OUT_LOCAL)/call_graph.o $(OUT_LOCAL)/instruction.o $(OUT_LOCAL)/heap_queue.o $(OUT_LOCAL)/priority_queue.o $(OUT_LOCAL)/dynamic_array.o $(OUT_LOCAL)/lightstack.o $(OUT_LOCAL)/optimizer.o $(OUT_LOCAL)/instruction_selector.o $(OUT_LOCAL)/jump_table.o $(OUT_LOCAL)/stack_data_area.o $(OUT_LOCAL)/interference_graph.o $(OUT_LOCAL)/dynamic_string.o $(OUT_LOCAL)/nesting_stack.o

front_end_test: front_end_test.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instruction.o heap_queue.o preproc.o dependency_tree.o priority_queue.o dynamic_array.o lightstack.o jump_table.o stack_data_area.o dynamic_string.o nesting_stack.o
	$(CC) -o $(OUT_LOCAL)/front_end_test $(OUT_LOCAL)/front_end_test.o $(OUT_LOCAL)/parser.o $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/heapstack.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/symtab.o $(OUT_LOCAL)/type_system.o $(OUT_LOCAL)/ast.o $(OUT_LOCAL)/cfg.o $(OUT_LOCAL)/call_graph.o $(OUT_LOCAL)/instruction.o $(OUT_LOCAL)/heap_queue.o $(OUT_LOCAL)/preproc.o $(OUT_LOCAL)/dependency_tree.o $(OUT_LOCAL)/priority_queue.o $(OUT_LOCAL)/dynamic_array.o $(OUT_LOCAL)/lightstack.o $(OUT_LOCAL)/jump_table.o $(OUT_LOCAL)/stack_data_area.o $(OUT_LOCAL)/dynamic_string.o $(OUT_LOCAL)/nesting_stack.o

front_end_testd: front_end_testd.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instructiond.o heap_queue.o preproc.o dependency_treed.o priority_queue.o dynamic_array.o lightstack.o jump_tabled.o stack_data_aread.o dynamic_stringd.o nesting_stackd.o
	$(CC) -o $(OUT_LOCAL)/front_end_testd $(OUT_LOCAL)/front_end_test.o $(OUT_LOCAL)/parser.o $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/heapstack.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/symtab.o $(OUT_LOCAL)/type_system.o $(OUT_LOCAL)/ast.o $(OUT_LOCAL)/cfg.o $(OUT_LOCAL)/call_graph.o $(OUT_LOCAL)/instructiond.o $(OUT_LOCAL)/heap_queue.o $(OUT_LOCAL)/preproc.o $(OUT_LOCAL)/dependency_treed.o $(OUT_LOCAL)/priority_queue.o $(OUT_LOCAL)/dynamic_array.o $(OUT_LOCAL)/lightstack.o $(OUT_LOCAL)/jump_tabled.o $(OUT_LOCAL)/stack_data_aread.o $(OUT_LOCAL)/dynamic_string.o $(OUT_LOCAL)/nesting_stackd.o

middle_end_test: middle_end_test.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instruction.o heap_queue.o preproc.o dependency_tree.o priority_queue.o dynamic_array.o lightstack.o jump_table.o optimizer.o stack_data_area.o dynamic_string.o nesting_stack.o
	$(CC) -o $(OUT_LOCAL)/middle_end_test $(OUT_LOCAL)/middle_end_test.o $(OUT_LOCAL)/parser.o $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/heapstack.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/symtab.o $(OUT_LOCAL)/type_system.o $(OUT_LOCAL)/ast.o $(OUT_LOCAL)/cfg.o $(OUT_LOCAL)/call_graph.o $(OUT_LOCAL)/instruction.o $(OUT_LOCAL)/heap_queue.o $(OUT_LOCAL)/preproc.o $(OUT_LOCAL)/dependency_tree.o $(OUT_LOCAL)/priority_queue.o $(OUT_LOCAL)/dynamic_array.o $(OUT_LOCAL)/lightstack.o $(OUT_LOCAL)/optimizer.o $(OUT_LOCAL)/jump_table.o $(OUT_LOCAL)/stack_data_area.o $(OUT_LOCAL)/dynamic_string.o $(OUT_LOCAL)/nesting_stack.o

instruction_selector_test: parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instruction.o heap_queue.o preproc.o dependency_tree.o priority_queue.o dynamic_array.o lightstack.o jump_table.o optimizer.o stack_data_area.o dynamic_string.o nesting_stack.o instruction_selector.o instruction_selector_test.o
	$(CC) -o $(OUT_LOCAL)/instruction_selector_test $(OUT_LOCAL)/parser.o $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/heapstack.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/symtab.o $(OUT_LOCAL)/type_system.o $(OUT_LOCAL)/ast.o $(OUT_LOCAL)/cfg.o $(OUT_LOCAL)/call_graph.o $(OUT_LOCAL)/instruction.o $(OUT_LOCAL)/heap_queue.o $(OUT_LOCAL)/preproc.o $(OUT_LOCAL)/dependency_tree.o $(OUT_LOCAL)/priority_queue.o $(OUT_LOCAL)/dynamic_array.o $(OUT_LOCAL)/lightstack.o $(OUT_LOCAL)/optimizer.o $(OUT_LOCAL)/jump_table.o $(OUT_LOCAL)/stack_data_area.o $(OUT_LOCAL)/dynamic_string.o $(OUT_LOCAL)/nesting_stack.o $(OUT_LOCAL)/instruction_selector.o $(OUT_LOCAL)/instruction_selector_test.o

instruction_selector_testd: parserd.o lexerd.o symtabd.o heapstackd.o type_systemd.o astd.o cfgd.o call_graphd.o lexstackd.o instructiond.o heap_queued.o preprocd.o dependency_treed.o priority_queued.o dynamic_arrayd.o lightstackd.o jump_tabled.o optimizerd.o stack_data_aread.o dynamic_stringd.o nesting_stackd.o instruction_selectord.o instruction_selector_testd.o
	$(CC) -o $(OUT_LOCAL)/instruction_selector_testd $(OUT_LOCAL)/parserd.o $(OUT_LOCAL)/lexerd.o $(OUT_LOCAL)/heapstackd.o $(OUT_LOCAL)/lexstackd.o $(OUT_LOCAL)/symtabd.o $(OUT_LOCAL)/type_systemd.o $(OUT_LOCAL)/astd.o $(OUT_LOCAL)/cfgd.o $(OUT_LOCAL)/call_graphd.o $(OUT_LOCAL)/instructiond.o $(OUT_LOCAL)/heap_queued.o $(OUT_LOCAL)/preprocd.o $(OUT_LOCAL)/dependency_treed.o $(OUT_LOCAL)/priority_queued.o $(OUT_LOCAL)/dynamic_arrayd.o $(OUT_LOCAL)/lightstackd.o $(OUT_LOCAL)/optimizerd.o $(OUT_LOCAL)/jump_tabled.o $(OUT_LOCAL)/stack_data_aread.o $(OUT_LOCAL)/dynamic_stringd.o $(OUT_LOCAL)/nesting_stackd.o $(OUT_LOCAL)/instruction_selectord.o $(OUT_LOCAL)/instruction_selector_testd.o

oc: compiler.o parser.o lexer.o symtab.o heapstack.o type_system.o ast.o cfg.o call_graph.o lexstack.o instruction.o heap_queue.o preproc.o dependency_tree.o priority_queue.o dynamic_array.o lightstack.o optimizer.o instruction_selector.o jump_table.o stack_data_area.o register_allocator.o instruction_scheduler.o interference_graph.o file_builder.o dynamic_string.o nesting_stack.o postprocessor.o
	$(CC) -o $(OUT_LOCAL)/oc $(OUT_LOCAL)/compiler.o $(OUT_LOCAL)/parser.o $(OUT_LOCAL)/lexer.o $(OUT_LOCAL)/heapstack.o $(OUT_LOCAL)/lexstack.o $(OUT_LOCAL)/symtab.o $(OUT_LOCAL)/type_system.o $(OUT_LOCAL)/ast.o $(OUT_LOCAL)/cfg.o $(OUT_LOCAL)/call_graph.o $(OUT_LOCAL)/instruction.o $(OUT_LOCAL)/heap_queue.o $(OUT_LOCAL)/preproc.o $(OUT_LOCAL)/dependency_tree.o $(OUT_LOCAL)/priority_queue.o $(OUT_LOCAL)/dynamic_array.o $(OUT_LOCAL)/lightstack.o $(OUT_LOCAL)/optimizer.o $(OUT_LOCAL)/instruction_selector.o $(OUT_LOCAL)/jump_table.o $(OUT_LOCAL)/stack_data_area.o $(OUT_LOCAL)/register_allocator.o $(OUT_LOCAL)/instruction_scheduler.o $(OUT_LOCAL)/interference_graph.o $(OUT_LOCAL)/file_builder.o $(OUT_LOCAL)/dynamic_string.o $(OUT_LOCAL)/nesting_stack.o $(OUT_LOCAL)/postprocessor.o

oc_debug: compilerd.o parserd.o lexerd.o symtabd.o heapstackd.o type_systemd.o astd.o cfgd.o call_graphd.o lexstackd.o instructiond.o heap_queued.o preprocd.o dependency_treed.o priority_queued.o dynamic_arrayd.o lightstackd.o optimizerd.o instruction_selectord.o jump_tabled.o stack_data_aread.o register_allocatord.o instruction_schedulerd.o interference_graphd.o file_builderd.o dynamic_stringd.o nesting_stackd.o postprocessord.o
	$(CC) -o $(OUT_LOCAL)/ocd $(OUT_LOCAL)/compilerd.o $(OUT_LOCAL)/parserd.o $(OUT_LOCAL)/lexerd.o $(OUT_LOCAL)/heapstackd.o $(OUT_LOCAL)/symtabd.o $(OUT_LOCAL)/lexstackd.o $(OUT_LOCAL)/type_systemd.o $(OUT_LOCAL)/astd.o $(OUT_LOCAL)/cfgd.o $(OUT_LOCAL)/call_graphd.o $(OUT_LOCAL)/instructiond.o $(OUT_LOCAL)/heap_queued.o $(OUT_LOCAL)/preprocd.o $(OUT_LOCAL)/dependency_treed.o $(OUT_LOCAL)/priority_queued.o $(OUT_LOCAL)/dynamic_arrayd.o $(OUT_LOCAL)/lightstackd.o $(OUT_LOCAL)/optimizerd.o $(OUT_LOCAL)/instruction_selectord.o $(OUT_LOCAL)/jump_tabled.o $(OUT_LOCAL)/stack_data_aread.o $(OUT_LOCAL)/register_allocatord.o $(OUT_LOCAL)/instruction_schedulerd.o $(OUT_LOCAL)/interference_graphd.o $(OUT_LOCAL)/file_builderd.o $(OUT_LOCAL)/dynamic_stringd.o $(OUT_LOCAL)/nesting_stackd.o $(OUT_LOCAL)/postprocessord.o

stest: symtab_test
	$(OUT_LOCAL)/symtab_test

stestd: symtab_testd
	$(OUT_LOCAL)/symtab_testd

test_data_area: stack_data_area_test
	$(OUT_LOCAL)/stack_data_area_test -f ./oc/test_files/data_area_test_input.ol

ptest: parser_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_LOCAL)/parser_test -i -d -f
	
ptest-debug: parser_test_debug
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_LOCAL)/parser_test_debug -i -d -f

front_test: front_end_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_LOCAL)/front_end_test -i -t -d -f

# No timed output
front_test_non_timed: front_end_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_LOCAL)/front_end_test -i -d -f

middle_test: middle_end_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_LOCAL)/middle_end_test -i -d -t -f

middle_test_non_timed: middle_end_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_LOCAL)/middle_end_test -i -d -f

selector_test: instruction_selector_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_LOCAL)/instruction_selector_test -i -d -t -f 

selector_test_non_timed: instruction_selector_test
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_LOCAL)/instruction_selector_test -i -d -f

string_test: dynamic_string_test
	$(OUT_LOCAL)/dynamic_string_test

compiler_test: oc
	@for input in $(inputs); do \
		output=$$(echo $$input | sed 's|^$(TEST_FILE_DIR)|$(OUTPUTTED_ASSEMBLY_DIR)|' | sed 's|\.ol$$|.s|'); \
		echo "Running ./oc/out/oc -ditsa -f $$input -o $$output"; \
		./oc/out/oc -ditsa -f $$input -o $$output; \
	done

# This is for our comparisons - a non-timed test
compiler_test_non_timed: oc
	@for input in $(inputs); do \
		output=$$(echo $$input | sed 's|^$(TEST_FILE_DIR)|$(OUTPUTTED_ASSEMBLY_DIR)|' | sed 's|\.ol$$|.s|'); \
		echo "Running ./oc/out/oc -disa -f $$input -o $$output"; \
		./oc/out/oc -disa -f $$input -o $$output; \
	done

# This test is both non-timed *and* only prints out register allocations
compiler_test_allocation_only: oc
	@for input in $(inputs); do \
		output=$$(echo $$input | sed 's|^$(TEST_FILE_DIR)|$(OUTPUTTED_ASSEMBLY_DIR)|' | sed 's|\.ol$$|.s|'); \
		echo "Running ./oc/out/oc -disa -f $$input -o $$output"; \
		./oc/out/oc -sra -f $$input -o $$output; \
	done

array_test: dynamic_array_test
	$(OUT_LOCAL)/dynamic_array_test

interference_graph_test: interference_graph_tester
	$(OUT_LOCAL)/interference_graph_test

######################################################################## CI VERSIONS #############################################################################

ltest-CI: lexer_test-CI
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_CI)/lexer_test

lexer_test-CI: lexer-CI.o lexer_test-CI.o lexstack-CI.o dynamic_string-CI.o
	$(CC) -o $(OUT_CI)/lexer_test $(OUT_CI)/lexer_test.o $(OUT_CI)/lexer.o $(OUT_CI)/lexstack.o $(OUT_CI)/dynamic_string.o

lexer_test-CI.o: $(TEST_SUITE_PATH)/lexer_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/lexer_test.c -o $(OUT_CI)/lexer_test.o

lexer-CI.o: $(LEX_PATH)/lexer.c
	$(CC) $(CFLAGS) $(LEX_PATH)/lexer.c -o $(OUT_CI)/lexer.o

heapstack-CI.o: $(STACK_PATH)/heapstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/heapstack.c -o $(OUT_CI)/heapstack.o

heap_queue-CI.o: $(QUEUE_PATH)/heap_queue.c
	$(CC) $(CFLAGS) $(QUEUE_PATH)/heap_queue.c -o $(OUT_CI)/heap_queue.o

dynamic_array-CI.o: $(DYNAMIC_ARRAY_PATH)/dynamic_array.c
	$(CC) $(CFLAGS) $(DYNAMIC_ARRAY_PATH)/dynamic_array.c -o $(OUT_CI)/dynamic_array.o

dynamic_string-CI.o: $(DYNAMIC_STRING_PATH)/dynamic_string.c
	$(CC) $(CFLAGS) $(DYNAMIC_STRING_PATH)/dynamic_string.c -o $(OUT_CI)/dynamic_string.o

priority_queue-CI.o: $(QUEUE_PATH)/priority_queue.c
	$(CC) $(CFLAGS) $(QUEUE_PATH)/priority_queue.c -o $(OUT_CI)/priority_queue.o

lexstack-CI.o: $(STACK_PATH)/lexstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/lexstack.c -o $(OUT_CI)/lexstack.o

lightstack-CI.o: $(STACK_PATH)/lightstack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/lightstack.c -o $(OUT_CI)/lightstack.o

nesting_stack-CI.o: $(STACK_PATH)/nesting_stack.c
	$(CC) $(CFLAGS) $(STACK_PATH)/nesting_stack.c -o $(OUT_CI)/nesting_stack.o

ast-CI.o: $(AST_PATH)/ast.c
	$(CC) $(CFLAGS) $(AST_PATH)/ast.c -o $(OUT_CI)/ast.o

preproc-CI.o: $(PREPROC_PATH)/preprocessor.c
	$(CC) $(CFLAGS) $(PREPROC_PATH)/preprocessor.c -o $(OUT_CI)/preproc.o

dependency_tree-CI.o: $(DEPENDENCY_TREE_PATH)/dependency_tree.c
	$(CC) $(CFLAGS) $(DEPENDENCY_TREE_PATH)/dependency_tree.c -o $(OUT_CI)/dependency_tree.o

stack_data_area-CI.o: $(STACK_DATA_AREA_PATH)/stack_data_area.c
	$(CC) $(CFLAGS) $(STACK_DATA_AREA_PATH)/stack_data_area.c -o $(OUT_CI)/stack_data_area.o

stack_data_area_test-CI.o: $(TEST_SUITE_PATH)/stack_data_area_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/stack_data_area_test.c -o $(OUT_CI)/stack_data_area_test.o

interference_graph_test-CI.o: $(TEST_SUITE_PATH)/interference_graph_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/interference_graph_test.c -o $(OUT_CI)/interference_graph_test.o

symtab-CI.o: $(SYMTAB_PATH)/symtab.c
	$(CC) $(CFLAGS) $(SYMTAB_PATH)/symtab.c -o $(OUT_CI)/symtab.o

jump_table-CI.o: $(JUMP_TABLE_PATH)/jump_table.c
	$(CC) $(CFLAGS) $(JUMP_TABLE_PATH)/jump_table.c -o $(OUT_CI)/jump_table.o

cfg-CI.o: $(CFG_PATH)/cfg.c
	$(CC) $(CFLAGS) $(CFG_PATH)/cfg.c -o $(OUT_CI)/cfg.o

optimizer-CI.o: $(OPTIMIZER_PATH)/optimizer.c
	$(CC) $(CFLAGS) $(OPTIMIZER_PATH)/optimizer.c -o $(OUT_CI)/optimizer.o

instruction_selector-CI.o: $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c
	$(CC) $(CFLAGS) $(INSTRUCTION_SELECTOR_PATH)/instruction_selector.c -o $(OUT_CI)/instruction_selector.o

instruction_scheduler-CI.o: $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c
	$(CC) $(CFLAGS) $(INSTRUCTION_SCHEDULER_PATH)/instruction_scheduler.c -o $(OUT_CI)/instruction_scheduler.o

register_allocator-CI.o: $(REGISTER_ALLOCATOR_PATH)/register_allocator.c
	$(CC) $(CFLAGS) $(REGISTER_ALLOCATOR_PATH)/register_allocator.c -o $(OUT_CI)/register_allocator.o

file_builder-CI.o: $(FILE_BUILDER_PATH)/file_builder.c
	$(CC) $(CFLAGS) $(FILE_BUILDER_PATH)/file_builder.c -o $(OUT_CI)/file_builder-CI.o

interference_graph-CI.o: $(INTERFERENCE_GRAPH_PATH)/interference_graph.c
	$(CC) $(CFLAGS) $(INTERFERENCE_GRAPH_PATH)/interference_graph.c -o $(OUT_CI)/interference_graph.o

type_system-CI.o: $(TYPE_SYSTEM_PATH)/type_system.c
	$(CC) $(CFLAGS) $(TYPE_SYSTEM_PATH)/type_system.c -o $(OUT_CI)/type_system.o

postprocessor-CI.o: $(POSTPROCESSOR_PATH)/postprocessor.c
	$(CC) $(CFLAGS) $(POSTPROCESSOR_PATH)/postprocessor.c -o $(OUT_CI)/postprocessor.o

parser-CI.o: $(PARSER_PATH)/parser.c
	$(CC) $(CFLAGS) $(PARSER_PATH)/parser.c -o $(OUT_CI)/parser.o

symtab_test-CI.o: $(TEST_SUITE_PATH)/symtab_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/symtab_test.c -o $(OUT_CI)/symtab_test.o

dynamic_array_test-CI.o: $(TEST_SUITE_PATH)/dynamic_array_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/dynamic_array_test.c -o $(OUT_CI)/dynamic_array_test.o

dynamic_string_test-CI.o: $(TEST_SUITE_PATH)/dynamic_string_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/dynamic_string_test.c -o $(OUT_CI)/dynamic_string_test.o

dynamic_string_test-CI: dynamic_string_test-CI.o dynamic_string-CI.o
	$(CC) -o $(OUT_CI)/dynamic_string_test $(OUT_CI)/dynamic_string_test.o $(OUT_CI)/dynamic_string.o

dynamic_array_test-CI: dynamic_array_test-CI.o dynamic_array-CI.o
	$(CC) -o $(OUT_CI)/dynamic_array_test $(OUT_CI)/dynamic_array_test.o $(OUT_CI)/dynamic_array.o

parser_test-CI.o: $(TEST_SUITE_PATH)/parser_test.c
	$(CC) $(CFLAGS) $(TEST_SUITE_PATH)/parser_test.c -o $(OUT_CI)/parser_test.o

parser_test-CI: parser-CI.o lexer-CI.o parser_test-CI.o symtab-CI.o lexstack-CI.o heapstack-CI.o type_system-CI.o ast-CI.o call_graph-CI.o heap_queue-CI.o lightstack-CI.o dynamic_array-CI.o stack_data_area-CI.o instruction-CI.o dynamic_string-CI.o nesting_stack-CI.o
	$(CC) -o $(OUT_CI)/parser_test $(OUT_CI)/parser_test.o $(OUT_CI)/parser.o $(OUT_CI)/lexstack.o $(OUT_CI)/lexer.o $(OUT_CI)/heapstack.o $(OUT_CI)/symtab.o $(OUT_CI)/type_system.o $(OUT_CI)/ast.o $(OUT_CI)/call_graph.o $(OUT_CI)/heap_queue.o $(OUT_CI)/lightstack.o $(OUT_CI)/dynamic_array.o $(OUT_CI)/stack_data_area.o $(OUT_CI)/instruction.o $(OUT_CI)/dynamic_string.o $(OUT_CI)/nesting_stack.o

symtab_test-CI: symtab-CI.o symtab_test-CI.o lexer-CI.o type_system-CI.o lexstack-CI.o lightstack-CI.o stack_data_area-CI.o instruction-CI.o dynamic_array-CI.o parser-CI.o cfg-CI.o ast-CI.o call_graph-CI.o heap_queue-CI.o heapstack-CI.o jump_table-CI.o dynamic_string-CI.o nesting_stack-CI.o
	$(CC) -o $(OUT_CI)/symtab_test $(OUT_CI)/lexer.o $(OUT_CI)/symtab_test.o $(OUT_CI)/symtab.o $(OUT_CI)/type_system.o $(OUT_CI)/lexstack.o $(OUT_CI)/lightstack.o $(OUT_CI)/stack_data_area.o $(OUT_CI)/instruction.o $(OUT_CI)/dynamic_array.o $(OUT_CI)/parser.o $(OUT_CI)/cfg.o $(OUT_CI)/ast.o $(OUT_CI)/call_graph.o $(OUT_CI)/heap_queue.o $(OUT_CI)/heapstack.o $(OUT_CI)/jump_table.o $(OUT_CI)/dynamic_string.o $(OUT_CI)/nesting_stack.o

stack_data_area_test-CI: stack_data_area_test-CI.o type_system-CI.o lexstack-CI.o lightstack-CI.o symtab-CI.o lexer-CI.o instruction-CI.o stack_data_area-CI.o dynamic_array-CI.o ast-CI.o call_graph-CI.o cfg-CI.o parser-CI.o heap_queue-CI.o heapstack-CI.o jump_table-CI.o dynamic_string-CI.o nesting_stack-CI.o
	$(CC) -o $(OUT_CI)/stack_data_area_test $(OUT_CI)/lexer.o $(OUT_CI)/stack_data_area_test.o $(OUT_CI)/symtab.o $(OUT_CI)/type_system.o $(OUT_CI)/lexstack.o $(OUT_CI)/lightstack.o $(OUT_CI)/stack_data_area.o $(OUT_CI)/instruction.o $(OUT_CI)/dynamic_array.o $(OUT_CI)/ast.o $(OUT_CI)/call_graph.o $(OUT_CI)/cfg.o $(OUT_CI)/parser.o $(OUT_CI)/heap_queue.o $(OUT_CI)/heapstack.o $(OUT_CI)/jump_table.o $(OUT_CI)/dynamic_string.o $(OUT_CI)/nesting_stack.o
	
call_graph-CI.o : $(CALL_GRAPH_PATH)/call_graph.c
	$(CC) $(CFLAGS) $(CALL_GRAPH_PATH)/call_graph.c -o $(OUT_CI)/call_graph.o

compiler-CI.o: ./oc/compiler/compiler.c 
	$(CC) $(CFLAGS) -o $(OUT_CI)/compiler.o ./oc/compiler/compiler.c

instruction-CI.o: $(INSTRUCTION_PATH)/instruction.c
	$(CC) $(CFLAGS) -o $(OUT_CI)/instruction.o $(INSTRUCTION_PATH)/instruction.c

front_end_test-CI.o: $(TEST_SUITE_PATH)/front_end_test.c
	$(CC) $(CFLAGS) -o $(OUT_CI)/front_end_test.o $(TEST_SUITE_PATH)/front_end_test.c

middle_end_test-CI.o: $(TEST_SUITE_PATH)/middle_end_test.c
	$(CC) $(CFLAGS) -o $(OUT_CI)/middle_end_test.o $(TEST_SUITE_PATH)/middle_end_test.c

instruction_selector_test-CI.o: $(TEST_SUITE_PATH)/instruction_selector_test.c
	$(CC) $(CFLAGS) -o $(OUT_CI)/instruction_selector_test.o $(TEST_SUITE_PATH)/instruction_selector_test.c

interference_graph_tester-CI: parser-CI.o lexer-CI.o symtab-CI.o heapstack-CI.o type_system-CI.o ast-CI.o cfg-CI.o call_graph-CI.o lexstack-CI.o instruction-CI.o heap_queue-CI.o priority_queue-CI.o dynamic_array-CI.o lightstack-CI.o optimizer-CI.o instruction_selector-CI.o jump_table-CI.o stack_data_area-CI.o interference_graph-CI.o interference_graph_test-CI.o dynamic_string-CI.o nesting_stack-CI.o
	$(CC) -o $(OUT_CI)/interference_graph_test $(OUT_CI)/interference_graph_test.o $(OUT_CI)/parser.o $(OUT_CI)/lexer.o $(OUT_CI)/heapstack.o $(OUT_CI)/lexstack.o $(OUT_CI)/symtab.o $(OUT_CI)/type_system.o $(OUT_CI)/ast.o $(OUT_CI)/cfg.o $(OUT_CI)/call_graph.o $(OUT_CI)/instruction.o $(OUT_CI)/heap_queue.o $(OUT_CI)/priority_queue.o $(OUT_CI)/dynamic_array.o $(OUT_CI)/lightstack.o $(OUT_CI)/optimizer.o $(OUT_CI)/instruction_selector.o $(OUT_CI)/jump_table.o $(OUT_CI)/stack_data_area.o $(OUT_CI)/interference_graph.o $(OUT_CI)/dynamic_string.o $(OUT_CI)/nesting_stack.o

front_end_test-CI: front_end_test-CI.o parser-CI.o lexer-CI.o symtab-CI.o heapstack-CI.o type_system-CI.o ast-CI.o cfg-CI.o call_graph-CI.o lexstack-CI.o instruction-CI.o heap_queue-CI.o preproc-CI.o dependency_tree-CI.o priority_queue-CI.o dynamic_array-CI.o lightstack-CI.o jump_table-CI.o stack_data_area-CI.o dynamic_string-CI.o nesting_stack-CI.o
	$(CC) -o $(OUT_CI)/front_end_test $(OUT_CI)/front_end_test.o $(OUT_CI)/parser.o $(OUT_CI)/lexer.o $(OUT_CI)/heapstack.o $(OUT_CI)/lexstack.o $(OUT_CI)/symtab.o $(OUT_CI)/type_system.o $(OUT_CI)/ast.o $(OUT_CI)/cfg.o $(OUT_CI)/call_graph.o $(OUT_CI)/instruction.o $(OUT_CI)/heap_queue.o $(OUT_CI)/preproc.o $(OUT_CI)/dependency_tree.o $(OUT_CI)/priority_queue.o $(OUT_CI)/dynamic_array.o $(OUT_CI)/lightstack.o $(OUT_CI)/jump_table.o $(OUT_CI)/stack_data_area.o $(OUT_CI)/dynamic_string.o $(OUT_CI)/nesting_stack.o

middle_end_test-CI: middle_end_test-CI.o parser-CI.o lexer-CI.o symtab-CI.o heapstack-CI.o type_system-CI.o ast-CI.o cfg-CI.o call_graph-CI.o lexstack-CI.o instruction-CI.o heap_queue-CI.o preproc-CI.o dependency_tree-CI.o priority_queue-CI.o dynamic_array-CI.o lightstack-CI.o jump_table-CI.o optimizer-CI.o stack_data_area-CI.o dynamic_string-CI.o nesting_stack-CI.o
	$(CC) -o $(OUT_CI)/middle_end_test $(OUT_CI)/middle_end_test.o $(OUT_CI)/parser.o $(OUT_CI)/lexer.o $(OUT_CI)/heapstack.o $(OUT_CI)/lexstack.o $(OUT_CI)/symtab.o $(OUT_CI)/type_system.o $(OUT_CI)/ast.o $(OUT_CI)/cfg.o $(OUT_CI)/call_graph.o $(OUT_CI)/instruction.o $(OUT_CI)/heap_queue.o $(OUT_CI)/preproc.o $(OUT_CI)/dependency_tree.o $(OUT_CI)/priority_queue.o $(OUT_CI)/dynamic_array.o $(OUT_CI)/lightstack.o $(OUT_CI)/optimizer.o $(OUT_CI)/jump_table.o $(OUT_CI)/stack_data_area.o $(OUT_CI)/dynamic_string.o $(OUT_CI)/nesting_stack.o

instruction_selector_test-CI: parser-CI.o lexer-CI.o symtab-CI.o heapstack-CI.o type_system-CI.o ast-CI.o cfg-CI.o call_graph-CI.o lexstack-CI.o instruction-CI.o heap_queue-CI.o preproc-CI.o dependency_tree-CI.o priority_queue-CI.o dynamic_array-CI.o lightstack-CI.o jump_table-CI.o optimizer-CI.o stack_data_area-CI.o dynamic_string-CI.o nesting_stack-CI.o instruction_selector-CI.o instruction_selector_test-CI.o
	$(CC) -o $(OUT_CI)/instruction_selector_test $(OUT_CI)/parser.o $(OUT_CI)/lexer.o $(OUT_CI)/heapstack.o $(OUT_CI)/lexstack.o $(OUT_CI)/symtab.o $(OUT_CI)/type_system.o $(OUT_CI)/ast.o $(OUT_CI)/cfg.o $(OUT_CI)/call_graph.o $(OUT_CI)/instruction.o $(OUT_CI)/heap_queue.o $(OUT_CI)/preproc.o $(OUT_CI)/dependency_tree.o $(OUT_CI)/priority_queue.o $(OUT_CI)/dynamic_array.o $(OUT_CI)/lightstack.o $(OUT_CI)/optimizer.o $(OUT_CI)/jump_table.o $(OUT_CI)/stack_data_area.o $(OUT_CI)/dynamic_string.o $(OUT_CI)/nesting_stack.o $(OUT_CI)/instruction_selector.o $(OUT_CI)/instruction_selector_test.o

oc-CI: compiler-CI.o parser-CI.o lexer-CI.o symtab-CI.o heapstack-CI.o type_system-CI.o ast-CI.o cfg-CI.o call_graph-CI.o lexstack-CI.o instruction-CI.o heap_queue-CI.o preproc-CI.o dependency_tree-CI.o priority_queue-CI.o dynamic_array-CI.o lightstack-CI.o optimizer-CI.o instruction_selector-CI.o jump_table-CI.o stack_data_area-CI.o register_allocator-CI.o instruction_scheduler-CI.o interference_graph-CI.o file_builder-CI.o dynamic_string-CI.o nesting_stack-CI.o postprocessor-CI.o
	$(CC) -o $(OUT_CI)/oc $(OUT_CI)/compiler.o $(OUT_CI)/parser.o $(OUT_CI)/lexer.o $(OUT_CI)/heapstack.o $(OUT_CI)/lexstack.o $(OUT_CI)/symtab.o $(OUT_CI)/type_system.o $(OUT_CI)/ast.o $(OUT_CI)/cfg.o $(OUT_CI)/call_graph.o $(OUT_CI)/instruction.o $(OUT_CI)/heap_queue.o $(OUT_CI)/preproc.o $(OUT_CI)/dependency_tree.o $(OUT_CI)/priority_queue.o $(OUT_CI)/dynamic_array.o $(OUT_CI)/lightstack.o $(OUT_CI)/optimizer.o $(OUT_CI)/instruction_selector.o $(OUT_CI)/jump_table.o $(OUT_CI)/stack_data_area.o $(OUT_CI)/register_allocator.o $(OUT_CI)/instruction_scheduler.o $(OUT_CI)/interference_graph.o $(OUT_CI)/file_builder-CI.o $(OUT_CI)/dynamic_string.o $(OUT_CI)/nesting_stack.o $(OUT_CI)/postprocessor.o

stest-CI: symtab_test-CI
	$(OUT_CI)/symtab_test

test_data_area-CI: stack_data_area_test-CI
	$(OUT_CI)/stack_data_area_test -f ./oc/test_files/data_area_test_input.ol

ptest-CI: parser_test-CI
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_CI)/parser_test -i -d -f

front_test-CI: front_end_test-CI
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_CI)/front_end_test -i -d -f

middle_test-CI: middle_end_test-CI
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_CI)/middle_end_test -i -d -f

selector_test-CI: instruction_selector_test-CI
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_CI)/instruction_selector_test -i -d -f

compiler_test-CI: oc-CI
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_CI)/oc -s -t -@ -i -d -f

compiler_test-non-timed-CI: oc-CI
	find $(TEST_FILE_DIR) -type f | sort | xargs -n 1 $(OUT_CI)/oc -s -@ -i -d -f

array_test-CI: dynamic_array_test-CI
	$(OUT_CI)/dynamic_array_test

string_test-CI: dynamic_string_test-CI
	$(OUT_CI)/dynamic_string_test

interference_graph_test-CI: interference_graph_tester-CI
	$(OUT_CI)/interference_graph_test

############################################################## CLEAN UTILITY(LOCAL ONLY) ###############################################

clean:
	rm -f $(OUT_LOCAL)/*
