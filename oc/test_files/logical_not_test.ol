/**
* Author: Jack Robbins
* Testing support for logical not operation in different scenarios
*/


fn ret_logical_not(x:i32) -> bool {
	ret !x;
}


pub fn main() -> i32 {
	let mut x:i32 = 3;

	if(!x) {
		ret @ret_logical_not(x);
	} else {
		ret 0;
	}
}
