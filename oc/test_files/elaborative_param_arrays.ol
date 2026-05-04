/**
* Author: Jack Robbins
* Handle the case where we are using an elaborative param of arrays(valid case)
*/

pub fn elaborative_param_arrays(arr_of_arr:params i32[4]) -> i32 {
	let result:mut i32 = 0;

	for(let i:mut i32 = 0; i < paramcount(arr_of_arr); i++){
		result += arr_of_arr[i][1];
	}

	ret result;
}


pub fn main() -> i32 {	
	let arr1:i32[] = [1, 2, 3, 4];
	let arr2:i32[] = [1, 3, 3, 4];
	let arr3:i32[] = [1, 4, 3, 4];
	let arr4:i32[] = [1, 5, 3, 4];

	//Should return 2 + 3 + 4 + 5 = 14
	ret @elaborative_param_arrays(arr1, arr2, arr3, arr4);
}
