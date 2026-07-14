/**
 * Author: Jack Robbins
 * This header file defines the external APIs for the Ollie build system
 */

#ifndef BUILD_SYSTEM_H
#define BUILD_SYSTEM_H

#include "../lexer/lexer.h"
#include "../utils/utility_structs.h"

/**
 * The main and only entry point to the build system revolves around
 * us parsing dependencies and constructing them into one gigantic, unified token
 * stream. This token stream is what we will use to actually parse and construct
 * the overall CFG
 */
ollie_token_stream_t* parse_dependencies_and_construct_token_stream(compiler_options_t* options);

#endif /* BUILD_SYSTEM_H */
