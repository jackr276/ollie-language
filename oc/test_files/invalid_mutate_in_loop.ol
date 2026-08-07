/**
 * Author: Jack Robbins
 * Test an invalid case where we try to mutate inside of a loop
 */


pub fn mutate_in_loop(x:mut i32) -> i32 {
	declare mutater:i32;
	let result:mut i32 = 0;

	while(x >= 0){
		x--;

		//INVALID - after the first go this is initialized
		mutater = x + 5;
		result += mutater;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @mutate_in_loop(5);
}
