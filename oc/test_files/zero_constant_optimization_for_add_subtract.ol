/**
 * Author: Jack Robbins
 * Test the parser level optimization where we can detect adding/subtracting by or from
 * 0 to validate that it works. When we do this optimization, we need to make sure
 * that we're preserving prior behavior like function calls
 */

//Atomically increasing global variable so we can tell our function call frequency
let pub global:mut i32 = 0;


//Serves as a way to see the number of calls
fn inc_and_get_global() -> i32{
	//TODO THIS IS A BUG - ITS NOT GENERATING PROPER OIR
	global++;

	ret global;
}


pub fn add_0_to_func_call() -> i32 {
	ret @inc_and_get_global() + 0;
}


pub fn add_func_call_to_0() -> i32 {
	ret 0 + @inc_and_get_global();
}


pub fn sub_func_call_from_0() -> i32 {
	ret 0 - @inc_and_get_global();
}


pub fn sub_0_from_func_call() -> i32 {
	ret @inc_and_get_global() - 0;
}


pub fn main() -> i32 {
	let result:mut i32 = 0;

	//Should give us -1 + 2 + 3 + 4 = 8
	result += @sub_func_call_from_0();
	result += @sub_0_from_func_call();
	result += @add_0_to_func_call();
	result += @add_func_call_to_0();

	OUNIT: [exit_status = 8]
	ret result;
}
