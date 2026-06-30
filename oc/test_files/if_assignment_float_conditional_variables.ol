/**
 * Author: Jack Robbins
 * Test float conditionals in if assignments where we have variable assignments to test 
 * our float-specific logic
 */

pub fn float_if_assignment(x:f32, y:i32, z:i32) -> i32 {
	declare result:mut i32;

	//Due to the inverse jumping - this should trigger the cmove if equal scenario
	if(x != 5) {
		result = y;
	} else {
		result = z;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [console = 8]
	ret @float_if_assignment(4.44, 8, 9);
}
