/**
* Author: Jack Robbins
* Testing a function predeclaration with bad parameter types
*/

// Predeclare
declare fn predeclared(i32, mut i16) -> i32;

fn test_func() -> i32 {
	let i:i32 = @predeclared(1, 3);

	ret i;
}

//We have mismatched parameter types - this should fail
fn predeclared(mut x:i32, y:i16) -> i32{
	ret x + y;
}

pub fn main() -> i32 {
	//Example call
	let mut i:i32 = @test_func();

	ret i;
}
