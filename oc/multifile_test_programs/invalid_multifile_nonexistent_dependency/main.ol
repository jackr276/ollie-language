/**
 * Author: Jack Robbins
 * Test a case where we'll have a build system failure for not being able to find a dependency
 */

//This dependency itself has a non-existent second order dependency
$import "first_order_dep";


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
