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

	//ILLEGAL - this should fail because we have definitely
	//been initialized by the time we get here
	x = 7;

	OUNIT: [fail_to_compile]
	ret x;
}
