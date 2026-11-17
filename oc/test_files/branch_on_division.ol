/**
 * Author: Jack Robbins
 * Test our internal logic for branching on division
 */


pub fn branch_on_signed(x:i32, y:i32) -> i32 {
	if(x / y) {
		ret 7;
	} else {
		ret 11;
	}
}


pub fn branch_on_unsigned(x:u32, y:u32) -> i32 {
	if(x / y) {
		ret 19;
	} else {
		ret 5;
	}
}


pub fn main() -> i32 {
	//Should return 11 + 19 = 30
	OUNIT: [exit_status = 30]
	ret @branch_on_signed(0, 5) + @branch_on_unsigned(7, 2);
}
