/**
* Author: Jack Robbins
* Test the case where we predeclare with fn and then declare with fn!
*/

declare pub fn my_func(i32, i32) -> i32;


pub fn! my_func(x:i32, y:i32) -> i32 {
	ret 0;
}

pub fn main() -> 32 {
	ret 0;
}
