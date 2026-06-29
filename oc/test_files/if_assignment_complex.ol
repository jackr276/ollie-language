/**
 * Author: Jack Robbins
 * Test a more complex version of if-assignment where we have nested if statements
 */


pub fn complex_if_assignment(x:i32) -> i32 {
	declare result:mut i32;

	if(x > 12) {
		if(x == 13){
			result = 1;
		} else {
			result = 3;
		}

	} else {
		if(x == 9){
			result = 4;
		} else {
			result = 6;
		}
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [console = 6]
	ret @complex_if_assignment(10);
}
