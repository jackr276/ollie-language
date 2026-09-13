/**
 * Author: Jack Robbins
 * Test the case where we're doing modulus(which is really division, signed and unsigned) with a memory
 * move as the second operand
 */


pub fn unsigned_mod_with_memory(x:u32[5], y:u32) -> u32 {
	ret y % x[2];
}


pub fn signed_mod_with_memory(x:i32[5], y:i32) -> i32 {
	ret y % x[3];
}



pub fn main() -> i32 {
	let u_arr:u32[5] = [1, 2, 3, 4, 5];
	let s_arr:i32[5] = [1, 2, 3, 4, 5];

	//Should return 0 + 3 = 3
	OUNIT: [exit_status = 3]
	ret @unsigned_mod_with_memory(u_arr, 33)
		+ @signed_mod_with_memory(s_arr, 11);
}
