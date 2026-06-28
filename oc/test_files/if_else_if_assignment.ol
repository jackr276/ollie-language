/**
* Author: Jack Robbins
* Test an if-else-if assignment that would be compatible with our conditional movement
* operations
*/


pub fn to_conditional_move(x:i32) -> i32 {
	declare result:mut i32;

	//This should be turned into conditional moves
	if(x > 10){
		result = 1;
	} else if(x > 8){
		result = 2;
	} else if(x > 6){
		result = 3;
	} else if (x > 4){
		result = 4;
	} else {
		result = 5;
	}

	ret	result;
}


pub fn main() -> i32 {
	OUNIT:[console = 4]
	ret @to_conditional_move(6);
}
