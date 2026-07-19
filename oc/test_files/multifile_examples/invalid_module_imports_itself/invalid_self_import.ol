/**
 * Author: Jack Robbins
 * This module is going to break a rule where it tries to import itself. This is not
 * allowed and should cause a failure
 */

$module invalid_self_reference;

//BAD - cannot be doing this
$import "invalid_self_reference";


pub fn dummy() -> i32 {
	ret 0;
}
