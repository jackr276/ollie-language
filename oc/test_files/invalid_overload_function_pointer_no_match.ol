/**
 * Author: Jack Robbins
 * Test an invalid case where no overload meets the given specifications
 */

//Dummy for us to use
pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}

//Dummy for us to use
pub fn add(x:i64, y:i64) -> i64 {
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
	//This will not work, no overload matches
	let fn_ptr:fn(i64, i64, fn(i64, i64) -> i64) -> i64 = perform_arithmetic;

	OUNIT: [fail_to_compile]
	ret @fn_ptr(5, 6, add);
}
