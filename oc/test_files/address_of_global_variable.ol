/**
* Author: Jack Robbins
* Test taking the address of a global variable
*/

declare global_var:mut i32;

fn test_func(x:i32*) -> i32 {
	ret *x + 32;
}

//Dummy
fn ret_param(x:i32) -> i32 {
	ret x;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	global_var = argc;

	//Address of a global var
	let x:mut i32* = &global_var + 3;

	//Try to offset it
	@ret_param(x[argc]);

	//Test a bin op
	@test_func(&global_var + argc);

	ret @test_func(x);
}
