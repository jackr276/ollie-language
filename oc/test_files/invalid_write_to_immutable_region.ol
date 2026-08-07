/**
 * Author: Jack Robbins
 * Test an invalid attempt to write to an immutable memory region. This
 * is never allowed
 */

pub fn write_to_immutable(x:i32[5]) -> void {
	x[1] = 7;
}


pub fn main() -> i32 {
	let arr:i32[5] = [1, 2, 3, 4, 5];

	OUNIT: [fail_to_compile]
	ret @write_to_immutable(arr);
}
