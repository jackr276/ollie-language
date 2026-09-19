/**
 * Author: Jack Robbins
 * Test an invalid case where we have a function that is overloaded but only differs by error
 * raising type. This will fail to compile
 */

define error generic_error_t;



pub fn! add(x:i32, y:i32) -> i32 {
	if(y < 0) {
		raise error;
	}


	ret x + y;
}


//BAD!
pub fn! add(x:i32, y:i32) -> f32 raises (generic_error_t) {
	if(y < 0) {
		raise error;
	}

	if(x < 0){
		raise generic_error_t;
	}

	ret x + y;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
