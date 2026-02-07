/**
* Author: Jack Robbins
* Test the preprocessor's ability to detect duplicated macro definitions
*/

//Duplicated name - should fail
$macro DUPLICATE 0 $endmacro
$macro DUPLICATE 1 $endmacro


pub fn main() -> i32 {
	ret duplicate;
}
