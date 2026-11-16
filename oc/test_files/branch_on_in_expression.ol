/**
 * Author: Jack Robbins
 * Test the case where we branch on an in expression
 */

pub fn branch_on_in(x:i32) -> i32 {
	if(x in (1, 2, 3, 4, 5)){
		ret 5;
	} else {
		ret 6;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 6]
	ret @branch_on_in(-1);
}
