/**
 * Generic testing suite for the lexer exclusively
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
//Link to the lexer
#include "../lexer/lexer.h"

/**
 * A small helper that prints the token out
*/
static inline void print_token(lexitem_t* lexitem){
	printf("LINE NUMBER %d: TOKEN %s", lexitem->line_num, lexitem_to_string(lexitem));
}


int main(int argc, char** argv){
	if(argc < 2){
		fprintf(stderr, "A filename must be provided\n");
	}

	FILE* fl;
	lexitem_t l;
	//Line number must be 32 bit
	u_int32_t parser_line_num;

	for(int32_t i = 1; i < argc; i++){
		//Open the file for reading only
		fl = fopen(argv[i], "r");
	
		if(fl == NULL){
			fprintf(stderr, "FILE could not be opened\n");
			return 1;
		}

		//Let the helper do all of the work
		ollie_token_stream_t token_stream = tokenize(fl, argv[i]);

		//Close the file when done
		fclose(fl);

		//Grab the first one
		l = get_next_token(&token_stream);

		//Print it
		print_token(&l);

		//The current seek head
		int64_t first_token_seek = GET_CURRENT_FILE_POSITION(fl);

		//Very rudimentary here
		while((l = get_next_token(fl, &parser_line_num)).tok != DONE){
			print_token(&l);
		}

		//Let's see if we can now "Reconsume" the tokens starting at a given position
		printf("=============== RECONSUMING FROM %ld ====================\n", first_token_seek);

		//Invoke the reconsumer
		reconsume_tokens(fl, first_token_seek);

		//Very rudimentary here
		while((l = get_next_token(fl, &parser_line_num)).tok != DONE){
			print_token(&l);
		}
		//Print the last one
		print_token(&l);

		//Let's see if we can now "Reconsume" the tokens starting at a given position
		printf("=============== DONE ====================\n");

		//Print the last one
		print_token(&l);


		//Destroy the entire array
		destroy_token_stream(&token_stream);
	}

	return 0;
}
