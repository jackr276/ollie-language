/**
* Author: Jack Robbins
* Testing relational operators
*/

pub fn relational_test(x:i32, y:i32) -> i32 {
	let z:i32 = x >= y;

	if(x <= y) {
		ret 2;
	}

	ret z;
}


pub fn main() -> i32{
	let x:i32 = 3;
	let y:i32 = 3;

	OUNIT: [exit_status = 2]
	ret @relational_test(x, y);
}
