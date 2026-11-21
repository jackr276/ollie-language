/**
* Author: Jack Robbins
* Testing support for logical not operation in different scenarios
*/


fn ret_logical_not(x:i32) -> bool {
	ret !x;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	let x:i32 = 3;

	if(!argc) {
		ret @ret_logical_not(x);
	} else {
		ret 0;
	}
}
