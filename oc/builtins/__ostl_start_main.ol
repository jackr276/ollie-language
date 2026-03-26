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
	/**
	* Our result is already in %rdi because of the parameter
	* passing convention, so all we need to do here is move
	* the code for the SYS_exit_group(231) into here
	*/
	asm {
		"
		movq $231, %rax
		syscall
		"
	    };

	//If that fails then we loop forever
	while(true){}
}


/**
 * This function requires a pointer to the main function as well as the argc and argv
 * values
 */
pub fn __ostl_start_main(main_pointer:fn(i32, char**, char**) -> i32, argc:i32, argv:char**) -> i32 {
	//The environment pointer always comes 1 after all of the argument vector arguments
	let envp:char** = &(argv[argc + 1]);

	//Invoke our actual main function
	let result:i32 = @main_pointer(argc, argv, envp);

	//This will kill the process
	@ostl_start_main_exit(result);
}
