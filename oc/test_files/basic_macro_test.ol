/**
* Author: Jack Robbins
* Test the most basic version imagineable of a macro
*/

//Replace NULL with 0
$macro NULL 0 $endmacro


//Super simple, should just replace
pub fn main() -> i32 {
	ret NULL;
}
