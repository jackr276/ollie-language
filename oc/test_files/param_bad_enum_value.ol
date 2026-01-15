/**
* Author: Jack Robbins
* Test switching on an enum value with an invalid member
*/

define enum my_enum {
	TYPE_ONE,
	TYPE_TWO,
	TYPE_THREE,
	TYPE_FOUR
} as custom_enum;

fn tester(arg:custom_enum) -> i32 {
	ret arg + 1;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	//Should be fine
	let x:i32 = @tester(TYPE_ONE);

	//Should fail - we don't have an equivalent of 6 in there
	let y:i32 = @tester(6);

	ret x + y;
}
