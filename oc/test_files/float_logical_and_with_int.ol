/**
* Author: Jack Robbins
* Test float logical and when there's also an int. It should be forced to float regardless
*/


pub fn float_logical_and(x:i32, y:f32) -> i32 {
	ret x && y;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
