/**
* Author: Jack Robbins
* Assign to a mutable array
*/

pub fn tester(x:mut i32[33]*) -> void{
	//Should work
	(*x)[3] = 3;
}

pub fn main() -> i32 {
	declare arr:mut i32[33];

	@tester(&arr);

	ret 0;
}
