/**
 * Author: Jack Robbins
 * Test an invalid case where the user is attempting to define the main file as a module
 */


//Invalid because we'll be passing this file in with -f, so it can't be a module
$module invalid_module;


 pub fn main() -> i32 {
 	OUNIT: [fail_to_compile];
 	ret 0;
 }
