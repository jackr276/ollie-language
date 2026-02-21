/**
* Author: Jack Robbins
* Test an invalid void[] declaration
*/

pub fn main() -> i32 {
	//This is invalid - we cannot have a void array no matter what. The parser should smack this down immediately
	declare x:void[4];

	ret 0;
}
