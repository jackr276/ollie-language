/**
 * Author: Jack Robbins
 * Test a very basic dependency in Ollie
 */

//The module macro declares this entire file as a module
$module test_dependency;


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}
