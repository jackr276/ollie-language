/**
* Author: Jack Robbins
* Testing parameter passing
*/

fn parameter_pass(x:i32, y:i32, z:i32, a:char, b:char, c:char) -> i32 {
	let mut k:i32 = x + y + z;
	let mut c:char = a + b + c;

	ret k + c;
}

fn parameter_pass2(x:i32, y:i32, z:i32, a:char, b:char, c:char) -> i32 {
	let mut k:i32 = x + y + z;
	let mut c:char = a + b + c;

	ret k + c;
}

fn parameter_pass3(a:i32) -> i32 {
	ret a;
}

fn pcount_r(mut x:u64) -> u64 {
	if( x == 0) {
		ret (x & 1) + @pcount_r(x >> 1);
	} else if (x == 1) {
		if(x > 3) {
			ret 1;
		}

		x = x + 1;
	} else {
		ret 0;
	}

	ret x * 3;
}


pub fn main() -> i32{
	let mut x:i32 = 3;
	let mut y:i32 = x - 1;
	let mut aa:i32 = x - y;
	let mut bb:i32 = x - y;
	let mut cc:i32 = x - y;
	let mut dd:i32 = x - y;
	let mut ee:i32 = x - y;
	let mut ff:i32 = x - y;

	let mut z:i32 = y + x;
	let a:char = 'a';
	let b:char = 'b';
	let ch:char = 'c';

	let mut k:i32 = y / z + aa;
	let mut c:i32 = y % k + cc + ee + ff;

	//Testing complexities
	if(k == 0) {
		ret @parameter_pass2(k+ 5, c * 3, z - 22, 'a', b + 'd', ch) + @parameter_pass(x, y, aa + bb, 'a', b, ch);
	} else {
		ret x + y - aa * bb;
	}
}
