/**
 * Author: Jack Robbins
 * Attempt to make an exhaustive switch with an enum type that is ineligible
 */


//Not eligible because there is a gap between 1 and 3
define enum my_enum {
	TWO = 3,
	ONE = 1,
	THREE = 4,
	FOUR = 5
};


pub fn invalid_exhaustive(x:enum my_enum) -> i32 {
	//Should fail - the type is not eligible
	switch(x){
		case TWO:
			ret 5;
		case ONE:
			ret 4;
		case THREE:
			ret 2;
		case FOUR:
			ret 4;
	}
}


pub fn main() -> i32 {
	ret 0;
}
