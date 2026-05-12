/**
* Author: Jack Robbins
* Test a super edgecase where someone is passing in pointers as elaborative params
* and then attempting to dereference them all in one go
*/


pub fn elaborative_param_of_pointers(x:i32, y:params mut i32*) -> i32 {
	let result:mut i32 = x;

	for(let i:mut i32 = 0; i < paramcount(y); i++) {
		result += y[i][0] + y[i][1];
	}

	ret result;
}


//Call into this function
pub fn main() -> i32 {
	let x:mut i32[4] = [1, 2, 3, 4];
	let y:mut i32[4] = [3, 6, 7, 10];
	let z:mut i32[4] = [4, 5, 8, 9];

	//Should return - 0 + (1 + 2) + (3 + 6) + (4 + 5) = 21
	ret @elaborative_param_of_pointers(0, x, y, z);
}
