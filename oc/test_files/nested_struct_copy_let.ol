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


//Pass by copy, returns 3 * x
pub fn my_fn(param:inner_struct) -> i32 {
	ret 3 * param:x;
}


pub fn main() -> i32 {
	let outer1:mut outer_struct = {[1, 2, 3, 4, 5], {1, 2.0}, 5};
	let outer2:mut outer_struct = {[1, 2, 3, 4, 5], {5, 5.0d}, 5};

	let x:i32 = 6;

	let final_struct:inner_struct = (x > 5) ? outer1:nested_inner
										 else outer2:nested_inner;

	/**
	 * Use the ternary struct assignment as the parameter 
	 * Should return 1 * 3 = 3
	 */
	ret @my_fn(final_struct);
}
