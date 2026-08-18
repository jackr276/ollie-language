/**
 * Author: Jack Robbins
 * Test our ability to inline with static variables
 */


inline fn add(operand:i32) -> i32 {
	let static x:mut i32 = 0;
	x += operand;
	
	ret x;
}


inline fn sub(operand:i32) -> i32 {
	let static x:mut i32 = 50;
	x -= operand;
	ret x;
}


pub fn main() -> i32 {
	let operand:i32 = 5;

	declare result1:i32;
	declare result2:i32;
	
	@add(operand);
	@add(operand);
	result1 = @add(operand);

	@sub(operand);
	@sub(operand);
	result2 = @sub(operand);

	//Should return 15 + 35 = 50
	OUNIT: [exit_status = 50]
	ret result1 + result2;
}
