/**
* Author: Jack Robbins
* Test how we are able to duplicate parameter names in scope that are
* not the uppermost scope in a function
*/

pub fn duplicate_name(tester:i32) -> i32 {
	//Stupid example but it illustrates the point
	{
		let tester:i32 = 5;

		ret tester;
	}
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
