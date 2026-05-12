/**
* Author: Jack Robbins
* Test an absurdly complex case where we have a nested ternary that is with a struct copy. Honestly
* if anyone is doing I would be surprised but we still need to support this out of the box
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
	let outer3:mut outer_struct = {[1, 2, 3, 4, 5], {7, 7.0d}, 5};

	let x:i32 = 6;
	let y:i32 = 7;

	/**
	 * Copy all of the way over here with our nested struct. We should
	 * hit the case where we end up using outer3
	 */
	let final_struct:inner_struct = (x > 5) ?
										((y > 7) ? outer1:nested_inner else outer3:nested_inner)
										 else outer2:nested_inner;

	/**
	 * Use the ternary struct assignment as the parameter 
	 * Should return 7 * 3 = 21
	 */
	ret @my_fn(final_struct);
}
