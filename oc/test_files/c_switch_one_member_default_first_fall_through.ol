/**
* Author: Jack Robbins
* Test a case where we have a c swtich with one non-default member
* that also does a fall-through, where the default clause comes first
*/


pub fn c_switch_fallthrough(x:mut i32) -> i32 {
	//This should hit the default
	switch(x){
		default:
			x--;

		case 4:
			x++;
			break;
	}
	
	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @c_switch_fallthrough(5);
}
