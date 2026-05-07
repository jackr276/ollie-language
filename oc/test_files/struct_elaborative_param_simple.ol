/**
 * Author: Jack Robbins
 * Test the use of a struct inside of an elaborative param
 */


define struct my_struct {
	c:char;
	x:i32[10];
	y:f32;
} as param_passed;


/**
 * Test the use of an elaborative param for these structs
 */
pub fn elaborative_param_structs(struct_arr:params param_passed) -> i32 {
	let result:mut i32 = 0;

	//Run through and sum
	for(let i:mut i32 = 0; i < paramcount(struct_arr); i++){
		//Grab a pointer out
		let struct_ref:param_passed* = &(struct_arr[i]);
			
		result += struct_ref=>c + struct_ref=>x[2];
	}

	ret result;
}


pub fn main() -> i32 {
	let struct1:param_passed = {'a', [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 3.33};
	
	/**
	 * Should return: 'a'(97) + 3 = 100
	 */
	ret @elaborative_param_structs(struct1);
}
