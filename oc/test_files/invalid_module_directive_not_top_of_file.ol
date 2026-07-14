/**
 * Author: Jack Robbins
 * Validate that any attempt to put the $module directive lower than the very first thing in 
 * a file leads to an immediate failure
 */

 pub fn dummy(x:i32) -> i32 {
 	ret 0;
 }

//BAD - may only ever be at the very top
$module "my_module";

pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
