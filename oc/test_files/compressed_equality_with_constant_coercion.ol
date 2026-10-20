/**
 * Author: Jack Robbins
 * Prove out that compressed equality works with constant coercion
 */


pub fn main() -> i32 {
	let y:mut i8 = 2;

	//Should work fine
	y += 3;

	OUNIT: [exit_status = 5]
	ret y;
}
