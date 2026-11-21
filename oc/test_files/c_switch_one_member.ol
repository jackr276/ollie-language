/**
* Author: Jack Robbins
* Test a case where we have a c swtich with one non-default member
*/

pub fn c_switch_one_member(x:mut i32) -> i32 {
	switch(x){
		case 5:
			x++;
			break;

		default:
			x--;
			break;
	}

	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 6]
	ret @c_switch_one_member(5);
}
