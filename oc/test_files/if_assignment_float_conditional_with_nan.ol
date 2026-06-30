/**
 * Author: Jack Robbins
 * Test float conditionals in if assignments where we have variable assignments to test 
 * our float-specific logic
 */

pub fn float_if_assignment(x:f32, y:i32, z:i32) -> i32 {
	declare result:mut i32;

	//Due to the inverse jumping - this should trigger the cmove if equal scenario
	if(x == 5) {
		result = y;
	} else {
		result = z;
	}

	ret result;
}


pub fn main() -> i32 {
	//Make this all 1's to turn it into NaN
	let x:mut i32 = 0;
	x = ~x;

	//Get this value to actually be NaN
	let NaN:f32 = *(<f32*>(&x));

	OUNIT: [console = 9]
	ret @float_if_assignment(NaN, 8, 9);
}
