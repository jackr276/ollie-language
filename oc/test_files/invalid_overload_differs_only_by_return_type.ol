/**
 * Author: Jack Robbins
 * Test an invalid case where we have a function that is overloaded but only differs by return
 * type. This will fail to compile
 */



pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


//BAD!
pub fn add(x:i32, y:i32) -> f32 {
	ret x + y;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
