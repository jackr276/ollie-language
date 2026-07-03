/**
 * Author: Jack Robbins
 * Attempt to make an exhaustive switch with an enum type that is eligible, but a switch
 * that does not use all of the values
 */


//This is eligible
define enum my_enum {
	TWO = 3,
	ONE = 1,
	THREE = 4,
	FOUR = 5,
	FIVE = 2
};


pub fn invalid_exhaustive(x:enum my_enum) -> i32 {
	//Should fail because we don't have all the values inside
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
