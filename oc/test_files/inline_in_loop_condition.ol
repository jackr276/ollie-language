/**
 * Author: Jack Robbins
 * Test a more complex inlining case where we are inlining inside of a
 * loop conditional
 */

let x:i32 = 5;


inline fn get_global_value() -> i32 {
	ret x;
}


inline fn loop_with_inline() -> i32 {
	let result:mut i32 = 0;

	//Test the inline inside of the conditional itself
	for(let i:mut i32 = 0; i < @get_global_value(); i++){
		result++;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @loop_with_inline();
}
