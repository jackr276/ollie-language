/**
 * Author: Jack Robbins
 * Test taking the address of an array
 */

pub fn mutate_array(x:mut i32[5]*) -> void {
	(*x)[1] = 5;
}


pub fn main() -> i32 {
	let x:mut i32[5] = [1, 2, 3, 4, 5];

	//Invoke the mutator
	@mutate_array(&x);

	OUNIT: [exit_status = 5]
	ret x[1];
}
