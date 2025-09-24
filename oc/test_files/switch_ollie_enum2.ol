/**
* Author: Jack Robbins
* Test switching on an enum
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

fn tester(param:i32) -> i32{
	let mut x:i32 = 32;

	switch(param){
		case TYPE_ONE -> {
			x = 32;
		}
		case TYPE_TWO -> {
			x = -3;
		}
		case 7 -> {}
		case TYPE_THREE -> {
			x = 211;
		}

		case 9 -> {
			x = 22;
		}

		default -> {
			x = x - 22;
		}
	}

	//So it isn't optimized away
	ret x;
}


pub fn main() -> i32{
	@tester(TYPE_ONE);
}
