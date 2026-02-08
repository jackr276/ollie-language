/**
* Author: Jack Robbins
* Test handling for bad macro that is completely empty
*/

//Invalid - you can't have an empty macro
$macro EMPTY $endmacro

pub fn main() -> i32 {
	ret EMPTY;
}
