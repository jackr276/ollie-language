/**
 * Author: Jack Robbins
 * Test the basic use of the size type
 */

pub fn ret_size(x:i32) -> size {
	ret sizeof(x);
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 4]
	ret @ret_size(3);
}
