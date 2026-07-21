/**
 * Author: Jack Robbins
 * Contrived example for a multifile dependecy test
 */

$module sub_dependency;

pub fn sub(x:i32, y:i32) -> i32 {
	ret x - y;
}
