/**
* Author: Jack Robbins
* Test a case where a user is attempting to inline a directly recursive function
*/


inline fn direct_recursive(x:mut i32, acc:mut i32) -> i32 {
	if(x == 0){
		ret acc;
	}

	//Direct recursion, invalid for inlining
	ret @direct_recursive(--x, ++acc);
}


pub fn main() -> i32 {
	ret @direct_recursive(55, 0);
}
