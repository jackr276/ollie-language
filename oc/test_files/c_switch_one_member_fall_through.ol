/**
* Author: Jack Robbins
* Test a case where we have a c swtich with one non-default member
* that also does a fall-through
*/

pub fn c_switch_member_fallthrough(x:mut i32) -> i32 {
	switch(x){
		case 5:
			x++;

		default:
			x--;
			break;
	}

	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @c_switch_member_fallthrough(5);
}
