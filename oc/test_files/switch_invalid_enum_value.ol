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
	switch(arg){
		case TYPE_ONE:
			ret 7;

		case TYPE_TWO:
			ret 5;

		//This should work - 3 is in the range
		case 3:
			ret 3;

		//This should fail. It's not possible
		//to put a seven in that enum type
		case 7:
			ret -3;

		default:
			ret 0;
	}
}


pub fn main(argc:i32, argv:char**) -> i32 {
	ret @tester(TYPE_ONE);
}
