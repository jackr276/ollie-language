/**
* Author: Jack Robbins
* Test an in statement that can be live evaluated by the parser
*/


define enum my_enum {
	ONE,
	TWO,
	THREE,
	FOUR,
	FIVE,
	SIX,
	SEVEN,
	EIGHT,
	NINE,
	TEN
};


pub fn main() -> i32 {
	OUNIT: [exit_status = 1]
	ret 5 in (ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN);
}
