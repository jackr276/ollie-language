/**
* Author: Jack Robbins
* This file aims to test the caller saving conventions of the ollie compiler
*/



fn parameter_pass2(x:i32, y:i32, z:i32, a:char, b:char, c:char) -> i32 {
	let mut k:i32 := x + y + z;
	let mut c:char := a + b + c;

	ret k + c;
}

fn parameter_pass() -> i32 {
	let mut x:i32 := 3;
	let mut y:i32 := 3;
	let mut z:i32 := 3;

	let mut k:i32 := x + y + z + @parameter_pass2(x, y, z, 'a', 'b', 'c');
	@parameter_pass();

	ret x + y + z + k;
}

fn parameter_pass3(a:i32) -> i32 {
	ret a;
}

fn pcount_r(mut x:u64) -> u64 {
	if( x == 0) then {
		ret (x & 1) + @pcount_r(x >> 1);
	} else if (x == 1) then{
		if(x > 3) then {
			ret 1;
		}

		x := x + 1;
	} else {
		ret 0;
	}

	@parameter_pass();

	ret x * 3;
}

fn main(argv:char**, argc:i32) -> i32 {
	@pcount_r(78l);

	ret 2;
}

