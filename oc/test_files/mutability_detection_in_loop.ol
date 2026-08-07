/**
 * Author: Jack Robbins
 * Test an edge case for mutability detection where we have an immutable variable initialized in
 * a loop
 */

pub fn immutable_in_loop(x:i32) -> i32 {
	let counter:mut i32 = 0;
	let result:mut i32 = 0;

	while(counter < x){
		//Verify that this works
		let helper:i32 = 5;
		result += helper;

		counter++;
	}

	ret	result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 25]
	ret @immutable_in_loop(5);
}
