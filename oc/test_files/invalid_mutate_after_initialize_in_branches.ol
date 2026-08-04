/**
 * Author: Jack Robbins
 * Test an invalid case where we mutate after we've initialized a variable
 * in a branch structure
 */


pub fn main(argc:i32, argv:char**) -> i32 {
	declare result:i32;

	if(argc == 1) {
		result = 1;
	} else if (argc == 2) {
		result = 2;
	} else if (argc == 3) {
		result = 3;
	} else {
		result = 4;
	}

	//INVALID - can't reassign here
	result = 5;

	OUNIT: [fail_to_compile]
	ret result;
}
