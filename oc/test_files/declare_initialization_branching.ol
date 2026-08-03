/**
 * Author: Jack Robbins
 * Test declare intiailization for immutable variables via branching
 */


pub fn main(argc:i32, argv:char**) -> i32 {
	declare x:i32;
	
	if(argc == 1){
		x = 5;
	} else {
		x = 9;
	}

	OUNIT: [exit_status = 5]
	ret x;
}
