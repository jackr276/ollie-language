/**
 * Author: Jack Robbins
 * "Result_getter" that simulates a driver class for all of our math
 */

$module result_getter;


namespace driver {
	/**
	 * If the new input is odd, we'll add to it. If the
	 * new input is even, we'll subtract it from the result
	 */
	fn helper(result:mut i32*, new_input:i32) -> void {

		
	}

	pub fn get_result(input_array:i32[10]) -> i32 {
		let result:mut i32 = 0;

		for(let i:i32 = 0; i < 10; i++){
			@helper(&result, input_array[i]);
		}
		
		ret result;
	}
}

