/**
* Author: Jack Robbins
* Testing type coercion rules
*/

pub fn main(argc:i32, argv:char**) -> i32 {
	define struct s {
		x:i32;
		y:char;
	} as my_struct;

	let mut custom:my_struct := {5, 'a'};

	//Should be casted to an int
	ret custom:y + custom:x;
}
