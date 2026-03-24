/**
* Author: Jack Robbins
* Test a super edgecase where someone is passing in pointers as elaborative params
* and then attempting to dereference them all in one go
*/


pub fn elaborative_params_pointers(x:i32, y:params mut i32*) -> i32 {
	let result:mut i32 = x;

	for(let i:mut i32 = 0; i < paramcount(y); i++) {
		result += y[i][0] + y[i][1];
	}

	ret result;
}


//Pure dummy
pub fn main() -> i32 {
	ret 0;
}
