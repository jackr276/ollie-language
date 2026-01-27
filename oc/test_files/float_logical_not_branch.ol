/**
* Author: Jack Robbins
* Test handling float logical notting
*/

pub fn float_logical_not_branch(x:f32) -> i32 {
	if(!x) {
		ret 33;
	} else {
		ret 0;
	}
}


pub fn main() -> i32 {
	ret @float_logical_not_branch(3.33);
}
