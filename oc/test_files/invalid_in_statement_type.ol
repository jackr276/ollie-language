/**
* Author: Jack Robbins
* Test an invalid attempt to use an in statement on an invalid type
*/

pub fn invalid_in(x:i32*) -> i32 {
	//Should fail - can't use a pointer type for this
	ret x in (1, 2, 3, 4);
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
