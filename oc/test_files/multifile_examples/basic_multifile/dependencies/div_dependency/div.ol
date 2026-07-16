/**
 * Author: Jack Robbins
 * Contrived example for a multifile dependecy test
 */

$module "div_dependency";

pub fn div(x:i32, y:i32) -> i32 {
	ret x / y;
}
