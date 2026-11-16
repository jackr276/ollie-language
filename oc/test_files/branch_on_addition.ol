/**
 * Author: Jack Robbins
 * Test a case where we're branching on an addition instruction
 */
 
pub fn branch_on_addition(x:i32, y:i32) -> i32 {
	if(x + y){
		ret 5;
	} else {
		ret 7;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 7]
	ret @branch_on_addition(5, -5);
}
