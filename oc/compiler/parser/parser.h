/**
 * Parser API file. Exposes all needed external functions used by the compiler
*/

//Include guards
#ifndef PARSER_H
#define PARSER_H

#include "../stack/stack.h"
#include "../symtab/symtab.h"
#include "../lexer/lexer.h"
#include "../type_system/type_system.h"
#include <sys/types.h>

typedef struct parse_message_t parse_message_t;

/**
 * What type of message do we have
 */
typedef enum {
	WARNING=0,
	PARSE_ERROR=1,
	INFO=2,
} parse_message_type_t;


/**
 * A specific type of error that we can give back if needed
 */
struct parse_message_t{
	//What type is it	
	parse_message_type_t message;
	//Info message given
	char* info;
	//Is this a fatal error
	u_int8_t fatal;
	//The line number
	u_int16_t line_num;
};


/**
 * Parse the entirety of the file. Returns 0 if successful
 */
u_int8_t parse(FILE* FL);

#endif /* PARSER_H */
