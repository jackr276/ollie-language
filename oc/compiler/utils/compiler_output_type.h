/**
 * Author: Jack Robbins
 * Encode what kind of output we want for the compiler to generate. This could
 * be but is not limited to: assembly only, object file only, or a full compilation.
 * By default we assume that a full compilation is the goal
*/ 

#ifndef COMPILER_OUTPUT_TYPE_H
#define COMPILER_OUTPUT_TYPE_H

typedef enum {
	OUTPUT_TYPE_FULL_COMPILATION,
	OUTPUT_TYPE_OBJECT_FILE,
	OUTPUT_TYPE_ASSEMBLY_ONLY,
} compiler_output_type_t;


#endif /* COMPILER_OUTPUT_TYPE_H */
