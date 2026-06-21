/**
* Author: Jack Robbins
* Test an in statement with an enum and an invalid comparator
*/

define enum my_enum {
	ONE,
	TWO,
	THREE,
	FOUR,
	FIVE,
	SIX,
	SEVEN,
	EIGHT
};


//Should fail - can't do this with an f32
pub fn is_in_enum_invalid(x:f32) -> i32 {
	ret x in (ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT);
}


pub fn main() -> i32 {
	ret @is_in_enum_invalid(5.555);
	
}
