/**
* Author: Jack Robbins
* Test the instance where we have a duplicate array type lookup
*/

fn tester() -> i32 {
	//Duplicate - the type itself should be reused
	let x:mut i32[4] = [1, 2, 3, 4];
	let y:mut i32[4] = [1, 2, 3, 4];
	
	ret 0;
}

pub fn main() -> i32 {
	let x:mut i32[] = [1, 2, 3, 4];
	let y:mut i32[] = [1, 2, 3, 4, 5];
	
	ret 0;
}
