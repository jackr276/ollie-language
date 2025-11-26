/**
* Author: Jack Robbins
* Testing parameter passing
*/

fn parameter_pass(x:i32, y:i32, z:i32, a:char, b:char, c:char) -> i32 {
	let k:mut i32 = x + y + z;
	let cc:mut char = a + b + c;

	ret k + cc;
}

fn parameter_pass2(x:i32, y:i32, z:i32, a:char, b:char, c:char) -> i32 {
	let k:mut i32 = x + y + z;
	let c:mut char = a + b + c;

	ret k + c;
}

fn parameter_pass3(a:i32) -> i32 {
	ret a;
}

fn pcount_r(x:mut u64) -> u64 {
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
	let x:mut i32 = 3;
	let y:mut i32 = x - 1;
	let aa:mut i32 = x - y;
	let bb:mut i32 = x - y;
	let cc:mut i32 = x - y;
	let dd:mut i32 = x - y;
	let ee:mut i32 = x - y;
	let ff:mut i32 = x - y;

	let z:mut i32 = y + x;
	let a:char = 'a';
	let b:char = 'b';
	let ch:char = 'c';

	let k:mut i32 = y / z + aa;
	let c:mut i32 = y % k + cc + ee + ff;

	//Testing complexities
	if(k == 0) {
		ret @parameter_pass2(k+ 5, c * 3, z - 22, 'a', b + 'd', ch) + @parameter_pass(x, y, aa + bb, 'a', b, ch);
	} else {
		ret x + y - aa * bb;
	}
}
