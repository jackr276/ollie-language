/**
* Author: Jack Robbins
* Test the ability for the transitive closure in the call graph to detect indirect
* recursion
*/

declare fn function_one(mut i32) -> i32;
declare fn function_two(mut i32) -> i32;
declare fn function_three(mut i32) -> i32;


fn function_one(x:mut i32) -> i32 {
	if(x == 0) {
		ret 0;
	}

	ret @function_two(x);
}

fn function_two(x:mut i32) -> i32 {
	ret @function_three(x);
}

//Indirect recursive chain for function 1. We note
//here that function one will eventually end up calling
//itself
fn function_three(x:mut i32) -> i32 {
	ret @function_one(x);
}

pub fn main() -> i32 {
	ret @function_one(3);
}
