/**
 * Author: Jack Robbins
 * Test our ability to do logical not with casting and a memory load
 */

pub fn logical_not_with_cast(x:f32) -> i32 {
	//Make this a stack variable
	let z:f32* = &x;

	if(!(<i32>x)){
		ret 5;
	} else {
		ret 7;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @logical_not_with_cast(0.00000001);
}
