/**
* Author: Jack Robbins
* Test a fail case where a user repeats a macro parameter
*/

$macro INVALID(x, y, y)
	x * y + x;
$endmacro

pub fn main() -> i32 {
	ret 0;
}
