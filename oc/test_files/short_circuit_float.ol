/**
* Author: Jack Robbins
* Test floating point short-circuiting logic
*/


pub fn float_short_circuit(x:mut f32, y:mut f32) -> i32 {
	let result:mut i32 = 0;

	//Should trigger a short circuit
	while(x && y) {
		result++;
		x--;
		y--;
	}

	ret result;
}


pub fn main() -> i32 {
	ret @float_short_circuit(3.222, 78.2);
}
