/**
* Author: Jack Robbins
* Test a valid use case of an elaborative stack param inside of a function pointer
*/

define fn(i32, params i32) -> i32 as my_func;


pub fn tester(x:i32, y:params i32) -> i32 {
	ret x + y[3];
}

pub fn invoke_tester(x:i32) -> i32 {
	let func_to_call:my_func = tester;

	ret @func_to_call(x, 1, 2, 3);
}


pub fn main() -> i32 {
	ret 0;
}
