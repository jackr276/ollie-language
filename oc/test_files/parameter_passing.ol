/**
* Author: Jack Robbins
* Testing parameter passing
*/

fn parameter_pass(x:i32, y:i32, z:i32, a:char, b:char, c:char) -> i32 {
	let mut k:i32 := x + y + z;
	let mut c:char := a + b + c;

	ret k + c;
}

fn parameter_pass2(x:i32, y:i32, z:i32, a:char, b:char, c:char) -> i32 {
	let mut k:i32 := x + y + z;
	let mut c:char := a + b + c;

	ret k + c;
}


fn main() -> i32{
	let mut x:i32 := 3;
	let mut y:i32 := x - 1;

	let mut z:i32 := y + x;
	let a:char := 'a';
	let b:char := 'b';
	let c:char := 'c';

	//Testing complexities
	@parameter_pass2(x + 5, y * 3, z - 22, 'a', b + 'd', c);

 	@parameter_pass(x, y, z, 'a', b, c);

	ret x + 1;
}
