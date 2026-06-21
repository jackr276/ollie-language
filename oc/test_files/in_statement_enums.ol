/**
* Author: Jack Robbins
* Test an in statement with an enum
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
	ret x in (ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT);
}


pub fn is_in_enum_enum(x:enum my_enum) -> i32 {
	ret x in (ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT);
}


pub fn main() -> i32 {
	//Should return 1 + 1 = 2
	OUNIT: [console = 2]
	ret @is_in_enum_int(THREE) + @is_in_enum_enum(FOUR);
	
}
