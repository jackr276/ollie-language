/**
* This file will test various cases where the compressed equality should specifically *not* work
*/


fn bad_size() -> i32 {
	let mut x:i32 := 3;
	let mut y:i64 := 222;

	//bad size
	x *= y;

	ret x;
}


fn main() -> i32 {
	ret 0;
}
