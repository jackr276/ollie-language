/**
 * Author: Jack Robbins
 * Test another case where the side type is going to matter - an array
 * using another array accessors as an index
 */


pub fn main() -> i32 {
	let index_array:i32[] = [0, 1, 2, 3, 4];
	let result_array:mut i32[] = [11, 12, -10, 13, 15];

	result_array[index_array[3]] = 27;

	OUNIT: [exit_status = 27]
	ret result_array[index_array[3]];
}
