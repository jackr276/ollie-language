/**
 * Author: Jack Robbins
 * This multifile program is designed to test dependencies that are several orders deep
 */

$module subber;

//Link to the zero checker
$import "zero_checker";


pub fn subber(x:i32, y:i32) -> i32 {
	if(@is_zero(x) || @is_zero(y)) {
		ret -1;
	}

	ret x - y;
}
