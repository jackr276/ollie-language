/**
* Author: Jack Robbins
* 
* Test the ability to detect if a variable may be used uninitialized
*/

pub fn used_uninit(input:i32) -> i32 {
	declare x:mut i32;

	if(input > 3) {
		x = 5;
	}

	//Could be used uninitialized - should fail
	ret x;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
