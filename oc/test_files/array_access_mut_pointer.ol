/**
* Author: Jack Robbins
* Attempt to assign to immutable array
*/

pub fn tester(x:mut i32**) -> void{
	//Should work
	(*x)[3] = 3;
}

pub fn main() -> i32 {
	declare arr:mut i32[33];

	@tester(&arr);

	ret 0;
}
