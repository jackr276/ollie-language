/**
* Author: Jack Robbins
* Test the fail case where a macro parameter call does not have a closing paren
*/

$macro PARAMED_MACRO(macro_param)
	(macro_param)++
$endmacro


pub fn main() -> i32 {
	let x:mut i32 = 3;

	PARAMED_MACRO(x;

	ret x;
}
