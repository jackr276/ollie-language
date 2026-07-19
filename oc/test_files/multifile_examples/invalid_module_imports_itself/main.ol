/**
 * Author: Jack Robbins
 * Test an invalid case where we have a dependency that tries to import itself
 */

$import "invalid_self_reference";


 pub fn main() -> i32 {
 	OUNIT: [fail_to_compile]
 	ret 0;
 }
