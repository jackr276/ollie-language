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
	printf("LINE NUMBER %d: TOKEN %s\n", lexitem->line_num, lexitem_to_string(lexitem));
}


int main(int argc, char** argv){
	if(argc < 2){
		fprintf(stderr, "A filename must be provided\n");
	}

	FILE* fl;
	lexitem_t l;

	for(int32_t i = 1; i < argc; i++){
		printf("=============== LEXER TEST FOR FILE %s =================\n\n", argv[i]);

		//Open the file for reading only
		fl = fopen(argv[i], "r");
	
		if(fl == NULL){
			fprintf(stderr, "FILE could not be opened\n");
			return 1;
		}

		//Let the helper do all of the work
		ollie_token_stream_t token_stream = tokenize(fl, argv[i]);

		//Did we succeed or fail here
		switch(token_stream.status){
			case STREAM_STATUS_FAILURE:
				printf("Tokenizing FAILED\n");
				//Close the file when done
				fclose(fl);

				continue;

			case STREAM_STATUS_SUCCESS:
				//Close the file when done
				fclose(fl);

				break;
		}

		//This is usually how called functions will reference this
		ollie_token_stream_t* token_stream_pointer = &token_stream;

		//Grab the first one
		l = get_next_token(token_stream_pointer);

		//Print it
		print_token(&l);

		//The first token's seek
		u_int32_t first_token_index = GET_CURRENT_TOKEN_INDEX(token_stream_pointer);

		//Very rudimentary here
		while((l = get_next_token(token_stream_pointer)).tok != DONE){
			print_token(&l);
		}

		//Let's see if we can now "Reconsume" the tokens starting at a given position
		printf("=============== RECONSUMING FROM %d ====================\n", first_token_index);

		//Reset the stream to the very start
		reset_stream_to_given_index(token_stream_pointer, first_token_index);

		//Very rudimentary here
		while((l = get_next_token(token_stream_pointer)).tok != DONE){
			print_token(&l);
		}
		//Print the last one
		print_token(&l);

		//Let's see if we can now "Reconsume" the tokens starting at a given position
		printf("=============== DONE ====================\n");

		//Print the last one
		print_token(&l);

		//Destroy the entire array
		destroy_token_stream(token_stream_pointer);
	}

	return 0;
}
