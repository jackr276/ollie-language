/**
* Author: Jack Robbins
* Test a case where we have an if assignment that can become a conditional move, but
* where both operands need to be converted
*/

pub fn if_assignment_converting(x:i32) -> i32 {
	declare result:mut i32;
	let true_const:bool = true;
	let false_const:bool = false;

	//These should both require converting moves
	if(x == 5) {
		result = true_const;
	} else {
		result = false_const;
	}
	
	ret result;
}


pub fn main() -> i32 {
	OUNIT:[console = 1]
	ret @if_assignment_converting(5);
}
