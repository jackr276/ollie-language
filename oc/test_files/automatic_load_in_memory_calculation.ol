/**
 * Author: Jack Robbins
 * Verify that we do properly emit an automatic load inside of memory address calculations
 */


pub fn mut_ptr(x:mut i32*) -> void {
	*x = 2;
}


pub fn main() -> i32 {
	let arr:mut i32[10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	let x:mut i32 = 5;
	let y:i32 = 6;

	//Mutate the x pointer
	@mut_ptr(&x);

	//Now use this on the LHS - should get loaded automatically
	arr[x] = y;

	OUNIT: [exit_status = 6]
	ret arr[x];
}
