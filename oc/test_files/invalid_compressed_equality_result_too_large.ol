/**
 * Author: Jack Robbins
 * Test an invalid case where the compressed equality gets coerced to a larger type
 * and won't fit back into the LHS
 */


pub fn main() -> i32 {
	let x:i64 = 5;
	let y:mut i32 = 6;
	let z:mut i32* = &y;

	//Should have the RHS converted to i64, which then won't fit into the i32
	*z += x;

	OUNIT: [fail_to_compile]
	ret *z;
}
