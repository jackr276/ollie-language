/**
* Author: Jack Robbins
* Test casting mutable to mutable
*/


fn tester(x:mut void*) -> i32 {
	ret *(<mut i32*> x);
}


pub fn main() -> i32 {
	let x:mut i32 = 3;

	@tester(&x);

	ret 0;
}
