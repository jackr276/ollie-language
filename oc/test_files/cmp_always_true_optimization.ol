/**
* Author: Jack Robbins
* Test the always true optimizations when a CMP instruction is involved
*/


pub fn cmp_always_true(x:i32, y:i32) -> i32 {
	//Always true
	if(5 > 1) {
		ret y;
	} else {
		ret x;
	}
}

pub fn main() -> i32 {
	ret 0;
}
