/**
* Special file for testing the instruction selector
*/

fn unsigned_shift() -> u32 {
	let mut z:u32 = 23;
	let mut x:u32 = z << 3;
	let mut y:u32 = z >> 5;
	//Bitwise or
	y = y | x;
	//Bitwise xor
	y = y ^ x;
	//Bitwise and
	y = y & x;

	ret x + y;
}

fn modulus() -> u32 {
	let mut z:u32 = 23;
	let mut x:u32 = z << 3;
	let mut y:i32 = z >> 5;

	//Modulus
	y = y % x;

	ret x + y;
}


pub fn main(arg:i32, argv:char**) -> i32 {
	let mut a:i32 = 3;
	let mut x:i32 = !a;
	let mut y:i32 = -3;

	x = x + y * 8;
	a = x - 0;
	x = !a;
	//Modulus
	x = x % -3;

	a = (x * -128) + (x - 11);
	x = x / 8;
	x = x && 21;
	x = x || 32;


	ret x + y + a;
}
