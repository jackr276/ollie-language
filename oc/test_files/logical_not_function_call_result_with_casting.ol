/**
 * Author: Jack Robbins
 * Test our ability to do logical not with casting from a function call result
 */


pub fn get_float() -> f32 {
	ret 0.00000001;
}


pub fn logical_not_with_cast() -> i32 {
	if(!(<i32>(@get_float()))){
		ret 5;
	} else {
		ret 7;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @logical_not_with_cast();
}
