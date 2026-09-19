/**
 * Author: Jack Robbins
 * Test an invalid case of what Ollie calls an ambiguous overload. This means that we are unable
 * to determine which overload to use based on the parameters in the function call
 */


//Dummy for us to use
pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn perform_arithmetic(x:i32, y:i32, arithmetic:fn(i32, i32) -> i32) -> i32 {
	ret @arithmetic(x, y);
}


pub fn perform_arithmetic(x:i16, y:i16, arithmetic:fn(i32, i32) -> i32) -> i32 {
	ret @arithmetic(x, y);
}


pub fn perform_arithmetic(x:f32, y:f32, arithmetic:fn(i32, i32) -> i32) -> i32 {
	ret @arithmetic(x, y);
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	/**
	 * This should be ambiguous because 5 and 6 could be i8's or i32's, so
	 * we don't know which rule to dispatch to
	 */
	ret @perform_arithmetic(<i8>5, <i8>6, add);
}

