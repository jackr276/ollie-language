/**
* Author: Jack Robbins
* Extreme case for the preprocessor
*/

$macro INC_BY_1(x) (x + 1) $endmacro

$macro INC_BY_2(x) (x + 2) $endmacro

$macro ADD(x, y) (x + y) $endmacro


pub fn addition(x:i32, y:i32) -> i32 {
	ret ADD(x, INC_BY_1(INC_BY_2(y)));
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
