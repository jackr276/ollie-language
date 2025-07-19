/**
* Author: Jack Robbins
* Testing type coercion rules
*/

fn main(argc:u32, argv:char**) -> i32 {
	let mut x:i32 := 2;
	let mut a:u64 := x;
	let mut y:i8 := 'a';

	if( x > y ) {
		ret y;
	}

	ret (y * x) * x;
}
