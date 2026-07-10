/**
* Author: Jack Robbins
* Test an exhaustive switch that has a bad input type and therefore will not
* work right
*/

/**
* An enum class
*/
define enum type_enum{
	TYPE_ONE,
	TYPE_TWO,
	TYPE_THREE,
	TYPE_FOUR,
	TYPE_FIVE
} as my_enum_type;

fn tester(param:my_enum_type) -> i32{
	let x:mut i32 = 32;

	switch(param){
		case TYPE_ONE -> {
			x = 32;
		}
		case TYPE_TWO -> {
			x = -3;
		}
		case TYPE_FOUR -> {
			x = -2;

		}
		case TYPE_THREE -> {
			x = 211;
		}

		case TYPE_FIVE -> {
			x = 22;
		}
	}

	//So it isn't optimized away
	ret x;
}


pub fn main() -> i32{
	let x:i32 = 88;

	OUNIT: [fail_to_compile]
	//BAD - can't have an i32 in here - must be an enum of the same type
	ret @tester(x);
}
