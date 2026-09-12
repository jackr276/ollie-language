/**
 * Author: Jack Robbins
 * Test combining bitwise binary operations with loads
 */


pub fn and_with_load(x:i32*, y:i32) -> i32 {
	ret y & *x;
}


pub fn or_with_load(x:i32*, y:i32) -> i32 {
	ret y | *x;
}


pub fn xor_with_load(x:i32*, y:i32) -> i32 {
	ret y ^ *x;
}


pub fn main() -> i32 {
	let x:i32 = 0x0F;
	let y:i32 = 5;

	//Should return 5 + 0xF + 10 = 30
	OUNIT: [exit_status = 30]
	ret @and_with_load(&y, x)
		+ @or_with_load(&y, x)
		+ @xor_with_load(&y, x);
}
