/**
* Author: Jack Robbins
* This file aims to test the caller saving conventions of the ollie compiler
*/

fn pcount_r(x:mut u64) -> u64 {
	let y:mut u64 = 32;
	if( x == 0) {
		ret (x & 1) + @pcount_r(x >> 1) + y;
	} else if (x == 1) {
		if(x > 3) {
			ret 1 + y;
		}

		x = x + 1;
	} else {
		ret 0;
	}

	ret x * 3 + y;
}

pub fn main(argc:i32, argv:char**) -> i32 {
	@pcount_r(78l);

	ret 2;
}

