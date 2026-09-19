/**
 * Author: Jack Robbins
 * Test an invalid case where we have overloaded functions that are predeclared, that only
 * differ by an error type
 */

declare fn sub(i32, i32) -> i32;

//BAD! - not different enough to overload
declare fn sub(i32, i32) -> f32;


fn sub(x:i32, y:i32) -> i32 {
	ret x - y;
}


fn sub(x:i32, y:i32) -> f32 {
	ret x - y;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
