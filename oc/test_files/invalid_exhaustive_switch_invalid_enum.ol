/**
 * Author: Jack Robbins
 * Attempt to make an exhaustive switch with an enum type that is ineligible
 */


//Not eligible because there is a gap between 1 and 3
define enum my_enum {
	TWO = 2,
	ONE = 1,
	THREE = 3,
	FOUR = 4
};


pub fn invalid_exhaustive(x:enum my_enum) -> i32 {
	//SHOULD FAIL - it's not complete
	switch(x){
		case TWO:
			ret 5;
		case THREE:
			ret 2;
		case FOUR:
			ret 4;
	}
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
