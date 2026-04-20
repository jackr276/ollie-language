/**
* Author: Jack Robbins
* Test the value numberer's ability to handle phi simplifications
*/


pub fn value_number_phi_simplify(x:i32, y:i32, cond:bool) -> i32 {
	//First result
	let result:i32 = x + y;

	//This is what we will optimize away
	declare useless_result:mut i32;

	if(cond) {
		useless_result = x + y;
	} else {
		useless_result = x + y;
	}

	ret result + useless_result;
}
