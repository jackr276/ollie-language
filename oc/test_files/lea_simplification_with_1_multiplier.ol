/**
* Author: Jack Robbins
* Test the simplifier's ability to handle simplifications when the lea multiplier
* is 1
*/


pub fn lea_with_1_multiplier(str:char*, x:i32) -> i32 {
	if(str[x] == '\0'){
		ret x;
	} else {
		ret 0;
	}
}


pub fn main() -> i32 {
	ret @lea_with_1_multiplier("Hello", 3);
}
