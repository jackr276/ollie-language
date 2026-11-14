/**
 * Author: Jack Robbins
 * Test a float conditional where we're using >= to see if this works
 * properly with NaN handling
 */


/**
 * This doesn't invert the branch so the test still works for the other way
 */
pub fn float_if_assignment_non_inverted(f1:f32, f2:f32, y:i32, z:i32) -> i32 {
	ret (f1 >= f2) ? y else z;
}


pub fn float_if_assignment(f1:f32, f2:f32, y:i32, z:i32) -> i32 {
	declare result:i32;

	//Nan can never be less than anything, so return z
	if(f1 < f2) {
		result = y;
	} else {
		result = z;
	}

	ret result;
}


pub fn main() -> i32 {
	//Make this all 1's to turn it into NaN
	let x:mut i32 = 0;
	x = ~x;

	//Get this value to actually be NaN
	let NaN:f32 = *(<f32*>(&x));
	let not_nan:f32 = 5.55;

	//Should return 9 + 11 = 20
	OUNIT: [exit_status = 20]
	ret @float_if_assignment(NaN, not_nan, 8, 9)
		+ @float_if_assignment_non_inverted(NaN, not_nan, 10, 11);
}
