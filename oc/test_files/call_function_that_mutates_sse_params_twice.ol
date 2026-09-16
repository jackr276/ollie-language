/**
 * Author: Jack Robbins
 * Test our ability to properly handle parameter registers
 * when we call a function that mutates parameters twice in a row
 * 
 * This is just the SSE version of the same thing
 */


//This function will clobber the value in %xmm0
pub fn mutate_parameters(x:mut f32, y:f32) -> f32 {
	x += y;

	ret x;
}



pub fn main() -> i32 {
	let x:f32 = 15.75;

	//The result of this is 23.5
	@mutate_parameters(x, 7.75);
	//The result of this *SHOULD* be 27.5
	let result2:f32 = @mutate_parameters(x, 11.75);

	OUNIT: [exit_status = 27]
	ret result2;
}
