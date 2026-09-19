/**
 * Author: Jack Robbins
 * Test a case where we are overloading but the only difference is the mutability on basic types
 */


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


//BAD! - not different enough
pub fn add(x:mut i32, y:mut i32) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @add(5, 6);
}
