/**
* Author: Jack Robbins
* Testing a function predeclaration
*/

// Predeclare
declare fn predeclared(void) -> i32;

fn test_func() -> i32 {
	let i:i32 = @predeclared();

	ret i;
}

fn predeclared() -> i32{
	ret 1;
}

pub fn main() -> i32 {
	//Example call
	let mut i:i32 = @test_func();

	ret i;
}
