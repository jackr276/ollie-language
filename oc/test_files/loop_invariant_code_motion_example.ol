/**
 * Author: Jack Robbins
 * Give an example of a loop that has a lot of junk internally that can be 
 * factored out of the inside to save on cycles
 */


pub fn lvm_example(x:i32[15], iter:i32) -> i32 {
	let counter:mut i32 = 0;
	let result:mut i32 = 0;

	while(counter < iter) {
		//x[5] is a repeated operation
		result += x[5];

		counter++;
	}

	ret result;
}



pub fn main() -> i32 {
	let x:i32[15] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];

	OUNIT: [exit_status = 60]
	ret @lvm_example(x, 10);
}
