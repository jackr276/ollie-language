/**
 * Author: Jack Robbins
 * Test a specific optimization in Ollie where, if the user initializes a memory
 * region via a let statement using return-by-copy, we will not memory copy
 * the result but will instead use the return-by-copy region as the stack region of
 * the let variable
 */

define struct my_struct {
	x:i32;
	y:i32[4];
	z:char;
} as custom_struct;


pub fn return_struct(x:i32, y:i32) -> custom_struct {
	ret {x, [y, 1, 2, 3], 'a'};
}


pub fn main() -> i32 {
	/**
	 * Let's do this twice to illustrate that they stay separate
	 */
	let ret_val1:custom_struct = @return_struct(1, 2);
	let ret_val2:custom_struct = @return_struct(5, 6);

	OUNIT:[exit_status = 8]
	ret ret_val1:y[0] + ret_val2:y[0];
}
