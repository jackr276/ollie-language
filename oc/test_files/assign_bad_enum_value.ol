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


pub fn main(argc:i32, argv:char**) -> i32 {
	//Should be fine
	let x:custom_enum = 3;

	//Invalid
	let y:custom_enum = 88;

	ret x + y;
}
