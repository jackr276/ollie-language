/**
* Author: Jack Robbins
* Test the PXOR interference in the register allocator
*/


//Completely meaningless function
pub fn do_something_float(x:f32) -> f32 {
	ret x + 3;
}


pub fn converted_floats(x:i32, y:i32) -> f32 {
	//Triggers the PXOR
	let x_float:mut f32 = x;

	//Use here
	let sum1:f32 = @do_something_float(x_float);

	//Do another conversion
	x_float = y;

	//Use here
	let sum2:f32 = @do_something_float(x_float);

	//So they don't get optimized
	ret sum1 + sum2;
}



pub fn main() -> i32 {
	 ret @converted_floats(3.33, 4.55);
}
