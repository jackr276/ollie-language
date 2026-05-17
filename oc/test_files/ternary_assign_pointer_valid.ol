/**
* Author: Jack Robbins
* Test a valid case where we perform pointer assignment on a ternary
*/


pub fn ternary_assign(decider:i32, x:i32*, y:mut i32*) -> i32* {
	/**
	 * Should work since we have an immut pointer return type
	 */
	ret (decider > 3) ? x else y;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;
	let y:mut i32 = 3;

	let x_ptr:mut i32* = &x;
	let y_ptr:mut i32* = &y;

	//OUNIT: [console = 5]
	ret *(@ternary_assign(4, x_ptr, y_ptr));
}
