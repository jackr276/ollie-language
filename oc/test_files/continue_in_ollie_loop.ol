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
		counter++;

		//Break once we hit 100
		break when(counter == 100);

		//Continue when we're even
		continue when(counter % 2 == 0);

		x++;
	}

	ret x;
}


pub fn main() -> i32 {
	let x:mut i32 = 5;

	//Should return x + 50 = 55
	OUNIT: [console = 55]
	ret @ollie_loop(x);
}
