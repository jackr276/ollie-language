/**
* Author: Jack Robbins
* Test a branch with one in member
*/


pub fn branch_one_member(x:i32) -> i32 {
	declare result:mut i32;

	if(x in (5)) {
		result = 11;
	} else {
		result = 12;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT:[exit_status = 12]
	ret @branch_one_member(-1);
}
