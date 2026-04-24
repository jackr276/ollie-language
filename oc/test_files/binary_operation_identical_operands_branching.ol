/**
* Author: Jack Robbins
* Test cases where we have binary operations with identical operands 
* that involve branching. This is specifically going to be for 
* relation operands
*/

pub fn branch_logical_and(x:i32) -> i32 {
	if(x && x) {
		ret 1;
	} else {
		ret 0;
	}
}


pub fn branch_logical_or(x:i32) -> i32 {
	if(x || x) {
		ret 1;
	} else {
		ret 0;
	}
}


pub fn branch_g_than(x:i32) -> i32 {
	if(x > x) {
		ret 1;
	} else {
		ret 0;
	}
}


pub fn branch_g_than_or_eq(x:i32) -> i32 {
	if(x >= x) {
		ret 1;
	} else {
		ret 0;
	}
}


pub fn branch_l_than(x:i32) -> i32 {
	if(x < x) {
		ret 1;
	} else {
		ret 0;
	}
}


pub fn branch_l_than_or_eq(x:i32) -> i32 {
	if(x <= x) {
		ret 1;
	} else {
		ret 0;
	}
}


pub fn branch_eq(x:i32) -> i32 {
	if(x == x) {
		ret 1;
	} else {
		ret 0;
	}
}


pub fn branch_not_eq(x:i32) -> i32 {
	if(x != x) {
		ret 1;
	} else {
		ret 0;
	}
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
