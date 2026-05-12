/**
* Author: Jack Robbins
* Test case where we have a ternary struct copy
*/

define struct custom {
	x:i32;
	y:i32;
	z:f64;
	ch:char;
} as small_struct;


/**
* Simple tester for copying structs via a ternary expression
*/
pub fn copy_structs_ternary(x:i32) -> i32 {
	let original1:small_struct = {1, 5, 3.33d, 'a'};
	let original2:small_struct = {2, 6, 4.44d, 'b'};

	//Triggers a ternary
	let copy:small_struct = (x > 2) ? original1 else original2;

	ret copy:x + copy:y;
}



pub fn main() -> i32 {
	//Should return 1 + 5 = 6
	ret @copy_structs_ternary(3);
}
