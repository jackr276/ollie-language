/**
 * Author: Jack Robbins
 * Test a very simple case where we're branching based on constant arithmetic. The simplifier
 * should detect and optimize this away
 */


 pub fn main() -> i32 {
 	OUNIT: [exit_status = 55]
 	if(5 + 4) {
		ret 55;
	} else {
		ret 0;
	}

 }
