/**
* Author: Jack Robbins
* Test the use of the elaborative param 'params' in ollie
*/


/**
* We can use the elaborative params type to pass in the number of
* parameters to a given function. 
*
* For our case here, params:i32 is a variable length stack array of i32 variables.
* The first 4 bytes are used to store the size(in bytes) of the params which can
* be used later on to extract the parameters
*/
pub fn elaborative_param(x:i32, y:i32, params elaborative_params:i32) -> i32 {
	let result:mut i32 = x + y;

	//Paramcount will get this for us
	let count:i32 = paramcount(elaborative_params);

	for(let i:i32 = 0; i < paramcount(elaborative_params); i++){
		result += elaborative_params[i];
	}

	ret result;
}


pub fn main() -> i32 {
	//Here 4, 5 and 6 would go onto the stack
	ret @elaborative_param(1, 2, 4, 5, 6);
}
