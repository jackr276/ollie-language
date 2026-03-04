/**
* Author: Jack Robbins
* Test the register allocator's ability to handle a floating point spill
*
* The strategy for something like this is to have some stupid calculation
* that keeps variables at the very top *alive* for a long time
*/


pub fn float_spill(x:f32, y:f32, z:f32, a:f32, b:f32, c:f32) -> f32 {
	let local1:f32 = 3.33;
	let local2:f32 = 3.33;
	let local3:f32 = 3.33;
	let local4:f32 = 3.33;
	let local5:f32 = 3.33;
	let local6:f32 = 3.33;
	let local8:f32 = 3.33;
	let local9:f32 = 3.33;
	let local10:f32 = 3.33;
	let local11:f32 = 3.33;
	let local12:f32 = 3.33;
	let local13:f32 = 3.33;
	let local14:f32 = 3.33;
	let local15:f32 = 3.33;
	
	//Keep this around to encourage interference
	let result1:f32 = local1 * local2 * 4;

	let result2:f32 = local1 + local2;
	let result3:f32 = local3 + local2;
	let result4:f32 = local4 + local2;

	declare result5:mut f32;

	if(local1 * 5 != 0) {
		result5 = local6 + local5;

	} else {
		result5 = local6 - local5 + local11;
	}
	
	//Keep these all around
	ret result1 + result2 + result3 + result4 
			+ result5 + local6 + local8 + local9 + local10 + local12 + local13 + local14 + local15;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
