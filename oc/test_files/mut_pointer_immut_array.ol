/**
* Author: Jack Robbins
* Attempt to assign to immutable array
*/

pub fn tester(x:mut i32**) -> void{
	//Should work
	(*x)[3] = 3;
}

pub fn main() -> i32 {
	let arr:i32[] = [3, 4, 5, 6, 7];

	//Should fail, not mutable
	@tester(&arr);

	ret 0;
}
