/**
* Author: Jack Robbins
* Testing type coercion rules
*/

pub fn main(argc:i32, argv:char**) -> i32 {
	let x:mut i32 = 2;
	let a:mut u64 = x;
	let y:mut i8 = 'a';

	if( x > y ) {
		ret y;
	}

	ret (y * x) * x;
}
