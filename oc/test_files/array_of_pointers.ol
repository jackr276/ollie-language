/**
* Author: Jack Robbins
* Test the functionality for an array of pointers
*/


/**
* Take in an array of pointers
*/
pub fn get_value(arr:i32*[5]) -> i32 {
	ret *(arr[2]);
}


pub fn populate_and_return_array(x:i32) -> i32 {
	let arr1:i32[] = [1, 2];
	let arr2:i32[] = [1, 2];
	let arr3:i32[] = [1, 2];
	let arr4:i32[] = [1, 2];
	let arr5:i32[] = [1, 2];

	let array_of_pointers:i32*[] = [arr1, arr2, arr3, arr4, arr5];

	ret @get_value(array_of_pointers);
}
