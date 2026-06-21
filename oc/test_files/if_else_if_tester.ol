/**
* Author: Jack Robbins
* Test a pattern where we have if else if but no else
*/

pub fn helper(x:i32) -> i32 {
	let result:mut i32 = 1;

	if(x > 5){
		result++;
	} else if (x > 4){
		result--;
	} else if(x > 2){
		result -= 2;
	}

	ret result;
} 


pub fn main() -> i32 {
	OUNIT: [console = 0]
	ret @helper(5);
}
