/**
 * Author: Jack Robbins
 * Test the case where we're doing division(signed, unsigned and float) with a memory
 * move as the second operand
 */


pub fn unsigned_div_with_memory(x:u32[5], y:u32) -> u32 {
	ret y / x[2];
}


pub fn signed_div_with_memory(x:i32[5], y:i32) -> i32 {
	ret y / x[3];
}


pub fn float_div_with_memory(x:f32[5], y:f32) -> f32 {
	ret y / x[1];
}


pub fn main() -> i32 {
	let u_arr:u32[5] = [1, 2, 3, 4, 5];
	let s_arr:i32[5] = [1, 2, 3, 4, 5];
	let f_arr:f32[5] = [1.1, 2.2, 3.3, 4.4, 5.5];

	//Should return 5 + 5 + 9.09 = 19
	OUNIT: [exit_status = 19]
	ret @unsigned_div_with_memory(u_arr, 15)
		+ @signed_div_with_memory(s_arr, 20)
		+ @float_div_with_memory(f_arr, 20);
}
