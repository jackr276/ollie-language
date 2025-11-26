/**
* Author: Jack Robbins
* Test casting immutable to immutable
*/


fn tester(x:void*) -> i32 {
	ret *(<i32*>x);
}


pub fn main() -> i32 {
	let x:mut i32 = 3;

	@tester(&x);

	ret 0;
}
