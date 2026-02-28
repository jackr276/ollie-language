/**
* Author: Jack Robbins
* Test the compiler's ability to handle caller saving on top of stack parameter passing
*/


fn parameter_pass(x:i32, y:i32, z:i32, a:char, b:char, c:char, d:i32) -> i32 {
	let k:mut i32 = x + y + z;
	let cc:mut char = a + b + c;

	let quotient:i32 = k / c;
	
	ret k + cc - d + quotient;
}


pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = x - 1;
	let aa:mut i32 = x - y;
	let bb:mut i32 = x - y;
	let cc:mut i32 = x - y;
	let dd:mut i32 = x - y;
	let ee:mut i32 = x - y;
	let ff:mut i32 = x - y;

	//Kind of dumb but just to show a point
	let func:fn(i32, i32, i32, char, char, char, i32) -> i32 = parameter_pass;
	let result:i32 = @func(aa, bb, cc, 'a', 'b', 'c', x);

	let k:mut i32 = y / ee + aa;
	let c:mut i32 = y % k + cc + ee + ff;


	ret result + k - c;
}
