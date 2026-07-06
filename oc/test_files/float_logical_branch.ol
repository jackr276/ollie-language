/**
* Author: Jack Robbins
* Test the ability to do a logical branch based on a floating point value
*/

//Dummy - purely exists to show how this would work
pub fn float_branch(x:f32) -> bool {
	if(x) {
		ret true;
	} else {
		ret false;
	}
	
}


//Dummy
pub fn main() -> i32 {
	OUNIT:[exit_status = 0]
	ret @float_branch(0.0);
}
