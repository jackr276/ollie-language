/**
* Author: Jack Robbins
* Test a case where we have a c swtich with one non-default member where the default member is on top
* in the switch(this has implications for fall-through in C-style switches)
*/


pub fn main() -> i32 {
	let x:mut i32 = 5;

	switch(x){
		default:
			x--;
			break;

		case 5:
			x++;
			break;
	}

	OUNIT: [exit_status = 6]
	ret x;
}
