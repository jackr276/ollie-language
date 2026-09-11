/**
 * Author: Jack Robbins
 * Test the most basic case for function overloading
 */



fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


fn add(x:f32, y:f32) -> f32 {
	ret x + y;
}


fn add(x:i8, y:i8) -> i8 {
	ret x + y;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;

	OUNIT: [exit_status = 11]
	ret @add(x, y);
}
