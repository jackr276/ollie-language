/**
* Author: Jack Robbins
* Test an extreme case where the user is defining a whole function as a macro. Note
* that this is probably one of the worst things that you could ever do from a code readability
* standpoint, it's just a stress test
*/

$macro MACROED_FUNCTION
pub fn macroed_function(x:i32, y:i32) -> i32 {
	ret x + y;
}

$endmacro

//Should be a copy-paste right here
MACROED_FUNCTION



pub fn main() -> i32 {
	ret @macroed_function(2, 3);
}
