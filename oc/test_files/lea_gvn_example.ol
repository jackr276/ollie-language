/**
 * Author: Jack Robbins
 * Test an example where lea simplification through global value numbering would make sense
 */


pub fn double_ptr(x:i32*) -> i32 {
	ret *x * 2;
}


pub fn lea_gvn(x:i32) -> i32 {
	let arr:i32[15] = [8, 7, 9, 1, -1, 7, 3, 8, 0, -11, -121, 15, 0xAE, 8, 5];
	let result:mut i32 = 5;

	result += @double_ptr(&(arr[x]));
	result += @double_ptr(&(arr[x]));

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 33]
	ret @lea_gvn(5);
}
