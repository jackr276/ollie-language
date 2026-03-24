/**
* Author: Jack Robbins
* Test the case where we have an elaborative param that
* also has local stack allocations
*/


pub fn elaborative_params(x:mut i32, y:params i32) -> i32 {
	let arr:i32[] = [1, 2, 3, 4];

	for(let i:mut i32 = 0; i < paramcount(y); i++){
		x += y[i];

		if(i < 4) {
			x += arr[i];
		}
	}

	ret x;
}


pub fn main() -> i32 {
	//Valide use where we don't pass anything
	ret @elaborative_params(1);
}
