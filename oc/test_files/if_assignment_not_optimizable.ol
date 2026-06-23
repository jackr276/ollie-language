/**
* Author: Jack Robbins
* Test a case where we have an if assignment that *cannot* be made into a conditional move
*/


pub fn dummy_fn() -> i32 {
	ret 1;
}


pub fn if_assignment(x:i32) -> i32 {
	let result:mut i32 = 5;

	if(x > 5) {
		result = 2;

	} else {
		//Should stop the entire thing from being optimized
		@dummy_fn();
		result = 3;
	}

	ret result;
}



pub fn main() -> i32 {
	OUNIT:[console = 3]
	ret @if_assignment(3);
}
