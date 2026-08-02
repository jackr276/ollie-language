/**
 * Author: Jack Robbins
 * Test using typesize with a float that will require coercion
 */


pub fn main() -> i32 {
	let x:i32[5] = [1, 2, 3, 4, 5];

	OUNIT: [exit_status = 24]
	ret sizeof(x) + 4.444;
}
