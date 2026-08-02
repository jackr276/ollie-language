/**
 * Author: Jack Robbins
 * Test a case where we may use a variable before it's intialized at a join
 * node
 */


pub fn main(argc:i32, argv:char**) -> i32 {
	declare x:mut i32;

	if(argc == 0){
		x = 5;
	}

	//Should fail because there is a path where x is uninitialized
	OUNIT: [fail_to_compile]
	ret x;
}
