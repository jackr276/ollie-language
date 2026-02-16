/**
* Author: Jack Robbins
* Test a basic case of a nested paren inside of a macro
*/

$macro NESTED_PAREN(x)
	x * 2 + 1
$endmacro

pub fn main() -> i32 {
	let x:mut i32 = 3;

	x = NESTED_PAREN((x + 1));

	ret x;
}
