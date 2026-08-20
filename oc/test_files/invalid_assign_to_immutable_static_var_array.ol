/**
 * Author: Jack Robbins
 * Test an invalid case where we are assigning to an immutable 
 * static variable
 */


 pub fn assign_static(x:i32, index:i32) -> i32 {
 	let static static_arr:i32[5] = [1, 2, 3, 4, 5];

	//INVALID - it's immutable
	static_arr[index] = x;

	ret x;
 }


 pub fn main() -> i32 {
 	OUNIT: [fail_to_compile]
 	ret @assign_static(5);
 }
