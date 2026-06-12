/**
* Author: Jack Robbins
* Test an intentional infinite loop scenario to validate how we handle it with all 
* of our dominance analysis, etc
*/


/**
 * If we see a case like this, I think we are going to assume that the
 * author wants this to be a trapping loop and *not* optimize it away
 */
pub fn infinite_loop() -> i32 {
	let x:mut i32 = 5;

	loop {
		x++;
	}

	//See what happens to the stuff after
	x += 2;
	ret x;
}


pub fn main() -> i32 {
	ret 0;
}

