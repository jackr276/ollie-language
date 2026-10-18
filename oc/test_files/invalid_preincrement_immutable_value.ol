/**
 * Author: Jack Robbins
 * Validate that we can catch mutability violations with preincrements
 */


pub fn main() -> i32 {
	let x:i32[] = [1, 2, 3, 4, 5];

	++x[2];

	OUNIT: [fail_to_compile]
	ret x[2];
}
