/**
* Author: Jack Robbins
* Test a case where we have a c swtich with one non-default member
*/


pub fn main() -> i32 {
	let x:mut i32 = 5;

	switch(x){
		case 5:
			x++;
			break;

		default:
			x--;
			break;
	}

	OUNIT: [exit_status = 6]
	ret x;
}
