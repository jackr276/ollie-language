/**
* Author: Jack Robbins
* Valid case where a user attempts to inline a public function
*/


declare pub inline fn valid_inline() -> i32;

//Use the predeclared
fn use_predeclared() -> i32 {
	ret @valid_inline();
}


//Define the predeclared function
pub inline fn valid_inline() -> i32 {
	ret 101;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 101]
	ret @use_predeclared();
}
