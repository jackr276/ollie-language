/**
* Author: Jack Robbins
* Test an instance where we have a floating point addition/subtraction operation
* with a source memory access that can be converted into a combined load/add/sub
*/


pub fn add_with_memory_op(x:f32[5], y:f32) -> i32 {
	ret y + x[3];
}


pub fn sub_with_memory_op(x:f32[5], y:f32) -> i32 {
	ret y - x[2];
}


pub fn main() -> i32 {
	let x:f32 = 5;
	let arr:f32[5] = [1.1, 2.2, 3.3, 4.4, 5.5];

	//Should return (5 + 4.4 = 9(rounded)) + (5 - 3.3 = 1(rounded)) = 10
	OUNIT: [exit_status = 10]
	ret @add_with_memory_op(arr, x) + @sub_with_memory_op(arr, x);
}
