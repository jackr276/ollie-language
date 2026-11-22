/**
* Author: Jack Robbins
*/


pub fn lexical_scope_test(x:i32, y:i32, z:mut i32) -> i32 {
	if( x == 0) {
		let x:mut i32 = 35;
		let y:mut i32 = 55;

		 z = x + y;
	} else {
		let x:mut i8 = 3;
		let y:mut i8 = 2;
		ret x / y;
	}

	ret x * y + z;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 1]
	ret @lexical_scope_test(3, 5, 3);
}
