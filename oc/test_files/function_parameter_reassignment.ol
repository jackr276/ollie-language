/**
 * Author: Jack Robbins
 * Test our ability to redefine function parameters internally
 */


pub fn internal_reassign(x:i32, y:mut i32) -> i32 {
	if(x > 5){
		y = 15;
	} else {
		y += 2;
	}

	ret y;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 9]
	ret @internal_reassign(5, 7);
}
