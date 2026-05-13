/**
 *  Author: Jack Robbins
 *
 *  The Ollie Run Validator has been designed to, in conjuction with the memory checker,
 *  give us automated capabilities when validating tests that are able to be validated
 *  via run. What does it mean to be validated via run? - if we compile and run the program,
 *  the *shell exit code* should match what we are expecting. Remember that the shell
 *  exit code is what main returns. Some tests have been designed to do this to validate
 *  full end-to-end runs
 *
 */

//Use our own in-house dynamic array
#include "../utils/dynamic_array/dynamic_array.h"
//We will be doing this multithreaded
#include <pthread.h>

//We'll need a mutex for all of the files that we wish to operate on
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * A generic array containing *all* of our test files that will be worked over
 * by the threads. We then also maintain a list of all files that are found
 * to be in error. This is done for the final summary
 */
dynamic_array_t test_files;
dynamic_array_t error_files;


/**
 * Entry point. This will perform our setup and call into
 * our threads
 */
int main() {

}
