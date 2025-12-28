/**
* Author: Jack Robbins
* Testing a switch that is not exhaustive and as such needs a default
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

	//This will fail - the switch is not exhaustive and needs a default
	switch(param){
		case TYPE_ONE -> {
			x = 32;
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
	@tester(TYPE_ONE);
}
