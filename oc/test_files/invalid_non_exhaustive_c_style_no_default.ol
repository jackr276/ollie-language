/**
 * Author: Jack Robbins
 * Make a valid exhaustive switch with the values out of order
 */


//This is eligible
define enum my_enum {
	TWO = 3,
	ONE = 1,
	THREE = 4,
	FOUR = 5,
	FIVE = 2
};


pub fn valid_exhaustive(x:enum my_enum) -> i32 {
	//SHOULD FAIL - we're missing one of our enums
	switch(x){
		case TWO:
			ret 5;
		case THREE:
			ret 2;
		case FOUR:
			ret 4;
		case FIVE:
			ret 1;
	}
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @valid_exhaustive(FOUR);
}
