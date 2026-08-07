/**
 * Author: Jack Robbins
 * Test an invalid case where we may be mutated after some branch
 * paths initialize and some don't
 */

pub fn main(argc:i32, argv:char**) -> i32 {
	declare x:i32;

	if (argc == 3){
		x = 1;
	} else if (argc == 4){
		x = 2;
	} else if (argc == 5){
		x = 3;
	}

	//INVALID - even though x isn't initialized on *all* paths,
	//it still may have been intialized so this is a "maybe"
	//mutation
	x = 7;

	OUNIT: [fail_to_compile]
	ret x;
}
