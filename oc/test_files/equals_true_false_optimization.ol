/**
 * Author: Jack Robbins
 * Test optimizations when we are comparing against true and false
 */


pub fn add_or_subtract(flag:u8, x:i32) -> i32 {
	if(flag == true){
		ret x + 5;
	} else {
		ret x - 5;
	}
}


pub fn multiply_or_divide(flag:u8, x:i32) -> i32 {
	if(flag != false){
		ret x * 5;
	} else {
		ret x / 5;
	}
}


pub fn main() -> i32 {
	//Should return 16 + 5 = 21
	OUNIT: [exit_status = 21]
	ret @add_or_subtract(true, 11) + @multiply_or_divide(false, 25);
}
