/**
 * Author: Jack Robbins
 * Test an invalid case where we use an uninitialized value in an initializer
 */


pub fn main(argc:i32, argv:char**) -> i32 {
	declare x:i32;

	if(argc > 0) {
		x = 5;
	}

	//BAD - maybe uninitialized
	let arr:i32[] = [1, 2, 3, 4, x];

	OUNIT: [fail_to_compile]
	ret 0;
}
