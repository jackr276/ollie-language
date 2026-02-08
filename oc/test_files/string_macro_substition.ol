/**
* Author: Jack Robbins
* Test a more involved macro substitution that does not take any parameters
*/

$macro SAMPLE_STR
"This is a sample string"

$endmacro


pub fn sample_string() -> char* {
	ret SAMPLE_STR;
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
