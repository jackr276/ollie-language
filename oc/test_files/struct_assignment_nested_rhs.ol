/**
* Author: Jack Robbins
* Test the case where we are assigning(copying) a struct that is nested inside of another struct. This
* specifically tests the case where we're copying on both the left and right hand sides so it is
* most complex than usual
*/

define struct inner {
	x:i32;
	y:f64;
} as inner_struct;


define struct outer {
	x:i32[5];
	nested_inner:mut inner_struct;
	holder:i32;
} as outer_struct;



pub fn main() -> i32 {
	let outer1:mut outer_struct = {[1, 2, 3, 4, 5], {1, 2.0}, 5};
	let outer2:mut outer_struct = {[1, 2, 3, 4, 5], {5, 5.0d}, 5};

	//Perform the copy from one nested struct into another
	outer1:nested_inner = outer2:nested_inner;

	//Should return 5
	ret outer1:nested_inner:x;
}
