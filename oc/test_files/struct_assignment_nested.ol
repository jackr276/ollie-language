/**
* Author: Jack Robbins
* Test the case where we are assigning(copying) a struct that is nested inside of another struct
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
	let outer:mut outer_struct = {[1, 2, 3, 4, 5], {1, 2.0}, 5};
	let inner:inner_struct = {5, 5.0d};
	
	//Try to assign a component of the inner struct over here
	outer:nested_inner = inner;

	//Should return 5
	ret outer:nested_inner:x;
}
