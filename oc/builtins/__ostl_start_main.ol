/**
 * Author: Jack Robbins
 * This file in the Ollie Standard Library(ostl) will actually
 * handle the calling of our user code.
 */

/**
 * The exit code is going to involve a syscall to the kernel with the given
 * result. Note that on paper this returns void because we're killing a process
 */
fn ostl_start_main_exit(result:i32) -> void { 


}


/**
 * This function requires a pointer to the main function as well as the argc and argv
 * values
 */
pub fn __ostl_start_main(main:fn(i32, char**, char**) -> i32, argc:i32, argv:char**) -> i32 {

}
