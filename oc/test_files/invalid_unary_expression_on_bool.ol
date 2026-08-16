/**
 * Author: Jack Robbins
 * Test the use of an invalid unary expression on a boolean type
 */


pub fn bitwise_not_bool(x:bool) -> bool {
	ret ~x;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @bitwise_not_bool(true);
}
