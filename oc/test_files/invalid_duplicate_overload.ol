/**
 * Author: Jack Robbins
 * Test a case where we have a duplicate overload
 */


fn sub(x:i32, y:i32) -> i32 {
	ret x - y;
}


fn sub(x:f32, y:f32) -> f32 {
	ret x - y;
}


//DUPLICATE!
fn sub(x:i32, y:i32) -> i32 {
	ret x - y;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @sub(6, 3);
}
