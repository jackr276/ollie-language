/**
* Author: Jack Robbins
* This file aims to test the caller saving conventions of the ollie compiler when we use
* indirect function calls
*/

/**
* Define a signature of a count function
*/
define fn(mut u32) -> u32 as count_function;

fn pcount_r(mut x:u32) -> u32 {
	let mut y:u32 := 32;
	if( x == 0) {
		ret (x & 1) + @pcount_r(x >> 1) + y;
	} else if (x == 1) {
		if(x > 3) {
			ret 1 + y;
		}

		x := x + 1;
	} else {
		ret 0;
	}

	ret x * 3 + y;
}

fn lcount_r(mut x:u32) -> u32 {
	let mut y:u32 := 32;
	if( x == 0) {
		ret (x & 3) + @pcount_r(x >> 5) + y;
	} else if (x == 5) {
		if(x > 3) {
			ret 1 + y;
		}
		x := x + 1;
	} else {
		ret 3;
	}

	ret x * 3 + y;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	declare mut a:u32;
	let mut x:u32 := 433;

	a := (x * -128) + (x - 11);
	x := x / 9;
	x := x && 21;
	x := x || 32;
	x := a - 3 + x;
	x := x && 21;

	//Both defined indirectly
	let z:count_function := pcount_r;
	let y:count_function := lcount_r;

	ret @z(32) + @z(32) + @y(17) + x;
}

