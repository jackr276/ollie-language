/**
 * Author: Jack Robbins
 * Test an invalid case where we try to assign an initializer to a type that's incompatible
 */


pub fn dummy(x:i32) -> i32 {
	ret x * 2;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @dummy([1, 2, 3, 4, 5]);
}
