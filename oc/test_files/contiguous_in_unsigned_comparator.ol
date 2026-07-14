/**
 * Author: Jack Robbins
 * Verify that we have the correct conditional move types for a contigious in with an unsigned comparator
 */

pub fn unsigned_comparator_in(x:u32) -> i32 {
	let result:mut i32 = 5;

	if(x in (1, 2, 4, 3, 7, 6, 5, 8, 10, 9)) {
		result++;
	} else {
		result--;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 10]
	ret @unsigned_comparator_in(7u) + @unsigned_comparator_in(0u);
}
