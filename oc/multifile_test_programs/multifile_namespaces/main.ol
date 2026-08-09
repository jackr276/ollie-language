/**
 * Author: Jack Robbins
 * This test file does simple arithmetic in a shakedown test of the build and
 * namespace system
 *
 * Basically this is just going to test if we can do some math over this array
 * while we pass it around
 */

$import "result_getter";

pub fn main() -> i32 {
	let input_arr:i32[10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	ret @driver::get_result(input_arr);
}
