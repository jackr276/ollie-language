/**
 * Author: Jack Robbins
 * Test a case where we have a basic ollie loop statement that just does some counting
 */


/**
 * In the end should just add 100 to our input x
 */
pub fn ollie_loop(x:mut i32) -> i32 {
	let counter:mut i32 = 0;

	loop {
		break when(counter == 100);

		counter++;
		x++;
	}

	ret x;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;

	//Should return x + 100 = 105
	OUNIT: [exit_status = 105]
	ret @ollie_loop(x);
}
