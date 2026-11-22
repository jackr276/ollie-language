/**
* Author: Jack Robbins
* Test a case where we have a c swtich with one non-default member where the default member is on top
* in the switch(this has implications for fall-through in C-style switches)
*/


pub fn c_switch_one_member_default_first(x:mut i32) -> i32 {
	switch(x){
		default:
			x--;
			break;

		case 5:
			x++;
			break;
	}

	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 6]
	ret @c_switch_one_member_default_first(5);
}
