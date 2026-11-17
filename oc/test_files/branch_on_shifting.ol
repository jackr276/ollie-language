/**
 * Author: Jack Robbins
 * Test our internal logic for branching on shifting
 */


pub fn branch_on_rshift_signed(x:i32, y:i32) -> i32 {
	if(x >> y) {
		ret 1;
	} else {
		ret 2;
	}
}


pub fn branch_on_rshift_unsigned(x:u32, y:u32) -> i32 {
	if(x >> y) {
		ret 3;
	} else {
		ret 4;
	}
}


pub fn branch_on_lshift_signed(x:i32, y:i32) -> i32 {
	if(x << y) {
		ret 5;
	} else {
		ret 6;
	}
}


pub fn branch_on_lshift_unsigned(x:u32, y:u32) -> i32 {
	if(x << y) {
		ret 7;
	} else {
		ret 8;
	}
}


pub fn main() -> i32 {
	//Should return 1 + 4 + 5 + 7 = 17
	OUNIT: [exit_status = 17]
	ret @branch_on_rshift_signed(5, 0)
		+ @branch_on_rshift_unsigned(0, 0)
		+ @branch_on_lshift_signed(5, 11)
		+ @branch_on_lshift_unsigned(15, 0);
}
