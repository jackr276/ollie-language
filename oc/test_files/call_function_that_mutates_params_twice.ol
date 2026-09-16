/**
 * Author: Jack Robbins
 * Test our ability to properly handle parameter registers
 * when we call a function that mutates parameters twice in a row
 */


//This function will clobber the value in %rdi
pub fn mutate_parameters(x:mut i32, y:i32) -> i32 {
	x += y;

	ret x;
}



pub fn main() -> i32 {
	let x:mut i32 = 15;

	//The result of this is 22
	@mutate_parameters(x, 7);
	//The result of this *SHOULD* be 23
	let result2:i32 = @mutate_parameters(x, 8);

	OUNIT: [exit_status = 23]
	ret result2;
}
