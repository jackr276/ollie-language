/**
* Author: Jack Robbins
* Test an invalid case where we have an enum value more than once
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


pub fn is_in_enum_int(x:i32) -> i32 {
	//Should fail because EIGHT is there twice
	ret x in (ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, EIGHT);
}


pub fn is_in_enum_enum(x:enum my_enum) -> i32 {
	ret x in (ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT);
}


pub fn main() -> i32 {
	ret @is_in_enum_int(THREE) + @is_in_enum_enum(FOUR);
}
