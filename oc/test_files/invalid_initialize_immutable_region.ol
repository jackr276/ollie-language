/**
 * Author: Jack Robbins
 * Test an invalid attempt to initialize something after we already have
 */


pub fn main() -> i32 {
	//This is fine
	let arr:i32[] = [1, 2, 3, 4];

	//This is not
	arr = [1, 2, 3, 4];

	OUNIT: [fail_to_compile]
	ret arr[0];
}
