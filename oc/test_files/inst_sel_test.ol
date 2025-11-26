/**
* Special file for testing the instruction selector
*/

fn unsigned_shift() -> u32 {
	let z:mut u32 = 23;
	let x:mut u32 = z << 3;
	let y:mut u32 = z >> x;

	ret x + y;
}


pub fn main(arg:i32, argv:char**) -> i32 {
	let z:mut i32 = 33;
	let c:mut i32 = z - 1;
	let a:mut i32 = 222;
	let x:mut i32 = !a;
	let y:mut i32 = -3;

	x = x + y * 8;
	a = x - 0;
	x = !a;

	a = (x * -128) + (x - 11);
	x = x / 9;
	x = x && 21;
	x = x || 32;
	x = a - z + x;
	x = x && 21;
	x = x || 32;
	x = x && 21;
	x = x || 32;
	x = c & x;

	ret x + y + a;
}
