/**
* Author: Jack Robbins
* Test a case where we have a c swtich with one non-default member
* that also does a fall-through, where the default clause comes first
*/


pub fn main() -> i32 {
	let x:mut i32 = 5;

	switch(x){
		default:
			x--;

		case 5:
			x++;
			break;
	}

	OUNIT: [console = 5]
	ret x;
}
