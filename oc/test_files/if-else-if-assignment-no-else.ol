/**
* Author: Jack Robbins
* Test a case where we have a chaining assignment with no else condition
*/

pub fn chained_assignment(x:i32) -> i32 {
	let result:mut i32 = 5;

	if(x > 5){
		result = 2;
	} else if(x > 3){
		result = 3;
	} else if(x > 1){
		result = 4;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @chained_assignment(0);
}
