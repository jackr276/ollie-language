/**
* Author: Jack Robbins
* Test the handling of f64 negative 0 in Ollie. Since -0.0 in IEEE 754 would have a
* 1 in the sign bit, we should be able to extract that one and return it. -0.0 is not
* equivalent to regular 0.0
*/


pub fn main() -> i32 {
	let x:f64 = -0.0d;

	OUNIT: [exit_status = 1]
	if((*(<u64*>(&x)) >> 63) == 1){
		ret 1;
	} else {
		ret 0;
	}
}
