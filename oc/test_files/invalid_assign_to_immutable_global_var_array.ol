/**
 * Author: Jack Robbins
 * Test an invalid case where we try to assign to an immutable global var array
 */


let x:i32[] = [1, 2, 3, 4, 5, 6];


pub fn main(argc:i32, argv:char**) -> i32 {
	//INVALID - not mutable
	x[3] = argc;

	OUNIT: [fail_to_compile]
	ret x[2];
}
