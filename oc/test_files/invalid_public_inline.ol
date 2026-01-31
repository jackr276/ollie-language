/**
* Author: Jack Robbins
* Invalid case where a user attempts to inline a public function
*/


inline pub fn invalid_inline() -> i32 {
	ret 0;
}


pub fn main() -> i32 {
	ret @invalid_inline();
}
