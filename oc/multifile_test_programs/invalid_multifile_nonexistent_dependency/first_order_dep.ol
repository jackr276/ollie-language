/**
 * Author: Jack Robbins
 * Test a case where we'll have a build system failure for not being able to find a dependency
 */


$module first_order_dep;

$import "nonexistent_module";


pub fn dummy() -> i32 {
	ret 0;
}
