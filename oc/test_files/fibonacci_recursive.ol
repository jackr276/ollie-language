/**
* Author: Jack Robbins
* This test file uses the fibonacci calculation to test recursive functions
*/


//Naive(not tail) recursive version
fn fib(x:i32) -> i32 {
	if(x == 0) {
		ret 0;
	}

	if(x == 1) {
		ret 1;
	}

	ret @fib(x - 1) + @fib(x - 2);
}


pub fn main() -> i32 {
	ret @fib(33);
}
