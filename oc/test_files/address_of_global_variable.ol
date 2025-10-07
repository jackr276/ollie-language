/**
* Author: Jack Robbins
* Test taking the address of a global variable
*/

declare global_var:i32;

fn test_func(x:i32*) -> i32 {
	ret *x + 32;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	global_var = argc;

	//Address of a global var
	let x:i32* = &global_var;

	ret @test_func(x);
}
