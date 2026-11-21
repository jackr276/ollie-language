/**
 * Author: Jack Robbins
 * Test our ability to handle a conditional break in a for loop
 */

pub fn break_in_for(x:mut i32, cond:i32) -> i32 {
	for(let i:mut i32 = 0; i <= 323; i++){
		x++;
		break when(x == cond);
	}

	ret x;
}


pub fn main() -> i32{
	OUNIT: [exit_status = 57]
	ret @break_in_for(5, 57);
}
