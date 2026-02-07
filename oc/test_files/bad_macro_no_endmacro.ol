/**
* Author: Jack Robbins
* Test the fail case where the user forgets the $endmacro directive
*/

//BAD - this has no $endmacro on it
$macro BAD_MACRO_NO_END 333

pub fn main() -> i32 {
	ret BAD_MACRO_NO_END;
}
