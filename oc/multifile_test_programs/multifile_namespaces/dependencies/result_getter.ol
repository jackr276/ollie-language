/**
 * Author: Jack Robbins
 * "Result_getter" that simulates a driver class for all of our math
 */

$module result_getter;

//Link to math
$import "math";

namespace driver {
	/**
	 * If the new input is even, we'll add to it. If the
	 * new input is odd, we'll subtract it from the result
	 */
	fn helper(result:mut i32*, new_input:i32) -> void {
		if(new_input % 2 == 0){
			@calculator::add::calculate(result, new_input);
		} else {
			@calculator::subtract::calculate(result, new_input);
		}
	}

	/**
	 * Run through the entire array to do our calculations
	 */
	pub fn get_result(input_array:i32[10]) -> i32 {
		let result:mut i32 = 0;

		for(let i:mut i32 = 0; i < 10; i++){
			@helper(&result, input_array[i]);
		}
		
		ret result;
	}
}

