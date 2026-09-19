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
	let x:f32 = 5;
	let y:f32 = 6;
	let a:i32 = 5;
	let b:i32 = 6;

	//Make sure we can resolve the function pointer
	let fn_ptr:fn(f32, f32) -> f32 = add;
	let fn_ptr2:fn(i32, i32) -> i32 = add;

	OUNIT: [exit_status = 22]
	ret @fn_ptr(x, y) + @fn_ptr2(a, b);
}
