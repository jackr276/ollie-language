/**
 * Author: Jack Robbins
 * Test a case where we predeclare overloaded functions
 */


declare fn sub(i32, i32) -> i32;
declare fn sub(f32, f32) -> f32;
declare fn sub(f64, f64) -> f64;



fn sub(x:i32, y:i32) -> i32 {
	ret x - y;
}


fn sub(x:f32, y:f32) -> f32 {
	ret x - y;
}


fn sub(x:f64, y:f64) -> f64 {
	ret x - y;
}


pub fn main() -> i32 {
	let x:f64 = 6.55;
	let y:f64 = 5.55;

	OUNIT: [exit_status = 1]
	ret <i32>@sub(x, y);
}
