/**
* Author: Jack Robbins
* Testing type coercion rules
*/


pub fn coercion_test(x:i32, a:u64, y:i8) -> i32 {
	if(x > y){
		ret y;
	}

	ret (y * x) * x;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	let x:i32 = 2;
	let a:u64 = x;
	let y:i8 = 2;

	OUNIT: [exit_status = 8]
	ret @coercion_test(x, a, y);
}
