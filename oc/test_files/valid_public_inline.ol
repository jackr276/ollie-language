/**
* Author: Jack Robbins
* Valid case where a user attempts to inline a public function
*/


pub inline fn valid_inline() -> i32 {
	ret 11;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 11]
	ret @valid_inline();
}
