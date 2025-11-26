/**
* Author: Jack Robbins
* Test dereferencing a pointer array in ollie
*/

pub fn main() -> i32 {
	declare arr:mut i32*[33];

	*(arr[3]) = 3;

	ret 0;
}
