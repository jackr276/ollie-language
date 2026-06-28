/**
* Author: Jack Robbins
* Testing the true/false values in the lexer/parser
*/

fn a_over_b(a:i32, b:i32) -> bool {
	ret a > b ? true else false;
}


pub fn main() -> i32 {
	OUNIT: [console = 1]
	ret @a_over_b(5, 4);
}
