/**
 * Author: Jack Robbins
 * Test an invalid case where we try to assign to an immutable global var
 */


let x:i32 = 5;


pub fn main(argc:i32, argv:char**) -> i32 {
	//INVALID - not mutable
	x = argc;

	OUNIT: [fail_to_compile]
	ret x;
}
