/**
 * Author: Jack Robbins
 * Test an invalid attempt to assign an F64 to pointer. In earlier types of the compiler this was mistakenly allowed
 */

 pub fn main() -> i32 {
 	let x:f64 = 5.555d;

	//BAD - should never work
	let y:i64* = x;

	OUNIT: [fail_to_compile]
	ret 0;
 }
