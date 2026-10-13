/**
 * Author: Jack Robbins
 * Stress test a case of our guaranteed assignment evaluation order where
 * we need to evaluate from left to right to make this work
 */



pub fn return_value_with_mod(x:mut i32*) -> i32 {
	*x = 197000;

	ret 5;
}




pub fn main() -> i32 {
	let x:mut i32 = 0;
	declare arr:mut i32[5];

	//If the left doesn't execute first this will segfault
	arr[x] = @return_value_with_mod(&x);

	OUNIT: [exit_status = 5]
	ret arr[0];
}
