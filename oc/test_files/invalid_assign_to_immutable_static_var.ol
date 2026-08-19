/**
 * Author: Jack Robbins
 * Test an invalid case where we are assigning to an immutable 
 * static variable
 */


 pub fn assign_static(x:i32) -> i32 {
 	let static static_var:i32 = 5;

	//INVALID - it's immutable
	static_var = x;

	ret x;
 }


 pub fn main() -> i32 {
 	OUNIT: [fail_to_compile]
 	ret @assign_static(5);
 }
