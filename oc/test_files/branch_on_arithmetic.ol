/**
 * Author: Jack Robbins
 * Test our ability to branch on arithmetic
 */


pub fn branch_on_arithmetic(x:i32, y:i32) -> i32 {
	ret (x % y) ? x else y;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 3]
	ret @branch_on_arithmetic(9, 3);
	
}
