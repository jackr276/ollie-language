/**
* Author: Jack Robbins
* Test an invalid case where we have an in member value more than once
*/

pub fn is_in_list(x:i32) -> i32 {
	//Should fail because we have 5 twice
	ret x in (1, 2, 3, 4, 5, 5, 6, 8);
}


pub fn main() -> i32 {
	ret 0;
}
