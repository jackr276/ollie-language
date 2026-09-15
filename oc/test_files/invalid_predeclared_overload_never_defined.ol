/**
 * Author: Jack Robbins
 * Test an invalid predeclared overload that was never defined
 */


declare pub fn add(i32, i32) -> i32;
//INVALID - was never defined
declare pub fn add(f32, f32) -> f32;


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @add(5, 4);
}
