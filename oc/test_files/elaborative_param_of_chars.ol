/**
 * Author: Jack Robbins
 * Test a simple case where we have an elaborative param of all chars
 */

pub fn elaborative_with_chars(x: params char) -> char {
	let result:mut char = 0;

	for(let i:mut i32 = 0; i < paramcount(x); i++){
		result += x[i];
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 21]
	ret @elaborative_with_chars(1, 2, 3, 4, 5, 6);
}
