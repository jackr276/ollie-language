/**
 * Author: Jack Robbins
 * Test an invalid attempt to mutate an immutable function parameter inside of a loop
 */

pub fn invalid_mutate_in_loop(x:i32, counter:mut i32) -> i32 {
	let result:mut i32 = 0;

	while(counter > 0) {
		counter--;
		//INVALID - cannot do this mutation
		x = 5;
		result += x;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @invalid_mutate_in_loop(5, 7);
}
