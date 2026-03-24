/**
* Author: Jack Robbins
* Test an invalid attempt to assign an invalid stack param
*/

//Invalid - we can't assign to this
pub fn elaborative_param(x:i32, y:i32, elaborative_params:params i32) -> i32 {
	let result:mut i32 = x + y;

	//Paramcount will get this for us
	let count:i32 = paramcount(elaborative_params);

	//Can't do this
	elaborative_params = 5;

	for(let i:i32 = 0; i < paramcount(elaborative_params); i++){
		result += elaborative_params[i];
	}

	ret result;
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
