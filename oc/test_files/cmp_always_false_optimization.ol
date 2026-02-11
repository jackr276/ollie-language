/**
* Author: Jack Robbins
* Test the always false optimizations when a CMP instruction is involved
*/


pub fn cmp_always_false(x:i32, y:i32) -> i32 {
	//Always false 
	if(1 > 5) {
		ret y;
	} else {
		ret x;
	}
}

pub fn main() -> i32 {
	ret 0;
}
